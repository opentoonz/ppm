#pragma once
#include <QDialog>
#include <QWidget>
#include <QColor>
#include <QPushButton>
#include <QLineEdit>
#include <QStringList>
#include <QSyntaxHighlighter>
#include <QPlainTextEdit>
#include <functional>

#include "workflow.h"  // WorkflowType / FlowGroupId

class QTableWidget;
class QListWidget;
class QTableWidgetItem;
class QVBoxLayout;
class QCheckBox;
class QCompleter;
class QKeyEvent;
class QGroupBox;
class QTextEdit;
class FlowchartPage;
class MacroLineEdit;
class QPlainTextEdit;

class FolderPathField : public QWidget {
  Q_OBJECT
  QLineEdit* m_edit;

public:
  FolderPathField(QWidget* parent = nullptr);
  void setText(const QString&);
  QString text() const;
protected slots:
  void onBrowserButtonClicked();
signals:
  void pathChanged();
};

class BasicsPage : public QWidget {
  Q_OBJECT

  QLineEdit *m_projectTitleEdit, *m_cutNumberPrefixEdit, *m_gasUrlEdit;
  QTextEdit* m_projectDescriptionEdit;
  FolderPathField *m_projectRootPathEdit, *m_logRootPathEdit,
      *m_projectInfoFolderPathEdit;
  QGroupBox*
      m_episodesGroupBox;  // チェック可能なグループボックスでHasEpisodesのON/OFFを兼ねる
  QCheckBox*
      m_useSequenceCheck;  // [SEQUENCE]マクロと、100カット毎のサブフォルダ仕分けを使うか
  QLineEdit*
      m_episodeNamesEdit;  // カンマまたはセミコロン区切りでEpisodeNamesを入力する
  MacroLineEdit* m_sceneThumbnailPathEdit;
  // Google Chatの投稿先の名前（カンマかセミコロン区切り）
  QLineEdit* m_chatDestinationsEdit;

public:
  BasicsPage(QWidget* parent = nullptr);
  void refresh();

protected slots:
  void onFieldEdited();
  void onChatFetchClicked();
  void onChatTestClicked();
  void onChatDestinationsEdited();
};

class PathMacrosPage : public QWidget {
  Q_OBJECT

  QTableWidget* m_table;

public:
  PathMacrosPage(QWidget* parent = nullptr);
  void refresh();

protected slots:
  void onAddButtonClicked();
  void onRemoveButtonClicked();
  void onItemChanged(QTableWidgetItem* item);
};

class WfGroupsPage : public QWidget {
  Q_OBJECT

  QTableWidget* m_table;
  // 各行が対応するFlowGroupId（テーブルの行順と対応させて保持する）
  QList<unsigned int> m_rowIds;

public:
  WfGroupsPage(QWidget* parent = nullptr);
  void refresh();

  // フローチャート型ダイアログの詳細パネルとして使う際、指定グループの行を選択状態にする
  void selectGroup(FlowGroupId fgId);

protected slots:
  void onAddButtonClicked();
  void onRemoveButtonClicked();
  void onItemChanged(QTableWidgetItem* item);
};

// クリックするとQColorDialogを開き、選択した色をボタンの背景に表示する
class ColorButton : public QPushButton {
  Q_OBJECT
  QColor m_color;

public:
  ColorButton(QWidget* parent = nullptr);
  void setColor(const QColor& color);
  QColor color() const { return m_color; }
signals:
  void colorChanged(const QColor&);
protected slots:
  void onButtonClicked();
};

class InfoTagsPage : public QWidget {
  Q_OBJECT

  QTableWidget* m_tagList;
  QVBoxLayout*
      m_detailPanelLayout;  // 詳細パネルを配置する、常に存在する土台のレイアウト
  QWidget*
      m_detailContainer;  // 選択中のタグに応じて都度作り直す詳細パネルの実体
  int m_currentIndex;     // tmpSettings.sceneInfoTags内の選択中インデックス

  void updateListRow(int index);
  void clearDetailPanel();
  void rebuildDetailPanel();

public:
  InfoTagsPage(QWidget* parent = nullptr);
  void refresh();

protected slots:
  void onAddButtonClicked();
  void onRemoveButtonClicked();
  void onCurrentRowChanged(int currentRow);
};

