#include "sharedfiles.h"
#include "mywindow.h"
#include "platform.h"
#include "usersettingspopup.h"
#include "actionconfirmationdialog.h"
#include "usersettings.h"
#include "googlegateway.h"
#include "googlechatgateway.h"
#include "retakedialog.h"
#include "sceneinfotag.h"
#include "projectsettingsdialog.h"
#include "mainflowchartscene.h"
#include "sceneknowledgepanel.h"
#include "registerscenedialog.h"
#include "aboutpopup.h"

#include <QEvent>
#include <QVBoxLayout>
#include <QFontMetrics>
#include <QStringList>
#include <iostream>
#include <QFile>
#include <QPushButton>
#include <QHeaderView>
#include <QProcess>
#include <QButtonGroup>
#include <QPushButton>
#include <QMessageBox>
#include <QProgressDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QStackedWidget>
#include <QTextStream>
#include <QTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
// #include <QDirIterator>
#include <QComboBox>
#include <QCoreApplication>
#include <QMenu>
#include <QScrollArea>
#include <QSettings>
#include <QGroupBox>
#include <QComboBox>
#include <QRegularExpression>
#include <QTextCodec>
#include <QFrame>
#include <QDirListing>
#include <QtConcurrent>
#include <QGraphicsScene>
#include <QFutureWatcher>
#include <QPointer>
#include <QAction>
#ifdef Q_OS_MACOS
#include <QMenuBar>
#endif
#include <QImage>
#include <QPixmap>
#include <algorithm>
#include <QSplitter>
#include <QSet>

namespace {
QMap<QString, QIcon> colorIcons;
const QString GengaSuffix = "_genga_ts";
const QString DougaSuffix = "_douga_ts";
const int RewindActionId  = -1;

QIcon color2Icon(QColor color) {
  if (colorIcons.contains(color.name())) return colorIcons.value(color.name());
  QPixmap pm(15, 15);
  pm.fill(color);
  QIcon icon(pm);
  colorIcons.insert(color.name(), icon);
  return icon;
}

// AsynchronousUndoButtonの"PPM_GoogleStepPos"/"PPM_PendingGoogleStepPos"プロパティに
// 格納するため、QList<int>をQVariantListへ変換する
QVariantList toVariantList(const QList<int>& list) {
  QVariantList ret;
  for (int v : list) ret.append(v);
  return ret;
}

void removeAllButtonsFromGroup(QButtonGroup* buttonGroup) {
  QList<QAbstractButton*> buttonList = buttonGroup->buttons();
  // リスト内のボタンをすべて除外
  for (QAbstractButton* button : buttonList) {
    buttonGroup->removeButton(button);
  }
}

}  // namespace

//----------------------------------------------
// Googleスプレッドシートの更新完了シグナルを受け取って
// Undo情報の格納と有効化を行う

AsynchronousUndoButton::AsynchronousUndoButton(bool hasGoogleStep,
                                               QWidget* parent)
    : QPushButton(parent) {
  if (hasGoogleStep) {
    setWaitState();
    connect(GoogleGateway::instance(),
            SIGNAL(googleSpreadSheetInput(QString, QStringList, int)), this,
            SLOT(onGoogleSpreadSheetInput(QString, QStringList, int)));
    connect(GoogleGateway::instance(),
            SIGNAL(googleSpreadSheetInputFailed(QString, int)), this,
            SLOT(onGoogleSpreadSheetInputFailed(QString, int)));
  } else
    setText(tr("Undo"));
  setToolTip(QString());
}

void AsynchronousUndoButton::setWaitState() {
  setText(tr("Please Wait ..."));
  setDisabled(true);
}

void AsynchronousUndoButton::onGoogleSpreadSheetInput(
    QString sceneNumber, QStringList previousValues, int requestId) {
  // sceneNumberは後半にバンクカットを含むので、PPM_SceneNumberの値と一致しないかもしれない
  if (!sceneNumber.startsWith(this->property("PPM_SceneNumber").toString()))
    return;

  // このボタン（＝このアクション）がまだ待っているGoogleステップかどうかをrequestIdで確認する。
  // ひとつのアクションに複数のGoogleスプレッドシート入力ステップがあり得るため、
  // 単純な位置の一致だけでなく「まだ保留中か」も確認する必要がある
  QVariantList pending = this->property("PPM_PendingGoogleStepPos").toList();
  if (!pending.removeOne(requestId)) return;
  setProperty("PPM_PendingGoogleStepPos", pending);

  QVariantList checkSums = this->property("PPM_FileCheckSums").toList();
  assert(requestId >= 0 && requestId < checkSums.size());
  // Googleの操作位置にUndo用の情報を格納
  checkSums.replace(requestId, previousValues);
  setProperty("PPM_FileCheckSums", checkSums);

  finishIfAllGoogleStepsDone();
}

void AsynchronousUndoButton::onGoogleSpreadSheetInputFailed(QString sceneNumber,
                                                            int requestId) {
  // sceneNumberは後半にバンクカットを含むので、PPM_SceneNumberの値と一致しないかもしれない
  if (!sceneNumber.startsWith(this->property("PPM_SceneNumber").toString()))
    return;

  QVariantList pending = this->property("PPM_PendingGoogleStepPos").toList();
  if (!pending.removeOne(requestId)) return;
  setProperty("PPM_PendingGoogleStepPos", pending);
  setProperty("PPM_HasGoogleFailure", true);
  // 失敗した位置のcheckSumは、execute()実行時に格納された空のリストのままにしておく
  // （Undo用の「変更前の値」が取得できなかった、という状態を表す）

  finishIfAllGoogleStepsDone();
}

void AsynchronousUndoButton::finishIfAllGoogleStepsDone() {
  // まだ他のGoogleスプレッドシート入力ステップの完了を待っている場合は、ボタンは有効化しない
  if (!this->property("PPM_PendingGoogleStepPos").toList().isEmpty()) return;

  if (this->property("PPM_HasGoogleFailure").toBool()) {
    setText(tr("Undo (*)"));
    setToolTip(tr("Failed To Register Google Spreadsheet."));
  } else {
    setText(tr("Undo"));
    setToolTip(QString());
  }
  setEnabled(true);
}

//----------------------------------------------

class MyItem : public QTableWidgetItem {
public:
  MyItem(const QString& text) : QTableWidgetItem(text) {
    setFlags(flags() ^ Qt::ItemIsEditable);
  }
};

// Thumbnail列のセルウィジェット。読み込んだ画像を保持しておき、行の高さ（小／大）が
// 切り替わったときに、ファイルを読み直さずその場で再スケールできるようにする
class ThumbnailLabel : public QLabel {
public:
  ThumbnailLabel(QWidget* parent = nullptr) : QLabel(parent) {
    setAlignment(Qt::AlignCenter);
  }
  void setImage(const QImage& image, int rowHeight) {
    m_image = image;
    rescale(rowHeight);
  }
  void rescale(int rowHeight) {
    if (m_image.isNull()) {
      setPixmap(QPixmap());
      return;
    }
    int targetHeight = std::max(1, rowHeight - 4);
    setPixmap(QPixmap::fromImage(m_image).scaledToHeight(
        targetHeight, Qt::SmoothTransformation));
  }

private:
  QImage m_image;
};

// サムネイル画像の読み込み（ファイルI/Oのみを行い、Qtウィジェットの生成・操作はここでは行わないため、
// QtConcurrentで別スレッド実行しても安全）
QImage loadThumbnailImage(const QString& path) {
  if (path.isEmpty() || !QFileInfo::exists(path)) return QImage();
  return QImage(path);
}

const int COLUMNINDEX_CHECKBOX     = 0;
const int COLUMNINDEX_OPEN         = 1;
const int COLUMNINDEX_WORKFLOWID   = 2;  // Hidden Column, used only for sorting
const int COLUMNINDEX_WORKFLOWNAME = 3;
const int COLUMNINDEX_MEDIA        = 4;
const int COLUMNINDEX_THUMBNAIL    = 5;
const int COLUMNINDEX_EPISODE      = 6;
const int COLUMNINDEX_SCENENUMBER  = 7;
const int COLUMNINDEX_LASTMODIFIED = 8;
const int COLUMNINDEX_ACTIONS      = 9;
const int COLUMNINDEX_INFORMATION  = 10;

// UserSettingsへの列表示状態の保存に使うキー名。列の追加・削除をしても
// 既存ユーザーの保存済み設定がずれないよう、列インデックスではなく固定の文字列で管理する
const QString ColumnVisibilityKey_Media            = "Media";
const QString ColumnVisibilityKey_Thumbnail        = "Thumbnail";
const QString ColumnVisibilityKey_Episode          = "Episode";
const QString ColumnVisibilityKey_LastModifiedDate = "LastModifiedDate";
const QString ColumnVisibilityKey_Information      = "Information";

MyTable::MyTable(QWidget* parent) : QTableWidget(0, 0, parent) {
  QStringList labels;
  labels << ""  // まとめて承認ボタンの列
         << tr("Open")
         << tr("WorkflowId")  // Hidden Column, used only for sorting
         << tr("Workflow Name") << tr("Media") << tr("Thumbnail")
         << tr("Episode")  // 話数機能を使わないプロジェクトでは常に空欄
         << tr("Scene Number") << tr("Last Modified Date") << tr("Action")
         << tr("Information", "Table Header Label");

  setColumnCount(labels.count());
  setHorizontalHeaderLabels(labels);

  // setItem(0, 0, new QTableWidgetItem("test"));

  setSortingEnabled(true);
  horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(horizontalHeader(), &QHeaderView::customContextMenuRequested, this,
          &MyTable::onHeaderContextMenuRequested);

  m_thumbnailBaseRowHeight = verticalHeader()->defaultSectionSize();
  m_thumbnailSizeLarge     = UserSettings::instance()->loadThumbnailSizeLarge();
  applyColumnVisibility();
  setColumnHidden(COLUMNINDEX_WORKFLOWID, true);  // hide the workflow id column
  // search();
  setItemPrototype(new MyItem(""));
}

void MyTable::applyColumnVisibility() {
  bool hasEpisodes = WorkflowManager::instance()->hasEpisodes();
  bool hasThumbnail =
      !WorkflowManager::instance()->sceneThumbnailPath().isEmpty();

  setColumnHidden(COLUMNINDEX_MEDIA,
                  !UserSettings::instance()->loadColumnVisible(
                      ColumnVisibilityKey_Media, true));
  setColumnHidden(COLUMNINDEX_THUMBNAIL,
                  !hasThumbnail || !UserSettings::instance()->loadColumnVisible(
                                       ColumnVisibilityKey_Thumbnail, true));
  setColumnHidden(COLUMNINDEX_EPISODE,
                  !hasEpisodes || !UserSettings::instance()->loadColumnVisible(
                                      ColumnVisibilityKey_Episode, true));
  setColumnHidden(COLUMNINDEX_LASTMODIFIED,
                  !UserSettings::instance()->loadColumnVisible(
                      ColumnVisibilityKey_LastModifiedDate, true));
  setColumnHidden(COLUMNINDEX_INFORMATION,
                  !UserSettings::instance()->loadColumnVisible(
                      ColumnVisibilityKey_Information, true));
}

void MyTable::setThumbnailSizeLarge(bool isLarge) {
  m_thumbnailSizeLarge = isLarge;
  UserSettings::instance()->saveThumbnailSizeLarge(isLarge);

  int rowHeight = currentThumbnailRowHeight();
  for (int r = 0; r < rowCount(); r++) {
    setRowHeight(r, rowHeight);
    if (auto* label =
            dynamic_cast<ThumbnailLabel*>(cellWidget(r, COLUMNINDEX_THUMBNAIL)))
      label->rescale(rowHeight);
  }
}

void MyTable::onHeaderContextMenuRequested(const QPoint& pos) {
  QMenu menu(this);

  // 表示は常にON固定で、操作はできない列
  struct FixedColumn {
    QString label;
  };
  QList<FixedColumn> fixedColumns = {{tr("Open")},
                                     {tr("Workflow Name")},
                                     {tr("Scene Number")},
                                     {tr("Action")}};
  for (auto& fc : fixedColumns) {
    QAction* action = menu.addAction(fc.label);
    action->setCheckable(true);
    action->setChecked(true);
    action->setEnabled(false);
  }

  menu.addSeparator();

  // 表示/非表示を切り替えられる列。トグルのたびに設定を保存し、即座に反映する
  auto addToggleableColumnAction = [this, &menu](const QString& label,
                                                 const QString& columnKey) {
    QAction* action = menu.addAction(label);
    action->setCheckable(true);
    action->setChecked(
        UserSettings::instance()->loadColumnVisible(columnKey, true));
    connect(action, &QAction::toggled, this, [columnKey](bool checked) {
      UserSettings::instance()->saveColumnVisible(columnKey, checked);
    });
    connect(action, &QAction::toggled, this, &MyTable::applyColumnVisibility);
  };

  addToggleableColumnAction(tr("Media"), ColumnVisibilityKey_Media);
  bool hasThumbnail =
      !WorkflowManager::instance()->sceneThumbnailPath().isEmpty();
  if (hasThumbnail)
    addToggleableColumnAction(tr("Thumbnail"), ColumnVisibilityKey_Thumbnail);
  if (WorkflowManager::instance()->hasEpisodes())
    addToggleableColumnAction(tr("Episode"), ColumnVisibilityKey_Episode);
  addToggleableColumnAction(tr("Last Modified Date"),
                            ColumnVisibilityKey_LastModifiedDate);
  addToggleableColumnAction(tr("Information"), ColumnVisibilityKey_Information);

  if (hasThumbnail) {
    menu.addSeparator();
    QMenu* sizeMenu      = menu.addMenu(tr("Thumbnail Size"));
    QAction* smallAction = sizeMenu->addAction(tr("Small"));
    QAction* largeAction = sizeMenu->addAction(tr("Large"));
    smallAction->setCheckable(true);
    largeAction->setCheckable(true);
    smallAction->setChecked(!m_thumbnailSizeLarge);
    largeAction->setChecked(m_thumbnailSizeLarge);
    connect(smallAction, &QAction::triggered, this,
            [this]() { setThumbnailSizeLarge(false); });
    connect(largeAction, &QAction::triggered, this,
            [this]() { setThumbnailSizeLarge(true); });
  }

  menu.exec(horizontalHeader()->mapToGlobal(pos));
}

namespace {
// conditionPathBase（ROOT/PREFIX/EPISODEまで解決済みの、カット非依存の条件パス）に対して、
// 指定した1カット分のSEQUENCE/TAKE/R_TAKE/Rewind/CUTNUMBERを解決し、キューファイルが
// 存在するかどうかを判定する。呼び出しごとに新たにローカルコピーを作って置換するため、
// 他のカットの呼び出しと状態を共有せず、QtConcurrentによる並列呼び出しでも安全
enum class CueStatus { Missing, Found, Ambiguous };
CueStatus cueStatus(const QString& expanded) {
  if (expanded.isEmpty()) return CueStatus::Missing;
  const QString actual = SharedFiles::actualPath(
      WorkflowManager::instance()->resolveProjectPath(expanded));
  if (actual.isEmpty()) return CueStatus::Ambiguous;
  return QFileInfo::exists(actual) ? CueStatus::Found : CueStatus::Missing;
}

CueStatus cutQueueFileExists(const QString& conditionPathBase,
                             const QString& cutName,
                             FlowGroupInfo::TakeNotationStyle takeStyle) {
  QString conditionPath = conditionPathBase;
  conditionPath.replace(
      "[SEQUENCE]", WorkflowManager::instance()->sceneNum2Sequence(cutName));
  if (!WorkflowManager::instance()->useSequence())
    conditionPath = WorkflowManager::collapseSeparators(conditionPath);

  QString takeSuffix;
  switch (takeStyle) {
  case FlowGroupInfo::TakeNotation_Rewind:
    takeSuffix = WorkflowManager::instance()->currentRewindSuffix(cutName);
    conditionPath.replace("[R]", takeSuffix);
    break;
  case FlowGroupInfo::TakeNotation_Take:
    takeSuffix = WorkflowManager::instance()->currentTakeSuffix(cutName);
    conditionPath.replace("[TAKE]", takeSuffix);
    break;
  case FlowGroupInfo::TakeNotation_RTake:
    takeSuffix = WorkflowManager::instance()->currentTakeSuffix(cutName, true);
    conditionPath.replace("[R_TAKE]", takeSuffix);
    break;
  default:
    break;
  }

  // 兼用カットの場合
  if (cutName.contains("_")) {
    QStringList dupCutList = {cutName};
    dupCutList.append(cutName.split("_", Qt::SkipEmptyParts));
    for (auto dupCutName : dupCutList) {
      QString tmpConditionPath = conditionPath;
      tmpConditionPath.replace("[CUTNUMBER]", dupCutName);
      auto status = cueStatus(tmpConditionPath);
      if (status != CueStatus::Missing) return status;
    }
    return CueStatus::Missing;
  } else {
    conditionPath.replace("[CUTNUMBER]", cutName);
    return cueStatus(conditionPath);
  }
}
}  // namespace

// 各ワークフローに所属するカット名のリストを、実際のキューファイルの有無から更新する
void MyTable::simpleSearch(FlowGroupId fgId) {
  QMap<WorkflowType, QStringList> progressList;
  // 現在fgIdグループに登録されているカット名のリストを取得する
  QStringList cutList = WorkflowManager::instance()->cutListInFlow(fgId) +
                        WorkflowManager::instance()->cutListInBranchFlow(fgId);
  cutList
      .removeDuplicates();  // 同じカットが同じフローの主流と傍流に同時に所属することは無いはずだが、念のため。

  FlowGroupInfo::TakeNotationStyle takeStyle = WorkflowManager::instance()
                                                   ->flowGroupInfos()
                                                   .value(fgId)
                                                   .takeNotationStyle;

  // 各ワークフローについて
  for (int wfId = 0; wfId < WorkflowManager::instance()->workflowCount();
       wfId++) {
    Workflow wf = WorkflowManager::instance()->getWorkflow(wfId);
    // fgIdグループに属していなければcontinue
    if (wf.groupId != fgId) continue;

    QStringList wfSceneList;
    // 各プロトコルについて
    for (auto protocol : wf.protocols) {
      // 条件パスを取得
      QString conditionPathBase = WorkflowManager::instance()->decodePathMacros(
          protocol.conditionPath, true);
      // 条件パスをデコード
      // PREFIX and EPISODE; resolve ROOT with the per-scene path.

      conditionPathBase.replace("[PREFIX]",
                                WorkflowManager::instance()->cutNumberPrefix());
      conditionPathBase.replace("[USER_NAME]",
                                UserSettings::instance()->userName());
      conditionPathBase.replace("[EPISODE]",
                                WorkflowManager::instance()->currentEpisode());

      // 既にwfSceneListに登録済みのカットは判定不要なので除く
      QStringList remainingCuts;
      for (auto& cutName : cutList)
        if (!wfSceneList.contains(cutName)) remainingCuts.append(cutName);

      // 各カットについて、キューファイルの有無をQtConcurrentで並列に判定する
      QList<CueStatus> foundList = QtConcurrent::blockingMapped(
          remainingCuts,
          [&conditionPathBase, takeStyle](const QString& cutName) {
            return cutQueueFileExists(conditionPathBase, cutName, takeStyle);
          });

      if (foundList.contains(CueStatus::Ambiguous)) {
        QMessageBox::warning(
            this, tr("Refresh"),
            tr("Multiple file names have the same Unicode spelling. The "
               "progress list was not updated."));
        return;
      }
      for (int i = 0; i < remainingCuts.size(); i++)
        if (foundList.at(i) == CueStatus::Found)
          // 存在するならWfSceneListに追加
          wfSceneList.append(remainingCuts.at(i));
    }  // 次のプロトコル

    // progressListに登録
    progressList.insert(wf.workflowType, wfSceneList);
  }

  // ソートする
  for (auto key : progressList.keys()) progressList[key].sort();

  // 保存する
  WorkflowManager::instance()->saveProgressList(fgId, progressList);
}

