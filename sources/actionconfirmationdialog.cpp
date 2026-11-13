#include "sharedfiles.h"
#include "actionconfirmationdialog.h"
#include "platform.h"
#include "workflow.h"
#include "googlegateway.h"
#include "googlechatgateway.h"
#include "usersettings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QVariant>
#include <QPainter>
#include <QPushButton>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <iostream>
#include <QCryptographicHash>
#include <QListWidget>
#include <QDirIterator>
#include <QButtonGroup>
#include <QRadioButton>
#include <QTextStream>
#include <QComboBox>
#include <QTabWidget>
#include <QSet>
#include <QScrollArea>
#include <QPlainTextEdit>
#include <QDateTime>
#include <algorithm>
#include <QHash>

#include <QJsonArray>

namespace {
const QString GengaSuffix = "_genga_ts";
const QString DougaSuffix = "_douga_ts";
const QString takeStr(int takeNumber) {
  return (takeNumber == 1) ? ""
                           : QString("_t%1").arg(QString::number(takeNumber));
}
const QString RewindBkupFolderBaseName = "old";

QByteArray fileChecksum(const QString& Fname) {
  QFile f(Fname);
  if (f.open(QFile::ReadOnly)) {
    QCryptographicHash hash(QCryptographicHash::Keccak_256);
    if (hash.addData(&f)) {
      return hash.result().toHex();
    }
  }
  return QByteArray();
}

bool hasImageExtension(const QString& toFileName) {
  const QStringList imgExts = {"bmp", "jpeg", "jpg", "png", "gif",
                               "icn", "icns", "svg", "tga", "tiff",
                               "tif", "wbmp", "webp"};
  return imgExts.contains(QFileInfo(toFileName).suffix().toLower());
}

bool replaceExtension(QString& fileName, const QString& extension) {
  if (QFileInfo(fileName).suffix() == extension) return false;
  int dotIndex = fileName.lastIndexOf(".");
  if (dotIndex == -1) return false;
  fileName.truncate(dotIndex + 1);
  fileName += extension;
  return true;
}

const QIcon color2Icon(const QColor& color) {
  QPixmap pm(15, 15);
  pm.fill(color);
  return QIcon(pm);
}

// 1 -> ""  2 -> "_t02"  3 -> "_t03"
QString takeNum2Suffix(const int take) {
  if (take == 1) return "";
  return QString("_t%1").arg(QString::number(take).rightJustified(2, '0'));
}

// 撮影のみで使うテイク表記
// 2 -> "_r"  3 -> "_r02"
QString takeNum2RSuffix(const int take) {
  if (take == 1)
    return "";
  else if (take == 2)
    return "_r";
  return QString("_r%1").arg(QString::number(take - 1).rightJustified(2, '0'));
}
}  // namespace

class IconWidget : public QWidget {
  ActionStepType m_type;
  bool m_isUndo;
  bool m_showWarning;

public:
  IconWidget(ActionStepType type, bool isUndo, bool showWarning = false)
      : m_type(type), m_isUndo(isUndo), m_showWarning(showWarning) {
    setFixedSize(48, 48);
  }

protected:
  void paintEvent(QPaintEvent* event) {
    QString pmPath;
    switch (m_type) {
    case Step_Rename:
    case Step_Copy:
      pmPath = ":Resources/ppm64.png";
      break;
    case Step_RenameFile:
      pmPath = ":Resources/renameFile64.svg";
      break;
    case Step_CopyFile:
      pmPath = ":Resources/copyFile64.svg";
      break;
    case Step_CopyFolderContents:
      pmPath = ":Resources/copyFolder64.svg";
      break;
    case Step_MoveFlowGroup:
      pmPath = ":Resources/moveFlow64.png";
      break;
    case Step_CheckExistence:
      pmPath = (m_showWarning) ? ":Resources/check_ng64.svg"
                               : ":Resources/check_ok64.svg";
      break;
    case Step_AskDelete:
      pmPath = ":Resources/delete64.svg";
      break;
    case Step_CreateCueFile:
      pmPath = ":Resources/cue64.svg";
      break;
    case Step_StartBranchFlow:
      pmPath = ":Resources/start_branch64.png";
      break;
    case Step_EndBranchFlow:
      pmPath = ":Resources/end_branch64.png";
      break;
    case Step_SetSceneInfo:
      pmPath = ":Resources/setInfo64.svg";
      break;
    case Step_InputGoogleSpreadsheet:
      pmPath = ":Resources/googleSheets64.png";
      break;
    case Step_CreateEmptyFolder:
      pmPath = ":Resources/new-folder.svg";
      break;
    case Step_ZipFolder:
      pmPath = ":Resources/zip-folder.svg";
      break;
    case Step_NotifyGoogleChat:
      pmPath = ":Resources/googleChat64.png";
      break;
    case Step_ConditionalBranchByInfo:
      pmPath = ":Resources/branch_info64.svg";
      break;
    case Step_ConditionalBranchByFileExistence:
      pmPath = ":Resources/branch_file64.svg";
      break;
    default:
      pmPath = ":Resources/ppm64.png";
      break;
    }
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.scale(0.75, 0.75);
    p.drawPixmap(0, 0, QPixmap(pmPath));
    if (m_isUndo) p.drawPixmap(0, 0, QPixmap(":Resources/undo64.png"));
    if (m_showWarning && m_type != Step_CheckExistence)
      p.drawPixmap(0, 0, QPixmap(":Resources/warning64.png"));
  }
};

//--------------------------------------------------

QString StepExecutor::realPath(const QString& displayPath,
                               const QString& sceneNumber,
                               const QString& stockSceneNumbers) const {
  return SharedFiles::actualPath(
      WorkflowManager::instance()->decodeObfuscatedPathMacros(
          displayPath, m_pathSequence,
          sceneNumber.isEmpty() ? m_pathSceneNumber : sceneNumber,
          stockSceneNumbers));
}

QString StepExecutor::hide(const QString& text) const {
  return WorkflowManager::instance()->hideObfuscatedPaths(text, m_pathSequence,
                                                          m_pathSceneNumber);
}
bool StepExecutor::createPathAndCopyFile(const QString& fileName,
                                         const QString& newName, QString& log) {
  QDir parentDir = QFileInfo(newName).dir();
  if (!parentDir.exists()) {
    QMessageBox::StandardButton ret = QMessageBox::question(
        nullptr, tr("Question"),
        tr("The folder %1 does not exist.\nDo you want to create it?")
            .arg(hide(parentDir.absolutePath())),
        {QMessageBox::Yes | QMessageBox::Cancel}, QMessageBox::Yes);
    if (ret == QMessageBox::Cancel) {
      log.append(tr("Canceled to create folder %1.\n")
                     .arg(hide(parentDir.absolutePath())));
      return false;
    }

    bool ok = parentDir.mkpath(".");
    if (!ok) {
      log.append(tr("Failed to create folder %1.\n")
                     .arg(hide(parentDir.absolutePath())));
      return false;
    }
  }
  return QFile::copy(fileName, newName);
}

//--------------------------------------------------
// フォルダの内容ごとコピー（Step_Copy/Step_CopyFile共用）

QVariant StepExecutor::FolderCopyResult::toVariant() const {
  return QVariantList{overwritten, copiedFiles, createdDirs};
}

StepExecutor::FolderCopyResult StepExecutor::FolderCopyResult::fromVariant(
    const QVariant& v) {
  FolderCopyResult result;
  QVariantList list = v.toList();
  if (list.size() >= 3) {
    result.overwritten = list.at(0).toBool();
    result.copiedFiles = list.at(1).toStringList();
    result.createdDirs = list.at(2).toStringList();
  }
  return result;
}

bool StepExecutor::copyFolderRecursively(const QString& fromPath,
                                         const QString& toPath,
                                         bool& overwriteAll, QString& log,
                                         FolderCopyResult& result) {
  const QString from = QDir::cleanPath(QDir(fromPath).absolutePath());
  const QString to   = QDir::cleanPath(QDir(toPath).absolutePath());

  // コピー先がコピー元自身、またはその内側にあると、コピーしたファイルまで再びコピー対象になってしまう
  if (toPath.isEmpty() || fromPath.isEmpty()) {
    log += tr("Ambiguous or empty file path.\n");
    return false;
  }
  if (SharedFiles::insideSource(from, to)) {
    log.append(
        tr("(The destination is the source folder itself or is inside it) "));
    return false;
  }
  if (QFileInfo::exists(to) && !QFileInfo(to).isDir()) {
    log.append(
        tr("(A file with the same name as the destination folder already "
           "exists) "));
    return false;
  }

  // コピーを始める前に、コピー元の全エントリを列挙しておく（隠しファイルも含める）
  QList<QFileInfo> entries;
  QDirIterator it(
      from,
      QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
      QDirIterator::Subdirectories);
  while (it.hasNext()) {
    it.next();
    if (SharedFiles::isMetadata(it.fileInfo())) continue;
    if (SharedFiles::actualPath(it.filePath()).isEmpty()) {
      log += tr("Multiple file names have the same Unicode spelling.\n");
      return false;
    }
    entries.append(it.fileInfo());
  }

  // コピー先フォルダ（と、存在しない親フォルダ）を作る
  QStringList missingDirs;
  for (QString p = to; !QFileInfo::exists(p);) {
    missingDirs.prepend(p);
    QString parent = QFileInfo(p).dir().absolutePath();
    if (QDir::cleanPath(parent) == p) break;
    p = QDir::cleanPath(parent);
  }
  if (missingDirs.size() > 1) {
    QString parentPath              = QFileInfo(to).dir().absolutePath();
    QMessageBox::StandardButton ret = QMessageBox::question(
        nullptr, tr("Question"),
        tr("The folder %1 does not exist.\nDo you want to create it?")
            .arg(hide(parentPath)),
        {QMessageBox::Yes | QMessageBox::Cancel}, QMessageBox::Yes);
    if (ret == QMessageBox::Cancel) {
      log.append(tr("(Canceled to create folder %1) ").arg(hide(parentPath)));
      return false;
    }
  }
  if (!missingDirs.isEmpty()) {
    if (!QDir().mkpath(to)) {
      log.append(tr("(Failed to create folder %1) ").arg(hide(to)));
      return false;
    }
    result.createdDirs.append(missingDirs);
  }

  for (const QFileInfo& fi : entries) {
    QString rel = QDir::cleanPath(fi.absoluteFilePath()).mid(from.length());
    if (!rel.startsWith('/')) rel.prepend('/');
    const QString dest = SharedFiles::actualPath(QDir::cleanPath(to + rel));
    if (dest.isEmpty() || SharedFiles::sameEntry(fi.absoluteFilePath(), dest)) {
      log += tr("Ambiguous source or destination.\n");
      return false;
    }

    if (fi.isDir()) {
      if (!QFileInfo::exists(dest)) {
        if (!QDir().mkpath(dest)) {
          log.append(tr("(Failed to create folder %1) ").arg(hide(dest)));
          return false;
        }
        result.createdDirs.append(dest);
      } else if (!QFileInfo(dest).isDir()) {
        log.append(
            tr("(A file with the same name as the folder %1 already exists) ")
                .arg(hide(dest)));
        return false;
      }
      continue;
    }

    QFileInfo destInfo(dest);
    if (destInfo.exists()) {
      if (destInfo.isDir()) {
        log.append(
            tr("(A folder with the same name as the file %1 already exists) ")
                .arg(hide(dest)));
        return false;
      }
      if (!overwriteAll) {
        QMessageBox::StandardButton owRet = QMessageBox::question(
            nullptr, tr("Question"),
            tr("The file %1 already exists.\nDo you want to overwrite it?")
                .arg(hide(dest)),
            {QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::Cancel},
            QMessageBox::Cancel);
        if (owRet == QMessageBox::Cancel) {
          log.append(tr("(Canceled) "));
          return false;
        }
        if (owRet == QMessageBox::YesToAll) overwriteAll = true;
      }
      if (!QFile::remove(dest)) {
        log.append(
            tr("(Failed to remove the existing file %1) ").arg(hide(dest)));
        return false;
      }
      result.overwritten = true;
    } else if (!QFileInfo::exists(destInfo.absolutePath())) {
      // 通常はフォルダのエントリが先に処理されて作成済みのはずだが、念のため
      if (!QDir().mkpath(destInfo.absolutePath())) {
        log.append(tr("(Failed to create folder %1) ")
                       .arg(hide(destInfo.absolutePath())));
        return false;
      }
      result.createdDirs.append(destInfo.absolutePath());
    }

    if (!QFile::copy(fi.absoluteFilePath(), dest)) {
      log.append(
          tr("(Failed to copy the file %1) ").arg(hide(fi.absoluteFilePath())));
      return false;
    }
    result.copiedFiles.append(dest);
  }
  return true;
}

bool StepExecutor::copyFolderToAll(const QString& fromPath,
                                   const QStringList& toPaths, QString& log,
                                   QVariant& checkSum) {
  bool ok           = true;
  bool overwriteAll = false;
  QVariantList results;
  for (const QString& toPath : toPaths) {
    log.append(tr("To: %1\n").arg(hide(toPath)));
    log.append(tr("  Copy folder: "));
    FolderCopyResult result;
    ok = copyFolderRecursively(fromPath, toPath, overwriteAll, log, result);
    if (!ok) {
      log.append(tr("Failed.\n"));
      break;
    }
    log.append(
        tr("Done. (%1 file(s) copied)\n").arg(result.copiedFiles.size()));
    results.append(result.toVariant());
  }
  checkSum = results;
  return ok;
}

bool StepExecutor::undoFolderCopyAll(const QVariant& checkSum, QString& log) {
  bool ok = true;
  for (const QVariant& v : checkSum.toList()) {
    FolderCopyResult result = FolderCopyResult::fromVariant(v);

    log.append(tr("  Remove copied files: "));
    int removedCount = 0;
    QStringList failedFiles;
    for (const QString& recorded : result.copiedFiles) {
      const QString file = SharedFiles::actualPath(recorded);
      if (file.isEmpty()) {
        log += tr("Ambiguous or empty file path.\n");
        return false;
      }
      if (!QFileInfo::exists(file)) continue;  // 既に無いものは何もしない
      if (QFile::remove(file))
        removedCount++;
      else
        failedFiles.append(file);
    }
    if (failedFiles.isEmpty())
      log.append(tr("Done. (%1 file(s) removed)\n").arg(removedCount));
    else {
      ok = false;
      log.append(tr("Failed.\n"));
      for (const QString& file : failedFiles)
        log.append(tr("    Could not remove: %1\n").arg(hide(file)));
    }

    // 新規に作ったフォルダは、深い順に、空になっているものだけ削除する
    // （作成後に他のファイルが入れられたフォルダは残す）
    QStringList dirs = result.createdDirs;
    std::sort(dirs.begin(), dirs.end(), [](const QString& a, const QString& b) {
      return a.length() > b.length();
    });
    for (const QString& recorded : dirs) {
      const QString dir = SharedFiles::actualPath(recorded);
      if (dir.isEmpty()) {
        log += tr("Ambiguous or empty file path.\n");
        return false;
      }
      if (!QFileInfo::exists(dir)) continue;
      auto cleanup = SharedFiles::removeEmptyFolder(dir);
      if (cleanup != SharedFiles::Cleanup::Kept) {
        if (cleanup == SharedFiles::Cleanup::Failed) {
          ok = false;
          log.append(tr("  Failed to remove the folder %1\n").arg(hide(dir)));
        }
      } else
        log.append(tr("  Kept the folder %1 because it is not empty.\n")
                       .arg(hide(dir)));
    }
  }
  return ok;
}

bool StepExecutor::isFolderCopyCheckSum(const QVariant& checkSum) {
  return checkSum.typeId() == QMetaType::QVariantList;
}

bool StepExecutor::folderCopyCheckSumHasOverwrite(const QVariant& checkSum) {
  for (const QVariant& v : checkSum.toList())
    if (FolderCopyResult::fromVariant(v).overwritten) return true;
  return false;
}

//--------------------------------------------------

StepExecutor_Rename::StepExecutor_Rename(const ActionStep& step,
                                         QStringList orgPaths, QString sequence,
                                         QString sceneNumber, bool isUndo)
    : StepExecutor(isUndo), m_orgPaths(orgPaths) {
  setPathContext(sequence, sceneNumber);
  if (m_orgPaths.size() == 1) {
    m_pathSceneNumbers.append(sceneNumber);
    QStringList list;
    for (auto& param : step.params) {
      // renameToは、難読化マクロ以外のパスが全てデコードされたフルパスになる。
      // 難読化マクロは、そのまま残す（操作に使う直前に、realRenameTo()でデコードする）
      QString renameTo = WorkflowManager::instance()->decodePath(
          param.toString(), sequence, sceneNumber);
      list.append(renameTo);
    }
    m_renameToList.append(list);
  }
  // 兼用カットのtnzの場合
  else {
    assert(sceneNumber.contains("_"));
    m_stockSceneNumbers   = sceneNumber;
    QStringList sceneNums = sceneNumber.split("_", Qt::SkipEmptyParts);
    for (auto orgPath : orgPaths) {
      QString _sceneNumber = sceneNumber;
      QString orgFileName =
          orgPath.right(orgPath.size() - orgPath.lastIndexOf('/'));
      for (auto sceneNum : sceneNums) {
        // カット番号を差し替え
        if (orgFileName.contains(sceneNum)) {
          _sceneNumber = sceneNum;
          break;
        }
      }

      m_pathSceneNumbers.append(_sceneNumber);
      QStringList list;
      for (auto& param : step.params) {
        // renameToは、難読化マクロ以外のパスが全てデコードされたフルパスになる
        QString renameTo = WorkflowManager::instance()->decodePath(
            param.toString(), sequence, _sceneNumber, sceneNumber);
        list.append(renameTo);
      }
      m_renameToList.append(list);
    }
  }
}

QWidget* StepExecutor_Rename::createWidget() {
  QWidget* widget = new QWidget();

  QStringList txts;
  bool showWarning = false;

  int i = 0;
  for (auto orgPath : m_orgPaths) {
    QString shortenOrgPath =
        WorkflowManager::instance()->relativePathFromProjectRoot(hide(orgPath));

    QString txt;
    if (!m_isUndo) {
      txt = tr("%1\n\nwill be renamed / copied to the following path(s):\n")
                .arg(shortenOrgPath);
      for (auto renameTo : m_renameToList[i]) {
        txt += tr("\n - %1").arg(
            WorkflowManager::instance()->relativePathFromProjectRoot(renameTo));
      }
    } else {  // undo case
      bool tmp_showWarning = true;
      txt                  = tr("The following file(s) will be removed:\n");
      for (auto renameTo : m_renameToList[i]) {
        txt += tr("\n - %1").arg(
            WorkflowManager::instance()->relativePathFromProjectRoot(renameTo));
        // ハッシュ値が一致しているかをチェックする
        // 一つでも一致していればOK
        if (tmp_showWarning) {
          for (auto renameFrom : m_renameToList[i]) {
            if (fileChecksum(realRenameTo(i, renameFrom)) ==
                m_checkSum.toList()[i].toByteArray())
              tmp_showWarning = false;
          }
        }
      }
      txt += tr("\nand\n%1will be reverted.").arg(shortenOrgPath);

      if (tmp_showWarning) {
        txt +=
            tr("\n\n[WARNING] The hash value of the renamed file has changed.\n"
               "The undo operation may not restore the original file.");
        showWarning = true;
      }
    }
    txts.append(txt);
    i++;
  }

  QHBoxLayout* lay = new QHBoxLayout();
  lay->setContentsMargins(10, 10, 10, 10);
  lay->setSpacing(10);
  lay->addWidget(new IconWidget(Step_Rename, m_isUndo, showWarning), 0);
  QVBoxLayout* labelLay = new QVBoxLayout();
  labelLay->setContentsMargins(0, 0, 0, 0);
  labelLay->setSpacing(15);
  {
    for (auto txt : txts) labelLay->addWidget(new QLabel(txt, widget));
  }
  lay->addLayout(labelLay, 1);
  widget->setLayout(lay);

  return widget;
}

bool StepExecutor_Rename::execute(QString& log, QVariant& checkSum) {
  if (!m_isUndo)
    return execute_rename(log, checkSum);
  else
    return execute_undo(log);
}

bool StepExecutor_Rename::execute_rename(QString& log, QVariant& checkSum) {
  bool ok = true;
  QList<QVariant> checkSumList;
  for (int j = 0; j < m_orgPaths.size(); j++) {
    m_orgPaths[j] = SharedFiles::actualPath(m_orgPaths[j]);
    if (!QFileInfo(m_orgPaths[j]).isFile()) {
      log += tr("Ambiguous or missing source file.\n");
      return false;
    }
    log.append(tr("[RENAME]\nFrom: %1\n").arg(hide(m_orgPaths[j])));

    QMessageBox::StandardButton owRet = QMessageBox::Cancel;

    checkSumList.append(fileChecksum(m_orgPaths[j]));
    // std::cout << "orgfile = " <<
    // QString::fromUtf8(fileChecksum(m_orgPath)).toStdString() << std::endl;

    for (int i = 0; i < m_renameToList[j].size(); i++) {
      // copyToは表示・ログ用（難読化マクロのまま）。ファイル操作には、デコードしたcopyToRealを使う
      QString copyTo     = m_renameToList[j].at(i);
      QString copyToReal = realRenameTo(j, copyTo);

      log.append(tr("To: %1\n").arg(copyTo));

      if (copyToReal.isEmpty() ||
          SharedFiles::sameEntry(m_orgPaths[j], copyToReal)) {
        log += tr("Ambiguous source or destination.\n");
        return false;
      }
      if (QFileInfo(copyToReal).isFile()) {
        // if (QFile::exists(copyTo)) { //←本当はシンプルにこちらにしたいが、
        // 正しくファイルの有無を読み取ってくれないようなので書き換えた
        log.append(tr("  Remove existing file: "));

        if (owRet != QMessageBox::YesToAll) {
          owRet = QMessageBox::question(
              nullptr, tr("Question"),
              tr("The file %1 already exists.\nDo you want to overwrite it?")
                  .arg(copyTo),
              {QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::Cancel},
              owRet);
        }
        if (owRet == QMessageBox::Cancel) {
          ok = false;
          log.append(tr("Canceled.\n"));
          break;
        }
        ok = QFile::remove(copyToReal);
        if (!ok) {
          log.append(tr("Failed.\n"));
          break;
        } else
          log.append(tr("Done.\n"));
      }

      log.append(tr("  Copy file: "));
      ok = createPathAndCopyFile(m_orgPaths[j], copyToReal, log);
      // std::cout << "copied = " <<
      // QString::fromUtf8(fileChecksum(copyTo)).toStdString() << std::endl;
      // progressLabelTexts[i].append((ok) ? tr(" Done.") : tr(" Failed."));
      if (!ok) {
        log.append(tr("Failed.\n"));
        break;
      } else
        log.append(tr("Done.\n"));
    }
    // remove original file
    log.append(tr("  Remove original file: "));
    if (ok) {
      ok = QFile::remove(m_orgPaths[j]);
      if (!ok)
        log.append(tr("Failed.\n"));
      else
        log.append(tr("Done.\n"));
    } else
      break;
  }
  checkSum = checkSumList;
  return ok;
}

bool StepExecutor_Rename::execute_undo(QString& log) {
  QList<QVariant> checkSumList = m_checkSum.toList();
  bool ok                      = true;
  int i                        = 0;
  for (auto orgPath : m_orgPaths) {
    QString revertFromPath;
    // 復帰元のパスを決める
    //  revertFromPathは表示・ログ用（難読化マクロのまま）。ファイル操作には、デコードして使う
    for (auto copyFromPath : m_renameToList[i]) {
      if (fileChecksum(realRenameTo(i, copyFromPath)) ==
          checkSumList[i].toByteArray()) {
        revertFromPath = copyFromPath;
        break;
      } else if (revertFromPath.isEmpty())
        revertFromPath = copyFromPath;
    }
    if (revertFromPath.isEmpty()) {
      log.append(tr(
          "[UNDO_RENAME]\nFailed reverting: The renamed files are missing."));
      return false;
    }

    log.append(tr("[UNDO_RENAME]\nFrom: %1\nTo: %2\n")
                   .arg(revertFromPath)
                   .arg(hide(orgPath)));

    orgPath = SharedFiles::actualPath(orgPath);
    if (!QFileInfo(realRenameTo(i, revertFromPath)).isFile() ||
        orgPath.isEmpty() ||
        SharedFiles::sameEntry(orgPath, realRenameTo(i, revertFromPath))) {
      log += tr("Ambiguous source or destination.\n");
      return false;
    }
    if (QFileInfo(orgPath).isFile()) {
      // if (QFile::exists(orgPath)) { //←本当はシンプルにこちらにしたいが、
      // 正しくファイルの有無を読み取ってくれないようなので書き換えた
      log.append(tr("  Removing original file: \n"));
      QMessageBox::StandardButton ret = QMessageBox::question(
          nullptr, tr("Question"),
          tr("The file %1 already exists.\nDo you want to overwrite it?")
              .arg(hide(orgPath)),
          {QMessageBox::Yes | QMessageBox::Cancel}, QMessageBox::Yes);
      if (ret == QMessageBox::Cancel) {
        log.append(tr("Canceled.\n"));
        return false;
      }
      if (!QFile::remove(orgPath)) {
        log.append(tr("Failed.\n"));
        return false;
      } else
        log.append(tr("Done.\n"));
    }

    log.append(tr("  Copy file: "));
    if (!createPathAndCopyFile(realRenameTo(i, revertFromPath), orgPath, log)) {
      log.append(tr("Failed.\n"));
      return false;
    } else
      log.append(tr("Done.\n"));

    // remove original file
    for (auto removePath : m_renameToList[i]) {
      log.append(tr("  Remove renamed files: "));
      if (ok) {
        ok = QFile::remove(realRenameTo(i, removePath));
        if (!ok)
          log.append(tr("Failed.\n"));
        else
          log.append(tr("Done.\n"));
      }
    }
    if (!ok) break;

    i++;
  }

  return ok;
}

