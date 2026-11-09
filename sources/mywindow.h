
#include <QWidget>
#include <QTableWidget>
#include <QDialog>
#include <QDir>
#include <QStringList>
#include <QDateTime>
#include <QMap>
#include <QListIterator>
#include <QPushButton>

#include <QCheckBox>
#include "workflow.h"

class QButtonGroup;
class UserSettingsPopup;
class QComboBox;
class QAbstractButton;
class RetakeDialog;
class QScrollArea;
class ProjectSettingsDialog;
class QStackedWidget;
class QLabel;
class QSplitter;
class MainFlowChartView;
class MainRolePaletteWidget;
class MainWorkflowNodeItem;
class MainGroupNodeItem;
class MainLinkItem;
class QLineEdit;
class SceneKnowledgePanel;

struct DataInfo {
  WorkflowType workflowType = Wf_Undefined;
  int protocolId            = -1;
  QString episode     = "";  // 話数機能を使わないプロジェクトでは空文字列のまま
  QString sequence    = "";
  QString sceneNumber = "";
  QString thumbnailPath =
      "";  // Scene Thumbnail Pathが未設定のプロジェクトでは空文字列のまま
  QDateTime lastModifiedTime;
  QString staffName = "";
  QStringList paths;
  QStringList fileNames;
  QString takeSuffix = "";
};

// Googleスプレッドシートの更新完了シグナルを受け取って
// Undo情報の格納と有効化を行う
class AsynchronousUndoButton : public QPushButton {
  Q_OBJECT
public:
  AsynchronousUndoButton(bool hasGoogleStep, QWidget* parent = nullptr);
  void setWaitState();
protected slots:
  // requestIdは、対応するStepExecutor_InputGoogleSpreadsheetがこのアクション内で
  // 何番目のステップかを示す（GoogleGateway::updateSingleCell呼び出し時に渡したもの）。
  // "PPM_PendingGoogleStepPos"プロパティ（未完了のGoogleステップの位置一覧）と突き合わせて、
  // このボタン・このアクションが待っている通知かどうかを判定する
  void onGoogleSpreadSheetInput(QString sceneNumber, QStringList previousValues,
                                int requestId);
  void onGoogleSpreadSheetInputFailed(QString sceneNumber, int requestId);

private:
  // 保留中のGoogleステップが全て完了（成功または失敗）したら、ボタンを有効化する
  void finishIfAllGoogleStepsDone();
};

class MyTable : public QTableWidget {
  Q_OBJECT

  // チェックボックスが１つだけONのときはfalseを返し、
  // 通常のonActionButtonClickedの処理を続ける
  bool onActionMulti();
  // 開くボタンに特殊な操作を追加した場合はtrueを返す。
  // 当面、紙LOの場合にサブフォルダのTNZを開くメニューを用意する
  bool setSpecialOpenButton(QPushButton* openButton, DataInfo info);

  int findRow(int ppm_row, int col = 1);

  // Thumbnail列のサイズ。小＝ボタン等に合わせた通常の行高、大＝その2倍
  bool m_thumbnailSizeLarge = false;
  // 「小」サイズでの行の高さ（コンストラクタ時点のverticalHeader()->defaultSectionSize()を基準にする）
  int m_thumbnailBaseRowHeight = -1;

public:
  MyTable(QWidget* parent = nullptr);
  // void search(QList<int> ids, QList<int> lockedWfIds, const QMap<int, int>&
  // wfId_cbId_map);

  // 詳細進捗を作り直す
  void simpleSearch(FlowGroupId fgId);
  // キャッシュされた詳細を元にリストを作り直す。appendModeがtrueのときは
  // 既存の行をクリアせずに追加する（All
  // Episodesモードで話数ごとに呼び出すため）
  void doRefresh(const QList<WorkflowType>& checkedWfTypes,
                 const QList<WorkflowType>& lockedWfTypes,
                 const QMap<WorkflowType, int>& wfType_cbId_map,
                 bool appendMode = false);