namespace {
// doRefreshのワークアイテム：1カット・1工程(WorkflowType)の組について、
// キューファイルの有無を判定するために必要な情報のみを持つ（Qtウィジェットへの参照は持たない）
struct DoRefreshWorkItem {
  FlowGroupId fgId;
  WorkflowType wfType;
  QString sceneNumber;
};

struct DoRefreshResult {
  bool found = false;
  bool ambiguous = false;
  DataInfo info;
};

// ワークアイテム1件についてキューファイルの有無を判定する。ファイルI/Oと
// WorkflowManagerの読み取り専用メソッドのみを呼び出すため、QtConcurrentによる
// 並列呼び出しでも安全（Qtウィジェットの生成・操作はここでは一切行わない）
DoRefreshResult doRefreshWorkItem(const DoRefreshWorkItem& item) {
  DoRefreshResult result;
  DataInfo info;
  info.episode     = WorkflowManager::instance()->currentEpisode();
  info.sceneNumber = item.sceneNumber;
  info.sequence =
      WorkflowManager::instance()->sceneNum2Sequence(item.sceneNumber);
  // Scene Thumbnail Pathが未設定のプロジェクトでは空文字列を返す
  info.thumbnailPath = SharedFiles::actualPath(
      WorkflowManager::instance()->sceneThumbnailPathFor(info.sequence,
                                                         item.sceneNumber));

  switch (WorkflowManager::instance()
              ->flowGroupInfos()
              .value(item.fgId)
              .takeNotationStyle) {
  case FlowGroupInfo::TakeNotation_Rewind:
    info.takeSuffix =
        WorkflowManager::instance()->currentRewindSuffix(item.sceneNumber);
    break;
  case FlowGroupInfo::TakeNotation_Take:
    info.takeSuffix =
        WorkflowManager::instance()->currentTakeSuffix(item.sceneNumber);
    break;
  case FlowGroupInfo::TakeNotation_RTake:
    info.takeSuffix =
        WorkflowManager::instance()->currentTakeSuffix(item.sceneNumber, true);
    break;
  default:
    break;
  }

  // 表示工程の各WFについて
  Workflow wf    = WorkflowManager::instance()->workflow(item.wfType);
  int protocolId = 0;
  for (auto protocol : wf.protocols) {
    // パスにあてはめ、キューファイルが存在するか確認
    QString conditionPath = WorkflowManager::instance()->decodePath(
        protocol.conditionPath, info.sequence, info.sceneNumber);

    QStringList searchConditionPaths = {conditionPath};

    // 兼用カットの場合、個別のカット番号のついたファイルも探索対象に加える
    // ※ただし、フォルダパスはカット番号の組み合わせ、ファイル名だけ個別のカット番号を用いる
    if (info.sceneNumber.contains("_")) {
      for (auto cutNum : info.sceneNumber.split("_", Qt::SkipEmptyParts)) {
        QString conditionPath_eachScene =
            WorkflowManager::instance()->decodePath(protocol.conditionPath,
                                                    info.sequence, cutNum,
                                                    info.sceneNumber);
        searchConditionPaths.append(conditionPath_eachScene);
      }
    }

    bool found = false;
    for (auto searchConditionPath : searchConditionPaths) {
      // 存在する場合
      auto* manager          = WorkflowManager::instance();
      const QString resolved = manager->decodeObfuscatedPathMacros(
          searchConditionPath, info.sequence, info.sceneNumber);
      const QString actual = SharedFiles::actualPath(resolved);
      if (!resolved.isEmpty() && actual.isEmpty()) {
        result.ambiguous = true;
        return result;
      }
      QFileInfo fileInfo(actual);
      if (fileInfo.exists()) {
        // ファイル名、パス、更新日時の追加
        info.paths.append(fileInfo.absoluteFilePath());
        info.fileNames.append(fileInfo.fileName());
        info.lastModifiedTime = fileInfo.lastModified();

        found = true;
      }
    }
    if (found) {
      info.workflowType = item.wfType;
      info.protocolId   = protocolId;
      result.found      = true;
      result.info       = info;
      break;
    }
    protocolId++;
  }
  return result;
}
}  // namespace

// キャッシュされた詳細を元にリストを作り直す
void MyTable::doRefresh(const QList<WorkflowType>& checkedWfTypes,
                        const QList<WorkflowType>& lockedWfTypes,
                        const QMap<WorkflowType, int>& wfType_cbId_map,
                        bool appendMode) {
  setSortingEnabled(false);
  if (!appendMode) {
    clearContents();
    setRowCount(0);
  }

  // 表示される工程をグループ毎に分ける
  QMap<FlowGroupId, QMap<WorkflowType, QStringList>> cutListsByGroup;
  for (auto wfType : checkedWfTypes) {
    FlowGroupId fgId = WorkflowManager::instance()->WfType2FgId(wfType);

    // 空のStringListを入れておく
    if (cutListsByGroup.contains(fgId))
      cutListsByGroup[fgId].insert(wfType, {});
    else {
      QMap<WorkflowType, QStringList> cutList = {{wfType, {}}};
      cutListsByGroup.insert(fgId, cutList);
    }
  }

  QDir projectRoot(WorkflowManager::instance()->projectRootPath());

  // 各グループについて、表示工程のカットリストを取得し、ワークアイテム（カット×工程の組）を集める
  QList<DoRefreshWorkItem> workItems;
  for (auto fgId : cutListsByGroup.keys()) {
    // 表示工程のカットリストを取得する
    WorkflowManager::instance()->getProgressList(fgId, cutListsByGroup[fgId]);

    // 表示工程について
    for (auto wfType : cutListsByGroup[fgId].keys()) {
      // カットリストの各カットについて
      for (auto sceneNumber : cutListsByGroup[fgId].value(wfType))
        workItems.append({fgId, wfType, sceneNumber});
    }
  }

  // 各ワークアイテムについて、キューファイルの有無をQtConcurrentで並列に判定する
  // （ファイルI/Oのみを行い、Qtウィジェットの生成はここでは行わない）
  QList<DoRefreshResult> results =
      QtConcurrent::blockingMapped(workItems, doRefreshWorkItem);

  // 判定結果に基づき、見つかったものだけをテーブルに登録する（QTableWidgetの操作はメインスレッドで行う）
  if (std::any_of(results.cbegin(), results.cend(),
                  [](const DoRefreshResult& r) { return r.ambiguous; }))
    QMessageBox::warning(this, tr("Refresh"),
                         tr("Multiple file names have the same Unicode "
                            "spelling. Conflicting scenes were skipped."));
  for (auto& result : results) {
    if (!result.found) continue;
    // Rowを1行追加
    setRowCount(rowCount() + 1);
    // registerItemする
    registerItem(rowCount() - 1, result.info,
                 lockedWfTypes.contains(result.info.workflowType),
                 wfType_cbId_map.value(result.info.workflowType));
  }

  update();
  setSortingEnabled(true);

  // sortItemsは安定ソートのため、最後に呼び出したものが最優先のソートキーになる。
  // ワークフロー＞話数（All
  // Episodesモードでの見やすさのため）＞メディア＞シーン番号の順に優先させる
  sortItems(COLUMNINDEX_SCENENUMBER);  // scene number
  sortItems(COLUMNINDEX_MEDIA);        // media
  sortItems(COLUMNINDEX_EPISODE);      // episode
  sortItems(COLUMNINDEX_WORKFLOWID);   // checkBoxId
}

void MyTable::registerItem(int r, DataInfo info, bool isLocked,
                           int checkboxId) {
  // labels << tr("Open")
  //   << tr("State")
  //   << tr("Scene Number")
  //   << tr("Last Modified Date")
  //   << tr("Done");
  //
  Workflow wf = WorkflowManager::instance()->workflow(info.workflowType);
  WorkflowProtocol protocol = wf.protocols.at(info.protocolId);
  int col                   = 0;

  // col0 :
  // まとめて承認用CheckBox：承認ボタンが１つの工程にチェックボックスを作成
  if (protocol.actions.size() == 1 && !protocol.actions.at(0).steps.isEmpty()) {
    QCheckBox* checkBox = new QCheckBox();
    checkBox->setProperty("PPM_Row", r);
    checkBox->setProperty("PPM_FilePaths", info.paths);
    checkBox->setProperty("PPM_Episode", info.episode);
    checkBox->setProperty("PPM_Sequence", info.sequence);
    checkBox->setProperty("PPM_SceneNumber", info.sceneNumber);
    checkBox->setProperty("PPM_WorkflowType", info.workflowType);
    checkBox->setProperty("PPM_ProtocolId", info.protocolId);
    connect(checkBox, SIGNAL(clicked(bool)), this,
            SLOT(onCheckBoxClicked(bool)));
    setCellWidget(r, col, checkBox);

    // ロックする
    checkBox->setDisabled(isLocked);
    if (isLocked)
      checkBox->setToolTip(
          tr("The checkbox to select this workflow is locked."));
  }
  col++;

  // col1 : Open Buttons
  QPushButton* openButton = new QPushButton(tr("Open"), this);

  QPushButton* xdtsButton =
      new QPushButton(QIcon(":Resources/xdts.svg"), "", this);
  QPushButton* folderButton =
      new QPushButton(QIcon(":Resources/folder.svg"), "", this);
  QPushButton* logButton =
      new QPushButton(QIcon(":Resources/log.svg"), "", this);
  xdtsButton->setFixedSize(23, 23);
  xdtsButton->setIconSize(QSize(18, 18));
  xdtsButton->setToolTip(tr("Open XDTS"));
  folderButton->setFixedSize(23, 23);
  folderButton->setIconSize(QSize(18, 18));
  folderButton->setToolTip(tr("Open Containing Folder"));
  logButton->setFixedSize(23, 23);
  logButton->setIconSize(QSize(18, 18));
  logButton->setToolTip(tr("View Log"));

  QFrame* container = new QFrame;
  QHBoxLayout* lay  = new QHBoxLayout();
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(1);
  {
    lay->addWidget(openButton, 1);
    lay->addSpacing(5);
    lay->addWidget(xdtsButton, 0);
    lay->addWidget(folderButton, 0);
    lay->addWidget(logButton, 0);
  }
  container->setProperty("PPM_Row", r);
  container->setLayout(lay);
  setCellWidget(r, col++, container);

  // 通常の場合
  if (info.paths.size() == 1) {
    openButton->setProperty("PPM_FilePath", info.paths[0]);
    openButton->setProperty("PPM_Row", r);
    if (info.paths[0].endsWith(".tvpp") || info.paths[0].endsWith(".clip") ||
        info.paths[0].endsWith(".tnz"))
      connect(openButton, SIGNAL(clicked()), this, SLOT(onOpenButtonClicked()));
    else if (
        !setSpecialOpenButton(
            openButton,
            info))  // 紙LOの場合はサブフォルダのTNZを開くメニューを用意する
      connect(openButton, SIGNAL(clicked()), this,
              SLOT(onFolderButtonClicked()));
  }
  // 兼用カットで、複数のキューファイルがある場合
  else {
    QMenu* menu = new QMenu();
    int i       = 0;
    for (auto cueFilePath : info.paths) {
      // std::cout << cueFilePath.toStdString() << std::endl;
      assert(cueFilePath.endsWith(".tnz"));
      QAction* openCueFileAction = menu->addAction(info.fileNames[i]);
      openCueFileAction->setProperty("PPM_FilePath", cueFilePath);
      openCueFileAction->setProperty("PPM_Row", r);
      connect(openCueFileAction, SIGNAL(triggered()), this,
              SLOT(onOpenButtonClicked()));
      i++;
    }
    openButton->setMenu(menu);
  }

  // Enable / disable, アクション実装
  // xdts path
  QStringList xdtsFileNames;
  bool ambiguousXdts    = false;
  QStringList xdtsPaths = WorkflowManager::instance()->xdtsPaths(
      wf.groupId, info.sequence, info.sceneNumber, xdtsFileNames, false,
      info.paths, &ambiguousXdts);
  if (ambiguousXdts)
    xdtsButton->setToolTip(
        tr("Multiple file names have the same Unicode spelling."));
  // std::cout << "xdtsPath = " << xdtsPath.toStdString() << std::endl;
  if (xdtsPaths.isEmpty())
    xdtsButton->setDisabled(true);
  else if (xdtsPaths.size() == 1) {
    xdtsButton->setProperty("PPM_FilePath", xdtsPaths[0]);
    connect(xdtsButton, SIGNAL(clicked()), this,
            SLOT(onOpenXdtsButtonClicked()));
  }
  // 複数のXDTSファイルがある場合
  else {
    QMenu* menu = new QMenu();
    int i       = 0;
    for (auto xdtsPath : xdtsPaths) {
      QAction* xdtsPathAction = menu->addAction(xdtsFileNames[i]);
      xdtsPathAction->setProperty("PPM_FilePath", xdtsPath);
      connect(xdtsPathAction, SIGNAL(triggered()), this,
              SLOT(onOpenXdtsButtonClicked()));
      i++;
    }
    xdtsButton->setMenu(menu);
  }

  // folder
  folderButton->setProperty("PPM_FilePath", info.paths[0]);
  connect(folderButton, SIGNAL(clicked()), this, SLOT(onFolderButtonClicked()));
  // log
  QString logFilePath =
      WorkflowManager::instance()->logFilePath(info.sequence, info.sceneNumber);
  if (QFileInfo(logFilePath).exists()) {
    logButton->setProperty("PPM_FilePath", logFilePath);
    connect(logButton, SIGNAL(clicked()), this, SLOT(onOpenLogButtonClicked()));
  } else
    logButton->setDisabled(true);

  // col2 :  CheckboxId (Hidden Column, 項目のソートに用いる)
  setItem(r, col++,
          new MyItem(QString::number(checkboxId).rightJustified(4, '0')));

  // col3 : Name
  QTableWidgetItem* nameItem = new MyItem(wf.name);
  nameItem->setIcon(color2Icon(wf.color));
  setItem(r, col++, nameItem);

  // col4 :  Media Type
  static QStringList MediaLabelTxts = {
      tr("Digital"), tr("Paper"),
      tr("Other")  // Other Type
  };

  QTableWidgetItem* mediaTypeItem =
      new MyItem(MediaLabelTxts[protocol.mediaType]);
  setItem(r, col++, mediaTypeItem);

  // Thumbnail（Scene Thumbnail
  // Pathが未設定のプロジェクトではinfo.thumbnailPathは常に空文字列）
  // 画像の読み込みはQtConcurrentで非同期に行い、完了後にメインスレッドでラベルへ反映する。
  // 読み込み完了時には行が既に作り直されている場合があるため、QPointerで安全に破棄チェックする
  if (!info.thumbnailPath.isEmpty()) {
    ThumbnailLabel* thumbLabel = new ThumbnailLabel(this);
    setCellWidget(r, col, thumbLabel);

    QPointer<ThumbnailLabel> labelPtr(thumbLabel);
    QPointer<MyTable> tablePtr(this);
    QFutureWatcher<QImage>* watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this,
            [labelPtr, tablePtr, watcher]() {
              QImage image = watcher->result();
              watcher->deleteLater();
              if (!labelPtr || !tablePtr)
                return;  // 行が既に破棄されている場合は何もしない
              labelPtr->setImage(image, tablePtr->currentThumbnailRowHeight());
            });
    watcher->setFuture(
        QtConcurrent::run(loadThumbnailImage, info.thumbnailPath));
  }
  col++;

  // Episode（話数機能を使わないプロジェクトではinfo.episodeは常に空文字列）
  QTableWidgetItem* episodeItem = new MyItem(info.episode);
  setItem(r, col++, episodeItem);

  // col5 : Scene Number
  QFont fnt;
  fnt.setPointSize(20);
  fnt.setFamily("Arial");
  QTableWidgetItem* sceneNumberItem =
      new MyItem(info.sceneNumber + info.takeSuffix);
  sceneNumberItem->setFont(fnt);
  setItem(r, col++, sceneNumberItem);

  // Take
  // QTableWidgetItem* takeNumberItem = new
  // MyItem(tr("t%1").arg(info.takeNumber)); QFont takeFnt;
  // takeFnt.setPointSize(12);
  // takeFnt.setFamily("Arial");
  // takeNumberItem->setFont(takeFnt);
  // if (info.takeNumber == 1)
  //   takeNumberItem->setForeground(Qt::gray);
  // takeNumberItem->setTextAlignment(Qt::AlignCenter);
  // setItem(r, col++, takeNumberItem);

  // col6 : Last Modified
  QString dateTimeStr;
  // 同じ日なら時間を表示
  if (QDateTime::currentDateTime().date() == info.lastModifiedTime.date())
    dateTimeStr = info.lastModifiedTime.toString(tr("hh:mm"));
  // 同じ年なら日だけ表示
  else if (QDateTime::currentDateTime().date().year() ==
           info.lastModifiedTime.date().year())
    dateTimeStr = info.lastModifiedTime.toString(tr("M/d"));
  // それ以外は年月日を表示
  else
    dateTimeStr = info.lastModifiedTime.toString(tr("yyyy/MM/dd"));

  setItem(r, col++, new MyItem(info.lastModifiedTime.toString(dateTimeStr)));

  // std::cout << "register time of row" << r << " name=" <<
  // wf.name.toStdString() << std::endl;

  // col7 : Done Button
  QStackedWidget* stackedContainer = new QStackedWidget(this);
  container                        = new QFrame;
  lay                              = new QHBoxLayout();
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(5);

  // 工程内リテイク(区別のためRewindと名付ける)機能を復活させる。
  // Rewindできる条件は ① FlowGroupがRewind表記の工程で ② Digitalであること
  bool isRewindable =
      WorkflowManager::instance()
              ->flowGroupInfos()
              .value(wf.groupId)
              .takeNotationStyle == FlowGroupInfo::TakeNotation_Rewind &&
      protocol.mediaType == Digital && info.paths.size() == 1;
  if (isRewindable) {
    QPushButton* rewindButton =
        new QPushButton(QIcon(":Resources/retake.svg"), "", this);
    rewindButton->setFixedSize(23, 23);
    rewindButton->setIconSize(QSize(16, 16));
    rewindButton->setToolTip(tr("Rewind"));
    rewindButton->setProperty("PPM_FilePath", info.paths[0]);
    rewindButton->setProperty("PPM_Episode", info.episode);
    rewindButton->setProperty("PPM_Sequence", info.sequence);
    rewindButton->setProperty("PPM_SceneNumber", info.sceneNumber);
    rewindButton->setProperty("PPM_WorkflowType", info.workflowType);
    rewindButton->setProperty("PPM_ProtocolId", info.protocolId);

    connect(rewindButton, SIGNAL(clicked()), this,
            SLOT(onRewindButtonClicked()));
    lay->addWidget(rewindButton, 0);
    lay->addSpacing(2);
  }

  for (int actionId = 0; actionId < protocol.actions.size(); actionId++) {
    PPM_Action action   = protocol.actions.at(actionId);
    QPushButton* button = new QPushButton(action.actionName, this);
    if (action.steps.isEmpty())  // 押しても何もしない、ダミー工程のボタン用
      button->setEnabled(false);
    else {
      button->setProperty("PPM_FilePaths", info.paths);
      button->setProperty("PPM_Episode", info.episode);
      button->setProperty("PPM_Sequence", info.sequence);
      button->setProperty("PPM_SceneNumber", info.sceneNumber);
      button->setProperty("PPM_WorkflowType", info.workflowType);
      button->setProperty("PPM_ProtocolId", info.protocolId);
      button->setProperty("PPM_ActionId", actionId);
      // 工程完了アクションでは
      // - 元データを消さない
      // - 移動するファイルを選べるようにする
      // if (!wf.moveFlowTo.isEmpty() && wf.moveFlowTo.at(actionId) != None)
      //   connect(button, SIGNAL(clicked()), this,
      //   SLOT(onFlowCompleteButtonClicked()));
      // else
      connect(button, SIGNAL(clicked()), this, SLOT(onActionButtonClicked()));
    }
    lay->addWidget(button, 1);
  }

  container->setLayout(lay);
  stackedContainer->addWidget(container);

  // ロックする
  stackedContainer->setProperty("PPM_WorkflowType", info.workflowType);
  stackedContainer->setProperty("PPM_ProtocolId", info.protocolId);
  stackedContainer->setDisabled(isLocked);
  if (isLocked)
    stackedContainer->setToolTip(tr("Action on this workflow is locked."));

  setCellWidget(r, col++, stackedContainer);

  // col8 : Information
  QString infoTxt = WorkflowManager::instance()->getSceneInfoTagsRichText(
      info.sequence, info.sceneNumber);
  if (!infoTxt.isEmpty()) {
    QLabel* label = new QLabel(infoTxt, this);
    label->setTextFormat(Qt::RichText);
    setCellWidget(r, col++, label);
  }

  setRowHeight(r, currentThumbnailRowHeight());
}

