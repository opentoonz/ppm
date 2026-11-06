#pragma once

#include <QDialog>
#include <QList>
#include <QCoreApplication>
#include <QTreeWidget>
#include <QJsonObject>
#include <QPair>
#include "workflow.h"
#include "renamescript.h"

class QListWidget;
class QButtonGroup;
class QComboBox;
class QLabel;
class QTreeWidgetItem;

class StepExecutor {
  Q_DECLARE_TR_FUNCTIONS(StepExecutor)
protected:
  bool m_isUndo;
  QVariant m_checkSum;

  // 難読化されたパスマクロ（パスマクロの「Obfuscate」がON）の扱い。
  // 難読化パスをユーザーから隠すため、各StepExecutorは、パスを「マクロの文字列のまま」保持して
  // 確認ダイアログとログに表示し、ファイルを実際に操作する直前に、realPath()でデコードする。
  QString m_pathSequence,
      m_pathSceneNumber;  // デコードに使う、カットの情報（コンストラクタで設定する）
  void setPathContext(const QString& sequence, const QString& sceneNumber) {
    m_pathSequence    = sequence;
    m_pathSceneNumber = sceneNumber;
  }
  // displayPath（難読化マクロのままのパス）を、操作に使う実際のパスにする。
  // 難読化マクロが定義しているパスに含まれる[ROOT]等の既定のマクロも、デコードされる。
  // sceneNumber/stockSceneNumbersは、デコードに別のカット番号（兼用カットの個々のカット番号等）を使う場合に指定する
  QString realPath(const QString& displayPath,
                   const QString& sceneNumber       = QString(),
                   const QString& stockSceneNumbers = QString()) const;
  // 実際のパスを含みうる文字列（エラーメッセージ等）から、難読化されたパスを、マクロの文字列に戻して隠す
  QString hide(const QString& text) const;

public:
  StepExecutor(bool isUndo) : m_isUndo(isUndo) {}
  virtual ~StepExecutor() = default;
  // checkSumはUndoのときだけ評価する
  virtual QWidget* createWidget() = 0;
  // checkSumは実行時に格納する
  virtual bool execute(QString& log, QVariant& checkSum) = 0;
  void setCheckSum(QVariant checkSum) { m_checkSum = checkSum; }
  bool createPathAndCopyFile(const QString& fileName, const QString& newName,
                             QString& log);

  // フォルダを内容ごとコピーする機能（Step_Copy/Step_CopyFileで共用）。
  // コピー先パス自体が、コピー元フォルダのコピーになる（コピー先/コピー元フォルダ名、にはならない）。
  // Undoのために、実際にコピーしたファイルと、新規に作ったフォルダを記録する
  struct FolderCopyResult {
    bool overwritten =
        false;  // 既存ファイルを上書きしたか（Undoでは元に戻せない）
    QStringList copiedFiles;  // コピーしたファイルの絶対パス
    QStringList createdDirs;  // 新規に作成したフォルダの絶対パス（浅い順）
    QVariant toVariant() const;
    static FolderCopyResult fromVariant(const QVariant& v);
  };
  // fromPathをtoPathへ内容ごとコピーする。toPath以下に同名ファイルがある場合は、確認ダイアログで
  // 上書きの可否を尋ねる（overwriteAllがtrueなら尋ねずに上書きする。「全て上書き」が選ばれるとtrueになる）。
  // 失敗した場合は、原因をlogに追記してfalseを返す
  bool copyFolderRecursively(const QString& fromPath, const QString& toPath,
                             bool& overwriteAll, QString& log,
                             FolderCopyResult& result);
  // fromPathを全てのコピー先へ内容ごとコピーし、Undo用の記録をcheckSumへ格納する
  bool copyFolderToAll(const QString& fromPath, const QStringList& toPaths,
                       QString& log, QVariant& checkSum);
  // copyFolderToAllが格納したcheckSumを元に、コピーで作成されたファイル・フォルダだけを削除する。
  // コピー先に元からあったファイルは消さない
  bool undoFolderCopyAll(const QVariant& checkSum, QString& log);
  // checkSumがcopyFolderToAllによるもの（フォルダコピー）かどうか。
  // ファイルコピーのcheckSumはハッシュ値（QByteArray）なので区別できる
  static bool isFolderCopyCheckSum(const QVariant& checkSum);
  // フォルダコピーのcheckSumに、既存ファイルを上書きした記録が含まれるか
  static bool folderCopyCheckSumHasOverwrite(const QVariant& checkSum);