  void registerItem(int row, DataInfo info, bool isLocked, int checkBoxId);
  void toggleLocks(WorkflowType wfType, bool lock);

  // Media/Episode/Thumbnail/Last Modified
  // Date/Informationの各列について、現在のプロジェクトの
  // 状態（HasEpisodes／Scene Thumbnail
  // Pathの有無）とUserSettingsに保存された表示設定に基づいて
  // 表示・非表示を再適用する。プロジェクト切り替え時、および列の表示設定を切り替えた直後に呼ぶ
  void applyColumnVisibility();
  // Thumbnail列のサイズ（小／大）を切り替える。既存行のサムネイル・行高もその場で更新する
  void setThumbnailSizeLarge(bool isLarge);
  // 現在の設定（小／大）に応じたThumbnail列の行の高さ
  int currentThumbnailRowHeight() const {
    return m_thumbnailSizeLarge ? m_thumbnailBaseRowHeight * 2
                                : m_thumbnailBaseRowHeight;
  }

private:
  // 開くボタンでファイルを開けなかったときのメッセージ
  void showOpenWithDefaultAppFailedMessage(const QString& path);
  void showStartProgramFailedMessage(const QString& program,
                                     const QString& path);

public slots:  // このへんの実装と、ソート機能つくるところから
  void onOpenButtonClicked();
  void onOpenXdtsButtonClicked();
  void onFolderButtonClicked();
  void onOpenLogButtonClicked();
  void onActionButtonClicked();
  void onRewindButtonClicked();
  // void onFlowCompleteButtonClicked();
  void onUndoButtonClicked();
  // void onFlowCompleteUndoButtonClicked();

  void onCheckBoxClicked(bool);
  void onHeaderContextMenuRequested(const QPoint& pos);
};

class RegisterDialog : public QDialog {
  Q_OBJECT

  QLineEdit* m_cutNameEdit;
  QComboBox *m_moveFlowGroupCombo, *m_infoCombo;
  QLabel*
      m_cutNameWarningLabel;  // 使えないカット名があるときに、理由を表示する
  QPushButton *m_moveFlowGroupButton, *m_registerInfoButton,
      *m_releaseInfoButton;

  // カット名の入力欄から、カット名の一覧を取り出す（前後の空白は除く）
  QStringList cutNames() const;
  // 使えないカット名があれば、警告を表示し、ボタンを無効にする
  void updateButtonsEnabled();

  // 任意の値をInfoに格納するようなカット情報で表示して用いる（バンクカット番号など）
  QLineEdit* m_infoValueEdit;

public:
  RegisterDialog(QWidget* parent = nullptr);
  // m_moveFlowGroupCombo/m_infoComboの項目を、現在のプロジェクトの
  // 工程グループ／SceneInfoTagの一覧に基づいて作り直す
  void refresh();
signals:
  // カットの所属が変わり、これらの工程グループの詳細進捗を作り直す必要がある
  void flowGroupsChanged(const QList<FlowGroupId>& fgIds);
public slots:
  void onMoveFlowGroupButtonClicked();
  void onRegisterInfo();
  void onReleaseInfo();
  void onInfoComboActivated();
  void onCutNamesEdited();
};

class MyTristateCheckBox : public QCheckBox {
  Q_OBJECT
public:
  MyTristateCheckBox(const QString& text, QWidget* parent = nullptr);

protected:
  void nextCheckState() override;
};

class MyWindow : public QWidget {
  Q_OBJECT
  MyTable* m_table;
  QButtonGroup *m_buttonGroup, *m_expandButtonGroup, *m_checkAtOnceButtonGroup,
      *m_lockButtonGroup, *m_reloadButtonGroup;
  RegisterDialog* m_registerDialog;
  RetakeDialog* m_retakeDialog;
  UserSettingsPopup* m_userSettingsPopup;