void MyTable::toggleLocks(WorkflowType wfType, bool lock) {
  for (int r = 0; r < rowCount(); r++) {
    QWidget* widget = cellWidget(r, COLUMNINDEX_ACTIONS);  // Action 列
    if (wfType == widget->property("PPM_WorkflowType").toInt()) {
      widget->setDisabled(lock);
      widget->setToolTip((lock) ? tr("Action on this workflow is locked.")
                                : QString());

      if (cellWidget(r, 0)) {  // まとめてCheck
        QCheckBox* checkBox = dynamic_cast<QCheckBox*>(cellWidget(r, 0));
        if (lock && checkBox->isChecked()) {
          checkBox->setChecked(false);
          onCheckBoxClicked(false);
        }
        checkBox->setDisabled(lock);
        checkBox->setToolTip(
            (lock) ? tr("The checkbox to select this workflow is locked.")
                   : QString());
      }
    }
  }
}

//----------------------------------------------
// 開くボタンに特殊な操作を追加した場合はtrueを返す。
// FlowGroupInfo::subfolderForOpenButtonが設定されている工程グループの場合、
// キューファイル自身ではなく、その相対パスで示されるサブフォルダ内のファイルを開けるようにする
bool MyTable::setSpecialOpenButton(QPushButton* openButton, DataInfo info) {
  Workflow wf = WorkflowManager::instance()->workflow(info.workflowType);

  FlowGroupInfo flowGroupInfo =
      WorkflowManager::instance()->flowGroupInfos().value(wf.groupId);
  if (flowGroupInfo.subfolderForOpenButton.isEmpty()) return false;

  // サブフォルダ名に含まれるマクロをデコードする
  QString subDirName = WorkflowManager::instance()->decodePathMacros(
      flowGroupInfo.subfolderForOpenButton);
  subDirName.replace("[PREFIX]",
                     WorkflowManager::instance()->cutNumberPrefix());
  subDirName.replace("[CUTNUMBER]", info.sceneNumber);
  if (subDirName.contains("[SEQUENCE]"))
    subDirName.replace("[SEQUENCE]", info.sequence);
  subDirName = WorkflowManager::collapseSeparators(subDirName);

  QDir dir = QFileInfo(info.paths[0]).dir();
  QString local = WorkflowManager::instance()->resolveProjectPath(subDirName);
  if (local.isEmpty()) return false;
  QString subfolder = SharedFiles::actualPath(dir.filePath(local));
  if (subfolder.isEmpty() || !dir.cd(subfolder)) return false;
  if (!dir.exists()) return false;
  QStringList filters;
  filters << "*.clip" << "*.tvpp" << "*.tnz";
  QStringList files =
      dir.entryList(filters, QDir::Files | QDir::NoDotAndDotDot);

  if (files.isEmpty()) return false;

  if (files.size() == 1) {
    openButton->setProperty("PPM_FilePath", dir.filePath(files[0]));
    connect(openButton, SIGNAL(clicked()), this, SLOT(onOpenButtonClicked()));
  } else {
    int r       = openButton->property("PPM_Row").toInt();
    QMenu* menu = new QMenu();
    for (auto file : files) {
      QString path            = dir.filePath(file);
      QAction* openFileAction = menu->addAction(file);
      openFileAction->setProperty("PPM_FilePath", path);
      openFileAction->setProperty("PPM_Row", r);
      connect(openFileAction, SIGNAL(triggered()), this,
              SLOT(onOpenButtonClicked()));
    }
    openButton->setMenu(menu);
  }

  return true;
}

//----------------------------------------------
// 整列前の列ID（ppm_row）から現在の列を探す
int MyTable::findRow(int ppm_row, int col) {
  for (int r = 0; r < rowCount(); r++) {
    if (cellWidget(r, col) &&
        cellWidget(r, col)->property("PPM_Row").toInt() == ppm_row)
      return r;
  }
  return -1;
}

//----------------------------------------------

void MyTable::onOpenButtonClicked() {
  QString path = sender()->property("PPM_FilePath").toString();
  int ppm_row  = sender()->property("PPM_Row").toInt();
  // 整列前の列ID（ppm_row）から現在の列を探す
  int row = findRow(ppm_row);
  if (row == -1) return;

  QString program = (path.endsWith(".tvpp"))
                        ? UserSettings::instance()->getTvpBinaryPath()
                    : (path.endsWith(".clip"))
                        ? UserSettings::instance()->getCspBinaryPath()
                        : UserSettings::instance()->getOpenToonzBinaryPath();

  if (program.trimmed().isEmpty()) {
    // アプリのパスが未設定の場合は、拡張子に関連付けられた既定のアプリで開く
    // （OpenToonzの引数は使わない）
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
      showOpenWithDefaultAppFailedMessage(path);
      return;
    }
  } else {
    QStringList arguments;
    // OpenToonzの引数は、ファイルのパスの前に渡す。
    // QProcess::startDetached()はprogram全体を実行ファイルのパスとして扱うため、
    // 引数はprogramに連結せず、argumentsに分けて入れる（"..."で囲んだ空白入りの引数も扱える）
    const bool isToonzScene = path.endsWith(".tnz");
    if (isToonzScene &&
        !UserSettings::instance()->getOpenToonzArgument().trimmed().isEmpty()) {
      arguments << QProcess::splitCommand(
          UserSettings::instance()->getOpenToonzArgument());
    }
    const auto mode = isToonzScene ? Platform::OpenMode::CommandLine
                                   : Platform::OpenMode::Document;
    if (!Platform::openFile(program, QDir::toNativeSeparators(path), mode,
                            arguments)) {
      showStartProgramFailedMessage(program, path);
      return;
    }
  }
  // 開いた列を黄色くする（開けなかった場合は、上でreturnしている）
  for (int c = 0; c < columnCount(); c++) {
    QTableWidgetItem* cellItem = item(row, c);
    if (cellItem)
      cellItem->setBackground(QColor(255, 253, 201));
    else {
      QWidget* widget = cellWidget(row, c);
      if (widget)
        widget->setStyleSheet("QFrame { background-color: rgb(255,253,201); }");
    }
  }
}

//----------------------------------------------

void MyTable::onOpenXdtsButtonClicked() {
  QString path = sender()->property("PPM_FilePath").toString();

  QString program = UserSettings::instance()->getXdtsViewerBinaryPath();
  if (program.trimmed().isEmpty()) {
    // アプリのパスが未設定の場合は、拡張子に関連付けられた既定のアプリで開く
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
      showOpenWithDefaultAppFailedMessage(path);
    return;
  }
  if (!Platform::openFile(program, path, Platform::OpenMode::CommandLine))
    showStartProgramFailedMessage(program, path);
}

//----------------------------------------------

void MyTable::showOpenWithDefaultAppFailedMessage(const QString& path) {
  QMessageBox::warning(
      this, tr("Warning"),
      tr("Could not open the file with the application associated with its "
         "extension:\n%1\n\n"
         "Check that an application is associated with the extension, or set "
         "the path of the application in Preferences.")
          .arg(QDir::toNativeSeparators(path)));
}

void MyTable::showStartProgramFailedMessage(const QString& program,
                                            const QString& path) {
  QMessageBox::warning(
      this, tr("Warning"),
      tr("Could not start the application:\n%1\n\n"
         "File to open:\n%2\n\n"
         "Check the path of the application in Preferences.")
          .arg(QDir::toNativeSeparators(program),
               QDir::toNativeSeparators(path)));
}

//----------------------------------------------

void MyTable::onFolderButtonClicked() {
  QString path    = sender()->property("PPM_FilePath").toString();
  QString dirPath = QFileInfo(path).absoluteDir().path();
  std::cout << "path = " << path.toStdString() << std::endl;
  std::cout << "dirPath = " << dirPath.toStdString() << std::endl;
  QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
}

//----------------------------------------------

void MyTable::onOpenLogButtonClicked() {
  QString path = sender()->property("PPM_FilePath").toString();
  QFile f(path);
  if (!f.open(QFile::ReadOnly | QFile::Text)) return;
  // QTextStream in(&f);

  // QTextCodec* codec = QTextCodec::codecForName("Shift-JIS");
  // QByteArray encodedString = f.readAll();

  QTextEdit* te = new QTextEdit(this);
  te->setWindowFlags(Qt::Tool | Qt::Dialog);
  te->setMarkdown(f.readAll());
  te->show();
}

//----------------------------------------------

void MyTable::onActionButtonClicked() {
  int yPos = dynamic_cast<QWidget*>(sender()->parent()->parent())
                 ->geometry()
                 .center()
                 .y();
  int row = rowAt(yPos);

  // 複数選択のチェックボックスがONになっているかどうかで挙動を分ける
  QCheckBox* multiCb = dynamic_cast<QCheckBox*>(cellWidget(row, 0));
  if (multiCb && multiCb->isChecked()) {
    std::cout << "This is multi!" << std::endl;
    if (onActionMulti()) return;
  }

  QStringList orgPaths = sender()->property("PPM_FilePaths").toStringList();
  QString episode      = sender()->property("PPM_Episode").toString();
  QString sequence     = sender()->property("PPM_Sequence").toString();
  QString sceneNumber  = sender()->property("PPM_SceneNumber").toString();
  WorkflowType workflowType =
      static_cast<WorkflowType>(sender()->property("PPM_WorkflowType").toInt());
  int protocolId = sender()->property("PPM_ProtocolId").toInt();
  int actionId   = sender()->property("PPM_ActionId").toInt();

  std::cout << "onActionButtonClicked" << std::endl
            << " orgPath = " << orgPaths[0].toStdString() << std::endl
            << " sequence = " << sequence.toStdString() << std::endl
            << " sceneNumber = " << sceneNumber.toStdString() << std::endl
            << " row = " << row << std::endl;

  // All
  // Episodesモードではこの行が所属する話数へ切り替えてから実行する必要がある
  // （ConfirmationDialogのコンストラクタ内でActionInfo::episodeを見て再度設定されるが、
  //   ダイアログ構築前のこのタイミングでのWorkflowManager呼び出しのためにも念のため合わせておく）
  WorkflowManager::instance()->setCurrentEpisode(episode);

  ActionConfirmationDialog dialog(
      {workflowType, protocolId, orgPaths, sequence, sceneNumber, episode},
      actionId, false);

  Workflow wf = WorkflowManager::instance()->workflow(workflowType);
  WorkflowProtocol protocol = wf.protocols.at(protocolId);
  QString actionName        = protocol.actions[actionId].actionName;

  int ret = dialog.exec();
  if (dialog.isCanceled()) return;
  if (!ret) {
    WorkflowManager::instance()->logAction(sequence, sceneNumber, wf.name,
                                           tr("[FAILED] ") + actionName);
    return;
  }

  // ログを記録する
  WorkflowManager::instance()->logAction(sequence, sceneNumber, wf.name,
                                         actionName);

  // COLUMNINDEX_ACTIONS以外をDisable
  for (int c = 0; c < columnCount(); c++) {
    QTableWidgetItem* cellItem = item(row, c);
    if (cellItem) cellItem->setBackground(Qt::lightGray);
    QWidget* widget = cellWidget(row, c);
    if (widget) {
      widget->setStyleSheet("QFrame { background-color: lightgray; }");
      if (c != COLUMNINDEX_ACTIONS) widget->setDisabled(true);
    }
  }
  // COLUMNINDEX_ACTIONSの列(※決め打ち)はアクションボタン。StackedWidgetを切り替える

  QStackedWidget* stackedWidget =
      dynamic_cast<QStackedWidget*>(cellWidget(row, COLUMNINDEX_ACTIONS));
  if (!stackedWidget) return;

  QList<int> googleStepPos          = dialog.googleStepPos();
  bool hasGoogleStep                = !googleStepPos.isEmpty();
  QVariantList googleStepPosVariant = toVariantList(googleStepPos);

  if (stackedWidget->count() == 1) {
    // キャンセルボタンを作成
    AsynchronousUndoButton* undoButton =
        new AsynchronousUndoButton(hasGoogleStep, this);
    stackedWidget->addWidget(undoButton);
    undoButton->setProperty("PPM_FilePaths", orgPaths);
    undoButton->setProperty("PPM_Episode", episode);
    undoButton->setProperty("PPM_Sequence", sequence);
    undoButton->setProperty("PPM_SceneNumber", sceneNumber);
    undoButton->setProperty("PPM_WorkflowType", workflowType);
    undoButton->setProperty("PPM_ProtocolId", protocolId);
    undoButton->setProperty("PPM_ActionId", actionId);
    undoButton->setProperty("PPM_FileCheckSums", dialog.fileCheckSums()[0]);
    undoButton->setProperty("PPM_GoogleStepPos", googleStepPosVariant);
    undoButton->setProperty("PPM_PendingGoogleStepPos", googleStepPosVariant);
    undoButton->setProperty("PPM_HasGoogleFailure", false);
    connect(undoButton, SIGNAL(clicked()), this, SLOT(onUndoButtonClicked()));
    stackedWidget->addWidget(undoButton);
  } else {
    if (hasGoogleStep) {
      AsynchronousUndoButton* undoButton =
          dynamic_cast<AsynchronousUndoButton*>(stackedWidget->widget(1));
      if (undoButton) undoButton->setWaitState();
    }
    // ActionId プロパティだけ切り替える
    stackedWidget->widget(1)->setProperty("PPM_ActionId", actionId);
    stackedWidget->widget(1)->setProperty("PPM_FileCheckSums",
                                          dialog.fileCheckSums()[0]);
    stackedWidget->widget(1)->setProperty("PPM_GoogleStepPos",
                                          googleStepPosVariant);
    stackedWidget->widget(1)->setProperty("PPM_PendingGoogleStepPos",
                                          googleStepPosVariant);
    stackedWidget->widget(1)->setProperty("PPM_HasGoogleFailure", false);
  }

  // リネーム先が無い場合は取り消しできない
  // stackedWidget->widget(1)->setDisabled(renameToList.isEmpty());

  stackedWidget->setCurrentIndex(1);  // Undoボタンのページに切り替える

  QString messageTxt = tr("Action Completed.");
  QMessageBox::information(this, tr("Information"), messageTxt);
}

