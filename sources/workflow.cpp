#include "sharedfiles.h"
#include "workflow.h"
#include "workflow.h"
#include "actionconfirmationdialog.h"
#include "usersettings.h"

#include <QDir>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include <QSettings>
#include <QCoreApplication>
#include <QUrl>
#include <QTextCodec>
#include <QLockFile>
#include <QCryptographicHash>
#include <iostream>
#include <algorithm>

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

namespace {

// 1 -> "_t01"  2 -> "_t02"  3 -> "_t03"
// （テイク2の[TAKE-1]は"_t01"になる。前テイクの素材を切り分けるサブフォルダ名に使う）
QString takeNum2Suffix(const int take) {
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

inline QList<QVariant> createList(QVariant val) {
  QList<QVariant> list;
  list.append(val);
  return list;
}

// Media Type
static QStringList MediaLabelTxts = {
    "Digital", "Paper",
    "Other"  // Other Type
};

// Scene Info Tag Types
static QMap<SceneInfoTagType, QString> SceneInfoTagTypeTxts = {
    {InfoTagType_Bool, "Bool"},
    {InfoTagType_Enum, "Enum"},
    {InfoTagType_String, "String"},
    {InfoTagType_StringList, "StringList"},
};

// 暗号化に使用する固定キー（1バイトの任意の文字）
const char XOR_KEY = 0x4A;

/**
 * @brief パスを簡単に暗号化（難読化）する関数
 * @param clearText 暗号化したい元のパス
 * @return JSON保存用のBase64文字列
 */
QString encryptPath(const QString& clearText) {
  if (clearText.isEmpty()) return QString();

  // 1. QString を UTF-8 のバイト配列に変換
  QByteArray buffer = clearText.toUtf8();

  // 2. 固定キーでXOR演算（簡易暗号化）
  for (int i = 0; i < buffer.size(); ++i) {
    buffer[i] = buffer[i] ^ XOR_KEY;
  }

  // 3. JSONに安全に書き込めるよう、Base64エンコードしてQStringとして返す
  return QString::fromLatin1(buffer.toBase64());
}

/**
 * @brief 暗号化されたパスを復号する関数
 * @param cipherText JSONから読み込んだBase64文字列
 * @return 元のパス（QString）
 */
QString decryptPath(const QString& cipherText) {
  if (cipherText.isEmpty()) return QString();

  // 1. Base64文字列をバイト配列にデコード
  QByteArray buffer = QByteArray::fromBase64(cipherText.toLatin1());

  // 2. 再び同じ固定キーでXOR演算（復号）
  for (int i = 0; i < buffer.size(); ++i) {
    buffer[i] = buffer[i] ^ XOR_KEY;
  }

  // 3. UTF-8からQStringに戻して返す
  return QString::fromUtf8(buffer);
}
}  // namespace

//=======================================================

QJsonObject ActionStep::toJson() const {
  QJsonObject actionStepObj;
  actionStepObj["ActionStepType"] =
      WorkflowManager::instance()->actionStepTypeName(type);

  QJsonObject paramsObj;
  switch (type) {
  case Step_Rename:
    paramsObj = StepExecutor_Rename::toJson(params);
    break;
  case Step_Copy:
    paramsObj = StepExecutor_Copy::toJson(params);
    break;
  case Step_CopyFolderContents:
    paramsObj = StepExecutor_CopyFolderContents::toJson(params);
    break;
  case Step_RenameFile:
  case Step_CopyFile:
    paramsObj = StepExecutor_RenameFile::toJson(params);
    break;
  case Step_MoveFlowGroup:
    paramsObj = StepExecutor_MoveFlowGroup::toJson(params);
    break;
  case Step_CheckExistence:
    paramsObj = StepExecutor_CheckExistence::toJson(params);
    break;
  case Step_AskDelete:
    paramsObj = StepExecutor_AskDelete::toJson(params);
    break;
  case Step_CreateCueFile:
    paramsObj = StepExecutor_CreateCueFile::toJson(params);
    break;
  case Step_StartBranchFlow:
  case Step_EndBranchFlow:
    paramsObj = StepExecutor_StartEndBranchFlow::toJson(params);
    break;
  case Step_SetSceneInfo:
    paramsObj = StepExecutor_SetSceneInfo::toJson(params);
    break;
  case Step_ConditionalBranchByInfo: {
    // 第一引数：info名 （QString）
    // 第二引数：条件となる数値 （QVariant）
    // 第三引数：正の場合（ActionStep）
    // 第四引数：負の場合（ActionStep）
    paramsObj["InfoName"] = params[0].toString();
    paramsObj["Condition"] =
        params[1].toString();  // Bool他かもしれないけどStringで統一する

    if (!params[2].toList().isEmpty()) {
      ActionStep as_true = {(ActionStepType)params[2].toList()[0].toInt(),
                            params[2].toList()[1].toList()};
      paramsObj["ActionIfTrue"] = as_true.toJson();
    }

    if (!params[3].toList().isEmpty()) {
      ActionStep as_false = {(ActionStepType)params[3].toList()[0].toInt(),
                             params[3].toList()[1].toList()};
      paramsObj["ActionIfFalse"] = as_false.toJson();
    }
    break;
  }
  case Step_ConditionalBranchByFileExistence: {
    // 第一引数：ファイル名 （QString）
    // 第二引数：存在する場合（ActionStep）
    // 第三引数：存在しない場合（ActionStep）
    paramsObj["ConditionFilePath"] = params[0].toString();

    if (!(params[1].toList().isEmpty())) {
      ActionStep as_true = {(ActionStepType)params[1].toList()[0].toInt(),
                            params[1].toList()[1].toList()};
      paramsObj["ActionIfTrue"] = as_true.toJson();
    }

    if (!(params[2].toList().isEmpty())) {
      ActionStep as_false = {(ActionStepType)params[2].toList()[0].toInt(),
                             params[2].toList()[1].toList()};
      paramsObj["ActionIfFalse"] = as_false.toJson();
    }
    break;
  }
  case Step_InputGoogleSpreadsheet:
    paramsObj = StepExecutor_InputGoogleSpreadsheet::toJson(params);
    break;
  case Step_ReleaseProgress:
    paramsObj = StepExecutor_ReleaseProgress::toJson(params);
    break;
  case Step_AddProgress:
    paramsObj = StepExecutor_AddProgress::toJson(params);
    break;
  case Step_CreateEmptyFolder:
    paramsObj = StepExecutor_CreateEmptyFolder::toJson(params);
    break;
  case Step_ZipFolder:
    paramsObj = StepExecutor_ZipFolder::toJson(params);
    break;
  case Step_NotifyGoogleChat:
    paramsObj = StepExecutor_NotifyGoogleChat::toJson(params);
    break;
  }
  actionStepObj["ActionStepVariables"] = paramsObj;

  return actionStepObj;
}

ActionStep ActionStep::fromJson(const QJsonObject& actionStepObj) {
  ActionStep actionStep;

  if (const QJsonValue v = actionStepObj["ActionStepType"]; v.isString())
    actionStep.type =
        WorkflowManager::instance()->actionStepTypeId(v.toString());

  if (const QJsonValue v = actionStepObj["ActionStepVariables"]; v.isObject()) {
    switch (actionStep.type) {
    case Step_Rename:
      actionStep.params = StepExecutor_Rename::fromJson(v.toObject());
      break;
    case Step_Copy:
      actionStep.params = StepExecutor_Copy::fromJson(v.toObject());
      break;
    case Step_CopyFolderContents:
      actionStep.params =
          StepExecutor_CopyFolderContents::fromJson(v.toObject());
      break;
    case Step_RenameFile:
    case Step_CopyFile:
      actionStep.params = StepExecutor_RenameFile::fromJson(v.toObject());
      break;
    case Step_MoveFlowGroup:
      actionStep.params = StepExecutor_MoveFlowGroup::fromJson(v.toObject());
      break;
    case Step_CheckExistence:
      actionStep.params = StepExecutor_CheckExistence::fromJson(v.toObject());
      break;
    case Step_AskDelete:
      actionStep.params = StepExecutor_AskDelete::fromJson(v.toObject());
      break;
    case Step_CreateCueFile:
      actionStep.params = StepExecutor_CreateCueFile::fromJson(v.toObject());
      break;
    case Step_StartBranchFlow:
    case Step_EndBranchFlow:
      actionStep.params =
          StepExecutor_StartEndBranchFlow::fromJson(v.toObject());
      break;
    case Step_SetSceneInfo:
      actionStep.params = StepExecutor_SetSceneInfo::fromJson(v.toObject());
      break;
    case Step_ConditionalBranchByInfo: {
      actionStep.params.reserve(4);
      // 第一引数：info名 （QString）
      // 第二引数：条件となる数値 （QVariant）
      // 第三引数：正の場合（ActionStep）
      // 第四引数：負の場合（ActionStep）
      QJsonObject cbObj = v.toObject();
      if (const QJsonValue v = cbObj["InfoName"]; v.isString())
        actionStep.params.append(v.toString());
      if (const QJsonValue v = cbObj["Condition"];
          v.isString())  // Bool他かもしれないけどStringで統一する..いいのかな
        actionStep.params.append(v.toString());

      if (const QJsonValue v = cbObj["ActionIfTrue"]; v.isObject()) {
        ActionStep as_true = ActionStep::fromJson(v.toObject());
        QVariantList as_true_list;
        as_true_list.append(as_true.type);
        as_true_list.append(QVariant(as_true.params));
        actionStep.params.append(QVariant(as_true_list));
      } else
        actionStep.params.append(QVariant(QVariantList()));

      if (const QJsonValue v = cbObj["ActionIfFalse"]; v.isObject()) {
        ActionStep as_false = ActionStep::fromJson(v.toObject());
        QVariantList as_false_list;
        as_false_list.append(as_false.type);
        as_false_list.append(QVariant(as_false.params));
        actionStep.params.append(QVariant(as_false_list));
      } else
        actionStep.params.append(QVariant(QVariantList()));

      break;
    }
    case Step_ConditionalBranchByFileExistence: {
      actionStep.params.reserve(3);
      // 第一引数：ファイル名 （QString）
      // 第二引数：存在する場合（ActionStep）
      // 第三引数：存在しない場合（ActionStep）
      QJsonObject cbObj = v.toObject();
      if (const QJsonValue v = cbObj["ConditionFilePath"]; v.isString())
        actionStep.params.append(v.toString());

      if (const QJsonValue v = cbObj["ActionIfTrue"]; v.isObject()) {
        ActionStep as_true = ActionStep::fromJson(v.toObject());
        QVariantList as_true_list;
        as_true_list.append(as_true.type);
        as_true_list.append(QVariant(as_true.params));
        actionStep.params.append(QVariant(as_true_list));
      } else
        actionStep.params.append(QVariant(QVariantList()));

      if (const QJsonValue v = cbObj["ActionIfFalse"]; v.isObject()) {
        ActionStep as_false = ActionStep::fromJson(v.toObject());
        QVariantList as_false_list;
        as_false_list.append(as_false.type);
        as_false_list.append(QVariant(as_false.params));
        actionStep.params.append(QVariant(as_false_list));
      } else
        actionStep.params.append(QVariant(QVariantList()));
      break;
    }
    case Step_InputGoogleSpreadsheet:
      actionStep.params =
          StepExecutor_InputGoogleSpreadsheet::fromJson(v.toObject());
      break;
    case Step_ReleaseProgress:
      actionStep.params = StepExecutor_ReleaseProgress::fromJson(v.toObject());
      break;
    case Step_AddProgress:
      actionStep.params = StepExecutor_AddProgress::fromJson(v.toObject());
      break;
    case Step_CreateEmptyFolder:
      actionStep.params =
          StepExecutor_CreateEmptyFolder::fromJson(v.toObject());
      break;
    case Step_ZipFolder:
      actionStep.params = StepExecutor_ZipFolder::fromJson(v.toObject());
      break;
    case Step_NotifyGoogleChat:
      actionStep.params = StepExecutor_NotifyGoogleChat::fromJson(v.toObject());
      break;
    }
  }

  return actionStep;
}

//=======================================================

QJsonObject PPM_Action::toJson() const {
  QJsonObject actionObj;
  actionObj["ActionName"] = actionName;
  QJsonArray stepsArray;
  for (auto step : steps) stepsArray.append(step.toJson());
  actionObj["Steps"] = stepsArray;

  QJsonArray nextFlowsArray;
  for (auto nextFlow : nextFlows) nextFlowsArray.append((double)nextFlow);
  actionObj["NextFlowTypeIds"] = nextFlowsArray;
  return actionObj;
}

namespace {
// Step_ConditionalBranchByInfo/FileExistenceのparamsに埋め込まれた入れ子のActionStep
// （[種類(int), params(QVariantList)]のリスト。空のリストは「None」）
bool unpackNestedStep(const QVariant& packed, ActionStep& step) {
  QVariantList list = packed.toList();
  if (list.size() < 2) return false;
  step = ActionStep{(ActionStepType)list[0].toInt(), list[1].toList()};
  return true;
}

// 入れ子の分岐先のパラメータ位置
QList<int> nestedStepParamIndices(const ActionStep& step) {
  if (step.type == Step_ConditionalBranchByInfo && step.params.size() >= 4)
    return {2, 3};
  if (step.type == Step_ConditionalBranchByFileExistence &&
      step.params.size() >= 3)
    return {1, 2};
  return {};
}

void collectAddProgressLinks(const QList<ActionStep>& steps, bool nested,
                             QList<NextFlowLink>& out) {
  for (const ActionStep& step : steps) {
    if (step.type == Step_AddProgress) {
      if (!step.params.isEmpty() &&
          step.params.at(0).toUInt() != (uint)Wf_Undefined)
        out.append({(WorkflowType)step.params.at(0).toUInt(), nested});
    } else {
      for (int i : nestedStepParamIndices(step)) {
        ActionStep nestedStep;
        if (unpackNestedStep(step.params.at(i), nestedStep))
          collectAddProgressLinks({nestedStep}, true, out);
      }
    }
  }
}

// typeを追加するStep_AddProgressを取り除く。トップレベルのステップは削除し、入れ子のものは「None」にする
int removeAddProgressFromSteps(QList<ActionStep>& steps, WorkflowType type) {
  int removed = 0;
  for (int s = steps.size() - 1; s >= 0; s--) {
    ActionStep& step = steps[s];
    if (step.type == Step_AddProgress) {
      if (!step.params.isEmpty() && step.params.at(0).toUInt() == (uint)type) {
        steps.removeAt(s);
        removed++;
      }
      continue;
    }
    for (int i : nestedStepParamIndices(step)) {
      ActionStep nestedStep;
      if (!unpackNestedStep(step.params.at(i), nestedStep)) continue;
      if (nestedStep.type == Step_AddProgress) {
        if (!nestedStep.params.isEmpty() &&
            nestedStep.params.at(0).toUInt() == (uint)type) {
          step.params[i] = QVariant(QVariantList());  // 「None」
          removed++;
        }
      } else {
        QList<ActionStep> nestedList = {nestedStep};
        int n = removeAddProgressFromSteps(nestedList, type);
        if (n > 0) {
          // 入れ子の中のさらに入れ子が書き換わったので、書き戻す
          QVariantList packed;
          packed.append((int)nestedList.at(0).type);
          packed.append(QVariant(nestedList.at(0).params));
          step.params[i] = packed;
          removed += n;
        }
      }
    }
  }
  return removed;
}
}  // namespace

QList<NextFlowLink> PPM_Action::nextFlowLinks() const {
  QList<NextFlowLink> links;
  for (WorkflowType next : nextFlows) {
    bool exists = false;
    for (const NextFlowLink& l : links)
      if (l.type == next) {
        exists = true;
        break;
      }
    if (!exists) links.append({next, false});
  }
  QList<NextFlowLink> stepLinks;
  collectAddProgressLinks(steps, false, stepLinks);
  for (const NextFlowLink& sl : stepLinks) {
    bool exists = false;
    for (NextFlowLink& l : links) {
      if (l.type != sl.type) continue;
      exists = true;
      if (!sl.conditional)
        l.conditional = false;  // 無条件で繋がるものがあれば、無条件のリンク
      break;
    }
    if (!exists) links.append(sl);
  }
  return links;
}

QList<WorkflowType> PPM_Action::allNextFlows() const {
  QList<WorkflowType> ret;
  for (const NextFlowLink& l : nextFlowLinks()) ret.append(l.type);
  return ret;
}

QList<WorkflowType> PPM_Action::stepAddedFlows() const {
  QList<NextFlowLink> stepLinks;
  collectAddProgressLinks(steps, false, stepLinks);
  QList<WorkflowType> ret;
  for (const NextFlowLink& l : stepLinks)
    if (!ret.contains(l.type)) ret.append(l.type);
  return ret;
}

int PPM_Action::removeAddProgressSteps(WorkflowType type) {
  return removeAddProgressFromSteps(steps, type);
}

PPM_Action PPM_Action::fromJson(const QJsonObject& actionObj) {
  PPM_Action action;

  if (const QJsonValue v = actionObj["ActionName"]; v.isString())
    action.actionName = v.toString();

  if (const QJsonValue v = actionObj["Steps"]; v.isArray()) {
    const QJsonArray stepsArray = v.toArray();
    for (const QJsonValue& stepVal : stepsArray) {
      action.steps.append(ActionStep::fromJson(stepVal.toObject()));
    }
  }

  if (const QJsonValue v = actionObj["NextFlowTypeIds"]; v.isArray()) {
    const QJsonArray nextFlowsArray = v.toArray();
    for (const QJsonValue& nextFlowVal : nextFlowsArray) {
      if (nextFlowVal.isDouble())
        action.nextFlows.append(
            static_cast<WorkflowType>(nextFlowVal.toDouble()));
    }
  }

  return action;
}

//=======================================================

QJsonObject WorkflowProtocol::toJson() const {
  QJsonObject protocolObj;
  protocolObj["MediaType"]     = MediaLabelTxts[mediaType];
  protocolObj["ConditionPath"] = conditionPath;

  QJsonArray actionsArray;
  for (auto action : actions) actionsArray.append(action.toJson());
  protocolObj["Actions"] = actionsArray;
  return protocolObj;
}

WorkflowProtocol WorkflowProtocol::fromJson(const QJsonObject& protocolObj) {
  WorkflowProtocol protocol;
  if (const QJsonValue v = protocolObj["MediaType"]; v.isString())
    protocol.mediaType =
        static_cast<MediaType>(MediaLabelTxts.indexOf(v.toString()));
  if (const QJsonValue v = protocolObj["ConditionPath"]; v.isString())
    protocol.conditionPath = v.toString();

  if (const QJsonValue v = protocolObj["Actions"]; v.isArray()) {
    const QJsonArray actionsArray = v.toArray();
    for (const QJsonValue& actionVal : actionsArray) {
      protocol.actions.append(PPM_Action::fromJson(actionVal.toObject()));
    }
  }
  return protocol;
}

//=======================================================

QJsonObject Workflow::toJson() const {
  QJsonObject wfObj;
  wfObj["WorkflowTypeId"] = (double)workflowType;
  wfObj["WorkflowGroupIdName"] =
      WorkflowManager::instance()->flowGroupIdName(groupId);
  wfObj["WorkflowName"]           = name;
  wfObj["Color"]                  = color.name();
  wfObj["Suffix"]                 = suffix;
  wfObj["ClearsRewindOnComplete"] = clearsRewindOnComplete;

  QJsonArray protocolsArray;
  for (auto protocol : protocols) protocolsArray.append(protocol.toJson());
  wfObj["Protocols"] = protocolsArray;

  return wfObj;
}

Workflow Workflow::fromJson(const QJsonObject& wfObj) {
  Workflow wf;

  if (const QJsonValue v = wfObj["WorkflowTypeId"]; v.isDouble())
    wf.workflowType = static_cast<WorkflowType>(v.toDouble());
  if (const QJsonValue v = wfObj["WorkflowGroupIdName"]; v.isString())
    wf.groupId = WorkflowManager::instance()->IdNameToFlowGroupId(v.toString());
  if (const QJsonValue v = wfObj["WorkflowName"]; v.isString())
    wf.name = v.toString();
  if (const QJsonValue v = wfObj["Color"]; v.isString())
    wf.color = QColor::fromString(v.toString());
  if (const QJsonValue v = wfObj["Suffix"]; v.isString())
    wf.suffix = v.toString();
  if (const QJsonValue v = wfObj["ClearsRewindOnComplete"]; v.isBool())
    wf.clearsRewindOnComplete = v.toBool();

  if (const QJsonValue v = wfObj["Protocols"]; v.isArray()) {
    const QJsonArray protocolsArray = v.toArray();
    for (const QJsonValue& protocolVal : protocolsArray)
      wf.protocols.append(WorkflowProtocol::fromJson(protocolVal.toObject()));
  }

  return wf;
}

//=======================================================

QString WorkflowManager::sceneNum2Sequence(QString cutName) const {
  if (!m_useSequence) return QString();      // シークエンスを使わない設定
  if (cutName.size() < 3) return QString();  // 2文字以下の場合
  // 100カットずつのまとまりにする＝連番の下2ケタを削る
  for (int i = 0; i < cutName.size(); i++) {
    if (!cutName.at(i).isDigit()) {
      if (i < 3)
        return QString();  // 先頭に数字が3ケタ以上続かない場合
      else {
        cutName.truncate(i - 2);
        return cutName;
      }
    }
  }

  cutName.truncate(cutName.size() - 2);
  return cutName;
}

bool WorkflowManager::isValidSceneName(const QString& cutName) const {
  if (cutName.isEmpty()) return false;
  if (!m_useSequence) return true;
  return !sceneNum2Sequence(cutName).isEmpty();
}

QString WorkflowManager::invalidSceneNameMessage(
    const QStringList& cutNames) const {
  return tr("The scene name(s) %1 cannot be used. While [SEQUENCE] is enabled "
            "in the project settings, "
            "scene names must begin with a number of 3 or more digits.")
      .arg(cutNames.join(", "));
}

QString WorkflowManager::collapseSeparators(const QString& path) {
  // 先頭の"//"は、ネットワークパス（UNC）のため残す
  const QString head = path.startsWith("//") ? QString("//") : QString();
  QString rest       = path.mid(head.size());
  while (rest.contains("//")) rest.replace("//", "/");
  return head + rest;
}

WorkflowManager* WorkflowManager::instance() {
  static WorkflowManager _instance;
  return &_instance;
}

WorkflowManager::WorkflowManager() {
  QString prjName =
      UserSettings::instance()->loadCurrentProjectName("sample_project");
  if (!prjName.isEmpty()) m_currentProjectName = prjName;

  //---- ファイル操作
  m_actionStepTypeInfos[Step_Rename] = {"Step_Rename", tr("Rename")};
  m_actionStepTypeInfos[Step_Copy]   = {"Step_Copy", tr("Copy")};
  m_actionStepTypeInfos[Step_CopyFolderContents] = {"Step_CopyFolderContents",
                                                    tr("Copy Folder Contents")};
  m_actionStepTypeInfos[Step_RenameFile]         = {"Step_RenameFile",
                                                    tr("Rename File")};
  m_actionStepTypeInfos[Step_CopyFile] = {"Step_CopyFile", tr("Copy File")};
  m_actionStepTypeInfos[Step_CreateCueFile] = {
      "Step_CreateCueFile",
      tr("Create Cue File")};  // 空のテキストファイルを作る
  m_actionStepTypeInfos[Step_AskDelete] = {"Step_AskDelete", tr("Ask Delete")};
  m_actionStepTypeInfos[Step_CreateEmptyFolder] = {
      "Step_CreateEmptyFolder",
      tr("Create Empty Folder")};  // 空のフォルダを作る
  m_actionStepTypeInfos[Step_ZipFolder] = {
      "Step_ZipFolder", tr("Zip Folder")};  // フォルダをZIP圧縮
  //---- 進捗管理
  m_actionStepTypeInfos[Step_MoveFlowGroup]   = {"Step_MoveFlowGroup",
                                                 tr("Move Flow Group")};
  m_actionStepTypeInfos[Step_StartBranchFlow] = {"Step_StartBranchFlow",
                                                 tr("Start Branch Flow")};
  m_actionStepTypeInfos[Step_EndBranchFlow]   = {"Step_EndBranchFlow",
                                                 tr("End Branch Flow")};
  m_actionStepTypeInfos[Step_SetSceneInfo]    = {"Step_SetSceneInfo",
                                                 tr("Set Scene Info Tag")};
  m_actionStepTypeInfos[Step_AddProgress]     = {
      "Step_AddProgress",
      tr("Add Next Workflow")};  // 次工程に、指定した工程を追加する
  m_actionStepTypeInfos[Step_ReleaseProgress] = {
      "Step_ReleaseProgress",
      tr("Release Progress")};  // 詳細進捗を解除する（ペイント完了時に打ち込みを解除するときのみ使用）
  //---- 確認、分岐
  m_actionStepTypeInfos[Step_CheckExistence]          = {"Step_CheckExistence",
                                                         tr("Check Existence")};
  m_actionStepTypeInfos[Step_ConditionalBranchByInfo] = {
      "Step_ConditionalBranchByInfo",
      tr("Conditional Branch By Info Tag")};  // infoに応じて条件を分岐する
  m_actionStepTypeInfos[Step_ConditionalBranchByFileExistence] = {
      "Step_ConditionalBranchByFileExistence",
      tr("Conditional Branch By File Existence")};  // ファイルの有無に応じて条件を分岐する
  //---- 外部連携その他
  m_actionStepTypeInfos[Step_InputGoogleSpreadsheet] = {
      "Step_InputGoogleSpreadsheet",
      tr("Input Google Spreadsheet")};  // Googleスプレッドシートのカット表に入力する
  m_actionStepTypeInfos[Step_NotifyGoogleChat] = {
      "Step_NotifyGoogleChat",
      tr("Notify Google Chat")};  // Google Chatへ通知を送る
  m_actionStepTypeInfos[Step_DougaRetakeRouting] = {
      "Step_DougaRetakeRouting",
      tr("Douga Retake Routing")};  // 動画リテイクのための特別なステップ。メイン／動検／演助チェックの有無を選択する
}

WorkflowManager::~WorkflowManager() { deleteAllSceneInfoTags(); }

namespace {
// ログ・シーン情報のルートフォルダの下の、シークエンスのフォルダへ移動する。
// sequenceが空文字列（シークエンスを使わない設定）の場合は、ルートフォルダのまま。
// createがtrueの場合は、フォルダが無ければ作る。falseでフォルダが無い場合は、falseを返す
bool enterSequenceFolder(QDir& dir, const QString& sequence, bool create) {
  if (create) {
    if (!dir.mkpath(sequence.isEmpty() ? QString(".") : sequence)) return false;
  } else if (!dir.exists())
    return false;
  return sequence.isEmpty() || dir.cd(sequence);
}
QString sequenceFolderPath(const QDir& dir, const QString& sequence) {
  return sequence.isEmpty() ? dir.path() : dir.filePath(sequence);
}
}  // namespace

void WorkflowManager::logAction(const QString& sequence,
                                const QString& cutNumber,
                                const QString& workflow,
                                const QString& action) {
  if (logRootPath().isEmpty()) return;
  QDir dir(logRootPathForCurrentEpisode());
  // シークエンス名のフォルダを作る（シークエンスを使わない場合は、ルートフォルダに直接保存する）
  if (!enterSequenceFolder(dir, sequence, true)) {
    QMessageBox::critical(nullptr, tr("Error"),
                          tr("Failed to access the log folder %1")
                              .arg(sequenceFolderPath(dir, sequence)));
    return;
  }

  QString fileName =
      m_cutNumberPrefix + cutNumber + ".md";  // markdownで書いてみる

  bool isFirstLog = !dir.exists(fileName);

  QFile logFile(dir.absoluteFilePath(fileName));
  if (!logFile.open(QFile::WriteOnly | QFile::Append)) {
    QMessageBox::critical(nullptr, tr("Error"),
                          tr("Failed to open the log file %1")
                              .arg(dir.absoluteFilePath(fileName)));
    return;
  }

  QString logStr;
  if (isFirstLog) {
    logStr +=
        "## " +
        tr("Log of the scene %1%2").arg(m_cutNumberPrefix).arg(cutNumber) +
        "\n\n" + "|" + tr("Date and time") + "|" + "Workflow" + "|" +
        tr("Action") + "|" + tr("User name") + "|\n" +
        "|-------------|----------|----------|---------|\n";

    // ts << "## " << tr("Log of the scene
    // %1%2").arg(m_cutNumberPrefix).arg(cutNumber)
    //   << "\n\n" << "|" << tr("Date and time") << "|" << "Workflow"
    //   << "|" << tr("Action") << "|" << tr("User name") << "|\n"
    //   << "|-------------|----------|----------|---------|\n";
  }

  QString username = qgetenv("USER");
  if (username.isEmpty()) username = qgetenv("USERNAME");

  logStr += "|" +
            QDateTime::currentDateTime().toString(tr("yyyy MM/dd hh:mm:ss")) +
            "|" + workflow + "|" + action + "|" + username + "|\n";
  // ts << "|" << QDateTime::currentDateTime().toString(tr("yyyy MM/dd
  // hh:mm:ss")).toLocal8Bit()
  //   << "|" << workflow.toLocal8Bit()
  //   << "|" << action.toLocal8Bit()
  //   << "|" << username
  //   << "|\n";
  //
  std::cout << logStr.toStdString() << std::endl << std::endl;
  std::cout << logStr.toLocal8Bit().toStdString() << std::endl;

  // QTextCodec* codec = QTextCodec::codecForName("Shift-JIS");

  // QByteArray encodedString = codec->fromUnicode(logStr);
  // logFile.write(encodedString);
  logFile.write(logStr.toUtf8());
  // QTextStream ts(&logFile);
  // ts << encodedString;
}

void WorkflowManager::saveSceneInfoTag(const QString& sequence,
                                       const QString& cutNumber,
                                       const QString& infoTagName,
                                       const QVariant& value,
                                       const QString& actionName) {
  if (infoFolderPath().isEmpty()) return;
  QDir dir(sceneInfoFolderPath());
  // シークエンス名のフォルダを作る
  if (!enterSequenceFolder(dir, sequence, true)) {
    QMessageBox::critical(nullptr, tr("Error"),
                          tr("Failed to access the scene info folder %1")
                              .arg(sequenceFolderPath(dir, sequence)));
    return;
  }

  QString fileName = m_cutNumberPrefix + cutNumber + ".ini";  // blu0001.ini
  QSettings settings(dir.absoluteFilePath(fileName), QSettings::IniFormat);

  settings.setValue(infoTagName, value);
  settings.sync();
  if (settings.status() != QSettings::NoError) {
    QMessageBox::critical(
        nullptr, tr("Error"),
        tr("Failed to write the scene info file %1")
            .arg(QDir::toNativeSeparators(settings.fileName())));
    return;
  }

  // RegisterDialogから呼ばれるときはactionNameが入っている
  if (!actionName.isEmpty())
    logAction(sequence, cutNumber, tr("Modify Cut Info"), actionName);
}

void WorkflowManager::releaseSceneInfoTag(const QString& sequence,
                                          const QString& cutNumber,
                                          const QString& infoTagName,
                                          const QString& actionName) {
  if (infoFolderPath().isEmpty()) return;
  QDir dir(sceneInfoFolderPath());
  // シークエンス名のフォルダを作る
  if (!enterSequenceFolder(dir, sequence, true)) {
    QMessageBox::critical(nullptr, tr("Error"),
                          tr("Failed to access the scene info folder %1")
                              .arg(sequenceFolderPath(dir, sequence)));
    return;
  }

  QString fileName = m_cutNumberPrefix + cutNumber + ".ini";  // blu0001.ini
  QSettings settings(dir.absoluteFilePath(fileName), QSettings::IniFormat);

  settings.remove(infoTagName);
  settings.sync();
  if (settings.status() != QSettings::NoError) {
    QMessageBox::critical(
        nullptr, tr("Error"),
        tr("Failed to write the scene info file %1")
            .arg(QDir::toNativeSeparators(settings.fileName())));
    return;
  }

  // RegisterDialogから呼ばれるときはactionNameが入っている
  if (!actionName.isEmpty())
    logAction(sequence, cutNumber, tr("Modify Cut Info"), actionName);
}

QVariant WorkflowManager::sceneInfoTag(const QString& sequence,
                                       const QString& cutNumber,
                                       const QString& infoTagName,
                                       const QVariant& defaultValue) const {
  if (infoFolderPath().isEmpty()) return defaultValue;
  QDir dir(sceneInfoFolderPath());
  if (!enterSequenceFolder(dir, sequence, false)) return defaultValue;

  QString fileName = m_cutNumberPrefix + cutNumber + ".ini";  // blu0001.ini

  if (!dir.exists(fileName)) return defaultValue;

  QSettings settings(dir.absoluteFilePath(fileName), QSettings::IniFormat);

  return settings.value(infoTagName, defaultValue);
}

const QMap<QString, QVariant> WorkflowManager::sceneInfoTags(
    const QString& sequence, const QString& cutNumber) {
  if (infoFolderPath().isEmpty()) return {};
  QDir dir(sceneInfoFolderPath());
  if (!enterSequenceFolder(dir, sequence, false))
    return QMap<QString, QVariant>();

  QString fileName = m_cutNumberPrefix + cutNumber + ".ini";  // blu0001.ini

  if (!dir.exists(fileName)) return QMap<QString, QVariant>();

  QSettings settings(dir.absoluteFilePath(fileName), QSettings::IniFormat);

  QMap<QString, QVariant> ret;
  for (auto key : settings.allKeys()) {
    ret.insert(key, settings.value(key));
  }
  return ret;
}

const QString WorkflowManager::getSceneInfoTagsRichText(
    const QString& sequence, const QString& cutNumber) {
  QMap<QString, QVariant> infos = sceneInfoTags(sequence, cutNumber);
  if (infos.isEmpty()) return QString();

  QString ret     = "&nbsp;";
  bool hasAnyInfo = false;

  for (auto infoTagName : infos.keys()) {
    QVariant value = infos.value(infoTagName);

    SceneInfoTagDisplayProperties dispProp =
        WorkflowManager::instance()->sceneInfoTagDisplayProperties(infoTagName,
                                                                   value);
    if (dispProp.labelTxt.isEmpty()) continue;

    ret += QString(
               "&nbsp;<span style=\"font-weight:600; color:%1; "
               "background-color:%2;\">&nbsp;%3&nbsp;</span>&nbsp;")
               .arg(dispProp.txtColor.name())
               .arg(dispProp.bgColor.name())
               .arg(dispProp.labelTxt);

    hasAnyInfo = true;
  }

  if (hasAnyInfo) return ret;

  return QString();
}

// 情報タグの表示用のデータを返す
const SceneInfoTagDisplayProperties
WorkflowManager::sceneInfoTagDisplayProperties(const QString& infoTagIdName,
                                               const QVariant& value) {
  for (auto infoTag : m_sceneInfoTags) {
    if (infoTag->idName() == infoTagIdName)
      return infoTag->displayProperties(value);
  }
  return SceneInfoTagDisplayProperties();
}

QString WorkflowManager::decodePathMacros(QString path, bool decodeObfuscated) {
  for (auto macroInfo : m_pathMacros) {
    if (path.contains(macroInfo.macroStr)) {
      // 難読化フラグの判定
      if (!macroInfo.doObfuscate || decodeObfuscated)
        path.replace(macroInfo.macroStr, macroInfo.path);
      return path;
    }
  }
  return path;
}

// アクション実行時、WorkflowのCodedPathを全てDecodeする。
// 兼用でない通常のカットの場合、bankSceneNumbersは空になる
// 兼用カットの場合、stockSceneNumbersはカット番号を繋げた文字列、sceneNumberには個々のカット番号が入る
QString WorkflowManager::decodePath(const QString& codedPath,
                                    const QString& sequence,
                                    const QString& sceneNumber,
                                    const QString& stockSceneNumbers,
                                    bool filesystemPath) {
  QString retPath = codedPath;

  retPath = decodePathMacros(retPath);

  // decode reserved macros
  retPath.replace("[PREFIX]", cutNumberPrefix());
  if (retPath.contains("[USER_NAME]"))
    retPath.replace("[USER_NAME]", UserSettings::instance()->userName());
  if (retPath.contains("[EPISODE]"))
    retPath.replace("[EPISODE]", m_currentEpisode);
  if (retPath.contains("[SEQUENCE]")) retPath.replace("[SEQUENCE]", sequence);
  // [SEQUENCE]が空文字列のとき（シークエンスを使わない設定）、"//"が残らないようにする
  if (sequence.isEmpty()) retPath = collapseSeparators(retPath);
  if (retPath.contains("[CUTNUMBER]")) {
    if (stockSceneNumbers.isEmpty())
      retPath.replace("[CUTNUMBER]", sceneNumber);
    else {  // 兼用カットの場合
      // ファイル名だけを個別のカット番号に置き換える
      int sepIndex =
          std::max(retPath.lastIndexOf('/'), retPath.lastIndexOf('\\'));
      QString folderPath = retPath.left(sepIndex);
      QString fileName   = retPath.right(retPath.size() - sepIndex);
      folderPath.replace("[CUTNUMBER]", stockSceneNumbers);
      fileName.replace("[CUTNUMBER]", sceneNumber);
      retPath = folderPath + fileName;
    }
  }

  //[TAKE]をテイク接尾辞に置き換え
  if (retPath.contains("[TAKE]"))
    retPath.replace("[TAKE]", currentTakeSuffix(sceneNumber));
  if (retPath.contains("[TAKE-1]"))
    retPath.replace("[TAKE-1]", previousTakeSuffix(sceneNumber));
  if (retPath.contains("[R_TAKE]"))
    retPath.replace("[R_TAKE]", takeNum2RSuffix(currentTake(sceneNumber)));
  //[R]をRewind接尾辞に置き換え
  if (retPath.contains("[R]")) {
    retPath.replace("[R]", currentRewindSuffix(sceneNumber));
  }

  // decode Root path
  if (filesystemPath)
    retPath = resolveProjectPath(retPath);
  else
    retPath.replace("[ROOT]", projectRootPath());

  //[PREV_EXISTING_TAKE]をテイク接尾辞に置き換える。存在する直前のテイク接尾辞を返す。
  // フォルダの有無を実際のパスで調べるため、[ROOT]などを置き換えた後に行う
  if (retPath.contains("[PREV_EXISTING_TAKE]")) {
    int curTake            = currentTake(sceneNumber);
    QString lastTakeSuffix = "";
    // テイクをさかのぼり、フォルダがあったらその接尾辞にする
    for (int lastTake = curTake - 1; lastTake >= 2; lastTake--) {
      QString tmpSuffix = takeNum2Suffix(lastTake);
      QString tmpPath   = retPath;
      tmpPath.replace("[PREV_EXISTING_TAKE]", tmpSuffix);
      // 難読化されたパスマクロが残っている場合は、デコードしてから調べる
      tmpPath = decodeObfuscatedPathMacros(tmpPath, sequence, sceneNumber,
                                           stockSceneNumbers);
      if (!tmpPath.isEmpty() &&
          QFileInfo(SharedFiles::actualPath(tmpPath)).isDir()) {
        lastTakeSuffix = tmpSuffix;
        break;
      }
    }
    retPath.replace("[PREV_EXISTING_TAKE]", lastTakeSuffix);
  }

  return retPath;
}

// 難読化オプションがONのパスマクロを、操作に使う直前に、実際のパスへデコードする
QString WorkflowManager::decodeObfuscatedPathMacros(
    const QString& path, const QString& sequence, const QString& sceneNumber,
    const QString& stockSceneNumbers) {
  QString result   = path;
  bool replacedAny = false;
  // マクロが定義しているパスに、別の難読化マクロが含まれる場合のため、置き換えるものが無くなるまで繰り返す
  // （循環した定義でも止まるよう、回数に上限を設ける）
  for (int depth = 0; depth < 5; depth++) {
    bool replaced = false;
    for (const PathMacroInfo& macroInfo : m_pathMacros) {
      if (macroInfo.doObfuscate && !macroInfo.macroStr.isEmpty() &&
          result.contains(macroInfo.macroStr)) {
        result.replace(macroInfo.macroStr, macroInfo.path);
        replaced = true;
      }
    }
    if (!replaced) break;
    replacedAny = true;
  }
  if (!replacedAny) return path;

  // 難読化されたパスに含まれていた、[ROOT]等の既定のマクロ（と通常のパスマクロ）をデコードする
  return decodePath(result, sequence, sceneNumber, stockSceneNumbers);
}

// 難読化マクロをデコードした後のパスを、マクロの文字列に戻す
QString WorkflowManager::hideObfuscatedPaths(const QString& text,
                                             const QString& sequence,
                                             const QString& sceneNumber,
                                             const QString& stockSceneNumbers) {
  // (デコード後のパス, マクロの文字列) の組。長いパスから順に置き換える
  QList<QPair<QString, QString>> pairs;
  for (const PathMacroInfo& macroInfo : m_pathMacros) {
    if (!macroInfo.doObfuscate || macroInfo.macroStr.isEmpty() ||
        macroInfo.path.isEmpty())
      continue;
    QString decoded = decodeObfuscatedPathMacros(
        macroInfo.macroStr, sequence, sceneNumber, stockSceneNumbers);
    // 短すぎるパスは、無関係な文字列にも一致してしまうため、対象にしない
    if (decoded.size() >= 3 && decoded != macroInfo.macroStr)
      pairs.append({decoded, macroInfo.macroStr});
  }
  std::sort(
      pairs.begin(), pairs.end(),
      [](const QPair<QString, QString>& a, const QPair<QString, QString>& b) {
        return a.first.size() > b.first.size();
      });

  QString result = text;
  for (const auto& pair : pairs) {
    // 区切り文字が"/"のものと"\"のもの、どちらで書かれていても取り除く
    result.replace(pair.first, pair.second, Qt::CaseInsensitive);
    QString native = QDir::toNativeSeparators(pair.first);
    if (native != pair.first)
      result.replace(native, pair.second, Qt::CaseInsensitive);
  }
  return result;
}

QString WorkflowManager::resolveProjectPath(const QString& expanded) const {
  QString stored = expanded;
  if (stored.contains("[ROOT]")) {
    if (!PathMappings::isAbsolute(m_projectRootPath)) {
      const QString localRoot = projectRootPath();
      if (localRoot.isEmpty()) return {};
      stored.replace("[ROOT]", localRoot);
      return stored;
    }
    stored.replace("[ROOT]", m_projectRootPath);
  }
  return UserSettings::instance()->pathMappings().filesystemPath(stored);
}

QString WorkflowManager::resolvePathFromStuffFolder(const QString& path) {
  if (path.isEmpty()) return path;
  if (PathMappings::isAbsolute(path))
    return SharedFiles::actualPath(
        UserSettings::instance()->pathMappings().filesystemPath(path));
  if (!QDir::isRelativePath(path)) return path;
  return QDir::cleanPath(UserSettings::instance()->stuffFolderPath() + "/" +
                         path);
}

// pathがProjectRootの下にある場合、Rootからの相対パスを返す
QString WorkflowManager::relativePathFromProjectRoot(const QString& path) {
  const QString rootPath = projectRootPath();
  if (rootPath.isEmpty() ||
      !(path == rootPath ||
        path.startsWith(rootPath.endsWith('/') ? rootPath : rootPath + '/')))
    return path;
  return QDir(rootPath).relativeFilePath(path);
}

// return decoded path
QString WorkflowManager::cutFolderPath(FlowGroupId id, QString sequence,
                                       QString sceneNumber, int take) {
  FlowGroupInfo info = m_flowGroupInfos.value(id);
  QString ret;
  for (auto macroInfo : m_pathMacros) {
    if (macroInfo.macroStr == info.standardCutFolderMacro) {
      ret = macroInfo.path;
      break;
    }
  }

  ret.replace("[PREFIX]", m_cutNumberPrefix);
  ret.replace("[USER_NAME]", UserSettings::instance()->userName());
  ret.replace("[EPISODE]", m_currentEpisode);
  ret.replace("[SEQUENCE]", sequence);
  if (sequence.isEmpty()) ret = collapseSeparators(ret);
  ret.replace("[CUTNUMBER]", sceneNumber);

  if (take > 1) {
    ret.replace("[TAKE]", takeNum2Suffix(take));
  } else
    ret.replace("[TAKE]", "");

  ret.replace("[R_TAKE]", takeNum2RSuffix(take));

  return SharedFiles::actualPath(resolveProjectPath(ret));
}

// 兼用カットの場合、0150_0152 → 0150 と
// 0152、のようにXDTSが2つに分かれることがある。
// 候補が複数あったとき、ファイルが実際に存在するパスを全て格納したリストを返す
QStringList WorkflowManager::xdtsPaths(FlowGroupId id, QString sequence,
                                       QString sceneNumber,
                                       QStringList& xdtsFileNames,
                                       bool obtainBothGengaAndDouga,
                                       const QStringList& cueFilePaths,
                                       bool* ambiguous) {
  bool conflict = false;
  if (ambiguous) *ambiguous = false;
  QStringList ret;

  QStringList sceneNames = sceneNumber.split('_', Qt::SkipEmptyParts);
  if (sceneNames.size() > 1) sceneNames.append(sceneNumber);

  FlowGroupInfo info   = m_flowGroupInfos.value(id);
  QString baseFileName = info.xdtsFileName;
  baseFileName.replace("[PREFIX]", m_cutNumberPrefix);
  baseFileName.replace("[USER_NAME]", UserSettings::instance()->userName());
  baseFileName.replace("[EPISODE]", m_currentEpisode);
  baseFileName.replace("[SEQUENCE]", sequence);
  // 撮影工程のタイムシートのパスには[R_TAKE]マクロが含まれている
  if (info.takeNotationStyle == FlowGroupInfo::TakeNotation_RTake &&
      baseFileName.contains("[R_TAKE]")) {
    baseFileName.replace("[R_TAKE]", currentTakeSuffix(sceneNumber, true));
  }

  // 探すフォルダ：標準カットフォルダと、キューファイルのあるフォルダ。
  // チェック担当ごとのフォルダへカットフォルダを移して受け渡すスタジオでは、
  // タイムシートが標準カットフォルダではなく、キューファイルと同じフォルダにある
  QStringList searchFolders;
  searchFolders.append(
      cutFolderPath(id, sequence, sceneNumber, currentTake(sceneNumber)));
  for (auto& cueFilePath : cueFilePaths) {
    if (cueFilePath.isEmpty()) continue;
    QString folder = QFileInfo(cueFilePath).absolutePath();
    bool already   = false;
    for (auto& f : searchFolders)
      if (SharedFiles::sameEntry(f, folder)) already = true;
    if (!already) searchFolders.append(folder);
  }

  // 見つかったファイルを追加する。同じ名前のファイルが複数のフォルダにある場合は、
  // 先に探したフォルダ（標準カットフォルダ）のものだけを使う
  // （xdtsFileNamesは、メニューの表示名やコピーするファイル名として使われるため）
  auto addIfExists = [&](const QString& fileName) {
    for (const QString& found : xdtsFileNames)
      if (found.normalized(QString::NormalizationForm_C) ==
          fileName.normalized(QString::NormalizationForm_C))
        return true;
    for (auto& folder : searchFolders) {
      if (folder.isEmpty()) continue;
      QString tmpPath = SharedFiles::actualPath(folder + "/" + fileName);
      if (tmpPath.isEmpty()) {
        conflict = true;
        return false;
      }
      if (QFileInfo(tmpPath).exists()) {
        ret.append(tmpPath);
        xdtsFileNames.append(QFileInfo(tmpPath).fileName());
        return true;
      }
    }
    return false;
  };

  for (auto sceneName : sceneNames) {
    QString fileName = baseFileName;
    fileName.replace("[CUTNUMBER]", sceneName);

    if (fileName.contains(
            "[GENGA_OR_DOUGA]")) {  // genga_ts または douga_ts
                                    // のどちらかが存在すれば格納する
      for (int d_or_g = 0; d_or_g < 2; d_or_g++) {
        QString tmpFileName = fileName;
        tmpFileName.replace("[GENGA_OR_DOUGA]",
                            (d_or_g == 0) ? "douga" : "genga");
        if (addIfExists(tmpFileName) && !obtainBothGengaAndDouga) break;
      }
    } else
      addIfExists(fileName);
  }

  if (conflict) {
    if (ambiguous) *ambiguous = true;
    xdtsFileNames.clear();
    return {};
  }
  return ret;
}

QString WorkflowManager::logFilePath(QString sequence, QString sceneNumber) {
  QString fileName = m_cutNumberPrefix + sceneNumber + ".md";
  if (logRootPath().isEmpty()) return {};
  return logRootPathForCurrentEpisode() +
         (sequence.isEmpty() ? QString() : "/" + sequence) + "/" + fileName;
}

QString WorkflowManager::googleCueFilePath(QString googleCueFileName) {
  const QString logRoot = logRootPath();
  if (logRoot.isEmpty()) return {};
  QDir(logRoot).mkpath("google_spreadsheet_cues");
  return logRoot + "/google_spreadsheet_cues/" + googleCueFileName +
         ".ini";
}

QStringList WorkflowManager::cutListInFlow(FlowGroupId id) {
  return m_cutListInFlow[id];
}

QStringList WorkflowManager::cutListInBranchFlow(FlowGroupId id) {
  return m_cutListInBranchFlow[id];
}

QList<FlowGroupId> WorkflowManager::flowGroupsContainingCuts(
    const QStringList& cutNames) {
  QList<FlowGroupId> ret;
  for (auto fgId : m_flowGroupInfos.keys()) {
    const QStringList& cutList = m_cutListInFlow[fgId];
    for (auto& cutName : cutNames) {
      if (cutList.contains(cutName)) {
        ret.append(fgId);
        break;
      }
    }
  }
  return ret;
}

void WorkflowManager::registerCutNames(QStringList cutNames, FlowGroupId fgId,
                                       QString actionName) {
  if (infoFolderPath().isEmpty()) return;
  QDir prjInfoFolder(progressFolderPath());
  if (!prjInfoFolder.exists()) prjInfoFolder.mkpath(".");

  bool isRetake = m_flowGroupInfos.value(fgId).isRetakeGroup;

  for (auto tmpFgId : m_flowGroupInfos.keys()) {
    const FlowGroupInfo& tmpInfo = m_flowGroupInfos.value(tmpFgId);
    // 通常フロー同士、またはリテイクフロー同士（リテイクの起点グループを含む）でのみ、
    // カットの登録先を切り替える
    bool isSibling =
        isRetake ? (tmpInfo.isRetakeGroup || tmpInfo.isRetakeSourceGroup)
                 : !tmpInfo.isRetakeGroup;
    if (!isSibling) continue;

    QString cutListSettingsName = tmpInfo.idName + ".ini";
    QSettings cutListSettings(
        prjInfoFolder.absoluteFilePath(cutListSettingsName),
        QSettings::IniFormat);

    m_cutListInFlow[tmpFgId] = cutListSettings.value("cutList").toStringList();

    bool somethingChanged = false;
    // add
    if (fgId == tmpFgId) {
      for (auto cutName : cutNames) {
        if (!m_cutListInFlow[tmpFgId].contains(cutName)) {
          m_cutListInFlow[tmpFgId].append(cutName);
          somethingChanged = true;

          if (!actionName.isEmpty()) {
            QString sequence = sceneNum2Sequence(cutName);
            if (isValidSceneName(cutName))
              logAction(sequence, cutName, tr("Switch Workflow"), actionName);
          }
        }
      }
      m_cutListInFlow[tmpFgId].sort(Qt::CaseInsensitive);
    } else {  // remove
      for (auto cutName : cutNames) {
        somethingChanged |= m_cutListInFlow[tmpFgId].removeAll(cutName) > 0;
      }
    }
    if (somethingChanged)
      cutListSettings.setValue("cutList", m_cutListInFlow[tmpFgId]);
  }
}

void WorkflowManager::registerCutNameToBranchFlowGroup(QStringList cutNames,
                                                       FlowGroupId fgId,
                                                       bool isAdd,
                                                       QString actionName) {
  if (infoFolderPath().isEmpty()) return;
  QDir prjInfoFolder(progressFolderPath());
  if (!prjInfoFolder.exists()) prjInfoFolder.mkpath(".");

  QString cutListSettingsName = m_flowGroupInfos[fgId].idName + ".ini";
  QSettings cutListSettings(prjInfoFolder.absoluteFilePath(cutListSettingsName),
                            QSettings::IniFormat);
  m_cutListInBranchFlow[fgId] =
      cutListSettings.value("cutList_branch").toStringList();

  bool somethingChanged = false;

  if (isAdd) {
    for (auto cutName : cutNames) {
      if (!m_cutListInBranchFlow[fgId].contains(cutName)) {
        m_cutListInBranchFlow[fgId].append(cutName);
        somethingChanged = true;

        if (!actionName.isEmpty()) {
          QString sequence = sceneNum2Sequence(cutName);
          if (isValidSceneName(cutName))
            logAction(sequence, cutName, tr("Add Branch Workflow"), actionName);
        }
      }
    }
    m_cutListInBranchFlow[fgId].sort(Qt::CaseInsensitive);
  } else {  // remove
    for (auto cutName : cutNames) {
      somethingChanged |= m_cutListInBranchFlow[fgId].removeAll(cutName) > 0;
    }
  }
  if (somethingChanged)
    cutListSettings.setValue("cutList_branch", m_cutListInBranchFlow[fgId]);
}

void WorkflowManager::switchCurrentProject(QString prjName) {
  m_currentProjectName = prjName;

  loadProject(m_currentProjectName);

  // カットリスト読み込み
  loadCutList();

  // iniファイルに保存
  UserSettings::instance()->saveCurrentProjectName(m_currentProjectName);
}

void WorkflowManager::loadCutList() {
  // カットリスト読み込み
  m_cutListInFlow.clear();
  m_cutListInBranchFlow.clear();
  if (infoFolderPath().isEmpty()) return;
  QDir prjInfoFolder(progressFolderPath());
  if (!prjInfoFolder.exists()) prjInfoFolder.mkpath(".");
  for (auto fgId : m_flowGroupInfos.keys()) {
    QString cutListSettingsName = m_flowGroupInfos[fgId].idName + ".ini";
    QSettings cutListSettings(
        prjInfoFolder.absoluteFilePath(cutListSettingsName),
        QSettings::IniFormat);
    m_cutListInFlow[fgId] = cutListSettings.value("cutList").toStringList();
    m_cutListInFlow[fgId].sort(Qt::CaseInsensitive);
    m_cutListInBranchFlow[fgId] =
        cutListSettings.value("cutList_branch").toStringList();
    m_cutListInBranchFlow[fgId].sort(Qt::CaseInsensitive);
  }
}

QString WorkflowManager::decodeConditionPathForToolTip(QString path) {
  for (auto macroInfo : m_pathMacros) {
    if (path.contains(macroInfo.macroStr)) {
      if (!macroInfo.labelName.isEmpty())
        path.replace(macroInfo.macroStr, macroInfo.labelName);
      break;
    }
  }

  path.replace("/", "/");

  path.replace("[PREFIX]", m_cutNumberPrefix);
  path.replace("[USER_NAME]", UserSettings::instance()->userName());
  path.replace("[EPISODE]",
               m_currentEpisode.isEmpty() ? tr("EPISODE") : m_currentEpisode);
  path.replace("[SEQUENCE]", m_useSequence ? "00" : "");
  if (!m_useSequence) path = collapseSeparators(path);
  path.replace("[CUTNUMBER]", "0000");
  path.replace("[R]", "");  // Rewindは空にする

  return path;
}

//-----------------------------------------------------------------------------
// テイク管理
//-----------------------------------------------------------------------------

// 現在のテイク番号を返す
int WorkflowManager::currentTake(const QString sceneNumber) const {
  if (infoFolderPath().isEmpty()) return -1;
  QDir dir(sceneInfoFolderPath());

  QString sequence = sceneNum2Sequence(sceneNumber);
  if (!isValidSceneName(sceneNumber)) return -1;
  if (!enterSequenceFolder(dir, sequence, false)) return -1;

  QString fileName = m_cutNumberPrefix + sceneNumber + ".ini";  // blu0001.ini

  if (!dir.exists(fileName)) return 1;

  QSettings settings(dir.absoluteFilePath(fileName), QSettings::IniFormat);
  if (settings.childGroups().contains("RetakeInfo")) {
    settings.beginGroup("RetakeInfo");
    return settings.value("CurrentTake", 1).toInt();
  }
  // リテイク設定がなければテイク1
  return 1;
}

QString WorkflowManager::currentTakeSuffix(const QString sceneNumber,
                                           const bool r_suffix) const {
  int take = currentTake(sceneNumber);
  if (take <= 1) return QString();

  return (r_suffix) ? takeNum2RSuffix(take) : takeNum2Suffix(take);
}

QString WorkflowManager::previousTakeSuffix(const QString sceneNumber,
                                            const bool r_suffix) const {
  int take = currentTake(sceneNumber);
  if (take <= 1) return QString();

  return (r_suffix) ? takeNum2RSuffix(take - 1) : takeNum2Suffix(take - 1);
}

QString WorkflowManager::currentRewindSuffix(const QString sceneNumber) const {
  QString sequence = sceneNum2Sequence(sceneNumber);
  if (!isValidSceneName(sceneNumber)) return QString();

  QString rewindInfo =
      sceneInfoTag(sequence, sceneNumber, "REWIND", "").toString();

  if (rewindInfo == "WIP" || rewindInfo == "BackOnTrack")
    return RewindStr;
  else
    return QString();
}

// リテイク
bool WorkflowManager::doRetake(const QString sceneNumber, const int takeNum,
                               const FlowGroupId passToFgId,
                               const QString comment) {
  QString sequence = sceneNum2Sequence(sceneNumber);
  if (!isValidSceneName(sceneNumber)) return false;
  if (infoFolderPath().isEmpty()) return false;
  QDir dir(sceneInfoFolderPath() +
           (sequence.isEmpty() ? QString() : "/" + sequence));
  if (!dir.exists())
    if (!dir.mkpath(".")) return false;

  QString fileName = m_cutNumberPrefix + sceneNumber + ".ini";  // blu0001.ini

  QSettings settings(dir.absoluteFilePath(fileName), QSettings::IniFormat);
  settings.beginGroup("RetakeInfo");
  settings.setValue("CurrentTake", takeNum);
  settings.beginGroup(
      QString("t%1").arg(QString::number(takeNum).rightJustified(2, '0')));
  {
    settings.setValue("DateTime", QDateTime::currentDateTime().toString());
    settings.setValue("PassedTo", m_flowGroupInfos.value(passToFgId).idName);
    settings.setValue("Comment", comment);
  }
  settings.endGroup();
  settings.endGroup();
  return true;
}

// カットフォルダ作成
QString WorkflowManager::createCutFolder(const QString sceneNumber,
                                         const int takeNum,
                                         const FlowGroupId passToFgId) {
  QString sequence = sceneNum2Sequence(sceneNumber);
  QString path     = cutFolderPath(passToFgId, sequence, sceneNumber, takeNum);
  std::cout << "create path : " << path.toStdString() << std::endl;
  if (!path.isEmpty() && QDir(path).mkpath("."))
    return path;
  else
    return QString();
}

QVariantMap WorkflowManager::obtainRetakeInfo(const QString sceneNumber,
                                              int takeNum) const {
  QVariantMap ret;

  if (infoFolderPath().isEmpty()) return {};
  QDir dir(sceneInfoFolderPath());

  QString sequence = sceneNum2Sequence(sceneNumber);
  if (!isValidSceneName(sceneNumber)) return ret;
  if (!enterSequenceFolder(dir, sequence, false)) return ret;

  QString fileName = m_cutNumberPrefix + sceneNumber + ".ini";  // blu0001.ini

  if (!dir.exists(fileName)) return ret;

  QSettings settings(dir.absoluteFilePath(fileName), QSettings::IniFormat);
  settings.beginGroup("RetakeInfo");

  if (takeNum < 0) takeNum = settings.value("CurrentTake").toInt();

  QString takeGroupName =
      QString("t%1").arg(QString::number(takeNum).rightJustified(2, '0'));

  if (!settings.childGroups().contains(takeGroupName)) return ret;
  settings.beginGroup(takeGroupName);
  for (auto key : settings.childKeys()) {
    ret.insert(key, settings.value(key));
  }
  settings.endGroup();
  settings.endGroup();

  return ret;
}

// 詳細進捗リスト管理
void WorkflowManager::getProgressSettingsFilePath(const FlowGroupId fgId,
                                                  QString& settingsPath,
                                                  QString& lockPath) {
  if (infoFolderPath().isEmpty()) return;
  QDir prjInfoFolder(progressCacheFolderPath());
  QString progressSettingsName = m_flowGroupInfos[fgId].idName + "_details.ini";
  QString lockFileName = m_flowGroupInfos[fgId].idName + "_details.lock";
  prjInfoFolder.mkpath(".");
  settingsPath = prjInfoFolder.absoluteFilePath(progressSettingsName);
  lockPath     = prjInfoFolder.absoluteFilePath(lockFileName);
}

void WorkflowManager::saveProgressList(
    const FlowGroupId fgId,
    const QMap<WorkflowType, QStringList> progressList) {
  QString settingsPath, lockPath;
  getProgressSettingsFilePath(fgId, settingsPath, lockPath);
  if (settingsPath.isEmpty()) return;

  QLockFile lockFile(lockPath);
  lockFile.lock();

  QSettings progressSettings(settingsPath, QSettings::IniFormat);

  for (auto type : progressList.keys()) {
    progressSettings.beginGroup(QString::number(type));

    QString wfTypeLabel;
    for (auto wf : m_workflows) {
      if (wf.workflowType == type) {
        wfTypeLabel = wf.name;
        break;
      }
    }
    progressSettings.setValue("name", wfTypeLabel);

    progressSettings.setValue("cutList", progressList.value(type));

    progressSettings.endGroup();
  }

  lockFile.unlock();
}

FlowGroupId WorkflowManager::WfType2FgId(const WorkflowType wfType) {
  for (auto wf : m_workflows) {
    if (wf.workflowType == wfType) return wf.groupId;
  }
  return Group_None;
}

void WorkflowManager::getProgressList(
    const FlowGroupId fgId, QMap<WorkflowType, QStringList>& progressList) {
  QString settingsPath, lockPath;
  getProgressSettingsFilePath(fgId, settingsPath, lockPath);
  if (settingsPath.isEmpty()) return;

  QLockFile lockFile(lockPath);
  lockFile.lock();
  QSettings progressSettings(settingsPath, QSettings::IniFormat);
  for (auto wfType : progressList.keys()) {
    progressSettings.beginGroup(QString::number(wfType));
    progressList.insert(wfType,
                        progressSettings.value("cutList").toStringList());
    progressSettings.endGroup();
  }

  lockFile.unlock();
}

bool WorkflowManager::isInProgress(const QString sceneNumber,
                                   const WorkflowType wfType) {
  FlowGroupId fgId = WfType2FgId(wfType);
  if (fgId == Group_None) return false;
  QMap<WorkflowType, QStringList> progressList;
  progressList.insert(wfType, {});
  getProgressList(fgId, progressList);
  return progressList.value(wfType).contains(sceneNumber);
}

void WorkflowManager::setProgress(const QString sceneNumber,
                                  const WorkflowType wfType, const bool doAdd) {
  FlowGroupId fgId = WfType2FgId(wfType);
  QString settingsPath, lockPath;
  getProgressSettingsFilePath(fgId, settingsPath, lockPath);
  if (settingsPath.isEmpty()) return;

  QLockFile lockFile(lockPath);
  lockFile.lock();

  QSettings progressSettings(settingsPath, QSettings::IniFormat);

  progressSettings.beginGroup(QString::number(wfType));

  QStringList cutList = progressSettings.value("cutList").toStringList();
  if (doAdd) {
    assert(cutList.indexOf(sceneNumber) == -1);
    if (!cutList.contains(sceneNumber)) {
      cutList.append(sceneNumber);
      cutList.sort();
    }
  } else {  // remove
    assert(cutList.indexOf(sceneNumber) != -1);
    cutList.removeAll(sceneNumber);
  }
  progressSettings.setValue("cutList", cutList);

  progressSettings.endGroup();

  lockFile.unlock();
}

// workflowデータを書き出す
bool WorkflowManager::saveProject(const QString& prjName,
                                  QString* errorString) {
  QString prjPath = UserSettings::instance()->stuffFolderPath() + "/projects/" +
                    prjName + ".ppmprj";
  QFile saveFile(prjPath);

  if (!saveFile.open(QIODevice::WriteOnly)) {
    qWarning("Couldn't open save file.");
    if (errorString)
      *errorString = QString("%1\n(%2)").arg(QDir::toNativeSeparators(prjPath),
                                             saveFile.errorString());
    return false;
  }

  QJsonObject wfObject;

  wfObject["title"]           = m_projectTitle;
  wfObject["description"]     = m_projectDescription;
  wfObject["projectRootPath"]       = m_projectRootPath;
  wfObject["logRootPath"]           = m_logRootPath;
  wfObject["projectInfoFolderPath"] = m_projectInfoFolderPath;
  wfObject["cutNumberPrefix"] = m_cutNumberPrefix;
  wfObject["useSequence"]     = m_useSequence;
  // GASのURLは、スプレッドシートへの書き込みやChatへの投稿ができる入口のため、難読化して保存する
  wfObject["gasUrl"]           = encryptPath(m_gasUrl);
  wfObject["gasUrlObfuscated"] = true;

  // Google Chatの投稿先の名前（Webhook URLはPPMには保存しない）
  wfObject["googleChatDestinations"] =
      QJsonArray::fromStringList(m_chatDestinations);
  wfObject["sceneThumbnailPath"] = m_sceneThumbnailPath;
  wfObject["hasEpisodes"]        = m_hasEpisodes;
  wfObject["episodeNames"]       = QJsonArray::fromStringList(m_episodeNames);

  QJsonArray wfArray;
  for (auto wf : m_workflows) {
    wfArray.append(wf.toJson());
  }
  wfObject["workflows"] = wfArray;

  // path macros
  QJsonArray pathMacroArray;
  for (auto macroInfo : m_pathMacros) {
    QJsonObject macroInfoObj;
    macroInfoObj["macroStr"] = macroInfo.macroStr;
    if (macroInfo.doObfuscate) {  // 難読化して保存
      macroInfoObj["path"]       = encryptPath(macroInfo.path);
      macroInfoObj["obfuscated"] = true;
    } else
      macroInfoObj["path"] = macroInfo.path;
    if (!macroInfo.labelName.isEmpty())
      macroInfoObj["labelName"] = macroInfo.labelName;
    pathMacroArray.append(macroInfoObj);
  }
  wfObject["pathMacros"] = pathMacroArray;

  // flow group info
  QJsonArray flowGroupInfoArray;
  for (auto flowGroupId : m_flowGroupInfos.keys()) {
    FlowGroupInfo fgInfo = m_flowGroupInfos.value(flowGroupId);
    QJsonObject flowGroupInfoObj;
    flowGroupInfoObj["flowGroupId"]            = (double)flowGroupId;
    flowGroupInfoObj["idName"]                 = fgInfo.idName;
    flowGroupInfoObj["labelName"]              = fgInfo.labelName;
    flowGroupInfoObj["standardCutFolderMacro"] = fgInfo.standardCutFolderMacro;
    flowGroupInfoObj["xdtsFileName"]           = fgInfo.xdtsFileName;
    flowGroupInfoObj["takeNotationStyle"]      = (int)fgInfo.takeNotationStyle;
    flowGroupInfoObj["isRetakeGroup"]          = fgInfo.isRetakeGroup;
    flowGroupInfoObj["isRetakeSourceGroup"]    = fgInfo.isRetakeSourceGroup;
    flowGroupInfoObj["subfolderForOpenButton"] = fgInfo.subfolderForOpenButton;
    flowGroupInfoArray.append(flowGroupInfoObj);
  }
  wfObject["flowGroupInfo"] = flowGroupInfoArray;

  // scene info tags
  QJsonArray sceneInfoTagsArray;
  for (auto sceneInfoTag : m_sceneInfoTags) {
    QJsonObject sceneInfoTagObj = sceneInfoTag->toJson();
    sceneInfoTagObj["type"] = SceneInfoTagTypeTxts.value(sceneInfoTag->type());
    sceneInfoTagObj["idName"]         = sceneInfoTag->idName();
    sceneInfoTagObj["canManuallySet"] = sceneInfoTag->canManuallySet();
    sceneInfoTagsArray.append(sceneInfoTagObj);
  }
  wfObject["sceneInfoTags"] = sceneInfoTagsArray;

  // フローチャートUIのノード配置情報
  if (!m_flowChartLayout.isEmpty()) {
    QJsonObject layoutObj;
    QJsonArray groupNodeArray;
    for (auto& groupNode : m_flowChartLayout.groupNodes) {
      QJsonObject nodeObj;
      nodeObj["flowGroupId"] = (double)groupNode.groupId;
      nodeObj["x"]           = groupNode.pos.x();
      nodeObj["y"]           = groupNode.pos.y();
      nodeObj["width"]       = groupNode.size.width();
      nodeObj["height"]      = groupNode.size.height();
      groupNodeArray.append(nodeObj);
    }
    layoutObj["groupNodes"] = groupNodeArray;
    QJsonArray wfNodeArray;
    for (auto& wfNode : m_flowChartLayout.workflowNodes) {
      QJsonObject nodeObj;
      nodeObj["workflowTypeId"] = (double)wfNode.workflowType;
      nodeObj["x"]              = wfNode.pos.x();
      nodeObj["y"]              = wfNode.pos.y();
      nodeObj["expanded"]       = wfNode.expanded;
      wfNodeArray.append(nodeObj);
    }
    layoutObj["workflowNodes"]  = wfNodeArray;
    wfObject["flowChartLayout"] = layoutObj;
  }

  // 役職（フローチャートUIの役職パレット）
  QJsonArray rolesArray;
  for (auto& role : m_roles) {
    QJsonObject roleObj;
    roleObj["id"]        = (double)role.id;
    roleObj["labelName"] = role.labelName;
    roleObj["suffix"]    = role.suffix;
    roleObj["color"]     = role.color.name();
    rolesArray.append(roleObj);
  }
  wfObject["roles"] = rolesArray;

  QByteArray data = QJsonDocument(wfObject).toJson();
  // 書き込みの途中で失敗した場合（ディスクの空き不足、ネットワークの切断など）も失敗として扱う
  bool ok = (saveFile.write(data) == data.size()) && saveFile.flush();
  if (!ok && errorString)
    *errorString = QString("%1\n(%2)").arg(QDir::toNativeSeparators(prjPath),
                                           saveFile.errorString());
  saveFile.close();
  if (ok) {
    m_loadedProjectName = prjName;
    m_loadedProjectHash =
        QCryptographicHash::hash(data, QCryptographicHash::Sha256);
  }
  return ok;
}

bool WorkflowManager::projectFileChangedSinceLoaded(
    const QString& prjName) const {
  QString prjPath = UserSettings::instance()->stuffFolderPath() + "/projects/" +
                    prjName + ".ppmprj";
  QFile file(prjPath);
  if (!file.exists()) return false;
  // 読み込んでいない別のプロジェクトのファイル
  if (prjName != m_loadedProjectName) return true;
  // 読めない場合は、保存の処理でエラーとして扱う
  if (!file.open(QIODevice::ReadOnly)) return false;
  return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256) !=
         m_loadedProjectHash;
}

