#pragma once

#include <QString>
#include <QColor>
#include <QList>
#include <QPair>
#include <QObject>
#include <QMap>
#include <QVariant>
#include <QPointF>
#include <QSizeF>
#include <limits>

#include "workflowtypes.h"
#include "sceneinfotag.h"

typedef QPair<QString, QStringList> RenameCommand;

const QString RewindStr = "_r";

// 制作工程グループのID。スタジオごとにプロジェクト設定で自由に定義できるよう、
// 固定のenumではなくユーザーが指定する一意の値として扱う。
typedef unsigned int FlowGroupId;
// 「見つからなかった」ことを表す番兵値。自動採番されるIDは0から始まるため衝突しない。
const FlowGroupId Group_None = std::numeric_limits<FlowGroupId>::max();

enum ActionStepType {
  //---- ファイル操作
  Step_Rename = 0,
  Step_Copy,  // Step_Renameのオリジナルを消さない版
  Step_CopyFolderContents,
  Step_RenameFile,  // 指定したキューファイル以外のファイルを任意の名前にリネームする
  Step_CopyFile,       // ↑のオリジナルを消さない版
  Step_CreateCueFile,  // 空のテキストファイルを作る
  Step_AskDelete,
  Step_CreateEmptyFolder,  // 空のフォルダを作る
  Step_ZipFolder,          // フォルダをZIP圧縮
  //---- 進捗管理
  Step_MoveFlowGroup,
  Step_StartBranchFlow,
  Step_EndBranchFlow,
  Step_SetSceneInfo,
  Step_AddProgress,  // 次工程に、指定した工程を追加する（Step_ConditionalBranch系の中で使い、条件を満たす場合だけ工程を枝分かれさせる）
  Step_ReleaseProgress,  // 詳細進捗を解除する（ペイント完了時に打ち込みを解除するときのみ使用）
  //---- 確認、分岐
  Step_CheckExistence,
  Step_ConditionalBranchByInfo,           // infoに応じて条件を分岐する
  Step_ConditionalBranchByFileExistence,  // ファイルの有無に応じて条件を分岐する
  //---- 外部連携その他
  Step_InputGoogleSpreadsheet,  // Googleスプレッドシートのカット表に入力する
  Step_NotifyGoogleChat,        // Google Chatのスペースへ通知メッセージを送る
  Step_DougaRetakeRouting,  // 動画リテイクのための特別なステップ。メイン／動検／演助チェックの有無を選択する
  Step_COUNT,
  Step_Unidentified = 9999
};
// Q_DECLARE_METATYPE(ActionStepType)

enum MediaType { Digital = 0, Paper, Other };

struct ActionStep {
  ActionStepType type;
  QList<QVariant> params;
  QJsonObject toJson() const;
  static ActionStep fromJson(const QJsonObject& json);
};
// Q_DECLARE_METATYPE(ActionStep)

// アクションの次工程への接続。nextFlowsによるものと、Step_AddProgressによるものがある
struct NextFlowLink {
  WorkflowType type = Wf_Undefined;
  // Step_ConditionalBranch系の入れ子の中にあるStep_AddProgressで追加される場合true（条件を満たしたときだけ繋がる）
  bool conditional = false;
};

struct PPM_Action {
  QString actionName;
  QList<ActionStep> steps;
  // 無条件の次工程。条件付きで追加される次工程は、Step_AddProgressステップが持つ
  QList<WorkflowType> nextFlows;
  // このアクションの、全ての次工程への接続（nextFlowsと、Step_AddProgressで追加される工程。重複は除く）。
  // 工程同士の連結を調べる処理は、nextFlowsを直接見ず、これを使う
  QList<NextFlowLink> nextFlowLinks() const;
  QList<WorkflowType> allNextFlows() const;
  // Step_AddProgressステップで追加される工程だけ（入れ子の中も含む）
  QList<WorkflowType> stepAddedFlows() const;
  // typeを追加するStep_AddProgressステップを取り除く。トップレベルのステップは削除し、
  // 条件分岐の中のステップは「None」にする。取り除いた数を返す
  int removeAddProgressSteps(WorkflowType type);
  QJsonObject toJson() const;
  static PPM_Action fromJson(const QJsonObject& json);
};