  virtual bool hasUI() { return true; }
  virtual bool isGoogleStep() { return false; }
};

//--------------------------------------------------

class StepExecutor_Rename : public StepExecutor {
  Q_DECLARE_TR_FUNCTIONS(StepExecutor_Rename)

  QStringList m_orgPaths;
  // リネーム先。難読化マクロはデコードせず、マクロのまま保持する（表示用）。操作の直前にrealRenameTo()でデコードする
  QList<QStringList> m_renameToList;
  // m_orgPathsの各パスについて、リネーム先のデコードに使ったカット番号（兼用カットでは、個々のカット番号）
  QStringList m_pathSceneNumbers;
  QString
      m_stockSceneNumbers;  // 兼用カットの場合のみ、カット番号を繋げた文字列
  QString realRenameTo(int orgIndex, const QString& displayPath) const {
    return realPath(displayPath, m_pathSceneNumbers.value(orgIndex),
                    m_stockSceneNumbers);
  }

public:
  StepExecutor_Rename(const ActionStep& step, QStringList orgPaths,
                      QString sequence, QString sceneNumber, bool isUndo);

  QWidget* createWidget() override;
  bool execute(QString& log, QVariant& checkSum) override;
  bool execute_rename(QString& log, QVariant& checkSum);
  bool execute_undo(QString& log);
  static QJsonObject toJson(const QList<QVariant>& params);
  static QVariantList fromJson(const QJsonObject& json);
};

//--------------------------------------------------

class StepExecutor_Copy : public StepExecutor {
  Q_DECLARE_TR_FUNCTIONS(StepExecutor_Copy)

  QString m_orgPath;
  QStringList m_copyToList;

public:
  StepExecutor_Copy(const ActionStep& step, QString orgPath, QString sequence,
                    QString sceneNumber, bool isUndo);

  QWidget* createWidget() override;
  bool execute(QString& log, QVariant& checkSum) override;
  bool execute_copy(QString& log, QVariant& checkSum);
  bool execute_undo(QString& log);
  static QJsonObject toJson(const QList<QVariant>& params);
  static QVariantList fromJson(const QJsonObject& json);
};

//--------------------------------------------------

// Stuffフォルダのrename_scriptsに置いたスクリプト（JavaScript）によるリネームルール。
// ファイル1つ分のコピー先パスを決める。スクリプトの仕様は、renamescript.hを参照
class ScriptFileRenamer {
  Q_DECLARE_TR_FUNCTIONS(ScriptFileRenamer)
  RenameScript m_script;

public:
  ScriptFileRenamer(const QString& scriptId, const QVariantMap& context);

  const QString ruleName();
  const QString ruleDetails();
  // ルールを使える状態か（スクリプトの読み込みに失敗した場合などはfalseを返し、errorに理由を入れる）
  bool isValid(QString& error);
  // 多数のファイルを続けてリネームする前後に呼ぶ（スクリプトの暴走を止める時間制限の開始・終了）
  void beginBatch();
  void endBatch();

  // コピー元フォルダからの相対パス（サブフォルダに入っている場合はそのフォルダを含む）を受け取り、
  // リネーム後の相対パス（サブフォルダに入れる場合はそのフォルダを含む）をresultに入れる。
  // ルールの対象外（リネームしない）の場合は、resultを空文字列にする。
  // 戻り値がfalseの場合はエラー（errorにメッセージ）
  bool renamedRelativePath(const QString& relativePath, QString& result,
                           QString& error);
};

//--------------------------------------------------
class MyFolderTree : public QTreeWidget {
  Q_OBJECT

  enum Role { Role_Path = Qt::UserRole, Role_IsDir };

  int m_maxFolderDepth;

  void createTreeChildren(QTreeWidgetItem* parentItem, int depth,
                          bool isAllMode, QStringList filesToBeCopied,
                          QStringList dirsToBeCopied, QStringList whiteList,
                          QStringList blackList);

  void doGetCheckedItems(QTreeWidgetItem* parentItem, QStringList& files,
                         QStringList& dirs);
  int treeCount(QTreeWidgetItem* parent = nullptr);

public:
  MyFolderTree(QWidget* parent, const QString rootPath, int maxFolderDepth,
               bool isAllMode, QStringList filesToBeCopied,
               QStringList dirsToBeCopied, QStringList whiteList,
               QStringList blackList);

