#include "sharedfiles.h"
#include "registerscenedialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QFrame>
#include <QMessageBox>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QMimeData>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QFont>

namespace {
QString mediaTypeDisplayName(MediaType type) {
  switch (type) {
  case Digital:
    return QObject::tr("Digital");
  case Paper:
    return QObject::tr("Paper");
  case Other:
    return QObject::tr("Other");
  }
  return QString();
}

// conditionPath（未デコードのまま）の拡張子。マクロ文字列自体にドットは含まれないため、
// デコードしなくても末尾の拡張子はそのまま取得できる
QString conditionPathExtension(const QString& conditionPath) {
  return QFileInfo(conditionPath).suffix().toLower();
}
}  // namespace

//--------------------------------
// FileDropField
//--------------------------------

FileDropField::FileDropField(QWidget* parent) : QWidget(parent) {
  setAcceptDrops(true);
  // プレーンなQWidgetはデフォルトではスタイルシートの背景・枠線を自前で描画しないため、
  // WA_StyledBackgroundを立てて有効にする
  setAttribute(Qt::WA_StyledBackground, true);
  setStyleSheet(
      "FileDropField { border: 2px dashed gray; border-radius: 4px; }");

  m_pathLabel = new QLabel(tr("Drop a file here, or click to browse..."), this);
  m_pathLabel->setWordWrap(true);
  m_pathLabel->setAlignment(Qt::AlignCenter);

  QVBoxLayout* lay = new QVBoxLayout(this);
  lay->setContentsMargins(10, 10, 10, 10);
  lay->addWidget(m_pathLabel);

  setMinimumHeight(60);
}

void FileDropField::setPath(const QString& path) {
  m_path = path;
  m_pathLabel->setText(
      path.isEmpty() ? tr("Drop a file here, or click to browse...") : path);
  emit pathChanged();
}

void FileDropField::dragEnterEvent(QDragEnterEvent* event) {
  if (event->mimeData()->hasUrls() && !event->mimeData()->urls().isEmpty())
    event->acceptProposedAction();
}

void FileDropField::dropEvent(QDropEvent* event) {
  QList<QUrl> urls = event->mimeData()->urls();
  if (urls.isEmpty()) return;
  QString localPath = urls.first().toLocalFile();
  if (localPath.isEmpty()) return;
  setPath(localPath);
  event->acceptProposedAction();
}

void FileDropField::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    QString path =
        QFileDialog::getOpenFileName(this, tr("Select Source File"), m_path);
    if (!path.isEmpty()) setPath(path);
  }
  QWidget::mousePressEvent(event);
}

//--------------------------------
// RegisterSceneDialog
//--------------------------------