QJsonObject StepExecutor_Rename::toJson(const QList<QVariant>& params) {
  QJsonObject retObj;
  QJsonArray renameToArray;
  for (auto param : params) renameToArray.append(param.toString());
  retObj["RenameTo"] = renameToArray;
  return retObj;
}

QVariantList StepExecutor_Rename::fromJson(const QJsonObject& paramsObj) {
  QVariantList params;

  if (const QJsonValue v = paramsObj["RenameTo"]; v.isArray()) {
    const QJsonArray renameToArray = v.toArray();
    for (const QJsonValue& renameToVal : renameToArray)
      params.append(renameToVal.toString());
  }

  return params;
}

//--------------------------------------------------

StepExecutor_Copy::StepExecutor_Copy(const ActionStep& step, QString orgPath,
                                     QString sequence, QString sceneNumber,
                                     bool isUndo)
    : StepExecutor(isUndo), m_orgPath(orgPath) {
  setPathContext(sequence, sceneNumber);
  for (auto& param : step.params) {
    QString copyTo = WorkflowManager::instance()->decodePath(
        param.toString(), sequence, sceneNumber);
    m_copyToList.append(copyTo);
  }
}

QWidget* StepExecutor_Copy::createWidget() {
  QString shortenOrgPath =
      WorkflowManager::instance()->relativePathFromProjectRoot(hide(m_orgPath));

  QWidget* widget = new QWidget();

  QString txt;
  bool showWarning = false;
  if (!m_isUndo) {
    // コピー元がフォルダの場合は、内容ごとコピーされる（コピー先パス自体がそのコピーになる）
    if (QFileInfo(m_orgPath).isDir())
      txt = tr("The folder %1\n(including all of its contents)\n\nwill be "
               "copied to the following folder path(s):\n")
                .arg(shortenOrgPath);
    else
      txt = tr("%1\n\nwill be copied to the following path(s):\n")
                .arg(shortenOrgPath);
    for (auto copyTo : m_copyToList) {
      txt += tr("\n - %1").arg(
          WorkflowManager::instance()->relativePathFromProjectRoot(copyTo));
    }
  } else {  // undo case
    bool isFolderCopy = isFolderCopyCheckSum(m_checkSum);
    txt = isFolderCopy ? tr("The files and folders created by the copy will be "
                            "removed from:\n")
                       : tr("The following file(s) will be removed:\n");
    for (auto copyTo : m_copyToList) {
      txt += tr("\n - %1").arg(
          WorkflowManager::instance()->relativePathFromProjectRoot(copyTo));
    }
    if (isFolderCopy) {
      txt +=
          tr("\n\nFiles and folders that already existed in the destination "
             "are not removed.");
      if (folderCopyCheckSumHasOverwrite(m_checkSum)) {
        txt += tr(
            "\n\n[WARNING] Some existing files were overwritten by the copy.\n"
            "The undo operation cannot restore them.");
        showWarning = true;
      }
    }
  }

  QHBoxLayout* lay = new QHBoxLayout();
  lay->setContentsMargins(10, 10, 10, 10);
  lay->setSpacing(10);
  lay->addWidget(new IconWidget(Step_Copy, m_isUndo, showWarning), 0);
  lay->addWidget(new QLabel(txt, widget), 1);
  widget->setLayout(lay);

  return widget;
}

bool StepExecutor_Copy::execute(QString& log, QVariant& checkSum) {
  if (!m_isUndo)
    return execute_copy(log, checkSum);
  else
    return execute_undo(log);
}

bool StepExecutor_Copy::execute_copy(QString& log, QVariant& checkSum) {
  m_orgPath = SharedFiles::actualPath(m_orgPath);
  if (!QFileInfo::exists(m_orgPath)) {
    log += tr("Ambiguous or missing source file.\n");
    return false;
  }
  log.append(tr("[COPY]\nFrom: %1\n").arg(hide(m_orgPath)));

  // コピー元がフォルダの場合は、内容ごとコピーする
  if (QFileInfo(m_orgPath).isDir()) {
    // 難読化マクロは、操作に使うここで、実際のパスにデコードする
    QStringList realCopyToList;
    for (const QString& copyTo : m_copyToList)
      realCopyToList.append(realPath(copyTo));
    return copyFolderToAll(m_orgPath, realCopyToList, log, checkSum);
  }

  bool ok                           = true;
  QMessageBox::StandardButton owRet = QMessageBox::Cancel;

  checkSum = fileChecksum(m_orgPath);
  // std::cout << "orgfile = " <<
  // QString::fromUtf8(fileChecksum(m_orgPath)).toStdString() << std::endl;

  for (int i = 0; i < m_copyToList.size(); i++) {
    // copyToは表示・ログ用（難読化マクロのまま）。ファイル操作には、デコードしたcopyToRealを使う
    QString copyTo     = m_copyToList.at(i);
    QString copyToReal = realPath(copyTo);

    log.append(tr("To: %1\n").arg(copyTo));

    if (copyToReal.isEmpty() || SharedFiles::sameEntry(m_orgPath, copyToReal)) {
      log += tr("Ambiguous source or destination.\n");
      return false;
    }
    if (QFile::exists(copyToReal)) {
      log.append(tr("  Remove existing file: "));

      if (owRet != QMessageBox::YesToAll) {
        owRet = QMessageBox::question(
            nullptr, tr("Question"),
            tr("The file %1 already exists.\nDo you want to overwrite it?")
                .arg(copyTo),
            {QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::Cancel},
            owRet);
      }
      if (owRet == QMessageBox::Cancel) {
        ok = false;
        log.append(tr("Canceled.\n"));
        break;
      }
      ok = QFile::remove(copyToReal);
      if (!ok) {
        log.append(tr("Failed.\n"));
        break;
      } else
        log.append(tr("Done.\n"));
    }

    log.append(tr("  Copy file: "));
    ok = createPathAndCopyFile(m_orgPath, copyToReal, log);
    // std::cout << "copied = " <<
    // QString::fromUtf8(fileChecksum(copyTo)).toStdString() << std::endl;
    // progressLabelTexts[i].append((ok) ? tr(" Done.") : tr(" Failed."));
    if (!ok) {
      log.append(tr("Failed.\n"));
      break;
    } else
      log.append(tr("Done.\n"));
  }

  return ok;
}

bool StepExecutor_Copy::execute_undo(QString& log) {
  log.append(tr("[UNDO_COPY]\n"));

  // フォルダのコピーだった場合は、コピーで作られたファイル・フォルダだけを削除する
  if (isFolderCopyCheckSum(m_checkSum))
    return undoFolderCopyAll(m_checkSum, log);

  // remove original file
  bool ok = true;
  for (auto removePath : m_copyToList) {
    log.append(tr("  Remove copied files: "));
    if (ok) {
      ok = QFile::remove(realPath(removePath));
      if (!ok)
        log.append(tr("Failed.\n"));
      else
        log.append(tr("Done.\n"));
    }
  }

  return ok;
}

QJsonObject StepExecutor_Copy::toJson(const QList<QVariant>& params) {
  QJsonObject retObj;
  QJsonArray copyToArray;
  for (auto param : params) copyToArray.append(param.toString());
  retObj["CopyTo"] = copyToArray;
  return retObj;
}

QVariantList StepExecutor_Copy::fromJson(const QJsonObject& paramsObj) {
  QVariantList params;

  if (const QJsonValue v = paramsObj["CopyTo"]; v.isArray()) {
    const QJsonArray copyToArray = v.toArray();
    for (const QJsonValue& copyToVal : copyToArray)
      params.append(copyToVal.toString());
  }

  return params;
}

//--------------------------------------------------

ScriptFileRenamer::ScriptFileRenamer(const QString& scriptId,
                                     const QVariantMap& context)
    : m_script(scriptId, context) {}

const QString ScriptFileRenamer::ruleName() { return m_script.info().name; }

const QString ScriptFileRenamer::ruleDetails() {
  // 説明が無い場合は、スクリプトのファイルの場所を示す
  if (m_script.info().description.isEmpty())
    return tr("Script: %1").arg(m_script.info().filePath);
  return m_script.info().description;
}

bool ScriptFileRenamer::isValid(QString& error) {
  if (m_script.isValid()) return true;
  error = m_script.info().error;
  return false;
}

void ScriptFileRenamer::beginBatch() {
  // 多数のファイルを続けてリネームする間の時間制限（暴走したスクリプトを止める）
  m_script.beginBatch(30000);
}

void ScriptFileRenamer::endBatch() { m_script.endBatch(); }

bool ScriptFileRenamer::renamedRelativePath(const QString& relativePath,
                                            QString& result, QString& error) {
  return m_script.rename(relativePath, result, error);
}

//--------------------------------------------------

int MyFolderTree::treeCount(QTreeWidgetItem* parent) {
  int count = 0;
  if (parent == nullptr) {
    int topCount = topLevelItemCount();
    for (int i = 0; i < topCount; i++) {
      QTreeWidgetItem* item = topLevelItem(i);
      if (item->isExpanded()) {
        count += treeCount(item);
      }
    }
    count += topCount;
  } else {
    int childCount = parent->childCount();
    for (int i = 0; i < childCount; i++) {
      QTreeWidgetItem* item = parent->child(i);
      if (item->isExpanded()) {
        count += treeCount(item);
      }
    }
    count += childCount;
  }
  return count;
}

MyFolderTree::MyFolderTree(QWidget* parent, const QString rootPath,
                           int maxFolderDepth, bool isAllMode,
                           QStringList filesToBeCopied,
                           QStringList dirsToBeCopied, QStringList whiteList,
                           QStringList blackList)
    : QTreeWidget(parent), m_maxFolderDepth(maxFolderDepth) {
  invisibleRootItem()->setData(0, Role_Path, rootPath);
  createTreeChildren(invisibleRootItem(), 1, isAllMode, filesToBeCopied,
                     dirsToBeCopied, whiteList, blackList);
  setHeaderHidden(true);

  connect(this, SIGNAL(itemCollapsed(QTreeWidgetItem*)), this,
          SLOT(updateHeight()));
  connect(this, SIGNAL(itemExpanded(QTreeWidgetItem*)), this,
          SLOT(updateHeight()));
  updateHeight();
}

void MyFolderTree::updateHeight() {
  int _treeCount = std::max(std::min(treeCount(), 5), 2);
  setMaximumHeight((_treeCount + 1) *
                   rowHeight(indexFromItem(topLevelItem(0))));
}

void MyFolderTree::createTreeChildren(QTreeWidgetItem* parentItem, int depth,
                                      bool isAllMode,
                                      QStringList filesToBeCopied,
                                      QStringList dirsToBeCopied,
                                      QStringList whiteList,
                                      QStringList blackList) {
  QString folderPath = parentItem->data(0, Role_Path).toString();
  if (folderPath.isEmpty()) return;

  // std::cout << "folderPath = " << folderPath.toStdString() << std::endl;
  QStringList files =
      QDir(folderPath)
          .entryList(QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
                     QDir::Name);
  QStringList dirs =
      QDir(folderPath)
          .entryList(QDir::Dirs | QDir::Readable | QDir::NoDotAndDotDot,
                     QDir::Name);
  for (auto file : files) {
    if (SharedFiles::isMetadata(QFileInfo(QDir(folderPath), file))) continue;
    QTreeWidgetItem* fileItem = new QTreeWidgetItem(parentItem, {file});
    fileItem->setData(0, Role_Path, folderPath + "/" + file);
    fileItem->setData(0, Role_IsDir, false);
    fileItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled |
                       Qt::ItemIsAutoTristate);

    bool doCheck =
        ((depth == 1 && filesToBeCopied.contains(fileItem->text(0))) ||
         isAllMode);
    if (depth == 1) {
      if (whiteList.contains(fileItem->text(0))) doCheck = true;
      if (blackList.contains(fileItem->text(0))) doCheck = false;
    }
    fileItem->setCheckState(0, (doCheck) ? Qt::Checked : Qt::Unchecked);
  }
  for (auto dir : dirs) {
    QTreeWidgetItem* dirItem = new QTreeWidgetItem(parentItem, {dir});
    dirItem->setData(0, Role_Path, folderPath + "/" + dir);
    dirItem->setData(0, Role_IsDir, true);
    dirItem->setIcon(0, QIcon(":Resources/folder_colored.svg"));
    dirItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled |
                      Qt::ItemIsAutoTristate);
    if (depth < m_maxFolderDepth)
      createTreeChildren(dirItem, depth + 1, isAllMode, filesToBeCopied,
                         dirsToBeCopied, whiteList, blackList);

    bool doCheck = ((depth == 1 && dirsToBeCopied.contains(dirItem->text(0))) ||
                    isAllMode);
    if (depth == 1) {
      if (whiteList.contains(dirItem->text(0))) doCheck = true;
      if (blackList.contains(dirItem->text(0))) doCheck = false;
    }
    dirItem->setCheckState(0, (doCheck) ? Qt::Checked : Qt::Unchecked);
  }
}

void MyFolderTree::doGetCheckedItems(QTreeWidgetItem* parentItem,
                                     QStringList& files, QStringList& dirs) {
  for (int i = 0; i < parentItem->childCount(); i++) {
    QTreeWidgetItem* child = parentItem->child(i);
    // dir case
    if (child->data(0, Role_IsDir).toBool()) {
      Qt::CheckState state = child->checkState(0);
      // if checked, register the path
      if (state == Qt::Checked)
        dirs.append(child->data(0, Role_Path).toString());
      // else if partially checked, seek children recursively
      else if (state == Qt::PartiallyChecked)
        doGetCheckedItems(child, files, dirs);
      // else if not checked, ignore this item
    }
    // file case
    else {
      // if checked, register the path
      if (child->checkState(0) == Qt::Checked)
        files.append(child->data(0, Role_Path).toString());
    }
  }
}

void MyFolderTree::getCheckedItems(QStringList& files, QStringList& dirs,
                                   QString rootPath) {
  doGetCheckedItems(invisibleRootItem(), files, dirs);
  for (auto& file : files) file.remove(rootPath + "/");
  for (auto& dir : dirs) dir.remove(rootPath + "/");
}

//----------------
// Mode文字列の解析・生成

StepExecutor_CopyFolderContents::Options
StepExecutor_CopyFolderContents::parseMode(const QString& modeString) {
  Options o;
  QString modeStr     = modeString;
  bool selectionFound = false;
  bool takeFound      = false;
  bool conflictFound  = false;
  bool renameFound    = false;

  // スクリプトの名前に使えない文字は、角括弧と、パスの区切り文字
  const QRegularExpression renameScriptRe(
      "^\\[RENAME_SCRIPT=([^\\]/\\\\]+)\\]");
  const QRegularExpression folderDepthRe("^\\[FOLDER_DEPTH=(\\d+)\\]");
  auto consume = [&modeStr](const QString& token) {
    const QString bracketed = "[" + token + "]";
    if (!modeStr.startsWith(bracketed)) return false;
    modeStr = modeStr.mid(bracketed.length());
    return true;
  };

  // 角括弧のオプションを、順序を問わず読み取る。
  // 同じ軸のオプションが複数あった場合は、先に書かれたものを優先する
  while (modeStr.startsWith('[')) {
    if (consume("ALL")) {
      if (!selectionFound) {
        o.selection    = Options::All;
        selectionFound = true;
      }
    } else if (consume("SELECT")) {
      if (!selectionFound) {
        o.selection    = Options::Select;
        selectionFound = true;
      }
    } else if (consume("ALL_WITHOUT_UI")) {
      if (!selectionFound) {
        o.selection    = Options::AllWithoutUI;
        selectionFound = true;
      }
    } else if (consume("MOVE"))
      o.move = true;
    else if (consume("TEAR_DOWN_SUBFOLDERS"))
      o.flattenSubfolders = true;
    else if (consume("RENAME_GENGA_TO_UCHIKOMI")) {
      // 従来の組み込みルールは、同梱のスクリプトに移行した。
      // 従来の指定（既存のプロジェクト）は、そのスクリプトを使う指定として読む
      if (!renameFound) {
        o.renameRule   = Options::Rename_Script;
        o.renameScript = RenameScript::LegacyGengaScriptId;
        renameFound    = true;
      }
    } else if (modeStr.startsWith("[RENAME_SCRIPT=") &&
               renameScriptRe.match(modeStr).hasMatch()) {
      QRegularExpressionMatch match = renameScriptRe.match(modeStr);
      if (!renameFound) {
        o.renameRule   = Options::Rename_Script;
        o.renameScript = match.captured(1);
        renameFound    = true;
      }
      modeStr = modeStr.mid(match.captured(0).length());
    } else if (consume("RENAME_NEXTTAKE")) {
      if (!takeFound) {
        o.takeConversion = Options::Take_Next;
        takeFound        = true;
      }
    } else if (consume("RENAME_T2R")) {
      if (!takeFound) {
        o.takeConversion = Options::Take_ToR;
        takeFound        = true;
      }
    } else if (consume("CONVERT_TO_JPG"))
      o.convertToJpg = true;
    else if (consume("OVERWRITE")) {
      if (!conflictFound) {
        o.conflictPolicy = Options::Conflict_Overwrite;
        conflictFound    = true;
      }
    } else if (consume("SKIP_EXISTING")) {
      if (!conflictFound) {
        o.conflictPolicy = Options::Conflict_Skip;
        conflictFound    = true;
      }
    } else if (consume("OVERWRITE_IF_NEWER")) {
      if (!conflictFound) {
        o.conflictPolicy = Options::Conflict_OverwriteIfNewer;
        conflictFound    = true;
      }
    } else {
      QRegularExpressionMatch match = folderDepthRe.match(modeStr);
      if (!match.hasMatch())
        break;  // 未知のトークン。以降は白黒リストとして扱う
      o.folderDepth = match.captured(1).toInt();
      modeStr       = modeStr.mid(match.captured(0).length());
    }
  }

  // 選択の指定が無い場合は、UIなしで全てコピーする
  if (!selectionFound) o.selection = Options::AllWithoutUI;

  o.whiteBlackList = modeStr;
  return o;
}

QString StepExecutor_CopyFolderContents::buildMode(const Options& o) {
  QString s;
  switch (o.selection) {
  case Options::All:
    s += "[ALL]";
    break;
  case Options::Select:
    s += "[SELECT]";
    break;
  case Options::AllWithoutUI:
    s += "[ALL_WITHOUT_UI]";
    break;
  }
  if (o.move) s += "[MOVE]";
  if (o.flattenSubfolders) s += "[TEAR_DOWN_SUBFOLDERS]";
  if (o.renameRule == Options::Rename_Script)
    s += "[RENAME_SCRIPT=" + o.renameScript + "]";
  if (o.takeConversion == Options::Take_Next)
    s += "[RENAME_NEXTTAKE]";
  else if (o.takeConversion == Options::Take_ToR)
    s += "[RENAME_T2R]";
  if (o.convertToJpg) s += "[CONVERT_TO_JPG]";
  switch (o.conflictPolicy) {
  case Options::Conflict_Overwrite:
    s += "[OVERWRITE]";
    break;
  case Options::Conflict_Skip:
    s += "[SKIP_EXISTING]";
    break;
  case Options::Conflict_OverwriteIfNewer:
    s += "[OVERWRITE_IF_NEWER]";
    break;
  case Options::Conflict_Ask:
    break;
  }
  if (o.folderDepth >= 0)
    s += "[FOLDER_DEPTH=" + QString::number(o.folderDepth) + "]";
  s += o.whiteBlackList;
  return s;
}

//----------------
StepExecutor_CopyFolderContents::StepExecutor_CopyFolderContents(
    FlowGroupId groupId, const ActionStep& step, QString orgPath,
    QString sequence, QString sceneNumber, bool isUndo, bool localDestination)
    : StepExecutor(isUndo)
    , m_skipCueFile(false)
    , m_fileListWidget(nullptr)
    , m_fileTree(nullptr) {
  setPathContext(sequence, sceneNumber);
  m_options = parseMode(step.params[0].toString());

  if (m_options.renameRule == Options::Rename_Script && !isUndo) {
    // スクリプトには、どのカットの処理かを伝える（使わなくてもよい）
    QVariantMap scriptContext;
    scriptContext["sceneNumber"] = sceneNumber;
    scriptContext["sequence"]    = sequence;
    scriptContext["episode"] = WorkflowManager::instance()->currentEpisode();
    m_renamer = new ScriptFileRenamer(m_options.renameScript, scriptContext);
  }

  // テイク接尾辞の変換（現在のテイクが2以上のときだけ有効）
  if (m_options.takeConversion != Options::Take_None) {
    int takeNum = WorkflowManager::instance()->currentTake(sceneNumber);
    if (takeNum > 1) {
      m_takeConversionActive = true;
      if (m_options.takeConversion == Options::Take_Next) {
        // 仕上リテイク受付で使用。テイク前のセルフォルダを現在のテイク接尾辞に変える
        m_takeSuffixPair.first  = takeNum2Suffix(takeNum - 1);
        m_takeSuffixPair.second = takeNum2Suffix(takeNum);
      } else {
        // 仕上リテイク上がりで使用。フォルダ名のテイク接尾辞を撮影用に変換
        m_takeSuffixPair.first  = takeNum2Suffix(takeNum);
        m_takeSuffixPair.second = takeNum2RSuffix(takeNum);
      }
    }
  }

  // ファイル選択の既定値の指定
  QStringList conditionList =
      m_options.whiteBlackList.split(';', Qt::SkipEmptyParts);
  for (auto condition : conditionList) {
    if (condition.startsWith('+'))
      m_whiteList.append(condition.right(condition.size() - 1));
    else if (condition.startsWith('-'))
      m_blackList.append(condition.right(condition.size() - 1));
  }

  m_orgFolderPath = step.params[1].toString();
  m_orgFileName   = QFileInfo(orgPath).fileName();
  if (m_orgFolderPath.isEmpty()) {
    m_orgFolderPath = QFileInfo(orgPath).absoluteDir().path();
  } else if (m_orgFolderPath == "[SKIP_CUE_FILE]") {
    m_orgFolderPath = QFileInfo(orgPath).absoluteDir().path();
    m_skipCueFile   = true;
  } else {
    m_orgFolderPath = WorkflowManager::instance()->decodePath(
        m_orgFolderPath, sequence, sceneNumber);
  }

  for (int i = 2; i < step.params.size(); i++) {
    QString copyTo = localDestination ? step.params[i].toString()
                                      : WorkflowManager::instance()->decodePath(
                                            step.params[i].toString(), sequence,
                                            sceneNumber);
    if (copyTo.contains("[CURRENT_DATE]"))
      copyTo.replace("[CURRENT_DATE]", QDate::currentDate().toString("yyMMdd"));
    m_copyToFolderList.append(copyTo);
  }

  if (!m_isUndo) {
    m_filesToBeCopied.append(m_orgFileName);

    QStringList xdtsFileNames;
    QStringList xdtsPaths = WorkflowManager::instance()->xdtsPaths(
        groupId, sequence, sceneNumber, xdtsFileNames, true, {orgPath},
        &m_ambiguousXdts);
    int i = 0;
    for (auto xdtsPath : xdtsPaths) {
      QString xdtsFileName = xdtsFileNames[i];
      m_filesToBeCopied.append(xdtsFileName);
      // _genga_ts, _douga_ts を含む場合は取り除く
      QString xdtsImgFolderName(xdtsFileName);
      xdtsImgFolderName =
          xdtsImgFolderName.remove(GengaSuffix, Qt::CaseInsensitive);
      xdtsImgFolderName =
          xdtsImgFolderName.remove(DougaSuffix, Qt::CaseInsensitive);
      xdtsImgFolderName.chop(5);
      xdtsImgFolderName = xdtsImgFolderName + QString("_xdts");
      m_dirsToBeCopied.append(xdtsImgFolderName);
      i++;
    }
  }
}

StepExecutor_CopyFolderContents::~StepExecutor_CopyFolderContents() {
  if (m_renamer) delete m_renamer;
}

QWidget* StepExecutor_CopyFolderContents::createWidget() {
  if (m_isUndo)
    return createWidget_undo();
  else
    return createWidget_do();
}