// テキスト中の既知のマクロ（"[CUTNUMBER]"等）を、全て同じ色で強調表示する。
// 色は、対象のウィジェットの背景色の明るさに合わせて、明るい背景用/暗い背景用を切り替える。
// providerが返す一覧に無い"[...]"（タイプミス等）には色を付けないため、入力ミスに気付きやすい
class MacroHighlighter : public QSyntaxHighlighter {
public:
  MacroHighlighter(QPlainTextEdit* edit,
                   std::function<QStringList()> macroProvider);
  // 強調表示する色（背景の明るさに応じた色）
  static QColor macroColor(const QWidget* widget);

protected:
  void highlightBlock(const QString& text) override;

private:
  QPlainTextEdit* m_edit;
  std::function<QStringList()> m_macroProvider;
};

// "["の入力をトリガーに補完候補を出すQLineEdit。既定ではtmpSettings.pathMacrosのmacroStr一覧
// ＋組み込みマクロ（[ROOT]等）を候補にするが、setMacroCandidateProvider()で差し替えられる
// QLineEditと同じ使い心地の1行入力欄に、マクロの強調表示（文字色）を付けるため、
// QLineEditではなく、1行専用に振る舞わせたQPlainTextEditとして実装している
// （QLineEditは文字ごとの色指定ができないため）。
// text()/setText()/editingFinished()等、QLineEditの同名のAPIを用意している
class MacroLineEdit : public QPlainTextEdit {
  Q_OBJECT
public:
  MacroLineEdit(QWidget* parent = nullptr);
  void setMacroCandidateProvider(std::function<QStringList()> provider);
  // trueにすると、組み込みのマクロを加えず、補完候補のマクロだけに色を付ける
  // （XDTSファイル名など、使えるマクロが限られている欄で使う）
  void setHighlightOnlyCandidates(bool on);
  QString text() const { return toPlainText(); }
  void setText(const QString& text);  // 改行は空白に置き換える
  int cursorPosition() const { return textCursor().position(); }
  void setCursorPosition(int pos);
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

  // 既定の補完候補：Path
  // Macrosページで定義されたマクロ＋実行時に自動置換される組み込みのマクロ
  static QStringList defaultMacroCandidates();
  // 強調表示するマクロの一覧：補完候補に加え、パスの解決時に置き換えられる組み込みのマクロ
  // （[SEQUENCE]はプロジェクトで使う設定のときだけ）
  static QStringList knownMacros(const QStringList& candidates);
  // text中の、knownMacros()にあるマクロの範囲（開始位置と長さ）を返す
  static QList<QPair<int, int>> findMacroRanges(const QString& text,
                                                const QStringList& macros);
signals:
  // Enterキーを押したとき、またはフォーカスを失ったときに発行される（QLineEditと同様）
  void editingFinished();

protected:
  void keyPressEvent(QKeyEvent* event) override;
  void focusOutEvent(QFocusEvent* event) override;
  void insertFromMimeData(const QMimeData* source) override;
protected slots:
  void onCompleterActivated(const QString& text);

private:
  QCompleter* m_completer;
  int m_macroStartPos;  // 補完対象の"["が入力された位置
  std::function<QStringList()> m_candidateProvider;
  bool m_highlightOnlyCandidates = false;
  MacroHighlighter* m_highlighter;
};

// Finderのカラム表示のような3カラム構成で、QList<Workflow>を表示・編集する
class WorkflowPage : public QWidget {
  Q_OBJECT

  // カラム①：Workflow一覧。工程グループごとの見出し行（ドラッグ不可、選択のみ可能）を挟んで、
  // 各工程グループに属するWorkflowが子として並ぶ。tmpSettings.workflowsは常に「工程グループの
  // リスト順（flowGroupInfos.keys()の昇順）でグループ単位にクラスタリングされている」前提を保つ
  QWidget*
      m_col1Widget;  // 一覧＋Add/Removeボタンを含むカラム全体（フローチャート型ダイアログでは非表示にする）
  QTableWidget* m_workflowList;
  int m_currentWfIndex;  // 選択中の行がWorkflow行のときのtmpSettings.workflows内インデックス。見出し行選択時は-1
  FlowGroupId
      m_currentGroupId;  // 選択中の行（見出し行／Workflow行いずれも）が属する工程グループ