  void getCheckedItems(QStringList& files, QStringList& dirs, QString rootPath);
protected slots:
  void updateHeight();
};

// フォルダの内容をコピー／移動する。
//
// 第一引数（Mode）は、角括弧のオプションを並べた文字列＋選択の既定値の指定。
// オプションは意味ごとの軸に分かれていて、軸が異なるものは自由に組み合わせられ、順序は問わない
// （保存時は決まった順序で書き出す）。
//   [対象の選び方]  [ALL]（全て選択済み）/ [SELECT]（何も選択していない）/
//   [ALL_WITHOUT_UI]（UIなしで全てコピー）
//                   [FOLDER_DEPTH=n]（選択ツリーの表示階層）、+名前;-名前（初期選択の強制ON/OFF）
//                   ※[ALL_WITHOUT_UI]では、表示階層とON/OFFの指定は無効
//   [転送方式]      [MOVE]（コピー後に元ファイルを削除する。省略時はコピー）
//   [名前の変換]
//   [TEAR_DOWN_SUBFOLDERS]（サブフォルダを展開し、ファイルをコピー先の直下に並べる）
//                   [RENAME_SCRIPT=名前]（ファイル名のリネームルール。Stuffフォルダのrename_scripts/名前.jsのスクリプト）
//                   ※従来の[RENAME_GENGA_TO_UCHIKOMI]は、同梱のスクリプトgenga_to_uchikomiを使う指定として読み込まれる
//                     （保存し直すと[RENAME_SCRIPT=genga_to_uchikomi]になる）
//                   [RENAME_NEXTTAKE] /
//                   [RENAME_T2R]（フォルダ名のテイク接尾辞の変換。どちらか一方のみ）
//                   実行順は「リネームルール → テイク接尾辞の変換 →
//                   フォルダの展開」
//   [内容の変換]
//   [CONVERT_TO_JPG]（画像をJPEGに変換する）。※元の画像を復元できなくなるため、[MOVE]とは併用できない
//   [同名ファイル]
//   コピー先に同名のファイルがあるときの処理。省略時は確認ダイアログ（上書き／スキップ／中止）
//                   [OVERWRITE]（確認せず上書き）/
//                   [SKIP_EXISTING]（確認せず既存を残す）/
//                   [OVERWRITE_IF_NEWER]（コピー元の方が新しい場合のみ上書き）
//                   ※上書きしなかったファイルは、[MOVE]でも元の場所に残る
// 第二引数にコピー元フォルダを指定する。空文字列の場合はキューファイルの所属フォルダになる。
//   [SKIP_CUE_FILE]を指定すると、キューファイルをコピー対象の初期選択から外す（承認用フォルダへのコピー時を除く）。
// 第三引数以降にコピー先フォルダを指定する。
class StepExecutor_CopyFolderContents : public StepExecutor {
  Q_DECLARE_TR_FUNCTIONS(StepExecutor_CopyFolderContents)
public:
  // Mode文字列の内容。設定画面とStepExecutorで共有する
  struct Options {
    enum Selection { All, Select, AllWithoutUI };
    enum TakeConversion { Take_None, Take_Next, Take_ToR };
    // Rename_Scriptは、Stuffフォルダのrename_scriptsに置いたスクリプト（renameScriptで指定）によるルール
    enum RenameRule { Rename_None, Rename_Script };
    // コピー先に同名のファイルがあるときの処理
    enum ConflictPolicy {
      Conflict_Ask,  // 一覧を見せて、上書き／スキップ／中止をユーザーに選ばせる（既定）
      Conflict_Overwrite,  // 確認せずに上書きする
      Conflict_Skip,       // 確認せずに、既存のファイルを残す（コピーしない）
      Conflict_OverwriteIfNewer  // コピー元の方が新しい場合だけ上書きする（それ以外は既存のファイルを残す）
    };

    Selection selection    = All;
    bool move              = false;
    bool flattenSubfolders = false;
    RenameRule renameRule  = Rename_None;
    QString
        renameScript;  // renameRuleがRename_Scriptのときの、スクリプトのid（ファイル名から拡張子を除いたもの）
    TakeConversion takeConversion = Take_None;
    bool convertToJpg             = false;
    ConflictPolicy conflictPolicy = Conflict_Ask;
    int folderDepth               = -1;  // 負の値は未指定（実行時は1）
    QString whiteBlackList;              // "+foo.png;-bar"形式のまま保持する