QWidget* StepExecutor_CopyFolderContents::createWidget_do() {
  QString shortenOrgFolderPath(hide(m_orgFolderPath));
  shortenOrgFolderPath.remove(WorkflowManager::instance()->projectRootPath(),
                              Qt::CaseInsensitive);

  QWidget* widget = new QWidget();

  // 選択ツリーが、フォルダの中身を一覧するために、実際のパスが必要。
  // 難読化マクロは、ここで初めてデコードする（画面には、デコード前のパスしか表示しない）
  QString orgFolderPath = realPath(m_orgFolderPath);

  const bool hasUI = (m_options.selection != Options::AllWithoutUI);
  if (hasUI) {
    int folderDepth = (m_options.folderDepth < 0) ? 1 : m_options.folderDepth;
    m_fileTree      = new MyFolderTree(
        widget, orgFolderPath, folderDepth, m_options.selection == Options::All,
        m_filesToBeCopied, m_dirsToBeCopied, m_whiteList, m_blackList);
  }

  QString behaviorStr = (m_options.move) ? tr("moved") : tr("copied");

  QHBoxLayout* lay = new QHBoxLayout();
  lay->setContentsMargins(10, 10, 10, 10);
  lay->setSpacing(10);
  QString iconRenamerError;
  bool showError = m_options.hasConflictingOptions() ||
                   (m_renamer && !m_renamer->isValid(iconRenamerError));
  lay->addWidget(new IconWidget(Step_CopyFolderContents, m_isUndo, showError),
                 0);

  QVBoxLayout* rightLayout = new QVBoxLayout();
  rightLayout->setContentsMargins(5, 5, 5, 5);
  rightLayout->setSpacing(5);
  {
    rightLayout->addWidget(
        new QLabel(tr("Source: %1").arg(shortenOrgFolderPath), widget), 0,
        Qt::AlignLeft);

    if (hasUI) {
      rightLayout->addWidget(
          new QLabel(
              tr("Please select files / folders to be %1.").arg(behaviorStr),
              widget),
          0, Qt::AlignLeft);
      rightLayout->addWidget(m_fileTree, 1);
      rightLayout->addWidget(
          new QLabel(tr("The above files / folders will be %1 to the following "
                        "locations:\n%2")
                         .arg(behaviorStr)
                         .arg(" - " + m_copyToFolderList.join("\n - ")),
                     widget),
          0, Qt::AlignLeft);
    } else {  // m_options.selection == AllWithoutUI
      rightLayout->addWidget(
          new QLabel(tr("All files / folders will be %1 to the following "
                        "locations:\n%2")
                         .arg(behaviorStr)
                         .arg(" - " + m_copyToFolderList.join("\n - ")),
                     widget),
          0, Qt::AlignLeft);
    }

    if (m_options.hasConflictingOptions())
      rightLayout->addWidget(
          new QLabel(tr("[ERROR] [MOVE] cannot be combined with "
                        "[CONVERT_TO_JPG]. This step will fail."),
                     widget),
          0, Qt::AlignLeft);
    QString renamerError;
    if (m_renamer && !m_renamer->isValid(renamerError)) {
      QLabel* errorLabel = new QLabel(tr("[ERROR] The rename rule is not "
                                         "available. This step will fail.\n%1")
                                          .arg(renamerError),
                                      widget);
      errorLabel->setWordWrap(true);
      rightLayout->addWidget(errorLabel, 0, Qt::AlignLeft);
    }
    if (m_skipCueFile)
      rightLayout->addWidget(
          new QLabel(tr("N.B. The cue file %1 will not be %2.")
                         .arg(m_orgFileName)
                         .arg(behaviorStr),
                     widget),
          0, Qt::AlignLeft);
    if (m_renamer) {
      rightLayout->addWidget(
          new QLabel(tr("N.B. Files are renamed and copied according to the "
                        "following rules:\n"
                        "     (For details of the renaming rules, hover over "
                        "the rule name to see a tooltip.)"),
                     widget),
          0, Qt::AlignLeft);
      QLabel* ruleLabel =
          new QLabel(QString("-  %1").arg(m_renamer->ruleName()), widget);
      ruleLabel->setStyleSheet(
          "border-bottom-width: 1px; border-bottom-style: solid; "
          "border-radius: 0px;");
      ruleLabel->setToolTip(m_renamer->ruleDetails());
      ruleLabel->setIndent(10);
      rightLayout->addWidget(ruleLabel, 0, Qt::AlignLeft);
    }
    // 「フォルダ名に含まれるテイク接尾辞は、現在のテイク番号に置き換えられます」
    if (m_takeConversionActive &&
        m_options.takeConversion == Options::Take_Next)
      rightLayout->addWidget(
          new QLabel(tr("N.B. Take suffixes in folder names will be replaced "
                        "with the ones with the current take number."),
                     widget),
          0, Qt::AlignLeft);
    // 「フォルダ名に含まれるテイク接尾辞は、"r"から始まる撮影用の接尾辞に置き換えられます」
    else if (m_takeConversionActive &&
             m_options.takeConversion == Options::Take_ToR)
      rightLayout->addWidget(
          new QLabel(tr("N.B. Take suffixes in folder names will be replaced "
                        "with suffixes for Satsuei that begin with \"r\"."),
                     widget),
          0, Qt::AlignLeft);
    // 同名のファイルがコピー先にある場合の処理（既定の「確認する」以外のとき）
    if (m_options.conflictPolicy == Options::Conflict_Overwrite)
      rightLayout->addWidget(
          new QLabel(
              tr("N.B. Existing files with the same name will be overwritten "
                 "without confirmation.\n"
                 "     (Overwritten files will not be restored on undo.)"),
              widget),
          0, Qt::AlignLeft);
    else if (m_options.conflictPolicy == Options::Conflict_Skip)
      rightLayout->addWidget(
          new QLabel(tr("N.B. Existing files with the same name will be kept, "
                        "and the corresponding files will not be %1.")
                         .arg(behaviorStr),
                     widget),
          0, Qt::AlignLeft);
    else if (m_options.conflictPolicy == Options::Conflict_OverwriteIfNewer)
      rightLayout->addWidget(
          new QLabel(tr("N.B. Existing files with the same name will be "
                        "overwritten only when the source file is newer.\n"
                        "     Otherwise, the existing files will be kept and "
                        "the corresponding files will not be %1.")
                         .arg(behaviorStr),
                     widget),
          0, Qt::AlignLeft);
    if (m_options.flattenSubfolders)
      rightLayout->addWidget(
          new QLabel(
              tr("N.B. All subfolders will be torn down and all files will be "
                 "placed directly under the destination folder."),
              widget),
          0, Qt::AlignLeft);
    if (m_options.convertToJpg) {
      rightLayout->addWidget(
          new QLabel(tr("N.B. All image files will be converted to the "
                        "following format : %1")
                         .arg("jpg"),
                     widget),
          0, Qt::AlignLeft);
    }
  }
  lay->addLayout(rightLayout, 1);
  widget->setLayout(lay);

  return widget;
}

QWidget* StepExecutor_CopyFolderContents::createWidget_undo() {
  QWidget* widget = new QWidget();

  // checkSum: { 上書きしたか, コピー先ごとの{ファイル, 作成したフォルダ},
  // 移動の対応表, 削除した元フォルダ }
  QVariantList undoInfo = m_checkSum.toList();
  bool showWarning      = (undoInfo.size() >= 4) && undoInfo[0].toBool();
  // 削除されるファイル／フォルダ（コピー先が複数ある場合は、全てのコピー先に共通のものを一度だけ表示する）
  QStringList files, dirs;
  if (undoInfo.size() >= 4) {
    for (const QVariant& destInfo : undoInfo[1].toList()) {
      QVariantList destLists = destInfo.toList();
      if (destLists.size() < 2) continue;
      for (const QString& file : destLists[0].toStringList())
        if (!files.contains(file)) files.append(file);
      for (const QString& dir : destLists[1].toStringList()) {
        // コピー先フォルダ自体（"."）や、その新規作成した親フォルダ（"../"で始まる）は一覧に出さない
        if (dir == "." || dir.startsWith("../")) continue;
        if (!dirs.contains(dir)) dirs.append(dir);
      }
    }
  }

  m_fileListWidget = new QListWidget(widget);
  m_fileListWidget->addItems(files);
  for (auto dirName : dirs) {
    m_fileListWidget->addItem(dirName);
    m_fileListWidget->item(m_fileListWidget->count() - 1)
        ->setIcon(QIcon(":Resources/folder_colored.svg"));
  }

  QHBoxLayout* lay = new QHBoxLayout();
  lay->setContentsMargins(10, 10, 10, 10);
  lay->setSpacing(10);
  lay->addWidget(new IconWidget(Step_CopyFolderContents, m_isUndo, showWarning),
                 0);

  QVBoxLayout* rightLayout = new QVBoxLayout();
  rightLayout->setContentsMargins(5, 5, 5, 5);
  rightLayout->setSpacing(5);
  {
    rightLayout->addWidget(m_fileListWidget, 1);
    rightLayout->addWidget(
        new QLabel(tr("The above files / folders will be removed from the "
                      "following locations:\n%1")
                       .arg(" - " + m_copyToFolderList.join("\n - ")),
                   widget),
        0, Qt::AlignLeft);
    if (m_options.move)
      rightLayout->addWidget(
          new QLabel(tr("And the above files / folders will be reverted to %1")
                         .arg(hide(m_orgFolderPath)),
                     widget),
          0, Qt::AlignLeft);
    if (showWarning)
      rightLayout->addWidget(
          new QLabel(tr("[WARNING] Some existing files were overwritten by the "
                        "copy.\nThe undo operation cannot restore them."),
                     widget),
          0, Qt::AlignLeft);
  }
  lay->addLayout(rightLayout, 1);
  widget->setLayout(lay);

  return widget;
}

bool StepExecutor_CopyFolderContents::execute(QString& log,
                                              QVariant& checkSum) {
  if (m_ambiguousXdts) {
    log += tr("Multiple file names have the same Unicode spelling.\n");
    return false;
  }
  if (m_isUndo)
    return execute_undo(log, checkSum);
  else
    return execute_do(log, checkSum);
}

QVariant StepExecutor_CopyFolderContents::makeCheckSum(
    bool overwritten, const QVariantList& perDestination,
    const QVariantList& moveMapping, const QStringList& removedSourceDirs) {
  return QVariantList{overwritten, perDestination, moveMapping,
                      removedSourceDirs};
}

bool StepExecutor_CopyFolderContents::buildPlan(const QString& orgFolderPath,
                                                QList<PlanItem>& plan,
                                                QString& log) {
  QDir srcDir(orgFolderPath);

  // ①
  // 選択された項目を、ファイル単位に展開する（空フォルダも作れるよう、フォルダ項目も含める）
  for (const QString& file : m_filesToBeCopied) {
    if (SharedFiles::isMetadata(QFileInfo(srcDir, file))) continue;
    PlanItem item;
    item.fromRel = item.toRel = file;
    plan.append(item);
  }
  for (const QString& dir : m_dirsToBeCopied) {
    PlanItem dirItem;
    dirItem.fromRel = dirItem.toRel = dir;
    dirItem.isDir                   = true;
    dirItem.originDir               = dir;
    plan.append(dirItem);

    // 隠しファイルは対象外
    QDirIterator it(srcDir.absoluteFilePath(dir),
                    QDir::AllEntries | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
      it.next();
      if (SharedFiles::isMetadata(it.fileInfo())) continue;
      PlanItem item;
      item.fromRel = item.toRel =
          srcDir.relativeFilePath(it.fileInfo().absoluteFilePath());
      item.isDir     = it.fileInfo().isDir();
      item.originDir = dir;
      plan.append(item);
    }
  }

  // ② リネームルール（ファイル名の変換）
  if (m_renamer) {
    QList<int> nonConformingIndices;
    m_renamer->beginBatch();
    for (int i = 0; i < plan.size(); i++) {
      if (plan[i].isDir) continue;
      QString renamed, renameError;
      if (!m_renamer->renamedRelativePath(plan[i].fromRel, renamed,
                                          renameError)) {
        m_renamer->endBatch();
        log.append(tr("Failed: The rename rule returned an error for %1.\n%2\n")
                       .arg(plan[i].fromRel)
                       .arg(renameError));
        return false;
      }
      if (renamed.isEmpty())
        nonConformingIndices.append(i);
      else
        plan[i].toRel = renamed;
    }
    m_renamer->endBatch();

    // リネームルールを使う場合、コピー先のフォルダ構成はリネーム後のファイルのパスで決まる。
    // 元のフォルダと同じ名前の（空の）フォルダが、コピー先に残らないよう、フォルダ項目のコピー先は持たない
    // （移動時の元フォルダの後片付けには使う）。ファイルの親フォルダは、コピー時に必要に応じて作られる
    for (PlanItem& item : plan)
      if (item.isDir) item.toRel.clear();

    if (!nonConformingIndices.isEmpty()) {
      QString fileListStr;
      for (int n = 0; n < 5 && n < nonConformingIndices.size(); n++)
        // 確認ダイアログには、デコード前（難読化マクロのまま）のコピー元のパスを表示する
        fileListStr += " - " +
                       QDir::cleanPath(m_orgFolderPath + "/" +
                                       plan[nonConformingIndices[n]].fromRel) +
                       "\n";
      if (nonConformingIndices.size() > 5)
        fileListStr +=
            tr(" and %1 more files.").arg(nonConformingIndices.size() - 5);

      QMessageBox msgBox(
          QMessageBox::Question, tr("Question"),
          tr("The following files do not meet the criteria to be "
             "renamed.\n\n%1\n\n"
             "Do you want to copy these files without renaming or skip them?")
              .arg(fileListStr));
      QPushButton* copyButton =
          msgBox.addButton(tr("Copy"), QMessageBox::AcceptRole);
      QPushButton* skipButton =
          msgBox.addButton(tr("Skip"), QMessageBox::NoRole);
      QPushButton* cancelButton = msgBox.addButton(QMessageBox::Cancel);
      msgBox.exec();
      if (msgBox.clickedButton() == cancelButton) {
        log.append(tr("Canceled.\n"));
        return false;
      }
      // 「Copy」の場合は、リネームせずそのままコピーする（コピー先＝コピー元のまま）
      if (msgBox.clickedButton() == skipButton) {
        for (int n = nonConformingIndices.size() - 1; n >= 0; n--)
          plan.removeAt(nonConformingIndices[n]);
      }
    }
  }

  // ③ フォルダ名のテイク接尾辞の変換（選択されたフォルダに含まれる項目のみ）
  if (m_takeConversionActive) {
    for (PlanItem& item : plan) {
      if (item.originDir.isEmpty()) continue;
      QString dirPart, namePart;
      if (item.isDir)
        dirPart = item.toRel;
      else {
        int slash = item.toRel.lastIndexOf('/');
        dirPart   = (slash < 0) ? QString() : item.toRel.left(slash);
        namePart  = item.toRel.mid(slash + 1);
      }
      if (m_takeSuffixPair.first.isEmpty()) {  // Take1からTake2にするとき
        if (dirPart.startsWith(item.originDir))
          dirPart = item.originDir + m_takeSuffixPair.second +
                    dirPart.mid(item.originDir.length());
      } else  // Take3以降
        dirPart.replace(m_takeSuffixPair.first, m_takeSuffixPair.second);
      item.toRel = item.isDir ? dirPart
                              : (dirPart.isEmpty() ? namePart
                                                   : dirPart + "/" + namePart);
    }
  }

  // ④
  // サブフォルダの展開（選択されたフォルダに含まれるファイルを、コピー先の直下に並べる）。
  //    フォルダ項目はコピー先を持たず（toRelが空）、移動時の元フォルダの後片付けにだけ使う
  if (m_options.flattenSubfolders) {
    for (PlanItem& item : plan) {
      if (item.originDir.isEmpty()) continue;
      if (item.isDir)
        item.toRel.clear();
      else
        item.toRel = item.toRel.mid(item.toRel.lastIndexOf('/') + 1);
    }
  }

  // ⑤ 画像のJPEG変換
  if (m_options.convertToJpg) {
    for (PlanItem& item : plan) {
      if (item.isDir) continue;
      QString toPath = item.toRel;
      if (hasImageExtension(toPath) && replaceExtension(toPath, "jpg")) {
        item.toRel        = toPath;
        item.convertImage = true;
      }
    }
  }
  return true;
}

bool StepExecutor_CopyFolderContents::execute_do(QString& log,
                                                 QVariant& checkSum) {
  const bool isMove = m_options.move;
  if (!isMove)
    log.append(
        tr("[COPY FOLDER CONTENTS]\nFrom: %1\n").arg(hide(m_orgFolderPath)));
  else
    log.append(
        tr("[MOVE FOLDER CONTENTS]\nFrom: %1\n").arg(hide(m_orgFolderPath)));

  if (m_options.hasConflictingOptions()) {
    log.append(
        tr("Failed: [MOVE] cannot be combined with [CONVERT_TO_JPG], because "
           "the original images could not be restored.\n"));
    return false;
  }
  QString renamerError;
  if (m_renamer && !m_renamer->isValid(renamerError)) {
    log.append(tr("Failed: The rename rule is not available.\n%1\n")
                   .arg(renamerError));
    return false;
  }

  // コピー元の難読化マクロは、操作に使うここで、実際のパスにデコードする
  // （画面・ログには、マクロのままのm_orgFolderPathを使う）
  QString orgFolderPath = realPath(m_orgFolderPath);
  if (orgFolderPath.isEmpty()) {
    log += tr("Ambiguous or empty file path.\n");
    return false;
  }

  m_filesToBeCopied.clear();
  m_dirsToBeCopied.clear();
  if (m_options.selection ==
      Options::AllWithoutUI) {  // m_fileListWidget表示せず、とにかく全部コピー
    m_filesToBeCopied =
        QDir(orgFolderPath)
            .entryList(QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
                       QDir::Name);
    m_dirsToBeCopied =
        QDir(orgFolderPath)
            .entryList(QDir::Dirs | QDir::Readable | QDir::NoDotAndDotDot,
                       QDir::Name);
  } else {
    m_fileTree->getCheckedItems(m_filesToBeCopied, m_dirsToBeCopied,
                                orgFolderPath);
  }

  if (m_filesToBeCopied.isEmpty() && m_dirsToBeCopied.isEmpty()) {
    log.append(tr("No file / dir is copied.\n"));
    checkSum =
        makeCheckSum(false, QVariantList(), QVariantList(), QStringList());
    return true;
  }

  // コピー（移動）の計画を作る
  QList<PlanItem> plan;
  if (!buildPlan(orgFolderPath, plan, log)) return false;

  // コピー先
  struct Destination {
    QString
        display;  // 確認ダイアログとログに表示する、デコード前（難読化マクロのまま）のパス
    QString path;  // 操作に使う、デコード後の実際のパス
  };
  QList<Destination> destinations;
  for (const QString& copyTo : m_copyToFolderList) {
    Destination d;
    d.display = copyTo;
    d.path    = realPath(copyTo);
    if (d.path.isEmpty() || SharedFiles::sameEntry(orgFolderPath, d.path)) {
      log += tr("The destination is ambiguous or inside the source folder.\n");
      return false;
    }
    for (const QString& selected : m_dirsToBeCopied) {
      if (SharedFiles::insideSource(QDir(orgFolderPath).filePath(selected),
                                    d.path)) {
        log +=
            tr("The destination is ambiguous or inside the source folder.\n");
        return false;
      }
    }
    destinations.append(d);
  }
  // [SKIP_CUE_FILE]のときは、キューファイルをコピーしない
  auto isSkipped = [this](const PlanItem& item, const Destination&) {
    return m_skipCueFile && !item.isDir && item.originDir.isEmpty() &&
           item.fromRel.normalized(QString::NormalizationForm_C) ==
               m_orgFileName.normalized(QString::NormalizationForm_C);
  };

  // コピー先のパスが重複するファイルの確認（サブフォルダの展開などで同名になる場合）
  {
    QHash<QString, int> destCounts;
    for (const PlanItem& item : plan)
      if (!item.isDir)
        destCounts[item.toRel.normalized(QString::NormalizationForm_C)]++;
    int duplicatedCount = 0;
    for (auto count : destCounts)
      if (count > 1) duplicatedCount += count - 1;
    if (duplicatedCount > 0) {
      QMessageBox::StandardButton ret = QMessageBox::question(
          nullptr, tr("Question"),
          tr("%1 file(s) have the same destination path as another file.\n"
             "The later ones will overwrite the earlier ones.\nDo you want to "
             "continue?")
              .arg(duplicatedCount),
          {QMessageBox::Yes | QMessageBox::Cancel}, QMessageBox::Cancel);
      if (ret == QMessageBox::Cancel) {
        log.append(tr("Canceled.\n"));
        return false;
      }
    }
  }

  QDir fromDir(orgFolderPath);

  // 同名のファイルがコピー先にある場合の処理（上書き／スキップ）を、ファイルごとに決める。
  // スキップするものは、「コピー先の番号|コピー先の相対パス」を記録しておく
  QSet<QString> conflictSkipKeys;
  QSet<QString>
      conflictSkippedFromRel;  // 1つでもスキップされたコピー元（移動時も、元ファイルを残す）
  {
    struct Conflict {
      int destIndex;
      const PlanItem* item;
      QString display;
    };
    QList<Conflict> conflicts;
    for (int di = 0; di < destinations.size(); di++) {
      const Destination& d = destinations[di];
      for (const PlanItem& item : plan) {
        if (isSkipped(item, d) || item.toRel.isEmpty()) continue;
        const QString dest = SharedFiles::actualPath(d.path + "/" + item.toRel);
        const QString src =
            SharedFiles::actualPath(fromDir.absoluteFilePath(item.fromRel));
        if (dest.isEmpty() || src.isEmpty() ||
            SharedFiles::sameEntry(src, dest)) {
          log += tr("Ambiguous source or destination.\n");
          return false;
        }
        if (!item.isDir && QFileInfo::exists(dest))
          conflicts.append({di, &item, d.display + "/" + item.toRel});
      }
    }

    Options::ConflictPolicy policy = m_options.conflictPolicy;
    if (!conflicts.isEmpty() && policy == Options::Conflict_Ask) {
      QString fileListStr;
      for (int n = 0; n < 10 && n < conflicts.size(); n++)
        fileListStr += " - " + conflicts[n].display + "\n";
      if (conflicts.size() > 10)
        fileListStr += tr(" and %1 more files.").arg(conflicts.size() - 10);
      QMessageBox msgBox(
          QMessageBox::Question, tr("Question"),
          tr("The following files already exist:\n%1\nDo you want to overwrite "
             "them, "
             "or skip them and keep the existing files?"
             "\nN.B. Overwritten files will not be restored on undo.")
              .arg(fileListStr));
      QPushButton* overwriteButton =
          msgBox.addButton(tr("Overwrite"), QMessageBox::AcceptRole);
      QPushButton* skipButton =
          msgBox.addButton(tr("Skip existing files"), QMessageBox::NoRole);
      QPushButton* cancelButton = msgBox.addButton(QMessageBox::Cancel);
      msgBox.setDefaultButton(cancelButton);
      msgBox.exec();
      if (msgBox.clickedButton() == overwriteButton)
        policy = Options::Conflict_Overwrite;
      else if (msgBox.clickedButton() == skipButton)
        policy = Options::Conflict_Skip;
      else {
        log.append(tr("Canceled.\n"));
        return false;
      }
    }

    for (const Conflict& c : conflicts) {
      bool skip = false;
      switch (policy) {
      case Options::Conflict_Skip:
        skip = true;
        break;
      case Options::Conflict_OverwriteIfNewer:
        // コピー元の方が新しい場合だけ上書きする
        skip = !(QFileInfo(fromDir.absoluteFilePath(c.item->fromRel))
                     .lastModified() >
                 QFileInfo(QDir::cleanPath(destinations[c.destIndex].path +
                                           "/" + c.item->toRel))
                     .lastModified());
        break;
      default:
        break;
      }
      if (skip) {
        conflictSkipKeys.insert(QString::number(c.destIndex) + "|" +
                                c.item->toRel);
        conflictSkippedFromRel.insert(c.item->fromRel);
      }
    }
  }

  bool isOverwritten = false;
  QSet<QString>
      copiedFromRel;  // 1つ以上のコピー先へコピーできた項目（移動時に元ファイルを消す対象）
  QVariantList perDestination;
  for (int destIndex = 0; destIndex < destinations.size(); destIndex++) {
    const Destination& d = destinations[destIndex];
    int skippedCount     = 0;
    log.append(tr("Start copying to %1.\n").arg(d.display));
    QStringList copiedFiles, createdDirs;
    // フォルダが無ければ作り、新規に作ったフォルダをUndo用に記録する
    // （コピー先フォルダ自体や、その親フォルダを新規に作った場合も含む）
    auto ensureDir = [&](const QString& absDir) {
      QStringList missing;
      for (QString p = QDir::cleanPath(absDir); !QFileInfo::exists(p);) {
        missing.prepend(p);
        QString parent = QDir::cleanPath(QFileInfo(p).dir().absolutePath());
        if (parent == p) break;
        p = parent;
      }
      if (missing.isEmpty()) return true;
      if (!QDir().mkpath(absDir)) return false;
      for (const QString& m : missing)
        createdDirs.append(QDir(d.path).relativeFilePath(m));
      return true;
    };

    log.append(tr("Making directory path.."));
    if (ensureDir(d.path))
      log.append(tr("Done.\n"));
    else {
      log.append(tr("Failed.\n"));
      return false;
    }

    for (const PlanItem& item : plan) {
      if (item.isDir) {
        if (item.toRel.isEmpty())
          continue;  // コピー先を持たない（展開時の元フォルダの後片付け用）
        const QString targetDir =
            SharedFiles::actualPath(QDir::cleanPath(d.path + "/" + item.toRel));
        if (targetDir.isEmpty() || !ensureDir(targetDir)) {
          log.append(tr("  Failed: %1\n").arg(item.toRel));
          return false;
        }
        continue;
      }
      if (isSkipped(item, d)) continue;
      // 同名のファイルがあり、スキップすると決めたもの
      if (conflictSkipKeys.contains(QString::number(destIndex) + "|" +
                                    item.toRel)) {
        skippedCount++;
        continue;
      }

      QString fromPath = fromDir.absoluteFilePath(item.fromRel);
      QString toPath =
          SharedFiles::actualPath(QDir::cleanPath(d.path + "/" + item.toRel));
      if (toPath.isEmpty() || SharedFiles::sameEntry(fromPath, toPath)) {
        log += tr("Ambiguous source or destination.\n");
        return false;
      }
      if (!ensureDir(QFileInfo(toPath).absolutePath())) {
        log.append(tr("  Failed: %1\n").arg(item.toRel));
        return false;
      }
      if (QFileInfo::exists(toPath)) {
        if (!QFile::remove(toPath)) {
          log.append(tr("   Remove existing file..."));
          log.append(tr("Failed.\n"));
          return false;
        }
        isOverwritten = true;
      }

      bool ok = (item.convertImage) ? QImage(fromPath).save(toPath)
                                    : QFile::copy(fromPath, toPath);
      if (!ok) {
        log.append(tr("  Failed: %1\n").arg(item.fromRel));
        return false;
      }
      copiedFiles.append(item.toRel);
      copiedFromRel.insert(item.fromRel);
    }
    if (skippedCount == 0)
      log.append(tr("  Done. (%1 file(s) copied)\n").arg(copiedFiles.size()));
    else
      log.append(
          tr("  Done. (%1 file(s) copied, %2 existing file(s) skipped)\n")
              .arg(copiedFiles.size())
              .arg(skippedCount));
    perDestination.append(QVariant(QVariantList{copiedFiles, createdDirs}));
  }

  // 移動の場合、元データを消す
  QVariantList moveMapping;
  QStringList removedSourceDirs;
  if (isMove) {
    log.append(tr("Start removing original files from %1.\n")
                   .arg(hide(m_orgFolderPath)));
    // Undoで元に戻せるよう、元のパスとコピー先のパスの対応表を記録する
    for (const PlanItem& item : plan) {
      // コピーされなかった（同名ファイルのスキップ等）ものは、元の場所に残す
      if (item.isDir || !copiedFromRel.contains(item.fromRel) ||
          conflictSkippedFromRel.contains(item.fromRel))
        continue;
      moveMapping.append(QVariant(QStringList{item.fromRel, item.toRel}));
      log.append(tr("   Removing %1...").arg(item.fromRel));
      if (QFile::remove(fromDir.absoluteFilePath(item.fromRel)))
        log.append(tr("Done.\n"));
      else {
        log.append(tr("Failed.\n"));
        return false;
      }
    }
    // 選択されたフォルダは、中身を移し終えて見えるファイルが残っていなければ削除する
    // （隠しファイルは元々コピーの対象外のため、それしか残っていない場合も削除する）。
    // 選択をスキップしたファイル等が残っているフォルダは削除しない
    QList<PlanItem> dirItems;
    for (const PlanItem& item : plan)
      if (item.isDir) dirItems.append(item);
    std::sort(dirItems.begin(), dirItems.end(),
              [](const PlanItem& a, const PlanItem& b) {
                return a.fromRel.length() > b.fromRel.length();
              });
    for (const PlanItem& item : dirItems) {
      QString dirPath = fromDir.absoluteFilePath(item.fromRel);
      if (!QFileInfo::exists(dirPath)) continue;
      auto cleanup = SharedFiles::removeEmptyFolder(dirPath);
      if (cleanup == SharedFiles::Cleanup::Kept) continue;
      log.append(tr("   Removing a folder %1...").arg(item.fromRel));
      if (cleanup == SharedFiles::Cleanup::Removed) {
        log.append(tr("Done.\n"));
        removedSourceDirs.append(item.fromRel);
      } else {
        log.append(tr("Failed.\n"));
        return false;
      }
    }
  }

  // Undoのための記録を格納
  // isOverwrittenは何かファイルを上書きコピーしたときのフラグ。Undo時に警告表示を出す
  checkSum = makeCheckSum(isOverwritten, perDestination, moveMapping,
                          removedSourceDirs);

  return true;
}