  // m_workflowListの行が見出し行かどうか、見出し行が表すgroupIdを判定する
  // （見出し行の列0アイテムにQt::UserRole/UserRole+1として埋め込んだ情報を読む）
  bool isHeaderRow(int row) const;
  FlowGroupId headerRowGroupId(int row) const;
  // tmpSettings.workflows内インデックスに対応するm_workflowList上の行を検索する
  int tableRowForWorkflowIndex(int index) const;
  // 見出し行を含むm_workflowListの行を、tmpSettings.workflows/flowGroupInfosから作り直す
  // （選択状態には触れない）
  void buildWorkflowListRows();
  // m_workflowListの選択行から、m_currentWfIndex/m_currentGroupIdを算出する
  // （行自体の選択操作は行わない）
  void syncCurrentSelectionFromRow(int row);
  // buildWorkflowListRows()を呼び、可能な限り選択中の項目（WorkflowならworkflowType、
  // 見出しならgroupId）を引き継いで選択し直し、m_currentWfIndex/m_currentGroupIdの更新と
  // カラム②～④の再構築まで行う。ドラッグ＆ドロップでの並べ替え・Group変更後に呼ぶ
  void rebuildWorkflowListRows();

  // カラム②：Workflow詳細＋WorkflowProtocol一覧（選択に応じて都度作り直す）。
  // 選択中Protocolの詳細（旧カラム③相当）は、Protocol一覧の下に入れ子で表示する
  // （m_col3Container/m_actionTable/m_col3HostLayoutはm_col2Containerの子になるため、
  //   m_col2Containerを破棄する際にまとめて破棄される点に注意）
  QVBoxLayout* m_col2HostLayout;
  QWidget* m_col2Container;
  QTableWidget* m_protocolTable;
  int m_currentProtocolIndex;

  // 選択中WorkflowProtocolの詳細＋PPM_Action一覧（m_col2Container内に都度作り直される）
  QVBoxLayout* m_col3HostLayout;
  QWidget* m_col3Container;
  QTableWidget* m_actionTable;
  int m_currentActionIndex;

  // カラム③：PPM_Action詳細＋ActionStep一覧＋パラメータ編集（選択に応じて都度作り直す）
  QVBoxLayout* m_col4HostLayout;
  QWidget* m_col4Container;
  QTableWidget* m_stepTable;
  // 選択中アクションの「Next
  // Flows」チェックボックス一覧（m_col4Container内に都度作り直される）
  QListWidget* m_nextFlowsList = nullptr;
  QVBoxLayout* m_stepDetailHostLayout;
  QWidget* m_stepDetailContainer;
  int m_currentStepIndex;

  void updateWorkflowListRow(int index);
  void updateProtocolTableRow(int index);
  void updateActionTableRow(int index);
  void updateStepTableRow(int index);
  // Step_AddProgressで追加される工程を、Next
  // Flows一覧で、色と注記で分かるようにする
  void updateNextFlowsMarks();

  void rebuildColumn2();
  void rebuildColumn3();
  void rebuildColumn4();
  void rebuildStepDetail();

public:
  WorkflowPage(QWidget* parent = nullptr);
  void refresh();

  // フローチャート型ダイアログの詳細パネルとして使うための補助。
  // 一覧カラムを非表示にし、選択はフローチャート側から与える
  void setListColumnVisible(bool visible);
  void selectWorkflowByType(WorkflowType type);

protected slots:
  void onAddWorkflowClicked();
  void onRemoveWorkflowClicked();
};

//--------------------------------

class ProjectSettingsDialog : public QDialog {
  Q_OBJECT

  QListWidget* m_categoryList;
  BasicsPage* m_basicsPage;
  PathMacrosPage* m_pathMacrosPage;
  WfGroupsPage* m_wfGroupsPage;
  InfoTagsPage* m_infoTagsPage;
  WorkflowPage* m_workflowPage;
  FlowchartPage*
      m_flowchartPage;  // "Workflows -
                        // Flowchart"カテゴリ。カラム②③はm_workflowPageと同等のものを内部で再利用する

  QLineEdit* m_projectNameEdit;
  QString
      m_savedProjectName;  // Saveされた際のプロジェクトID名（保存後にMyWindowがプロジェクト切り替えに使う）

  void updateTitle();
  // 現在の内容をppmprjに保存する。プロジェクト名が空等の理由で保存できなかった場合はfalseを返す
  bool doSave();

public:
  ProjectSettingsDialog(QWidget* parent = nullptr);
  static QStringList categoryNames();
  void selectCategory(int row);

  void refresh();
  // Save成功時に保存先として使われたプロジェクトID名。まだSaveされていない場合は空文字列
  QString savedProjectName() const { return m_savedProjectName; }

  // Closeボタン／タイトルバーのxボタン／Escキーで共通して呼ばれる。
  // DirtyFlagが立っている場合は保存するか破棄するかを確認してから閉じる
  void reject() override;

protected slots:
  void onSaveClicked();
};