//----------------------------------------------
// 複数選択の場合の挙動
bool MyTable::onActionMulti() {
  QList<ActionInfo> actionInfos;
  QList<int> rows;
  // チェックされたチェックボックスを探索
  for (int row = 0; row < rowCount(); row++) {
    QCheckBox* multiCb = dynamic_cast<QCheckBox*>(cellWidget(row, 0));
    if (multiCb && multiCb->isChecked()) {
      QStringList orgPaths = multiCb->property("PPM_FilePaths").toStringList();
      QString episode      = multiCb->property("PPM_Episode").toString();
      QString sequence     = multiCb->property("PPM_Sequence").toString();
      QString sceneNumber  = multiCb->property("PPM_SceneNumber").toString();
      WorkflowType workflowType = static_cast<WorkflowType>(
          multiCb->property("PPM_WorkflowType").toInt());
      int protocolId = multiCb->property("PPM_ProtocolId").toInt();
      actionInfos.append(
          {workflowType, protocolId, orgPaths, sequence, sceneNumber, episode});
      rows.append(row);
    }
  }

  // チェックボックスが１つだけONのときはfalseを返し、
  // 通常のonActionButtonClickedの処理を続ける
  if (actionInfos.size() == 1) {
    // チェックボックス解除
    dynamic_cast<QCheckBox*>(cellWidget(rows[0], 0))->setChecked(false);
    return false;
  }

  ActionConfirmationDialog dialog(actionInfos);

  int ret = dialog.exec();
  if (dialog.isCanceled()) return true;

  bool isSomethingFailed = !ret;

  QList<int> failedIndex = dialog.failedActionIndexes();
  // それぞれのアクションごとに処理をする
  for (int i = 0; i < actionInfos.size(); i++) {
    ActionInfo actionInfo = actionInfos.at(i);
    int row               = rows.at(i);

    WorkflowType workflowType = actionInfo.workflowType;
    int protocolId            = actionInfo.protocolId;
    // int workflowId = actionInfo.workflowId;
    QStringList orgPaths = actionInfo.orgPaths;
    QString sequence     = actionInfo.sequence;
    QString sceneNumber  = actionInfo.sceneNumber;
    Workflow wf          = WorkflowManager::instance()->workflow(workflowType);
    WorkflowProtocol protocol = wf.protocols.at(protocolId);
    QString actionName        = protocol.actions[0].actionName;

    // ダイアログ構築時、各ActionInfoの処理ごとにWorkflowManagerの現在の話数が
    // 切り替わっているため（actionconfirmationdialog.cpp参照）、ここでこの行の話数に戻す
    WorkflowManager::instance()->setCurrentEpisode(actionInfo.episode);

    if (failedIndex.contains(i)) {
      assert(isSomethingFailed);
      WorkflowManager::instance()->logAction(sequence, sceneNumber, wf.name,
                                             tr("[FAILED] ") + actionName);
      continue;
    }

    // ログを記録する
    WorkflowManager::instance()->logAction(sequence, sceneNumber, wf.name,
                                           actionName);

    // COLUMNINDEX_ACTIONS以外をDisable
    for (int c = 0; c < columnCount(); c++) {
      QTableWidgetItem* cellItem = item(row, c);
      if (cellItem) cellItem->setBackground(Qt::lightGray);
      QWidget* widget = cellWidget(row, c);
      if (widget) {
        widget->setStyleSheet("QFrame { background-color: lightgray; }");
        if (c != COLUMNINDEX_ACTIONS) widget->setDisabled(true);

        // チェックボックス解除
        if (c == 0) dynamic_cast<QCheckBox*>(widget)->setChecked(false);
      }
    }
    // COLUMNINDEX_ACTIONSの列(※決め打ち)はアクションボタン。StackedWidgetを切り替える

    QStackedWidget* stackedWidget =
        dynamic_cast<QStackedWidget*>(cellWidget(row, COLUMNINDEX_ACTIONS));
    if (!stackedWidget) continue;

    QList<int> googleStepPos          = dialog.googleStepPos(i);
    bool hasGoogleStep                = !googleStepPos.isEmpty();
    QVariantList googleStepPosVariant = toVariantList(googleStepPos);

    if (stackedWidget->count() == 1) {
      // キャンセルボタンを作成
      AsynchronousUndoButton* undoButton =
          new AsynchronousUndoButton(hasGoogleStep, this);
      stackedWidget->addWidget(undoButton);
      undoButton->setProperty("PPM_FilePaths", orgPaths);
      undoButton->setProperty("PPM_Episode", actionInfo.episode);
      undoButton->setProperty("PPM_Sequence", sequence);
      undoButton->setProperty("PPM_SceneNumber", sceneNumber);
      undoButton->setProperty("PPM_WorkflowType", workflowType);
      undoButton->setProperty("PPM_ProtocolId", protocolId);
      undoButton->setProperty("PPM_ActionId", 0);
      undoButton->setProperty("PPM_FileCheckSums", dialog.fileCheckSums()[i]);
      undoButton->setProperty("PPM_GoogleStepPos", googleStepPosVariant);
      undoButton->setProperty("PPM_PendingGoogleStepPos", googleStepPosVariant);
      undoButton->setProperty("PPM_HasGoogleFailure", false);
      connect(undoButton, SIGNAL(clicked()), this, SLOT(onUndoButtonClicked()));
      stackedWidget->addWidget(undoButton);
    } else {
      if (hasGoogleStep) {
        AsynchronousUndoButton* undoButton =
            dynamic_cast<AsynchronousUndoButton*>(stackedWidget->widget(1));
        if (undoButton) undoButton->setWaitState();
      }
      // ActionId プロパティだけ切り替える
      stackedWidget->widget(1)->setProperty("PPM_ActionId", 0);
      stackedWidget->widget(1)->setProperty("PPM_FileCheckSums",
                                            dialog.fileCheckSums()[i]);
      stackedWidget->widget(1)->setProperty("PPM_GoogleStepPos",
                                            googleStepPosVariant);
      stackedWidget->widget(1)->setProperty("PPM_PendingGoogleStepPos",
                                            googleStepPosVariant);
      stackedWidget->widget(1)->setProperty("PPM_HasGoogleFailure", false);
    }

    stackedWidget->setCurrentIndex(1);  // Undoボタンのページに切り替える
  }

  QString messageTxt = tr("Action Completed.");
  if (isSomethingFailed) messageTxt += tr(" Some action(s) have failed.");
  QMessageBox::information(this, tr("Information"), messageTxt);

  return true;
}

//----------------------------------------------

void MyTable::onRewindButtonClicked() {
  QString orgPath     = sender()->property("PPM_FilePath").toString();
  QString episode     = sender()->property("PPM_Episode").toString();
  QString sequence    = sender()->property("PPM_Sequence").toString();
  QString sceneNumber = sender()->property("PPM_SceneNumber").toString();
  WorkflowType workflowType =
      static_cast<WorkflowType>(sender()->property("PPM_WorkflowType").toInt());
  int protocolId = sender()->property("PPM_ProtocolId").toInt();

  int yPos = dynamic_cast<QWidget*>(sender()->parent()->parent())
                 ->geometry()
                 .center()
                 .y();
  int row = rowAt(yPos);

  // All Episodesモードではこの行が所属する話数へ切り替えてから実行する
  WorkflowManager::instance()->setCurrentEpisode(episode);

  RewindDestinationDialog distDialog(workflowType, protocolId);

  if (!distDialog.exec()) return;

  QPair<WorkflowType, int> destinationWfTypePair =
      distDialog.destinationWorkflow();
  Workflow destinationWf =
      WorkflowManager::instance()->workflow(destinationWfTypePair.first);

  QString actionName = tr("Rewind: Return to %1").arg(destinationWf.name);

  RewindConfirmationDialog dialog(
      {workflowType, protocolId, {orgPath}, sequence, sceneNumber, episode},
      destinationWfTypePair, false);

  Workflow wf = WorkflowManager::instance()->workflow(workflowType);
  int ret     = dialog.exec();
  if (dialog.isCanceled()) return;
  if (!ret) {
    WorkflowManager::instance()->logAction(sequence, sceneNumber, wf.name,
                                           tr("[FAILED] ") + actionName);
    return;
  }

  // ログを記録する
  WorkflowManager::instance()->logAction(sequence, sceneNumber, wf.name,
                                         actionName);

  // COLUMNINDEX_ACTIONS以外をDisable
  for (int c = 0; c < columnCount(); c++) {
    QTableWidgetItem* cellItem = item(row, c);
    if (cellItem) cellItem->setBackground(Qt::lightGray);
    QWidget* widget = cellWidget(row, c);
    if (widget) {
      widget->setStyleSheet("QFrame { background-color: lightgray; }");
      if (c != COLUMNINDEX_ACTIONS) widget->setDisabled(true);
    }
  }
  // COLUMNINDEX_ACTIONSの列(※決め打ち)はアクションボタン。StackedWidgetを切り替える

  QStackedWidget* stackedWidget =
      dynamic_cast<QStackedWidget*>(cellWidget(row, COLUMNINDEX_ACTIONS));
  if (!stackedWidget) return;

  if (stackedWidget->count() == 1) {
    // キャンセルボタンを作成
    QPushButton* undoButton = new QPushButton(tr("Undo"), this);
    stackedWidget->addWidget(undoButton);
    QStringList orgPaths = {orgPath};
    undoButton->setProperty("PPM_FilePaths", orgPaths);
    undoButton->setProperty("PPM_Episode", episode);
    undoButton->setProperty("PPM_Sequence", sequence);
    undoButton->setProperty("PPM_SceneNumber", sceneNumber);
    undoButton->setProperty("PPM_WorkflowType", workflowType);
    undoButton->setProperty("PPM_ProtocolId", protocolId);
    undoButton->setProperty("PPM_DestinationWorkflowType",
                            destinationWfTypePair.first);
    undoButton->setProperty("PPM_DestinationProtocolId",
                            destinationWfTypePair.second);
    undoButton->setProperty("PPM_ActionId", RewindActionId);  //-1がRewindを示す
    undoButton->setProperty("PPM_FileCheckSums", dialog.fileCheckSums());
    connect(undoButton, SIGNAL(clicked()), this, SLOT(onUndoButtonClicked()));
    stackedWidget->addWidget(undoButton);
  } else {
    // ActionId プロパティだけ切り替える
    stackedWidget->widget(1)->setProperty("PPM_ActionId",
                                          RewindActionId);  //-1がRewindを示す
    stackedWidget->widget(1)->setProperty("PPM_FileCheckSums",
                                          dialog.fileCheckSums());
    stackedWidget->widget(1)->setProperty("PPM_DestinationWorkflowType",
                                          destinationWfTypePair.first);
    stackedWidget->widget(1)->setProperty("PPM_DestinationProtocolId",
                                          destinationWfTypePair.second);
  }

  // リネーム先が無い場合は取り消しできない
  // stackedWidget->widget(1)->setDisabled(renameToList.isEmpty());

  stackedWidget->setCurrentIndex(1);  // Undoボタンのページに切り替える
}

//----------------------------------------------

void MyTable::onUndoButtonClicked() {
  QStringList orgPaths = sender()->property("PPM_FilePaths").toStringList();
  QString episode      = sender()->property("PPM_Episode").toString();
  QString sequence     = sender()->property("PPM_Sequence").toString();
  QString sceneNumber  = sender()->property("PPM_SceneNumber").toString();
  WorkflowType workflowType =
      static_cast<WorkflowType>(sender()->property("PPM_WorkflowType").toInt());
  int protocolId = sender()->property("PPM_ProtocolId").toInt();

  int actionId  = sender()->property("PPM_ActionId").toInt();
  bool isRewind = actionId == RewindActionId;
  QPair<WorkflowType, int> destinationWfTypePair;
  if (isRewind) {
    destinationWfTypePair.first = static_cast<WorkflowType>(
        sender()->property("PPM_DestinationWorkflowType").toInt());
    destinationWfTypePair.second =
        sender()->property("PPM_DestinationProtocolId").toInt();
  }

  QList<QVariant> fileCheckSums =
      sender()->property("PPM_FileCheckSums").toList();
  int yPos =
      dynamic_cast<QWidget*>(sender()->parent())->geometry().center().y();
  int row = rowAt(yPos);

  // All Episodesモードではこの行が所属する話数へ切り替えてから実行する
  WorkflowManager::instance()->setCurrentEpisode(episode);

  // RewindのUndoの場合はRewindダイアログを開く。
  ConfirmationDialog* dialog = nullptr;
  if (isRewind)
    dialog = new RewindConfirmationDialog({workflowType,
                                           protocolId,
                                           {orgPaths[0]},
                                           sequence,
                                           sceneNumber,
                                           episode},
                                          destinationWfTypePair, true,
                                          fileCheckSums);
  else
    dialog = new ActionConfirmationDialog(
        {workflowType, protocolId, orgPaths, sequence, sceneNumber, episode},
        actionId, true, fileCheckSums);

  Workflow wf = WorkflowManager::instance()->workflow(workflowType);
  WorkflowProtocol protocol = wf.protocols.at(protocolId);
  QString actionName;
  if (isRewind) {
    Workflow destinationWf =
        WorkflowManager::instance()->workflow(destinationWfTypePair.first);
    actionName = tr("Rewind: Return to %1").arg(destinationWf.name);
  } else
    actionName = protocol.actions[actionId].actionName;

  int ret = dialog->exec();
  if (dialog->isCanceled()) {
    delete dialog;
    return;
  }
  if (!ret) {
    WorkflowManager::instance()->logAction(sequence, sceneNumber, wf.name,
                                           tr("[UNDO FAILED] ") + actionName);
    delete dialog;
    return;
  }

  delete dialog;

  // ログを記録する
  WorkflowManager::instance()->logAction(sequence, sceneNumber, wf.name,
                                         tr("[UNDO] ") + actionName);
  // COLUMNINDEX_ACTIONS以外をEnable
  for (int c = 0; c < columnCount(); c++) {
    QTableWidgetItem* cellItem = item(row, c);
    if (cellItem) cellItem->setBackground(Qt::white);
    QWidget* widget = cellWidget(row, c);
    if (widget) {
      widget->setStyleSheet("QFrame { background-color: white; }");
      if (c != COLUMNINDEX_ACTIONS) widget->setEnabled(true);
    }
  }
  // 最後の列(※決め打ち)はアクションボタン。StackedWidgetを切り替える
  QStackedWidget* stackedWidget =
      dynamic_cast<QStackedWidget*>(cellWidget(row, COLUMNINDEX_ACTIONS));
  if (!stackedWidget) return;

  stackedWidget->setCurrentIndex(0);  // Actionボタンのページに切り替える

  if (!isRewind) {
    QString messageTxt = tr("Undoing Completed.");
    QMessageBox::information(this, tr("Information"), messageTxt);
  }
}

void MyTable::onCheckBoxClicked(bool on) {
  int ppm_row = sender()->property("PPM_Row").toInt();

  QColor col = (on) ? QColor(194, 219, 255) : QColor(Qt::white);

  // 整列前の列ID（ppm_row）から現在の列を探す
  int row = findRow(ppm_row, 0);
  if (row == -1) return;

  // チェックした列青くする
  for (int c = 0; c < columnCount(); c++) {
    QTableWidgetItem* cellItem = item(row, c);
    if (cellItem)
      cellItem->setBackground(col);
    else {
      QWidget* widget = cellWidget(row, c);
      if (widget)
        widget->setStyleSheet(
            QString("QFrame { background-color: rgb(%1,%2,%3); }")
                .arg(col.red())
                .arg(col.green())
                .arg(col.blue()));
    }
  }
  return;
}

//----------------------------------------------
RegisterDialog::RegisterDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(tr("Register", "Dialog title"));
  m_cutNameEdit         = new QLineEdit(this);
  m_moveFlowGroupCombo  = new QComboBox(this);
  m_infoCombo           = new QComboBox(this);
  m_moveFlowGroupButton = new QPushButton(tr("Move"), this);
  // List<QPushButton*> buttons;
  // Map< FlowGroupId, QString > buttonLabels =
  //  {Group_LO, tr("LO IN")},
  // Group_RoughGen, tr("Move to RoughGen")},
  // Group_Genga, tr("Move to Genga")},
  // Group_Douga, tr("Move to Douga")},
  // Group_Shiage, tr("Move to Shiage")} ,
  // Group_Satsuei, tr("Move to Satsuei")} };

  m_cutNameEdit->setStyleSheet("QLineEdit { font : bold 20px; }");
  m_moveFlowGroupCombo->setStyleSheet("QComboBox { font : bold 20px; }");
  m_infoCombo->setStyleSheet("QComboBox { font : bold 20px; }");
  m_infoCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  /*
  for (auto fgId : buttonLabels.keys()) {
    QPushButton* button = new QPushButton(buttonLabels[fgId], this);
    button->setProperty("PPM_FlowId", fgId);
    button->setProperty("PPM_ActionName", buttonLabels[fgId]);
    button->setFocusPolicy(Qt::NoFocus);
    connect(button, SIGNAL(clicked()), this, SLOT(onFlowStartButtonClicked()));
    buttons.append(button);
  }
  */

  // refresh();

  //
  // QList<QPair<QString, QVariant>> infos = {
  //  {"BG_UP", 1 },
  //  {"GENGA_TYPE", "Paper" },
  //  {"GENGA_TYPE", "Digital" },
  //  {"3DCG", "Set"},
  //  {"3DCG", "Done"},
  //  {"BANK", QVariant()}, // バンクカットは入力フィールドで値を指定する
  //  {"BG_ONLY", 1 }
  //};
  // int id = 0;
  // for (auto info : infos) {
  //  CutInfoDisplayProperties displayProp =
  //  WorkflowManager::instance()->cutInfoDisplayProperties(info.first,
  //  info.second); m_infoCombo->addItem( displayProp.labelTxt, QList<QVariant>{
  //  info.first, info.second }); m_infoCombo->setItemData(id,
  //  displayProp.bgColor, Qt::BackgroundRole); m_infoCombo->setItemData(id,
  //  displayProp.txtColor, Qt::ForegroundRole); id++;
  //}

  m_infoValueEdit = new QLineEdit(this);
  m_infoValueEdit->setStyleSheet("QLineEdit { font : bold 20px; }");
  m_infoValueEdit->setDisabled(true);

  m_registerInfoButton  = new QPushButton(tr("Register"), this);
  m_releaseInfoButton   = new QPushButton(tr("Release"), this);
  m_cutNameWarningLabel = new QLabel(this);
  m_cutNameWarningLabel->setWordWrap(true);
  m_cutNameWarningLabel->setStyleSheet("QLabel { color: #d33; }");
  m_cutNameWarningLabel->hide();

  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setContentsMargins(5, 5, 5, 5);
  mainLay->setSpacing(10);
  {
    mainLay->addWidget(new QLabel(tr("Separate cut names by comma, colon or "
                                     "semicolon. (Ex. 0001;0002;0003)"),
                                  this),
                       0, Qt::AlignLeft | Qt::AlignVCenter);
    mainLay->addWidget(m_cutNameEdit, 0);
    mainLay->addWidget(m_cutNameWarningLabel, 0);

    QGroupBox* workFlowGroupBox   = new QGroupBox(tr("Move Workflow"), this);
    QHBoxLayout* workFlowGroupLay = new QHBoxLayout();
    workFlowGroupLay->setContentsMargins(5, 5, 5, 5);
    workFlowGroupLay->setSpacing(3);
    {
      workFlowGroupLay->addWidget(
          new QLabel(tr("Move to the flow group :"), this), 0);
      workFlowGroupLay->addWidget(m_moveFlowGroupCombo, 0);
      workFlowGroupLay->addSpacing(20);
      workFlowGroupLay->addWidget(m_moveFlowGroupButton, 0);
      workFlowGroupLay->addStretch(1);
      // for (auto button : buttons)
      //   buttonLay->addWidget(button, 1);
    }
    workFlowGroupBox->setLayout(workFlowGroupLay);

    mainLay->addWidget(workFlowGroupBox, 0);

    QGroupBox* infoGroupBox = new QGroupBox(tr("Register Info"), this);
    QHBoxLayout* infoLay    = new QHBoxLayout();
    infoLay->setContentsMargins(5, 5, 5, 5);
    infoLay->setSpacing(10);
    {
      infoLay->addWidget(m_infoCombo, 0);
      infoLay->addWidget(m_infoValueEdit, 1);
      infoLay->addSpacing(20);
      infoLay->addWidget(m_registerInfoButton, 0);
      infoLay->addWidget(m_releaseInfoButton, 0);
    }
    infoGroupBox->setLayout(infoLay);

    mainLay->addWidget(infoGroupBox, 0);
  }
  setLayout(mainLay);

  connect(m_moveFlowGroupButton, SIGNAL(clicked()), this,
          SLOT(onMoveFlowGroupButtonClicked()));
  connect(m_registerInfoButton, SIGNAL(clicked()), this,
          SLOT(onRegisterInfo()));
  connect(m_releaseInfoButton, SIGNAL(clicked()), this, SLOT(onReleaseInfo()));
  connect(m_cutNameEdit, SIGNAL(textChanged(const QString&)), this,
          SLOT(onCutNamesEdited()));
  connect(m_infoCombo, SIGNAL(activated(int)), this,
          SLOT(onInfoComboActivated()));
}

QStringList RegisterDialog::cutNames() const {
  QStringList names;
  for (const QString& name : m_cutNameEdit->text().split(
           QRegularExpression(";|:|,"), Qt::SkipEmptyParts)) {
    const QString trimmed = name.trimmed();
    if (!trimmed.isEmpty()) names.append(trimmed);
  }
  return names;
}

// 使えないカット名がある間は、警告を表示し、ボタンを無効にする
// （プロジェクト設定で[SEQUENCE]を使う場合は、カット名の先頭に3ケタ以上の数字が必要）
void RegisterDialog::updateButtonsEnabled() {
  QStringList invalidNames;
  for (const QString& name : cutNames())
    if (!WorkflowManager::instance()->isValidSceneName(name))
      invalidNames.append(name);
  const bool ok = invalidNames.isEmpty();
  m_cutNameWarningLabel->setText(
      ok ? QString()
         : WorkflowManager::instance()->invalidSceneNameMessage(invalidNames));
  m_cutNameWarningLabel->setVisible(!ok);
  m_moveFlowGroupButton->setEnabled(ok);
  m_registerInfoButton->setEnabled(ok);
  m_releaseInfoButton->setEnabled(ok);
}