    // 同時に指定できない組み合わせか（[MOVE]と[CONVERT_TO_JPG]）
    bool hasConflictingOptions() const { return move && convertToJpg; }
  };
  static Options parseMode(const QString& modeStr);
  static QString buildMode(const Options& options);

private:
  Options m_options;

  QString m_orgFolderPath;
  QStringList m_copyToFolderList;
  QStringList m_filesToBeCopied, m_dirsToBeCopied;

  QListWidget* m_fileListWidget;
  MyFolderTree* m_fileTree;

  // 初期値に入れる項目、省く項目
  QStringList m_whiteList, m_blackList;

  bool
      m_skipCueFile;  // trueのとき、Cueファイルの選択を無効化する（すでにStep_Copyで別名コピー済みなので）
  QString m_orgFileName;

  // テイク接尾辞の変換ペア。
  // 仕上げリテイク受付（[RENAME_NEXTTAKE]）の場合は、テイク接尾辞を なし → _t02
  // に変更するペア
  // 仕上げリテイク上がり（[RENAME_T2R]）の場合は、テイク接尾辞を_t02→_rに変更するペア
  // 現在のテイクが1以下で変換の必要がない場合は、first/secondとも空のまま
  bool m_takeConversionActive = false;
  QPair<QString, QString> m_takeSuffixPair;

  ScriptFileRenamer* m_renamer = nullptr;

  // コピー（移動）の計画の1項目。コピー元フォルダからの相対パスで表す
  struct PlanItem {
    QString fromRel;
    QString toRel;
    bool isDir = false;  // 空フォルダを含めて作るためのフォルダ項目
    QString
        originDir;  // 選択されたフォルダに含まれる項目の場合、そのフォルダの相対パス
    bool convertImage = false;
  };
  // 選択された項目から、コピー先パスを含む計画を作る。ユーザーが中断した場合はfalse
  bool buildPlan(const QString& orgFolderPath, QList<PlanItem>& plan,
                 QString& log);
  // Undo用の記録（checkSum）を作る
  static QVariant makeCheckSum(bool overwritten,
                               const QVariantList& perDestination,
                               const QVariantList& moveMapping,
                               const QStringList& removedSourceDirs);

public:
  StepExecutor_CopyFolderContents(FlowGroupId groupId, const ActionStep& step,
                                  QString orgPath, QString sequence,
                                  QString sceneNumber, bool isUndo);
  ~StepExecutor_CopyFolderContents() override;
  QWidget* createWidget() override;
  QWidget* createWidget_do();
  QWidget* createWidget_undo();
  bool execute(QString& log, QVariant& checkSum) override;
  bool execute_do(QString& log, QVariant& checkSum);
  bool execute_undo(QString& log, QVariant& checkSum);
  static QJsonObject toJson(const QList<QVariant>& params);
  static QVariantList fromJson(const QJsonObject& json);
};

//--------------------------------------------------
//
// 指定したファイルを任意の名前にリネームする。
// 第一引数にコピー元ファイル名を指定する。
// 第二引数以降にコピー先ファイル名を指定する。

class StepExecutor_RenameFile : public StepExecutor {
  Q_DECLARE_TR_FUNCTIONS(StepExecutor_RenameFile)

  QString m_renameFrom;
  QStringList m_renameToList;

  enum Mode { Rename, Copy } m_mode;

public:
  StepExecutor_RenameFile(const ActionStep& step, QString sequence,
                          QString sceneNumber, bool isUndo);

  QWidget* createWidget() override;
  bool execute(QString& log, QVariant& checkSum) override;
  bool execute_rename(QString& log, QVariant& checkSum);
  bool execute_undo(QString& log);
  static QJsonObject toJson(const QList<QVariant>& params);
  static QVariantList fromJson(const QJsonObject& json);
};

//--------------------------------------------------

class StepExecutor_MoveFlowGroup : public StepExecutor {
  Q_DECLARE_TR_FUNCTIONS(StepExecutor_MoveFlowGroup)

  FlowGroupId m_beforeId, m_afterId;
  QString m_sceneNumber;

public:
  StepExecutor_MoveFlowGroup(FlowGroupId beforeGroupId, const ActionStep& step,
                             QString sequence, QString sceneNumber,
                             bool isUndo);

  QWidget* createWidget() override;
  bool execute(QString& log, QVariant& checkSum) override;
  static QJsonObject toJson(const QList<QVariant>& params);
  static QVariantList fromJson(const QJsonObject& json);
};