// パス条件と、それに対するアクション
struct WorkflowProtocol {
  MediaType mediaType;
  QString conditionPath;  // このパスを満たすファイルが現れたらキューに並べる
  QList<PPM_Action> actions;
  QJsonObject toJson() const;
  static WorkflowProtocol fromJson(const QJsonObject& json);
};

struct Workflow {
  WorkflowType workflowType;  // 自動採番される一意のID。ユーザーは変更できない
  FlowGroupId groupId;
  QString name;
  QColor color;
  // 「条件パスのリネーム連鎖を自動設定」で、後工程のキューファイル名の末尾に付与される文字列。
  // 役職に紐づく接尾辞であることが多い（例：演助チェック→"_ej"、監督チェック→"_k"）
  QString suffix = "";

  // ひとつのWorkflowTypeに複数のプロトコルをまとめる
  QList<WorkflowProtocol> protocols;

  // このワークフローの完了時にREWINDシーン情報タグを解除するか
  // （旧: Wf_LO_Complete/Wf_RoughGen_Complete/Wf_Genga_Completeへの決め打ち）
  bool clearsRewindOnComplete = false;

  QJsonObject toJson() const;
  static Workflow fromJson(const QJsonObject& json);
};

struct FlowGroupInfo {
  QString idName;     // 設定ファイル等に使われる名前
  QString labelName;  // 表示用の名前（翻訳される）
  QString standardCutFolderMacro;
  QString xdtsFileName;

  enum TakeNotationStyle {
    TakeNotation_None = 0,  // このグループではテイク表記を扱わない
    TakeNotation_Rewind,    // [R]（リワインド）表記を使う
    TakeNotation_Take,      // [TAKE]（リテイク番号）表記を使う
    TakeNotation_RTake      // [R_TAKE]表記を使う（撮影工程用）
  };
  TakeNotationStyle takeNotationStyle = TakeNotation_None;
  bool isRetakeGroup                  = false;  // リテイク工程グループかどうか
  bool isRetakeSourceGroup =
      false;  // リテイクの起点となる工程グループかどうか（例：撮影）
  // 「開く」ボタンで、キューファイル自身ではなくこの相対パスで示されるサブフォルダ内のファイルを
  // 開くようにする（空文字列なら従来通りキューファイルの親フォルダを開く）。
  // [PREFIX]/[CUTNUMBER]等のマクロを含められる
  QString subfolderForOpenButton = "";
};

struct PathMacroInfo {
  QString macroStr;
  QString path;
  QString labelName = "";
  bool doObfuscate =
      false;  // 難読化：ユーザーにパスを秘匿するための簡単な暗号化処理を行う
};

// 役職。フローチャートUIの役職パレットから工程ノードを作成する際、
// 工程名・表示色・接尾辞の既定値として使われる
struct Role {
  unsigned int id =
      0;  // 作成順に割り振られる内部管理用の値。UIでは不可視でよい
  QString labelName;
  QString suffix;
  QColor color;
};

//--------------------------------
// フローチャートUIのノード配置情報。工程の意味構造（Workflow/nextFlows）からは独立した、
// 見た目のレイアウトのみを保持する。リンクは保存せず、常にnextFlowsから導出する

// 工程グループの背景枠ノード。飛び地に対応するため、同一groupIdのノードを複数持てる
struct FlowChartGroupNode {
  FlowGroupId groupId = Group_None;
  QPointF pos;
  QSizeF size;
};

