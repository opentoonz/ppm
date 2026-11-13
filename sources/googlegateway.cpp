#include "googlegateway.h"

#include "workflow.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUrl>

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#include <iostream>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>

#include <QDateTime>
#include <QSettings>
#include <QElapsedTimer>
#include <QTextStream>
#include <QStringConverter>

namespace {
QMutex mutex;

// 全リクエストの経過（キュー投入／開始／成功／失敗）を追記していくログ。
// 書き込みキューファイル(.ini)は成功すると削除されてしまうため、成功したのに反映されていない、
// 途中でアプリが終了した等の事後調査ができるよう、全件の履歴をここに残す
QMutex logMutex;
void appendGoogleLog(const QString& cueFilePath, const QString& line) {
  QMutexLocker logLocker(&logMutex);
  QDir dir = QFileInfo(cueFilePath).dir();
  if (!dir.exists()) dir.mkpath(".");
  QFile file(dir.absoluteFilePath("google_spreadsheet.log"));
  if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    return;
  QTextStream out(&file);
  out.setEncoding(QStringConverter::Utf8);
  out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " "
      << line << "\n";
}
}  // namespace

//-----------------------------------------------------------------------------------

GoogleWorker::GoogleWorker(QString gasUrl, QString sheetName,
                           QStringList cutNumbers, QString flowName,
                           QStringList values, int requestId)
    : m_gasUrl(gasUrl)
    , m_sheetName(sheetName)
    , m_cutNumbers(cutNumbers)
    , m_flowName(flowName)
    , m_values(values)
    , m_requestId(requestId)
    , m_networkManager(new QNetworkAccessManager(this)) {}

void GoogleWorker::process() {
  QString requestSummary = QString("id=%1 sheet=%2 cuts=%3 flow=%4 value=%5")
                               .arg(m_requestId)
                               .arg(m_sheetName, m_cutNumbers.join("_"),
                                    m_flowName, m_values.join("_"));
  // ログの出力先フォルダの特定にのみ使う（実際のキューファイルは排他取得後に作る）
  QString logDirRefPath = WorkflowManager::instance()->googleCueFilePath("log");
  if (logDirRefPath.isEmpty()) {
    emit inputFailed(m_cutNumbers.join("_"), m_requestId);
    emit processEnd();
    return;
  }
  appendGoogleLog(logDirRefPath, "[QUEUED] " + requestSummary);

  // 他のリクエストが処理中の間はここで待つ（同時に複数のリクエストを投げない）
  QElapsedTimer totalTimer;
  totalTimer.start();
  QMutexLocker locker(&mutex);
  qint64 waitedMs = totalTimer.elapsed();

  // 書き込みキューファイルを作成
  QString googleCueFileName =
      QDateTime::currentDateTime().toString("yyMMdd_hhmm_sszzz");
  QString googleCueFilepath =
      WorkflowManager::instance()->googleCueFilePath(googleCueFileName);
  appendGoogleLog(googleCueFilepath,
                  QString("[START] cue=%1.ini waited=%2ms %3")
                      .arg(googleCueFileName)
                      .arg(waitedMs)
                      .arg(requestSummary));
  QSettings cueSetting(googleCueFilepath, QSettings::IniFormat);
  cueSetting.setValue("SheetName", m_sheetName);
  cueSetting.setValue("CutNumber", m_cutNumbers.join("_"));
  cueSetting.setValue("FlowName", m_flowName);
  cueSetting.setValue("Value", m_values.join("_"));
  QString username = qgetenv("USER");
  if (username.isEmpty()) username = qgetenv("USERNAME");
  cueSetting.setValue("UserName", username);

  // GASに送信するJSONデータの構築
  QJsonObject jsonReq;
  jsonReq.insert("sheetName", m_sheetName);
  jsonReq.insert("cutNumbers", QJsonArray::fromStringList(m_cutNumbers));
  jsonReq.insert("flowName", m_flowName);
  jsonReq.insert("values", QJsonArray::fromStringList(m_values));

  QJsonDocument doc(jsonReq);
  QByteArray postData = doc.toJson(QJsonDocument::Compact);

  // GASのウェブアプリはリダイレクト(302)を返すため、リダイレクト追従属性を有効化する
  QUrl url(m_gasUrl);
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
  // GASのリダイレクト先(script.googleusercontent.com等)への切り替え時、QtのHTTP/2実装がコネクションを
  // 誤って再利用し "Host requires authentication"
  // で失敗することがあるため(QTBUG-97522)、HTTP/1.1に固定する
  request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

  QEventLoop eventLoop;
  QTimer timer;
  timer.setSingleShot(true);

  connect(&timer, &QTimer::timeout, &eventLoop, &QEventLoop::quit);
  connect(m_networkManager, &QNetworkAccessManager::finished, &eventLoop,
          &QEventLoop::quit);

  timer.start(30000);  // タイムアウト30秒
  QNetworkReply* reply = m_networkManager->post(request, postData);
  eventLoop.exec();

  bool success = false;
  QStringList previousValues;
  QString sceneNumberStr = m_cutNumbers.join("_");
  // 失敗した場合の原因。コンソールはGUIアプリでは見えないため、失敗時は
  // 書き込みキューファイル(.ini)にも記録して、後から原因を確認できるようにする
  QString errorDetail;
  QString responseExcerpt;  // ログ用。GASの応答本文の先頭部分
  int httpStatus =
      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

  if (timer.isActive() && reply->error() == QNetworkReply::NoError) {
    QByteArray responseData = reply->readAll();
    responseExcerpt = QString::fromUtf8(responseData.left(600)).simplified();
    QJsonDocument jsonResponse = QJsonDocument::fromJson(responseData);
    QJsonObject jsonObj        = jsonResponse.object();

    if (jsonObj.value("success").toBool()) {
      QJsonArray prevValsArray = jsonObj.value("prevValues").toArray();
      for (const auto& val : prevValsArray)
        previousValues.append(val.toString());
      success = true;
    } else if (jsonResponse.isNull() || !jsonResponse.isObject()) {
      // GASがJSONではなくHTML等（エラーページ、ログイン要求など）を返した場合
      errorDetail = QString(
                        "Unexpected (non-JSON) response. HTTP status=%1, "
                        "body(first 500 chars)=%2")
                        .arg(httpStatus)
                        .arg(QString::fromUtf8(responseData.left(500)));
    } else {
      errorDetail =
          QString("GAS Error: %1").arg(jsonObj.value("error").toString());
    }
  } else if (!timer.isActive()) {
    errorDetail = "Timeout (30s) waiting for the GAS response.";
  } else {
    errorDetail = QString(
                      "HTTP Connection Error: %1 "
                      "(QNetworkReply::NetworkError=%2, HTTP status=%3)")
                      .arg(reply->errorString())
                      .arg((int)reply->error())
                      .arg(httpStatus);
  }

  reply->deleteLater();

  if (success) {
    emit inputDone(sceneNumberStr, previousValues, m_requestId);
    // 書き込みキューファイルを削除する。QSettingsの未書き込みの変更が残っていると、削除後に
    // 再作成されてしまうため、先にsync()しておく。また、ネットワーク共有上ではファイル作成直後の
    // QFileInfo::exists()が古いキャッシュで偽になることがあり、存在確認を挟むと削除が
    // スキップされて「成功したのに.iniが残る」状態になり得るため、存在確認はせず直接削除する
    cueSetting.sync();
    bool removed = QFile::remove(googleCueFilepath);
    appendGoogleLog(
        googleCueFilepath,
        QString(
            "[OK] cue=%1.ini http=%2 elapsed=%3ms cueRemoved=%4 response=%5")
            .arg(googleCueFileName)
            .arg(httpStatus)
            .arg(totalTimer.elapsed())
            .arg(removed ? "yes" : "NO")
            .arg(responseExcerpt));
  } else {
    std::cout << errorDetail.toStdString() << std::endl;
    cueSetting.setValue("FailedAt",
                        QDateTime::currentDateTime().toString(Qt::ISODate));
    cueSetting.setValue("Error", errorDetail);
    cueSetting.sync();
    appendGoogleLog(googleCueFilepath,
                    QString("[FAILED] cue=%1.ini http=%2 elapsed=%3ms error=%4")
                        .arg(googleCueFileName)
                        .arg(httpStatus)
                        .arg(totalTimer.elapsed())
                        .arg(errorDetail));
    emit inputFailed(sceneNumberStr, m_requestId);
  }

  emit processEnd();
}