RegisterSceneDialog::RegisterSceneDialog(WorkflowType workflowType,
                                         QWidget* parent)
    : QDialog(parent), m_workflowType(workflowType) {
  Workflow wf = WorkflowManager::instance()->workflow(workflowType);
  FlowGroupInfo groupInfo =
      WorkflowManager::instance()->flowGroupInfos().value(wf.groupId);

  setWindowTitle(tr("Register a Scene"));

  QLabel* headerLabel = new QLabel(
      tr("Register a scene starting from:\nFlow Group: %1\nWorkflow: %2")
          .arg(groupInfo.labelName, wf.name),
      this);
  QFont headerFont = headerLabel->font();
  headerFont.setBold(true);
  headerLabel->setFont(headerFont);

  m_sceneNameEdit = new QLineEdit(this);
  m_sceneNameEdit->setPlaceholderText(tr("e.g. 0123b"));
  // カット登録ダイアログ（RegisterDialog）のカット名欄に合わせる
  m_sceneNameEdit->setStyleSheet("QLineEdit { font : bold 20px; }");

  m_copyMethodRadio =
      new QRadioButton(tr("Copy/rename an existing file"), this);
  m_emptyMethodRadio =
      new QRadioButton(tr("Create an empty file/folder"), this);
  m_copyMethodRadio->setChecked(true);
  m_methodButtonGroup = new QButtonGroup(this);
  m_methodButtonGroup->addButton(m_copyMethodRadio);
  m_methodButtonGroup->addButton(m_emptyMethodRadio);

  m_sourceFileField = new FileDropField(this);

  m_protocolCombo = new QComboBox(this);
  for (int i = 0; i < wf.protocols.size(); i++) {
    const WorkflowProtocol& protocol = wf.protocols.at(i);
    m_protocolCombo->addItem(
        tr("%1: %2").arg(mediaTypeDisplayName(protocol.mediaType),
                         protocol.conditionPath),
        i);
  }

  QFormLayout* formLay = new QFormLayout();
  formLay->addRow(tr("Scene Name"), m_sceneNameEdit);
  // 使えないカット名のときは、Registerボタンが無効になるため、理由はここに表示する
  m_sceneNameWarningLabel = new QLabel(this);
  m_sceneNameWarningLabel->setWordWrap(true);
  m_sceneNameWarningLabel->setStyleSheet("QLabel { color: #d33; }");
  m_sceneNameWarningLabel->hide();
  formLay->addRow(QString(), m_sceneNameWarningLabel);

  QVBoxLayout* methodLay = new QVBoxLayout();
  methodLay->setContentsMargins(0, 0, 0, 0);
  methodLay->addWidget(m_copyMethodRadio);
  methodLay->addWidget(m_sourceFileField);
  methodLay->addWidget(m_emptyMethodRadio);
  QGroupBox* methodBox = new QGroupBox(tr("Cue File Creation Method"), this);
  methodBox->setLayout(methodLay);

  formLay->addRow(tr("Protocol"), m_protocolCombo);

  m_registerButton          = new QPushButton(tr("Register"), this);
  QPushButton* cancelButton = new QPushButton(tr("Cancel"), this);
  m_registerButton->setDefault(true);

  QHBoxLayout* buttonLay = new QHBoxLayout();
  buttonLay->addStretch(1);
  buttonLay->addWidget(m_registerButton);
  buttonLay->addWidget(cancelButton);

  QVBoxLayout* mainLay = new QVBoxLayout(this);
  mainLay->setContentsMargins(15, 15, 15, 15);
  mainLay->setSpacing(12);
  mainLay->addWidget(headerLabel);
  mainLay->addLayout(formLay);
  mainLay->addWidget(methodBox);
  mainLay->addLayout(buttonLay);

  setMinimumWidth(420);

  connect(m_sceneNameEdit, &QLineEdit::textChanged, this,
          &RegisterSceneDialog::onInputEdited);
  connect(m_methodButtonGroup, &QButtonGroup::buttonClicked, this,
          &RegisterSceneDialog::onMethodChanged);
  connect(m_sourceFileField, &FileDropField::pathChanged, this,
          &RegisterSceneDialog::onSourceFileChanged);
  connect(m_protocolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &RegisterSceneDialog::onInputEdited);
  connect(m_registerButton, &QPushButton::clicked, this,
          &RegisterSceneDialog::onRegisterClicked);
  connect(cancelButton, &QPushButton::clicked, this,
          &RegisterSceneDialog::reject);

  onMethodChanged();
  updateRegisterButtonEnabled();
}

void RegisterSceneDialog::onMethodChanged() {
  bool copyMethod = m_copyMethodRadio->isChecked();
  m_sourceFileField->setVisible(copyMethod);
  updateProtocolComboFromExtension();
  updateRegisterButtonEnabled();
}

void RegisterSceneDialog::onSourceFileChanged() {
  updateProtocolComboFromExtension();
  updateRegisterButtonEnabled();
}

void RegisterSceneDialog::onInputEdited() { updateRegisterButtonEnabled(); }

void RegisterSceneDialog::updateProtocolComboFromExtension() {
  QString extension =
      m_copyMethodRadio->isChecked()
          ? QFileInfo(m_sourceFileField->path()).suffix().toLower()
          : QStringLiteral("txt");
  if (extension.isEmpty()) return;

  Workflow wf = WorkflowManager::instance()->workflow(m_workflowType);
  QList<int> matchingIndices;
  for (int i = 0; i < wf.protocols.size(); i++) {
    if (conditionPathExtension(wf.protocols.at(i).conditionPath) == extension)
      matchingIndices.append(i);
  }
  if (matchingIndices.isEmpty()) return;

  if (matchingIndices.size() > 1) {
    QMessageBox::information(this, tr("Multiple Protocols Match"),
                             tr("The file extension \"%1\" matches the "
                                "conditions for multiple protocols.")
                                 .arg(extension));
  }
  m_protocolCombo->setCurrentIndex(
      m_protocolCombo->findData(matchingIndices.first()));
}