bool StepExecutor_CopyFolderContents::execute_undo(QString& log,
                                                   QVariant& checkSum) {
  if (!m_options.move)
    log.append(tr("[UNDO_COPY FOLDER CONTENTS]\n"));
  else
    log.append(tr("[UNDO_MOVE FOLDER CONTENTS]\n"));

  QVariantList undoInfo = m_checkSum.toList();
  if (undoInfo.size() < 4) {
    log.append(tr("No file / dir is copied.\n"));
    return true;
  }
  QVariantList perDestination   = undoInfo[1].toList();
  QVariantList moveMapping      = undoInfo[2].toList();
  QStringList removedSourceDirs = undoInfo[3].toStringList();

  // コピー先。難読化マクロは、操作に使うここで、実際のパスにデコードする
  // （ログには、マクロのままのm_copyToFolderListを使う）
  QStringList destPaths;
  for (const QString& copyTo : m_copyToFolderList) {
    const QString path = realPath(copyTo);
    if (path.isEmpty()) {
      log += tr("Ambiguous or empty file path.\n");
      return false;
    }
    destPaths.append(path);
  }

  // ファイルの復帰
  if (m_options.move) {
    QString orgFolderPath = realPath(m_orgFolderPath);
    log.append(tr("Start reverting files to %1.\n").arg(hide(m_orgFolderPath)));
    if (orgFolderPath.isEmpty()) {
      log += tr("Ambiguous or empty file path.\n");
      return false;
    }
    QDir toDir(orgFolderPath);

    // 移動時に削除した元のフォルダを作り直す
    for (const QString& dirRel : removedSourceDirs)
      QDir().mkpath(toDir.absoluteFilePath(dirRel));

    for (const QVariant& mapping : moveMapping) {
      QStringList fromTo = mapping.toStringList();
      if (fromTo.size() < 2) continue;
      // この項目をコピーしたコピー先を探す
      QString copiedPath;
      for (int i = 0; i < perDestination.size() && i < destPaths.size(); i++) {
        QVariantList destLists = perDestination[i].toList();
        if (!destLists.isEmpty() &&
            destLists[0].toStringList().contains(fromTo[1])) {
          copiedPath = QDir::cleanPath(destPaths[i] + "/" + fromTo[1]);
          break;
        }
      }
      log.append(tr("   Copying the file  %1...").arg(fromTo[0]));
      QString revertTo = toDir.absoluteFilePath(fromTo[0]);
      copiedPath       = SharedFiles::actualPath(copiedPath);
      revertTo         = SharedFiles::actualPath(revertTo);
      bool ok          = !revertTo.isEmpty() && !copiedPath.isEmpty() &&
                QDir().mkpath(QFileInfo(revertTo).absolutePath()) &&
                !QFileInfo::exists(revertTo) &&
                QFile::copy(copiedPath, revertTo);
      if (ok)
        log.append(tr("Done.\n"));
      else {
        log.append(tr("Failed.\n"));
        return false;
      }
    }
  }

  // コピーで作られたファイル・フォルダだけを削除する（コピー先に元からあったものは消さない）
  for (int i = 0; i < perDestination.size() && i < destPaths.size(); i++) {
    QVariantList destLists = perDestination[i].toList();
    if (destLists.size() < 2) continue;
    const QString& copyTo = destPaths[i];
    log.append(
        tr("Start removing files from %1.\n").arg(m_copyToFolderList.value(i)));

    for (const QString& fileName : destLists[0].toStringList()) {
      QString removePath =
          SharedFiles::actualPath(QDir::cleanPath(copyTo + "/" + fileName));
      if (removePath.isEmpty()) {
        log += tr("Ambiguous or empty file path.\n");
        return false;
      }
      if (!QFileInfo::exists(removePath)) continue;  // 既に無いものは何もしない
      log.append(tr("  Remove the file %1...").arg(fileName));
      if (QFile::remove(removePath))
        log.append(tr("Done.\n"));
      else {
        log.append(tr("Failed.\n"));
        return false;
      }
    }

    // 新規に作ったフォルダは、深い順に、空になっているものだけ削除する
    // （コピー先フォルダ自体は"."、新規に作った親フォルダは"../"で始まる相対パスで記録されている）
    QStringList dirs = destLists[1].toStringList();
    std::sort(dirs.begin(), dirs.end(),
              [&copyTo](const QString& a, const QString& b) {
                return QDir::cleanPath(copyTo + "/" + a).length() >
                       QDir::cleanPath(copyTo + "/" + b).length();
              });
    for (const QString& dirName : dirs) {
      QString removeDirPath =
          SharedFiles::actualPath(QDir::cleanPath(copyTo + "/" + dirName));
      if (removeDirPath.isEmpty()) {
        log += tr("Ambiguous or empty file path.\n");
        return false;
      }
      if (!QFileInfo::exists(removeDirPath)) continue;
      auto cleanup = SharedFiles::removeEmptyFolder(removeDirPath);
      if (cleanup == SharedFiles::Cleanup::Kept) {
        log.append(
            tr("  Kept the folder %1 because it is not empty.\n").arg(dirName));
        continue;
      }
      log.append(tr("  Remove the directory %1...").arg(dirName));
      if (cleanup == SharedFiles::Cleanup::Removed)
        log.append(tr("Done.\n"));
      else {
        log.append(tr("Failed.\n"));
        return false;
      }
    }
  }

  return true;
}

QJsonObject StepExecutor_CopyFolderContents::toJson(
    const QList<QVariant>& params) {
  QJsonObject retObj;
  retObj["Mode"] = params[0].toString();
  retObj["From"] = params[1].toString();
  QJsonArray toArray;
  for (int i = 2; i < params.size(); i++) toArray.append(params[i].toString());
  retObj["To"] = toArray;
  return retObj;
}

QVariantList StepExecutor_CopyFolderContents::fromJson(
    const QJsonObject& paramsObj) {
  QVariantList params;

  if (const QJsonValue v = paramsObj["Mode"]; v.isString())
    params.append(v.toString());

  if (const QJsonValue v = paramsObj["From"]; v.isString())
    params.append(v.toString());

  if (const QJsonValue v = paramsObj["To"]; v.isArray()) {
    const QJsonArray toArray = v.toArray();
    for (const QJsonValue& toVal : toArray) params.append(toVal.toString());
  }

  return params;
}
//--------------------------------------------------

StepExecutor_RenameFile::StepExecutor_RenameFile(const ActionStep& step,
                                                 QString sequence,
                                                 QString sceneNumber,
                                                 bool isUndo)
    : StepExecutor(isUndo) {
  setPathContext(sequence, sceneNumber);
  m_mode = (step.type == Step_RenameFile) ? Rename : Copy;

  bool isFirstArg = true;
  for (auto& param : step.params) {
    QString path = WorkflowManager::instance()->decodePath(
        param.toString(), sequence, sceneNumber);

    if (isFirstArg) {
      m_renameFrom = path;
      isFirstArg   = false;
    } else
      m_renameToList.append(path);
  }
}

QWidget* StepExecutor_RenameFile::createWidget() {
  // std::cout << "m_renameFrom = " << m_renameFrom.toStdString() << std::endl;

  QWidget* widget = new QWidget();

  QString txt;
  bool showWarning;
  if (!m_isUndo) {
    // TODO: フルパス表記でもいいか？確認する
    if (m_mode == Rename)
      txt = tr("%1\n\nwill be renamed to the following path(s):\n")
                .arg(m_renameFrom);
    else if (QFileInfo(realPath(m_renameFrom))
                 .isDir())  // Copy：フォルダは内容ごとコピーされる
      txt = tr("The folder %1\n(including all of its contents)\n\nwill be "
               "copied to the following folder path(s):\n")
                .arg(m_renameFrom);
    else  // Copy
      txt = tr("%1\n\nwill be copied to the following path(s):\n")
                .arg(m_renameFrom);
    for (auto renameTo : m_renameToList) {
      txt += tr("\n - %1").arg(renameTo);
    }
    showWarning = false;
  } else if (m_mode == Copy &&
             isFolderCopyCheckSum(
                 m_checkSum)) {  // undo case（フォルダのコピー）
    showWarning = false;
    txt =
        tr("The files and folders created by the copy will be removed from:\n");
    for (auto renameTo : m_renameToList) txt += tr("\n - %1").arg(renameTo);
    txt +=
        tr("\n\nFiles and folders that already existed in the destination are "
           "not removed.");
    if (folderCopyCheckSumHasOverwrite(m_checkSum)) {
      txt +=
          tr("\n\n[WARNING] Some existing files were overwritten by the copy.\n"
             "The undo operation cannot restore them.");
      showWarning = true;
    }
  } else {  // undo case
    showWarning = true;
    txt         = tr("The following file(s) will be removed:\n");
    for (auto renameTo : m_renameToList) {
      txt += tr("\n - %1").arg(renameTo);
      // ハッシュ値が一致しているかをチェックする
      // 一つでも一致していればOK
      if (showWarning) {
        for (auto copyFromPath : m_renameToList) {
          if (fileChecksum(realPath(copyFromPath)) == m_checkSum.toByteArray())
            showWarning = false;
        }
      }
    }

    if (m_mode == Rename)
      txt += tr("\nand\n%1will be reverted.").arg(m_renameFrom);

    if (showWarning)
      txt +=
          tr("\n\n[WARNING] The hash value of the renamed file has changed.\n"
             "The undo operation may not restore the original file.");
  }

  QHBoxLayout* lay = new QHBoxLayout();
  lay->setContentsMargins(10, 10, 10, 10);
  lay->setSpacing(10);
  lay->addWidget(new IconWidget(Step_RenameFile, m_isUndo, showWarning), 0);
  lay->addWidget(new QLabel(txt, widget), 1);
  widget->setLayout(lay);

  return widget;
}

bool StepExecutor_RenameFile::execute(QString& log, QVariant& checkSum) {
  if (!m_isUndo)
    return execute_rename(log, checkSum);
  else
    return execute_undo(log);
}

bool StepExecutor_RenameFile::execute_rename(QString& log, QVariant& checkSum) {
  QString commandName =
      (m_mode == Rename) ? tr("RENAME_FILE") : tr("COPY_FILE");

  log.append(tr("[%1]\nFrom: %2\n").arg(commandName).arg(m_renameFrom));

  // コピー元・コピー先の難読化マクロは、操作に使うここで、実際のパスにデコードする
  // （表示・ログには、マクロのままのm_renameFrom/m_renameToListを使う）
  const QString renameFromReal = realPath(m_renameFrom);
  if (!QFileInfo::exists(renameFromReal)) {
    log += tr("Ambiguous or empty file path.\n");
    return false;
  }

  // Copyモードでコピー元がフォルダの場合は、内容ごとコピーする
  // （Renameモードのフォルダ移動は対象外）
  if (m_mode == Copy && QFileInfo(renameFromReal).isDir()) {
    QStringList realRenameToList;
    for (const QString& renameTo : m_renameToList)
      realRenameToList.append(realPath(renameTo));
    return copyFolderToAll(renameFromReal, realRenameToList, log, checkSum);
  }

  bool ok                           = true;
  QMessageBox::StandardButton owRet = QMessageBox::Cancel;

  checkSum = fileChecksum(renameFromReal);
  // std::cout << "orgfile = " <<
  // QString::fromUtf8(fileChecksum(m_orgPath)).toStdString() << std::endl;

  for (int i = 0; i < m_renameToList.size(); i++) {
    QString copyTo     = m_renameToList.at(i);
    QString copyToReal = realPath(copyTo);

    log.append(tr("To: %1\n").arg(copyTo));

    if (copyToReal.isEmpty() ||
        SharedFiles::sameEntry(renameFromReal, copyToReal)) {
      log += tr("Ambiguous source or destination.\n");
      return false;
    }
    if (QFile::exists(copyToReal)) {
      log.append(tr("  Remove existing file: "));

      if (owRet != QMessageBox::YesToAll) {
        owRet = QMessageBox::question(
            nullptr, tr("Question"),
            tr("The file %1 already exists.\nDo you want to overwrite it?")
                .arg(copyTo),
            {QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::Cancel},
            owRet);
      }
      if (owRet == QMessageBox::Cancel) {
        ok = false;
        log.append(tr("Canceled.\n"));
        break;
      }
      ok = QFile::remove(copyToReal);
      if (!ok) {
        log.append(tr("Failed.\n"));
        break;
      } else
        log.append(tr("Done.\n"));
    }

    log.append(tr("  Copy file: "));

    ok = createPathAndCopyFile(renameFromReal, copyToReal, log);
    // std::cout << "copied = " <<
    // QString::fromUtf8(fileChecksum(copyTo)).toStdString() << std::endl;
    // progressLabelTexts[i].append((ok) ? tr(" Done.") : tr(" Failed."));
    if (!ok) {
      log.append(tr("Failed.\n"));
      break;
    } else
      log.append(tr("Done.\n"));
  }
  if (m_mode == Rename) {
    // remove original file
    log.append(tr("  Remove original file: "));
    if (ok) {
      ok = QFile::remove(renameFromReal);
      if (!ok)
        log.append(tr("Failed.\n"));
      else
        log.append(tr("Done.\n"));
    }
  }

  return ok;
}

bool StepExecutor_RenameFile::execute_undo(QString& log) {
  if (m_mode == Rename) {
    // revertTo/revertFromPathは表示・ログ用（難読化マクロのまま）。ファイル操作には、デコードして使う
    QString revertTo = m_renameFrom;

    QString revertFromPath;
    // 復帰元のパスを決める
    for (auto copyFromPath : m_renameToList) {
      if (fileChecksum(realPath(copyFromPath)) == m_checkSum.toByteArray()) {
        revertFromPath = copyFromPath;
        break;
      } else if (revertFromPath.isEmpty())
        revertFromPath = copyFromPath;
    }
    if (revertFromPath.isEmpty()) {
      log.append(
          tr("[UNDO_RENAME_FILE]\nFailed reverting: The renamed files are "
             "missing."));
      return false;
    }

    log.append(tr("[UNDO_RENAME_FILE]\nFrom: %1\nTo: %2\n")
                   .arg(revertFromPath)
                   .arg(revertTo));

    const QString revertToReal = realPath(revertTo);
    if (!QFileInfo(realPath(revertFromPath)).isFile() ||
        revertToReal.isEmpty() ||
        SharedFiles::sameEntry(realPath(revertFromPath), revertToReal)) {
      log += tr("Ambiguous source or destination.\n");
      return false;
    }
    if (QFile::exists(revertToReal)) {
      log.append(tr("  Removing original file: \n"));
      QMessageBox::StandardButton ret = QMessageBox::question(
          nullptr, tr("Question"),
          tr("The file %1 already exists.\nDo you want to overwrite it?")
              .arg(revertTo),
          {QMessageBox::Yes | QMessageBox::Cancel}, QMessageBox::Yes);
      if (ret == QMessageBox::Cancel) {
        log.append(tr("Canceled.\n"));
        return false;
      }
      if (!QFile::remove(revertToReal)) {
        log.append(tr("Failed.\n"));
        return false;
      } else
        log.append(tr("Done.\n"));
    }

    log.append(tr("  Copy file: "));
    if (!QFile::copy(realPath(revertFromPath), revertToReal)) {
      log.append(tr("Failed.\n"));
      return false;
    } else
      log.append(tr("Done.\n"));
  } else {
    log.append(tr("[UNDO_COPY_FILE]\n"));
    // フォルダのコピーだった場合は、コピーで作られたファイル・フォルダだけを削除する
    if (isFolderCopyCheckSum(m_checkSum))
      return undoFolderCopyAll(m_checkSum, log);
  }

  // remove original file
  bool ok = true;
  for (auto removePath : m_renameToList) {
    if (m_mode == Rename)
      log.append(tr("  Remove renamed files: "));
    else
      log.append(tr("  Remove copied files: "));
    if (ok) {
      ok = QFile::remove(realPath(removePath));
      if (!ok)
        log.append(tr("Failed.\n"));
      else
        log.append(tr("Done.\n"));
    }
  }

  return ok;
}

QJsonObject StepExecutor_RenameFile::toJson(const QList<QVariant>& params) {
  QJsonObject retObj;
  retObj["Mode"] = params[0].toString();
  retObj["From"] = params[1].toString();
  QJsonArray toArray;
  for (int i = 2; i < params.size(); i++) toArray.append(params[i].toString());
  retObj["To"] = toArray;
  return retObj;
}

QVariantList StepExecutor_RenameFile::fromJson(const QJsonObject& paramsObj) {
  QVariantList params;

  if (const QJsonValue v = paramsObj["Mode"]; v.isString())
    params.append(v.toString());

  if (const QJsonValue v = paramsObj["From"]; v.isString())
    params.append(v.toString());

  if (const QJsonValue v = paramsObj["To"]; v.isArray()) {
    const QJsonArray toArray = v.toArray();
    for (const QJsonValue& toVal : toArray) params.append(toVal.toString());
  }

  return params;
}

//--------------------------------------------------
StepExecutor_MoveFlowGroup::StepExecutor_MoveFlowGroup(
    FlowGroupId beforeGroupId, const ActionStep& step, QString sequence,
    QString sceneNumber, bool isUndo)
    : StepExecutor(isUndo)
    , m_beforeId(beforeGroupId)
    , m_sceneNumber(sceneNumber) {
  m_afterId = (FlowGroupId)step.params[0].toInt();
}

QWidget* StepExecutor_MoveFlowGroup::createWidget() {
  QWidget* widget = new QWidget();

  QString beforeFlowName =
      WorkflowManager::instance()->flowGroupLabelName(m_beforeId);
  QString afterFlowName =
      WorkflowManager::instance()->flowGroupLabelName(m_afterId);

  QString txt;
  if (m_isUndo)
    txt = tr("Workflow will be reverted from \"%1\" to \"%2\".")
              .arg(afterFlowName)
              .arg(beforeFlowName);
  else
    txt = tr("Workflow will be moved from \"%1\" to \"%2\".")
              .arg(beforeFlowName)
              .arg(afterFlowName);

  QHBoxLayout* lay = new QHBoxLayout();
  lay->setContentsMargins(10, 10, 10, 10);
  lay->setSpacing(10);
  lay->addWidget(new IconWidget(Step_MoveFlowGroup, m_isUndo, false), 0);
  lay->addWidget(new QLabel(txt, widget), 1);
  widget->setLayout(lay);

  return widget;
}

bool StepExecutor_MoveFlowGroup::execute(QString& log, QVariant& checkSum) {
  log.append(tr("[MOVE FLOW GROUP]\n"));
  // 次工程に送る処理。ログもこの中で記録する
  WorkflowManager::instance()->registerCutNames(
      {m_sceneNumber}, (m_isUndo) ? m_beforeId : m_afterId);
  return true;
}

QJsonObject StepExecutor_MoveFlowGroup::toJson(const QList<QVariant>& params) {
  QJsonObject retObj;
  retObj["MoveToFlowGroup"] = WorkflowManager::instance()->flowGroupIdName(
      (FlowGroupId)params[0].toInt());
  return retObj;
}

QVariantList StepExecutor_MoveFlowGroup::fromJson(
    const QJsonObject& paramsObj) {
  QVariantList params;

  if (const QJsonValue v = paramsObj["MoveToFlowGroup"]; v.isString()) {
    FlowGroupId fgId =
        WorkflowManager::instance()->IdNameToFlowGroupId(v.toString());
    params.append(static_cast<int>(fgId));
  }

  return params;
}
//--------------------------------------------------

StepExecutor_CheckExistence::StepExecutor_CheckExistence(const ActionStep& step,
                                                         QString sequence,
                                                         QString sceneNumber,
                                                         bool isUndo)
    : StepExecutor(isUndo), m_checkMode(Existence), m_target(File) {
  setPathContext(sequence, sceneNumber);
  QString modeStr = step.params[0].toString();
  if (modeStr.startsWith("[ABSENCE]")) {
    m_checkMode = Absence;
    modeStr     = modeStr.right(modeStr.size() - QString("[ABSENCE]").length());
  } else if (modeStr.startsWith("[EXISTENCE]")) {
    m_checkMode = Existence;
    modeStr = modeStr.right(modeStr.size() - QString("[EXISTENCE]").length());
  }
  if (modeStr.startsWith("[SCENEINFO]")) m_target = SceneInfo;

  for (int i = 1; i < step.params.size(); i++) {
    QString checkPath = step.params[i].toString();
    if (m_target == File)
      checkPath = WorkflowManager::instance()->decodePath(checkPath, sequence,
                                                          sceneNumber);

    m_filesToBeChecked.append(checkPath);
  }

  if (m_target == SceneInfo) {
    m_cutInfo =
        WorkflowManager::instance()->sceneInfoTags(sequence, sceneNumber);
  }
}