// 工程ノード。1つのWorkflowにつき1ノード
struct FlowChartWorkflowNode {
  WorkflowType workflowType = Wf_Undefined;
  QPointF pos;            // シーン絶対座標
  bool expanded = false;  // プロトコル／アクションを展開表示中か
};

struct FlowChartLayout {
  QList<FlowChartGroupNode> groupNodes;
  QList<FlowChartWorkflowNode> workflowNodes;

  bool isEmpty() const {
    return groupNodes.isEmpty() && workflowNodes.isEmpty();
  }
};

struct ActionStepInfo {
  QString idName;
  QString labelName;
};

class WorkflowManager : public QObject {  // singleton
  Q_OBJECT

  QString m_currentProjectName;

  // 最後に読み込んだ／保存したppmprjのプロジェクトIDと、その内容のハッシュ。
  // 保存の前に、ほかの人がファイルを書き換えていないかを調べるのに使う
  QString m_loadedProjectName;
  QByteArray m_loadedProjectHash;

  // project settings
  QString m_projectTitle;
  QString
      m_projectDescription;  // プロジェクトの概要を人間が読める文章として記録する
  // 以下の3つのパスは、プロジェクト設定に書かれた値をそのまま保持する（相対パスもそのまま）。
  // 実際に使うパスは、projectRootPath()等のゲッターでStuffフォルダ起点に解決したものを使う
  QString m_projectRootPath;
  QString m_logRootPath;
  QString m_projectInfoFolderPath;
  QString m_cutNumberPrefix;
  // [SEQUENCE]マクロと、100カット毎のサブフォルダ（ログ・シーン情報の保存先）の仕分けを行うか。
  // OFFの場合、sceneNum2Sequence()は常に空文字列を返し、[SEQUENCE]は空文字列に置き換えられる
  bool m_useSequence = true;
  QString m_gasUrl;  // Google Apps ScriptのウェブアプリURL
  // Google
  // Chatの投稿先の名前の一覧。Step_NotifyGoogleChatステップから、名前で参照する。
  // 投稿先のWebhook URLは、投稿権限を持つ秘密情報のため、PPMには持たせない
  // （Google Apps Scriptのスクリプト プロパティに、同じ名前で登録する）
  QStringList m_chatDestinations;
  // カットのサムネイル画像のパス（[ROOT][PREFIX][SEQUENCE][CUTNUMBER]等のマクロを含む）。
  // 空文字列の場合、カット一覧のThumbnail列は常に非表示になる
  QString m_sceneThumbnailPath;
  // 話数（Episode）：テレビシリーズ等、複数話数で構成される作品のみ使用する。
  // 話数ごとに個別のProjectSettingsを持たせず、話数一覧のみをプロジェクト設定として持ち、
  // 「現在アクティブな話数」はプロジェクトを跨いだセッション状態として扱う
  bool m_hasEpisodes = false;
  QStringList m_episodeNames;
  QString
      m_currentEpisode;  // m_episodeNamesのいずれか。話数機能を使わない場合は空文字列のまま
  QList<Workflow> m_workflows;
  // パスのマクロ
  QList<PathMacroInfo> m_pathMacros;
  QMap<FlowGroupId, FlowGroupInfo> m_flowGroupInfos;
  // カット情報
  QList<SceneInfoTag*> m_sceneInfoTags;
  // フローチャートUIのノード配置情報
  FlowChartLayout m_flowChartLayout;
  // 役職（フローチャートUIの役職パレット）
  QList<Role> m_roles;

  // 主流の工程グループ（1カットにつき1つ）
  QMap<FlowGroupId, QStringList> m_cutListInFlow;
  // 傍流の工程グループ（1カットが0個または複数持てる）
  QMap<FlowGroupId, QStringList> m_cutListInBranchFlow;
  QMap<ActionStepType, ActionStepInfo> m_actionStepTypeInfos;