QString RegisterSceneDialog::computeDestinationPath() const {
  QString cutName = m_sceneNameEdit->text().trimmed();
  if (cutName.isEmpty()) return QString();
  if (!WorkflowManager::instance()->isValidSceneName(cutName)) return QString();
  QString sequence = WorkflowManager::instance()->sceneNum2Sequence(cutName);

  int protocolIndex = m_protocolCombo->currentData().toInt();
  Workflow wf       = WorkflowManager::instance()->workflow(m_workflowType);
  if (protocolIndex < 0 || protocolIndex >= wf.protocols.size())
    return QString();

  return WorkflowManager::instance()->decodePath(
      wf.protocols.at(protocolIndex).conditionPath, sequence, cutName);
}

void RegisterSceneDialog::updateRegisterButtonEnabled() {
  const QString sceneName = m_sceneNameEdit->text().trimmed();
  const bool nameInvalid =
      !sceneName.isEmpty() &&
      !WorkflowManager::instance()->isValidSceneName(sceneName);
  m_sceneNameWarningLabel->setText(
      nameInvalid
          ? WorkflowManager::instance()->invalidSceneNameMessage({sceneName})
          : QString());
  m_sceneNameWarningLabel->setVisible(nameInvalid);
  bool ok = !m_sceneNameEdit->text().trimmed().isEmpty() &&
            m_protocolCombo->currentIndex() >= 0 &&
            !computeDestinationPath().isEmpty();
  if (m_copyMethodRadio->isChecked())
    ok = ok && !m_sourceFileField->path().isEmpty();
  m_registerButton->setEnabled(ok);
}

bool RegisterSceneDialog::findExistingRegistration(
    const QString& cutName, WorkflowType& outWfType) const {
  for (auto fgId : WorkflowManager::instance()->flowGroupIds()) {
    QMap<WorkflowType, QStringList> progressList;
    for (int i = 0; i < WorkflowManager::instance()->workflowCount(); i++) {
      Workflow wf = WorkflowManager::instance()->getWorkflow(i);
      if (wf.groupId == fgId) progressList.insert(wf.workflowType, {});
    }
    if (progressList.isEmpty()) continue;
    WorkflowManager::instance()->getProgressList(fgId, progressList);
    for (auto wfType : progressList.keys()) {
      if (progressList.value(wfType).contains(cutName)) {
        outWfType = wfType;
        return true;
      }
    }
  }
  return false;
}