void RegisterDialog::onCutNamesEdited() { updateButtonsEnabled(); }

// m_moveFlowGroupCombo/m_infoComboの項目を、現在のプロジェクトの工程グループ／SceneInfoTagの
// 一覧に基づいて作り直す（プロジェクト設定の保存後、内容を最新化するために呼び出される）
void RegisterDialog::refresh() {
  m_moveFlowGroupCombo->clear();
  // 工程グループをコンボボックスに登録
  for (auto fgId : WorkflowManager::instance()->flowGroupIds()) {
    QString labelStr = WorkflowManager::instance()->flowGroupLabelName(fgId);
    m_moveFlowGroupCombo->addItem(labelStr, fgId);
  }
  m_moveFlowGroupCombo->setCurrentIndex(0);

  m_infoCombo->clear();
  int id = 0;
  for (auto infoTag : WorkflowManager::instance()->sceneInfoTagList()) {
    if (!infoTag->canManuallySet()) continue;
    switch (infoTag->type()) {
    case InfoTagType_Bool:
    case InfoTagType_String:
    case InfoTagType_StringList: {
      SceneInfoTagDisplayProperties displayProp = infoTag->displayProperties();
      m_infoCombo->addItem(displayProp.labelTxt, infoTag->idName());
      m_infoCombo->setItemData(id, displayProp.bgColor, Qt::BackgroundRole);
      m_infoCombo->setItemData(id, displayProp.txtColor, Qt::ForegroundRole);
      id++;
    } break;
    case InfoTagType_Enum: {
      EnumSceneInfoTag* enumInfoTag = dynamic_cast<EnumSceneInfoTag*>(infoTag);
      for (auto enumVal : enumInfoTag->valueList()) {
        SceneInfoTagDisplayProperties displayProp =
            infoTag->displayProperties(enumVal);
        QStringList valList = {infoTag->idName(), enumVal};
        m_infoCombo->addItem(displayProp.labelTxt, valList);
        m_infoCombo->setItemData(id, displayProp.bgColor, Qt::BackgroundRole);
        m_infoCombo->setItemData(id, displayProp.txtColor, Qt::ForegroundRole);
        id++;
      }
    } break;
    default:
      break;
    }
  }
  // プロジェクト設定の変更（[SEQUENCE]を使うか、等）に合わせて、カット名の検証をやり直す
  updateButtonsEnabled();
}

void RegisterDialog::onMoveFlowGroupButtonClicked() {
  QStringList list = cutNames();
  if (list.isEmpty()) return;

  FlowGroupId fgId =
      static_cast<FlowGroupId>(m_moveFlowGroupCombo->currentData().toInt());
  QString actionName =
      tr("Move to the flow group %1")
          .arg(WorkflowManager::instance()->flowGroupLabelName(fgId));
  // 移動先と、カットが元いた工程グループの詳細進捗を、後で作り直す
  QList<FlowGroupId> affectedGroups =
      WorkflowManager::instance()->flowGroupsContainingCuts(list);
  if (!affectedGroups.contains(fgId)) affectedGroups.append(fgId);
  WorkflowManager::instance()->registerCutNames(list, fgId, actionName);
  emit flowGroupsChanged(affectedGroups);

  QMessageBox::information(this, tr("Information"),
                           tr("Registration completed."));

  m_cutNameEdit->setText("");
  accept();
}

void RegisterDialog::onRegisterInfo() {
  QStringList list = cutNames();
  if (list.isEmpty()) return;

  if (m_infoValueEdit->isEnabled() && m_infoValueEdit->text().isEmpty()) {
    QMessageBox::warning(this, tr("Warning"), tr("The value field is empty!"));
    return;
  }

  QString infoTagId;
  QVariant value;
  if (m_infoCombo->currentData().canConvert<QStringList>()) {
    QStringList dataList = m_infoCombo->currentData().toStringList();
    if (dataList.size() == 1) {
      infoTagId = dataList[0];
      for (auto sceneInfoTag :
           WorkflowManager::instance()->sceneInfoTagList()) {
        if (sceneInfoTag->idName() == infoTagId) {
          switch (sceneInfoTag->type()) {
          case InfoTagType_Bool:
            value = 1;
            break;
          case InfoTagType_String:
            value = m_infoValueEdit->text();
            break;
          case InfoTagType_StringList: {
            QStringList list = m_infoValueEdit->text().split(
                QRegularExpression(";|:|,"), Qt::SkipEmptyParts);
            value = QVariant(list);
          } break;
          default:
            break;
          }
        }
      }
    } else {
      assert(dataList.size() == 2);
      infoTagId = dataList[0];
      value     = dataList[1];
    }
  } else
    return;

  // ログに記録用の名前
  QString actionName = m_infoCombo->currentText();
  actionName += " : " + tr("Register", "Log");

  for (auto cutName : list) {
    // 使えないカット名があるときはボタンが無効になるため、通常はここに来ない
    if (!WorkflowManager::instance()->isValidSceneName(cutName)) continue;
    QString sequence = WorkflowManager::instance()->sceneNum2Sequence(cutName);
    WorkflowManager::instance()->saveSceneInfoTag(sequence, cutName, infoTagId,
                                                  value, actionName);
  }
  QMessageBox::information(this, tr("Information"),
                           tr("Registration completed."));
  m_cutNameEdit->setText("");

  if (m_infoValueEdit->isEnabled()) m_infoValueEdit->setText("");
  accept();
}

void RegisterDialog::onReleaseInfo() {
  QStringList list = cutNames();
  if (list.isEmpty()) return;

  QString infoTagId;
  // 選択肢（enum）のカット情報の場合だけ、外す値を持つ。
  // それ以外（真偽値・文字列・文字列のリスト）は、値にかかわらず外す
  QVariant value;

  if (m_infoCombo->currentData().canConvert<QStringList>()) {
    QStringList dataList = m_infoCombo->currentData().toStringList();
    if (dataList.size() == 1) {
      infoTagId = dataList[0];
    } else {
      assert(dataList.size() == 2);
      infoTagId = dataList[0];
      value     = dataList[1];
    }
  } else
    return;

  // QList<QVariant> dataList = m_infoCombo->currentData().toList();
  // QString infoName = dataList[0].toString();
  // QVariant value = dataList[1];

  // ログに記録用の名前
  QString actionName = m_infoCombo->currentText();
  actionName += " : " + tr("Release", "Log");

  for (auto cutName : list) {
    // 使えないカット名があるときはボタンが無効になるため、通常はここに来ない
    if (!WorkflowManager::instance()->isValidSceneName(cutName)) continue;
    QString sequence = WorkflowManager::instance()->sceneNum2Sequence(cutName);
    QVariant current = WorkflowManager::instance()->sceneInfoTag(
        sequence, cutName, infoTagId, QVariant());
    if (!current.isValid()) continue;  // 付いていない
    // INIから読んだ値は文字列になるため、文字列として比べる
    if (value.isValid() && value.toString() != current.toString()) continue;
    WorkflowManager::instance()->releaseSceneInfoTag(sequence, cutName,
                                                     infoTagId, actionName);
  }
  QMessageBox::information(this, tr("Information"), tr("Release completed."));
  m_cutNameEdit->setText("");
  accept();
}

void RegisterDialog::onInfoComboActivated() {
  if (m_infoCombo->currentData().canConvert<QString>()) {
    QString infoTagId = m_infoCombo->currentData().toString();
    for (auto sceneInfoTag : WorkflowManager::instance()->sceneInfoTagList()) {
      if (sceneInfoTag->idName() == infoTagId) {
        if (sceneInfoTag->type() == InfoTagType_String ||
            sceneInfoTag->type() == InfoTagType_StringList)
          m_infoValueEdit->setEnabled(true);
        else
          m_infoValueEdit->setDisabled(true);
        return;
      }
    }
  }
  m_infoValueEdit->setDisabled(true);

  // if (m_infoCombo->currentData().toList()[0].toString() == "BANK")
  //   m_infoValueEdit->setEnabled(true);
  // else
  //   m_infoValueEdit->setDisabled(true);
}

//----------------------------------------------
MyTristateCheckBox::MyTristateCheckBox(const QString& text, QWidget* parent)
    : QCheckBox(text, parent) {
  setTristate();
}

void MyTristateCheckBox::nextCheckState() {
  if (checkState() == Qt::Checked)
    setCheckState(Qt::Unchecked);
  else
    setCheckState(Qt::Checked);
}

//----------------------------------------------

MyWindow::MyWindow() : m_projectSettingsDialog(nullptr) {
  m_registerDialog      = new RegisterDialog(this);
  m_retakeDialog        = new RetakeDialog(this);
  m_userSettingsPopup   = new UserSettingsPopup(this);
  m_table               = new MyTable(this);
  m_sceneKnowledgePanel = new SceneKnowledgePanel(this);
  connect(m_sceneKnowledgePanel, &SceneKnowledgePanel::closeRequested, this,
          [this]() {
            m_rightContentStack->setCurrentWidget(m_table);
            // フローチャートUI上のハイライトも元に戻す
            for (auto* nodeItem : m_flowchartNodeItems.values())
              nodeItem->setHighlighted(false);
          });

  m_buttonGroup            = new QButtonGroup(this);
  m_expandButtonGroup      = new QButtonGroup(this);
  m_checkAtOnceButtonGroup = new QButtonGroup(this);
  m_lockButtonGroup        = new QButtonGroup(this);
  m_reloadButtonGroup      = new QButtonGroup(this);
  m_prjCombo               = new QComboBox(this);
  m_projectLabel           = new QLabel(tr("Project:"), this);

  m_viewModeToggleButton =
      new QPushButton(QIcon(":Resources/swap.svg"), "", this);
  m_viewModeToggleButton->setCheckable(true);
  m_viewModeToggleButton->setFixedSize(23, 23);
  m_viewModeToggleButton->setIconSize(QSize(18, 18));
  m_viewModeToggleButton->setToolTip(
      tr("Switch between list view and flowchart view"));

  m_refreshButton  = new QPushButton(tr("Refresh"), this);
  m_registerButton = new QPushButton(tr("Register"), this);
  m_retakeButton   = new QPushButton(tr("Retake"), this);

#ifdef Q_OS_MACOS
  auto* menuBar = new QMenuBar(this);
  auto* project = menuBar->addMenu(tr("Project"));
  // Cocoa moves these two roles into the application menu.
  auto* about = new QAction(tr("About PPM"), project);
  about->setMenuRole(QAction::AboutRole);
  project->addAction(about);
  connect(about, &QAction::triggered, this, &MyWindow::onAboutTriggered);
  auto* preferences = new QAction(tr("Preferences"), project);
  preferences->setMenuRole(QAction::PreferencesRole);
  preferences->setShortcut(QKeySequence::Preferences);
  project->addAction(preferences);
  connect(preferences, &QAction::triggered, m_userSettingsPopup,
          &QDialog::exec);

  auto* settings = project->addMenu(tr("Project Settings"));
  settings->menuAction()->setMenuRole(QAction::NoRole);
  const QStringList categories = ProjectSettingsDialog::categoryNames();
  for (int row = 0; row < categories.size(); ++row) {
    settings->addAction(categories.at(row), this,
                        [this, row] { openProjectSettings(row); });
  }
#else
  m_openSettingsButton =
      new QPushButton(QIcon(":Resources/gear.svg"), "", this);
  m_openSettingsButton->setFixedSize(23, 23);
  m_openSettingsButton->setIconSize(QSize(18, 18));
  m_openSettingsButton->setToolTip(tr("Project Settings / Preferences"));
  connect(m_openSettingsButton, &QPushButton::clicked, this,
          &MyWindow::onSettingsButtonClicked);
#endif

  m_sceneSearchEdit = new QLineEdit(this);
  m_sceneSearchEdit->setPlaceholderText(tr("Scene Number"));
  connect(m_sceneSearchEdit, &QLineEdit::returnPressed, this,
          &MyWindow::onSceneSearchClicked);
  m_sceneSearchButton =
      new QPushButton(QIcon(":Resources/search.svg"), "", this);
  m_sceneSearchButton->setFixedSize(23, 23);
  m_sceneSearchButton->setIconSize(QSize(18, 18));
  m_sceneSearchButton->setToolTip(tr("Search Scene"));

  QStringList prjList =
      QDir(UserSettings::instance()->stuffFolderPath() + "/projects")
          .entryList(QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
                     QDir::Name);
  for (auto prjName : prjList) {
    if (!prjName.endsWith(".ppmprj")) continue;
    prjName.chop(7);
    m_prjCombo->addItem(prjName);
  }
  if (m_prjCombo->findText(WorkflowManager::instance()->currentProjectName()) >=
      0)
    m_prjCombo->setCurrentText(
        WorkflowManager::instance()->currentProjectName());
  else if (m_prjCombo->count() > 0)
    // 前回開いていたプロジェクトの設定が見つからない場合は、先頭のプロジェクトを選択し直す
    m_prjCombo->setCurrentIndex(0);

  // 話数（Episode）機能ON時のみ表示する選択コンボ。項目・可視性はonCurrentPrjChangedで設定する
  m_episodeCombo             = new QComboBox(this);
  m_episodeRow               = new QWidget(this);
  QHBoxLayout* episodeRowLay = new QHBoxLayout(m_episodeRow);
  episodeRowLay->setContentsMargins(0, 0, 0, 0);
  episodeRowLay->setSpacing(10);
  episodeRowLay->addWidget(new QLabel(tr("Episode:"), this), 0);
  episodeRowLay->addWidget(m_episodeCombo, 1);
  m_episodeRow->setVisible(false);

  m_buttonGroup->setExclusive(false);
  m_expandButtonGroup->setExclusive(false);
  m_checkAtOnceButtonGroup->setExclusive(false);
  m_lockButtonGroup->setExclusive(false);
  m_reloadButtonGroup->setExclusive(false);

  // --- 左パネル ---
  m_topControlsContainer = new QWidget(this);
  // 明示的な最小サイズを（レイアウト由来の既定値ではなく）小さい値に固定しておく。
  // これが無いと、現在採用中の行数のレイアウトが要求する最小幅がそのままこのウィジェットの
  // 最小幅になり、QSplitterがその幅より縮められなくなってしまう。特に1行レイアウトは
  // 必要な最小幅が大きいため、そのままだと区切り線を狭める方向にドラッグしたときに
  // 1行の最小幅に達するまで縮まなくなり、そこで急に折り返し（4行）まで飛んでしまう
  // （2行を経由する幅の範囲をほとんど体験できない）非対称な挙動になる
  m_topControlsContainer->setMinimumSize(1, 1);
  // m_mainSplitterの区切り線移動等によるリサイズを検知し、行数（4/2/1）を再判定する
  m_topControlsContainer->installEventFilter(this);

  m_wfButtonsScrollArea = new QScrollArea(this);
  m_wfButtonsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_wfButtonsScrollArea->setWidgetResizable(true);
  m_wfButtonsScrollArea->setSizeAdjustPolicy(
      QAbstractScrollArea::AdjustToContents);
  // ボタンの作成はrebuildListPanel()で行う

  m_flowchartView = new MainFlowChartView(this);
  m_flowchartView->setScene(new QGraphicsScene(this));
  // 背景色はProject SettingsのFlowChartScene（flowchartscene.cpp）と合わせる
  m_flowchartView->scene()->setBackgroundBrush(QColor(241, 238, 232));
  m_mainRolePalette = new MainRolePaletteWidget(m_flowchartView);
  m_mainRolePalette->setOnSelectionChanged([this]() { refreshList(); });
  m_flowchartView->attachRolePalette(m_mainRolePalette);

  m_leftContentStack = new QStackedWidget(this);
  m_leftContentStack->addWidget(m_wfButtonsScrollArea);
  m_leftContentStack->addWidget(m_flowchartView);

  QWidget* leftPanel        = new QWidget(this);
  QVBoxLayout* leftPanelLay = new QVBoxLayout(leftPanel);
  leftPanelLay->setContentsMargins(5, 5, 5, 5);
  leftPanelLay->setSpacing(5);
  {
    leftPanelLay->addWidget(m_topControlsContainer, 0);
    leftPanelLay->addWidget(m_leftContentStack, 1);
  }

  // 右側パネル：通常はMyTable、カット検索時はSceneKnowledgePanelに切り替える
  m_rightContentStack = new QStackedWidget(this);
  m_rightContentStack->addWidget(m_table);
  m_rightContentStack->addWidget(m_sceneKnowledgePanel);

  m_mainSplitter = new QSplitter(Qt::Horizontal, this);
  m_mainSplitter->addWidget(leftPanel);
  m_mainSplitter->addWidget(m_rightContentStack);
  m_mainSplitter->setStretchFactor(0, 0);
  m_mainSplitter->setStretchFactor(1, 1);
  m_mainSplitter->setSizes(
      {320, 1000});  // 保存された状態が無い場合のデフォルト
  m_mainSplitter->restoreState(
      UserSettings::instance()->loadMainSplitterState());

  QVBoxLayout* outerLay = new QVBoxLayout();
  outerLay->setContentsMargins(0, 0, 0, 0);
  outerLay->addWidget(m_mainSplitter);
  setLayout(outerLay);

  m_flowchartModeActive =
      UserSettings::instance()->loadLeftPanelFlowchartMode();
  m_viewModeToggleButton->setChecked(m_flowchartModeActive);
  m_leftContentStack->setCurrentWidget(
      m_flowchartModeActive ? static_cast<QWidget*>(m_flowchartView)
                            : static_cast<QWidget*>(m_wfButtonsScrollArea));
  rebuildTopControlsLayout();

  connect(m_viewModeToggleButton, SIGNAL(clicked()), this,
          SLOT(onViewModeToggled()));
  connect(m_refreshButton, SIGNAL(clicked()), this, SLOT(onRefresh()));
  connect(m_registerButton, SIGNAL(clicked()), m_registerDialog, SLOT(exec()));
  connect(m_registerDialog, &RegisterDialog::flowGroupsChanged, this,
          &MyWindow::onFlowGroupsChanged);
  connect(m_retakeDialog, &RetakeDialog::flowGroupsChanged, this,
          &MyWindow::onFlowGroupsChanged);
  connect(m_retakeButton, SIGNAL(clicked()), m_retakeDialog, SLOT(exec()));
  connect(m_sceneSearchButton, SIGNAL(clicked()), this,
          SLOT(onSceneSearchClicked()));
  connect(m_buttonGroup, SIGNAL(buttonClicked(QAbstractButton*)), this,
          SLOT(onWorkflowButtonClicked(QAbstractButton*)));
  connect(m_checkAtOnceButtonGroup, SIGNAL(buttonClicked(QAbstractButton*)),
          this, SLOT(onCheckAtOnceButtonClicked(QAbstractButton*)));
  connect(m_lockButtonGroup, SIGNAL(idToggled(int, bool)), this,
          SLOT(onLockButtonToggled(int, bool)));
  connect(m_reloadButtonGroup, SIGNAL(idClicked(int)), this,
          SLOT(onReloadButtonClicked(int)));

  connect(m_prjCombo, SIGNAL(currentTextChanged(const QString&)), this,
          SLOT(onCurrentPrjChanged(const QString&)));
  connect(m_episodeCombo, SIGNAL(currentIndexChanged(int)), this,
          SLOT(onCurrentEpisodeChanged()));

  // 前回開いていたプロジェクトが見つからない場合はm_prjComboの選択が先頭項目にフォールバックしているため、
  // WorkflowManager::currentProjectName()ではなくm_prjComboの実際の選択内容を使う
  onCurrentPrjChanged(m_prjCombo->currentText());
}