  // 進捗（progress）・進捗の詳細のキャッシュ（progress_cache）・カット情報（scene_info）の
  // 保存先ベースフォルダ。hasEpisodes()がtrueならm_projectInfoFolderPath直下に
  // m_currentEpisode名のサブフォルダを作り、そちらを返す
  QString projectInfoFolderPathForCurrentEpisode() const {
    if (m_hasEpisodes && !m_currentEpisode.isEmpty())
      return infoFolderPath() + "/" + m_currentEpisode;
    return infoFolderPath();
  }
  // 工程グループの所属（本流・傍流）を記録するフォルダ。工程グループのID名.ini
  // が入る
  QString progressFolderPath() const {
    return projectInfoFolderPathForCurrentEpisode() + "/progress";
  }
  // 工程ごとのカットの一覧（工程グループの更新ボタンで作り直せるキャッシュ）のフォルダ。
  // 工程グループのID名_details.ini と、その書き込み中のロックファイルが入る
  QString progressCacheFolderPath() const {
    return projectInfoFolderPathForCurrentEpisode() + "/progress_cache";
  }
  // カット情報・テイク（リテイク）の記録のフォルダ。シークエンスごとのフォルダの下に、カットごとのiniが入る
  QString sceneInfoFolderPath() const {
    return projectInfoFolderPathForCurrentEpisode() + "/scene_info";
  }
  // ログ保存先のベースフォルダ。hasEpisodes()がtrueならm_logRootPath直下に
  // m_currentEpisode名のサブフォルダを作り、そちらを返す
  QString logRootPathForCurrentEpisode() const {
    if (m_hasEpisodes && !m_currentEpisode.isEmpty())
      return logRootPath() + "/" + m_currentEpisode;
    return logRootPath();
  }

  WorkflowManager();

public:
  ~WorkflowManager();
  // カット名から[SEQUENCE]の文字列（先頭の数字列の下2桁を除いたもの）を求める。
  // シークエンスを使わない設定の場合は、常に空文字列を返す
  QString sceneNum2Sequence(QString cutName) const;
  // カット名が、現在の設定で使えるものかを返す。
  // シークエンスを使う場合は、先頭に3ケタ以上の数字列が必要。使わない場合は、空でなければよい。
  // （sceneNum2Sequence()が空文字列を返すことは、シークエンスを使わない場合はエラーではない）
  bool isValidSceneName(const QString& cutName) const;
  // 使えないカット名があるときの、ユーザー向けの警告メッセージ
  QString invalidSceneNameMessage(const QStringList& cutNames) const;
  bool useSequence() const { return m_useSequence; }
  void setUseSequence(bool on) { m_useSequence = on; }
  // [SEQUENCE]が空文字列に置き換わったことで生じた、連続する"/"を1つにまとめる（先頭のUNCの"//"は残す）
  static QString collapseSeparators(const QString& path);

  static WorkflowManager* instance();
  int workflowCount() { return m_workflows.count(); }
  Workflow getWorkflow(int index) { return m_workflows.at(index); }

  Workflow workflow(WorkflowType type) {
    for (auto& wf : m_workflows) {
      if (wf.workflowType == type) return wf;
    }
    return Workflow();
  }

  QString projectTitle() const { return m_projectTitle; }
  void setProjectTitle(const QString& title) { m_projectTitle = title; }
  QString projectDescription() const { return m_projectDescription; }
  void setProjectDescription(const QString& description) {
    m_projectDescription = description;
  }
  // 相対パスをStuffフォルダ（UserSettings::stuffFolderPath()）起点の絶対パスに解決する。
  // 空文字列と絶対パスはそのまま返す
  static QString resolvePathFromStuffFolder(const QString& path);