//--------------------------------------------------

class StepExecutor_CheckExistence : public StepExecutor {
  Q_DECLARE_TR_FUNCTIONS(StepExecutor_CheckExistence)

  QStringList m_filesToBeChecked;
  // 現在のカットのカット情報をコンストラクタで取得しておく
  QMap<QString, QVariant> m_cutInfo;

  enum CheckMode { Existence, Absence } m_checkMode;

  enum Target { File, SceneInfo } m_target;

public:
  StepExecutor_CheckExistence(const ActionStep& step, QString sequence,
                              QString sceneNumber, bool isUndo);

  QWidget* createWidget() override;
  bool execute(QString& log, QVariant& checkSum) override;
  static QJsonObject toJson(const QList<QVariant>& params);
  static QVariantList fromJson(const QJsonObject& json);
};

//--------------------------------------------------

class StepExecutor_AskDelete : public StepExecutor {
  Q_DECLARE_TR_FUNCTIONS(StepExecutor_AskDelete)

  QString m_fileToBeDeleted;
  bool m_doDelete;
  QButtonGroup* m_buttonGroup;

  // キューファイル以外を消す場合、事前にチェックを行う
  bool m_doesExist;
  bool m_isDir;

public:
  StepExecutor_AskDelete(const ActionStep& step, QString orgPath,
                         QString sequence, QString sceneNumber, bool isUndo);

  QWidget* createWidget() override;
  bool execute(QString& log, QVariant& checkSum) override;
  static QJsonObject toJson(const QList<QVariant>& params);
  static QVariantList fromJson(const QJsonObject& json);
};
//--------------------------------------------------

class StepExecutor_CreateCueFile : public StepExecutor {
  Q_DECLARE_TR_FUNCTIONS(StepExecutor_CreateCueFile)

  QString m_fileToBeCreated;
  QString m_fileContent;
  // 確認画面を作った時点で、同名のファイルがすでにあったか（上書きの警告に使う）
  bool m_fileExists;

  // 取り消し用のデータ（checkSum）から、上書き前の中身を取り出す。
  // 上書きしていない（新しく作った）場合は false を返す
  bool overwrittenContent(QByteArray& content) const;

public:
  StepExecutor_CreateCueFile(const ActionStep& step, QString sequence,
                             QString sceneNumber, bool isUndo);

  QWidget* createWidget() override;
  bool execute(QString& log, QVariant& checkSum) override;
  static QJsonObject toJson(const QList<QVariant>& params);
  static QVariantList fromJson(const QJsonObject& json);
};

//--------------------------------------------------

class StepExecutor_StartEndBranchFlow : public StepExecutor {
  Q_DECLARE_TR_FUNCTIONS(StepExecutor_StartEndBranchFlow)

  FlowGroupId m_fgId;
  bool m_isStart;
  QString m_sceneNumber;

public:
  StepExecutor_StartEndBranchFlow(bool isStart, const ActionStep& step,
                                  QString sequence, QString sceneNumber,
                                  bool isUndo);

  QWidget* createWidget() override;
  bool execute(QString& log, QVariant& checkSum) override;
  static QJsonObject toJson(const QList<QVariant>& params);
  static QVariantList fromJson(const QJsonObject& json);
};

//--------------------------------------------------

class StepExecutor_SetSceneInfo : public StepExecutor {
  Q_DECLARE_TR_FUNCTIONS(StepExecutor_SetSceneInfo)

  QString m_sequence;
  QString m_sceneNumber;
  QString m_infoName;
  QVariant m_value;

public:
  StepExecutor_SetSceneInfo(const ActionStep& step, QString sequence,
                            QString sceneNumber, bool isUndo);

  QWidget* createWidget() override;
  bool execute(QString& log, QVariant& checkSum) override;
  static QJsonObject toJson(const QList<QVariant>& params);
  static QVariantList fromJson(const QJsonObject& json);
};

//--------------------------------------------------

class StepExecutor_InputGoogleSpreadsheet : public StepExecutor {
  Q_DECLARE_TR_FUNCTIONS(StepExecutor_InputGoogleSpreadsheet)