namespace {
// m_topControlsContainerの実際の横幅から、採用する行数（4/2/1）を決める。
// 一覧モードの初期スプリッタ幅（320px前後）で4行になるよう、閾値は目安として設定している
int topControlsRowCountForWidth(int width) {
  if (width < 340) return 4;
  if (width < 560) return 2;
  return 1;
}
}  // namespace

void MyWindow::rebuildTopControlsLayout() {
  int rowCount = topControlsRowCountForWidth(m_topControlsContainer->width());
  if (rowCount == m_topControlsRowCount)
    return;  // 行数が変わらないなら組み直さない
  m_topControlsRowCount = rowCount;

  // 既存のレイアウトを一旦破棄する。子ウィジェット自体（m_prjCombo等）は破棄されず、
  // m_topControlsContainerの子のまま残るので、以下で新しいレイアウトへそのまま組み込み直せる
  if (QLayout* oldLayout = m_topControlsContainer->layout()) delete oldLayout;

  // グループA：表示切替／プロジェクト選択／設定
  QHBoxLayout* projectLay = new QHBoxLayout();
  projectLay->setContentsMargins(0, 0, 0, 0);
  projectLay->setSpacing(10);
  projectLay->addWidget(m_viewModeToggleButton, 0);
  projectLay->addWidget(m_projectLabel, 0);
  projectLay->addWidget(m_prjCombo, 1);
#ifndef Q_OS_MACOS
  projectLay->addWidget(m_openSettingsButton, 0);
#endif

  // グループC：更新／登録／リテイクの各ボタン
  QHBoxLayout* buttonLay = new QHBoxLayout();
  buttonLay->setContentsMargins(0, 0, 0, 0);
  buttonLay->setSpacing(3);
  buttonLay->addWidget(m_refreshButton, 1);
  buttonLay->addWidget(m_registerButton, 1);
  buttonLay->addWidget(m_retakeButton, 1);

  // グループD：カット検索
  QHBoxLayout* searchLay = new QHBoxLayout();
  searchLay->setContentsMargins(0, 0, 0, 0);
  searchLay->setSpacing(3);
  searchLay->addWidget(m_sceneSearchEdit, 1);
  searchLay->addWidget(m_sceneSearchButton, 0);

  if (rowCount == 4) {
    // 4行：グループA／Episode／C／Dをそれぞれ独立した行にする（最も狭い横幅向け）
    QVBoxLayout* lay = new QVBoxLayout(m_topControlsContainer);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(5);
    lay->addLayout(projectLay, 0);
    lay->addWidget(m_episodeRow, 0);
    lay->addLayout(buttonLay, 0);
    lay->addLayout(searchLay, 0);
  } else if (rowCount == 2) {
    // 2行：グループA+Episodeを1行目、C+Dを2行目にまとめる
    projectLay->addWidget(m_episodeRow, 0);

    QVBoxLayout* lay = new QVBoxLayout(m_topControlsContainer);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(5);
    lay->addLayout(projectLay, 0);

    QHBoxLayout* row2Lay = new QHBoxLayout();
    row2Lay->setContentsMargins(0, 0, 0, 0);
    row2Lay->setSpacing(10);
    row2Lay->addLayout(buttonLay, 1);
    row2Lay->addLayout(searchLay, 1);
    lay->addLayout(row2Lay, 0);
  } else {
    // 1行：全グループを横一列にまとめる（最も広い横幅向け）
    projectLay->addWidget(m_episodeRow, 0);

    QHBoxLayout* lay = new QHBoxLayout(m_topControlsContainer);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);
    lay->addLayout(projectLay, 0);
    lay->addLayout(buttonLay, 0);
    lay->addLayout(searchLay, 0);
    lay->addStretch(1);
  }
}

void MyWindow::onViewModeToggled() {
  // 再構築の前にチェック状態を保存し、再構築後（新しいモードのウィジェットが作られた後）に
  // loadListIndices経由で復元することで、一覧⇔フローチャート間でチェック状態を引き継ぐ
  saveCurrentCheckedState();

  m_flowchartModeActive = m_viewModeToggleButton->isChecked();
  m_leftContentStack->setCurrentWidget(
      m_flowchartModeActive ? static_cast<QWidget*>(m_flowchartView)
                            : static_cast<QWidget*>(m_wfButtonsScrollArea));
  rebuildTopControlsLayout();
  rebuildLeftPanelContent();
  updateWorkflowTooltips();
  refreshList();
}

void MyWindow::onRefresh() {
  WorkflowManager::instance()->loadCutList();
  refreshList();
}

void MyWindow::refreshList() {
  QList<WorkflowType> checkedWfTypes;
  QList<WorkflowType> lockedWfTypes;  // 選択されたWFの中でロックされているもの

  for (auto button : m_buttonGroup->buttons()) {
    if (button->isChecked()) {
      WorkflowType wfType =
          (WorkflowType)button->property("PPM_WfType").toInt();
      int buttonId = m_buttonGroup->id(button);
      // フローチャートモードのノードには非表示のロックチェックボックスを同じIDで登録しているため、
      // ここは常にnullでないはずだが、念のためガードする
      QAbstractButton* lockButton = m_lockButtonGroup->button(buttonId);
      bool isLocked               = lockButton && lockButton->isChecked();

      checkedWfTypes.append(wfType);
      if (isLocked) lockedWfTypes.append(wfType);
    }
  }

  // 「All
  // Episodes」（話数機能ON、かつコンボの選択が空文字列＝全話数）のときは、
  // 話数ごとにsetCurrentEpisode+loadCutListしながらdoRefresh()を繰り返し呼び出し、
  // 結果を1つのテーブルに積み上げる。各行がどの話数由来かはDataInfo::episodeで区別される
  bool allEpisodesMode = WorkflowManager::instance()->hasEpisodes() &&
                         m_episodeCombo->currentData().toString().isEmpty();

  QMap<FlowGroupId, int> cutCounts;

  if (allEpisodesMode) {
    QString originalEpisode = WorkflowManager::instance()->currentEpisode();
    QStringList episodes    = WorkflowManager::instance()->episodeNames();
    for (int i = 0; i < episodes.size(); i++) {
      WorkflowManager::instance()->setCurrentEpisode(episodes.at(i));
      WorkflowManager::instance()->loadCutList();
      m_table->doRefresh(checkedWfTypes, lockedWfTypes, m_wfType_cbId_map,
                         i > 0);
      for (auto fgId : WorkflowManager::instance()->flowGroupIds()) {
        QStringList cutList =
            WorkflowManager::instance()->cutListInFlow(fgId) +
            WorkflowManager::instance()->cutListInBranchFlow(fgId);
        cutList.removeDuplicates();
        cutCounts[fgId] += cutList.count();
      }
    }
    // 元々アクティブだった話数に戻す
    WorkflowManager::instance()->setCurrentEpisode(originalEpisode);
    WorkflowManager::instance()->loadCutList();
  } else {
    m_table->doRefresh(checkedWfTypes, lockedWfTypes, m_wfType_cbId_map);
    for (auto fgId : WorkflowManager::instance()->flowGroupIds()) {
      QStringList cutList =
          WorkflowManager::instance()->cutListInFlow(fgId) +
          WorkflowManager::instance()->cutListInBranchFlow(fgId);
      cutList.removeDuplicates();
      cutCounts[fgId] = cutList.count();
    }
  }

  for (auto fgId : WorkflowManager::instance()->flowGroupIds()) {
    if (!m_checkAtOnceButtonGroup->button((int)fgId)) continue;
    m_checkAtOnceButtonGroup->button((int)fgId)->setText(
        QString("%1  (%2)")
            .arg(WorkflowManager::instance()->flowGroupLabelName(fgId))
            .arg(cutCounts.value(fgId)));
  }

  // フローチャートモードのカット数ラベルも同様に更新する
  // （工程グループノードのラベルは上のcutCountsをそのまま使う。工程ノードのラベルは
  //  All
  //  Episodesモードの場合、このループの後で元に戻ったカレント話数の値のみを反映する）
  if (m_flowchartModeActive) {
    for (auto fgId : m_flowchartGroupCutCountLabels.keys()) {
      QString txt = tr("%1cuts").arg(cutCounts.value(fgId));
      for (auto label : m_flowchartGroupCutCountLabels.value(fgId))
        label->setText(txt);
    }

    QMap<FlowGroupId, QMap<WorkflowType, QStringList>> progressListsByGroup;
    for (auto it = m_flowchartCutCountLabels.constBegin();
         it != m_flowchartCutCountLabels.constEnd(); ++it) {
      WorkflowType wfType = it.key();
      // rebuildFlowchartPanel()で作ったキャッシュを使う（クリックのたびに走るrefreshList()の中で
      // WorkflowManager::workflow(type)のディープコピーをノード数分繰り返さないため）
      FlowGroupId fgId = m_flowchartWfTypeToGroupId.value(wfType, Group_None);
      if (!progressListsByGroup.contains(fgId)) {
        QMap<WorkflowType, QStringList> progressList;
        for (int i = 0; i < WorkflowManager::instance()->workflowCount(); i++) {
          Workflow wf = WorkflowManager::instance()->getWorkflow(i);
          if (wf.groupId == fgId) progressList.insert(wf.workflowType, {});
        }
        WorkflowManager::instance()->getProgressList(fgId, progressList);
        progressListsByGroup.insert(fgId, progressList);
      }
      const QStringList& cutNames =
          progressListsByGroup.value(fgId).value(wfType);
      int count = cutNames.size();
      it.value()->setText(count > 0 ? QString::number(count) : QString());
      it.value()->setVisible(count > 0);

      // マウスオーバーで登録済みカット名の一覧を表示する。10個ずつで改行し、長くなりすぎないようにする
      if (count > 0) {
        QStringList lines;
        for (int i = 0; i < cutNames.size(); i += 10)
          lines.append(cutNames.mid(i, 10).join(", "));
        it.value()->setToolTip(tr("Scenes in this workflow: ") +
                               lines.join(",\n"));
      } else {
        it.value()->setToolTip(QString());
      }
    }

    // カット数のバッジの表示・桁数に合わせて、工程ノードの幅と枠・リンクを合わせ直す
    updateFlowchartGeometry();

    // 工程グループノードの3stateチェックボックスを、枠内の工程の現在のチェック状態から集計し直す。
    // 個々の工程ノードのクリックだけでなく、役職パレットからの一括選択等、このハンドラを経由しない
    // 変更経路もあるため、refreshList()のたびに全ての枠についてまとめて更新する
    for (auto it = m_flowchartGroupCheckboxMembers.constBegin();
         it != m_flowchartGroupCheckboxMembers.constEnd(); ++it) {
      QCheckBox* groupCb = it.key();
      bool anyChecked = false, anyUnchecked = false;
      for (auto memberType : it.value()) {
        if (auto* memberCb =
                qobject_cast<QCheckBox*>(m_buttonGroup->button(memberType))) {
          if (memberCb->isChecked())
            anyChecked = true;
          else
            anyUnchecked = true;
        }
      }
      Qt::CheckState newState =
          (anyChecked && anyUnchecked)
              ? Qt::PartiallyChecked
              : (anyChecked ? Qt::Checked : Qt::Unchecked);
      if (groupCb->checkState() != newState) {
        groupCb->blockSignals(true);
        groupCb->setCheckState(newState);
        groupCb->blockSignals(false);
      }
    }

    // 役職パレットは工程ノードのチェック状態に応じたアイコンを描画するが、チェックボックスの
    // 切り替え自体はここ（refreshList）を呼ぶだけでパレット側の再描画を促さないため、
    // 明示的にupdate()を呼んで即座に反映させる
    m_mainRolePalette->update();
  }
}

// void MyWindow::refreshList() {
//   QList<int> checkedWfIds;
//   QList<int> lockedWfIds; //選択されたWFの中でロックされているインデックス
//
//   for (auto button : m_buttonGroup->buttons()) {
//     QVariantList wfIdList = button->property("PPM_WfIdList").toList();
//     if (button->isChecked()) {
//       int buttonId = m_buttonGroup->id(button);
//       bool isLocked = m_lockButtonGroup->button(buttonId)->isChecked();
//
//       for (auto wfId : wfIdList) {
//         checkedWfIds.append(wfId.toInt());
//         if (isLocked)
//           lockedWfIds.append(wfId.toInt());
//       }
//     }
//   }
//   m_table->search(checkedWfIds, lockedWfIds, m_wfId_cbId_map);
//
//   for (int fgId = Group_LO; fgId < FlowGroupCount; fgId++) {
//     if (!m_checkAtOnceButtonGroup->button(fgId)) continue;
//     QStringList cutList =
//     WorkflowManager::instance()->cutListInFlow((FlowGroupId)fgId) +
//     WorkflowManager::instance()->cutListInBranchFlow((FlowGroupId)fgId);
//     cutList.removeDuplicates();
//     m_checkAtOnceButtonGroup->button(fgId)->setText(
//       QString("%1
//       (%2)").arg(WorkflowManager::instance()->flowGroupName((FlowGroupId)fgId))
//       .arg(cutList.count())
//     );
//   }
// }

void MyWindow::saveCurrentCheckedState() {
  QString prjName = WorkflowManager::instance()->currentProjectName();

  QList<int> checkedFlowTypes;
  for (auto button : m_buttonGroup->buttons()) {
    if (button->isChecked()) {
      checkedFlowTypes.append(button->property("PPM_WfType").toInt());
    }
  }

  QList<int> expandedFlowGroupIds;
  for (auto fgId : WorkflowManager::instance()->flowGroupIds()) {
    if (!m_expandButtonGroup->button((int)fgId)) continue;
    if (m_expandButtonGroup->button((int)fgId)->isChecked())
      expandedFlowGroupIds.append((int)fgId);
  }

  QList<int> unlockedFlowTypes;
  for (auto lockBtn : m_lockButtonGroup->buttons()) {
    if (!lockBtn->isChecked()) {
      unlockedFlowTypes.append(lockBtn->property("PPM_WfType").toInt());
    }
  }

  UserSettings::instance()->saveListIndices(
      prjName, checkedFlowTypes, expandedFlowGroupIds, unlockedFlowTypes);
}

void MyWindow::closeEvent(QCloseEvent* event) {
  // Googleスプレッドシートへの書き込み／Google
  // Chatへの通知が未完了のまま終了すると、
  // 未送信の内容が失われる（セルが空白のままになる、通知が届かない）ため、確認を挟む
  int pendingGoogle = GoogleGateway::instance()->pendingCount();
  int pendingChat   = GoogleChatGateway::instance()->pendingCount();
  if (pendingGoogle > 0 || pendingChat > 0) {
    QString message;
    if (pendingGoogle > 0)
      message +=
          tr("%1 Google Spreadsheet write(s) have not been completed yet.\n")
              .arg(pendingGoogle);
    if (pendingChat > 0)
      message += tr("%1 Google Chat notification(s) have not been sent yet.\n")
                     .arg(pendingChat);
    message +=
        tr("If you quit now, the unsent data will NOT be written or sent.\n"
           "Do you want to quit anyway?");
    QMessageBox msgBox(QMessageBox::Warning, tr("Warning"), message,
                       QMessageBox::NoButton, this);
    QPushButton* waitBtn =
        msgBox.addButton(tr("Wait"), QMessageBox::RejectRole);
    QPushButton* quitBtn =
        msgBox.addButton(tr("Quit anyway"), QMessageBox::DestructiveRole);
    msgBox.setDefaultButton(waitBtn);
    msgBox.setEscapeButton(waitBtn);
    msgBox.exec();
    if (msgBox.clickedButton() != quitBtn) {
      event->ignore();
      return;
    }
  }

  saveCurrentCheckedState();
  UserSettings::instance()->saveWindowGeometry(saveGeometry());
  UserSettings::instance()->saveLeftPanelFlowchartMode(m_flowchartModeActive);
  UserSettings::instance()->saveMainSplitterState(m_mainSplitter->saveState());

  QWidget::closeEvent(event);
}

bool MyWindow::eventFilter(QObject* watched, QEvent* event) {
  // m_mainSplitterの区切り線移動等でm_topControlsContainerの横幅が変わったら、
  // アダプティブなレイアウトの行数（4/2/1）を再判定する
  if (watched == m_topControlsContainer && event->type() == QEvent::Resize)
    rebuildTopControlsLayout();
  return QWidget::eventFilter(watched, event);
}

// m_buttonGroup等の全QButtonGroupをクリアしてから、現在のモードに応じて
// rebuildListPanel()/rebuildFlowchartPanel()のどちらかを呼ぶ
void MyWindow::rebuildLeftPanelContent() {
  removeAllButtonsFromGroup(m_buttonGroup);
  removeAllButtonsFromGroup(m_expandButtonGroup);
  removeAllButtonsFromGroup(m_checkAtOnceButtonGroup);
  removeAllButtonsFromGroup(m_lockButtonGroup);
  removeAllButtonsFromGroup(m_reloadButtonGroup);
  m_wfType_cbId_map.clear();
  m_flowchartGroupCutCountLabels.clear();
  m_flowchartCutCountLabels.clear();
  m_flowchartGroupFrames.clear();
  m_flowchartLinks.clear();
  // 作り直し前の（破棄される）チェックボックス／アイテムへのぶら下がりポインタを残さないよう、
  // ここで一括してクリアする（再構築するのはフローチャートモードのときのみ）
  m_flowchartWfTypeToGroupCheckbox.clear();
  m_flowchartGroupCheckboxMembers.clear();
  m_flowchartNodeItems.clear();

  if (m_flowchartModeActive)
    rebuildFlowchartPanel();
  else
    rebuildListPanel();
}