  // projectRootPath()／logRootPath()／infoFolderPath()は、Stuffフォルダ起点に解決した
  // 実際に使うパスを返す。プロジェクト設定に書かれた値そのもの（相対パスのまま）は
  // ～Setting()で取得する（プロジェクト設定ダイアログでの表示・編集用）
  QString projectRootPath() const {
    return resolvePathFromStuffFolder(m_projectRootPath);
  }
  QString projectRootPathSetting() const { return m_projectRootPath; }
  void setProjectRootPath(const QString& path) { m_projectRootPath = path; }
  QString logRootPath() const {
    return resolvePathFromStuffFolder(m_logRootPath);
  }
  QString logRootPathSetting() const { return m_logRootPath; }
  void setLogRootPath(const QString& path) { m_logRootPath = path; }
  QString infoFolderPath() const {
    return resolvePathFromStuffFolder(m_projectInfoFolderPath);
  }
  QString infoFolderPathSetting() const { return m_projectInfoFolderPath; }
  void setInfoFolderPath(const QString& path) {
    m_projectInfoFolderPath = path;
  }

  QString cutNumberPrefix() const { return m_cutNumberPrefix; }
  void setCutNumberPrefix(const QString& prefix) { m_cutNumberPrefix = prefix; }
  QString currentProjectName() const { return m_currentProjectName; }
  // Google Apps Scriptのウェブアプリ経由でスプレッドシートに書き込むための設定
  // (書き込み先のスプレッドシートはApps Scriptプロジェクト自体に紐づくため、
  //  PPM側ではURLのみを保持する)
  QString gasUrl() const { return m_gasUrl; }
  void setGasUrl(const QString& url) { m_gasUrl = url; }
  const QStringList& chatDestinations() const { return m_chatDestinations; }
  void setChatDestinations(const QStringList& destinations) {
    m_chatDestinations = destinations;
  }
  bool hasChatDestination(const QString& destinationName) const {
    return m_chatDestinations.contains(destinationName);
  }
  QString sceneThumbnailPath() const { return m_sceneThumbnailPath; }
  void setSceneThumbnailPath(const QString& path) {
    m_sceneThumbnailPath = path;
  }
  // 現在のプロジェクトにサムネイルパスが設定されている場合のみ、指定したカットのサムネイル画像パスを解決する。
  // 設定されていない場合は空文字列を返す
  QString sceneThumbnailPathFor(const QString& sequence,
                                const QString& sceneNumber) {
    if (m_sceneThumbnailPath.isEmpty()) return QString();
    return decodePath(m_sceneThumbnailPath, sequence, sceneNumber);
  }

  // 話数（Episode）機能
  bool hasEpisodes() const { return m_hasEpisodes; }
  void setHasEpisodes(bool has) { m_hasEpisodes = has; }
  const QStringList& episodeNames() const { return m_episodeNames; }
  void setEpisodeNames(const QStringList& names) { m_episodeNames = names; }
  // 現在アクティブな話数（[EPISODE]マクロや、話数別に分かれた進捗・ログ等の
  // 保存先フォルダの解決に使われる）。hasEpisodes()がfalseのときは無視される
  QString currentEpisode() const { return m_currentEpisode; }
  void setCurrentEpisode(const QString& episode) { m_currentEpisode = episode; }

  // ProjectSettingsDialogでの編集内容を書き戻すためのセッター
  void setWorkflows(const QList<Workflow>& workflows) {
    m_workflows = workflows;
  }
  void setPathMacros(const QList<PathMacroInfo>& macros) {
    m_pathMacros = macros;
  }
  void setFlowGroupInfos(const QMap<FlowGroupId, FlowGroupInfo>& infos) {
    m_flowGroupInfos = infos;
  }
  // 渡されたtagは呼び出し側が引き続き所有する（内部でclone()して保持する）
  void setSceneInfoTagList(const QList<SceneInfoTag*>& tags) {
    deleteAllSceneInfoTags();
    for (auto tag : tags) m_sceneInfoTags.append(tag->clone());
  }

  // フローチャートUIのノード配置情報
  const FlowChartLayout& flowChartLayout() const { return m_flowChartLayout; }
  void setFlowChartLayout(const FlowChartLayout& layout) {
    m_flowChartLayout = layout;
  }