QWidget* StepExecutor_CheckExistence::createWidget() {
  bool ok = true;
  QString txt;
  if (m_isUndo)
    txt = tr("This check is ignored during undo.");
  else {
    if (m_target == File) {
      for (const QString& display_checkPath : m_filesToBeChecked) {
        // ファイルの有無の確認には、難読化マクロをデコードした実際のパスを使う。
        // 画面には、デコード前（マクロのまま）のパスを表示する
        QString checkPath = realPath(display_checkPath);

        if (checkPath.isEmpty() && !display_checkPath.isEmpty()) {
          txt += tr("Ambiguous or empty file path.\n");
          ok = false;
        } else if (m_checkMode == Existence) {
          if (QFileInfo(checkPath).exists())
            txt += tr("[OK]");
          else {
            txt += tr("[MISSING]");
            ok = false;
          }
        } else {  // Absence : 無いことを確認する
          if (!QFileInfo(checkPath).exists())
            txt += tr("[OK]");
          else {
            txt += tr("[EXISTS]");
            ok = false;
          }
        }
        txt += " " + display_checkPath + "\n";
      }

      if (m_checkMode == Existence) {
        if (ok)
          txt += "\n" + tr("File(s) necessary to proceed the action exist!");
        else
          txt +=
              "\n" + tr("File(s) necessary to proceed the action is missing!");
      } else {  // Absence : 無いことを確認する
        if (ok)
          txt += "\n" + tr("File absence check passed.");
        else
          txt += "\n" + tr("File(s) that should not exist to proceed the "
                           "action are found!");
      }
    } else {  // Check scene info

      for (int i = 0; i < m_filesToBeChecked.size(); i += 2) {
        QString key   = m_filesToBeChecked[i];
        QString value = m_filesToBeChecked[i + 1];
        SceneInfoTagDisplayProperties prop =
            WorkflowManager::instance()->sceneInfoTagDisplayProperties(
                key, (value.isEmpty()) ? "1" : value);

        if (m_checkMode == Existence) {
          // カット情報が無い、またはカット情報が0ならfalse
          // valueが空でなく、カット情報がvalueと一致していなければfalse
          if (!m_cutInfo.contains(key) || m_cutInfo.value(key) == "0" ||
              (!value.isEmpty() && m_cutInfo.value(key) != value)) {
            ok = false;
            txt += tr("[MISSING] Cut info \"%1\" is missing!\n")
                       .arg(prop.labelTxt);
          } else
            txt += tr("[OK] Cut info \"%1\" is set.\n").arg(prop.labelTxt);
        }
        // ここから Absenceの場合実装
        else {  // checkMode == Absence
          // カット情報が有り、かつ0でない
          //  かつvalueが空またはカット情報がvalueと一致している場合false
          if (m_cutInfo.contains(key) && m_cutInfo.value(key) != "0" &&
              (value.isEmpty() || m_cutInfo.value(key) == value)) {
            ok = false;
            txt += tr("[EXISTS] Cut info \"%1\" exists!\n").arg(prop.labelTxt);
          } else
            txt += tr("[OK] Cut info \"%1\" is not set.\n").arg(prop.labelTxt);
        }
      }

      if (m_checkMode == Existence) {
        if (ok)
          txt +=
              "\n" + tr("Scene info(s) necessary to proceed the action exist!");
        else
          txt +=
              "\n" +
              tr("Scene info(s) necessary to proceed the action is missing!");
      } else {  // Absence : 無いことを確認する
        if (ok)
          txt += "\n" + tr("Scene info absence check passed.");
        else
          txt += "\n" + tr("Scene info(s) that should not exist to proceed the "
                           "action are found!");
      }
    }
  }

  QWidget* widget = new QWidget();

  QHBoxLayout* lay = new QHBoxLayout();
  lay->setContentsMargins(10, 10, 10, 10);
  lay->setSpacing(10);
  lay->addWidget(new IconWidget(Step_CheckExistence, m_isUndo, !ok), 0);
  lay->addWidget(new QLabel(txt, widget), 1);
  widget->setLayout(lay);
  return widget;
}

bool StepExecutor_CheckExistence::execute(QString& log, QVariant& checkSum) {
  if (!m_isUndo && m_target == File)
    for (const QString& path : m_filesToBeChecked)
      if (!path.isEmpty() && realPath(path).isEmpty()) {
        log += tr("Ambiguous or empty file path.\n");
        return false;
      }
  return true;
}

QJsonObject StepExecutor_CheckExistence::toJson(const QList<QVariant>& params) {
  QJsonObject retObj;
  retObj["Mode"] = params[0].toString();
  QJsonArray checkPathArray;
  for (int i = 1; i < params.size(); i++)
    checkPathArray.append(params[i].toString());
  retObj["PathsToBeChecked"] = checkPathArray;
  return retObj;
}

QVariantList StepExecutor_CheckExistence::fromJson(
    const QJsonObject& paramsObj) {
  QVariantList params;

  if (const QJsonValue v = paramsObj["Mode"]; v.isString())
    params.append(v.toString());

  if (const QJsonValue v = paramsObj["PathsToBeChecked"]; v.isArray()) {
    const QJsonArray checkPathArray = v.toArray();
    for (const QJsonValue& checkPathVal : checkPathArray)
      params.append(checkPathVal.toString());
  }

  return params;
}

//--------------------------------------------------

StepExecutor_AskDelete::StepExecutor_AskDelete(const ActionStep& step,
                                               QString orgPath,
                                               QString sequence,
                                               QString sceneNumber, bool isUndo)
    : StepExecutor(isUndo)
    , m_fileToBeDeleted(orgPath)
    , m_doesExist(true)
    , m_isDir(false)
    , m_buttonGroup(nullptr) {
  setPathContext(sequence, sceneNumber);
  // 初期値
  m_doDelete = step.params[0].toBool();
  // キューファイル以外を消す場合
  if (step.params.size() >= 2) {
    // m_fileToBeDeletedは、難読化マクロをデコードしない（画面・ログ用）。有無の確認と削除には、デコードして使う
    m_fileToBeDeleted = WorkflowManager::instance()->decodePath(
        step.params[1].toString(), sequence, sceneNumber);

    QFileInfo fi(realPath(m_fileToBeDeleted));
    m_doesExist = fi.exists();
    m_isDir     = fi.isDir();
  }
}

QWidget* StepExecutor_AskDelete::createWidget() {
  QWidget* widget = new QWidget();

  QHBoxLayout* lay = new QHBoxLayout();
  lay->setContentsMargins(10, 10, 10, 10);
  lay->setSpacing(10);
  // always show warning mark on undo
  lay->addWidget(new IconWidget(Step_AskDelete, m_isUndo, m_isUndo), 0);

  if (m_isUndo)
    lay->addWidget(new QLabel(tr("This action cannot be undone."), widget), 1);
  else {
    QString targetTypeStr = (m_isDir) ? tr("folder") : tr("file");

    if (!m_doesExist) {
      lay->addWidget(
          new QLabel(
              tr("This action will be skipped since the %1 %2 does not exist.")
                  .arg(targetTypeStr)
                  .arg(m_fileToBeDeleted),
              widget),
          1);
    } else {
      QVBoxLayout* rightLayout = new QVBoxLayout();
      rightLayout->setContentsMargins(5, 5, 5, 5);
      rightLayout->setSpacing(5);
      {
        rightLayout->addWidget(
            new QLabel(tr("Do you want to delete the following %1?:\n\n%2\n")
                           .arg(targetTypeStr)
                           .arg(m_fileToBeDeleted),
                       widget),
            0, Qt::AlignLeft);

        m_buttonGroup = new QButtonGroup(widget);
        m_buttonGroup->setExclusive(true);

        QHBoxLayout* buttonsLay = new QHBoxLayout();
        buttonsLay->setContentsMargins(0, 0, 0, 0);
        buttonsLay->setSpacing(20);
        {
          QRadioButton* btn =
              new QRadioButton(tr("Delete the %1.").arg(targetTypeStr), widget);
          m_buttonGroup->addButton(btn, 0);
          btn->setChecked(m_doDelete);
          buttonsLay->addWidget(btn, 0);

          btn = new QRadioButton(tr("Keep the %1.").arg(targetTypeStr), widget);
          m_buttonGroup->addButton(btn, 1);
          btn->setChecked(!m_doDelete);
          buttonsLay->addWidget(btn, 0);

          buttonsLay->addStretch(1);
        }
        rightLayout->addLayout(buttonsLay, 0);

        rightLayout->addWidget(
            new QLabel(tr("\nNote that this action cannot be undone."), widget),
            0, Qt::AlignLeft);
      }
      lay->addLayout(rightLayout, 1);
    }
  }
  widget->setLayout(lay);

  return widget;
}

bool StepExecutor_AskDelete::execute(QString& log, QVariant& checkSum) {
  if (m_isUndo) return true;
  if (!m_fileToBeDeleted.isEmpty() && realPath(m_fileToBeDeleted).isEmpty()) {
    log += tr("Ambiguous or empty file path.\n");
    return false;
  }
  if (!m_doesExist) return true;
  m_doDelete = (m_buttonGroup->checkedId() == 0);
  if (!m_doDelete) return true;

  bool ok = false;
  // 削除には、難読化マクロをデコードした実際のパスを使う（ログには、マクロのままのパスを残す）
  const QString fileToBeDeletedReal = realPath(m_fileToBeDeleted);
  if (fileToBeDeletedReal.isEmpty()) {
    log += tr("Ambiguous or empty file path.\n");
    return false;
  }
  if (m_isDir) {
    log.append(tr("[DELETE FOLDER]\nDeleting the folder %1...")
                   .arg(m_fileToBeDeleted));
    // 当面安全のため、空のフォルダでなければ消せないようにする
    ok = SharedFiles::removeEmptyFolder(fileToBeDeletedReal) ==
         SharedFiles::Cleanup::Removed;
  } else {
    log.append(
        tr("[DELETE FILE]\nDeleting the file %1...").arg(m_fileToBeDeleted));
    ok = QFile(fileToBeDeletedReal).remove();
  }
  if (ok)
    log.append(tr("Done.\n"));
  else {
    log.append(tr("Failed.\n"));
  }
  return ok;
}

QJsonObject StepExecutor_AskDelete::toJson(const QList<QVariant>& params) {
  QJsonObject retObj;
  retObj["DeleteDefault"] = params[0].toBool();

  if (params.size() >= 2) retObj["FileToDelete"] = params[1].toString();
  return retObj;
}

QVariantList StepExecutor_AskDelete::fromJson(const QJsonObject& paramsObj) {
  QVariantList params;

  if (const QJsonValue v = paramsObj["DeleteDefault"]; v.isBool())
    params.append(v.toBool());

  if (const QJsonValue v = paramsObj["FileToDelete"]; v.isString())
    params.append(v.toString());

  return params;
}
//--------------------------------------------------

StepExecutor_CreateCueFile::StepExecutor_CreateCueFile(const ActionStep& step,
                                                       QString sequence,
                                                       QString sceneNumber,
                                                       bool isUndo,
                                                       bool localDestination)
    : StepExecutor(isUndo), m_fileContent(""), m_fileExists(false) {
  setPathContext(sequence, sceneNumber);
  m_fileToBeCreated =
      localDestination ? step.params[0].toString()
                       : WorkflowManager::instance()->decodePath(
                             step.params[0].toString(), sequence, sceneNumber);
  if (step.params.count() >= 2) m_fileContent = step.params[1].toString();
  if (!m_isUndo) m_fileExists = QFileInfo(realPath(m_fileToBeCreated)).isFile();
}

// checkSum の形式：{ 上書きしたか(bool), 上書き前の中身(QByteArray) }
// （この形式より前に実行したアクションの checkSum は空なので、新しく作った扱いになる）
bool StepExecutor_CreateCueFile::overwrittenContent(QByteArray& content) const {
  const QVariantList list = m_checkSum.toList();
  if (list.size() < 2 || !list[0].toBool()) return false;
  content = list[1].toByteArray();
  return true;
}

QWidget* StepExecutor_CreateCueFile::createWidget() {
  QWidget* widget = new QWidget();

  QHBoxLayout* lay = new QHBoxLayout();
  lay->setContentsMargins(10, 10, 10, 10);
  lay->setSpacing(10);
  // 同名のファイルを上書きするときは、警告のマークを付ける
  lay->addWidget(new IconWidget(Step_CreateCueFile, m_isUndo, m_fileExists),
                 0);

  QString txt;
  QByteArray orgContent;
  if (m_isUndo) {
    if (overwrittenContent(orgContent))
      txt = tr("The cue file %1 will be restored to the content before the "
               "action.")
                .arg(m_fileToBeCreated);
    else
      txt = tr("The cue file %1 will be removed.").arg(m_fileToBeCreated);
  } else if (m_fileExists)
    txt = tr("The cue file %1 already exists. It will be overwritten.")
              .arg(m_fileToBeCreated);
  else
    txt = tr("The cue file %1 will be created.").arg(m_fileToBeCreated);

  lay->addWidget(new QLabel(txt, widget), 1);
  widget->setLayout(lay);

  return widget;
}

bool StepExecutor_CreateCueFile::execute(QString& log, QVariant& checkSum) {
  // ファイル操作には、難読化マクロをデコードした実際のパスを使う（画面・ログには、マクロのままのパスを使う）
  const QString fileToBeCreatedReal = realPath(m_fileToBeCreated);
  if (fileToBeCreatedReal.isEmpty()) {
    log += tr("Ambiguous or empty file path.\n");
    return false;
  }

  if (m_isUndo) {
    // 上書きしていた場合は、消さずに、上書き前の中身に戻す
    QByteArray orgContent;
    if (overwrittenContent(orgContent)) {
      log.append(tr("[UNDO_CREATE CUE FILE]\nRestoring %1 ...")
                     .arg(m_fileToBeCreated));
      QFile file(fileToBeCreatedReal);
      bool ok = file.open(QIODevice::WriteOnly) &&
                file.write(orgContent) == orgContent.size();
      file.close();
      if (ok)
        log.append(tr("Done.\n"));
      else
        log.append(tr("Failed.\n"));
      return ok;
    }

    log.append(
        tr("[UNDO_CREATE CUE FILE]\nRemoving %1 ...").arg(m_fileToBeCreated));

    bool ok = QFile::remove(fileToBeCreatedReal);
    if (ok)
      log.append(tr("Done.\n"));
    else
      log.append(tr("Failed.\n"));
    return true;
  } else {
    log.append(tr("[CREATE CUE FILE]\nCreating %1 ...").arg(m_fileToBeCreated));

    // 同名のファイルがあるときは、上書きしてよいか確認し、取り消し用に上書き前の中身を取っておく
    checkSum = QVariantList{false};
    if (QFileInfo(fileToBeCreatedReal).isFile()) {
      QMessageBox::StandardButton ret = QMessageBox::question(
          nullptr, tr("Question"),
          tr("The file %1 already exists.\nDo you want to overwrite it?")
              .arg(m_fileToBeCreated),
          {QMessageBox::Yes | QMessageBox::Cancel}, QMessageBox::Yes);
      if (ret == QMessageBox::Cancel) {
        log.append(tr("Canceled.\n"));
        return false;
      }
      QFile orgFile(fileToBeCreatedReal);
      if (!orgFile.open(QIODevice::ReadOnly)) {
        log.append(tr("Failed.\n"));
        return false;
      }
      checkSum = QVariantList{true, orgFile.readAll()};
      orgFile.close();
    }

    // create parent folder
    QDir parentDir = QFileInfo(fileToBeCreatedReal).dir();
    if (!parentDir.exists()) {
      QMessageBox::StandardButton ret = QMessageBox::question(
          nullptr, tr("Question"),
          tr("The folder %1 does not exist.\nDo you want to create it?")
              .arg(hide(parentDir.absolutePath())),
          {QMessageBox::Yes | QMessageBox::Cancel}, QMessageBox::Yes);
      if (ret == QMessageBox::Cancel) {
        log.append(tr("Canceled to create folder %1.\n")
                       .arg(hide(parentDir.absolutePath())));
        return false;
      }

      bool ok = parentDir.mkpath(".");
      if (!ok) {
        log.append(tr("Failed to create folder %1.\n")
                       .arg(hide(parentDir.absolutePath())));
        return false;
      }
    }

    QFile file(fileToBeCreatedReal);

    bool ok = file.open(QIODevice::WriteOnly | QIODevice::Text);

    if (!m_fileContent.isEmpty()) {
      QTextStream out(&file);
      out << m_fileContent << "\n";
    }

    file.close();

    if (ok)
      log.append(tr("Done.\n"));
    else
      log.append(tr("Failed.\n"));
    return ok;
  }
  return true;
}

QJsonObject StepExecutor_CreateCueFile::toJson(const QList<QVariant>& params) {
  QJsonObject retObj;
  retObj["FileToBeCreated"] = params[0].toString();
  if (params.size() >= 2) retObj["FileContent"] = params[1].toString();
  return retObj;
}

QVariantList StepExecutor_CreateCueFile::fromJson(
    const QJsonObject& paramsObj) {
  QVariantList params;

  if (const QJsonValue v = paramsObj["FileToBeCreated"]; v.isString())
    params.append(v.toString());

  if (const QJsonValue v = paramsObj["FileContent"]; v.isString())
    params.append(v.toString());

  return params;
}
//--------------------------------------------------

StepExecutor_StartEndBranchFlow::StepExecutor_StartEndBranchFlow(
    bool isStart, const ActionStep& step, QString sequence, QString sceneNumber,
    bool isUndo)
    : StepExecutor(isUndo), m_isStart(isStart), m_sceneNumber(sceneNumber) {
  m_fgId = (FlowGroupId)step.params[0].toInt();
}

QWidget* StepExecutor_StartEndBranchFlow::createWidget() {
  QWidget* widget = new QWidget();

  QString flowName = WorkflowManager::instance()->flowGroupLabelName(m_fgId);

  QString txt;
  if (m_isStart) {
    if (m_isUndo)
      txt = tr("Branch workflow \"%1\" will be cancelled.").arg(flowName);
    else
      txt = tr("Branch workflow \"%1\" will be started.").arg(flowName);
  } else {
    if (m_isUndo)
      txt = tr("Branch workflow \"%1\" will be reverted.").arg(flowName);
    else
      txt = tr("Branch workflow \"%1\" will be ended.").arg(flowName);
  }

  QHBoxLayout* lay = new QHBoxLayout();
  lay->setContentsMargins(10, 10, 10, 10);
  lay->setSpacing(10);
  lay->addWidget(
      new IconWidget((m_isStart) ? Step_StartBranchFlow : Step_EndBranchFlow,
                     m_isUndo, false),
      0);
  lay->addWidget(new QLabel(txt, widget), 1);
  widget->setLayout(lay);

  return widget;
}

bool StepExecutor_StartEndBranchFlow::execute(QString& log,
                                              QVariant& checkSum) {
  if (m_isStart)
    log.append(tr("[START BRANCH FLOW GROUP]\n"));
  else
    log.append(tr("[END BRANCH FLOW GROUP]\n"));

  // 傍流工程の登録処理。ログもこの中で記録する
  WorkflowManager::instance()->registerCutNameToBranchFlowGroup(
      {m_sceneNumber}, m_fgId, m_isStart != m_isUndo);
  return true;
}

QJsonObject StepExecutor_StartEndBranchFlow::toJson(
    const QList<QVariant>& params) {
  QJsonObject retObj;
  retObj["FlowGroupIdName"] = WorkflowManager::instance()->flowGroupIdName(
      (FlowGroupId)params[0].toInt());
  return retObj;
}

QVariantList StepExecutor_StartEndBranchFlow::fromJson(
    const QJsonObject& paramsObj) {
  QVariantList params;

  if (const QJsonValue v = paramsObj["FlowGroupIdName"]; v.isString()) {
    FlowGroupId fgId =
        WorkflowManager::instance()->IdNameToFlowGroupId(v.toString());
    params.append(static_cast<int>(fgId));
  }

  return params;
}
//--------------------------------------------------

StepExecutor_SetSceneInfo::StepExecutor_SetSceneInfo(const ActionStep& step,
                                                     QString sequence,
                                                     QString sceneNumber,
                                                     bool isUndo)
    : StepExecutor(isUndo), m_sequence(sequence), m_sceneNumber(sceneNumber) {
  m_infoName = step.params[0].toString();
  if (step.params.size() > 1) m_value = step.params[1];
  // 値が空の場合は「カット情報を外す」。ppmprjから読んだ値は空でも空の文字列で、
  // Qt6ではQVariant::isNull()がtrueにならないため、ここで値なしにそろえる
  if (m_value.toString().isEmpty()) m_value = QVariant();
}

QWidget* StepExecutor_SetSceneInfo::createWidget() {
  QString labelTxt;
  if (m_isUndo) {
    QList<QVariant> undoInfo = m_checkSum.toList();
    // もともとInfoが入っていなかった場合
    if (undoInfo.isEmpty())
      labelTxt =
          tr("Info \"%1\" will be released.")
              .arg(WorkflowManager::instance()
                       ->sceneInfoTagDisplayProperties(m_infoName, m_value)
                       .labelTxt);
    else
      labelTxt =
          tr("Info \"%1\" will be reverted to \"%2\".")
              .arg(WorkflowManager::instance()
                       ->sceneInfoTagDisplayProperties(m_infoName, m_value)
                       .labelTxt)
              .arg(WorkflowManager::instance()
                       ->sceneInfoTagDisplayProperties(m_infoName, undoInfo[0])
                       .labelTxt);
  } else {
    if (m_value.isNull()) {
      QVariant currentInfo = WorkflowManager::instance()->sceneInfoTag(
          m_sequence, m_sceneNumber, m_infoName, QVariant());
      if (!currentInfo.isValid())
        labelTxt =
            tr("There is no cut info \"%1\". Nothing happens in this step.")
                .arg(m_infoName);
      else
        labelTxt = tr("Info \"%1\" will be released.")
                       .arg(WorkflowManager::instance()
                                ->sceneInfoTagDisplayProperties(m_infoName,
                                                                currentInfo)
                                .labelTxt);
    } else
      labelTxt =
          tr("Info \"%1\" will be registered.")
              .arg(WorkflowManager::instance()
                       ->sceneInfoTagDisplayProperties(m_infoName, m_value)
                       .labelTxt);
  }
  QWidget* widget = new QWidget();

  QHBoxLayout* lay = new QHBoxLayout();
  lay->setContentsMargins(10, 10, 10, 10);
  lay->setSpacing(10);
  lay->addWidget(new IconWidget(Step_SetSceneInfo, m_isUndo), 0);
  lay->addWidget(new QLabel(labelTxt, widget), 1);
  widget->setLayout(lay);
  return widget;
}

bool StepExecutor_SetSceneInfo::execute(QString& log, QVariant& checkSum) {
  if (m_isUndo) {
    log.append(tr("[UNDO_SET_SCENE_INFO]\n"));
    QList<QVariant> undoInfo = m_checkSum.toList();
    // もともとInfoが入っていなかった場合
    if (undoInfo.isEmpty()) {
      log.append(tr("Releasing Info : %1\n").arg(m_infoName));
      WorkflowManager::instance()->releaseSceneInfoTag(
          m_sequence, m_sceneNumber, m_infoName);
    } else {
      log.append(tr("Reverting Info : %1\n").arg(m_infoName));
      WorkflowManager::instance()->saveSceneInfoTag(m_sequence, m_sceneNumber,
                                                    m_infoName, undoInfo[0]);
    }
  } else {
    log.append(tr("[SET_SCENE_INFO]\n"));
    if (m_value.isNull()) {
      QMap<QString, QVariant> infos =
          WorkflowManager::instance()->sceneInfoTags(m_sequence, m_sceneNumber);
      if (infos.contains(m_infoName)) {
        // Undoのためにリストを格納
        checkSum = QList<QVariant>{infos.value(m_infoName)};
        log.append(tr("Releasing Info : %1\n").arg(m_infoName));
        WorkflowManager::instance()->releaseSceneInfoTag(
            m_sequence, m_sceneNumber, m_infoName);
      } else {
        checkSum = QList<QVariant>{};
        log.append(
            tr("There is no cut info %1. Do nothing.\n").arg(m_infoName));
      }

    } else {
      QMap<QString, QVariant> infos =
          WorkflowManager::instance()->sceneInfoTags(m_sequence, m_sceneNumber);
      if (infos.contains(m_infoName))
        // Undoのためにリストを格納
        checkSum = QList<QVariant>{infos.value(m_infoName)};
      else
        checkSum = QList<QVariant>{};

      log.append(tr("Setting Info : %1\n").arg(m_infoName));
      WorkflowManager::instance()->saveSceneInfoTag(m_sequence, m_sceneNumber,
                                                    m_infoName, m_value);
    }
  }
  log.append(tr("Done.\n"));
  return true;
}