  QString m_sheetName, m_flowName;
  QStringList m_sceneNumbers;
  // Valueフィールドをセミコロンで区切った各候補（マクロはデコード済み）。区切りが無ければ
  // 要素数1のリストになる。ユーザーはcreateWidget()内のコンボボックスでどれを使うか選べる
  QStringList m_valueOptions;
  int m_selectedValueIndex =
      0;  // m_valueOptionsのうち、現在選択されている値のインデックス
  int m_bankCutStartIndex;
  // このアクション内でのステップ実行順（stepExecutorList内でのインデックス）。呼び出し元
  // （ConfirmationDialogの各コンストラクタ）が設定する。ひとつのアクションに複数の
  // Googleスプレッドシート入力ステップがある場合に、非同期の完了通知がどのステップに
  // 対応するかをAsynchronousUndoButton側で区別するためのIDとして、そのままGoogleGatewayへ渡す
  int m_googleStepIndex = -1;

  // createWidget()の非Undo時の説明文（各カット→現在選択中の値、の一覧）を組み立てる。
  // コンボボックスで選択が変わるたびに呼び直し、ラベルの表示を更新する
  QString buildInputLabelText() const;

public:
  StepExecutor_InputGoogleSpreadsheet(const ActionStep& step, QString sequence,
                                      QString sceneNumber, bool isUndo);

  QWidget* createWidget() override;
  bool execute(QString& log, QVariant& checkSum) override;
  bool isGoogleStep() override { return true; }
  void setGoogleStepIndex(int index) { m_googleStepIndex = index; }
  int googleStepIndex() const { return m_googleStepIndex; }
  static QJsonObject toJson(const QList<QVariant>& params);
  static QVariantList fromJson(const QJsonObject& json);
};

//--------------------------------------------------

class StepExecutor_RegisterProgress : public StepExecutor {
  QString m_sceneNumber;
  WorkflowType m_fromType;
  QList<WorkflowType> m_toTypes;

public:
  // 実行時に実際に登録した次工程を、checkSumに記録する（Undoでは、条件分岐を再評価せず、これを使う）
  static QVariant encodeToTypes(const QList<WorkflowType>& toTypes);
  static bool decodeToTypes(const QVariant& checkSum,
                            QList<WorkflowType>& toTypes);

  StepExecutor_RegisterProgress(QString sceneNumber, WorkflowType fromType,
                                QList<WorkflowType> toTypes, bool isUndo);
  QWidget* createWidget() override { return nullptr; };
  bool execute(QString& log, QVariant& checkSum) override;
  bool hasUI() override { return false; }
};

class StepExecutor_ReleaseProgress : public StepExecutor {
  QString m_sceneNumber;
  WorkflowType m_type;

public:
  StepExecutor_ReleaseProgress(const ActionStep& step, QString sceneNumber,
                               bool isUndo);
  QWidget* createWidget() override { return nullptr; };
  bool execute(QString& log, QVariant& checkSum) override;
  bool hasUI() override { return false; }
  static QJsonObject toJson(const QList<QVariant>& params);
  static QVariantList fromJson(const QJsonObject& json);

  const WorkflowType type() { return m_type; }
};

//--------------------------------------------------

// 次工程に、指定した工程を追加する。Step_ConditionalBranch系の中に入れて使い、条件を満たす場合だけ
// 工程を枝分かれさせる。executeは何もせず、追加の実体は、ダイアログが、PPM_Action::nextFlowsに
// このステップの工程を足したものをStepExecutor_RegisterProgressに渡して行う
class StepExecutor_AddProgress : public StepExecutor {
  Q_DECLARE_TR_FUNCTIONS(StepExecutor_AddProgress)
  WorkflowType m_type;

public:
  StepExecutor_AddProgress(const ActionStep& step, QString sceneNumber,
                           bool isUndo);
  QWidget* createWidget() override;
  bool execute(QString& log, QVariant& checkSum) override;
  bool hasUI() override { return !m_isUndo; }
  static QJsonObject toJson(const QList<QVariant>& params);
  static QVariantList fromJson(const QJsonObject& json);

  WorkflowType type() const { return m_type; }
};

// ステップの種類に応じて、アクションの次工程のリストを更新する
// （Step_ReleaseProgressなら取り除き、Step_AddProgressなら追加する。重複はしない）
void applyProgressStepToNextFlows(StepExecutor* executor,
                                  QList<WorkflowType>& nextFlowIds);

class StepExecutor_CreateEmptyFolder : public StepExecutor {
  Q_DECLARE_TR_FUNCTIONS(StepExecutor_CreateEmptyFolder)

  QString m_folderPath;

public:
  StepExecutor_CreateEmptyFolder(const ActionStep& step, QString sequence,
                                 QString sceneNumber, bool isUndo);