  // 役職（フローチャートUIの役職パレット）
  const QList<Role>& roles() const { return m_roles; }
  void setRoles(const QList<Role>& roles) { m_roles = roles; }

  void logAction(const QString& sequence, const QString& cutNumber,
                 const QString& workflow, const QString& action);

  void saveSceneInfoTag(const QString& sequence, const QString& cutNumber,
                        const QString& infoTagName, const QVariant& value,
                        const QString& actionName = QString());
  void releaseSceneInfoTag(const QString& sequence, const QString& cutNumber,
                           const QString& infoTagName,
                           const QString& actionName = QString());
  QVariant sceneInfoTag(const QString& sequence, const QString& cutNumber,
                        const QString& infoTagName,
                        const QVariant& defaultValue) const;
  const QMap<QString, QVariant> sceneInfoTags(const QString& sequence,
                                              const QString& cutNumber);
  const QString getSceneInfoTagsRichText(const QString& sequence,
                                         const QString& cutNumber);

  const QList<SceneInfoTag*>& sceneInfoTagList() { return m_sceneInfoTags; }
  const SceneInfoTagDisplayProperties sceneInfoTagDisplayProperties(
      const QString& infoTagName, const QVariant& value = "");
  void deleteAllSceneInfoTags() {
    for (auto sceneInfoTag : m_sceneInfoTags) delete sceneInfoTag;
    m_sceneInfoTags.clear();
  }

  // 既定では難読化パスはデコードしない。
  // 難読化パスは、操作に使う直前に、decodeObfuscatedPathMacros()でデコードする
  QString decodePathMacros(QString path, bool decodeObfuscated = false);

  // 難読化オプションがONのパスマクロを、操作（ファイルの読み書き等）に使う直前に、実際のパスへデコードする。
  // 確認ダイアログやログには、デコード前（マクロの文字列のまま）のパスを表示して、難読化されたパスを
  // ユーザーから隠すこと。マクロが定義しているパスに[ROOT]等の既定のマクロが使われている場合は、
  // それもデコードする（sequence/sceneNumber/stockSceneNumbersは、decodePath()と同じ意味）。
  // 難読化マクロを含まないパスは、そのまま返す
  QString decodeObfuscatedPathMacros(const QString& path,
                                     const QString& sequence,
                                     const QString& sceneNumber,
                                     const QString& stockSceneNumbers = "");
  // textに含まれる、難読化マクロをデコードした後のパスを、マクロの文字列に戻す。
  // 実際のパスが入りうるメッセージやログから、難読化されたパスを隠すための安全策
  QString hideObfuscatedPaths(const QString& text, const QString& sequence,
                              const QString& sceneNumber,
                              const QString& stockSceneNumbers = "");
  const QList<PathMacroInfo>& pathMacros() { return m_pathMacros; }

  // アクション実行時、WorkflowのCodedPathを全てDecodeする。
  QString decodePath(const QString& codedPath, const QString& sequence,
                     const QString& sceneNumber,
                     const QString& bankSceneNumbers = "",
                     bool filesystemPath             = true);

  QString resolveProjectPath(const QString& expanded) const;
  QString relativePathFromProjectRoot(const QString& path);

  QString cutFolderPath(FlowGroupId id, QString sequence, QString sceneNumber,
                        int take = 1);
  QStringList xdtsPaths(FlowGroupId id, QString sequence, QString sceneNumber,
                        QStringList& xdtsFileNames,
                        bool obtainBothGengaAndDouga    = false,
                        const QStringList& cueFilePaths = QStringList(),
                        bool* ambiguous                 = nullptr);
  QString logFilePath(QString sequence, QString sceneNumber);
  QString googleCueFilePath(const QString googleCueFileName);