//-----------------------------------------------------------------------------------

GoogleGateway* GoogleGateway::instance() {
  static GoogleGateway _instance;
  return &_instance;
}

GoogleGateway::GoogleGateway(QObject* parent) : QObject(parent) {}

bool GoogleGateway::isActive() const {
  return !WorkflowManager::instance()->gasUrl().isEmpty();
}

bool GoogleGateway::updateSingleCell(QString sheetName, QStringList cutNumbers,
                                     QString flowName, QStringList values,
                                     QStringList& prevValues, int requestId) {
  QString gasUrl = WorkflowManager::instance()->gasUrl();
  if (gasUrl.isEmpty()) {
    std::cout << "Google GAS URL is not configured!" << std::endl;
    return false;
  }

  QThread* workerThread = new QThread(this);
  GoogleWorker* worker  = new GoogleWorker(gasUrl, sheetName, cutNumbers,
                                           flowName, values, requestId);

  worker->moveToThread(workerThread);

  connect(workerThread, &QThread::started, worker, &GoogleWorker::process);
  connect(worker, &GoogleWorker::processEnd, workerThread, &QThread::quit);
  connect(worker, &GoogleWorker::processEnd, worker, &QObject::deleteLater);
  connect(workerThread, &QThread::finished, workerThread,
          &QObject::deleteLater);
  connect(worker, &GoogleWorker::inputDone, this,
          &GoogleGateway::googleSpreadSheetInput);
  connect(worker, &GoogleWorker::inputFailed, this,
          &GoogleGateway::googleSpreadSheetInputFailed);
  // processEndは成功・失敗のどちらでも必ず発行される。thisをコンテキストにしているため、
  // カウンタの更新はメインスレッド上で行われ、inputDone/inputFailedの後に処理される
  connect(worker, &GoogleWorker::processEnd, this,
          [this]() { --m_pendingCount; });

  ++m_pendingCount;
  workerThread->start();

  return true;
}