  QWidget* createWidget() override;
  bool execute(QString& log, QVariant& checkSum) override;
  static QJsonObject toJson(const QList<QVariant>& params);
  static QVariantList fromJson(const QJsonObject& json);
};

//--------------------------------------------------

class DougaRetakeWidget : public QWidget {
  Q_OBJECT
  QString m_sceneNumber;
  enum CheckProcessId {
    MainCheck,
    DougaKensa,
    // EnjoCheck 演助チェックは必ず通る
  };
  QButtonGroup* m_checkButtonGroup;
  QLabel* m_cueFileLabel;

public:
  DougaRetakeWidget(const QString sceneNumber, const QString orgPath,
                    QWidget* parent = nullptr);
  QString getCueFileName();
protected slots:
  void onCheckChanged();
};

class StepExecutor_DougaRetakeRouting : public StepExecutor {
  Q_DECLARE_TR_FUNCTIONS(StepExecutor_DougaRetakeRouting)

  QString m_orgPath;
  QString m_sequence;
  QString m_sceneNumber;
  DougaRetakeWidget* m_retakeWidget;

public:
  StepExecutor_DougaRetakeRouting(const ActionStep& step, QString orgPath,
                                  QString sequence, QString sceneNumber,
                                  bool isUndo);

  QWidget* createWidget() override;
  bool execute(QString& log, QVariant& checkSum) override;
};

class StepExecutor_ZipFolder : public StepExecutor {
  Q_DECLARE_TR_FUNCTIONS(StepExecutor_ZipFolder)

  QString m_orgFolderPath;
  QString m_zipFilePath;

  bool createPathAndConvertImageFile(const QString& fileName,
                                     const QString& newName, QString& log);
  bool doZip(const QString& sourcefolderPath, const QStringList& toFileList,
             QString& log, QVariant& checkSum);

public:
  StepExecutor_ZipFolder(const ActionStep& step, QString orgPath,
                         QString sequence, QString sceneNumber, bool isUndo);
  QWidget* createWidget() override;
  bool execute(QString& log, QVariant& checkSum) override;
  static QJsonObject toJson(const QList<QVariant>& params);
  static QVariantList fromJson(const QJsonObject& json);
};

//--------------------------------------------------

// Google Chatのスペースへ通知メッセージを送る。
// パラメータ: [0]投稿先名(QString) [1]通知メッセージ(QString)
// [2]自由記入欄の有無(bool)
//   [3]キャンセル時に通知するか(bool) [4]キャンセルメッセージ(QString)
//   [5]スレッドキー(QString)
// 実行時にはcheckSumへ実際に送信した本文（自由記入欄の内容を含む）を保存し、
// Undo時にキャンセル通知の本文（キャンセルメッセージ省略時は元の本文の再掲）を作るのに使う
class StepExecutor_NotifyGoogleChat : public StepExecutor {
  Q_DECLARE_TR_FUNCTIONS(StepExecutor_NotifyGoogleChat)

  QString m_destinationName;
  // 投稿先の名前がプロジェクトに登録されていて、Google Apps
  // ScriptのURLが設定されているか。 そうでない場合、このステップは飛ばされる
  bool m_isDestinationRegistered = false;
  bool m_hasGasUrl               = false;
  bool isAvailable() const { return m_isDestinationRegistered && m_hasGasUrl; }
  // 飛ばされる理由の説明（飛ばされない場合は空文字列）
  QString skipReasonText() const;
  QString m_message;  // マクロ展開済みの通知メッセージ
  QString
      m_cancelMessage;  // マクロ展開済みのキャンセルメッセージ。空なら元のメッセージを再掲する
  QString m_threadKey;  // マクロ展開済みのスレッドキー
  bool m_allowFreeText  = false;
  bool m_notifyOnCancel = true;
  QString
      m_freeText;  // 確認ダイアログの自由記入欄に入力された内容（非Undo時のみ）

  // 実際に送信する本文。非Undo時は通知メッセージ＋自由記入欄、Undo時はキャンセル通知の本文
  QString buildOutgoingText() const;

public:
  // workflowName/actionNameは、[WORKFLOW_NAME]/[ACTION_NAME]マクロに代入される、
  // このステップを実行しているWorkflowとPPM_Actionの名前
  StepExecutor_NotifyGoogleChat(const ActionStep& step, QString sequence,
                                QString sceneNumber, bool isUndo,
                                QString workflowName, QString actionName);