QJsonObject StepExecutor_SetSceneInfo::toJson(const QList<QVariant>& params) {
  QJsonObject retObj;
  retObj["InfoName"] = params[0].toString();
  retObj["Value"]    = params[1].toString();  // String固定で
  return retObj;
}

QVariantList StepExecutor_SetSceneInfo::fromJson(const QJsonObject& paramsObj) {
  QVariantList params;

  if (const QJsonValue v = paramsObj["InfoName"]; v.isString())
    params.append(v.toString());

  if (const QJsonValue v = paramsObj["Value"]; v.isString())
    params.append(v.toString());

  return params;
}

//--------------------------------------------------
StepExecutor_InputGoogleSpreadsheet::StepExecutor_InputGoogleSpreadsheet(
    const ActionStep& step, QString sequence, QString sceneNumber, bool isUndo)
    : StepExecutor(isUndo) {
  m_sceneNumbers = sceneNumber.split("_");
  m_sheetName    = step.params[0].toString();
  m_sheetName.replace("[EPISODE]",
                      WorkflowManager::instance()->currentEpisode());
  m_flowName = step.params[1].toString();

  // Valueフィールドがセミコロン区切りの場合、各文字列を候補としてコンボボックスから選べるようにする
  m_valueOptions = step.params[2].toString().split(";", Qt::SkipEmptyParts);
  if (m_valueOptions.isEmpty()) m_valueOptions.append(QString());
  for (auto& option : m_valueOptions) {
    option = option.trimmed();
    if (option == "[CURRENT_DATE]")
      option = QDateTime::currentDateTime().toString("yyyy/MM/dd");
    if (option.contains("[USER_NAME]"))
      option.replace("[USER_NAME]", UserSettings::instance()->userName());
  }

  // バンクカットのカット表も記入する
  m_bankCutStartIndex = m_sceneNumbers.size();
  QVariant bankCuts   = WorkflowManager::instance()->sceneInfoTag(
      sequence, sceneNumber, "BANK", QVariant());
  if (bankCuts.isValid()) m_sceneNumbers.append(bankCuts.toStringList());
}

QString StepExecutor_InputGoogleSpreadsheet::buildInputLabelText() const {
  // スプレッドシートのシート〇 : ステップ△ の値が以下の通り入力されます：
  // カット● → "値"
  QString labelTxt = tr("The values of Sheet \"%1\" : Step \"%2\" in the "
                        "spreadsheet will be input as follows :\n\n")
                         .arg(m_sheetName)
                         .arg(m_flowName);
  QString currentValue = m_valueOptions.value(m_selectedValueIndex);
  int index            = 0;
  for (auto scheneNumber : m_sceneNumbers) {
    if (index < m_bankCutStartIndex)
      labelTxt +=
          tr("  - Scene %1 -> %2\n").arg(scheneNumber).arg(currentValue);
    else
      labelTxt +=
          tr("  - [BANK] Scene %1 -> %2\n").arg(scheneNumber).arg(currentValue);

    index++;
  }
  return labelTxt;
}

QWidget* StepExecutor_InputGoogleSpreadsheet::createWidget() {
  QString labelTxt;
  bool isActive       = GoogleGateway::instance()->isActive();
  bool showValueCombo = false;
  if (!isActive) {
    // プロジェクトにGoogle Apps
    // ScriptのURLが設定されていないため、このステップは飛ばされます。
    labelTxt =
        tr("This step will be skipped because the project does not have a "
           "Google Apps Script URL setting.");
  } else {
    if (m_isUndo) {
      QList<QVariant> undoInfo = m_checkSum.toList();
      // スプレッドシートのシート〇 : ステップ△ の値が以下の通り戻されます：
      // カット● → 空欄 / "Undoの値"
      labelTxt = tr("The values of Sheet \"%1\" : Step \"%2\" in the "
                    "spreadsheet will be reverted as follows :\n\n")
                     .arg(m_sheetName)
                     .arg(m_flowName);
      int index = 0;
      for (auto undoVal : undoInfo) {
        QString prevStr = undoVal.toString();
        if (index < m_bankCutStartIndex) {
          if (prevStr.isEmpty())
            labelTxt +=
                tr("  - Scene %1 -> Empty cell\n").arg(m_sceneNumbers[index]);
          else
            labelTxt += tr("  - Scene %1 -> \"%2\"\n")
                            .arg(m_sceneNumbers[index])
                            .arg(prevStr);
        } else {
          if (prevStr.isEmpty())
            labelTxt += tr("  - [BANK] Scene %1 -> Empty cell\n")
                            .arg(m_sceneNumbers[index]);
          else
            labelTxt += tr("  - [BANK] Scene %1 -> \"%2\"\n")
                            .arg(m_sceneNumbers[index])
                            .arg(prevStr);
        }
        index++;
      }
    } else {
      labelTxt       = buildInputLabelText();
      showValueCombo = m_valueOptions.size() > 1;
    }
    // 完了ボタンを押した後、このステップは非同期に処理されます。取り消しボタンはステップ完了後に有効になります。
    labelTxt +=
        tr("\nN.B. After pressing the Done button, this step will be processed "
           "asynchronously.\n"
           "The undo button will become active after the step is completed.");
  }
  QWidget* widget = new QWidget();

  QVBoxLayout* contentLay = new QVBoxLayout();
  contentLay->setContentsMargins(0, 0, 0, 0);
  contentLay->setSpacing(6);
  QLabel* label = new QLabel(labelTxt, widget);
  contentLay->addWidget(label);

  if (showValueCombo) {
    // Valueフィールドがセミコロン区切りだった場合、実際にスプレッドシートへ入力する値を選べるようにする
    QComboBox* valueCombo = new QComboBox(widget);
    valueCombo->addItems(m_valueOptions);
    valueCombo->setCurrentIndex(m_selectedValueIndex);
    QHBoxLayout* valComboLay = new QHBoxLayout();
    valComboLay->setContentsMargins(0, 0, 0, 0);
    valComboLay->setSpacing(4);
    {
      valComboLay->addWidget(new QLabel(tr("Input Value:")), 0);
      valComboLay->addWidget(valueCombo, 1);
    }
    contentLay->addLayout(valComboLay);
    QObject::connect(
        valueCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        valueCombo, [this, label](int index) {
          m_selectedValueIndex = index;
          label->setText(buildInputLabelText() +
                         tr("\nN.B. After pressing the Done button, this step "
                            "will be processed asynchronously.\n"
                            "The undo button will become active after the step "
                            "is completed."));
        });
  }

  QHBoxLayout* lay = new QHBoxLayout();
  lay->setContentsMargins(10, 10, 10, 10);
  lay->setSpacing(10);
  lay->addWidget(
      new IconWidget(Step_InputGoogleSpreadsheet, m_isUndo, !isActive), 0);
  lay->addLayout(contentLay, 1);
  widget->setLayout(lay);
  return widget;
}

bool StepExecutor_InputGoogleSpreadsheet::execute(QString& log,
                                                  QVariant& checkSum) {
  bool isActive = GoogleGateway::instance()->isActive();
  bool ret      = true;
  if (m_isUndo) {
    log.append(tr("[UNDO_INPUT_GOOGLE_SPREADSHEET]\n"));
    if (isActive) {
      QStringList values = m_checkSum.toStringList();
      QStringList dummyList;
      ret = GoogleGateway::instance()->updateSingleCell(
          m_sheetName, m_sceneNumbers, m_flowName, values, dummyList,
          m_googleStepIndex);
    } else
      log.append(tr(" (Skipped - No Google Apps Script URL)\n"));
  } else {
    log.append(tr("[INPUT_GOOGLE_SPREADSHEET]\n"));
    QStringList prevList;
    if (isActive)
      ret = GoogleGateway::instance()->updateSingleCell(
          m_sheetName, m_sceneNumbers, m_flowName,
          {m_valueOptions.value(m_selectedValueIndex)}, prevList,
          m_googleStepIndex);
    else
      log.append(tr(" (Skipped - No Google Apps Script URL)\n"));
    checkSum = prevList;
  }
  log.append(tr("Done.\n"));
  return ret;
}

QJsonObject StepExecutor_InputGoogleSpreadsheet::toJson(
    const QList<QVariant>& params) {
  QJsonObject retObj;
  retObj["SheetName"] = params[0].toString();
  retObj["FlowName"]  = params[1].toString();
  retObj["Value"]     = params[2].toString();
  return retObj;
}

QVariantList StepExecutor_InputGoogleSpreadsheet::fromJson(
    const QJsonObject& paramsObj) {
  QVariantList params;

  if (const QJsonValue v = paramsObj["SheetName"]; v.isString())
    params.append(v.toString());

  if (const QJsonValue v = paramsObj["FlowName"]; v.isString())
    params.append(v.toString());

  if (const QJsonValue v = paramsObj["Value"]; v.isString())
    params.append(v.toString());

  return params;
}

//--------------------------------------------------

StepExecutor_RegisterProgress::StepExecutor_RegisterProgress(
    QString sceneNumber, WorkflowType fromType, QList<WorkflowType> toTypes,
    bool isUndo)
    : StepExecutor(isUndo)
    , m_sceneNumber(sceneNumber)
    , m_fromType(fromType)
    , m_toTypes(toTypes) {}

QVariant StepExecutor_RegisterProgress::encodeToTypes(
    const QList<WorkflowType>& toTypes) {
  // 他のステップのcheckSum（QVariantList等）と取り違えないよう、接頭辞つきの文字列にする
  QStringList ids;
  for (auto toType : toTypes) ids.append(QString::number(toType));
  return QString("PROGRESS:") + ids.join(",");
}

bool StepExecutor_RegisterProgress::decodeToTypes(
    const QVariant& checkSum, QList<WorkflowType>& toTypes) {
  if (checkSum.typeId() != QMetaType::QString) return false;
  const QString str = checkSum.toString();
  if (!str.startsWith("PROGRESS:")) return false;
  toTypes.clear();
  for (const QString& id : str.mid(9).split(",", Qt::SkipEmptyParts))
    toTypes.append(id.toUInt());
  return true;
}

bool StepExecutor_RegisterProgress::execute(QString& log, QVariant& checkSum) {
  if (m_isUndo) {
    // 実行時に登録した次工程を使う（記録が無ければ、現在の条件で求めた次工程を使う）
    QList<WorkflowType> toTypes = m_toTypes;
    decodeToTypes(m_checkSum, toTypes);
    // toTypesから抜いて、fromTypeに入れる
    for (auto toType : toTypes)
      WorkflowManager::instance()->setProgress(m_sceneNumber, toType, false);
    WorkflowManager::instance()->setProgress(m_sceneNumber, m_fromType, true);
  } else {
    checkSum = encodeToTypes(m_toTypes);
    // fromTypeから抜いて、toTypesに入れる
    WorkflowManager::instance()->setProgress(m_sceneNumber, m_fromType, false);
    for (auto toType : m_toTypes)
      WorkflowManager::instance()->setProgress(m_sceneNumber, toType, true);
  }
  return true;
}

//-----------------------

StepExecutor_AddProgress::StepExecutor_AddProgress(const ActionStep& step,
                                                   QString sceneNumber,
                                                   bool isUndo)
    : StepExecutor(isUndo) {
  m_type = step.params.isEmpty() ? (WorkflowType)Wf_Undefined
                                 : (WorkflowType)step.params[0].toUInt();
}

QWidget* StepExecutor_AddProgress::createWidget() {
  QWidget* widget = new QWidget();

  QHBoxLayout* lay = new QHBoxLayout();
  lay->setContentsMargins(10, 10, 10, 10);
  lay->setSpacing(10);

  const QString wfName = WorkflowManager::instance()->workflow(m_type).name;
  lay->addWidget(new IconWidget(Step_AddProgress, m_isUndo, false), 0);
  lay->addWidget(
      new QLabel(
          tr("The scene will also be sent to the workflow \"%1\".").arg(wfName),
          widget),
      1);
  widget->setLayout(lay);
  return widget;
}

bool StepExecutor_AddProgress::execute(QString& log, QVariant& checkSum) {
  // 工程への追加は、アクション最後のStepExecutor_RegisterProgressがまとめて行う
  return true;
}

QJsonObject StepExecutor_AddProgress::toJson(const QList<QVariant>& params) {
  QJsonObject retObj;
  retObj["WorkflowTypeId"] =
      params.isEmpty() ? 0.0 : (double)params[0].toUInt();
  return retObj;
}

QVariantList StepExecutor_AddProgress::fromJson(const QJsonObject& paramsObj) {
  QVariantList params;
  if (const QJsonValue v = paramsObj["WorkflowTypeId"]; v.isDouble())
    params.append((uint)v.toDouble());
  return params;
}

void applyProgressStepToNextFlows(StepExecutor* executor,
                                  QList<WorkflowType>& nextFlowIds) {
  // ReleaseProgressを挿入することで、このアクションの次工程への移行をキャンセルすることもできるようにする
  if (auto* relProgStep =
          dynamic_cast<StepExecutor_ReleaseProgress*>(executor)) {
    nextFlowIds.removeAll(relProgStep->type());
  }
  // AddProgressを挿入することで、条件を満たす場合に、次工程を追加できる
  else if (auto* addProgStep =
               dynamic_cast<StepExecutor_AddProgress*>(executor)) {
    if (addProgStep->type() != Wf_Undefined &&
        !nextFlowIds.contains(addProgStep->type()))
      nextFlowIds.append(addProgStep->type());
  }
}

StepExecutor_ReleaseProgress::StepExecutor_ReleaseProgress(
    const ActionStep& step, QString sceneNumber, bool isUndo)
    : StepExecutor(isUndo), m_sceneNumber(sceneNumber) {
  m_type = (WorkflowType)step.params[0].toUInt();
}

bool StepExecutor_ReleaseProgress::execute(QString& log, QVariant& checkSum) {
  if (m_isUndo) {
    // 実行時に、実際にtypeから抜いていた場合のみ、typeにもどす
    // （実行時にカットがtypeに入っていなかった場合に、Undoで新たに入れてしまわないため）
    if (m_checkSum.toBool())
      WorkflowManager::instance()->setProgress(m_sceneNumber, m_type, true);
  } else {
    // カットがtypeに入っている場合のみ、typeから抜く。抜いたかどうかをcheckSumに記録する
    const bool released =
        m_type != Wf_Undefined &&
        WorkflowManager::instance()->isInProgress(m_sceneNumber, m_type);
    if (released)
      WorkflowManager::instance()->setProgress(m_sceneNumber, m_type, false);
    checkSum = released;
  }
  return true;
}

QJsonObject StepExecutor_ReleaseProgress::toJson(
    const QList<QVariant>& params) {
  QJsonObject retObj;
  retObj["WorkflowTypeId"] = (double)params[0].toUInt();
  return retObj;
}

QVariantList StepExecutor_ReleaseProgress::fromJson(
    const QJsonObject& paramsObj) {
  QVariantList params;

  if (const QJsonValue v = paramsObj["WorkflowTypeId"]; v.isDouble())
    params.append((uint)v.toDouble());

  return params;
}

//--------------------------------------------------
StepExecutor_CreateEmptyFolder::StepExecutor_CreateEmptyFolder(
    const ActionStep& step, QString sequence, QString sceneNumber, bool isUndo)
    : StepExecutor(isUndo) {
  setPathContext(sequence, sceneNumber);
  m_folderPath = WorkflowManager::instance()->decodePath(
      step.params[0].toString(), sequence, sceneNumber);
  if (m_folderPath.contains("[CURRENT_DATE]"))
    m_folderPath.replace("[CURRENT_DATE]",
                         QDate::currentDate().toString("yyMMdd"));
}

QWidget* StepExecutor_CreateEmptyFolder::createWidget() {
  QWidget* widget = new QWidget();

  QHBoxLayout* lay = new QHBoxLayout();
  lay->setContentsMargins(10, 10, 10, 10);
  lay->setSpacing(10);

  QString txt;
  bool showWarning = false;
  if (m_isUndo) {
    // Execute時にフォルダ作成に成功していればcheckSumにtrueが入っている
    if (m_checkSum.toBool())
      txt =
          tr("The folder %1 will be removed if it is empty.").arg(m_folderPath);
    else {
      txt = tr("Since the folder %1 was not created at execution, this "
               "operation will be skipped on Undo.")
                .arg(m_folderPath);
      showWarning = true;
    }
  } else
    txt = tr("An empty folder %1 will be created.").arg(m_folderPath);

  lay->addWidget(new IconWidget(Step_CreateEmptyFolder, m_isUndo, showWarning),
                 0);

  lay->addWidget(new QLabel(txt, widget), 1);
  widget->setLayout(lay);

  return widget;
}

bool StepExecutor_CreateEmptyFolder::execute(QString& log, QVariant& checkSum) {
  if (m_isUndo) {
    log.append(tr("[UNDO_CREATE EMPTY FOLDER]\n"));
    // もしExecute時にこの操作がスキップされていたら何もしない
    if (!m_checkSum.toBool()) {
      log.append(tr("Skipped as the folder was not created at execution.\n"));
      return true;
    }
    // 操作には、難読化マクロをデコードした実際のパスを使う（ログには、マクロのままのパスを使う）
    const QString folderPathReal = realPath(m_folderPath);
    if (folderPathReal.isEmpty()) {
      log += tr("Ambiguous or empty file path.\n");
      return false;
    }

    // もしフォルダが存在しない、または空でなければ何もしない
    QDir dir(folderPathReal);
    const auto cleanup = dir.exists()
                             ? SharedFiles::removeEmptyFolder(folderPathReal)
                             : SharedFiles::Cleanup::Kept;
    if (cleanup == SharedFiles::Cleanup::Kept) {
      log.append(tr("Skipped as the folder does not exist or is not empty.\n"));
      return true;
    }
    log.append(tr("Removing %1... ").arg(m_folderPath));
    bool ok = cleanup == SharedFiles::Cleanup::Removed;
    if (ok)
      log.append(tr("Done.\n"));
    else
      log.append(tr("Failed.\n"));
    return ok;
  } else {
    log.append(tr("[CREATE EMPTY FOLDER]\n"));
    // 操作には、難読化マクロをデコードした実際のパスを使う（ログには、マクロのままのパスを使う）
    const QString folderPathReal = realPath(m_folderPath);
    if (folderPathReal.isEmpty()) {
      log += tr("Ambiguous or empty file path.\n");
      return false;
    }
    QDir dir(folderPathReal);
    if (dir.exists()) {
      log.append(tr("Skipped as the folder already exists.\n"));
      checkSum = false;
      return true;
    }
    bool ok = QDir("").mkpath(folderPathReal);
    if (ok)
      log.append(tr("Done.\n"));
    else
      log.append(tr("Failed.\n"));
    checkSum = ok;
    return true;
  }
}

QJsonObject StepExecutor_CreateEmptyFolder::toJson(
    const QList<QVariant>& params) {
  QJsonObject retObj;
  retObj["FolderPath"] = params[0].toString();
  return retObj;
}

QVariantList StepExecutor_CreateEmptyFolder::fromJson(
    const QJsonObject& paramsObj) {
  QVariantList params;

  if (const QJsonValue v = paramsObj["FolderPath"]; v.isString())
    params.append(v.toString());

  return params;
}

//--------------------------------------------------

DougaRetakeWidget::DougaRetakeWidget(const QString sceneNumber,
                                     const QString orgPath, QWidget* parent)
    : QWidget(parent), m_sceneNumber(sceneNumber) {
  QVariantMap retakeMap =
      WorkflowManager::instance()->obtainRetakeInfo(sceneNumber);
  QString comment = retakeMap.value("Comment").toString();

  m_checkButtonGroup           = new QButtonGroup(this);
  m_cueFileLabel               = new QLabel(this);
  QPushButton* mainCheckButton = new QPushButton(tr("Main Check"), this);
  QPushButton* doukenButton    = new QPushButton(tr("Douga Kensa"), this);
  // QPushButton* enjoCheckButton = new QPushButton(tr("Enjo Check"), this);
  m_checkButtonGroup->setExclusive(false);
  m_checkButtonGroup->addButton(mainCheckButton, MainCheck);
  m_checkButtonGroup->addButton(doukenButton, DougaKensa);
  // m_checkButtonGroup->addButton(enjoCheckButton, EnjoCheck);
  for (auto button : m_checkButtonGroup->buttons()) {
    button->setCheckable(true);
    button->setChecked(true);
  }

  QVBoxLayout* layout = new QVBoxLayout();
  layout->setContentsMargins(10, 10, 10, 10);
  layout->setSpacing(10);

  QHBoxLayout* commentLay = new QHBoxLayout();
  commentLay->setContentsMargins(0, 0, 0, 0);
  commentLay->setSpacing(10);
  {
    commentLay->addWidget(new QLabel(tr("Comment:"), this), 0);
    commentLay->addWidget(new QLabel(comment, this), 1);
  }
  layout->addLayout(commentLay, 0);

  // 「動画リテイク作業後に行うチェック工程を選択してください」
  layout->addWidget(new QLabel(tr("Please select the checking process to be "
                                  "performed after the douga retake work."),
                               this),
                    0);

  QHBoxLayout* chkButtonsLay = new QHBoxLayout();
  chkButtonsLay->setContentsMargins(0, 0, 0, 0);
  chkButtonsLay->setSpacing(2);
  {
    chkButtonsLay->addWidget(new QLabel(tr("Douga Retake -> "), this), 0);
    chkButtonsLay->addWidget(mainCheckButton, 1);
    chkButtonsLay->addWidget(new QLabel(tr(" -> "), this), 0);
    chkButtonsLay->addWidget(doukenButton, 1);
    chkButtonsLay->addWidget(new QLabel(tr(" -> Enjo Check -> Shiage"), this),
                             0);
    // chkButtonsLay->addWidget(enjoCheckButton, 1);
    // chkButtonsLay->addWidget(new QLabel(tr(" -> Shiage"), this), 0);
  }
  layout->addLayout(chkButtonsLay, 0);

  QHBoxLayout* lay = new QHBoxLayout();
  lay->addWidget(new IconWidget(Step_Rename, false), 0);

  QString shortenOrgPath = QDir(WorkflowManager::instance()->projectRootPath())
                               .relativeFilePath(orgPath);

  QString txt;
  txt = tr("%1 will be renamed to the following path:").arg(shortenOrgPath);
  QVBoxLayout* pathLay = new QVBoxLayout();
  pathLay->setContentsMargins(0, 0, 0, 0);
  pathLay->setSpacing(5);
  {
    pathLay->addWidget(new QLabel(txt, this), 0);
    pathLay->addWidget(m_cueFileLabel, 0);
  }
  lay->addLayout(pathLay, 1);

  layout->addLayout(lay, 0);

  setLayout(layout);

  connect(m_checkButtonGroup, SIGNAL(idToggled(int, bool)), this,
          SLOT(onCheckChanged()));
  onCheckChanged();
}

QString DougaRetakeWidget::getCueFileName() {
  QString ret =
      WorkflowManager::instance()->cutNumberPrefix() + m_sceneNumber + "_douga";
  // 演助チェックは必ず通る
  // if (m_checkButtonGroup->button(EnjoCheck)->isChecked())
  ret += "_ej";
  if (m_checkButtonGroup->button(DougaKensa)->isChecked()) ret += "_dk";
  if (m_checkButtonGroup->button(MainCheck)->isChecked()) ret += "_ch";
  ret += "_d.txt";
  return ret;
}

void DougaRetakeWidget::onCheckChanged() {
  m_cueFileLabel->setText(" - " + getCueFileName());
}

//----

StepExecutor_DougaRetakeRouting::StepExecutor_DougaRetakeRouting(
    const ActionStep& step, QString orgPath, QString sequence,
    QString sceneNumber, bool isUndo)
    : StepExecutor(isUndo)
    , m_orgPath(orgPath)
    , m_sequence(sequence)
    , m_sceneNumber(sceneNumber)
    , m_retakeWidget(nullptr) {}

QWidget* StepExecutor_DougaRetakeRouting::createWidget() {
  if (m_isUndo) {
    QWidget* widget      = new QWidget();
    QStringList pathList = m_checkSum.toStringList();

    QString txt = tr("The file %1 will be removed\n").arg(pathList[1]);
    txt += tr("\nand\n%1will be reverted.").arg(pathList[0]);

    QHBoxLayout* lay = new QHBoxLayout();
    lay->setContentsMargins(10, 10, 10, 10);
    lay->setSpacing(10);
    lay->addWidget(new IconWidget(Step_Rename, true), 0);
    lay->addWidget(new QLabel(txt, widget));
    widget->setLayout(lay);

    return widget;
  }

  m_retakeWidget = new DougaRetakeWidget(m_sceneNumber, m_orgPath);
  return m_retakeWidget;
}