  QScrollArea* m_wfButtonsScrollArea;
  QComboBox* m_prjCombo;
  QComboBox* m_episodeCombo;  // 話数機能ON時のみ表示。「All
                              // Episodes」の項目データは空文字列
  QWidget*
      m_episodeRow;  // m_episodeComboを含む行全体。話数機能OFF時はこれごと隠す

  ProjectSettingsDialog* m_projectSettingsDialog;

  QMap<WorkflowType, int> m_wfType_cbId_map;

  // --- 左パネル：一覧／フローチャート切り替え ---
  bool m_flowchartModeActive = false;
  QPushButton* m_viewModeToggleButton;
  // Project:コンボ／Episode行／各種ボタン。中身はモードに依らず共通で、m_topControlsContainerの
  // 実際の横幅（m_mainSplitterの区切り線移動で変わる）に応じて4行／2行／1行にアダプティブに
  // 組み直す（rebuildTopControlsLayout()。widthはeventFilter()経由のリサイズ通知で検知する）
  QWidget* m_topControlsContainer;
  // rebuildTopControlsLayout()が直近に選んだ行数（4/2/1）。リサイズのたびに毎回作り直さずに済むよう、
  // 実際に行数が変わる場合のみレイアウトを組み直すために使う
  int m_topControlsRowCount = -1;
  QStackedWidget* m_leftContentStack;
  // 左パネル／m_table間の分割。起動時に前回終了時のサイズをUserSettingsから復元する
  QSplitter* m_mainSplitter;
  MainFlowChartView* m_flowchartView;
  MainRolePaletteWidget* m_mainRolePalette;
  // フローチャートモードでカット数を表示しているラベル。refreshList()内で値を更新するために保持する
  QMap<FlowGroupId, QList<QLabel*>>
      m_flowchartGroupCutCountLabels;  // 飛び地の場合、同じfgIdに複数枠
  QMap<WorkflowType, QLabel*> m_flowchartCutCountLabels;
  // 工程→所属グループIDのキャッシュ。rebuildFlowchartPanel()で一度だけ構築する。
  // WorkflowManager::workflow(type)（Workflow構造体のディープコピーを伴う）をrefreshList()の
  // 中で毎回呼ばずに済ませるためのもの
  QMap<WorkflowType, FlowGroupId> m_flowchartWfTypeToGroupId;
  // 工程ノードのチェックボックスが切り替わった際に、それを含む工程グループノードの
  // 3stateチェックボックスを更新するためのキャッシュ（rebuildFlowchartPanel()で構築）。
  // 飛び地の場合は枠ごとに別のチェックボックスになるため、工程は必ずどれか1つの枠に対応する
  QMap<WorkflowType, QCheckBox*> m_flowchartWfTypeToGroupCheckbox;
  QMap<QCheckBox*, QList<WorkflowType>> m_flowchartGroupCheckboxMembers;
  // 工程ノードのグラフィックスアイテム。updateWorkflowTooltips()がノード全体（チェックボックスの
  // 小さい領域だけでなく）にツールチップを設定できるよう、rebuildFlowchartPanel()で構築する
  QMap<WorkflowType, MainWorkflowNodeItem*> m_flowchartNodeItems;
  // 工程ノードの幅がカット数に合わせて変わったときに、工程グループの枠とリンクを
  // 合わせ直すためのキャッシュ（rebuildFlowchartPanel()で構築する）
  struct FlowchartGroupFrame {
    MainGroupNodeItem* item;
    QList<WorkflowType> members;
  };
  QList<FlowchartGroupFrame> m_flowchartGroupFrames;
  struct FlowchartLink {
    MainLinkItem* item;
    WorkflowType from, to;
  };
  QList<FlowchartLink> m_flowchartLinks;