  // Messageが空欄のときに使われる既定のメッセージ（マクロ未展開）。
  // プロジェクト設定側のMessage欄のプレースホルダー表示にも使う。
  // hasEpisodesがfalse（話数機能を使わないプロジェクト）のときは、Episodeの項目を省く
  static QString defaultMessageTemplate(bool hasEpisodes);

  QWidget* createWidget() override;
  bool execute(QString& log, QVariant& checkSum) override;
  static QJsonObject toJson(const QList<QVariant>& params);
  static QVariantList fromJson(const QJsonObject& json);
};

//--------------------------------------------------

struct ActionInfo {
  // int workflowId;
  WorkflowType workflowType;
  int protocolId;
  QStringList orgPaths;
  QString sequence;
  QString sceneNumber;
  // 話数機能を使わないプロジェクトでは空文字列のまま。ConfirmationDialog側で
  // StepExecutor作成前にWorkflowManager::setCurrentEpisode()へ反映される
  QString episode = "";
};

class ConfirmationDialog : public QDialog {
  Q_OBJECT
protected:
  QList<ActionInfo> m_actionInfos;
  bool m_isUndo;
  bool m_isCanceled;

  QList<QList<StepExecutor*>> m_stepExecutors;

  QList<QList<QVariant>> m_fileCheckSums;

  // アクションごとに、そのアクション内でGoogleスプレッドシート入力ステップが位置する
  // 全てのインデックス（stepExecutorList内での位置）を保持する。ひとつのアクションに
  // 複数のGoogleスプレッドシート入力ステップがあり得るため、単一のintではなくリストで持つ
  QList<QList<int>> m_googleStepPos;

  StepExecutor* createExecutor(const FlowGroupId groupId,
                               const ActionStep& step,
                               const QStringList& orgPaths,
                               const QString& sequence,
                               const QString& sceneNumber, const bool isUndo,
                               int index                   = 0,
                               const QString& workflowName = QString(),
                               const QString& actionName   = QString());

public:
  ConfirmationDialog(ActionInfo actionInfo, bool isUndo);
  ConfirmationDialog(QList<ActionInfo> actionInfos, bool isUndo);

  ~ConfirmationDialog();
  // QWidget* createConfirmationWidget(ActionStep* step);

  QList<QList<QVariant>> fileCheckSums() { return m_fileCheckSums; }

  bool isCanceled() { return m_isCanceled; }

  // 指定したアクション（複数カット一括操作時のインデックス。単一アクションなら既定の0）が持つ、
  // Googleスプレッドシート入力ステップの位置（stepExecutorList内でのインデックス）の一覧。
  // Googleスプレッドシート入力ステップが無ければ空のリストを返す
  QList<int> googleStepPos(int index = 0) const {
    return m_googleStepPos[index];
  }

  virtual void registerFailedActionIndex(int) {}
protected slots:
  void onOkClicked();
};

//--------------------------------------------------

class ActionConfirmationDialog : public ConfirmationDialog {
  Q_OBJECT
  int m_actionId;
  QList<int> m_failedActionIndexes;

public:
  ActionConfirmationDialog(ActionInfo actionInfo, int actionId, bool isUndo,
                           // undo用にデータをセット
                           QList<QVariant> checkSums = QList<QVariant>());
  ActionConfirmationDialog(QList<ActionInfo> actionInfos);

  // 複数実行したとき、失敗したアクションを記録しておく
  void registerFailedActionIndex(int index) override {
    m_failedActionIndexes.append(index);
  }
  QList<int> failedActionIndexes() { return m_failedActionIndexes; }
};

//--------------------------------------------------

class RewindDestinationDialog : public QDialog {
  Q_OBJECT

  QComboBox* m_comboBox;

public:
  RewindDestinationDialog(WorkflowType workflowType, int protocolId);
  QPair<WorkflowType, int> destinationWorkflow();
};

//--------------------------------------------------

class RewindConfirmationDialog : public ConfirmationDialog {
  Q_OBJECT

  QPair<WorkflowType, int> m_destinationWfTypePair;

public:
  RewindConfirmationDialog(ActionInfo actionInfo,
                           QPair<WorkflowType, int> destinationWfTypePair,
                           bool isUndo,
                           // undo用にデータをセット
                           QList<QVariant> checkSums = QList<QVariant>());

  QList<QVariant> fileCheckSums() { return m_fileCheckSums[0]; }
};