bool StepExecutor_DougaRetakeRouting::execute(QString& log,
                                              QVariant& checkSum) {
  if (!m_isUndo) {
    log.append(tr("[DOUGA_RETAKE]\nRename From: %1\n").arg(hide(m_orgPath)));

    QString cueFileName = m_retakeWidget->getCueFileName();
    QString renameToPath =
        QFileInfo(m_orgPath).dir().absoluteFilePath(cueFileName);
    log.append(tr("To: %1\n").arg(cueFileName));

    bool ok = true;
    renameToPath = SharedFiles::actualPath(renameToPath);
    if (renameToPath.isEmpty() || QFileInfo::exists(renameToPath)) {
      log += tr("Ambiguous or occupied destination.\n");
      return false;
    }
    {
      log.append(tr("  Copy file: "));
      ok = createPathAndCopyFile(m_orgPath, renameToPath, log);
    }
    // remove original file
    log.append(tr("  Remove original file: "));
    if (ok) {
      ok = QFile::remove(m_orgPath);
      if (!ok)
        log.append(tr("Failed.\n"));
      else
        log.append(tr("Done.\n"));
    }

    QStringList pathList = {m_orgPath, renameToPath};
    checkSum             = pathList;

    return ok;
  } else {  // Undo case
    QStringList pathList = m_checkSum.toStringList();

    log.append(tr("[UNDO_DOUGA_RETAKE]\nRename From: %1\nTo: %2\n")
                   .arg(pathList[1])
                   .arg(pathList[0]));
    pathList[0] = SharedFiles::actualPath(pathList[0]);
    if (pathList[0].isEmpty() || QFileInfo::exists(pathList[0])) {
      log += tr("Ambiguous or occupied destination.\n");
      return false;
    }
    {
      log.append(tr("  Copy file: "));
      if (!createPathAndCopyFile(pathList[1], pathList[0], log)) {
        log.append(tr("Failed.\n"));
        return false;
      } else
        log.append(tr("Done.\n"));
    }
    log.append(tr("  Remove renamed file: "));

    bool ok = QFile::remove(pathList[1]);
    if (!ok)
      log.append(tr("Failed.\n"));
    else
      log.append(tr("Done.\n"));
    return ok;
  }
}

//------------------------------------------

StepExecutor_ZipFolder::StepExecutor_ZipFolder(const ActionStep& step,
                                               QString orgPath,
                                               QString sequence,
                                               QString sceneNumber, bool isUndo)
    : StepExecutor(isUndo) {
  setPathContext(sequence, sceneNumber);
  m_orgFolderPath = step.params[0].toString();
  if (m_orgFolderPath.isEmpty()) {
    m_orgFolderPath = QFileInfo(orgPath).absoluteDir().path();
  } else
    m_orgFolderPath = WorkflowManager::instance()->decodePath(
        m_orgFolderPath, sequence, sceneNumber);

  m_zipFilePath = step.params[1].toString();
  m_zipFilePath = WorkflowManager::instance()->decodePath(
      m_zipFilePath, sequence, sceneNumber);
}

QWidget* StepExecutor_ZipFolder::createWidget() {
  QWidget* widget = new QWidget();

  QString txt;
  if (!m_isUndo) {
    txt = tr("%1\n\nwill be ZIPed to the following path:\n\n%2")
              .arg(hide(m_orgFolderPath))
              .arg(m_zipFilePath);
  } else {  // undo case
    txt = tr("The following file will be removed:\n\n%1").arg(m_zipFilePath);
  }

  QHBoxLayout* lay = new QHBoxLayout();
  lay->setContentsMargins(10, 10, 10, 10);
  lay->setSpacing(10);
  lay->addWidget(new IconWidget(Step_ZipFolder, m_isUndo), 0);
  lay->addWidget(new QLabel(txt, widget), 1);
  widget->setLayout(lay);

  return widget;
}

bool StepExecutor_ZipFolder::execute(QString& log, QVariant& checkSum) {
  if (!m_isUndo) {
    log.append(tr("[ZIP FOLDER]\nFrom: %1\nTo: %2")
                   .arg(hide(m_orgFolderPath))
                   .arg(m_zipFilePath));

    // 操作に使うここで、難読化マクロをデコードして、実際のパスにする（画面・ログには、マクロのままのパスを使う）
    QString fromPath = realPath(m_orgFolderPath);
    if (fromPath.isEmpty()) {
      log += tr("Ambiguous or empty file path.\n");
      return false;
    }

    QString toPath = realPath(m_zipFilePath);
    if (toPath.isEmpty()) {
      log += tr("Ambiguous or empty file path.\n");
      return false;
    }

    bool ok = true;
    // フォルダ作成
    QFileInfo fi(toPath);
    // 上書き確認ダイアログ
    if (fi.exists()) {
      QMessageBox::StandardButton ret = QMessageBox::question(
          nullptr, tr("Question"),
          tr("The file %1 already exists.\nDo you want to overwrite it?")
              .arg(m_zipFilePath),
          {QMessageBox::Yes | QMessageBox::Cancel}, QMessageBox::Yes);
      if (ret == QMessageBox::Cancel) {
        log.append(tr("Canceled to overwrite file %1.\n").arg(m_zipFilePath));
        return false;
      }
    }
    // フォルダ作成確認ダイアログ
    else if (!fi.dir().exists()) {
      QMessageBox::StandardButton ret = QMessageBox::question(
          nullptr, tr("Question"),
          tr("The folder %1 does not exist.\nDo you want to create it?")
              .arg(hide(fi.dir().absolutePath())),
          {QMessageBox::Yes | QMessageBox::Cancel}, QMessageBox::Yes);
      if (ret == QMessageBox::Cancel) {
        log.append(tr("Canceled to create folder %1.\n")
                       .arg(hide(fi.dir().absolutePath())));
        return false;
      }

      ok = fi.dir().mkpath(".");
      if (!ok) {
        log.append(tr("Failed to create folder %1.\n")
                       .arg(hide(fi.dir().absolutePath())));
        return false;
      }
    }

    ok = Platform::zipFolder(fromPath, toPath);

    checkSum = m_zipFilePath;
    if (ok)
      log.append(tr("Done.\n"));
    else
      log.append(tr("Failed.\n"));
    return ok;
  }

  // undo case
  log.append(tr("[UNDO ZIP FOLDER]\n"));
  log.append(tr(" Start removing file %1...").arg(m_zipFilePath));
  // 操作に使うここで、難読化マクロをデコードして、実際のパスにする（ログには、マクロのままのパスを使う）
  bool ok = QFile::remove(realPath(m_zipFilePath));
  if (ok)
    log.append(tr("Done.\n"));
  else {
    log.append(tr("Failed.\n"));
  }

  return ok;
}

QJsonObject StepExecutor_ZipFolder::toJson(const QList<QVariant>& params) {
  QJsonObject retObj;
  retObj["FolderPath"]  = params[0].toString();
  retObj["ZipFilePath"] = params[1].toString();
  return retObj;
}

QVariantList StepExecutor_ZipFolder::fromJson(const QJsonObject& paramsObj) {
  QVariantList params;

  if (const QJsonValue v = paramsObj["FolderPath"]; v.isString())
    params.append(v.toString());

  if (const QJsonValue v = paramsObj["ZipFilePath"]; v.isString())
    params.append(v.toString());

  return params;
}

//--------------------------------------------------

QString StepExecutor_NotifyGoogleChat::defaultMessageTemplate(
    bool hasEpisodes) {
  // 話数機能を使わないプロジェクトでは、Episodeの項目自体を省く
  return tr("The following action has completed.\n"
            "Project:[PROJECT_NAME]\n") +
         (hasEpisodes ? tr("Episode:[EPISODE] / Scene:[CUTNUMBER]\n")
                      : tr("Scene:[CUTNUMBER]\n")) +
         tr("DateTime:[CURRENT_DATETIME]\n"
            "Workflow:[WORKFLOW_NAME]\n"
            "Action:[ACTION_NAME]\n"
            "User:[USER_NAME]");
}

StepExecutor_NotifyGoogleChat::StepExecutor_NotifyGoogleChat(
    const ActionStep& step, QString sequence, QString sceneNumber, bool isUndo,
    QString workflowName, QString actionName)
    : StepExecutor(isUndo) {
  // 通知メッセージ・キャンセルメッセージ・スレッドキーには、パス系マクロ（[CUTNUMBER]等）と
  // [USER_NAME]、[CURRENT_DATE]に加え、このステップ専用の[PROJECT_NAME]、[CURRENT_DATETIME]、
  // [WORKFLOW_NAME]、[ACTION_NAME]を使える
  auto decodeMacros = [&](const QString& text) {
    QString decoded = WorkflowManager::instance()->decodePath(
        text, sequence, sceneNumber, {}, false);
    QDateTime now = QDateTime::currentDateTime();
    decoded.replace("[CURRENT_DATE]", now.toString("yyyy/MM/dd"));
    decoded.replace("[CURRENT_DATETIME]",
                    now.toString(tr("yyyy MM/dd hh:mm:ss")));
    decoded.replace("[PROJECT_NAME]",
                    WorkflowManager::instance()->projectTitle());
    decoded.replace("[WORKFLOW_NAME]", workflowName);
    decoded.replace("[ACTION_NAME]", actionName);
    return decoded;
  };

  m_destinationName = step.params.value(0).toString();
  m_isDestinationRegistered =
      WorkflowManager::instance()->hasChatDestination(m_destinationName);
  m_hasGasUrl = !WorkflowManager::instance()->gasUrl().isEmpty();
  // Messageが空欄（空白のみを含む）の場合は、既定のメッセージを使う
  QString messageTemplate = step.params.value(1).toString();
  if (messageTemplate.trimmed().isEmpty())
    messageTemplate =
        defaultMessageTemplate(WorkflowManager::instance()->hasEpisodes());
  m_message       = decodeMacros(messageTemplate);
  m_allowFreeText = step.params.value(2).toBool();
  m_notifyOnCancel =
      (step.params.size() > 3) ? step.params.at(3).toBool() : true;
  m_cancelMessage = decodeMacros(step.params.value(4).toString());
  m_threadKey     = decodeMacros(step.params.value(5).toString()).trimmed();
}

QString StepExecutor_NotifyGoogleChat::buildOutgoingText() const {
  if (!m_isUndo) {
    QString text     = m_message;
    QString freeText = m_freeText.trimmed();
    if (!freeText.isEmpty()) text += (text.isEmpty() ? "" : "\n\n") + freeText;
    return text;
  }

  // Undo：元の通知が送られていない（飛ばされた）場合は、キャンセル通知も送らない
  QString originalText = m_checkSum.toString();
  if (originalText.isEmpty()) return QString();
  if (!m_cancelMessage.isEmpty()) return m_cancelMessage;
  // キャンセルメッセージが省略されている場合は、元の通知（自由記入欄の内容を含む）を再掲する
  return tr("[The following operation was canceled]") + "\n" + originalText;
}

QString StepExecutor_NotifyGoogleChat::skipReasonText() const {
  if (!m_hasGasUrl)
    // プロジェクトにGoogle Apps
    // ScriptのURLが設定されていないため、このステップは飛ばされます。
    return tr(
        "This step will be skipped because the project does not have a Google "
        "Apps Script URL setting.");
  if (!m_isDestinationRegistered)
    // プロジェクト設定にこの投稿先が登録されていないため、このステップは飛ばされます。
    return tr("This step will be skipped because the Google Chat destination "
              "\"%1\" "
              "is not registered in the project settings.")
        .arg(m_destinationName);
  return QString();
}

QWidget* StepExecutor_NotifyGoogleChat::createWidget() {
  bool skipped = false;
  QString headerTxt;
  QString previewTxt;
  if (!isAvailable()) {
    skipped   = true;
    headerTxt = skipReasonText();
  } else if (m_isUndo) {
    previewTxt = buildOutgoingText();
    if (!m_notifyOnCancel || previewTxt.isEmpty())
      headerTxt = tr("No cancel notification will be sent to Google Chat.");
    else
      headerTxt = tr("The following cancel notification will be sent to the "
                     "Google Chat destination \"%1\":")
                      .arg(m_destinationName);
  } else {
    previewTxt = buildOutgoingText();
    headerTxt  = tr("The following message will be sent to the Google Chat "
                     "destination \"%1\":")
                    .arg(m_destinationName);
  }
  bool showPreview =
      !skipped && !previewTxt.isEmpty() && (!m_isUndo || m_notifyOnCancel);

  QWidget* widget = new QWidget();

  QVBoxLayout* contentLay = new QVBoxLayout();
  contentLay->setContentsMargins(0, 0, 0, 0);
  contentLay->setSpacing(6);

  QLabel* headerLabel = new QLabel(headerTxt, widget);
  headerLabel->setWordWrap(true);
  contentLay->addWidget(headerLabel);

  QLabel* previewLabel = nullptr;
  if (showPreview) {
    previewLabel = new QLabel(previewTxt, widget);
    previewLabel->setWordWrap(true);
    previewLabel->setFrameShape(QFrame::StyledPanel);
    previewLabel->setMargin(6);
    previewLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    contentLay->addWidget(previewLabel);
  }

  if (!skipped && !m_isUndo && m_allowFreeText) {
    // 自由記入欄：入力した内容は、通知メッセージの後ろに追記して送られる
    contentLay->addWidget(
        new QLabel(tr("Additional message (optional):"), widget));
    QPlainTextEdit* freeTextEdit = new QPlainTextEdit(widget);
    freeTextEdit->setFixedHeight(70);
    freeTextEdit->setPlainText(m_freeText);
    contentLay->addWidget(freeTextEdit);
    QObject::connect(freeTextEdit, &QPlainTextEdit::textChanged, freeTextEdit,
                     [this, freeTextEdit, previewLabel]() {
                       m_freeText = freeTextEdit->toPlainText();
                       if (previewLabel)
                         previewLabel->setText(buildOutgoingText());
                     });
  }

  if (!skipped && (!m_isUndo || m_notifyOnCancel))
    // 完了ボタンを押した後、このステップは非同期に送信されます。
    contentLay->addWidget(new QLabel(tr("N.B. After pressing the Done button, "
                                        "the message is sent asynchronously."),
                                     widget));

  QHBoxLayout* lay = new QHBoxLayout();
  lay->setContentsMargins(10, 10, 10, 10);
  lay->setSpacing(10);
  lay->addWidget(new IconWidget(Step_NotifyGoogleChat, m_isUndo, skipped), 0);
  lay->addLayout(contentLay, 1);
  widget->setLayout(lay);
  return widget;
}

bool StepExecutor_NotifyGoogleChat::execute(QString& log, QVariant& checkSum) {
  log.append(m_isUndo ? tr("[UNDO_NOTIFY_GOOGLE_CHAT]\n")
                      : tr("[NOTIFY_GOOGLE_CHAT]\n"));

  if (!m_hasGasUrl) {
    log.append(tr(" (Skipped - No Google Apps Script URL)\n"));
    return true;
  }
  if (!m_isDestinationRegistered) {
    log.append(tr(" (Skipped - Destination \"%1\" is not registered)\n")
                   .arg(m_destinationName));
    return true;
  }
  if (m_isUndo && !m_notifyOnCancel) {
    log.append(tr(" (Skipped - Cancel notification is turned off)\n"));
    return true;
  }

  QString text = buildOutgoingText();
  if (text.trimmed().isEmpty()) {
    log.append(tr(" (Skipped - Nothing to send)\n"));
    return true;
  }

  // 送信は非同期。失敗した場合は、GoogleChatGatewayが警告ダイアログを表示する
  GoogleChatGateway::instance()->post(WorkflowManager::instance()->gasUrl(),
                                      m_destinationName, text, m_threadKey);
  // Undo時にキャンセル通知の元の本文として使う
  if (!m_isUndo) checkSum = text;
  log.append(tr("Done.\n"));
  return true;
}

QJsonObject StepExecutor_NotifyGoogleChat::toJson(
    const QList<QVariant>& params) {
  QJsonObject retObj;
  retObj["Destination"]    = params.value(0).toString();
  retObj["Message"]        = params.value(1).toString();
  retObj["AllowFreeText"]  = params.value(2).toBool();
  retObj["NotifyOnCancel"] = (params.size() > 3) ? params.at(3).toBool() : true;
  retObj["CancelMessage"]  = params.value(4).toString();
  retObj["ThreadKey"]      = params.value(5).toString();
  return retObj;
}

QVariantList StepExecutor_NotifyGoogleChat::fromJson(
    const QJsonObject& paramsObj) {
  // 欠けているキーは既定値で補い、常に6個のパラメータを返す
  QVariantList params;
  params.append(paramsObj["Destination"].toString());
  params.append(paramsObj["Message"].toString());
  params.append(paramsObj["AllowFreeText"].toBool(false));
  params.append(paramsObj["NotifyOnCancel"].toBool(true));
  params.append(paramsObj["CancelMessage"].toString());
  params.append(paramsObj["ThreadKey"].toString());
  return params;
}

//--------------------------------------------------

ConfirmationDialog::ConfirmationDialog(QList<ActionInfo> actionInfos,
                                       bool isUndo)
    : m_actionInfos(actionInfos), m_isUndo(isUndo), m_isCanceled(true) {
  setWindowTitle(isUndo ? tr("Confirm Undo") : tr("Confirm Action"));
  // m_googleStepPosの各アクション分の要素は、派生クラスのコンストラクタがappendする
}

ConfirmationDialog::ConfirmationDialog(ActionInfo actionInfo, bool isUndo)
    : m_isUndo(isUndo), m_isCanceled(true) {
  setWindowTitle(isUndo ? tr("Confirm Undo") : tr("Confirm Action"));
  m_actionInfos.append(actionInfo);
  // m_googleStepPosの要素は、派生クラスのコンストラクタがappendする
}

ConfirmationDialog::~ConfirmationDialog() {
  for (auto executorList : m_stepExecutors)
    for (auto executor : executorList) delete executor;
}

//--------------------------------------------------

ActionConfirmationDialog::ActionConfirmationDialog(ActionInfo actionInfo,
                                                   int actionId, bool isUndo,
                                                   QList<QVariant> checkSums)
    : ConfirmationDialog(actionInfo, isUndo) {
  m_googleStepPos.append(QList<int>{});

  WorkflowType workflowType = actionInfo.workflowType;
  int protocolId            = actionInfo.protocolId;
  QStringList orgPaths      = actionInfo.orgPaths;
  QString sequence          = actionInfo.sequence;
  QString sceneNumber       = actionInfo.sceneNumber;

  // 話数機能ON時、このActionInfoが所属する話数へ切り替えてからパス解決を行う
  WorkflowManager::instance()->setCurrentEpisode(actionInfo.episode);

  QPushButton* okButton     = new QPushButton(tr("OK"), this);
  QPushButton* cancelButton = new QPushButton(tr("Cancel"), this);
  Workflow wf = WorkflowManager::instance()->workflow(workflowType);
  WorkflowProtocol protocol = wf.protocols.at(protocolId);
  QString actionName        = protocol.actions[actionId].actionName;

  QList<StepExecutor*> stepExecutorList;
  QList<WorkflowType> nextFlowIds = protocol.actions[actionId].nextFlows;
  for (auto step : protocol.actions[actionId].steps) {
    StepExecutor* executor =
        createExecutor(wf.groupId, step, orgPaths, sequence, sceneNumber,
                       isUndo, 0, wf.name, actionName);
    if (!executor) continue;
    if (executor->isGoogleStep()) {
      // ひとつのアクションに複数のGoogleスプレッドシート入力ステップがあり得るため、
      // 上書きせずリストに追加する。各ステップにも自身の位置を教え、非同期の完了通知を
      // 区別できるようにする
      int pos = stepExecutorList.count();
      m_googleStepPos[0].append(pos);
      if (auto* googleExecutor =
              dynamic_cast<StepExecutor_InputGoogleSpreadsheet*>(executor))
        googleExecutor->setGoogleStepIndex(pos);
    }

    // ReleaseProgress/AddProgressで、このアクションの次工程を、取り消したり追加したりできる
    applyProgressStepToNextFlows(executor, nextFlowIds);

    if (isUndo) {  // 逆順に実行する
      executor->setCheckSum(checkSums.takeFirst());
      stepExecutorList.push_front(executor);
    } else
      stepExecutorList.append(executor);
  }
  // 最後に、詳細進捗リストを更新するExecutorを追加する（ReleaseProgressステップでキャンセル可）
  if (isUndo) {
    StepExecutor_RegisterProgress* registerProgress =
        new StepExecutor_RegisterProgress(sceneNumber, wf.workflowType,
                                          nextFlowIds, isUndo);
    // 実行時に実際に登録した次工程の記録を探して渡す（Undo時の条件分岐の再評価結果には頼らない）
    for (int i = 0; i < checkSums.size(); i++) {
      QList<WorkflowType> dummy;
      if (StepExecutor_RegisterProgress::decodeToTypes(checkSums.at(i),
                                                       dummy)) {
        registerProgress->setCheckSum(checkSums.takeAt(i));
        break;
      }
    }
    stepExecutorList.push_front(registerProgress);
  } else
    stepExecutorList.append(new StepExecutor_RegisterProgress(
        sceneNumber, wf.workflowType, nextFlowIds, isUndo));

  // Rewind明けにRewind情報を消す、のUndo
  if (isUndo && !checkSums.isEmpty() &&
      checkSums.last().canConvert(QMetaType::QVariantList)) {
    StepExecutor* executor = new StepExecutor_SetSceneInfo(
        {Step_SetSceneInfo, {"REWIND", QVariant()}}, sequence, sceneNumber,
        isUndo);
    executor->setCheckSum(checkSums.takeLast());
    stepExecutorList.push_front(executor);
  }
  // Rewind明けにカット情報REWINDをWIPからBackOnTrackにする
  // 次の工程グループ移動時にカット情報REWINDを解除する
  else if (!isUndo) {
    QString RewindState =
        WorkflowManager::instance()
            ->sceneInfoTag(sequence, sceneNumber, "REWIND", "")
            .toString();
    if ((RewindState == "WIP" || RewindState == "BackOnTrack") &&
        wf.clearsRewindOnComplete)
      stepExecutorList.append(new StepExecutor_SetSceneInfo(
          {Step_SetSceneInfo, {"REWIND", QVariant()}}, sequence, sceneNumber,
          isUndo));
    else if (RewindState == "WIP")
      stepExecutorList.append(new StepExecutor_SetSceneInfo(
          {Step_SetSceneInfo, {"REWIND", "BackOnTrack"}}, sequence, sceneNumber,
          isUndo));
  }

  m_stepExecutors.append(stepExecutorList);

  setStyleSheet(
      "QLabel{font-size: 10pt;}\nQRadioButton{font-size: "
      "10pt;}\nQLabel#LargeSizedLabel{font-size: 16pt;}");

  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setContentsMargins(10, 10, 10, 10);
  mainLay->setSpacing(5);
  {
    QString titleString = tr("Scene : %1").arg(sceneNumber);
    // テイクを表示
    if (WorkflowManager::instance()
            ->flowGroupInfos()
            .value(wf.groupId)
            .isRetakeGroup)
      titleString.append(
          WorkflowManager::instance()->currentTakeSuffix(sceneNumber));

    titleString.append(
        tr("  Workflow: %1\nAction : %2").arg(wf.name).arg(actionName));
    if (isUndo) titleString.push_front(tr("[Undoing Action]\n"));
    QLabel* titleLabel = new QLabel(titleString, this);
    titleLabel->setObjectName("LargeSizedLabel");
    mainLay->addWidget(titleLabel, 0);

    QVBoxLayout* stepLay    = mainLay;
    QWidget* stepWidget     = nullptr;
    QScrollArea* scrollArea = nullptr;
    if (stepExecutorList.size() >= 5) {
      stepWidget = new QWidget(this);
      stepLay    = new QVBoxLayout();
      stepLay->setContentsMargins(0, 0, 0, 0);
      stepLay->setSpacing(5);
      stepWidget->setLayout(stepLay);
      scrollArea = new QScrollArea(this);
      scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
      scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
      mainLay->addWidget(scrollArea, 0);
    }

    for (auto executor : stepExecutorList) {
      if (!executor->hasUI()) continue;
      QFrame* line = new QFrame(this);
      line->setFrameShape(QFrame::HLine);
      line->setFrameShadow(QFrame::Sunken);
      stepLay->addWidget(line, 0);
      stepLay->addWidget(executor->createWidget(), 0);
    }

    if (scrollArea) {
      stepLay->addStretch(1);
      scrollArea->setWidget(stepWidget);
    }

    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLay->addWidget(line, 0);

    mainLay->addWidget(new QLabel(tr("Are you sure ?")), 0);

    QFrame* solidLine = new QFrame(this);
    solidLine->setFrameShape(QFrame::HLine);
    solidLine->setFrameShadow(QFrame::Plain);
    mainLay->addWidget(solidLine, 0);

    QHBoxLayout* buttonsLay = new QHBoxLayout();
    buttonsLay->setContentsMargins(10, 10, 10, 10);
    buttonsLay->setSpacing(20);
    {
      buttonsLay->addStretch(1);
      buttonsLay->addWidget(okButton, 0);
      buttonsLay->addWidget(cancelButton, 0);
      buttonsLay->addStretch(1);
    }
    mainLay->addLayout(buttonsLay);
  }
  setLayout(mainLay);

  if (m_invalidConditionPath) {
    okButton->setEnabled(false);
    mainLay->addWidget(new QLabel(
        tr("The condition path is ambiguous or unavailable."), this));
  }
  connect(okButton, SIGNAL(clicked()), this, SLOT(onOkClicked()));
  connect(cancelButton, &QPushButton::clicked, [&]() {
    m_isCanceled = true;
    accept();
  });
}