// 既存の一覧UI（チェックボックス縦並び）を構築する。onCurrentPrjChangedに元々あった処理の抽出
void MyWindow::rebuildListPanel() {
  QList<int> checkedFlowTypes;
  QList<int> expandedFlowGroupIds;
  QList<int> unlockedFlowTypes;
  // プロジェクトごとに状態を保持する
  UserSettings::instance()->loadListIndices(
      WorkflowManager::instance()->currentProjectName(), checkedFlowTypes,
      expandedFlowGroupIds, unlockedFlowTypes);

  if (m_wfButtonsScrollArea->widget()) delete m_wfButtonsScrollArea->widget();

  QWidget* checkBoxesWidget = new QWidget(this);
  QVBoxLayout* cbLay        = new QVBoxLayout();
  cbLay->setContentsMargins(3, 3, 3, 3);
  cbLay->setSpacing(5);
  {
    FlowGroupId currentFlowGroupId = Group_None;
    QVBoxLayout* currentLayout     = nullptr;

    // 一つのチェックボックスに紙／デジタル両方のWorkflowを登録する
    int checkboxId = 0;
    for (int id = 0; id < WorkflowManager::instance()->workflowCount(); id++) {
      Workflow wf = WorkflowManager::instance()->getWorkflow(id);

      if (wf.groupId != currentFlowGroupId) {
        // 初期値
        bool visible         = expandedFlowGroupIds.contains(wf.groupId);
        QCheckBox* visibleCB = new QCheckBox("", this);
        visibleCB->setChecked(visible);
        visibleCB->setStyleSheet(
            QString("QCheckBox{"
                    "color: black;"
                    "background-color:black;"
                    "}"
                    "QCheckBox::indicator:unchecked{"
                    "color: black;"
                    "image: url(:/Resources/arrow_closed.png);"
                    "}"
                    "QCheckBox::indicator:checked{"
                    "color: black;"
                    "image: url(:/Resources/arrow_opened.png);"
                    "}"));
        visibleCB->setSizePolicy(QSizePolicy::Fixed,
                                 QSizePolicy::MinimumExpanding);
        m_expandButtonGroup->addButton(visibleCB, (int)wf.groupId);

        // 一括でチェックを入れる／解除するチェックボックス
        // QCheckBox* checkAtOnceCB = new QCheckBox("", this);
        MyTristateCheckBox* checkAtOnceCB = new MyTristateCheckBox("", this);
        checkAtOnceCB->setStyleSheet(
            QString("QCheckBox{"
                    "color: white;"
                    "background-color: black;"
                    "border: 3px solid black;"
                    "}"
                    "QCheckBox::indicator:unchecked{"
                    "image: url(:/Resources/check_off.png);"
                    "}"));
        // checkAtOnceCB->setTristate();
        // 一部選択中の表示
        m_checkAtOnceButtonGroup->addButton(checkAtOnceCB, (int)wf.groupId);

        // 工程の詳細進捗を現在のファイルから更新するボタン(デザインはあとで)
        QPushButton* reloadButton = new QPushButton("", this);
        reloadButton->setStyleSheet(
            QString("QPushButton{"
                    "color: white;"
                    "background-color: black;"
                    "border: 3px solid black;"
                    "image: url(:/Resources/reload.png);"
                    "}"
                    "QPushButton:hover{"
                    "background-color: gray;"
                    "}"));
        reloadButton->setToolTip(reloadButtonToolTip());
        m_reloadButtonGroup->addButton(reloadButton, (int)wf.groupId);

        QWidget* container = new QWidget(this);
        currentLayout      = new QVBoxLayout();
        currentLayout->setContentsMargins(2, 2, 2, 2);
        currentLayout->setSpacing(5);
        container->setLayout(currentLayout);

        QHBoxLayout* titleLay = new QHBoxLayout();
        titleLay->setContentsMargins(2, 0, 2, 0);
        titleLay->setSpacing(0);
        {
          titleLay->addWidget(visibleCB, 0);
          titleLay->addWidget(checkAtOnceCB, 1);
          titleLay->addWidget(reloadButton, 0);
        }
        cbLay->addLayout(titleLay, 0);
        cbLay->addWidget(container, 0);

        connect(visibleCB, SIGNAL(clicked(bool)), container,
                SLOT(setVisible(bool)));
        container->setVisible(visible);

        currentFlowGroupId = wf.groupId;
      }

      QCheckBox* cb = new QCheckBox(wf.name);
      // 背景色は、チェックボックスを入れた土台のウィジェットに、パレットのWindow色で付ける。
      // ・Qt6.10以降のWindows11スタイルは、スタイルシートで設定されたパレットのButton色で
      //   インジケータを塗るため、チェックボックスにスタイルシートで背景色を付けると、
      //   工程色の上に白いチェックが乗って見えなくなる
      // ・Qt6.12のWindows11スタイルでは、チェックボックス自体にautoFillBackgroundと
      //   パレットで背景色を付けても描かれない
      QWidget* cbBase = new QWidget();
      cbBase->setAutoFillBackground(true);
      QPalette cbPalette = cbBase->palette();
      cbPalette.setColor(QPalette::Window, wf.color);
      cbBase->setPalette(cbPalette);
      QHBoxLayout* cbBaseLay = new QHBoxLayout(cbBase);
      cbBaseLay->setContentsMargins(0, 0, 0, 0);
      cbBaseLay->addWidget(cb);
      QVariantList idList = {id};
      cb->setProperty("PPM_WfType", wf.workflowType);

      // 右クリックで「この工程から始まるカットを登録する」コマンドを出す
      cb->setContextMenuPolicy(Qt::CustomContextMenu);
      connect(cb, &QWidget::customContextMenuRequested, this,
              [this, wfType = wf.workflowType](const QPoint& pos) {
                showRegisterSceneMenu(
                    wfType, static_cast<QWidget*>(sender())->mapToGlobal(pos));
              });

      // ツールチップは、[EPISODE]マクロの解決に現在の話数の値が必要なため、
      // 話数が確定した後にupdateWorkflowTooltips()でまとめて設定する
      m_buttonGroup->addButton(cb, wf.workflowType);
      m_wfType_cbId_map.insert(wf.workflowType, checkboxId);
      if (checkedFlowTypes.contains(wf.workflowType)) cb->setChecked(true);

      // ロックボタン
      QCheckBox* lockButton = createLockButton(
          wf, !unlockedFlowTypes.contains(wf.workflowType), true);
      // 工程名の土台の高さを、ロックボタンの高さにそろえる
      // （Windows11スタイルのチェックボックスはロックボタンより背が高く、色の帯の高さがずれるため）
      cbBase->setFixedHeight(lockButton->sizeHint().height());

      QHBoxLayout* cbLay = new QHBoxLayout();
      cbLay->setContentsMargins(0, 0, 0, 0);
      cbLay->setSpacing(0);
      {
        cbLay->addWidget(cbBase, 1);
        cbLay->addWidget(lockButton, 0);
      }
      currentLayout->addLayout(cbLay, 0);

      checkboxId++;
    }
    // std::cout << "checkboxId = " << checkboxId << std::endl;
    cbLay->addStretch(1);
  }
  checkBoxesWidget->setLayout(cbLay);

  m_wfButtonsScrollArea->setWidget(checkBoxesWidget);

  // checkAtOnceボタンの状態を同期する

  QMap<int, Qt::CheckState> buttonStates;
  for (auto button : m_buttonGroup->buttons()) {
    bool on = button->isChecked();
    WorkflowType wfType =
        static_cast<WorkflowType>(button->property("PPM_WfType").toInt());
    FlowGroupId fgId = WorkflowManager::instance()->workflow(wfType).groupId;
    if (buttonStates.contains(fgId)) {
      if (buttonStates.value(fgId) == Qt::PartiallyChecked) continue;
      if ((buttonStates.value(fgId) == Qt::Checked && !on) ||
          (buttonStates.value(fgId) == Qt::Unchecked && on))
        buttonStates.insert(fgId, Qt::PartiallyChecked);
    } else
      buttonStates.insert(fgId, ((on) ? Qt::Checked : Qt::Unchecked));
  }
  for (auto fgId : buttonStates.keys())
    dynamic_cast<QCheckBox*>(m_checkAtOnceButtonGroup->button(fgId))
        ->setCheckState(buttonStates.value(fgId));
}

namespace {
// フローチャートの工程ノードの最小の幅（プロジェクト設定側のNodeMinWidthと同じ）
const int FlowchartNodeMinWidth = 180;
}  // namespace

// フローチャートUIのノード／グループ／リンクを構築する
void MyWindow::rebuildFlowchartPanel() {
  QList<int> checkedFlowTypes;
  QList<int> expandedFlowGroupIds;  // フローチャートモードでは使わない
  QList<int> unlockedFlowTypes;
  UserSettings::instance()->loadListIndices(
      WorkflowManager::instance()->currentProjectName(), checkedFlowTypes,
      expandedFlowGroupIds, unlockedFlowTypes);

  QGraphicsScene* scene = m_flowchartView->scene();
  scene->clear();

  FlowChartLayout layout = WorkflowManager::instance()->flowChartLayout();
  if (layout.workflowNodes.isEmpty() &&
      WorkflowManager::instance()->workflowCount() > 0)
    layout = buildDefaultMainFlowChartLayout();

  // WorkflowManager::workflow(type)は「見つからない」場合デフォルト構築のWorkflowを返すが、
  // Workflow::workflowTypeにデフォルト初期値が無く不定値になるため、見つかったかどうかの
  // 判定には使えない。ここでは事前にQMapを作り、確実な存在チェックを行う
  QMap<WorkflowType, Workflow> workflowsByType;
  for (int i = 0; i < WorkflowManager::instance()->workflowCount(); i++) {
    Workflow wf = WorkflowManager::instance()->getWorkflow(i);
    workflowsByType.insert(wf.workflowType, wf);
  }

  QMap<WorkflowType, MainWorkflowNodeItem*> nodeItems;
  // クリック・再描画のたびにWorkflowManager::workflow(type)（Workflow構造体のディープコピーを
  // 伴う）を呼ばずに済むよう、ここで一度だけキャッシュを作っておく
  m_flowchartWfTypeToGroupId.clear();
  QMap<QRgb, QList<QAbstractButton*>> roleButtonCache;

  // 工程ノードを作る。チェックボックスは既存のm_buttonGroup等へそのまま登録し、
  // 一覧モードと同じrefreshList()の仕組みで右側のカット一覧に反映されるようにする
  for (auto& node : layout.workflowNodes) {
    if (!workflowsByType.contains(node.workflowType))
      continue;  // レイアウトに残っている削除済み工程は無視する
    Workflow wf = workflowsByType.value(node.workflowType);
    m_flowchartWfTypeToGroupId.insert(wf.workflowType, wf.groupId);

    QColor textColor = (wf.color.lightness() > 128) ? QColor(25, 25, 25)
                                                    : QColor(240, 240, 240);

    // 背景の角丸はMainWorkflowNodeItem::paint()が自前で描くため、埋め込みwidget自体は透明にする
    QWidget* content = new QWidget();
    content->setAttribute(Qt::WA_TranslucentBackground, true);
    // ノードの高さはProject
    // Settings側（flowchartscene.cppのNodeHeaderHeight=30）と合わせる。
    // 保存済みレイアウトの縦間隔もその高さを前提に計算されているため、高さが異なると
    // 上下に隙間なく並ぶノード同士が重なってしまう
    const qreal nodeH       = 30;
    const int nodeMarginH   = 10;
    const int nodeSpacing   = 6;
    QHBoxLayout* nodeLay = new QHBoxLayout(content);
    nodeLay->setContentsMargins(nodeMarginH, 2, nodeMarginH, 2);
    nodeLay->setSpacing(nodeSpacing);

    // スタイルシートでbackgroundを指定しないこと。Qt6.10以降のWindows11スタイルは、
    // スタイルシートで設定されたパレットのButton色でインジケータを塗るため、
    // transparentを指定するとチェック時の塗りが消えて見えなくなる。
    // QCheckBoxは元々背景を塗らないので、指定しなくても透明になる
    QCheckBox* cb = new QCheckBox();
    cb->setProperty("PPM_WfType", wf.workflowType);
    m_buttonGroup->addButton(cb, wf.workflowType);
    m_wfType_cbId_map.insert(wf.workflowType, m_wfType_cbId_map.size());
    if (checkedFlowTypes.contains(wf.workflowType)) cb->setChecked(true);
    // toggled経由でrefreshList()を直結しない：cbはm_buttonGroupのメンバーでもあるため、
    // ユーザークリック時はm_buttonGroupのbuttonClicked→onWorkflowButtonClicked→refreshList()が
    // 既に呼ばれる。ここでも繋ぐと1クリックでrefreshList()が二重に走り、反応が遅くなる
    nodeLay->addWidget(cb, 0);
    roleButtonCache[wf.color.rgba()].append(cb);

    // ロックボタン（ノードの右端に置く。背景はノードの色のままにする）
    QCheckBox* lockButton = createLockButton(
        wf, !unlockedFlowTypes.contains(wf.workflowType), false);

    QLabel* nameLabel = new QLabel(wf.name);
    nameLabel->setStyleSheet(
        QString("color: rgb(%1,%2,%3); background: transparent;")
            .arg(textColor.red())
            .arg(textColor.green())
            .arg(textColor.blue()));
    // 工程名とカット数のバッジは1つの行にまとめる。ノードが最小幅で余白が余るときは、
    // 工程名とバッジの間を空け、バッジをロックボタンの左隣に置く
    QHBoxLayout* nameLay = new QHBoxLayout();
    nameLay->setContentsMargins(0, 0, 0, 0);
    nameLay->setSpacing(nodeSpacing);
    nameLay->addWidget(nameLabel, 0);
    nameLay->addStretch(1);

    QLabel* cutCountLabel = new QLabel();
    cutCountLabel->setStyleSheet(
        "background-color: white; color: black; border-radius: 8px; padding: "
        "1px 6px;");
    cutCountLabel->setVisible(false);
    nameLay->addWidget(cutCountLabel, 0);
    nodeLay->addLayout(nameLay, 1);
    m_flowchartCutCountLabels.insert(wf.workflowType, cutCountLabel);
    // ロックボタンの幅は鍵の画像の幅にそろえる（右側に余分な余白が付かないようにする）
    lockButton->setFixedWidth(QPixmap(":/Resources/lock.png").width());
    nodeLay->addWidget(lockButton, 0);

    // ノードの幅は、カット数が決まった後にupdateFlowchartGeometry()で部品の幅から求める
    content->setFixedSize(QSize(FlowchartNodeMinWidth, (int)nodeH));
    MainWorkflowNodeItem* item =
        new MainWorkflowNodeItem(wf.workflowType, wf.color, content);
    scene->addItem(item);
    item->setPos(node.pos);
    nodeItems.insert(wf.workflowType, item);
    m_flowchartNodeItems.insert(wf.workflowType, item);

    // 右クリックで「この工程から始まるカットを登録する」コマンドを出す。
    // contentはQGraphicsProxyWidgetに埋め込まれているため、QWidget::mapToGlobal()は
    // ビューの拡大率・パン位置を考慮できず位置がずれる。itemのシーン座標経由でビューの
    // ビューポート座標へ変換してからmapToGlobal()する必要がある
    content->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(content, &QWidget::customContextMenuRequested, this,
            [this, wfType = wf.workflowType, item](const QPoint& pos) {
              QPointF scenePos   = item->mapToScene(QPointF(pos));
              QPoint viewportPos = m_flowchartView->mapFromScene(scenePos);
              showRegisterSceneMenu(
                  wfType,
                  m_flowchartView->viewport()->mapToGlobal(viewportPos));
            });
  }

  m_mainRolePalette->setRoleButtonCache(roleButtonCache);

  // 工程グループの背景枠を作る（同一グループ内の連結成分ごと。飛び地はそれぞれ別枠になる）
  for (auto fgId : WorkflowManager::instance()->flowGroupIds()) {
    for (auto& component : groupWorkflowsByConnectedComponent(fgId)) {
      QRectF bounds;
      bool first = true;
      for (auto wfType : component) {
        if (!nodeItems.contains(wfType)) continue;
        // 枠の大きさはProject
        // Settings側（flowchartscene.cppのグループ枠の自動導出）と揃える。
        // MainWorkflowNodeItem::boundingRect()はハイライト用の余白を含むため使わず、
        // 設定画面側のWorkflowNodeItem::boundingRect()と同じく、ノード本体を1px広げた矩形を使う
        MainWorkflowNodeItem* nodeItem = nodeItems.value(wfType);
        QRectF r                       = nodeItem->mapRectToScene(
            nodeItem->widgetRect().adjusted(-1, -1, 1, 1));
        bounds = first ? r : bounds.united(r);
        first  = false;
      }
      if (first) continue;  // この連結成分の工程ノードが1つも配置されていない

      // flowchartscene.cppのGroupMargin／GroupLabelHeightと同じ値
      const qreal margin = 20, labelH = 30;
      QRectF frameRect(bounds.topLeft() - QPointF(margin, margin + labelH),
                       bounds.size() + QSizeF(margin * 2, margin * 2 + labelH));

      // 背景の角丸はMainGroupNodeItem::paint()が自前で描くため、埋め込みwidget自体は透明にする
      QWidget* content = new QWidget();
      content->setAttribute(Qt::WA_TranslucentBackground, true);
      content->setFixedSize(frameRect.size().toSize());
      // ヘッダー行（チェックボックス／カット数／reloadボタン）を枠の最上部に固定し、
      // 残りは背景のみ（下に重なる工程ノードのプロキシアイテムがそのまま見えるようにする）
      QVBoxLayout* groupOuterLay = new QVBoxLayout(content);
      groupOuterLay->setContentsMargins(8, 4, 8, 4);
      groupOuterLay->setSpacing(0);
      QHBoxLayout* groupLay = new QHBoxLayout();
      groupLay->setContentsMargins(0, 0, 0, 0);
      groupLay->setSpacing(6);
      groupOuterLay->addLayout(groupLay, 0);
      groupOuterLay->addStretch(1);

      // このノード（枠）に含まれる工程だけを対象にした3stateチェックボックス
      // （工程一覧UIのm_checkAtOnceButtonGroupとは別系統。飛び地の場合は枠ごとに独立して管理する）
      bool anyChecked = false, anyUnchecked = false;
      for (auto wfType : component) {
        if (auto* memberCb =
                qobject_cast<QCheckBox*>(m_buttonGroup->button(wfType))) {
          if (memberCb->isChecked())
            anyChecked = true;
          else
            anyUnchecked = true;
        }
      }
      Qt::CheckState initialState =
          (anyChecked && anyUnchecked)
              ? Qt::PartiallyChecked
              : (anyChecked ? Qt::Checked : Qt::Unchecked);

      MyTristateCheckBox* groupCb = new MyTristateCheckBox(
          WorkflowManager::instance()->flowGroupLabelName(fgId), content);
      // グループ名のフォントはProject
      // Settings側（flowchartscene.cppのGroupNodeItem::paint()）と揃える
      QFont groupFont = groupCb->font();
      groupFont.setBold(true);
      groupCb->setFont(groupFont);
      groupCb->blockSignals(true);
      groupCb->setCheckState(initialState);
      groupCb->blockSignals(false);
      QList<WorkflowType> members = component;
      // toggled(bool)ではなくclicked()を使う："-"（PartiallyChecked）状態からのクリックは
      // MyTristateCheckBox::nextCheckState()でCheckedへ切り替わるが、Qt内部ではPartiallyChecked時点で
      // 既にisChecked()相当の値がtrue扱いになっており、toggled(bool)が発火しないことがある。
      // clicked()なら実際のクリックのたびに必ず発火するため、その時点のcheckState()を見て判定する
      connect(
          groupCb, &QAbstractButton::clicked, this, [this, members, groupCb]() {
            bool checked = (groupCb->checkState() != Qt::Unchecked);
            for (auto wfType : members) {
              if (auto* memberCb =
                      qobject_cast<QCheckBox*>(m_buttonGroup->button(wfType)))
                memberCb->setChecked(checked);
            }
            refreshList();
          });
      groupLay->addWidget(groupCb, 1);

      // 工程ノードのチェックボックスが切り替わった際に、この枠の3stateチェックボックスを
      // 更新できるよう、逆引き用のキャッシュに登録しておく（onWorkflowButtonClickedから使う）
      m_flowchartGroupCheckboxMembers.insert(groupCb, members);
      for (auto wfType : members)
        m_flowchartWfTypeToGroupCheckbox.insert(wfType, groupCb);

      QLabel* cutCountLabel = new QLabel(content);
      m_flowchartGroupCutCountLabels[fgId].append(cutCountLabel);
      groupLay->addWidget(cutCountLabel, 0);

      QPushButton* reloadButton = new QPushButton(content);
      reloadButton->setIcon(QIcon(":Resources/reload_black.png"));
      reloadButton->setIconSize(QSize(16, 16));
      reloadButton->setFixedSize(20, 20);
      reloadButton->setFlat(true);
      reloadButton->setToolTip(reloadButtonToolTip());
      m_reloadButtonGroup->addButton(reloadButton, (int)fgId);
      groupLay->addWidget(reloadButton, 0);

      MainGroupNodeItem* groupItem = new MainGroupNodeItem(fgId, content);
      scene->addItem(groupItem);
      groupItem->setPos(frameRect.topLeft());
      m_flowchartGroupFrames.append({groupItem, component});
    }
  }

  // リンク（nextFlowsから導出。編集用と異なり常に(from,to)ペアで1本に集約するだけの表示専用）
  for (int i = 0; i < WorkflowManager::instance()->workflowCount(); i++) {
    Workflow wf = WorkflowManager::instance()->getWorkflow(i);
    if (!nodeItems.contains(wf.workflowType)) continue;
    QSet<WorkflowType> targets;
    for (auto& protocol : wf.protocols)
      for (auto& action : protocol.actions)
        for (auto next : action.allNextFlows())
          if (nodeItems.contains(next)) targets.insert(next);
    for (auto next : targets) {
      MainLinkItem* link = new MainLinkItem();
      scene->addItem(link);
      link->setEndpoints(nodeItems.value(wf.workflowType)->outputPortScenePos(),
                         nodeItems.value(next)->inputPortScenePos());
      m_flowchartLinks.append({link, wf.workflowType, next});
    }
  }

  updateFlowchartGeometry();

  // カット検索パネルが開いている間にフローチャートが再構築された場合、ハイライトを再適用する
  // （ノードはこの関数の中で毎回作り直されるため、ハイライト状態は引き継がれない）
  if (m_rightContentStack->currentWidget() == m_sceneKnowledgePanel) {
    for (auto foundType : m_sceneKnowledgePanel->foundWorkflowTypes()) {
      if (m_flowchartNodeItems.contains(foundType))
        m_flowchartNodeItems.value(foundType)->setHighlighted(true);
    }
  }

  m_mainRolePalette->refresh();
}