  // トップコントロール行を構成する個々のウィジェット（rebuildTopControlsLayout()で
  // 行数が変わるたびに再配置される。再生成せず使い回すことで、その都度のリークを避ける）
  QLabel* m_projectLabel;  // "Project:"ラベル
  QPushButton* m_refreshButton;
  QPushButton* m_registerButton;
  QPushButton* m_retakeButton;
#ifndef Q_OS_MACOS
  void onSettingsButtonClicked();
  QPushButton* m_openSettingsButton;  // 歯車アイコン。クリックするとProject
                                      // Settings/Preferencesのメニューを出す
#endif
  QLineEdit* m_sceneSearchEdit;
  QPushButton* m_sceneSearchButton;  // 虫眼鏡アイコン

  // 右側パネル：通常はMyTable、カット検索時はSceneKnowledgePanelに切り替える
  QStackedWidget* m_rightContentStack;
  SceneKnowledgePanel* m_sceneKnowledgePanel;

  // フローチャートの工程ノード／工程一覧の項目の右クリックメニューを表示する。
  // 「この工程から始まるカットを登録する」コマンドを実行するとRegisterSceneDialogを開き、
  // 実際に登録が行われた場合は関係する工程グループのsimpleSearch()とrefreshList()を呼び直す
  void showRegisterSceneMenu(WorkflowType wfType, const QPoint& globalPos);

  // プロジェクト設定ダイアログのSave後の共通処理
  // （保存先プロジェクトへの切り替え＋一覧再構築＋詳細進捗の更新）
  void applySavedProjectSettings(const QString& newPrjName);

  // 現在のチェック／ロック解除状態をUserSettingsへ保存する（closeEvent、モード切替の両方で使う）
  void saveCurrentCheckedState();
  // m_topControlsContainerの現在の横幅に応じて、内部レイアウトを4行／2行／1行に組み直す
  // （幅による行数の判定結果がm_topControlsRowCountと変わらない場合は何もしない）
  void rebuildTopControlsLayout();
  // m_buttonGroup等の全QButtonGroupをクリアしてから、現在のモードに応じて
  // rebuildListPanel()/rebuildFlowchartPanel()のどちらかを呼ぶ
  void rebuildLeftPanelContent();
  // 既存の一覧UI（チェックボックス縦並び）を構築する。onCurrentPrjChangedに元々あった処理の抽出
  void rebuildListPanel();
  // フローチャートUIのノード／グループ／リンクを構築する
  void rebuildFlowchartPanel();
  // 工程グループの更新ボタンのツールチップ（一覧／フローチャートで共通）
  QString reloadButtonToolTip() const;
  // フローチャートの工程ノードの幅を、今の中身（カット数のバッジの有無・桁数）に合わせ、
  // それに合わせて工程グループの枠の大きさとリンクの端点を更新する
  void updateFlowchartGeometry();
  // 工程のロックボタンを作り、m_lockButtonGroupに登録する（一覧／フローチャートで共通）。
  // withBackgroundがtrueのときは、工程の色の背景を付ける
  QCheckBox* createLockButton(const Workflow& wf, bool locked,
                              bool withBackground);

public:
  MyWindow();

protected:
  void closeEvent(QCloseEvent* event) override;
  // m_topControlsContainerのリサイズ（m_mainSplitterの区切り線移動含む）を検知し、
  // rebuildTopControlsLayout()で行数を再判定するために使う
  bool eventFilter(QObject* watched, QEvent* event) override;

public slots:
  void refreshList();
  void updateWorkflowTooltips();
  void onRefresh();
  void onCurrentPrjChanged(const QString&);
  void onCurrentEpisodeChanged();
  void onWorkflowButtonClicked(QAbstractButton*);
  void onCheckAtOnceButtonClicked(QAbstractButton*);
  void onLockButtonToggled(int, bool);
  void onReloadButtonClicked(int);
  // カット登録・リテイクでカットの所属が変わった工程グループの詳細進捗を作り直す
  void onFlowGroupsChanged(const QList<FlowGroupId>& fgIds);
  void openProjectSettings(int category = -1);
  void onViewModeToggled();
  void onSceneSearchClicked();
  void onAboutTriggered();
};