  QStringList cutListInFlow(FlowGroupId);
  QStringList cutListInBranchFlow(FlowGroupId);
  // cutNamesのいずれかが（主流として）所属している工程グループの一覧
  QList<FlowGroupId> flowGroupsContainingCuts(const QStringList& cutNames);
  void registerCutNames(QStringList cutNames, FlowGroupId fgId,
                        QString actionName = QString());

  // 傍流のグループを登録。主流と同様フォルダの検索に用いられる。主流とは別にカウントされる
  void registerCutNameToBranchFlowGroup(QStringList cutNames, FlowGroupId fgId,
                                        bool isAdd,
                                        QString actionName = QString());

  void switchCurrentProject(QString prjName);
  void loadCutList();

  const QMap<FlowGroupId, FlowGroupInfo>& flowGroupInfos() {
    return m_flowGroupInfos;
  }
  QList<FlowGroupId> flowGroupIds() const { return m_flowGroupInfos.keys(); }
  QString flowGroupLabelName(const FlowGroupId id) const {
    return m_flowGroupInfos[id].labelName;
  }
  QString flowGroupIdName(const FlowGroupId id) const {
    return m_flowGroupInfos[id].idName;
  }
  FlowGroupId IdNameToFlowGroupId(const QString& _idName) const {
    for (auto groupId : m_flowGroupInfos.keys()) {
      if (m_flowGroupInfos[groupId].idName == _idName) return groupId;
    }
    return Group_None;  // not found
  }

  QString actionStepTypeName(const ActionStepType type) const {
    return m_actionStepTypeInfos[type].idName;
  }
  QString actionStepTypeLabelName(const ActionStepType type) const {
    return m_actionStepTypeInfos[type].labelName;
  }
  ActionStepType actionStepTypeId(const QString& typeName) const {
    QMapIterator<ActionStepType, ActionStepInfo> itr(m_actionStepTypeInfos);
    while (itr.hasNext()) {
      itr.next();
      if (itr.value().idName == typeName) return itr.key();
    }

    return Step_Unidentified;
  }

  QString decodeConditionPathForToolTip(QString path);

  // テイク管理
  int currentTake(const QString sceneNumber) const;
  QString currentTakeSuffix(const QString sceneNumber,
                            const bool r_suffix = false) const;
  QString previousTakeSuffix(const QString sceneNumber,
                             const bool r_suffix = false) const;
  QString currentRewindSuffix(const QString sceneNumber) const;

  bool doRetake(const QString sceneNumber, const int takeNum,
                const FlowGroupId passToFgId, const QString comment);
  QString createCutFolder(const QString sceneNumber, const int takeNum,
                          const FlowGroupId passToFgId);
  QVariantMap obtainRetakeInfo(const QString sceneNumber,
                               int takeNum = -1) const;

  // 詳細進捗リスト管理
  void getProgressSettingsFilePath(const FlowGroupId fgId,
                                   QString& settingsPath, QString& lockPath);
  void saveProgressList(const FlowGroupId fgId,
                        const QMap<WorkflowType, QStringList> progressList);
  FlowGroupId WfType2FgId(const WorkflowType wfType);
  void getProgressList(const FlowGroupId fgId,
                       QMap<WorkflowType, QStringList>& progressList);
  void setProgress(const QString sceneNumber, const WorkflowType wfType,
                   const bool doAdd);
  // sceneNumberが、wfTypeの詳細進捗リストに載っているか
  bool isInProgress(const QString sceneNumber, const WorkflowType wfType);

  // workflowデータを書き出す。失敗した場合はfalseを返し、
  // errorStringにファイルのパスと理由を入れる
  bool saveProject(const QString& prjName, QString* errorString = nullptr);
  // prjNameのppmprjに保存すると、PPMが読み込んでいない内容を上書きしてしまうか。
  // 読み込んだ後にファイルが書き換えられた場合と、
  // 読み込んでいない別のプロジェクトのファイルが既にある場合にtrueを返す
  bool projectFileChangedSinceLoaded(const QString& prjName) const;
  bool loadProject(const QString& prjName);
};