bool WorkflowManager::loadProject(const QString& prjName) {
  QString prjPath = UserSettings::instance()->stuffFolderPath() + "/projects/" +
                    prjName + ".ppmprj";

  QFile loadFile(prjPath);
  if (!loadFile.open(QIODevice::ReadOnly)) {
    qWarning("Couldn't open save file.");
    return false;
  }

  QByteArray loadedData = loadFile.readAll();
  m_loadedProjectName   = prjName;
  m_loadedProjectHash =
      QCryptographicHash::hash(loadedData, QCryptographicHash::Sha256);

  QJsonDocument loadDoc(QJsonDocument::fromJson(loadedData));
  QJsonObject wfObject = loadDoc.object();

  if (const QJsonValue v = wfObject["title"]; v.isString())
    m_projectTitle = v.toString();
  // 概要フィールド：キーが無いファイル（旧形式）を読み込んだ際に
  // 前に開いていたプロジェクトの値を引き継がないよう、既定値を明示的にリセットしてから読む
  m_projectDescription.clear();
  if (const QJsonValue v = wfObject["description"]; v.isString())
    m_projectDescription = v.toString();
  if (const QJsonValue v = wfObject["projectRootPath"]; v.isString())
    m_projectRootPath = v.toString();
  if (const QJsonValue v = wfObject["logRootPath"]; v.isString())
    m_logRootPath = v.toString();
  if (const QJsonValue v = wfObject["projectInfoFolderPath"]; v.isString())
    m_projectInfoFolderPath = v.toString();
  if (const QJsonValue v = wfObject["cutNumberPrefix"]; v.isString())
    m_cutNumberPrefix = v.toString();
  // キーが無い（旧形式の）プロジェクトは、シークエンスを使う（ON）
  m_useSequence = true;
  if (const QJsonValue v = wfObject["useSequence"]; v.isBool())
    m_useSequence = v.toBool();
  if (const QJsonValue v = wfObject["gasUrl"]; v.isString()) {
    m_gasUrl = v.toString();
    // 難読化して保存されたものは元に戻す（難読化されていない旧形式のプロジェクトは、そのまま読み込む）
    if (wfObject["gasUrlObfuscated"].toBool()) m_gasUrl = decryptPath(m_gasUrl);
  }

  // Google Chatの投稿先：キーが無いファイル（旧形式）を読み込んだ際に
  // 前のプロジェクトの値を引き継がないよう、いったん空にしてから読む
  m_chatDestinations.clear();
  if (const QJsonValue v = wfObject["googleChatDestinations"]; v.isArray()) {
    for (const QJsonValue& destVal : v.toArray()) {
      // 名前の文字列。リリース前の形式（名前とWebhook
      // URLのオブジェクト）で保存されたファイルからは、
      // 名前だけを取り出し、URLは捨てる
      QString name = destVal.isObject()
                         ? destVal.toObject().value("name").toString()
                         : destVal.toString();
      if (!name.isEmpty() && !m_chatDestinations.contains(name))
        m_chatDestinations.append(name);
    }
  }

  // キーが無いファイル（旧形式）を読み込んだ際に前のプロジェクトの値を引き継がないよう、
  // 既定値へ明示的にリセットしてから読む
  m_sceneThumbnailPath.clear();
  if (const QJsonValue v = wfObject["sceneThumbnailPath"]; v.isString())
    m_sceneThumbnailPath = v.toString();

  // 話数機能：キーが無いファイル（旧形式や話数を使わないプロジェクト）を読み込んだ際に
  // 前に開いていたプロジェクトの値を引き継がないよう、既定値へ明示的にリセットしてから読む
  m_hasEpisodes = false;
  m_episodeNames.clear();
  m_currentEpisode.clear();
  if (const QJsonValue v = wfObject["hasEpisodes"]; v.isBool())
    m_hasEpisodes = v.toBool();
  if (const QJsonValue v = wfObject["episodeNames"]; v.isArray()) {
    for (const QJsonValue& nameVal : v.toArray())
      if (nameVal.isString()) m_episodeNames.append(nameVal.toString());
  }

  // flow group info
  // ※
  // workflowsのgroupId解決（idName→FlowGroupId）で参照するため、workflowsより先にパースする
  if (const QJsonValue v = wfObject["flowGroupInfo"]; v.isArray()) {
    m_flowGroupInfos.clear();
    const QJsonArray flowGroupInfoArray = v.toArray();
    for (const QJsonValue& flowGroupInfoVal : flowGroupInfoArray) {
      QJsonObject flowGroupInfoObj = flowGroupInfoVal.toObject();
      FlowGroupId fgId             = Group_None;
      FlowGroupInfo info;
      if (const QJsonValue v = flowGroupInfoObj["flowGroupId"]; v.isDouble())
        fgId = static_cast<FlowGroupId>(v.toDouble());
      if (const QJsonValue v = flowGroupInfoObj["idName"]; v.isString())
        info.idName = v.toString();
      if (const QJsonValue v = flowGroupInfoObj["labelName"]; v.isString())
        info.labelName = v.toString();
      if (const QJsonValue v = flowGroupInfoObj["standardCutFolderMacro"];
          v.isString())
        info.standardCutFolderMacro = v.toString();
      if (const QJsonValue v = flowGroupInfoObj["xdtsFileName"]; v.isString())
        info.xdtsFileName = v.toString();
      if (const QJsonValue v = flowGroupInfoObj["takeNotationStyle"];
          v.isDouble())
        info.takeNotationStyle = static_cast<FlowGroupInfo::TakeNotationStyle>(
            static_cast<int>(v.toDouble()));
      if (const QJsonValue v = flowGroupInfoObj["isRetakeGroup"]; v.isBool())
        info.isRetakeGroup = v.toBool();
      if (const QJsonValue v = flowGroupInfoObj["isRetakeSourceGroup"];
          v.isBool())
        info.isRetakeSourceGroup = v.toBool();
      if (const QJsonValue v = flowGroupInfoObj["subfolderForOpenButton"];
          v.isString())
        info.subfolderForOpenButton = v.toString();

      m_flowGroupInfos.insert(fgId, info);
    }
  }

  if (const QJsonValue v = wfObject["workflows"]; v.isArray()) {
    m_workflows.clear();
    const QJsonArray wfArray = v.toArray();
    for (const QJsonValue& wfVal : wfArray)
      m_workflows.append(Workflow::fromJson(wfVal.toObject()));
  }

  if (const QJsonValue v = wfObject["pathMacros"]; v.isArray()) {
    m_pathMacros.clear();
    const QJsonArray pathMacroArray = v.toArray();
    for (const QJsonValue& pathMacroVal : pathMacroArray) {
      QJsonObject macroInfoObj = pathMacroVal.toObject();
      PathMacroInfo macroInfo;
      if (const QJsonValue v = macroInfoObj["macroStr"]; v.isString())
        macroInfo.macroStr = v.toString();
      if (const QJsonValue v = macroInfoObj["path"]; v.isString())
        macroInfo.path = v.toString();
      if (const QJsonValue v = macroInfoObj["labelName"]; v.isString())
        macroInfo.labelName = v.toString();
      if (const QJsonValue v = macroInfoObj["obfuscated"]; v.isBool())
        macroInfo.doObfuscate = v.toBool();
      // 難読化マクロを元に戻す
      if (macroInfo.doObfuscate) macroInfo.path = decryptPath(macroInfo.path);
      m_pathMacros.append(macroInfo);
    }
  }

  // scene info tags
  if (const QJsonValue v = wfObject["sceneInfoTags"]; v.isArray()) {
    deleteAllSceneInfoTags();
    const QJsonArray sceneInfoTagsArray = v.toArray();
    for (const QJsonValue& sceneInfoTagVal : sceneInfoTagsArray) {
      // タイプ毎に生成
      QJsonObject sceneInfoTagObj = sceneInfoTagVal.toObject();
      // enumでなくタイプ名の文字列
      SceneInfoTagType type =
          SceneInfoTagTypeTxts.key(sceneInfoTagObj["type"].toString());
      SceneInfoTag* tag;
      switch (type) {
      case InfoTagType_Bool:
        tag = BoolSceneInfoTag::fromJson(sceneInfoTagObj);
        break;
      case InfoTagType_Enum:
        tag = EnumSceneInfoTag::fromJson(sceneInfoTagObj);
        break;
      case InfoTagType_String:
        tag = StringSceneInfoTag::fromJson(sceneInfoTagObj);
        break;
      case InfoTagType_StringList:
        tag = StringListSceneInfoTag::fromJson(sceneInfoTagObj);
        break;
      }
      m_sceneInfoTags.append(tag);
    }
  }

  // フローチャートUIのノード配置情報：キーが無いファイル（旧形式）を読み込んだ際に
  // 前に開いていたプロジェクトの値を引き継がないよう、明示的にリセットしてから読む
  m_flowChartLayout = FlowChartLayout();
  if (const QJsonValue v = wfObject["flowChartLayout"]; v.isObject()) {
    QJsonObject layoutObj = v.toObject();
    if (const QJsonValue gv = layoutObj["groupNodes"]; gv.isArray()) {
      for (const QJsonValue& nodeVal : gv.toArray()) {
        QJsonObject nodeObj = nodeVal.toObject();
        FlowChartGroupNode groupNode;
        if (const QJsonValue v = nodeObj["flowGroupId"]; v.isDouble())
          groupNode.groupId = static_cast<FlowGroupId>(v.toDouble());
        groupNode.pos =
            QPointF(nodeObj["x"].toDouble(), nodeObj["y"].toDouble());
        groupNode.size =
            QSizeF(nodeObj["width"].toDouble(), nodeObj["height"].toDouble());
        m_flowChartLayout.groupNodes.append(groupNode);
      }
    }
    if (const QJsonValue wv = layoutObj["workflowNodes"]; wv.isArray()) {
      for (const QJsonValue& nodeVal : wv.toArray()) {
        QJsonObject nodeObj = nodeVal.toObject();
        FlowChartWorkflowNode wfNode;
        if (const QJsonValue v = nodeObj["workflowTypeId"]; v.isDouble())
          wfNode.workflowType = static_cast<WorkflowType>(v.toDouble());
        wfNode.pos = QPointF(nodeObj["x"].toDouble(), nodeObj["y"].toDouble());
        wfNode.expanded = nodeObj["expanded"].toBool();
        m_flowChartLayout.workflowNodes.append(wfNode);
      }
    }
  }

  // 役職（フローチャートUIの役職パレット）
  m_roles.clear();
  if (const QJsonValue v = wfObject["roles"]; v.isArray()) {
    for (const QJsonValue& roleVal : v.toArray()) {
      QJsonObject roleObj = roleVal.toObject();
      Role role;
      if (const QJsonValue v = roleObj["id"]; v.isDouble())
        role.id = static_cast<unsigned int>(v.toDouble());
      if (const QJsonValue v = roleObj["labelName"]; v.isString())
        role.labelName = v.toString();
      if (const QJsonValue v = roleObj["suffix"]; v.isString())
        role.suffix = v.toString();
      if (const QJsonValue v = roleObj["color"]; v.isString())
        role.color = QColor::fromString(v.toString());
      m_roles.append(role);
    }
  }

  return true;
}