void RegisterSceneDialog::onRegisterClicked() {
  QString cutName = m_sceneNameEdit->text().trimmed();
  // 使えないカット名のときは、Registerボタンが無効になっており、理由はm_sceneNameWarningLabelに表示される。念のため確認する
  if (!WorkflowManager::instance()->isValidSceneName(cutName)) {
    QMessageBox::warning(
        this, tr("Warning"),
        WorkflowManager::instance()->invalidSceneNameMessage({cutName}));
    return;
  }
  QString sequence = WorkflowManager::instance()->sceneNum2Sequence(cutName);

  int protocolIndex = m_protocolCombo->currentData().toInt();
  Workflow wf       = WorkflowManager::instance()->workflow(m_workflowType);
  if (protocolIndex < 0 || protocolIndex >= wf.protocols.size()) {
    QMessageBox::warning(this, tr("Warning"), tr("Please select a protocol."));
    return;
  }
  WorkflowProtocol protocol = wf.protocols.at(protocolIndex);

  auto* manager             = WorkflowManager::instance();
  const QString displayPath = computeDestinationPath();
  QString destPath          = SharedFiles::actualPath(
      manager->decodeObfuscatedPathMacros(displayPath, sequence, cutName));
  if (destPath.isEmpty()) {
    QMessageBox::warning(
        this, tr("Warning"),
        tr("Could not determine the destination cue file path."));
    return;
  }

  bool copyMethod    = m_copyMethodRadio->isChecked();
  QString sourcePath = SharedFiles::actualPath(m_sourceFileField->path());
  if (copyMethod) {
    if (sourcePath.isEmpty() || !QFileInfo::exists(sourcePath)) {
      QMessageBox::warning(this, tr("Warning"),
                           tr("Please specify an existing source file."));
      return;
    }
    if (SharedFiles::sameEntry(sourcePath, destPath)) {
      QMessageBox::warning(
          this, tr("Warning"),
          tr("The source file and the destination cue file are the same."));
      return;
    }
  }

  // 既に他の工程の詳細進捗リストに登録されていないかを探す
  WorkflowType existingWfType = Wf_Undefined;
  bool alreadyRegistered = findExistingRegistration(cutName, existingWfType);

  //--- 確認ダイアログ ---
  QDialog confirmDialog(this);
  confirmDialog.setWindowTitle(tr("Confirm Registration"));
  QVBoxLayout* confirmLay = new QVBoxLayout(&confirmDialog);

  FlowGroupInfo targetGroupInfo =
      WorkflowManager::instance()->flowGroupInfos().value(wf.groupId);
  QString summary = tr("Scene: %1\nFlow Group: %2\nWorkflow: %3\n")
                        .arg(cutName, targetGroupInfo.labelName, wf.name);
  if (copyMethod)
    summary += tr("\nThe cue file will be created by copying:\n  %1\n  -> %2")
                   .arg(sourcePath, displayPath);
  else {
    bool isFolder = QFileInfo(destPath).suffix().isEmpty();
    summary += tr("\nAn empty %1 will be created:\n  %2")
                   .arg(isFolder ? tr("folder") : tr("file"), displayPath);
  }
  // 同じ名前のキューファイルが既にある場合
  // コピーの場合は上書きする。空のファイル／フォルダの場合は既存のものを残す
  bool destExists = QFileInfo::exists(destPath);
  if (destExists) {
    if (copyMethod)
      summary += tr("\n\n[WARNING] The file %1 already exists. "
                    "It will be overwritten.")
                     .arg(displayPath);
    else
      summary +=
          tr("\n\nThe %1 %2 already exists. It will be kept as it is.")
              .arg(QFileInfo(destPath).isDir() ? tr("folder") : tr("file"),
                   displayPath);
  }
  QLabel* summaryLabel = new QLabel(summary, &confirmDialog);
  summaryLabel->setWordWrap(true);
  confirmLay->addWidget(summaryLabel);

  // 既に別の工程グループの工程に登録されている場合の選択肢
  QRadioButton* replaceRadio     = nullptr;
  QRadioButton* addAsBranchRadio = nullptr;  // 元の工程グループに残る
  QRadioButton* moveKeepOldRadio = nullptr;  // 登録先に移り、元を傍流に
  FlowGroupId existingGroupId    = Group_None;
  if (alreadyRegistered) {
    Workflow existingWf = WorkflowManager::instance()->workflow(existingWfType);
    existingGroupId     = existingWf.groupId;
    if (existingGroupId == wf.groupId) {
      // 同じ工程グループ内では、どの工程に並ぶかはキューファイルで決まるため、選択肢は出さない
      QLabel* alreadyLabel = new QLabel(
          tr("\nThis scene is already in workflow \"%1\" of the same "
             "workflow group.\n"
             "Which workflows it is listed in is decided by the cue files in "
             "the scene folder.")
              .arg(existingWf.name),
          &confirmDialog);
      alreadyLabel->setWordWrap(true);
      confirmLay->addWidget(alreadyLabel);
    } else {
      QString existingGroupName =
          WorkflowManager::instance()->flowGroupLabelName(existingGroupId);
      QLabel* alreadyLabel = new QLabel(
          tr("\nThis scene is already in workflow \"%1\" of the workflow "
             "group \"%2\".\n"
             "How do you want to register it?")
              .arg(existingWf.name, existingGroupName),
          &confirmDialog);
      alreadyLabel->setWordWrap(true);
      confirmLay->addWidget(alreadyLabel);

      replaceRadio = new QRadioButton(
          tr("Replace: Move to \"%1\", and remove it from \"%2\"")
              .arg(targetGroupInfo.labelName, existingGroupName),
          &confirmDialog);
      addAsBranchRadio = new QRadioButton(
          tr("Add: Stay in \"%2\", and add \"%1\" as a branch flow")
              .arg(targetGroupInfo.labelName, existingGroupName),
          &confirmDialog);
      moveKeepOldRadio = new QRadioButton(
          tr("Add: Move to \"%1\", and keep \"%2\" as a branch flow")
              .arg(targetGroupInfo.labelName, existingGroupName),
          &confirmDialog);
      replaceRadio->setChecked(true);
      QButtonGroup* replaceGroup = new QButtonGroup(&confirmDialog);
      replaceGroup->addButton(replaceRadio);
      replaceGroup->addButton(addAsBranchRadio);
      replaceGroup->addButton(moveKeepOldRadio);
      confirmLay->addWidget(replaceRadio);
      confirmLay->addWidget(addAsBranchRadio);
      confirmLay->addWidget(moveKeepOldRadio);
    }
  }

  QDialogButtonBox* buttonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &confirmDialog);
  connect(buttonBox, &QDialogButtonBox::accepted, &confirmDialog,
          &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, &confirmDialog,
          &QDialog::reject);
  confirmLay->addWidget(buttonBox);

  if (confirmDialog.exec() != QDialog::Accepted) return;

  bool doReplace     = replaceRadio && replaceRadio->isChecked();
  bool doAddAsBranch = addAsBranchRadio && addAsBranchRadio->isChecked();
  bool doMoveKeepOld = moveKeepOldRadio && moveKeepOldRadio->isChecked();

  // ① Replaceの場合、既存の登録を解除する
  // （元の工程グループに傍流として所属していた場合は、それも外す）
  if (doReplace) {
    WorkflowManager::instance()->setProgress(cutName, existingWfType, false);
    WorkflowManager::instance()->registerCutNameToBranchFlowGroup(
        {cutName}, existingGroupId, false);
  }

  // ② 工程グループへの登録／切り替え
  if (doAddAsBranch) {
    // 元の工程グループに所属したまま、登録先の工程グループを傍流として加える
    WorkflowManager::instance()->registerCutNameToBranchFlowGroup(
        {cutName}, wf.groupId, true, tr("Register Scene"));
  } else {
    // 登録先の工程グループに移る。登録先で傍流になっていた場合は、傍流を外す
    WorkflowManager::instance()->registerCutNameToBranchFlowGroup(
        {cutName}, wf.groupId, false);
    WorkflowManager::instance()->registerCutNames({cutName}, wf.groupId,
                                                  tr("Register Scene"));
    // 元の工程グループを傍流として残す
    if (doMoveKeepOld)
      WorkflowManager::instance()->registerCutNameToBranchFlowGroup(
          {cutName}, existingGroupId, true, tr("Register Scene"));
  }

  // ③
  // 工程の詳細進捗リストに登録し、ログに記録する（setProgress自体はログを取らないため）
  WorkflowManager::instance()->setProgress(cutName, m_workflowType, true);
  WorkflowManager::instance()->logAction(sequence, cutName, wf.name,
                                         tr("Register Scene"));

  // ⑤ キューファイルの作成／コピー
  QDir destDir = QFileInfo(destPath).dir();
  if (!destDir.exists() && !destDir.mkpath(".")) {
    QMessageBox::critical(this, tr("Error"),
                          tr("Failed to create the folder %1.")
                              .arg(manager->hideObfuscatedPaths(
                                  destDir.absolutePath(), sequence, cutName)));
    return;
  }

  bool ok;
  QString errorMsg;
  if (copyMethod) {
    if (QFileInfo::exists(destPath) && !QFile::remove(destPath)) {
      ok       = false;
      errorMsg = tr("Failed to remove the existing file %1.").arg(displayPath);
    } else {
      ok = QFile::copy(sourcePath, destPath);
      if (!ok) errorMsg = tr("Failed to copy the file to %1.").arg(displayPath);
    }
  } else if (destExists) {  // 既存のファイル／フォルダをそのまま使う
    ok = true;
  } else if (QFileInfo(destPath).suffix().isEmpty()) {  // フォルダ
    ok = QDir().mkpath(destPath);
    if (!ok) errorMsg = tr("Failed to create the folder %1.").arg(displayPath);
  } else {  // 空のファイル
    QFile file(destPath);
    ok = file.open(QIODevice::WriteOnly | QIODevice::Text);
    file.close();
    if (!ok) errorMsg = tr("Failed to create the file %1.").arg(displayPath);
  }

  m_didRegister       = true;
  m_registeredGroupId = wf.groupId;
  m_previousGroupId =
      alreadyRegistered
          ? WorkflowManager::instance()->WfType2FgId(existingWfType)
          : Group_None;

  if (ok)
    QMessageBox::information(
        this, tr("Success"),
        tr("The scene \"%1\" has been registered.").arg(cutName));
  else
    QMessageBox::critical(this, tr("Error"), errorMsg);

  accept();
}