void MyWindow::onCurrentPrjChanged(const QString& newPrjName) {
  // この中でワークフローの読み込みを行う
  WorkflowManager::instance()->switchCurrentProject(newPrjName);

  // 左パネル（一覧／フローチャートのどちらか現在のモード）を再構築する
  rebuildLeftPanelContent();

  // 話数（Episode）コンボボックスを最新の内容に作り直す
  bool hasEpisodes = WorkflowManager::instance()->hasEpisodes();
  m_episodeRow->setVisible(hasEpisodes);
  m_table->applyColumnVisibility();
  m_episodeCombo->blockSignals(true);
  m_episodeCombo->clear();
  if (hasEpisodes) {
    QStringList episodeNames = WorkflowManager::instance()->episodeNames();
    for (auto& name : episodeNames) m_episodeCombo->addItem(name, name);
    // 2話数以上の場合のみ、全話数をまとめて閲覧・操作する項目を追加する
    // （データはvalue()代わりに空文字列を使い、通常の話数名と区別する）
    if (episodeNames.size() >= 2)
      m_episodeCombo->addItem(tr("All Episodes"), QString());

    QString savedEpisode =
        UserSettings::instance()->loadCurrentEpisode(newPrjName);
    int idx = m_episodeCombo->findData(savedEpisode);
    m_episodeCombo->setCurrentIndex(idx >= 0 ? idx : 0);
  }
  m_episodeCombo->blockSignals(false);

  // シグナルをブロックして選択していたため、WorkflowManager側の状態をここで明示的に合わせる
  // （All Episodesモードの場合、個々の話数への切り替えはrefreshList()側で行う）
  if (hasEpisodes && !m_episodeCombo->currentData().toString().isEmpty()) {
    WorkflowManager::instance()->setCurrentEpisode(
        m_episodeCombo->currentData().toString());
    WorkflowManager::instance()->loadCutList();
  } else if (!hasEpisodes) {
    WorkflowManager::instance()->setCurrentEpisode(QString());
  }

  // 話数が確定したので、条件パスのツールチップ（[EPISODE]マクロを含む）を更新する
  updateWorkflowTooltips();

  // Registerダイアログのコンボボックスも、切り替え後のプロジェクトの内容に作り直す
  m_registerDialog->refresh();
  // Retakeダイアログの送り先ボタンも、切り替え後のプロジェクトの工程グループに作り直す
  m_retakeDialog->refresh();

  // 右のリストの更新
  refreshList();
}

void MyWindow::updateWorkflowTooltips() {
  for (auto button : m_buttonGroup->buttons()) {
    WorkflowType wfType =
        static_cast<WorkflowType>(button->property("PPM_WfType").toInt());
    Workflow wf        = WorkflowManager::instance()->workflow(wfType);
    QString tooltipStr = tr("Conditional file(s):");
    for (int protocolId = 0; protocolId < wf.protocols.size(); protocolId++)
      tooltipStr += tr("\n- %1").arg(
          WorkflowManager::instance()->decodeConditionPathForToolTip(
              wf.protocols.at(protocolId).conditionPath));
    button->setToolTip(tooltipStr);

    // フローチャートモードでは、チェックボックスの小さい領域だけでなくノード全体をマウスオーバー
    // したときにもツールチップが出るよう、グラフィックスアイテム自体にも同じ内容を設定する
    if (auto* item = m_flowchartNodeItems.value(wfType, nullptr))
      item->setToolTip(tooltipStr);
  }
}

void MyWindow::onCurrentEpisodeChanged() {
  QString selected =
      m_episodeCombo->currentData().toString();  // 空文字列 = All Episodes
  UserSettings::instance()->saveCurrentEpisode(
      WorkflowManager::instance()->currentProjectName(), selected);
  if (!selected.isEmpty()) {
    WorkflowManager::instance()->setCurrentEpisode(selected);
    WorkflowManager::instance()->loadCutList();
  }
  // 話数が変わったので、条件パスのツールチップ（[EPISODE]マクロを含む）を更新する
  // （All
  // Episodesモードの場合、m_currentEpisodeはrefreshList()内で元の値に復元される）
  updateWorkflowTooltips();
  // All
  // Episodesモードでの話数ごとのsetCurrentEpisode/loadCutListはrefreshList()内で行う
  refreshList();
}

void MyWindow::onWorkflowButtonClicked(QAbstractButton* button) {
  // クリックしたボタンが属するグループのStateを必要なら切り替える
  WorkflowType wfType =
      static_cast<WorkflowType>(button->property("PPM_WfType").toInt());
  FlowGroupId fgId = WorkflowManager::instance()->workflow(wfType).groupId;

  QCheckBox* checkAtOnceButton =
      dynamic_cast<QCheckBox*>(m_checkAtOnceButtonGroup->button(fgId));

  // checkAtOnceButtonはフローチャートモードのノードには存在しない
  // （こちらは工程グループノードの3stateチェックボックスが別途担う）ため、その場合はスキップする
  if (checkAtOnceButton) {
    if (checkAtOnceButton->checkState() == Qt::PartiallyChecked) {
      Qt::CheckState newState =
          ((button->isChecked()) ? Qt::Checked : Qt::Unchecked);

      // ボタンの状況をチェックする
      for (auto but : m_buttonGroup->buttons()) {
        WorkflowType tmpWfType =
            static_cast<WorkflowType>(but->property("PPM_WfType").toInt());
        FlowGroupId tmpFgId =
            WorkflowManager::instance()->workflow(tmpWfType).groupId;
        if (fgId != tmpFgId) continue;

        if (but->isChecked() && newState == Qt::Checked) continue;
        if (!but->isChecked() && newState == Qt::Unchecked) continue;

        newState = Qt::PartiallyChecked;
        break;
      }

      if (newState != Qt::PartiallyChecked)
        checkAtOnceButton->setCheckState(newState);
    } else
      checkAtOnceButton->setCheckState(Qt::PartiallyChecked);
  }
  // フローチャートモードの工程グループノードの3stateチェックボックスは、個々のクリックではなく
  // refreshList()内で一括して更新する（役職パレットからの一括操作等、このハンドラを経由しない
  // 変更経路もあるため）

  refreshList();
}

void MyWindow::onCheckAtOnceButtonClicked(QAbstractButton* checkAtOnceButton) {
  bool on = (dynamic_cast<QCheckBox*>(checkAtOnceButton)->checkState() ==
             Qt::Checked);

  int groupId = m_checkAtOnceButtonGroup->id(checkAtOnceButton);

  for (auto& button : m_buttonGroup->buttons()) {
    WorkflowType wfType =
        static_cast<WorkflowType>(button->property("PPM_WfType").toInt());
    FlowGroupId fgId = WorkflowManager::instance()->workflow(wfType).groupId;
    if (groupId != fgId) continue;

    button->setChecked(on);
  }
  refreshList();
}

void MyWindow::onLockButtonToggled(int id, bool lock) {
  QAbstractButton* button = m_buttonGroup->button(id);
  if (!button) return;
  WorkflowType wfType =
      static_cast<WorkflowType>(button->property("PPM_WfType").toInt());

  m_table->toggleLocks(wfType, lock);
}

QCheckBox* MyWindow::createLockButton(const Workflow& wf, bool locked,
                                      bool withBackground) {
  QCheckBox* lockButton = new QCheckBox();
  QString styleSheet;
  // 一覧表示では、工程名の帯に合わせて工程の色の背景を付ける
  if (withBackground)
    styleSheet += QString(
                      "QCheckBox{"
                      "background-color:rgb(%1,%2,%3);"
                      "border: 1px solid rgb(%1,%2,%3);"
                      "}")
                      .arg(wf.color.red())
                      .arg(wf.color.green())
                      .arg(wf.color.blue());
  styleSheet +=
      "QCheckBox::indicator:unchecked{"
      "image: url(:/Resources/unlock.png);"
      "}"
      "QCheckBox::indicator:unchecked:hover{"
      "image: url(:/Resources/unlock_hover.png);"
      "}"
      "QCheckBox::indicator:checked{"
      "image: url(:/Resources/lock.png);"
      "}"
      "QCheckBox::indicator:checked:hover{"
      "image: url(:/Resources/lock_hover.png);"
      "}";
  lockButton->setStyleSheet(styleSheet);
  lockButton->setToolTip(
      tr("Lock: Disable the actions and the checkboxes of the scenes in this "
         "workflow, to prevent operating them by mistake."));
  m_lockButtonGroup->addButton(lockButton, wf.workflowType);
  lockButton->setProperty("PPM_WfType", wf.workflowType);
  lockButton->setChecked(locked);
  return lockButton;
}

namespace {
// QGraphicsProxyWidgetに埋め込んだウィジェットの大きさを変える。埋め込んだウィジェットの
// setFixedSize()だけではプロキシの大きさが追従せず、中身が元の幅で切れてしまうため、
// プロキシ側の最小・最大サイズと大きさも合わせる
void resizeProxyWidget(QGraphicsProxyWidget* proxy, const QSize& size) {
  QWidget* w = proxy->widget();
  if (!w || w->size() == size) return;
  w->setFixedSize(size);
  proxy->setMinimumSize(size);
  proxy->setMaximumSize(size);
  proxy->resize(size);
}
}  // namespace

void MyWindow::updateFlowchartGeometry() {
  // 工程ノード：レイアウトの推奨サイズ（非表示のカット数バッジは含まれない）から幅を求める
  for (auto* nodeItem : m_flowchartNodeItems) {
    QWidget* content = nodeItem->widget();
    if (!content || !content->layout()) continue;
    content->layout()->invalidate();
    int w = qMax(FlowchartNodeMinWidth, content->layout()->sizeHint().width());
    resizeProxyWidget(nodeItem, QSize(w, content->height()));
  }

  // 工程グループの枠：所属する工程ノードを囲む大きさに合わせ直す（rebuildFlowchartPanel()と同じ計算）
  for (auto& frame : m_flowchartGroupFrames) {
    QRectF bounds;
    bool first = true;
    for (auto wfType : frame.members) {
      MainWorkflowNodeItem* nodeItem = m_flowchartNodeItems.value(wfType);
      if (!nodeItem) continue;
      QRectF r =
          nodeItem->mapRectToScene(nodeItem->widgetRect().adjusted(-1, -1, 1, 1));
      bounds = first ? r : bounds.united(r);
      first  = false;
    }
    if (first || !frame.item->widget()) continue;
    const qreal margin = 20, labelH = 30;
    QRectF frameRect(bounds.topLeft() - QPointF(margin, margin + labelH),
                     bounds.size() + QSizeF(margin * 2, margin * 2 + labelH));
    resizeProxyWidget(frame.item, frameRect.size().toSize());
    frame.item->setPos(frameRect.topLeft());
  }

  // リンク：工程ノードの幅が変わると上辺・下辺の中央の位置も変わるため、端点を付け直す
  for (auto& link : m_flowchartLinks) {
    MainWorkflowNodeItem* fromItem = m_flowchartNodeItems.value(link.from);
    MainWorkflowNodeItem* toItem   = m_flowchartNodeItems.value(link.to);
    if (fromItem && toItem)
      link.item->setEndpoints(fromItem->outputPortScenePos(),
                              toItem->inputPortScenePos());
  }
}

QString MyWindow::reloadButtonToolTip() const {
  return tr("Reload: Look for the cue files of all the scenes in this "
            "workflow group again, and rebuild the scene lists of its "
            "workflows.");
}

// この工程グループについて、詳細進捗ファイルを作り直す
void MyWindow::onReloadButtonClicked(int groupId) {
  m_table->simpleSearch((FlowGroupId)groupId);
  // m_table->search((FlowGroupId)groupId);
  refreshList();
}

void MyWindow::onFlowGroupsChanged(const QList<FlowGroupId>& fgIds) {
  for (auto fgId : fgIds) m_table->simpleSearch(fgId);
  refreshList();
}

// プロジェクト設定ダイアログのSave後の共通処理
void MyWindow::applySavedProjectSettings(const QString& newPrjName) {
  // プロジェクト選択コンボを実体に合わせる（新規プロジェクト名なら一覧に追加する）
  if (m_prjCombo->findText(newPrjName) < 0) m_prjCombo->addItem(newPrjName);
  m_prjCombo->blockSignals(true);
  m_prjCombo->setCurrentText(newPrjName);
  m_prjCombo->blockSignals(false);

  // プロジェクトが変わっていない場合もWorkflow/FlowGroup構成が変更されている可能性があるため、
  // 常にプロジェクトの読み込みと左側のワークフロー一覧の再構築を行う
  // （RegisterDialogのコンボボックスもonCurrentPrjChanged内で更新される）
  onCurrentPrjChanged(newPrjName);

  // 新しいプロジェクト設定に基づいて、工程の詳細進捗を現在のファイルから全工程グループ分更新する
  // （MyWindow::onReloadButtonClickedと同じ処理を全工程グループに対して行う）
  for (auto fgId : WorkflowManager::instance()->flowGroupIds())
    m_table->simpleSearch(fgId);
  refreshList();
}

#ifndef Q_OS_MACOS
void MyWindow::onSettingsButtonClicked() {
  QMenu menu(this);
  QAction* projectSettingsAction = menu.addAction(tr("Project Settings"));
  QAction* preferencesAction     = menu.addAction(tr("Preferences"));
  QAction* aboutAction           = menu.addAction(tr("About PPM"));
  QAction* chosen                = menu.exec(m_openSettingsButton->mapToGlobal(
      QPoint(0, m_openSettingsButton->height())));
  if (chosen == projectSettingsAction)
    openProjectSettings();
  else if (chosen == preferencesAction)
    m_userSettingsPopup->exec();
  else if (chosen == aboutAction)
    onAboutTriggered();
}

#endif

void MyWindow::openProjectSettings(int category) {
  if (!m_projectSettingsDialog)
    m_projectSettingsDialog = new ProjectSettingsDialog(this);

  if (m_projectSettingsDialog->isVisible()) {
    if (category >= 0) m_projectSettingsDialog->selectCategory(category);
    m_projectSettingsDialog->raise();
    return;
  }
  m_projectSettingsDialog->refresh();
  if (category >= 0) m_projectSettingsDialog->selectCategory(category);
  m_projectSettingsDialog->exec();

  // Saveボタンでは閉じずに保存できるようになったため、ダイアログはCloseで閉じられる度に
  // 必ずQDialog::Rejectedを返す。保存が行われたかどうかはsavedProjectName()で判定する
  // （空文字列なら、一度もSaveされずにCloseされた＝変更なし）
  if (m_projectSettingsDialog->savedProjectName().isEmpty()) return;

  // Saveされたプロジェクトが今回開いていたプロジェクトと異なる場合、
  // Save後はそちらがカレントプロジェクトになる（設定を別プロジェクトにコピー保存した扱い）
  applySavedProjectSettings(m_projectSettingsDialog->savedProjectName());
}

void MyWindow::onSceneSearchClicked() {
  QString cutName = m_sceneSearchEdit->text().trimmed();
  if (cutName.isEmpty()) return;

  if (!m_sceneKnowledgePanel->loadScene(cutName)) {
    QMessageBox::warning(this, tr("Warning"),
                         tr("The searched scene was not found."));
    return;
  }

  m_rightContentStack->setCurrentWidget(m_sceneKnowledgePanel);

  // フローチャートUIが表示されているときは、所属工程があればハイライトし、
  // ハイライトしたノード全体が中央に収まるようパン・ズームする
  for (auto* nodeItem : m_flowchartNodeItems.values())
    nodeItem->setHighlighted(false);
  if (m_flowchartModeActive) {
    QRectF highlightBounds;
    for (auto foundType : m_sceneKnowledgePanel->foundWorkflowTypes()) {
      if (!m_flowchartNodeItems.contains(foundType)) continue;
      MainWorkflowNodeItem* nodeItem = m_flowchartNodeItems.value(foundType);
      nodeItem->setHighlighted(true);
      highlightBounds =
          highlightBounds.isNull()
              ? nodeItem->sceneBoundingRect()
              : highlightBounds.united(nodeItem->sceneBoundingRect());
    }
    if (!highlightBounds.isNull())
      m_flowchartView->focusOnRect(highlightBounds);
  }
}

void MyWindow::showRegisterSceneMenu(WorkflowType wfType,
                                     const QPoint& globalPos) {
  QMenu menu(this);
  QAction* registerAction =
      menu.addAction(tr("Register a scene starting from this workflow"));
  if (menu.exec(globalPos) != registerAction) return;

  RegisterSceneDialog dialog(wfType, this);
  dialog.exec();
  if (!dialog.didRegister()) return;

  // 登録によって影響を受けた工程グループ（登録先、および元々別グループに登録されていた場合は
  // そちらも）の詳細進捗を、実際のキューファイルの状態から更新し直す
  m_table->simpleSearch(dialog.registeredGroupId());
  if (dialog.previousGroupId() != Group_None &&
      dialog.previousGroupId() != dialog.registeredGroupId())
    m_table->simpleSearch(dialog.previousGroupId());

  refreshList();
}

//------------------------------------

void MyWindow::onAboutTriggered() {
  AboutPopup aboutPopup(this);
  aboutPopup.exec();
}