ActionConfirmationDialog::ActionConfirmationDialog(
    QList<ActionInfo> actionInfos)
    : ConfirmationDialog(actionInfos, false) {
  QPushButton* okButton     = new QPushButton(tr("OK"), this);
  QPushButton* cancelButton = new QPushButton(tr("Cancel"), this);
  setStyleSheet(
      "QLabel{font-size: 10pt;}\nQRadioButton{font-size: "
      "10pt;}\nQLabel#LargeSizedLabel{font-size: 16pt;}");

  QTabWidget* tabWidget = new QTabWidget(this);
  int index             = 0;
  for (auto actionInfo : actionInfos) {
    WorkflowType workflowType = actionInfo.workflowType;
    int protocolId            = actionInfo.protocolId;
    QStringList orgPaths      = actionInfo.orgPaths;
    QString sequence          = actionInfo.sequence;
    QString sceneNumber       = actionInfo.sceneNumber;
    m_googleStepPos.append(QList<int>{});

    // 話数機能ON時、このActionInfoが所属する話数へ切り替えてからパス解決を行う。
    // 複数話数にまたがる一括操作でも、各StepExecutorはこのタイミングで
    // WorkflowManagerから正しい話数のパスを取得できる
    WorkflowManager::instance()->setCurrentEpisode(actionInfo.episode);

    Workflow wf = WorkflowManager::instance()->workflow(workflowType);
    WorkflowProtocol protocol = wf.protocols.at(protocolId);
    // まとめて実行時はactionId=0を実行する
    QString actionName = protocol.actions[0].actionName;

    QList<StepExecutor*> stepExecutorList;
    QList<WorkflowType> nextFlowIds = protocol.actions[0].nextFlows;
    for (auto step : protocol.actions[0].steps) {
      StepExecutor* executor =
          createExecutor(wf.groupId, step, orgPaths, sequence, sceneNumber,
                         false, index, wf.name, actionName);
      if (!executor) continue;
      if (executor->isGoogleStep()) {
        // ひとつのアクションに複数のGoogleスプレッドシート入力ステップがあり得るため、
        // 上書きせずリストに追加する。各ステップにも自身の位置を教え、非同期の完了通知を
        // 区別できるようにする
        int pos = stepExecutorList.count();
        m_googleStepPos[index].append(pos);
        if (auto* googleExecutor =
                dynamic_cast<StepExecutor_InputGoogleSpreadsheet*>(executor))
          googleExecutor->setGoogleStepIndex(pos);
      }

      // ReleaseProgress/AddProgressで、このアクションの次工程を、取り消したり追加したりできる
      applyProgressStepToNextFlows(executor, nextFlowIds);

      stepExecutorList.append(executor);
    }
    // 最後に、詳細進捗リストを更新するExecutorを追加する（ReleaseProgressステップでキャンセル可）
    stepExecutorList.append(new StepExecutor_RegisterProgress(
        sceneNumber, wf.workflowType, nextFlowIds, false));

    // Rewind復帰（1カットずつ実行する場合と同じ条件にする）
    // グループ内最後の工程では、作業中・チェック中のどちらでもカット情報REWINDを解除する
    QString RewindState =
        WorkflowManager::instance()
            ->sceneInfoTag(sequence, sceneNumber, "REWIND", "")
            .toString();
    if ((RewindState == "WIP" || RewindState == "BackOnTrack") &&
        wf.clearsRewindOnComplete)
      stepExecutorList.append(new StepExecutor_SetSceneInfo(
          {Step_SetSceneInfo, {"REWIND", QVariant()}}, sequence, sceneNumber,
          false));
    else if (RewindState == "WIP")
      stepExecutorList.append(new StepExecutor_SetSceneInfo(
          {Step_SetSceneInfo, {"REWIND", "BackOnTrack"}}, sequence, sceneNumber,
          false));

    m_stepExecutors.append(stepExecutorList);

    QWidget* pageWidget = new QWidget(this);

    QVBoxLayout* pageLay = new QVBoxLayout();
    pageLay->setContentsMargins(10, 10, 10, 10);
    pageLay->setSpacing(5);
    {
      QString titleString = tr("Scene : %1").arg(sceneNumber);
      titleString.append(
          tr("  Workflow: %1\nAction : %2").arg(wf.name).arg(actionName));
      QLabel* titleLabel = new QLabel(titleString, this);
      titleLabel->setObjectName("LargeSizedLabel");
      pageLay->addWidget(titleLabel, 0);

      for (auto executor : stepExecutorList) {
        QFrame* line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        pageLay->addWidget(line, 0);
        pageLay->addWidget(executor->createWidget(), 0);
      }
      QFrame* line = new QFrame(this);
      line->setFrameShape(QFrame::HLine);
      line->setFrameShadow(QFrame::Sunken);
      pageLay->addWidget(line, 0);

      pageLay->addStretch(1);
    }
    pageWidget->setLayout(pageLay);

    tabWidget->addTab(pageWidget, sceneNumber);

    index++;
  }

  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setContentsMargins(10, 10, 10, 10);
  mainLay->setSpacing(5);
  {
    mainLay->addWidget(tabWidget, 0);
    mainLay->addWidget(new QLabel(tr("Are you sure ?")), 0);

    QFrame* solidLine = new QFrame(this);
    solidLine->setFrameShape(QFrame::HLine);
    solidLine->setFrameShadow(QFrame::Plain);
    mainLay->addWidget(solidLine, 0);

    QHBoxLayout* buttonsLay = new QHBoxLayout();
    buttonsLay->setContentsMargins(10, 10, 10, 10);
    buttonsLay->setSpacing(20);
    {
      buttonsLay->addStretch(1);
      buttonsLay->addWidget(okButton, 0);
      buttonsLay->addWidget(cancelButton, 0);
      buttonsLay->addStretch(1);
    }
    mainLay->addLayout(buttonsLay);
  }
  setLayout(mainLay);

  if (m_invalidConditionPath) {
    okButton->setEnabled(false);
    mainLay->addWidget(new QLabel(
        tr("The condition path is ambiguous or unavailable."), this));
  }
  connect(okButton, SIGNAL(clicked()), this, SLOT(onOkClicked()));
  connect(cancelButton, &QPushButton::clicked, [&]() {
    m_isCanceled = true;
    accept();
  });
}
//--------------------------------------------------

StepExecutor* ConfirmationDialog::createExecutor(
    const FlowGroupId groupId, const ActionStep& step,
    const QStringList& orgPaths, const QString& sequence,
    const QString& sceneNumber, const bool isUndo, int index,
    const QString& workflowName, const QString& actionName) {
  StepExecutor* executor = nullptr;
  QString orgPath        = orgPaths[0];
  switch (step.type) {
  case Step_Rename:
    executor =
        new StepExecutor_Rename(step, orgPaths, sequence, sceneNumber, isUndo);
    break;
  case Step_Copy:
    executor =
        new StepExecutor_Copy(step, orgPath, sequence, sceneNumber, isUndo);
    break;
  case Step_CopyFolderContents:
    executor = new StepExecutor_CopyFolderContents(
        groupId, step, orgPath, sequence, sceneNumber, isUndo);
    break;
  case Step_RenameFile:
  case Step_CopyFile:
    executor = new StepExecutor_RenameFile(step, sequence, sceneNumber, isUndo);
    break;
  case Step_MoveFlowGroup:
    executor = new StepExecutor_MoveFlowGroup(groupId, step, sequence,
                                              sceneNumber, isUndo);
    break;
  case Step_CheckExistence:
    executor =
        new StepExecutor_CheckExistence(step, sequence, sceneNumber, isUndo);
    break;
  case Step_AskDelete:
    executor = new StepExecutor_AskDelete(step, orgPath, sequence, sceneNumber,
                                          isUndo);
    break;
  case Step_CreateCueFile:
    executor =
        new StepExecutor_CreateCueFile(step, sequence, sceneNumber, isUndo);
    break;
  case Step_StartBranchFlow:
    executor = new StepExecutor_StartEndBranchFlow(true, step, sequence,
                                                   sceneNumber, isUndo);
    break;
  case Step_EndBranchFlow:
    executor = new StepExecutor_StartEndBranchFlow(false, step, sequence,
                                                   sceneNumber, isUndo);
    break;
  case Step_SetSceneInfo:
    executor =
        new StepExecutor_SetSceneInfo(step, sequence, sceneNumber, isUndo);
    break;
  case Step_ConditionalBranchByInfo: {
    // 第一引数：info名 （QString）
    // 第二引数：条件となる数値 （QVariant）
    // 第三引数：正の場合（ActionStep）
    // 第四引数：負の場合（ActionStep）
    QString infoName  = step.params[0].toString();
    QString condition = step.params[1].toString();
    bool val          = false;
    QMap<QString, QVariant> infos =
        WorkflowManager::instance()->sceneInfoTags(sequence, sceneNumber);
    if (infos.contains(infoName)) {
      QVariant current = infos.value(infoName);
      if (condition.isEmpty()) {
        // 条件となる値が空の場合：真偽値のカット情報は値が真かどうか、
        // それ以外のカット情報は付いているかどうかで判定する
        bool isBoolTag = false;
        for (auto tag : WorkflowManager::instance()->sceneInfoTagList()) {
          if (tag->idName() == infoName) {
            isBoolTag = (tag->type() == InfoTagType_Bool);
            break;
          }
        }
        // QString→boolは、空・"0"・"false"以外を真とする
        val = isBoolTag ? current.toBool() : !current.toString().isEmpty();
      } else
        // INIから読んだ値は文字列になるため、文字列として比べる
        val = (current.toString() == condition);
    }
    int paramId = (val) ? 2 : 3;

    if (step.params[paramId].toList().isEmpty()) return nullptr;

    ActionStep as = {(ActionStepType)step.params[paramId].toList()[0].toInt(),
                     step.params[paramId].toList()[1].toList()};
    // std::cout << "paramcount = " << as.params.size() << std::endl;

    executor = createExecutor(groupId, as, orgPaths, sequence, sceneNumber,
                              isUndo, index, workflowName, actionName);
    break;
  }
  case Step_ConditionalBranchByFileExistence: {
    // 第一引数：ファイル名 （QString）
    // 第二引数：存在する場合（ActionStep）
    // 第三引数：存在しない場合（ActionStep）
    QString conditionPath = WorkflowManager::instance()->decodePath(
        step.params[0].toString(), sequence, sceneNumber);

    // 条件判定に使う直前に、難読化マクロをデコードする（画面には表示されない）
    conditionPath = WorkflowManager::instance()->decodeObfuscatedPathMacros(
        conditionPath, sequence, sceneNumber);

    const QString actual = SharedFiles::actualPath(conditionPath);
    if (!step.params[0].toString().isEmpty() && actual.isEmpty()) {
      m_invalidConditionPath = true;
      return nullptr;
    }
    bool val = QFileInfo(actual).exists();

    int paramId = (val) ? 1 : 2;

    if (step.params[paramId].toList().isEmpty()) return nullptr;
    ActionStep as = {(ActionStepType)step.params[paramId].toList()[0].toInt(),
                     step.params[paramId].toList()[1].toList()};
    executor      = createExecutor(groupId, as, orgPaths, sequence, sceneNumber,
                                   isUndo, index, workflowName, actionName);
    break;
  }
  case Step_InputGoogleSpreadsheet:
    executor = new StepExecutor_InputGoogleSpreadsheet(step, sequence,
                                                       sceneNumber, isUndo);
    break;
  case Step_DougaRetakeRouting:
    executor = new StepExecutor_DougaRetakeRouting(step, orgPath, sequence,
                                                   sceneNumber, isUndo);
    break;
  case Step_ReleaseProgress:
    executor = new StepExecutor_ReleaseProgress(step, sceneNumber, isUndo);
    break;
  case Step_AddProgress:
    executor = new StepExecutor_AddProgress(step, sceneNumber, isUndo);
    break;
  case Step_CreateEmptyFolder:
    executor =
        new StepExecutor_CreateEmptyFolder(step, sequence, sceneNumber, isUndo);
    break;
  case Step_ZipFolder:
    executor = new StepExecutor_ZipFolder(step, orgPath, sequence, sceneNumber,
                                          isUndo);
    break;
  case Step_NotifyGoogleChat:
    executor = new StepExecutor_NotifyGoogleChat(
        step, sequence, sceneNumber, isUndo, workflowName, actionName);
    break;
  }
  return executor;
}

void ConfirmationDialog::onOkClicked() {
  m_isCanceled = false;
  // bool ok = true;

  QMap<int, QString> errorLogTexts;
  int index = 0;
  for (auto executorList : m_stepExecutors) {
    QString logText;
    QList<QVariant> checkSums;
    for (auto executor : executorList) {
      QVariant checkSum;
      if (!executor->execute(logText, checkSum)) {
        errorLogTexts.insert(index, logText);
        registerFailedActionIndex(index);
        break;
      }
      // undo用にデータを格納
      if (!m_isUndo) checkSums.append(checkSum);
    }
    // undo用にデータを格納
    if (!m_isUndo) m_fileCheckSums.append(checkSums);
    index++;
  }

  if (!errorLogTexts.isEmpty()) {
    // 警告出し、フォルダへのリンクを提供
    QString warningStr =
        (m_isUndo) ? tr("Undoing was failed or interrupted. Click \"Show "
                        "Details\" for the log."
                        "\nYou may need to check and modify files manually.")
                   : tr("The action was failed or interrupted in the following "
                        "scene(s). Click \"Show Details\" for the log."
                        "\nYou may need to check and modify files manually.");

    QString detailText;
    for (auto index : errorLogTexts.keys()) {
      // 念のため、ログに実パスが混入していた場合は、難読化マクロの表記に戻す
      QString logStr = WorkflowManager::instance()->hideObfuscatedPaths(
          errorLogTexts.value(index), m_actionInfos[index].sequence,
          m_actionInfos[index].sceneNumber);
      detailText += tr("[Scene: %1]\n%2")
                        .arg(m_actionInfos[index].sceneNumber)
                        .arg(logStr);
    }

    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("Warning"));
    msgBox.setText(warningStr);
    msgBox.setDetailedText(detailText);
    QPushButton* ignoreBtn =
        msgBox.addButton(tr("Ignore"), QMessageBox::AcceptRole);
    QPushButton* openContainingBtn = msgBox.addButton(
        tr("Open containing folder(s)"), QMessageBox::HelpRole);
    msgBox.setDefaultButton(ignoreBtn);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.exec();

    // int ret = QMessageBox::warning(this, tr("Warning"), warningStr,
    // buttons[0], buttons[1]);

    if (msgBox.clickedButton() == openContainingBtn) {
      for (auto index : errorLogTexts.keys()) {
        QString dirPath =
            QFileInfo(m_actionInfos[index].orgPaths[0]).absoluteDir().path();
        QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
      }
    }
    reject();
  } else {
    // QMessageBox::information(this, tr("Done"), logText);
    // QString messageTxt = (m_isUndo) ? tr("Undoing Completed.") : tr("Action
    // Completed."); QMessageBox::information(this, tr("Information"),
    // messageTxt);
    accept();
  }
}

//--------------------------------------------------
RewindDestinationDialog::RewindDestinationDialog(WorkflowType workflowType,
                                                 int protocolId) {
  setWindowTitle(tr("Select Rewind Destination"));
  m_comboBox = new QComboBox(this);
  // int wfId = workflowId;
  Workflow wf = WorkflowManager::instance()->workflow(workflowType);
  m_comboBox->addItem(color2Icon(wf.color), wf.name,
                      QVariantList{static_cast<int>(workflowType), protocolId});

  // 同じワークフローグループのワークフローを集める
  QList<QPair<WorkflowType, Workflow>> workflowsInGroup;
  int groupId = wf.groupId;
  for (int wfId = 0; wfId < WorkflowManager::instance()->workflowCount();
       wfId++) {
    Workflow wf = WorkflowManager::instance()->getWorkflow(wfId);
    if (wf.groupId == groupId) workflowsInGroup.append({wf.workflowType, wf});
  }

  WorkflowType wfType = workflowType;
  while (1) {
    bool found = false;
    // 同じワークフローグループのワークフローについて
    for (auto wfPair : workflowsInGroup) {
      WorkflowType tmpWfType = wfPair.first;
      Workflow tmpWf         = wfPair.second;
      int tmpProtocolId      = 0;
      for (auto protocol : tmpWf.protocols) {
        for (auto action : protocol.actions) {
          if (action.allNextFlows().contains(wfType)) {
            // 一致するならリストに登録しwfTypeを更新
            found             = true;
            QVariantList list = {static_cast<int>(tmpWfType), tmpProtocolId};
            m_comboBox->insertItem(0, color2Icon(tmpWf.color), tmpWf.name,
                                   list);
            wfType = tmpWf.workflowType;
            break;
          }
        }
        tmpProtocolId++;
      }
      if (found) break;
    }

    // 工程をさかのぼり見つからなくなったらbreak
    if (!found) break;
  }

  QPushButton* selectBtn = new QPushButton(tr("Select"), this);
  QPushButton* cancelBtn = new QPushButton(tr("Cancel"), this);

  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setContentsMargins(10, 10, 10, 10);
  mainLay->setSpacing(5);
  {
    mainLay->addWidget(
        new QLabel(tr("Before which workflow do you want to return ?"), this),
        0);
    mainLay->addWidget(m_comboBox, 0);
    QHBoxLayout* buttonLay = new QHBoxLayout();
    buttonLay->setContentsMargins(5, 5, 5, 5);
    buttonLay->setSpacing(10);
    buttonLay->setAlignment(Qt::AlignCenter);
    {
      buttonLay->addWidget(selectBtn, 0);
      buttonLay->addWidget(cancelBtn, 0);
    }
    mainLay->addLayout(buttonLay, 0);
  }
  setLayout(mainLay);

  connect(selectBtn, SIGNAL(clicked()), this, SLOT(accept()));
  connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
}

QPair<WorkflowType, int> RewindDestinationDialog::destinationWorkflow() {
  QVariantList list = m_comboBox->currentData().toList();
  return {static_cast<WorkflowType>(list[0].toInt()), list[1].toInt()};
}

//--------------------------------------------------
// このへんから！
RewindConfirmationDialog::RewindConfirmationDialog(
    ActionInfo actionInfo, QPair<WorkflowType, int> destinationWfTypePair,
    bool isUndo, QList<QVariant> checkSums)
    : ConfirmationDialog(actionInfo, isUndo)
    , m_destinationWfTypePair(destinationWfTypePair) {
  if (!isUndo) setWindowTitle(tr("Confirm Rewind"));
  m_googleStepPos.append(
      QList<
          int>{});  // Rewindのステップ構成にGoogleスプレッドシート入力は含まれない
  WorkflowType workflowType = actionInfo.workflowType;
  QStringList orgPaths      = actionInfo.orgPaths;
  QString sequence          = actionInfo.sequence;
  QString sceneNumber       = actionInfo.sceneNumber;

  // 話数機能ON時、このActionInfoが所属する話数へ切り替えてからパス解決を行う
  WorkflowManager::instance()->setCurrentEpisode(actionInfo.episode);

  QPushButton* okButton     = new QPushButton(tr("OK"), this);
  QPushButton* cancelButton = new QPushButton(tr("Cancel"), this);
  Workflow wf = WorkflowManager::instance()->workflow(workflowType);
  Workflow destinationWf =
      WorkflowManager::instance()->workflow(destinationWfTypePair.first);
  WorkflowProtocol destinationProtocol =
      destinationWf.protocols.at(m_destinationWfTypePair.second);

  // 現在のファイルの代わりに置くテキストファイル。RewindがONの状態の条件ファイル名を取得
  QString cueFileName;
  {
    QString tmpConditionPath = destinationProtocol.conditionPath;
    //[R]をRewind接尾辞に置き換え
    tmpConditionPath.replace("[R]", RewindStr);
    QString tmp = WorkflowManager::instance()->decodePath(
        tmpConditionPath, sequence, sceneNumber);
    cueFileName = QFileInfo(tmp).fileName() + ".txt";
  }

  QString rootDirPath = QFileInfo(orgPaths[0]).dir().absolutePath();
  QString relativeRootDirPath =
      QDir(WorkflowManager::instance()->projectRootPath())
          .relativeFilePath(rootDirPath);

  // ルートフォルダを確認し、oldフォルダがある場合はold2、以降old3,old4...と追加する
  // ★Undoの場合は存在する最もおおきいoldフォルダを探す
  QString rewindBkupFolderName = RewindBkupFolderBaseName;
  QString copyFolderArgument;
  int rewindCount = 1;
  // std::cout << "rootDirPath = " << rootDirPath.toStdString() << std::endl;
  while (QDir(rootDirPath).exists(rewindBkupFolderName)) {
    if (rewindCount > 1) copyFolderArgument += ";";
    copyFolderArgument += "-" + rewindBkupFolderName;
    rewindCount++;
    if (!isUndo)
      rewindBkupFolderName =
          RewindBkupFolderBaseName + QString::number(rewindCount);
    else {  // undo case
      QString nextBkupFolderName =
          RewindBkupFolderBaseName + QString::number(rewindCount);
      if (QDir(rootDirPath).exists(nextBkupFolderName))
        rewindBkupFolderName = nextBkupFolderName;
      else
        break;
    }
  }

  // Rewindの操作
  QList<ActionStep> steps = {
      {Step_CopyFolderContents,
       {"[ALL][MOVE]" + copyFolderArgument, "",
        rootDirPath + "/" + rewindBkupFolderName}},
      {Step_CreateCueFile, {rootDirPath + "/" + cueFileName}},
      {Step_SetSceneInfo, {"REWIND", "WIP"}},
      // ここに、Rewind用のStepを追加。詳細工程を戻し、Rewind登録をする
  };

  QList<StepExecutor*> stepExecutorList;

  for (auto step : steps) {
    StepExecutor* executor;
    if (step.type == Step_CopyFolderContents)
      executor = new StepExecutor_CopyFolderContents(
          wf.groupId, step, orgPaths[0], sequence, sceneNumber, isUndo, true);
    else if (step.type == Step_CreateCueFile)
      executor = new StepExecutor_CreateCueFile(step, sequence, sceneNumber,
                                                isUndo, true);
    else
      executor = createExecutor(wf.groupId, step, orgPaths, sequence,
                                sceneNumber, isUndo);
    if (!executor) continue;
    if (isUndo) {  // 逆順に実行する
      executor->setCheckSum(checkSums.takeFirst());
      stepExecutorList.push_front(executor);
    } else
      stepExecutorList.append(executor);
  }

  // 最後に、詳細進捗リストを更新するExecutorを追加する
  StepExecutor* progressExecutor = new StepExecutor_RegisterProgress(
      sceneNumber, wf.workflowType, {destinationWf.workflowType}, isUndo);
  if (isUndo)
    stepExecutorList.push_front(progressExecutor);
  else
    stepExecutorList.append(progressExecutor);

  m_stepExecutors.append(stepExecutorList);

  setStyleSheet(
      "QLabel{font-size: 10pt;}\nQRadioButton{font-size: "
      "10pt;}\nQLabel#LargeSizedLabel{font-size: 16pt;}");

  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setContentsMargins(10, 10, 10, 10);
  mainLay->setSpacing(5);
  {
    QString titleString = tr("Scene : %1   Workflow : %2\nRewind to: %3")
                              .arg(sceneNumber)
                              .arg(wf.name)
                              .arg(destinationWf.name);
    if (isUndo) titleString.push_front(tr("[Undoing Action]\n"));
    QLabel* titleLabel = new QLabel(titleString, this);
    titleLabel->setObjectName("LargeSizedLabel");
    mainLay->addWidget(titleLabel, 0);

    for (auto executor : stepExecutorList) {
      if (!executor->hasUI()) continue;
      QFrame* line = new QFrame(this);
      line->setFrameShape(QFrame::HLine);
      line->setFrameShadow(QFrame::Sunken);
      mainLay->addWidget(line, 0);
      mainLay->addWidget(executor->createWidget(), 0);
    }
    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLay->addWidget(line, 0);

    mainLay->addWidget(new QLabel(tr("Are you sure ?")), 0);

    QFrame* solidLine = new QFrame(this);
    solidLine->setFrameShape(QFrame::HLine);
    solidLine->setFrameShadow(QFrame::Plain);
    mainLay->addWidget(solidLine, 0);

    QHBoxLayout* buttonsLay = new QHBoxLayout();
    buttonsLay->setContentsMargins(10, 10, 10, 10);
    buttonsLay->setSpacing(20);
    {
      buttonsLay->addStretch(1);
      buttonsLay->addWidget(okButton, 0);
      buttonsLay->addWidget(cancelButton, 0);
      buttonsLay->addStretch(1);
    }
    mainLay->addLayout(buttonsLay);
  }
  setLayout(mainLay);

  if (m_invalidConditionPath) {
    okButton->setEnabled(false);
    mainLay->addWidget(new QLabel(
        tr("The condition path is ambiguous or unavailable."), this));
  }
  connect(okButton, SIGNAL(clicked()), this, SLOT(onOkClicked()));
  connect(cancelButton, &QPushButton::clicked, [&]() {
    m_isCanceled = true;
    accept();
  });
}
