#include "projectsettingsdialog.h"
#include "projectsettingsdialog_flowchart.h"  // FlowchartPage
#include "projectsettings_common.h"
#include "workflow.h"
#include "googlechatgateway.h"
#include "actionconfirmationdialog.h"  // StepExecutor_NotifyGoogleChat::defaultMessageTemplate()

#include <QApplication>
#include <QTextLayout>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPointer>
#include <QDateTime>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QFileDialog>
#include <QTableWidget>
#include <QHeaderView>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QColorDialog>
#include <QSet>
#include <QCompleter>
#include <QStringListModel>
#include <QStandardItemModel>
#include <QDesktopServices>
#include <QUrl>
#include <QKeyEvent>
#include <QScrollArea>
#include <QStyledItemDelegate>
#include <QBrush>
#include <QAbstractItemView>
#include <QGroupBox>
#include <QMenu>
#include <QPainter>
#include <QIcon>
#include <QPixmap>
#include <QDropEvent>
#include <QTimer>
#include <QRegularExpression>
#include <QIntValidator>
#include <algorithm>
#include <functional>

// tmpSettings / markDirty / g_onDirtyChanged
// は、フローチャート型ダイアログと共有するため projectsettings_common.h/.cpp
// へ移動した

// Google
// Chatの投稿先の名前を変更したとき、Step_NotifyGoogleChatステップの参照先を付け替える
// （ActionStepの入れ子展開処理を使うため、実装はこのファイルの後半にある）
static void renameChatDestinationReferencesInTmpSettings(
    const QString& oldName, const QString& newName);

// 工程グループのIDを変更したとき、工程の所属（groupId）、
// 工程グループを指定するステップ、フローチャートの配置情報の参照先を付け替える
// （同上の理由で、実装はこのファイルの後半にある）
static void remapFlowGroupReferencesInTmpSettings(FlowGroupId oldId,
                                                  FlowGroupId newId);
// ids のいずれかの工程グループを指定しているステップの数を返す
static int countFlowGroupReferencesInTmpSettings(const QSet<FlowGroupId>& ids);

//--------------------------------
FolderPathField::FolderPathField(QWidget* parent) : QWidget(parent) {
  m_edit                     = new QLineEdit(this);
  QPushButton* browserButton = new QPushButton("...", this);

  browserButton->setFixedWidth(25);

  QHBoxLayout* lay = new QHBoxLayout();
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);
  {
    lay->addWidget(m_edit, 1);
    lay->addWidget(browserButton, 0);
  }
  setLayout(lay);

  connect(browserButton, SIGNAL(clicked()), this,
          SLOT(onBrowserButtonClicked()));
  connect(m_edit, SIGNAL(editingFinished()), this, SIGNAL(pathChanged()));
}

void FolderPathField::onBrowserButtonClicked() {
  // 相対パスはStuffフォルダ起点として解釈する
  QString initPath =
      WorkflowManager::resolvePathFromStuffFolder(m_edit->text());

  QString ret = QFileDialog::getExistingDirectory(
      this, tr("Specify Folder"), (initPath.isEmpty()) ? "/home" : initPath,
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

  if (ret.isEmpty() || ret == initPath) return;

  m_edit->setText(ret);
  emit pathChanged();
}
void FolderPathField::setText(const QString& txt) { m_edit->setText(txt); }
QString FolderPathField::text() const { return m_edit->text(); }

//--------------------------------

namespace {
QFormLayout* newProjectFormLayout(QWidget* parent = nullptr) {
  auto* layout = new QFormLayout(parent);
  layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  return layout;
}

// 入力欄の文字列を、投稿先の名前のリストにする（カンマかセミコロン区切り。全角の「、」「，」「；」も区切りとして扱う。
// 前後の空白と、空の名前、重複は除く）
QStringList parseChatDestinationNames(const QString& text) {
  QString unified = text;
  for (QChar separator :
       {QChar(u';'), QChar(u'、'), QChar(u'，'), QChar(u'；')})
    unified.replace(separator, QChar(u','));
  QStringList names;
  for (QString name : unified.split(',', Qt::SkipEmptyParts)) {
    name = name.trimmed();
    if (!name.isEmpty() && !names.contains(name)) names.append(name);
  }
  return names;
}
}  // namespace

BasicsPage::BasicsPage(QWidget* parent) : QWidget(parent) {
  m_projectTitleEdit       = new QLineEdit(this);
  m_projectDescriptionEdit = new QTextEdit(this);
  m_projectDescriptionEdit->setAcceptRichText(false);
  m_projectDescriptionEdit->setMaximumHeight(80);
  m_cutNumberPrefixEdit    = new QLineEdit(this);
  m_sceneThumbnailPathEdit = new MacroLineEdit(this);
  m_sceneThumbnailPathEdit->setMacroCandidateProvider([]() {
    QStringList macros = {"[ROOT]", "[PREFIX]", "[CUTNUMBER]", "[USER_NAME]"};
    if (tmpSettings.useSequence) macros.insert(2, "[SEQUENCE]");
    for (auto& macroInfo : tmpSettings.pathMacros)
      macros.append(macroInfo.macroStr);
    return macros;
  });
  m_useSequenceCheck =
      new QCheckBox(tr("Group Scenes by 100 [SEQUENCE]"), this);
  m_useSequenceCheck->setToolTip(
      tr("When on, scenes are grouped by hundreds: the [SEQUENCE] macro is the "
         "leading number of the scene name "
         "without its last two digits (e.g. 0123 -> 01), and the logs and "
         "scene information are saved in a subfolder "
         "for each sequence. Scene names must begin with a number of 3 or more "
         "digits.\n"
         "When off, the [SEQUENCE] macro cannot be used (it is replaced with "
         "an empty string), and the logs and scene "
         "information are saved without sequence subfolders. Scene names can "
         "be anything.\n"
         "Do not change this in the middle of production: the existing logs "
         "and scene information (including the take "
         "information) will no longer be found unless the folders are "
         "reorganized by hand."));
  m_projectRootPathEdit       = new FolderPathField(this);
  m_projectInfoFolderPathEdit = new FolderPathField(this);
  m_logRootPathEdit           = new FolderPathField(this);
  m_gasUrlEdit                = new QLineEdit(this);
  // GASのURLは、スプレッドシートへの書き込みやChatへの投稿ができる入口のため、画面には表示しない
  m_gasUrlEdit->setEchoMode(QLineEdit::Password);
  m_gasUrlEdit->setPlaceholderText(tr("(Not set)"));

  QGridLayout* mainLay = new QGridLayout();
  mainLay->setContentsMargins(10, 10, 10, 10);
  mainLay->setHorizontalSpacing(5);
  mainLay->setVerticalSpacing(10);
  {
    mainLay->addWidget(new QLabel(tr("Project Title"), this), 0, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(m_projectTitleEdit, 0, 1,
                       Qt::AlignLeft | Qt::AlignVCenter);
    QString tooltip_title = tr("Set the title of the project.");
    mainLay->itemAtPosition(0, 0)->widget()->setToolTip(tooltip_title);
    mainLay->itemAtPosition(0, 1)->widget()->setToolTip(tooltip_title);

    mainLay->addWidget(new QLabel(tr("Description"), this), 1, 0,
                       Qt::AlignRight | Qt::AlignTop);
    mainLay->addWidget(m_projectDescriptionEdit, 1, 1);
    QString tooltip_description =
        tr("Write a human-readable description of the project.");
    mainLay->itemAtPosition(1, 0)->widget()->setToolTip(tooltip_description);
    m_projectDescriptionEdit->setToolTip(tooltip_description);

    mainLay->addWidget(new QLabel(tr("Scene Number Prefix [PREFIX]"), this), 2,
                       0, Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(m_cutNumberPrefixEdit, 2, 1,
                       Qt::AlignLeft | Qt::AlignVCenter);
    QString tooltip_prefix = tr(
        "Set the prefix to be added before the scene number. It will be "
        "substituted as the string for the [PREFIX] macro in the file path.");
    mainLay->itemAtPosition(2, 0)->widget()->setToolTip(tooltip_prefix);
    mainLay->itemAtPosition(2, 1)->widget()->setToolTip(tooltip_prefix);

    mainLay->addWidget(new QLabel(tr("Project Root Folder [ROOT]"), this), 3, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(m_projectRootPathEdit, 3, 1);
    // 3つのフォルダパス共通の、相対パスの扱いの説明（WorkflowManager::resolvePathFromStuffFolder()）
    const QString tooltip_relativePath =
        tr("A relative path is interpreted as relative to the Stuff folder "
           "(e.g. \"sample_data\" means \"[Stuff folder]/sample_data\").");
    QString tooltip_root =
        tr("Specify the root folder for the project data. It will be "
           "substituted as the string for the [ROOT] macro in the file path.") +
        "\n" + tooltip_relativePath;
    mainLay->itemAtPosition(3, 0)->widget()->setToolTip(tooltip_root);
    mainLay->itemAtPosition(3, 1)->widget()->setToolTip(tooltip_root);

    mainLay->addWidget(new QLabel(tr("Project Info Folder"), this), 4, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(m_projectInfoFolderPathEdit, 4, 1);
    QString tooltip_info =
        tr("Specify the folder where scene progress information will be "
           "saved.") +
        "\n" + tooltip_relativePath;
    mainLay->itemAtPosition(4, 0)->widget()->setToolTip(tooltip_info);
    mainLay->itemAtPosition(4, 1)->widget()->setToolTip(tooltip_info);

    mainLay->addWidget(new QLabel(tr("Log Root Folder"), this), 5, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(m_logRootPathEdit, 5, 1);
    QString tooltip_log =
        tr("Specify the root folder where the operation log for each scene "
           "will be saved.") +
        "\n" + tooltip_relativePath;
    mainLay->itemAtPosition(5, 0)->widget()->setToolTip(tooltip_log);
    mainLay->itemAtPosition(5, 1)->widget()->setToolTip(tooltip_log);

    mainLay->addWidget(new QLabel(tr("Google Apps Script URL"), this), 6, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(m_gasUrlEdit, 6, 1);
    QString tooltip_gasUrl =
        tr("Specify the URL of the deployed Google Apps Script web app that "
           "receives progress updates and Google Chat notifications.\n"
           "The web app writes into whichever spreadsheet its Apps Script "
           "project is bound to.\n"
           "Leave this empty to disable the Google Spreadsheet integration and "
           "the Google Chat notification.\n"
           "The URL allows anyone who knows it to use the web app, so it is "
           "stored in an obfuscated form and is not displayed. "
           "To change it, select all (Ctrl+A) and enter the new URL.");
    mainLay->itemAtPosition(6, 0)->widget()->setToolTip(tooltip_gasUrl);
    mainLay->itemAtPosition(6, 1)->widget()->setToolTip(tooltip_gasUrl);

    // Google
    // Chatの投稿先の名前（カンマかセミコロン区切り）：Step_NotifyGoogleChatステップから名前で参照する。
    // Webhook URLはPPMには登録せず、Google Apps Scriptのスクリプト
    // プロパティに、同じ名前で登録する
    QLabel* chatLabel      = new QLabel(tr("Google Chat Destinations"), this);
    m_chatDestinationsEdit = new QLineEdit(this);
    m_chatDestinationsEdit->setPlaceholderText(tr("name1, name2, ..."));
    QPushButton* chatFetchButton = new QPushButton(tr("Get from Script"), this);
    chatFetchButton->setToolTip(
        tr("Get the destination names registered in the Google Apps Script, "
           "and add the ones that are not listed here yet."));
    QPushButton* chatTestButton =
        new QPushButton(tr("Send Test Message"), this);
    chatTestButton->setToolTip(tr("Send a test message to a destination."));
    QWidget* chatRow        = new QWidget(this);
    QHBoxLayout* chatRowLay = new QHBoxLayout(chatRow);
    chatRowLay->setContentsMargins(0, 0, 0, 0);
    chatRowLay->addWidget(m_chatDestinationsEdit, 1);
    chatRowLay->addWidget(chatFetchButton, 0);
    chatRowLay->addWidget(chatTestButton, 0);
    mainLay->addWidget(chatLabel, 7, 0, Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(chatRow, 7, 1);
    QString tooltip_chat =
        tr("The names of the Google Chat destinations that the \"Notify Google "
           "Chat\" step can post to, "
           "separated by commas or semicolons.\n"
           "The webhook URLs are not stored in PPM. Register them in the "
           "script properties of the Google Apps Script "
           "under the same names (CHAT_WEBHOOK_<name>), so that users of PPM "
           "never see them.\n"
           "Messages are sent through the Google Apps Script set above.");
    chatLabel->setToolTip(tooltip_chat);
    m_chatDestinationsEdit->setToolTip(tooltip_chat);

    connect(chatFetchButton, SIGNAL(clicked()), this,
            SLOT(onChatFetchClicked()));
    connect(chatTestButton, SIGNAL(clicked()), this, SLOT(onChatTestClicked()));
    connect(m_chatDestinationsEdit, SIGNAL(editingFinished()), this,
            SLOT(onChatDestinationsEdited()));

    mainLay->addWidget(new QLabel(tr("Scene Thumbnail Path"), this), 8, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(m_sceneThumbnailPathEdit, 8, 1);
    QString tooltip_thumbnail =
        tr("Specify the path to each scene's thumbnail image. If left empty, "
           "the Thumbnail column will not be shown in the scene list.");
    mainLay->itemAtPosition(8, 0)->widget()->setToolTip(tooltip_thumbnail);
    m_sceneThumbnailPathEdit->setToolTip(tooltip_thumbnail);

    // 話数（Episode）：テレビシリーズ等、複数話数で構成される作品の場合のみ使用する任意機能
    m_episodesGroupBox = new QGroupBox(tr("Has Episodes"), this);
    m_episodesGroupBox->setCheckable(true);
    m_episodesGroupBox->setChecked(false);
    m_episodesGroupBox->setToolTip(
        tr("Enable this if the project is organized into multiple episodes "
           "(e.g. a TV series), each containing its own set of scenes."));
    QVBoxLayout* episodesLay = new QVBoxLayout(m_episodesGroupBox);
    m_episodeNamesEdit       = new QLineEdit(m_episodesGroupBox);
    m_episodeNamesEdit->setToolTip(
        tr("Separate episode names by comma or semicolon. Each name will be "
           "substituted for the [EPISODE] macro in the file path when that "
           "episode is active."));
    episodesLay->addWidget(
        new QLabel(tr("Episode Names [EPISODE] (comma or semicolon separated)"),
                   m_episodesGroupBox));
    episodesLay->addWidget(m_episodeNamesEdit);
    mainLay->addWidget(m_useSequenceCheck, 9, 1);
    mainLay->addWidget(m_episodesGroupBox, 10, 0, 1, 2);
  }
  mainLay->setColumnStretch(0, 0);
  mainLay->setColumnStretch(1, 1);
  mainLay->setRowStretch(11, 1);
  setLayout(mainLay);

  connect(m_projectTitleEdit, SIGNAL(editingFinished()), this,
          SLOT(onFieldEdited()));
  connect(m_projectDescriptionEdit, SIGNAL(textChanged()), this,
          SLOT(onFieldEdited()));
  connect(m_cutNumberPrefixEdit, SIGNAL(editingFinished()), this,
          SLOT(onFieldEdited()));
  connect(m_useSequenceCheck, SIGNAL(toggled(bool)), this,
          SLOT(onFieldEdited()));
  connect(m_gasUrlEdit, SIGNAL(editingFinished()), this, SLOT(onFieldEdited()));
  connect(m_sceneThumbnailPathEdit, SIGNAL(editingFinished()), this,
          SLOT(onFieldEdited()));
  connect(m_projectRootPathEdit, SIGNAL(pathChanged()), this,
          SLOT(onFieldEdited()));
  connect(m_projectInfoFolderPathEdit, SIGNAL(pathChanged()), this,
          SLOT(onFieldEdited()));
  connect(m_logRootPathEdit, SIGNAL(pathChanged()), this,
          SLOT(onFieldEdited()));
  connect(m_episodesGroupBox, SIGNAL(toggled(bool)), this,
          SLOT(onFieldEdited()));
  connect(m_episodeNamesEdit, SIGNAL(editingFinished()), this,
          SLOT(onFieldEdited()));
}

void BasicsPage::refresh() {
  m_projectTitleEdit->setText(tmpSettings.projectTitle);
  // setPlainText()はtextChanged()を発火してしまうため、リフレッシュ直後に
  // 誤ってdirty扱いされないよう一時的にシグナルを止める
  m_projectDescriptionEdit->blockSignals(true);
  m_projectDescriptionEdit->setPlainText(tmpSettings.projectDescription);
  m_projectDescriptionEdit->blockSignals(false);
  m_cutNumberPrefixEdit->setText(tmpSettings.cutNumberPrefix);
  m_useSequenceCheck->blockSignals(true);
  m_useSequenceCheck->setChecked(tmpSettings.useSequence);
  m_useSequenceCheck->blockSignals(false);
  m_projectRootPathEdit->setText(tmpSettings.projectRootPath);
  m_projectInfoFolderPathEdit->setText(tmpSettings.projectInfoFolderPath);
  m_logRootPathEdit->setText(tmpSettings.logRootPath);
  m_gasUrlEdit->setText(tmpSettings.gasUrl);
  m_sceneThumbnailPathEdit->setText(tmpSettings.sceneThumbnailPath);
  // setChecked()はtoggled(bool)を発火してしまう（editingFinished系のシグナルと違い、
  // プログラムからの変更でも発火する）ため、リフレッシュ直後に誤ってdirty扱いされないよう
  // 一時的にシグナルを止める
  m_episodesGroupBox->blockSignals(true);
  m_episodesGroupBox->setChecked(tmpSettings.hasEpisodes);
  m_episodesGroupBox->blockSignals(false);
  m_episodeNamesEdit->setText(tmpSettings.episodeNames.join(", "));

  m_chatDestinationsEdit->setText(tmpSettings.chatDestinations.join(", "));
}

void BasicsPage::onFieldEdited() {
  tmpSettings.projectTitle          = m_projectTitleEdit->text();
  tmpSettings.projectDescription    = m_projectDescriptionEdit->toPlainText();
  tmpSettings.cutNumberPrefix       = m_cutNumberPrefixEdit->text();
  tmpSettings.useSequence           = m_useSequenceCheck->isChecked();
  tmpSettings.projectRootPath       = m_projectRootPathEdit->text();
  tmpSettings.projectInfoFolderPath = m_projectInfoFolderPathEdit->text();
  tmpSettings.hasEpisodes           = m_episodesGroupBox->isChecked();
  tmpSettings.episodeNames          = m_episodeNamesEdit->text().split(
      QRegularExpression("[,;]"), Qt::SkipEmptyParts);
  for (auto& name : tmpSettings.episodeNames) name = name.trimmed();
  tmpSettings.logRootPath        = m_logRootPathEdit->text();
  tmpSettings.gasUrl             = m_gasUrlEdit->text();
  tmpSettings.sceneThumbnailPath = m_sceneThumbnailPathEdit->text();
  markDirty();
}

void BasicsPage::onChatDestinationsEdited() {
  QStringList oldNames = tmpSettings.chatDestinations;
  QStringList newNames =
      parseChatDestinationNames(m_chatDestinationsEdit->text());
  // 表示を、空白や重複を取り除いた形に整える
  m_chatDestinationsEdit->setText(newNames.join(", "));
  if (newNames == oldNames) return;

  // 名前を1つだけ書き換えた場合は、その投稿先を参照しているステップの参照先も、新しい名前に付け替える
  if (oldNames.size() == newNames.size()) {
    int diffIndex = -1, diffCount = 0;
    for (int i = 0; i < oldNames.size(); i++)
      if (oldNames.at(i) != newNames.at(i)) {
        diffCount++;
        diffIndex = i;
      }
    if (diffCount == 1 && !oldNames.contains(newNames.at(diffIndex)))
      renameChatDestinationReferencesInTmpSettings(oldNames.at(diffIndex),
                                                   newNames.at(diffIndex));
  }
  tmpSettings.chatDestinations = newNames;
  markDirty();
}

void BasicsPage::onChatFetchClicked() {
  QString gasUrl = m_gasUrlEdit->text().trimmed();
  if (gasUrl.isEmpty()) {
    QMessageBox::warning(this, tr("Warning"),
                         tr("Set the Google Apps Script URL first."));
    return;
  }

  QPointer<BasicsPage> self(this);
  GoogleChatGateway::instance()->fetchDestinations(
      gasUrl, [self](bool ok, const QStringList& names, const QString& error) {
        QWidget* parent = self ? static_cast<QWidget*>(self.data())
                               : QApplication::activeWindow();
        if (!ok) {
          QMessageBox::warning(
              parent, tr("Warning"),
              tr("Failed to get the destination names from the Google Apps "
                 "Script.\n"
                 "(The script must be updated to the latest Code.gs and "
                 "deployed as a new version.)\n\n%1")
                  .arg(error));
          return;
        }
        if (!self) return;

        // まだ入力欄にない名前を追加する
        QStringList current =
            parseChatDestinationNames(self->m_chatDestinationsEdit->text());
        QStringList added, onlyInPpm;
        for (const QString& name : names) {
          if (!current.contains(name)) {
            current.append(name);
            added.append(name);
          }
        }
        for (const QString& name : current)
          if (!names.contains(name)) onlyInPpm.append(name);
        if (!added.isEmpty()) {
          self->m_chatDestinationsEdit->setText(current.join(", "));
          self->onChatDestinationsEdited();
        }

        QString message = added.isEmpty()
                              ? tr("All the destinations registered in the "
                                   "script are already listed here.")
                              : tr("Added %1 destination(s):\n - %2")
                                    .arg(added.size())
                                    .arg(added.join("\n - "));
        if (!onlyInPpm.isEmpty())
          message +=
              "\n\n" + tr("The following destinations are not registered in "
                          "the script, so messages to them will fail:\n - %1")
                           .arg(onlyInPpm.join("\n - "));
        QMessageBox::information(parent, tr("Information"), message);
      });
}

void BasicsPage::onChatTestClicked() {
  QStringList names = parseChatDestinationNames(m_chatDestinationsEdit->text());
  if (names.isEmpty()) {
    QMessageBox::information(this, tr("Information"),
                             tr("Enter the destination names first."));
    return;
  }
  QString gasUrl = m_gasUrlEdit->text().trimmed();
  if (gasUrl.isEmpty()) {
    QMessageBox::warning(this, tr("Warning"),
                         tr("Set the Google Apps Script URL first."));
    return;
  }

  // 投稿先が複数ある場合は、どれに送るかをメニューで選ぶ
  QString name = names.first();
  if (names.size() > 1) {
    QMenu menu;
    for (const QString& n : names) menu.addAction(n);
    QWidget* button = qobject_cast<QWidget*>(sender());
    QAction* chosen =
        menu.exec(button ? button->mapToGlobal(QPoint(0, button->height()))
                         : QCursor::pos());
    if (!chosen) return;
    name = chosen->text();
  }

  QPointer<BasicsPage> self(this);
  GoogleChatGateway::instance()->post(
      gasUrl, name,
      tr("[PPM] Test message (%1)")
          .arg(QDateTime::currentDateTime().toString("yyyy/MM/dd hh:mm:ss")),
      QString(), [self, name](bool ok, const QString& error) {
        QWidget* parent = self ? static_cast<QWidget*>(self.data())
                               : QApplication::activeWindow();
        if (ok)
          QMessageBox::information(
              parent, tr("Information"),
              tr("The test message was sent to \"%1\".").arg(name));
        else
          QMessageBox::warning(
              parent, tr("Warning"),
              tr("Failed to send the test message to \"%1\".\n\n%2")
                  .arg(name, error));
      });
}

//--------------------------------

namespace {
const int PathMacroColumn_Macro     = 0;
const int PathMacroColumn_Path      = 1;
const int PathMacroColumn_Label     = 2;
const int PathMacroColumn_Obfuscate = 3;

// テーブルのセル編集時に、MacroLineEditと同じマクロ入力補助
// （"["の入力でマクロを補完候補として表示する）と、マクロの色付けを使えるようにする。
// ステップのパスの一覧（buildStepPathListField）、パスマクロの「パス」、
// 工程グループの「XDTSファイル名」で使う
class MacroLineEditDelegate : public QStyledItemDelegate {
  std::function<QStringList()>
      m_candidateProvider;         // nullptrならMacroLineEditの既定候補を使う
  bool m_highlightOnlyCandidates;  // trueなら補完候補のマクロだけに色を付ける
public:
  MacroLineEditDelegate(
      QObject* parent, std::function<QStringList()> candidateProvider = nullptr,
      bool highlightOnlyCandidates = false)
      : QStyledItemDelegate(parent)
      , m_candidateProvider(candidateProvider)
      , m_highlightOnlyCandidates(highlightOnlyCandidates) {}
  QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&,
                        const QModelIndex&) const override {
    MacroLineEdit* edit = new MacroLineEdit(parent);
    if (m_candidateProvider)
      edit->setMacroCandidateProvider(m_candidateProvider);
    edit->setHighlightOnlyCandidates(m_highlightOnlyCandidates);
    // 選択行の背景の上でも読めるよう、白地に黒字にする（マクロの色も、明るい背景用になる）
    QPalette pal = edit->palette();
    pal.setColor(QPalette::Base, Qt::white);
    pal.setColor(QPalette::Text, Qt::black);
    edit->setPalette(pal);
    // QPlainTextEditのEnterキーは、デリゲートの標準処理では確定されないため、Enterで確定して編集を終える
    QAbstractItemDelegate* self = const_cast<MacroLineEditDelegate*>(this);
    QObject::connect(edit, &MacroLineEdit::editingFinished, self,
                     [self, edit]() {
                       emit self->commitData(edit);
                       emit self->closeEditor(edit);
                     });
    return edit;
  }
  // 編集中でないセルも、マクロを文字色で強調表示する
  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    const QString text = opt.text;
    opt.text.clear();
    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter,
                       opt.widget);  // 背景・選択・フォーカス枠（文字なし）
    const QRect textRect =
        style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget)
            .adjusted(3, 0, -3, 0);
    const bool selected = opt.state & QStyle::State_Selected;
    const QString shown = QFontMetrics(opt.font).elidedText(
        text, Qt::ElideRight, textRect.width());
    QTextLayout layout(shown, opt.font);
    QList<QTextLayout::FormatRange> formats;
    if (!selected) {  // 選択中は、選択色の背景との対比が崩れないよう、色を付けない
      const QStringList candidates =
          m_candidateProvider ? m_candidateProvider()
                              : MacroLineEdit::defaultMacroCandidates();
      const QStringList macros = m_highlightOnlyCandidates
                                     ? candidates
                                     : MacroLineEdit::knownMacros(candidates);
      for (const auto& range : MacroLineEdit::findMacroRanges(shown, macros)) {
        QTextLayout::FormatRange fr;
        fr.start  = range.first;
        fr.length = range.second;
        fr.format.setForeground(MacroHighlighter::macroColor(
            opt.widget ? opt.widget : option.widget));
        formats.append(fr);
      }
    }
    layout.setFormats(formats);
    layout.beginLayout();
    QTextLine line = layout.createLine();
    line.setLineWidth(textRect.width());
    layout.endLayout();
    painter->save();
    painter->setClipRect(textRect);
    painter->setPen(opt.palette.color(selected ? QPalette::HighlightedText
                                               : QPalette::Text));
    layout.draw(painter, QPointF(textRect.left(),
                                 textRect.top() +
                                     (textRect.height() - line.height()) / 2));
    painter->restore();
  }
  void setEditorData(QWidget* editor, const QModelIndex& index) const override {
    static_cast<MacroLineEdit*>(editor)->setText(
        index.data(Qt::EditRole).toString());
  }
  void setModelData(QWidget* editor, QAbstractItemModel* model,
                    const QModelIndex& index) const override {
    model->setData(index, static_cast<MacroLineEdit*>(editor)->text(),
                   Qt::EditRole);
  }
};

// パスマクロの「パス」で使えるマクロ（パスの解決で置き換えられる組み込みのマクロ）。
// パスマクロの中に別のパスマクロを書いても置き換えられないので、パスマクロは候補にしない
QStringList pathMacroPathCandidates() {
  QStringList macros = {"[ROOT]",    "[PREFIX]",    "[CUTNUMBER]",
                        "[EPISODE]", "[USER_NAME]", "[TAKE]",
                        "[TAKE-1]",  "[R_TAKE]",    "[PREV_EXISTING_TAKE]",
                        "[R]"};
  if (tmpSettings.useSequence) macros.insert(2, "[SEQUENCE]");
  return macros;
}

// 工程グループの「XDTSファイル名」で使えるマクロ（WorkflowManager::xdtsPaths()で置き換えられるもの）
QStringList xdtsFileNameCandidates() {
  QStringList macros = {"[PREFIX]",    "[CUTNUMBER]", "[EPISODE]",
                        "[USER_NAME]", "[R_TAKE]",    "[GENGA_OR_DOUGA]"};
  if (tmpSettings.useSequence) macros.insert(2, "[SEQUENCE]");
  return macros;
}
};  // namespace

PathMacrosPage::PathMacrosPage(QWidget* parent) : QWidget(parent) {
  m_table = new QTableWidget(0, 4, this);
  QStringList headers;
  headers << tr("Macro") << tr("Path") << tr("Label") << tr("Obfuscate");
  m_table->setHorizontalHeaderLabels(headers);
  m_table->verticalHeader()->setVisible(false);
  m_table->horizontalHeader()->setSectionResizeMode(
      PathMacroColumn_Macro, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(PathMacroColumn_Path,
                                                    QHeaderView::Stretch);
  m_table->horizontalHeader()->setSectionResizeMode(
      PathMacroColumn_Label, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(
      PathMacroColumn_Obfuscate, QHeaderView::ResizeToContents);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
  // 「パス」の列で、"["の入力でマクロを補完し、マクロに色を付ける
  m_table->setStyleSheet(
      "QPlainTextEdit { background-color: white; color: black; }");
  m_table->setItemDelegateForColumn(
      PathMacroColumn_Path,
      new MacroLineEditDelegate(m_table, &pathMacroPathCandidates));
  m_table->setToolTip(
      tr("Define macros (e.g. [ROOT]) that can be used in file paths.\n"
         "\"Obfuscate\" encodes the path when saving, hiding it from users who "
         "view the settings file."));

  QPushButton* addButton    = new QPushButton(tr("Add"), this);
  QPushButton* removeButton = new QPushButton(tr("Remove"), this);

  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setContentsMargins(10, 10, 10, 10);
  {
    mainLay->addWidget(m_table, 1);

    QHBoxLayout* buttonsLay = new QHBoxLayout();
    buttonsLay->setContentsMargins(0, 0, 0, 0);
    {
      buttonsLay->addStretch(1);
      buttonsLay->addWidget(addButton, 0);
      buttonsLay->addWidget(removeButton, 0);
    }
    mainLay->addLayout(buttonsLay, 0);
  }
  setLayout(mainLay);

  connect(addButton, SIGNAL(clicked()), this, SLOT(onAddButtonClicked()));
  connect(removeButton, SIGNAL(clicked()), this, SLOT(onRemoveButtonClicked()));
  connect(m_table, SIGNAL(itemChanged(QTableWidgetItem*)), this,
          SLOT(onItemChanged(QTableWidgetItem*)));
}

void PathMacrosPage::refresh() {
  m_table->blockSignals(true);

  m_table->setRowCount(0);
  for (auto& macroInfo : tmpSettings.pathMacros) {
    int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, PathMacroColumn_Macro,
                     new QTableWidgetItem(macroInfo.macroStr));

    QTableWidgetItem* pathItem;
    if (macroInfo.doObfuscate) {
      // 難読化ONのときは実際の値を表示せず、プレースホルダーのみを表示する。
      // 誤って上書き保存されないよう、このセル自体を編集不可にする
      pathItem = new QTableWidgetItem(tr("-- This path is obfuscated --"));
      pathItem->setFlags(pathItem->flags() & ~Qt::ItemIsEditable);
      pathItem->setToolTip(tr(
          "This path is hidden. Turn off \"Obfuscate\" to view or edit it."));
    } else {
      pathItem = new QTableWidgetItem(macroInfo.path);
    }
    m_table->setItem(row, PathMacroColumn_Path, pathItem);

    m_table->setItem(row, PathMacroColumn_Label,
                     new QTableWidgetItem(macroInfo.labelName));

    QCheckBox* obfuscateCheck = new QCheckBox();
    obfuscateCheck->setChecked(macroInfo.doObfuscate);
    QWidget* checkContainer = new QWidget();
    QHBoxLayout* checkLay   = new QHBoxLayout(checkContainer);
    checkLay->setContentsMargins(0, 0, 0, 0);
    checkLay->setAlignment(Qt::AlignCenter);
    checkLay->addWidget(obfuscateCheck);
    m_table->setCellWidget(row, PathMacroColumn_Obfuscate, checkContainer);
    connect(
        obfuscateCheck, &QCheckBox::toggled, this,
        [this, row, obfuscateCheck](bool checked) {
          if (row < 0 || row >= tmpSettings.pathMacros.size()) return;

          if (checked) {
            if (tmpSettings.pathMacros[row].path.isEmpty()) {
              // 隠すべき値が無い状態でのONを禁止し、警告を出して操作を取り消す
              QMessageBox::warning(this, tr("Warning"),
                                   tr("Path is empty. Enter a Path before "
                                      "turning Obfuscate on."));
              obfuscateCheck->blockSignals(true);
              obfuscateCheck->setChecked(false);
              obfuscateCheck->blockSignals(false);
              return;
            }

            QMessageBox::StandardButton ret = QMessageBox::question(
                this, tr("Confirm"),
                tr("Turning this on will hide the Path from users who open "
                   "this dialog.\n"
                   "When you turn it off again, you will need to re-enter the "
                   "Path.\n"
                   "Are you sure you want to continue?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (ret != QMessageBox::Yes) {
              // ユーザーがキャンセルしたのでチェックを戻す（この際toggledの再発火を防ぐ）
              obfuscateCheck->blockSignals(true);
              obfuscateCheck->setChecked(false);
              obfuscateCheck->blockSignals(false);
              return;
            }
            tmpSettings.pathMacros[row].doObfuscate = true;
          } else {
            tmpSettings.pathMacros[row].doObfuscate = false;
            tmpSettings.pathMacros[row].path.clear();
          }
          markDirty();
          refresh();  // Path列の表示内容・編集可否を切り替えるため一覧全体を作り直す
        });
  }

  m_table->blockSignals(false);
}

void PathMacrosPage::onAddButtonClicked() {
  PathMacroInfo newMacroInfo;
  newMacroInfo.macroStr = tr("[NEW_MACRO]");
  tmpSettings.pathMacros.append(newMacroInfo);
  markDirty();
  refresh();

  int newRow = m_table->rowCount() - 1;
  m_table->selectRow(newRow);
  m_table->editItem(m_table->item(newRow, PathMacroColumn_Macro));
}

void PathMacrosPage::onRemoveButtonClicked() {
  QSet<int> rows;
  for (auto* item : m_table->selectedItems()) rows.insert(item->row());
  if (rows.isEmpty()) return;

  QList<int> sortedRows = rows.values();
  std::sort(sortedRows.begin(), sortedRows.end(), std::greater<int>());
  for (int row : sortedRows) tmpSettings.pathMacros.removeAt(row);

  markDirty();
  refresh();
}

void PathMacrosPage::onItemChanged(QTableWidgetItem* item) {
  int row = item->row();
  if (row < 0 || row >= tmpSettings.pathMacros.size()) return;

  switch (item->column()) {
  case PathMacroColumn_Macro:
    tmpSettings.pathMacros[row].macroStr = item->text();
    break;
  case PathMacroColumn_Path:
    // 難読化ON時はセルを編集不可にしているため通常は発生しないが、念のためプレースホルダーの
    // 表示文字列で実際の値を上書きしてしまわないよう防御する
    if (!tmpSettings.pathMacros[row].doObfuscate)
      tmpSettings.pathMacros[row].path = item->text();
    break;
  case PathMacroColumn_Label:
    tmpSettings.pathMacros[row].labelName = item->text();
    break;
  }
  markDirty();
}

//--------------------------------

namespace {
const int WfGroupColumn_Id                     = 0;
const int WfGroupColumn_IdName                 = 1;
const int WfGroupColumn_LabelName              = 2;
const int WfGroupColumn_StandardCutFolderMacro = 3;
const int WfGroupColumn_XdtsFileName           = 4;
const int WfGroupColumn_TakeNotationStyle      = 5;
const int WfGroupColumn_IsRetakeGroup          = 6;
const int WfGroupColumn_IsRetakeSourceGroup    = 7;
const int WfGroupColumn_SubfolderForOpenButton = 8;
};  // namespace

WfGroupsPage::WfGroupsPage(QWidget* parent) : QWidget(parent) {
  m_table = new QTableWidget(0, 9, this);
  QStringList headers;
  headers << tr("ID") << tr("ID Name") << tr("Label Name")
          << tr("Standard Cut Folder Macro") << tr("XDTS File Name")
          << tr("Take Notation") << tr("Is Retake Group")
          << tr("Is Retake Source") << tr("Subfolder For Open Button");
  m_table->setHorizontalHeaderLabels(headers);
  m_table->verticalHeader()->setVisible(false);
  m_table->horizontalHeader()->setSectionResizeMode(
      WfGroupColumn_Id, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(
      WfGroupColumn_IdName, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(
      WfGroupColumn_LabelName, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(
      WfGroupColumn_StandardCutFolderMacro, QHeaderView::Stretch);
  m_table->horizontalHeader()->setSectionResizeMode(WfGroupColumn_XdtsFileName,
                                                    QHeaderView::Stretch);
  m_table->horizontalHeader()->setSectionResizeMode(
      WfGroupColumn_TakeNotationStyle, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(
      WfGroupColumn_IsRetakeGroup, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(
      WfGroupColumn_IsRetakeSourceGroup, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(
      WfGroupColumn_SubfolderForOpenButton, QHeaderView::Stretch);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
  // 「XDTSファイル名」の列で、"["の入力でマクロを補完し、使えるマクロだけに色を付ける
  m_table->setStyleSheet(
      "QPlainTextEdit { background-color: white; color: black; }");
  m_table->setItemDelegateForColumn(
      WfGroupColumn_XdtsFileName,
      new MacroLineEditDelegate(m_table, &xdtsFileNameCandidates, true));
  m_table->setToolTip(tr(
      "Define the processing step groups (e.g. LO, Genga, Douga...) used in "
      "this project.\n"
      "\"ID\" must be a unique non-negative integer; it is referenced "
      "internally and should not be changed once cuts have been registered.\n"
      "\"Standard Cut Folder Macro\" must match a macro defined in the Path "
      "Macros page.\n"
      "\"Take Notation\" controls how the current take/rewind number is "
      "displayed for cuts in this group.\n"
      "\"Is Retake Group\"/\"Is Retake Source\" control which groups appear as "
      "retake destinations, and which group cuts are retaken from, in the "
      "Retake dialog.\n"
      "\"Subfolder For Open Button\" (optional): if set, the \"Open\" button "
      "opens a .clip/.tvpp/.tnz file found in this subfolder (relative to the "
      "cue file's folder) instead of the cue file's own folder. Can contain "
      "macros such as [PREFIX]/[CUTNUMBER]."));

  QPushButton* addButton    = new QPushButton(tr("Add"), this);
  QPushButton* removeButton = new QPushButton(tr("Remove"), this);

  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setContentsMargins(10, 10, 10, 10);
  {
    mainLay->addWidget(m_table, 1);

    QHBoxLayout* buttonsLay = new QHBoxLayout();
    buttonsLay->setContentsMargins(0, 0, 0, 0);
    {
      buttonsLay->addStretch(1);
      buttonsLay->addWidget(addButton, 0);
      buttonsLay->addWidget(removeButton, 0);
    }
    mainLay->addLayout(buttonsLay, 0);
  }
  setLayout(mainLay);

  connect(addButton, SIGNAL(clicked()), this, SLOT(onAddButtonClicked()));
  connect(removeButton, SIGNAL(clicked()), this, SLOT(onRemoveButtonClicked()));
  connect(m_table, SIGNAL(itemChanged(QTableWidgetItem*)), this,
          SLOT(onItemChanged(QTableWidgetItem*)));
}

void WfGroupsPage::refresh() {
  m_table->blockSignals(true);

  m_table->setRowCount(0);
  m_rowIds.clear();

  // チェックボックスをセルの中央に配置して埋め込み、変更をtmpSettings.flowGroupInfosへライブ反映する
  auto addCheckboxCell = [this](
                             int row, int column, bool checked,
                             std::function<void(FlowGroupInfo&, bool)> setter) {
    QCheckBox* check = new QCheckBox();
    check->setChecked(checked);
    QWidget* container = new QWidget();
    QHBoxLayout* lay   = new QHBoxLayout(container);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setAlignment(Qt::AlignCenter);
    lay->addWidget(check);
    m_table->setCellWidget(row, column, container);
    connect(check, &QCheckBox::toggled, this,
            [this, row, setter](bool checked) {
              if (row < 0 || row >= m_rowIds.size()) return;
              FlowGroupId fgId   = (FlowGroupId)m_rowIds.at(row);
              FlowGroupInfo info = tmpSettings.flowGroupInfos.value(fgId);
              setter(info, checked);
              tmpSettings.flowGroupInfos.insert(fgId, info);
              markDirty();
            });
  };

  QList<FlowGroupId> ids =
      tmpSettings.flowGroupInfos.keys();  // QMapのキー順（昇順）
  for (auto fgId : ids) {
    const FlowGroupInfo& info = tmpSettings.flowGroupInfos.value(fgId);

    int row = m_table->rowCount();
    m_table->insertRow(row);
    m_rowIds.append((unsigned int)fgId);

    m_table->setItem(row, WfGroupColumn_Id,
                     new QTableWidgetItem(QString::number((unsigned int)fgId)));
    m_table->setItem(row, WfGroupColumn_IdName,
                     new QTableWidgetItem(info.idName));
    m_table->setItem(row, WfGroupColumn_LabelName,
                     new QTableWidgetItem(info.labelName));
    m_table->setItem(row, WfGroupColumn_StandardCutFolderMacro,
                     new QTableWidgetItem(info.standardCutFolderMacro));
    m_table->setItem(row, WfGroupColumn_XdtsFileName,
                     new QTableWidgetItem(info.xdtsFileName));

    QComboBox* takeNotationCombo = new QComboBox();
    takeNotationCombo->addItem(tr("None"), FlowGroupInfo::TakeNotation_None);
    takeNotationCombo->addItem(tr("Rewind [R]"),
                               FlowGroupInfo::TakeNotation_Rewind);
    takeNotationCombo->addItem(tr("Take [TAKE]"),
                               FlowGroupInfo::TakeNotation_Take);
    takeNotationCombo->addItem(tr("R-Take [R_TAKE]"),
                               FlowGroupInfo::TakeNotation_RTake);
    takeNotationCombo->setCurrentIndex(
        takeNotationCombo->findData(info.takeNotationStyle));
    m_table->setCellWidget(row, WfGroupColumn_TakeNotationStyle,
                           takeNotationCombo);
    connect(
        takeNotationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this, row](int) {
          if (row < 0 || row >= m_rowIds.size()) return;
          QComboBox* combo = qobject_cast<QComboBox*>(
              m_table->cellWidget(row, WfGroupColumn_TakeNotationStyle));
          if (!combo) return;
          FlowGroupId fgId   = (FlowGroupId)m_rowIds.at(row);
          FlowGroupInfo info = tmpSettings.flowGroupInfos.value(fgId);
          info.takeNotationStyle =
              (FlowGroupInfo::TakeNotationStyle)combo->currentData().toInt();
          tmpSettings.flowGroupInfos.insert(fgId, info);
          markDirty();
        });

    addCheckboxCell(
        row, WfGroupColumn_IsRetakeGroup, info.isRetakeGroup,
        [](FlowGroupInfo& i, bool checked) { i.isRetakeGroup = checked; });
    addCheckboxCell(row, WfGroupColumn_IsRetakeSourceGroup,
                    info.isRetakeSourceGroup,
                    [](FlowGroupInfo& i, bool checked) {
                      i.isRetakeSourceGroup = checked;
                    });

    m_table->setItem(row, WfGroupColumn_SubfolderForOpenButton,
                     new QTableWidgetItem(info.subfolderForOpenButton));
  }

  m_table->blockSignals(false);
}

void WfGroupsPage::selectGroup(FlowGroupId fgId) {
  int row = m_rowIds.indexOf((unsigned int)fgId);
  if (row < 0) return;
  m_table->clearSelection();
  m_table->selectRow(row);
  if (m_table->item(row, 0)) m_table->scrollToItem(m_table->item(row, 0));
}

void WfGroupsPage::onAddButtonClicked() {
  unsigned int newId = 0;
  for (auto fgId : tmpSettings.flowGroupInfos.keys())
    newId = std::max(newId, (unsigned int)fgId + 1);

  tmpSettings.flowGroupInfos.insert((FlowGroupId)newId, FlowGroupInfo());
  markDirty();
  refresh();

  int newRow = m_table->rowCount() - 1;
  m_table->selectRow(newRow);
  m_table->editItem(m_table->item(newRow, WfGroupColumn_IdName));
}

void WfGroupsPage::onRemoveButtonClicked() {
  QSet<int> rows;
  for (auto* item : m_table->selectedItems()) rows.insert(item->row());
  if (rows.isEmpty()) return;

  QSet<FlowGroupId> ids;
  for (int row : rows) ids.insert((FlowGroupId)m_rowIds.at(row));

  // 工程が所属している工程グループは削除しない
  // （削除すると、工程がどの工程グループにも属さない状態で残ってしまうため）
  QStringList wfNames;
  for (auto& wf : tmpSettings.workflows)
    if (ids.contains(wf.groupId))
      wfNames.append(QString("%1 (%2)").arg(
          wf.name, tmpSettings.flowGroupInfos.value(wf.groupId).labelName));
  if (!wfNames.isEmpty()) {
    QMessageBox::warning(
        this, tr("Warning"),
        tr("The workflow group cannot be deleted because the following "
           "workflows belong to it:\n\n%1\n\n"
           "Delete these workflows or move them to another workflow group "
           "first.")
            .arg(wfNames.join("\n")));
    return;
  }

  // 工程グループを指定しているステップがある場合は、確認してから削除する
  int refCount = countFlowGroupReferencesInTmpSettings(ids);
  if (refCount > 0 &&
      QMessageBox::warning(
          this, tr("Warning"),
          tr("%1 step(s) (Move Flow Group, Start Branch Flow, End Branch Flow) "
             "specify the workflow group to be deleted.\n"
             "They will not work correctly until you specify another "
             "workflow group in them.\n\n"
             "Do you want to delete the workflow group?")
              .arg(refCount),
          QMessageBox::Yes | QMessageBox::Cancel,
          QMessageBox::Cancel) != QMessageBox::Yes)
    return;

  for (auto id : ids) tmpSettings.flowGroupInfos.remove(id);
  // フローチャートの、削除した工程グループの枠の配置情報も取り除く
  auto& groupNodes = tmpSettings.flowChartLayout.groupNodes;
  for (int i = groupNodes.size() - 1; i >= 0; i--)
    if (ids.contains(groupNodes.at(i).groupId)) groupNodes.removeAt(i);

  markDirty();
  refresh();
}

void WfGroupsPage::onItemChanged(QTableWidgetItem* item) {
  int row = item->row();
  if (row < 0 || row >= m_rowIds.size()) return;

  FlowGroupId fgId = (FlowGroupId)m_rowIds.at(row);

  if (item->column() == WfGroupColumn_Id) {
    bool ok            = false;
    unsigned int newId = item->text().toUInt(&ok);
    if (!ok || (newId != (unsigned int)fgId &&
                tmpSettings.flowGroupInfos.contains((FlowGroupId)newId))) {
      // 不正な値、または既に使われているIDの場合は元に戻す
      m_table->blockSignals(true);
      item->setText(QString::number((unsigned int)fgId));
      m_table->blockSignals(false);
      return;
    }
    if (newId == (unsigned int)fgId) return;

    FlowGroupInfo info = tmpSettings.flowGroupInfos.take(fgId);
    tmpSettings.flowGroupInfos.insert((FlowGroupId)newId, info);
    remapFlowGroupReferencesInTmpSettings(fgId, (FlowGroupId)newId);
    markDirty();
    refresh();  // IDの昇順に並び替えるため一覧を作り直す
    int newRow = m_rowIds.indexOf(newId);
    if (newRow >= 0) m_table->selectRow(newRow);
    return;
  }

  FlowGroupInfo info = tmpSettings.flowGroupInfos.value(fgId);
  switch (item->column()) {
  case WfGroupColumn_IdName:
    info.idName = item->text();
    break;
  case WfGroupColumn_LabelName:
    info.labelName = item->text();
    break;
  case WfGroupColumn_StandardCutFolderMacro:
    info.standardCutFolderMacro = item->text();
    break;
  case WfGroupColumn_XdtsFileName:
    info.xdtsFileName = item->text();
    break;
  case WfGroupColumn_SubfolderForOpenButton:
    info.subfolderForOpenButton = item->text();
    break;
  default:
    return;
  }

  tmpSettings.flowGroupInfos.insert(fgId, info);
  markDirty();
}

//--------------------------------

ColorButton::ColorButton(QWidget* parent) : QPushButton(parent) {
  setFixedWidth(50);
  setColor(QColor(Qt::white));
  connect(this, SIGNAL(clicked()), this, SLOT(onButtonClicked()));
}

void ColorButton::setColor(const QColor& color) {
  m_color = color;
  setStyleSheet(
      QString("QPushButton { background-color: %1; border: 1px solid gray; }")
          .arg(color.name()));
}

void ColorButton::onButtonClicked() {
  QColor newColor = QColorDialog::getColor(m_color, this, tr("Select Color"));
  if (!newColor.isValid()) return;
  setColor(newColor);
  emit colorChanged(newColor);
}

//--------------------------------

namespace {
const int InfoTagListColumn_IdName = 0;
const int InfoTagListColumn_Type   = 1;

QString sceneInfoTagTypeDisplayName(SceneInfoTagType type) {
  switch (type) {
  case InfoTagType_Bool:
    return QObject::tr("Bool");
  case InfoTagType_Enum:
    return QObject::tr("Enum");
  case InfoTagType_String:
    return QObject::tr("String");
  case InfoTagType_StringList:
    return QObject::tr("String List");
  }
  return QString();
}

SceneInfoTagDisplayProperties defaultDisplayProperties() {
  return {QObject::tr("New Tag"), QColor(Qt::white), QColor(Qt::black)};
}

struct ColorPreset {
  QString name;
  QColor bgColor;
  QColor txtColor;
};

// Gmailのラベルの色プリセットを元にした、背景色・文字色の組み合わせ一覧
const QList<ColorPreset>& colorPresets() {
  static const QList<ColorPreset> presets = {
      {QObject::tr("Gray"), QColor("#666666"), QColor("#ffffff")},
      {QObject::tr("Light Gray"), QColor("#cccccc"), QColor("#000000")},
      {QObject::tr("Pale Gray"), QColor("#efefef"), QColor("#000000")},
      {QObject::tr("Red"), QColor("#fb4c2f"), QColor("#ffffff")},
      {QObject::tr("Light Red"), QColor("#f6c5be"), QColor("#000000")},
      {QObject::tr("Pale Red"), QColor("#ffe6e0"), QColor("#000000")},
      {QObject::tr("Orange"), QColor("#ffa847"), QColor("#000000")},
      {QObject::tr("Light Orange"), QColor("#ffe6c7"), QColor("#000000")},
      {QObject::tr("Pale Orange"), QColor("#fff3e4"), QColor("#000000")},
      {QObject::tr("Yellow"), QColor("#fad165"), QColor("#000000")},
      {QObject::tr("Light Yellow"), QColor("#fef1d1"), QColor("#000000")},
      {QObject::tr("Pale Yellow"), QColor("#fffae8"), QColor("#000000")},
      {QObject::tr("Green"), QColor("#16a765"), QColor("#ffffff")},
      {QObject::tr("Light Green"), QColor("#b9e4d0"), QColor("#000000")},
      {QObject::tr("Pale Green"), QColor("#e2f5ea"), QColor("#000000")},
      {QObject::tr("Teal"), QColor("#43d692"), QColor("#000000")},
      {QObject::tr("Light Teal"), QColor("#c6f3de"), QColor("#000000")},
      {QObject::tr("Pale Teal"), QColor("#e8fbf3"), QColor("#000000")},
      {QObject::tr("Blue"), QColor("#4a86e8"), QColor("#ffffff")},
      {QObject::tr("Light Blue"), QColor("#c9daf8"), QColor("#000000")},
      {QObject::tr("Pale Blue"), QColor("#eaf1fd"), QColor("#000000")},
      {QObject::tr("Purple"), QColor("#a479e2"), QColor("#ffffff")},
      {QObject::tr("Light Purple"), QColor("#e4d7f5"), QColor("#000000")},
      {QObject::tr("Pink"), QColor("#f691b3"), QColor("#000000")},
      {QObject::tr("Light Pink"), QColor("#fcdee8"), QColor("#000000")},
  };
  return presets;
}

// 背景色・文字色の組み合わせをすばやく選べるプリセットコンボボックスを作る。
// 選んだ瞬間にonPickedが呼ばれ、先頭の項目は「未選択」のプレースホルダとして機能する
QComboBox* buildColorPresetCombo(
    QWidget* parent,
    std::function<void(const QColor&, const QColor&)> onPicked) {
  QComboBox* combo = new QComboBox(parent);
  combo->addItem(QObject::tr("(Color Preset...)"));
  for (auto& preset : colorPresets()) {
    int idx = combo->count();
    combo->addItem(preset.name);
    combo->setItemData(idx, QBrush(preset.bgColor), Qt::BackgroundRole);
    combo->setItemData(idx, QBrush(preset.txtColor), Qt::ForegroundRole);
  }
  QObject::connect(
      combo, QOverload<int>::of(&QComboBox::activated), parent,
      [combo, onPicked](int idx) {
        if (idx <= 0 || idx > colorPresets().size()) return;
        const ColorPreset& preset = colorPresets().at(idx - 1);
        onPicked(preset.bgColor, preset.txtColor);
        combo->setCurrentIndex(
            0);  // 選択後はプレースホルダに戻す（プリセットは一回きりの適用操作のため）
      });
  return combo;
}

// WorkflowManager::getSceneInfoTagsRichText()と同じ見た目になるようにプレビュー用のHTMLを組み立てる
QString sceneInfoTagPreviewHtml(const SceneInfoTagDisplayProperties& prop) {
  if (prop.labelTxt.isEmpty()) return QString();
  return QString(
             "&nbsp;<span style=\"font-weight:600; color:%1; "
             "background-color:%2;\">&nbsp;%3&nbsp;</span>&nbsp;")
      .arg(prop.txtColor.name())
      .arg(prop.bgColor.name())
      .arg(prop.labelTxt);
}

};  // namespace

InfoTagsPage::InfoTagsPage(QWidget* parent)
    : QWidget(parent), m_detailContainer(nullptr), m_currentIndex(-1) {
  m_tagList = new QTableWidget(0, 2, this);
  m_tagList->setHorizontalHeaderLabels({tr("ID Name"), tr("Type")});
  m_tagList->verticalHeader()->setVisible(false);
  m_tagList->horizontalHeader()->setSectionResizeMode(InfoTagListColumn_IdName,
                                                      QHeaderView::Stretch);
  m_tagList->horizontalHeader()->setSectionResizeMode(
      InfoTagListColumn_Type, QHeaderView::ResizeToContents);
  m_tagList->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_tagList->setSelectionMode(QAbstractItemView::SingleSelection);
  m_tagList->setEditTriggers(
      QAbstractItemView::
          NoEditTriggers);  // 一覧では編集せず、右側の詳細パネルで編集する
  m_tagList->setFixedWidth(220);

  QPushButton* addButton    = new QPushButton(tr("Add"), this);
  QPushButton* removeButton = new QPushButton(tr("Remove"), this);

  // 詳細パネルの土台。中身（m_detailContainer）は選択されたタグに応じて都度作り直す
  QWidget* detailPanelHost = new QWidget(this);
  m_detailPanelLayout      = new QVBoxLayout(detailPanelHost);
  m_detailPanelLayout->setContentsMargins(10, 10, 10, 10);
  m_detailPanelLayout->setSpacing(10);

  QHBoxLayout* mainLay = new QHBoxLayout();
  mainLay->setContentsMargins(10, 10, 10, 10);
  {
    QVBoxLayout* leftLay = new QVBoxLayout();
    leftLay->setContentsMargins(0, 0, 0, 0);
    {
      leftLay->addWidget(m_tagList, 1);

      QHBoxLayout* buttonsLay = new QHBoxLayout();
      buttonsLay->setContentsMargins(0, 0, 0, 0);
      {
        buttonsLay->addStretch(1);
        buttonsLay->addWidget(addButton, 0);
        buttonsLay->addWidget(removeButton, 0);
      }
      leftLay->addLayout(buttonsLay, 0);
    }
    mainLay->addLayout(leftLay, 0);

    mainLay->addWidget(detailPanelHost, 1);
  }
  setLayout(mainLay);

  connect(addButton, SIGNAL(clicked()), this, SLOT(onAddButtonClicked()));
  connect(removeButton, SIGNAL(clicked()), this, SLOT(onRemoveButtonClicked()));
  connect(m_tagList, &QTableWidget::currentCellChanged, this,
          [this](int currentRow, int, int, int) {
            onCurrentRowChanged(currentRow);
          });
}

void InfoTagsPage::refresh() {
  m_tagList->blockSignals(true);
  m_tagList->setRowCount(0);
  for (auto tag : tmpSettings.sceneInfoTags) {
    int row = m_tagList->rowCount();
    m_tagList->insertRow(row);
    m_tagList->setItem(row, InfoTagListColumn_IdName,
                       new QTableWidgetItem(tag->idName()));
    m_tagList->setItem(
        row, InfoTagListColumn_Type,
        new QTableWidgetItem(sceneInfoTagTypeDisplayName(tag->type())));
  }
  if (m_tagList->rowCount() > 0) m_tagList->setCurrentCell(0, 0);
  m_tagList->blockSignals(false);

  // selectRow()等のシグナル発火に頼らず、常に明示的に詳細パネルを作り直す
  // （Qt内部のカレントインデックスが既に0のままになっていると、シグナルが発火せず
  //   古いセッションのウィジェットがクリアされないまま残ってしまうため）
  m_currentIndex = (m_tagList->rowCount() > 0) ? 0 : -1;
  rebuildDetailPanel();
}

void InfoTagsPage::updateListRow(int index) {
  if (index < 0 || index >= tmpSettings.sceneInfoTags.size()) return;
  SceneInfoTag* tag = tmpSettings.sceneInfoTags.at(index);
  if (auto* item = m_tagList->item(index, InfoTagListColumn_IdName))
    item->setText(tag->idName());
  if (auto* item = m_tagList->item(index, InfoTagListColumn_Type))
    item->setText(sceneInfoTagTypeDisplayName(tag->type()));
}

void InfoTagsPage::clearDetailPanel() {
  if (m_detailContainer) {
    // ウィジェットごと同期的に破棄する（deleteLater()は次のイベントループまで実行が遅延されるため、
    // refresh()の直後にexec()するケース等では削除前に次のパネルが作られ、二重表示の原因になっていた）
    delete m_detailContainer;
    m_detailContainer = nullptr;
  }
}

void InfoTagsPage::onAddButtonClicked() {
  QString newIdName = tr("NEW_TAG");
  int suffix        = 1;
  auto idNameExists = [](const QString& name) {
    for (auto tag : tmpSettings.sceneInfoTags)
      if (tag->idName() == name) return true;
    return false;
  };
  while (idNameExists(newIdName)) newIdName = tr("NEW_TAG_%1").arg(++suffix);

  tmpSettings.sceneInfoTags.append(
      new BoolSceneInfoTag(newIdName, true, defaultDisplayProperties()));
  markDirty();
  refresh();
  m_tagList->selectRow(m_tagList->rowCount() - 1);
}

void InfoTagsPage::onRemoveButtonClicked() {
  QSet<int> rows;
  for (auto* item : m_tagList->selectedItems()) rows.insert(item->row());
  if (rows.isEmpty()) return;

  QList<int> sortedRows = rows.values();
  std::sort(sortedRows.begin(), sortedRows.end(), std::greater<int>());
  for (int row : sortedRows) {
    delete tmpSettings.sceneInfoTags.at(row);
    tmpSettings.sceneInfoTags.removeAt(row);
  }

  markDirty();
  refresh();
}

void InfoTagsPage::onCurrentRowChanged(int currentRow) {
  m_currentIndex = currentRow;
  rebuildDetailPanel();
}

void InfoTagsPage::rebuildDetailPanel() {
  clearDetailPanel();

  m_detailContainer         = new QWidget();
  QVBoxLayout* detailLayout = new QVBoxLayout(m_detailContainer);
  detailLayout->setContentsMargins(0, 0, 0, 0);
  detailLayout->setSpacing(10);
  m_detailPanelLayout->addWidget(m_detailContainer);

  if (m_currentIndex < 0 ||
      m_currentIndex >= tmpSettings.sceneInfoTags.size()) {
    detailLayout->addWidget(
        new QLabel(tr("No scene info tag selected."), this));
    detailLayout->addStretch(1);
    return;
  }

  SceneInfoTag* tag = tmpSettings.sceneInfoTags.at(m_currentIndex);
  int index         = m_currentIndex;  // ラムダのキャプチャ用

  // プレビュー：実際のカット一覧での表示（WorkflowManager::getSceneInfoTagsRichText）を模したもの
  detailLayout->addWidget(new QLabel(tr("Preview:"), this));
  QLabel* previewLabel = new QLabel(this);
  previewLabel->setTextFormat(Qt::RichText);
  previewLabel->setWordWrap(true);
  previewLabel->setStyleSheet(
      "QLabel { background-color: white; border: 1px solid gray; padding: 4px; "
      "}");
  detailLayout->addWidget(previewLabel);

  auto updatePreview = [this, index, previewLabel]() {
    if (index < 0 || index >= tmpSettings.sceneInfoTags.size()) return;
    SceneInfoTag* t = tmpSettings.sceneInfoTags[index];

    QString html;
    switch (t->type()) {
    case InfoTagType_Bool:
      html += sceneInfoTagPreviewHtml(t->displayProperties());
      break;
    case InfoTagType_String:
      // 文字列情報の表示部分はダミーのテキストで代用する
      html += sceneInfoTagPreviewHtml(t->displayProperties(QString("####")));
      break;
    case InfoTagType_StringList:
      html +=
          sceneInfoTagPreviewHtml(t->displayProperties(QStringList{"####"}));
      break;
    case InfoTagType_Enum: {
      EnumSceneInfoTag* enumTag = static_cast<EnumSceneInfoTag*>(t);
      for (auto value : enumTag->valueList())
        html += sceneInfoTagPreviewHtml(enumTag->displayProperties(value));
      break;
    }
    }
    previewLabel->setText(html.isEmpty() ? tr("(nothing is displayed)") : html);
  };
  updatePreview();

  QFormLayout* formLay = newProjectFormLayout();
  formLay->setContentsMargins(0, 0, 0, 0);

  QLineEdit* idNameEdit = new QLineEdit(tag->idName());
  connect(idNameEdit, &QLineEdit::editingFinished, this,
          [this, index, idNameEdit]() {
            if (index < 0 || index >= tmpSettings.sceneInfoTags.size()) return;
            tmpSettings.sceneInfoTags[index]->setIdName(idNameEdit->text());
            updateListRow(index);
            markDirty();
          });
  formLay->addRow(tr("ID Name"), idNameEdit);

  QCheckBox* canManuallySetCheck = new QCheckBox();
  canManuallySetCheck->setChecked(tag->canManuallySet());
  connect(canManuallySetCheck, &QCheckBox::toggled, this,
          [this, index](bool checked) {
            if (index < 0 || index >= tmpSettings.sceneInfoTags.size()) return;
            tmpSettings.sceneInfoTags[index]->setCanManuallySet(checked);
            markDirty();
          });
  formLay->addRow(tr("Can Manually Set"), canManuallySetCheck);

  QComboBox* typeCombo = new QComboBox();
  typeCombo->addItem(sceneInfoTagTypeDisplayName(InfoTagType_Bool),
                     InfoTagType_Bool);
  typeCombo->addItem(sceneInfoTagTypeDisplayName(InfoTagType_Enum),
                     InfoTagType_Enum);
  typeCombo->addItem(sceneInfoTagTypeDisplayName(InfoTagType_String),
                     InfoTagType_String);
  typeCombo->addItem(sceneInfoTagTypeDisplayName(InfoTagType_StringList),
                     InfoTagType_StringList);
  typeCombo->setCurrentIndex(typeCombo->findData(tag->type()));
  connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this, index, typeCombo](int) {
            if (index < 0 || index >= tmpSettings.sceneInfoTags.size()) return;
            SceneInfoTagType newType =
                (SceneInfoTagType)typeCombo->currentData().toInt();

            SceneInfoTag* old = tmpSettings.sceneInfoTags[index];
            if (old->type() == newType) return;

            QString idName            = old->idName();
            bool canManuallySet       = old->canManuallySet();
            SceneInfoTag* replacement = nullptr;
            switch (newType) {
            case InfoTagType_Bool:
              replacement = new BoolSceneInfoTag(idName, canManuallySet,
                                                 defaultDisplayProperties());
              break;
            case InfoTagType_Enum:
              replacement = new EnumSceneInfoTag(idName, canManuallySet);
              break;
            case InfoTagType_String:
              replacement = new StringSceneInfoTag(idName, canManuallySet,
                                                   defaultDisplayProperties());
              break;
            case InfoTagType_StringList:
              replacement = new StringListSceneInfoTag(
                  idName, canManuallySet, defaultDisplayProperties());
              break;
            }
            delete old;
            tmpSettings.sceneInfoTags[index] = replacement;

            updateListRow(index);
            markDirty();
            // typeComboはこれから破棄されるm_detailContainerの子であり、currentIndexChangedの
            // シグナル処理はコンボボックス自身の後処理がまだ続いている途中である。
            // ここで同期的にrebuildDetailPanel()（=typeCombo自身を含むコンテナのdelete）を行うと
            // コンボボックスの内部処理が解放済みメモリにアクセスしクラッシュする
            // （QPushButtonのclickedと異なりQComboBoxのcurrentIndexChangedは
            //   送信元の破棄に対して安全ではない）。イベントループに戻ってから実行する。
            QTimer::singleShot(0, this, [this]() { rebuildDetailPanel(); });
          });
  formLay->addRow(tr("Type"), typeCombo);

  detailLayout->addLayout(formLay);

  // タイプ別のプロパティ編集UI
  switch (tag->type()) {
  case InfoTagType_Bool:
  case InfoTagType_String:
  case InfoTagType_StringList: {
    SceneInfoTagDisplayProperties prop = tag->displayProperties();

    QFormLayout* propLay = newProjectFormLayout();
    propLay->setContentsMargins(0, 0, 0, 0);

    QLineEdit* labelEdit       = new QLineEdit(prop.labelTxt);
    ColorButton* bgColorButton = new ColorButton();
    bgColorButton->setColor(prop.bgColor);
    ColorButton* txtColorButton = new ColorButton();
    txtColorButton->setColor(prop.txtColor);

    auto applyDisplayProperties = [this, index, labelEdit, bgColorButton,
                                   txtColorButton, updatePreview]() {
      if (index < 0 || index >= tmpSettings.sceneInfoTags.size()) return;
      SceneInfoTagDisplayProperties newProp = {
          labelEdit->text(), bgColorButton->color(), txtColorButton->color()};
      SceneInfoTag* t = tmpSettings.sceneInfoTags[index];
      switch (t->type()) {
      case InfoTagType_Bool:
        static_cast<BoolSceneInfoTag*>(t)->setDisplayProperties(newProp);
        break;
      case InfoTagType_String:
        static_cast<StringSceneInfoTag*>(t)->setDisplayProperties(newProp);
        break;
      case InfoTagType_StringList:
        static_cast<StringListSceneInfoTag*>(t)->setDisplayProperties(newProp);
        break;
      default:
        break;
      }
      updatePreview();
      markDirty();
    };
    connect(labelEdit, &QLineEdit::editingFinished, this,
            applyDisplayProperties);
    connect(bgColorButton, &ColorButton::colorChanged, this,
            applyDisplayProperties);
    connect(txtColorButton, &ColorButton::colorChanged, this,
            applyDisplayProperties);

    QComboBox* colorPresetCombo = buildColorPresetCombo(
        this, [bgColorButton, txtColorButton, applyDisplayProperties](
                  const QColor& bg, const QColor& txt) {
          bgColorButton->setColor(bg);
          txtColorButton->setColor(txt);
          applyDisplayProperties();
        });

    propLay->addRow(tr("Label"), labelEdit);
    propLay->addRow(tr("Color Preset"), colorPresetCombo);
    propLay->addRow(tr("Background Color"), bgColorButton);
    propLay->addRow(tr("Text Color"), txtColorButton);
    detailLayout->addLayout(propLay);
    detailLayout->addStretch(1);
    break;
  }
  case InfoTagType_Enum: {
    EnumSceneInfoTag* enumTag = static_cast<EnumSceneInfoTag*>(tag);

    QHBoxLayout* valuesHeaderLay = new QHBoxLayout();
    valuesHeaderLay->setContentsMargins(0, 0, 0, 0);
    valuesHeaderLay->addWidget(new QLabel(tr("Values:"), this), 0);
    valuesHeaderLay->addStretch(1);
    QPushButton* addValueButton    = new QPushButton(tr("Add Value"), this);
    QPushButton* removeValueButton = new QPushButton(tr("Remove Value"), this);
    valuesHeaderLay->addWidget(addValueButton, 0);
    valuesHeaderLay->addWidget(removeValueButton, 0);
    detailLayout->addLayout(valuesHeaderLay);

    QTableWidget* valueTable =
        new QTableWidget((int)enumTag->valueList().size(), 5, this);
    valueTable->setHorizontalHeaderLabels(
        {tr("Value"), tr("Label"), tr("Background"), tr("Text"), tr("Preset")});
    valueTable->verticalHeader()->setVisible(false);
    valueTable->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::ResizeToContents);
    valueTable->horizontalHeader()->setSectionResizeMode(1,
                                                         QHeaderView::Stretch);
    valueTable->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::ResizeToContents);
    valueTable->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::ResizeToContents);
    valueTable->horizontalHeader()->setSectionResizeMode(
        4, QHeaderView::ResizeToContents);
    valueTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    valueTable->setSelectionMode(QAbstractItemView::ExtendedSelection);

    int row = 0;
    for (auto value : enumTag->valueList()) {
      SceneInfoTagDisplayProperties valueProp =
          enumTag->displayProperties(value);

      QTableWidgetItem* valueItem = new QTableWidgetItem(value);
      valueItem->setData(Qt::UserRole,
                         value);  // 現在のキー（変更前の値）を保持しておく
      valueTable->setItem(row, 0, valueItem);
      valueTable->setItem(row, 1, new QTableWidgetItem(valueProp.labelTxt));

      ColorButton* bgButton = new ColorButton();
      bgButton->setColor(valueProp.bgColor);
      valueTable->setCellWidget(row, 2, bgButton);

      ColorButton* txtButton = new ColorButton();
      txtButton->setColor(valueProp.txtColor);
      valueTable->setCellWidget(row, 3, txtButton);

      auto applyValueColors = [this, index, valueTable, row, updatePreview,
                               bgButton, txtButton]() {
        if (index < 0 || index >= tmpSettings.sceneInfoTags.size()) return;
        EnumSceneInfoTag* t =
            static_cast<EnumSceneInfoTag*>(tmpSettings.sceneInfoTags[index]);
        QString value = valueTable->item(row, 0)->data(Qt::UserRole).toString();
        SceneInfoTagDisplayProperties prop = t->displayProperties(value);
        prop.bgColor                       = bgButton->color();
        prop.txtColor                      = txtButton->color();
        t->addItem(value, prop);
        updatePreview();
        markDirty();
      };
      connect(bgButton, &ColorButton::colorChanged, this, applyValueColors);
      connect(txtButton, &ColorButton::colorChanged, this, applyValueColors);

      QComboBox* valuePresetCombo =
          buildColorPresetCombo(this, [bgButton, txtButton, applyValueColors](
                                          const QColor& bg, const QColor& txt) {
            bgButton->setColor(bg);
            txtButton->setColor(txt);
            applyValueColors();
          });
      valueTable->setCellWidget(row, 4, valuePresetCombo);

      row++;
    }

    connect(
        valueTable, &QTableWidget::itemChanged, this,
        [this, index, valueTable, updatePreview](QTableWidgetItem* item) {
          if (index < 0 || index >= tmpSettings.sceneInfoTags.size()) return;
          EnumSceneInfoTag* t =
              static_cast<EnumSceneInfoTag*>(tmpSettings.sceneInfoTags[index]);
          int row = item->row();

          if (item->column() == 0) {
            // 値（キー）の変更：古いキーの内容を新しいキーへ引き継ぐ
            QString oldValue = item->data(Qt::UserRole).toString();
            QString newValue = item->text();
            if (newValue.isEmpty() ||
                (newValue != oldValue && t->valueList().contains(newValue))) {
              // 空、または他の値と重複する場合は元に戻す
              valueTable->blockSignals(true);
              item->setText(oldValue);
              valueTable->blockSignals(false);
              return;
            }
            if (newValue == oldValue) return;
            SceneInfoTagDisplayProperties prop = t->displayProperties(oldValue);
            t->removeItem(oldValue);
            t->addItem(newValue, prop);
            item->setData(Qt::UserRole, newValue);
          } else if (item->column() == 1) {
            QString value =
                valueTable->item(row, 0)->data(Qt::UserRole).toString();
            SceneInfoTagDisplayProperties prop = t->displayProperties(value);
            prop.labelTxt                      = item->text();
            t->addItem(value, prop);
          }
          updatePreview();
          markDirty();
        });

    connect(addValueButton, &QPushButton::clicked, this, [this, index]() {
      if (index < 0 || index >= tmpSettings.sceneInfoTags.size()) return;
      EnumSceneInfoTag* t =
          static_cast<EnumSceneInfoTag*>(tmpSettings.sceneInfoTags[index]);
      QString newValue = tr("NEW_VALUE");
      int suffix       = 1;
      while (t->valueList().contains(newValue))
        newValue = tr("NEW_VALUE_%1").arg(++suffix);
      t->addItem(newValue, defaultDisplayProperties());
      markDirty();
      rebuildDetailPanel();
    });
    connect(removeValueButton, &QPushButton::clicked, this,
            [this, index, valueTable]() {
              if (index < 0 || index >= tmpSettings.sceneInfoTags.size())
                return;
              EnumSceneInfoTag* t = static_cast<EnumSceneInfoTag*>(
                  tmpSettings.sceneInfoTags[index]);
              QSet<QString> valuesToRemove;
              for (auto* item : valueTable->selectedItems())
                valuesToRemove.insert(valueTable->item(item->row(), 0)
                                          ->data(Qt::UserRole)
                                          .toString());
              for (auto& value : valuesToRemove) t->removeItem(value);
              markDirty();
              rebuildDetailPanel();
            });

    detailLayout->addWidget(valueTable, 1);
    break;
  }
  }
}

//--------------------------------

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

// WorkflowのIDはユーザーが編集できない内部管理用の値になったため、Name（重複しうる）に
// カッコ書きでIDを添えて一覧・コンボボックスなどで一意に見分けられるようにする
QString workflowDisplayLabel(const Workflow& wf) {
  return QObject::tr("%1 (#%2)").arg(wf.name).arg((uint)wf.workflowType);
}

// ActionStepの現在値を読み書きするためのアクセサ。
// 直接参照(QList<QVariant>&)を保持すると、他の箇所でのAdd/Remove操作によってQListが
// 再配置された際にダングリングになる危険があるため、常にインデックス経由で
// tmpSettingsから辿り直す。Step_ConditionalBranchByInfo/FileExistenceの入れ子ActionStepも
// 同じ仕組みで（親のeditorを介して）合成することで、任意の深さの入れ子に対応する。
typedef std::function<void(std::function<void(ActionStep&)>)> StepEditor;

ActionStep readStep(const StepEditor& editor) {
  ActionStep result{Step_Rename, {}};
  editor([&result](ActionStep& s) { result = s; });
  return result;
}

StepEditor makeTopStepEditor(int wfIndex, int protoIndex, int actionIndex,
                             int stepIndex) {
  return [wfIndex, protoIndex, actionIndex,
          stepIndex](std::function<void(ActionStep&)> fn) {
    if (wfIndex < 0 || wfIndex >= tmpSettings.workflows.size()) return;
    QList<WorkflowProtocol>& protocols =
        tmpSettings.workflows[wfIndex].protocols;
    if (protoIndex < 0 || protoIndex >= protocols.size()) return;
    QList<PPM_Action>& actions = protocols[protoIndex].actions;
    if (actionIndex < 0 || actionIndex >= actions.size()) return;
    QList<ActionStep>& steps = actions[actionIndex].steps;
    if (stepIndex < 0 || stepIndex >= steps.size()) return;
    fn(steps[stepIndex]);
  };
}

QVariant packActionStep(ActionStepType type, const QList<QVariant>& params) {
  QVariantList packed;
  packed.append((int)type);
  packed.append(QVariant(params));
  return packed;
}

// 空のリスト（＝「None：何もしない」。実行時はcreateExecutorがスキップし、JSON保存時はキー自体が
// 省略される表現）はStep_Unidentifiedとして返し、UI上で「None」として扱えるようにする
ActionStep unpackActionStep(const QVariant& packed) {
  QVariantList list = packed.toList();
  if (list.size() < 2) return ActionStep{Step_Unidentified, {}};
  return ActionStep{(ActionStepType)list[0].toInt(), list[1].toList()};
}

// 入れ子のActionStep（Step_ConditionalBranchByInfo/FileExistenceのparams中に埋め込まれたステップ）
// を編集するeditorを、親のeditorから合成する
StepEditor makeNestedStepEditor(StepEditor parentEditor, int paramIndex) {
  return [parentEditor, paramIndex](std::function<void(ActionStep&)> fn) {
    parentEditor([paramIndex, fn](ActionStep& parent) {
      while (parent.params.size() <= paramIndex)
        parent.params.append(
            QVariant(QVariantList()));  // 不足分は「None」で埋める
      ActionStep nested = unpackActionStep(parent.params[paramIndex]);
      fn(nested);
      // 「None」は空のリストとして書き戻す（toJson/実行時のスキップ判定と揃った表現）
      if (nested.type == Step_Unidentified)
        parent.params[paramIndex] = QVariant(QVariantList());
      else
        parent.params[paramIndex] = packActionStep(nested.type, nested.params);
    });
  };
}

// actionconfirmationdialog.cpp内のIconWidget::paintEventと同じ対応表
QIcon iconForActionStepType(ActionStepType type) {
  QString pmPath;
  switch (type) {
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
    pmPath = ":Resources/check_ok64.svg";
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
  return QIcon(pmPath);
}

QList<QVariant> defaultParamsForStepType(ActionStepType type) {
  switch (type) {
  case Step_Rename:
  case Step_Copy:
    return {};
  case Step_RenameFile:
  case Step_CopyFile:
    return {QString()};
  case Step_CopyFolderContents:
    return {QString("[ALL]"), QString()};
  case Step_MoveFlowGroup:
  case Step_StartBranchFlow:
  case Step_EndBranchFlow:
    return {(uint)0};
  case Step_CheckExistence:
    return {QString("[EXISTENCE]")};
  case Step_AskDelete:
    return {false};
  case Step_CreateCueFile:
    return {QString()};
  case Step_SetSceneInfo:
    return {QString(), QString()};
  // 条件が「偽」のときのステップは既定で「None」（空のリスト＝何もしない）とする
  case Step_ConditionalBranchByInfo:
    return {QString(), QString(), packActionStep(Step_Rename, {}),
            QVariant(QVariantList())};
  case Step_ConditionalBranchByFileExistence:
    return {QString(), packActionStep(Step_Rename, {}),
            QVariant(QVariantList())};
  case Step_InputGoogleSpreadsheet:
    return {QString(), QString(), QString()};
  case Step_DougaRetakeRouting:
    return {};
  case Step_ReleaseProgress:
    return {(uint)0};
  case Step_AddProgress:
    return {
        (uint)
            Wf_Undefined};  // 追加する工程は、ユーザーが選ぶまで未設定にしておく
  case Step_CreateEmptyFolder:
    return {QString()};
  case Step_ZipFolder:
    return {QString(), QString()};
  case Step_NotifyGoogleChat:
    // 投稿先, メッセージ, 自由記入欄の有無, キャンセル時に通知するか,
    // キャンセルメッセージ, スレッドキー
    return {QString(), QString(), false, true, QString(), QString()};
  }
  return {};
}

void ensureParamSize(ActionStep& s, int index) {
  while (s.params.size() <= index) s.params.append(QVariant());
}

// Step_ReleaseProgress/Step_AddProgressが削除対象のWorkflowを参照していれば、Wf_Undefinedにリセットする
// （Step_ConditionalBranchByInfo/FileExistenceの入れ子ActionStepも再帰的に辿る）。
// 見つかった場合はtrueを返す
bool resetReleaseProgressReferences(QList<ActionStep>& steps,
                                    WorkflowType deletedId) {
  bool foundAny = false;
  for (auto& step : steps) {
    if (step.type == Step_ReleaseProgress || step.type == Step_AddProgress) {
      if (!step.params.isEmpty() &&
          step.params.at(0).toUInt() == (uint)deletedId) {
        step.params[0] = (uint)Wf_Undefined;
        foundAny       = true;
      }
    } else if (step.type == Step_ConditionalBranchByInfo &&
               step.params.size() >= 4) {
      for (int i = 2; i <= 3; i++) {
        ActionStep nested            = unpackActionStep(step.params.at(i));
        QList<ActionStep> nestedList = {nested};
        if (resetReleaseProgressReferences(nestedList, deletedId)) {
          step.params[i] =
              packActionStep(nestedList.at(0).type, nestedList.at(0).params);
          foundAny = true;
        }
      }
    } else if (step.type == Step_ConditionalBranchByFileExistence &&
               step.params.size() >= 3) {
      for (int i = 1; i <= 2; i++) {
        ActionStep nested            = unpackActionStep(step.params.at(i));
        QList<ActionStep> nestedList = {nested};
        if (resetReleaseProgressReferences(nestedList, deletedId)) {
          step.params[i] =
              packActionStep(nestedList.at(0).type, nestedList.at(0).params);
          foundAny = true;
        }
      }
    }
  }
  return foundAny;
}

// Step_NotifyGoogleChatの投稿先名がoldNameのものをnewNameへ付け替える
// （Step_ConditionalBranchByInfo/FileExistenceの入れ子ActionStepも再帰的に辿る）
void renameChatDestinationInSteps(QList<ActionStep>& steps,
                                  const QString& oldName,
                                  const QString& newName) {
  for (auto& step : steps) {
    if (step.type == Step_NotifyGoogleChat) {
      if (!step.params.isEmpty() && step.params.at(0).toString() == oldName)
        step.params[0] = newName;
    } else if (step.type == Step_ConditionalBranchByInfo &&
               step.params.size() >= 4) {
      for (int i = 2; i <= 3; i++) {
        ActionStep nested            = unpackActionStep(step.params.at(i));
        QList<ActionStep> nestedList = {nested};
        renameChatDestinationInSteps(nestedList, oldName, newName);
        if (nested.type != Step_Unidentified)
          step.params[i] =
              packActionStep(nestedList.at(0).type, nestedList.at(0).params);
      }
    } else if (step.type == Step_ConditionalBranchByFileExistence &&
               step.params.size() >= 3) {
      for (int i = 1; i <= 2; i++) {
        ActionStep nested            = unpackActionStep(step.params.at(i));
        QList<ActionStep> nestedList = {nested};
        renameChatDestinationInSteps(nestedList, oldName, newName);
        if (nested.type != Step_Unidentified)
          step.params[i] =
              packActionStep(nestedList.at(0).type, nestedList.at(0).params);
      }
    }
  }
}

// 工程グループを指定するステップ（工程グループを移動／傍流を開始／傍流を終了）の
// 参照先がoldIdのものをnewIdへ付け替える
// （Step_ConditionalBranchByInfo/FileExistenceの入れ子ActionStepも再帰的に辿る）
void remapFlowGroupIdInSteps(QList<ActionStep>& steps, FlowGroupId oldId,
                             FlowGroupId newId) {
  for (auto& step : steps) {
    if (step.type == Step_MoveFlowGroup || step.type == Step_StartBranchFlow ||
        step.type == Step_EndBranchFlow) {
      if (!step.params.isEmpty() &&
          (FlowGroupId)step.params.at(0).toUInt() == oldId)
        step.params[0] = (uint)newId;
    } else if (step.type == Step_ConditionalBranchByInfo &&
               step.params.size() >= 4) {
      for (int i = 2; i <= 3; i++) {
        ActionStep nested            = unpackActionStep(step.params.at(i));
        QList<ActionStep> nestedList = {nested};
        remapFlowGroupIdInSteps(nestedList, oldId, newId);
        if (nested.type != Step_Unidentified)
          step.params[i] =
              packActionStep(nestedList.at(0).type, nestedList.at(0).params);
      }
    } else if (step.type == Step_ConditionalBranchByFileExistence &&
               step.params.size() >= 3) {
      for (int i = 1; i <= 2; i++) {
        ActionStep nested            = unpackActionStep(step.params.at(i));
        QList<ActionStep> nestedList = {nested};
        remapFlowGroupIdInSteps(nestedList, oldId, newId);
        if (nested.type != Step_Unidentified)
          step.params[i] =
              packActionStep(nestedList.at(0).type, nestedList.at(0).params);
      }
    }
  }
}

// ids のいずれかの工程グループを指定しているステップの数を数える
// （入れ子ActionStepも再帰的に辿る）
int countFlowGroupIdInSteps(const QList<ActionStep>& steps,
                            const QSet<FlowGroupId>& ids) {
  int count = 0;
  for (auto& step : steps) {
    if (step.type == Step_MoveFlowGroup || step.type == Step_StartBranchFlow ||
        step.type == Step_EndBranchFlow) {
      if (!step.params.isEmpty() &&
          ids.contains((FlowGroupId)step.params.at(0).toUInt()))
        count++;
    } else if (step.type == Step_ConditionalBranchByInfo &&
               step.params.size() >= 4) {
      for (int i = 2; i <= 3; i++)
        count += countFlowGroupIdInSteps({unpackActionStep(step.params.at(i))},
                                         ids);
    } else if (step.type == Step_ConditionalBranchByFileExistence &&
               step.params.size() >= 3) {
      for (int i = 1; i <= 2; i++)
        count += countFlowGroupIdInSteps({unpackActionStep(step.params.at(i))},
                                         ids);
    }
  }
  return count;
}

struct RemovedWorkflowReferenceInfo {
  int nextFlowsCount       = 0;
  int releaseProgressCount = 0;
};

// 削除されるWorkflowへの参照を、プロジェクト内の全Workflowから取り除く
RemovedWorkflowReferenceInfo removeWorkflowReferences(WorkflowType deletedId) {
  RemovedWorkflowReferenceInfo info;
  for (auto& wf : tmpSettings.workflows) {
    for (auto& protocol : wf.protocols) {
      for (auto& action : protocol.actions) {
        info.nextFlowsCount += action.nextFlows.removeAll(deletedId);
        if (resetReleaseProgressReferences(action.steps, deletedId))
          info.releaseProgressCount++;
      }
    }
  }
  return info;
}

// editの右に候補選択ボタンを付けた行を返す。ボタンを押すとsuggestionProviderの候補を
// メニューで表示し、選んだ候補をapplyに渡す
// （後工程のCondition
// Path等、他のデータから導出できる候補をワンクリックで入力できるようにする）
QWidget* wrapWithSuggestButton(QWidget* parentWidget, QWidget* edit,
                               std::function<QStringList()> suggestionProvider,
                               std::function<void(const QString&)> apply) {
  QWidget* rowWidget  = new QWidget(parentWidget);
  QHBoxLayout* rowLay = new QHBoxLayout(rowWidget);
  rowLay->setContentsMargins(0, 0, 0, 0);
  rowLay->addWidget(edit, 1);
  QPushButton* suggestButton =
      new QPushButton(QObject::tr("Suggest..."), rowWidget);
  suggestButton->setToolTip(
      QObject::tr("Pick from the condition path(s) of the next workflow(s)."));
  QObject::connect(suggestButton, &QPushButton::clicked, parentWidget,
                   [suggestButton, suggestionProvider, apply]() {
                     QStringList candidates = suggestionProvider();
                     if (candidates.isEmpty()) {
                       QMenu emptyMenu;
                       emptyMenu.addAction(QObject::tr("(No candidates)"))
                           ->setEnabled(false);
                       emptyMenu.exec(suggestButton->mapToGlobal(
                           QPoint(0, suggestButton->height())));
                       return;
                     }
                     QMenu menu;
                     for (auto& candidate : candidates)
                       menu.addAction(candidate);
                     QAction* chosen = menu.exec(suggestButton->mapToGlobal(
                         QPoint(0, suggestButton->height())));
                     if (!chosen) return;
                     apply(chosen->text());
                   });
  rowLay->addWidget(suggestButton, 0);
  return rowWidget;
}

// suggestionProviderを指定すると、フィールド右に候補選択ボタンを追加する
void buildStepStringField(
    QVBoxLayout* parentLay, QWidget* parentWidget, const QString& label,
    StepEditor editor, int paramIndex, std::function<void()> onChanged,
    const QString& tooltip                          = QString(),
    std::function<QStringList()> suggestionProvider = nullptr) {
  ActionStep step = readStep(editor);
  QString value   = (paramIndex < step.params.size())
                        ? step.params.at(paramIndex).toString()
                        : QString();

  QFormLayout* fl = newProjectFormLayout();
  fl->setContentsMargins(0, 0, 0, 0);
  QLineEdit* edit = new QLineEdit(value, parentWidget);
  if (!tooltip.isEmpty()) edit->setToolTip(tooltip);
  QObject::connect(edit, &QLineEdit::editingFinished, parentWidget,
                   [editor, paramIndex, edit, onChanged]() {
                     editor([paramIndex, edit](ActionStep& s) {
                       ensureParamSize(s, paramIndex);
                       s.params[paramIndex] = edit->text();
                     });
                     onChanged();
                   });

  if (suggestionProvider)
    fl->addRow(label, wrapWithSuggestButton(
                          parentWidget, edit, suggestionProvider,
                          [editor, paramIndex, edit,
                           onChanged](const QString& chosen) {
                            edit->setText(chosen);
                            editor([paramIndex, chosen](ActionStep& s) {
                              ensureParamSize(s, paramIndex);
                              s.params[paramIndex] = chosen;
                            });
                            onChanged();
                          }));
  else
    fl->addRow(label, edit);
  parentLay->addLayout(fl);
}

// MacroLineEditを使う文字列パラメータ編集フィールド。candidateProviderを省略した場合は
// MacroLineEditの既定の候補（Path Macros +
// [ROOT]等の組み込みマクロ）がそのまま使われる
void buildStepMacroLineField(
    QVBoxLayout* parentLay, QWidget* parentWidget, const QString& label,
    StepEditor editor, int paramIndex, std::function<void()> onChanged,
    std::function<QStringList()> candidateProvider  = nullptr,
    const QString& tooltip                          = QString(),
    std::function<QStringList()> suggestionProvider = nullptr) {
  ActionStep step = readStep(editor);
  QString value   = (paramIndex < step.params.size())
                        ? step.params.at(paramIndex).toString()
                        : QString();

  QFormLayout* fl = newProjectFormLayout();
  fl->setContentsMargins(0, 0, 0, 0);
  MacroLineEdit* edit = new MacroLineEdit(parentWidget);
  edit->setText(value);
  if (candidateProvider) edit->setMacroCandidateProvider(candidateProvider);
  if (!tooltip.isEmpty()) edit->setToolTip(tooltip);
  QObject::connect(edit, &MacroLineEdit::editingFinished, parentWidget,
                   [editor, paramIndex, edit, onChanged]() {
                     editor([paramIndex, edit](ActionStep& s) {
                       ensureParamSize(s, paramIndex);
                       s.params[paramIndex] = edit->text();
                     });
                     onChanged();
                   });
  if (suggestionProvider)
    fl->addRow(label, wrapWithSuggestButton(
                          parentWidget, edit, suggestionProvider,
                          [editor, paramIndex, edit,
                           onChanged](const QString& chosen) {
                            edit->setText(chosen);
                            editor([paramIndex, chosen](ActionStep& s) {
                              ensureParamSize(s, paramIndex);
                              s.params[paramIndex] = chosen;
                            });
                            onChanged();
                          }));
  else
    fl->addRow(label, edit);
  parentLay->addLayout(fl);
}

void buildStepBoolField(QVBoxLayout* parentLay, QWidget* parentWidget,
                        const QString& label, StepEditor editor, int paramIndex,
                        std::function<void()> onChanged,
                        const QString& tooltip = QString()) {
  ActionStep step = readStep(editor);
  bool value      = (paramIndex < step.params.size())
                        ? step.params.at(paramIndex).toBool()
                        : false;

  QFormLayout* fl = newProjectFormLayout();
  fl->setContentsMargins(0, 0, 0, 0);
  QCheckBox* check = new QCheckBox(parentWidget);
  check->setChecked(value);
  if (!tooltip.isEmpty()) check->setToolTip(tooltip);
  QObject::connect(check, &QCheckBox::toggled, parentWidget,
                   [editor, paramIndex, onChanged](bool checked) {
                     editor([paramIndex, checked](ActionStep& s) {
                       ensureParamSize(s, paramIndex);
                       s.params[paramIndex] = checked;
                     });
                     onChanged();
                   });
  fl->addRow(label, check);
  // チェックボックスだけでなく、左のラベルにもツールチップを表示する
  if (!tooltip.isEmpty()) {
    if (QWidget* labelWidget = fl->labelForField(check))
      labelWidget->setToolTip(tooltip);
  }
  parentLay->addLayout(fl);
}

// フォーカスを失ったときに入力内容を確定する複数行テキスト入力。
// 変更の通知（onChanged）がパラメータ編集フォーム全体の作り直しを伴うため、
// 1文字ごとのtextChangedではなく、フォーカスアウト時にだけ確定する
class CommitPlainTextEdit : public QPlainTextEdit {
  std::function<void()> m_onCommit;

public:
  CommitPlainTextEdit(QWidget* parent) : QPlainTextEdit(parent) {}
  void setCommitHandler(std::function<void()> handler) { m_onCommit = handler; }

protected:
  void focusOutEvent(QFocusEvent* event) override {
    QPlainTextEdit::focusOutEvent(event);
    if (m_onCommit) m_onCommit();
  }
};

// 複数行の文字列パラメータ編集フィールド（Step_NotifyGoogleChatのメッセージ等）。
// macroProviderを指定すると、そのリストにあるマクロを、文字色で強調表示する
void buildStepMultiLineField(
    QVBoxLayout* parentLay, QWidget* parentWidget, const QString& label,
    StepEditor editor, int paramIndex, std::function<void()> onChanged,
    const QString& tooltip = QString(), const QString& placeholder = QString(),
    int height = 70, std::function<QStringList()> macroProvider = nullptr) {
  ActionStep step = readStep(editor);
  QString value   = (paramIndex < step.params.size())
                        ? step.params.at(paramIndex).toString()
                        : QString();

  QFormLayout* fl = newProjectFormLayout();
  fl->setContentsMargins(0, 0, 0, 0);
  CommitPlainTextEdit* edit = new CommitPlainTextEdit(parentWidget);
  edit->setPlainText(value);
  // 空欄のときに薄い文字で表示される（実際に値として保存・使用される内容ではない）
  if (!placeholder.isEmpty()) edit->setPlaceholderText(placeholder);
  edit->setFixedHeight(height);
  edit->setTabChangesFocus(true);
  if (macroProvider)
    new MacroHighlighter(
        edit, macroProvider);  // editの子になり、editと共に破棄される
  if (!tooltip.isEmpty()) edit->setToolTip(tooltip);
  edit->setCommitHandler([editor, paramIndex, edit, onChanged, value]() {
    QString newValue = edit->toPlainText();
    // 変更が無ければ何もしない（フォーカスを移動しただけでフォームを作り直さないため）
    if (newValue == value) return;
    editor([paramIndex, newValue](ActionStep& s) {
      ensureParamSize(s, paramIndex);
      s.params[paramIndex] = newValue;
    });
    onChanged();
  });
  fl->addRow(label, edit);
  parentLay->addLayout(fl);
}

// プロジェクト設定に登録されたGoogle Chatの投稿先から選ぶコンボボックス
void buildStepChatDestinationField(QVBoxLayout* parentLay,
                                   QWidget* parentWidget, const QString& label,
                                   StepEditor editor, int paramIndex,
                                   std::function<void()> onChanged) {
  ActionStep step = readStep(editor);
  QString value   = (paramIndex < step.params.size())
                        ? step.params.at(paramIndex).toString()
                        : QString();

  QFormLayout* fl = newProjectFormLayout();
  fl->setContentsMargins(0, 0, 0, 0);
  QComboBox* combo = new QComboBox(parentWidget);
  combo->addItem(QObject::tr("(Not selected)"), QString());
  bool found = value.isEmpty();
  for (auto& name : tmpSettings.chatDestinations) {
    combo->addItem(name, name);
    if (name == value) found = true;
  }
  // 登録が削除された（または名前が一致しない）投稿先を参照している場合も、
  // 設定の不整合に気付けるよう、その名前を残して表示する
  if (!found)
    combo->addItem(QObject::tr("%1 (not registered)").arg(value), value);
  combo->setCurrentIndex(combo->findData(value));
  combo->setToolTip(
      QObject::tr("Destinations are registered in the Basics page of the "
                  "project settings."));
  QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                   parentWidget, [editor, paramIndex, combo, onChanged](int) {
                     QString newVal = combo->currentData().toString();
                     editor([paramIndex, newVal](ActionStep& s) {
                       ensureParamSize(s, paramIndex);
                       s.params[paramIndex] = newVal;
                     });
                     onChanged();
                   });
  fl->addRow(label, combo);
  parentLay->addLayout(fl);
}

void buildStepFlowGroupField(QVBoxLayout* parentLay, QWidget* parentWidget,
                             const QString& label, StepEditor editor,
                             int paramIndex, std::function<void()> onChanged) {
  ActionStep step    = readStep(editor);
  unsigned int value = (paramIndex < step.params.size())
                           ? step.params.at(paramIndex).toUInt()
                           : 0;

  QFormLayout* fl = newProjectFormLayout();
  fl->setContentsMargins(0, 0, 0, 0);
  QComboBox* combo = new QComboBox(parentWidget);
  for (auto fgId : tmpSettings.flowGroupInfos.keys())
    combo->addItem(tmpSettings.flowGroupInfos.value(fgId).labelName,
                   (uint)fgId);
  combo->setCurrentIndex(combo->findData(value));
  QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                   parentWidget, [editor, paramIndex, combo, onChanged](int) {
                     unsigned int newVal = combo->currentData().toUInt();
                     editor([paramIndex, newVal](ActionStep& s) {
                       ensureParamSize(s, paramIndex);
                       s.params[paramIndex] = newVal;
                     });
                     onChanged();
                   });
  fl->addRow(label, combo);
  parentLay->addLayout(fl);
}

void buildStepWorkflowField(QVBoxLayout* parentLay, QWidget* parentWidget,
                            const QString& label, StepEditor editor,
                            int paramIndex, std::function<void()> onChanged) {
  ActionStep step    = readStep(editor);
  unsigned int value = (paramIndex < step.params.size())
                           ? step.params.at(paramIndex).toUInt()
                           : 0;

  QFormLayout* fl = newProjectFormLayout();
  fl->setContentsMargins(0, 0, 0, 0);
  QComboBox* combo = new QComboBox(parentWidget);
  for (auto& wf : tmpSettings.workflows)
    combo->addItem(workflowDisplayLabel(wf), (uint)wf.workflowType);
  combo->setCurrentIndex(combo->findData(value));
  QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                   parentWidget, [editor, paramIndex, combo, onChanged](int) {
                     unsigned int newVal = combo->currentData().toUInt();
                     editor([paramIndex, newVal](ActionStep& s) {
                       ensureParamSize(s, paramIndex);
                       s.params[paramIndex] = newVal;
                     });
                     onChanged();
                   });
  fl->addRow(label, combo);
  parentLay->addLayout(fl);
}

// SceneInfoTagのidName一覧から選択するコンボボックス（Step_ConditionalBranchByInfoのInfo
// Name等）
void buildStepInfoNameField(QVBoxLayout* parentLay, QWidget* parentWidget,
                            const QString& label, StepEditor editor,
                            int paramIndex, std::function<void()> onChanged) {
  ActionStep step = readStep(editor);
  QString value   = (paramIndex < step.params.size())
                        ? step.params.at(paramIndex).toString()
                        : QString();

  QFormLayout* fl = newProjectFormLayout();
  fl->setContentsMargins(0, 0, 0, 0);
  QComboBox* combo = new QComboBox(parentWidget);
  for (auto* tag : tmpSettings.sceneInfoTags)
    combo->addItem(tag->idName(), tag->idName());
  // プロジェクトに無いカット情報名が設定されている場合も、消さずに表示する
  if (!value.isEmpty() && combo->findData(value) < 0)
    combo->addItem(QObject::tr("%1 (not defined)").arg(value), value);
  combo->setCurrentIndex(combo->findData(value));
  QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                   parentWidget, [editor, paramIndex, combo, onChanged](int) {
                     QString newVal = combo->currentData().toString();
                     editor([paramIndex, newVal](ActionStep& s) {
                       ensureParamSize(s, paramIndex);
                       s.params[paramIndex] = newVal;
                     });
                     onChanged();
                   });
  fl->addRow(label, combo);
  parentLay->addLayout(fl);
}

// startIndexから末尾までの可変長パラメータを、文字列一覧として編集する（Rename等の宛先パス一覧）。
// candidateProviderを省略した場合はMacroLineEditの既定の補完候補がそのまま使われる
// suggestionProviderを指定すると、Add/Removeボタンの並びに候補選択ボタンを追加する
// （後工程のCondition
// Path等、他のデータから導出できる候補をワンクリックで行として追加できるようにする）
void buildStepPathListField(
    QVBoxLayout* parentLay, QWidget* parentWidget, const QString& label,
    StepEditor editor, int startIndex, std::function<void()> onChanged,
    std::function<QStringList()> candidateProvider  = nullptr,
    std::function<QStringList()> suggestionProvider = nullptr) {
  ActionStep step = readStep(editor);

  parentLay->addWidget(new QLabel(label, parentWidget));
  QTableWidget* table = new QTableWidget(0, 1, parentWidget);
  table->setHorizontalHeaderLabels({QObject::tr("Path")});
  table->verticalHeader()->setVisible(false);
  table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSelectionMode(QAbstractItemView::ExtendedSelection);
  // 以前はAllEditTriggers（CurrentChangedを含む）を指定しており、セルが選択されただけで
  // 編集モードに入り、選択ハイライトの背景の上に編集用エディタが重なって文字が読めなくなる
  // 不具合の原因になっていた。他の編集可能なテーブルと同様、ダブルクリック等の明示的な
  // 操作でのみ編集モードに入るようにする
  table->setEditTriggers(QAbstractItemView::DoubleClicked |
                         QAbstractItemView::EditKeyPressed |
                         QAbstractItemView::AnyKeyPressed);
  // 編集中のエディタ（MacroLineEdit）が常に読める配色になるようにする
  table->setStyleSheet(
      "QPlainTextEdit { background-color: white; color: black; }");
  // "["の入力でPath Macrosのマクロを補完できるようにする
  table->setItemDelegate(new MacroLineEditDelegate(table, candidateProvider));
  for (int i = startIndex; i < step.params.size(); i++) {
    int row = table->rowCount();
    table->insertRow(row);
    table->setItem(row, 0, new QTableWidgetItem(step.params.at(i).toString()));
  }
  parentLay->addWidget(table);

  QObject::connect(table, &QTableWidget::itemChanged, parentWidget,
                   [editor, startIndex, onChanged](QTableWidgetItem* item) {
                     int paramIndex = startIndex + item->row();
                     QString text   = item->text();
                     editor([paramIndex, text](ActionStep& s) {
                       ensureParamSize(s, paramIndex);
                       s.params[paramIndex] = text;
                     });
                     onChanged();
                   });

  QHBoxLayout* btnLay = new QHBoxLayout();
  btnLay->setContentsMargins(0, 0, 0, 0);
  QPushButton* addBtn    = new QPushButton(QObject::tr("Add"), parentWidget);
  QPushButton* removeBtn = new QPushButton(QObject::tr("Remove"), parentWidget);
  btnLay->addStretch(1);
  if (suggestionProvider) {
    QPushButton* suggestButton =
        new QPushButton(QObject::tr("Suggest..."), parentWidget);
    suggestButton->setToolTip(QObject::tr(
        "Add a row from the condition path(s) of the next workflow(s)."));
    QObject::connect(
        suggestButton, &QPushButton::clicked, parentWidget,
        [suggestButton, suggestionProvider, editor, onChanged]() {
          QStringList candidates = suggestionProvider();
          if (candidates.isEmpty()) {
            QMenu emptyMenu;
            emptyMenu.addAction(QObject::tr("(No candidates)"))
                ->setEnabled(false);
            emptyMenu.exec(
                suggestButton->mapToGlobal(QPoint(0, suggestButton->height())));
            return;
          }
          QMenu menu;
          for (auto& candidate : candidates) menu.addAction(candidate);
          QAction* chosen = menu.exec(
              suggestButton->mapToGlobal(QPoint(0, suggestButton->height())));
          if (!chosen) return;
          editor([chosen](ActionStep& s) { s.params.append(chosen->text()); });
          onChanged();
        });
    btnLay->addWidget(suggestButton);
  }
  btnLay->addWidget(addBtn);
  btnLay->addWidget(removeBtn);
  parentLay->addLayout(btnLay);

  QObject::connect(addBtn, &QPushButton::clicked, parentWidget,
                   [editor, onChanged]() {
                     editor([](ActionStep& s) { s.params.append(QString()); });
                     onChanged();
                   });
  QObject::connect(removeBtn, &QPushButton::clicked, parentWidget,
                   [editor, table, startIndex, onChanged]() {
                     QSet<int> rows;
                     for (auto* item : table->selectedItems())
                       rows.insert(item->row());
                     if (rows.isEmpty()) return;
                     QList<int> sortedRows = rows.values();
                     std::sort(sortedRows.begin(), sortedRows.end(),
                               std::greater<int>());
                     editor([sortedRows, startIndex](ActionStep& s) {
                       for (int row : sortedRows) {
                         int paramIndex = startIndex + row;
                         if (paramIndex >= 0 && paramIndex < s.params.size())
                           s.params.removeAt(paramIndex);
                       }
                     });
                     onChanged();
                   });
}

//---
//Step_CopyFolderContentsのModeパラメータ（角括弧オプションを組み合わせた文字列）を、
//    意味ごとの軸（対象の選び方／転送方式／名前の変換／内容の変換）に分けて、
//    コンボボックス・チェックボックス・入力フィールドで編集できるようにする。
//    文字列の解析・生成は、実行側（StepExecutor_CopyFolderContents::parseMode/buildMode）と共通
void buildStepCopyFolderContentsModeField(QVBoxLayout* parentLay,
                                          QWidget* parentWidget,
                                          const QString& label,
                                          StepEditor editor, int paramIndex,
                                          std::function<void()> onChanged) {
  typedef StepExecutor_CopyFolderContents::Options Options;
  ActionStep step = readStep(editor);
  QString modeStr = (paramIndex < step.params.size())
                        ? step.params.at(paramIndex).toString()
                        : QString();
  Options mode    = StepExecutor_CopyFolderContents::parseMode(modeStr);

  QGroupBox* box      = new QGroupBox(label, parentWidget);
  QVBoxLayout* boxLay = new QVBoxLayout(box);

  //--- 対象の選び方
  QGroupBox* selectionBox = new QGroupBox(QObject::tr("Target Selection"), box);
  QFormLayout* selectionLay = newProjectFormLayout(selectionBox);

  QComboBox* selectionCombo = new QComboBox(parentWidget);
  selectionCombo->addItem(QObject::tr("All selected"), (int)Options::All);
  selectionCombo->setItemData(
      0,
      QObject::tr("Everything is selected at first. Use the blacklist to "
                  "exclude items."),
      Qt::ToolTipRole);
  selectionCombo->addItem(QObject::tr("None selected"), (int)Options::Select);
  selectionCombo->setItemData(
      1,
      QObject::tr(
          "Nothing is selected at first. Use the whitelist to select items."),
      Qt::ToolTipRole);
  selectionCombo->addItem(QObject::tr("All, without selection screen"),
                          (int)Options::AllWithoutUI);
  selectionCombo->setItemData(
      2,
      QObject::tr("Everything is copied without showing the selection screen."),
      Qt::ToolTipRole);
  selectionCombo->setCurrentIndex(
      selectionCombo->findData((int)mode.selection));
  selectionLay->addRow(QObject::tr("Default Selection"), selectionCombo);

  QLineEdit* folderDepthEdit = new QLineEdit(
      mode.folderDepth >= 0 ? QString::number(mode.folderDepth) : QString(),
      parentWidget);
  folderDepthEdit->setValidator(new QIntValidator(0, 99, folderDepthEdit));
  folderDepthEdit->setPlaceholderText(QObject::tr("(unspecified)"));
  selectionLay->addRow(QObject::tr("Folder Depth"), folderDepthEdit);

  QLineEdit* listEdit = new QLineEdit(mode.whiteBlackList, parentWidget);
  listEdit->setPlaceholderText(QObject::tr("+name;-name"));
  listEdit->setToolTip(
      QObject::tr("Semicolon-separated. Prefix \"+\" to force-select "
                  "(whitelist) or \"-\" to force-deselect (blacklist)."));
  selectionLay->addRow(QObject::tr("Whitelist / Blacklist"), listEdit);
  boxLay->addWidget(selectionBox);

  //---
  //転送方式（項目が1つだけなので、グループボックスは使わず、縦方向の大きさを節約する）
  QFormLayout* transferLay = newProjectFormLayout();
  QComboBox* transferCombo = new QComboBox(parentWidget);
  transferCombo->addItem(QObject::tr("Copy"), false);
  transferCombo->addItem(QObject::tr("Move"), true);
  transferCombo->setItemData(
      0, QObject::tr("Copy the files and keep the originals."),
      Qt::ToolTipRole);
  transferCombo->setItemData(
      1, QObject::tr("Delete the original files after copying."),
      Qt::ToolTipRole);
  transferCombo->setCurrentIndex(mode.move ? 1 : 0);
  transferLay->addRow(QObject::tr("Transfer"), transferCombo);
  boxLay->addLayout(transferLay);

  //--- 名前の変換
  QGroupBox* nameBox = new QGroupBox(QObject::tr("Name Conversion"), box);
  nameBox->setToolTip(
      QObject::tr("When several are selected, they are applied in this order: "
                  "Rename Rule, Take Suffix, Flatten subfolders."));
  QFormLayout* nameLay = newProjectFormLayout(nameBox);

  // リネームルール：Stuffフォルダのrename_scriptsに置いたスクリプトから選ぶ
  QComboBox* renameRuleCombo = new QComboBox(parentWidget);
  renameRuleCombo->addItem(QObject::tr("(None)"), QString());
  QString currentRuleKey;
  if (mode.renameRule == Options::Rename_Script)
    currentRuleKey = "script:" + mode.renameScript;
  {
    QStandardItemModel* ruleModel =
        qobject_cast<QStandardItemModel*>(renameRuleCombo->model());
    for (const RenameScript::Info& info : RenameScript::availableScripts()) {
      QString key    = "script:" + info.id;
      bool isCurrent = (key == currentRuleKey);
      // 読み込みに失敗したスクリプトは、選べないようにする（設定済みのものは、そのまま表示する）
      QString text =
          info.isValid() ? info.name : QObject::tr("%1 (error)").arg(info.name);
      renameRuleCombo->addItem(text, key);
      int index = renameRuleCombo->count() - 1;
      renameRuleCombo->setItemData(
          index, info.isValid() ? info.description : info.error,
          Qt::ToolTipRole);
      if (!info.isValid() && !isCurrent && ruleModel && ruleModel->item(index))
        ruleModel->item(index)->setEnabled(false);
    }
    // 設定されているスクリプトが見つからない場合も、設定の不整合に気付けるよう、その名前を残して表示する
    if (renameRuleCombo->findData(currentRuleKey) < 0) {
      // Stuffフォルダに無くても、同梱のスクリプトであれば、そのまま使える
      RenameScript::Info currentInfo =
          RenameScript::scriptInfo(mode.renameScript);
      if (currentInfo.isValid()) {
        renameRuleCombo->addItem(currentInfo.name, currentRuleKey);
        renameRuleCombo->setItemData(renameRuleCombo->count() - 1,
                                     currentInfo.description, Qt::ToolTipRole);
      } else {
        renameRuleCombo->addItem(
            QObject::tr("%1 (not found)").arg(mode.renameScript),
            currentRuleKey);
        renameRuleCombo->setItemData(renameRuleCombo->count() - 1,
                                     currentInfo.error, Qt::ToolTipRole);
      }
    }
  }
  renameRuleCombo->setCurrentIndex(renameRuleCombo->findData(currentRuleKey));
  renameRuleCombo->setToolTip(
      renameRuleCombo
          ->itemData(renameRuleCombo->currentIndex(), Qt::ToolTipRole)
          .toString());

  QPushButton* openScriptsFolderButton =
      new QPushButton(QObject::tr("Open Folder"), parentWidget);
  openScriptsFolderButton->setToolTip(QObject::tr(
      "Open the rename_scripts folder in the Stuff folder. "
      "Rename scripts (.js files) placed there can be selected here."));
  QObject::connect(
      openScriptsFolderButton, &QPushButton::clicked, parentWidget,
      [parentWidget]() {
        RenameScript::installBundledSamplesIfNeeded();
        QString folderPath = RenameScript::scriptsFolderPath();
        if (folderPath.isEmpty()) {
          QMessageBox::information(
              parentWidget, QObject::tr("Information"),
              QObject::tr("The Stuff folder is not configured, so the "
                          "rename_scripts folder cannot be used."));
          return;
        }
        QDir().mkpath(folderPath);
        QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
      });
  QWidget* ruleRowWidget  = new QWidget(parentWidget);
  QHBoxLayout* ruleRowLay = new QHBoxLayout(ruleRowWidget);
  ruleRowLay->setContentsMargins(0, 0, 0, 0);
  ruleRowLay->addWidget(renameRuleCombo, 1);
  ruleRowLay->addWidget(openScriptsFolderButton, 0);
  nameLay->addRow(QObject::tr("Rename Rule"), ruleRowWidget);

  QComboBox* takeCombo = new QComboBox(parentWidget);
  takeCombo->addItem(QObject::tr("(None)"), (int)Options::Take_None);
  takeCombo->addItem(QObject::tr("To current take"), (int)Options::Take_Next);
  takeCombo->setItemData(1,
                         QObject::tr("Replace the take suffix in folder names "
                                     "with the one of the current take."),
                         Qt::ToolTipRole);
  takeCombo->addItem(QObject::tr("To shooting take"), (int)Options::Take_ToR);
  takeCombo->setItemData(
      2,
      QObject::tr("Replace the take suffix in folder names with the one for "
                  "shooting (starting with \"r\")."),
      Qt::ToolTipRole);
  takeCombo->setCurrentIndex(takeCombo->findData((int)mode.takeConversion));
  nameLay->addRow(QObject::tr("Take Suffix"), takeCombo);

  QCheckBox* flattenCheck =
      new QCheckBox(QObject::tr("Flatten subfolders"), parentWidget);
  flattenCheck->setToolTip(
      QObject::tr("Place all files of the subfolders directly under the "
                  "destination folder."));
  flattenCheck->setChecked(mode.flattenSubfolders);
  nameLay->addRow(QString(), flattenCheck);
  boxLay->addWidget(nameBox);

  //--- 内容の変換と、同名のファイルがコピー先にあるときの処理
  //    （それぞれ項目が1つだけなので、グループボックスは使わず、縦方向の大きさを節約する）
  QFormLayout* contentConflictLay = newProjectFormLayout();
  QCheckBox* convertToJpgCheck =
      new QCheckBox(QObject::tr("Convert to JPEG"), parentWidget);
  convertToJpgCheck->setChecked(mode.convertToJpg);
  contentConflictLay->addRow(QObject::tr("Images"), convertToJpgCheck);

  QComboBox* conflictCombo = new QComboBox(parentWidget);
  conflictCombo->addItem(QObject::tr("Ask"), (int)Options::Conflict_Ask);
  conflictCombo->setItemData(
      0, QObject::tr("Choose overwrite, skip or cancel when copying."),
      Qt::ToolTipRole);
  conflictCombo->addItem(QObject::tr("Overwrite"),
                         (int)Options::Conflict_Overwrite);
  conflictCombo->setItemData(
      1, QObject::tr("Overwrite existing files without confirmation."),
      Qt::ToolTipRole);
  conflictCombo->addItem(QObject::tr("Skip"), (int)Options::Conflict_Skip);
  conflictCombo->setItemData(
      2, QObject::tr("Keep existing files without confirmation."),
      Qt::ToolTipRole);
  conflictCombo->addItem(QObject::tr("Overwrite if newer"),
                         (int)Options::Conflict_OverwriteIfNewer);
  conflictCombo->setItemData(
      3,
      QObject::tr("Overwrite only if the source file is newer. Otherwise, keep "
                  "the existing file."),
      Qt::ToolTipRole);
  conflictCombo->setCurrentIndex(
      conflictCombo->findData((int)mode.conflictPolicy));
  conflictCombo->setToolTip(
      QObject::tr("Files that are not overwritten stay where they are, "
                  "even when moving (the original files are not deleted)."));
  contentConflictLay->addRow(QObject::tr("If File Exists"), conflictCombo);
  boxLay->addLayout(contentConflictLay);

  parentLay->addWidget(box);

  // 併用できない組み合わせ（MOVEとCONVERT_TO_JPG）や、無効な指定（UIなしのときの階層・白黒リスト）を操作不能にする
  auto updateEnabledStates = [selectionCombo, folderDepthEdit, listEdit,
                              transferCombo, convertToJpgCheck]() {
    bool noUi =
        (selectionCombo->currentData().toInt() == (int)Options::AllWithoutUI);
    folderDepthEdit->setEnabled(!noUi);
    listEdit->setEnabled(!noUi);

    bool isMove                  = transferCombo->currentData().toBool();
    const QString convertTooltip = QObject::tr(
        "Cannot be combined with Move, because the original images could not "
        "be restored.");
    convertToJpgCheck->setEnabled(!isMove);
    convertToJpgCheck->setToolTip(isMove ? convertTooltip : QString());
    if (QStandardItemModel* transferModel =
            qobject_cast<QStandardItemModel*>(transferCombo->model())) {
      if (QStandardItem* moveItem = transferModel->item(1)) {
        moveItem->setEnabled(!convertToJpgCheck->isChecked());
        moveItem->setToolTip(convertToJpgCheck->isChecked() ? convertTooltip
                                                            : QString());
      }
    }
  };
  updateEnabledStates();

  auto applyMode = [editor, paramIndex, onChanged, selectionCombo,
                    folderDepthEdit, listEdit, transferCombo, renameRuleCombo,
                    takeCombo, flattenCheck, convertToJpgCheck, conflictCombo,
                    updateEnabledStates]() {
    updateEnabledStates();
    Options m;
    m.selection = (Options::Selection)selectionCombo->currentData().toInt();
    m.move      = transferCombo->currentData().toBool();
    m.flattenSubfolders = flattenCheck->isChecked();
    QString ruleKey     = renameRuleCombo->currentData().toString();
    if (ruleKey.startsWith("script:")) {
      m.renameRule   = Options::Rename_Script;
      m.renameScript = ruleKey.mid(QString("script:").length());
    }
    m.takeConversion =
        (Options::TakeConversion)takeCombo->currentData().toInt();
    m.convertToJpg = convertToJpgCheck->isChecked() && !m.move;
    m.conflictPolicy =
        (Options::ConflictPolicy)conflictCombo->currentData().toInt();
    m.folderDepth      = folderDepthEdit->text().isEmpty()
                             ? -1
                             : folderDepthEdit->text().toInt();
    m.whiteBlackList   = listEdit->text();
    QString newModeStr = StepExecutor_CopyFolderContents::buildMode(m);
    editor([paramIndex, newModeStr](ActionStep& s) {
      ensureParamSize(s, paramIndex);
      s.params[paramIndex] = newModeStr;
    });
    onChanged();
  };

  QObject::connect(selectionCombo,
                   QOverload<int>::of(&QComboBox::currentIndexChanged),
                   parentWidget, [applyMode](int) { applyMode(); });
  QObject::connect(transferCombo,
                   QOverload<int>::of(&QComboBox::currentIndexChanged),
                   parentWidget, [applyMode](int) { applyMode(); });
  QObject::connect(renameRuleCombo,
                   QOverload<int>::of(&QComboBox::currentIndexChanged),
                   parentWidget, [applyMode](int) { applyMode(); });
  QObject::connect(takeCombo,
                   QOverload<int>::of(&QComboBox::currentIndexChanged),
                   parentWidget, [applyMode](int) { applyMode(); });
  QObject::connect(flattenCheck, &QCheckBox::toggled, parentWidget,
                   [applyMode](bool) { applyMode(); });
  QObject::connect(convertToJpgCheck, &QCheckBox::toggled, parentWidget,
                   [applyMode](bool) { applyMode(); });
  QObject::connect(conflictCombo,
                   QOverload<int>::of(&QComboBox::currentIndexChanged),
                   parentWidget, [applyMode](int) { applyMode(); });
  QObject::connect(folderDepthEdit, &QLineEdit::editingFinished, parentWidget,
                   applyMode);
  QObject::connect(listEdit, &QLineEdit::editingFinished, parentWidget,
                   applyMode);
}

//---
//Step_CheckExistenceのModeパラメータも同様にコンボボックス・チェックボックスで編集する
struct CheckExistenceMode {
  QString checkModeToken = "EXISTENCE";  // ABSENCE / EXISTENCE
  bool sceneInfo         = false;
};

CheckExistenceMode parseCheckExistenceMode(QString modeStr) {
  CheckExistenceMode m;
  if (modeStr.startsWith("[ABSENCE]")) {
    m.checkModeToken = "ABSENCE";
    modeStr          = modeStr.mid(QString("[ABSENCE]").length());
  } else if (modeStr.startsWith("[EXISTENCE]")) {
    m.checkModeToken = "EXISTENCE";
    modeStr          = modeStr.mid(QString("[EXISTENCE]").length());
  }
  if (modeStr.startsWith("[SCENEINFO]")) m.sceneInfo = true;
  return m;
}

QString buildCheckExistenceModeString(const CheckExistenceMode& m) {
  QString s = "[" + m.checkModeToken + "]";
  if (m.sceneInfo) s += "[SCENEINFO]";
  return s;
}

void buildStepCheckExistenceModeField(QVBoxLayout* parentLay,
                                      QWidget* parentWidget,
                                      const QString& label, StepEditor editor,
                                      int paramIndex,
                                      std::function<void()> onChanged) {
  ActionStep step         = readStep(editor);
  QString modeStr         = (paramIndex < step.params.size())
                                ? step.params.at(paramIndex).toString()
                                : QString();
  CheckExistenceMode mode = parseCheckExistenceMode(modeStr);

  QFormLayout* fl = newProjectFormLayout();
  fl->setContentsMargins(0, 0, 0, 0);

  QComboBox* modeCombo = new QComboBox(parentWidget);
  modeCombo->addItem(QObject::tr("ABSENCE (warn if it exists)"), "ABSENCE");
  modeCombo->addItem(QObject::tr("EXISTENCE (warn if it does not exist)"),
                     "EXISTENCE");
  modeCombo->setCurrentIndex(modeCombo->findData(mode.checkModeToken));
  fl->addRow(label, modeCombo);

  QCheckBox* sceneInfoCheck = new QCheckBox(
      QObject::tr("Check a Scene Info Tag instead of a file (SCENEINFO)"),
      parentWidget);
  sceneInfoCheck->setChecked(mode.sceneInfo);
  fl->addRow(QString(), sceneInfoCheck);

  parentLay->addLayout(fl);

  auto applyMode = [editor, paramIndex, onChanged, modeCombo,
                    sceneInfoCheck]() {
    CheckExistenceMode m;
    m.checkModeToken   = modeCombo->currentData().toString();
    m.sceneInfo        = sceneInfoCheck->isChecked();
    QString newModeStr = buildCheckExistenceModeString(m);
    editor([paramIndex, newModeStr](ActionStep& s) {
      ensureParamSize(s, paramIndex);
      s.params[paramIndex] = newModeStr;
    });
    onChanged();
  };

  QObject::connect(modeCombo,
                   QOverload<int>::of(&QComboBox::currentIndexChanged),
                   parentWidget, [applyMode](int) { applyMode(); });
  QObject::connect(sceneInfoCheck, &QCheckBox::toggled, parentWidget,
                   [applyMode](bool) { applyMode(); });
}

// allowNoneは入れ子ステップ（Step_ConditionalBranchByInfo/FileExistenceの分岐先）でのみtrueにする。
// 「None」（何もしない）はStep_Unidentifiedで表現し、親のparamsには空のリストとして格納される
// nextConditionPathProviderは、このActionStepが属するPPM_ActionのnextFlows先ワークフローの
// Condition
// Pathを集めたもの。RenameTo/CopyTo/FileToBeCreatedフィールドの入力補助に使う
void buildStepParamsForm(
    QVBoxLayout* parentLay, QWidget* parentWidget, StepEditor editor,
    std::function<void()> onChanged,
    std::function<QStringList()> nextConditionPathProvider = nullptr,
    bool allowNone                                         = false) {
  ActionStep step = readStep(editor);

  QFormLayout* typeFormLay = newProjectFormLayout();
  typeFormLay->setContentsMargins(0, 0, 0, 0);
  QComboBox* typeCombo = new QComboBox(parentWidget);
  if (allowNone)
    typeCombo->addItem(QObject::tr("None"), (int)Step_Unidentified);
  for (int t = 0; t < (int)Step_COUNT; t++) {
    ActionStepType type = (ActionStepType)t;
    // 「動画リテイク経路選択」は特定のスタジオ向けのステップのため、選択肢には出さない。
    // 既存のプロジェクトで使われている場合は、その種類を表示できるように残す
    if (type == Step_DougaRetakeRouting && step.type != type) continue;
    typeCombo->addItem(
        iconForActionStepType(type),
        WorkflowManager::instance()->actionStepTypeLabelName(type), type);
  }
  typeCombo->setCurrentIndex(typeCombo->findData(step.type));
  QObject::connect(typeCombo,
                   QOverload<int>::of(&QComboBox::currentIndexChanged),
                   parentWidget, [editor, typeCombo, onChanged](int) {
                     ActionStepType newType =
                         (ActionStepType)typeCombo->currentData().toInt();
                     editor([newType](ActionStep& s) {
                       if (s.type == newType) return;
                       s.type   = newType;
                       s.params = defaultParamsForStepType(newType);
                     });
                     onChanged();
                   });
  typeFormLay->addRow(QObject::tr("Step Type"), typeCombo);
  parentLay->addLayout(typeFormLay);

  switch (step.type) {
  case Step_Rename:
  case Step_Copy:
    buildStepPathListField(parentLay, parentWidget,
                           QObject::tr("Destination Paths"), editor, 0,
                           onChanged, nullptr, nextConditionPathProvider);
    break;
  case Step_RenameFile:
  case Step_CopyFile:
    buildStepMacroLineField(parentLay, parentWidget, QObject::tr("Source Path"),
                            editor, 0, onChanged);
    buildStepPathListField(parentLay, parentWidget,
                           QObject::tr("Destination Paths"), editor, 1,
                           onChanged);
    break;
  case Step_CopyFolderContents:
    buildStepCopyFolderContentsModeField(
        parentLay, parentWidget, QObject::tr("Mode"), editor, 0, onChanged);
    // 空欄（キューファイルのあるフォルダ）のほか、キューファイルを除いてコピーする
    // [SKIP_CUE_FILE]も候補にする
    buildStepMacroLineField(
        parentLay, parentWidget, QObject::tr("Source Path"), editor, 1,
        onChanged,
        []() {
          QStringList macros = MacroLineEdit::defaultMacroCandidates();
          macros.prepend("[SKIP_CUE_FILE]");
          return macros;
        },
        QObject::tr("Leave this empty to copy from the folder of the cue "
                    "file.\n[SKIP_CUE_FILE]: the folder of the cue file, "
                    "without copying the cue file itself."));
    // 通常のパス系マクロに加え、実行時の日付に置き換わる[CURRENT_DATE]も候補にする
    buildStepPathListField(
        parentLay, parentWidget, QObject::tr("Destination Paths"), editor, 2,
        onChanged, []() {
          QStringList macros = {"[ROOT]", "[PREFIX]", "[CUTNUMBER]",
                                "[CURRENT_DATE]", "[USER_NAME]"};
          for (auto& macroInfo : tmpSettings.pathMacros)
            macros.append(macroInfo.macroStr);
          return macros;
        });
    break;
  case Step_MoveFlowGroup:
  case Step_StartBranchFlow:
  case Step_EndBranchFlow:
    buildStepFlowGroupField(parentLay, parentWidget, QObject::tr("Flow Group"),
                            editor, 0, onChanged);
    break;
  case Step_CheckExistence:
    buildStepCheckExistenceModeField(parentLay, parentWidget,
                                     QObject::tr("Mode"), editor, 0, onChanged);
    buildStepPathListField(parentLay, parentWidget, QObject::tr("Targets"),
                           editor, 1, onChanged);
    break;
  case Step_AskDelete:
    buildStepBoolField(parentLay, parentWidget,
                       QObject::tr("Delete By Default"), editor, 0, onChanged);
    buildStepMacroLineField(parentLay, parentWidget,
                            QObject::tr("Path (optional)"), editor, 1,
                            onChanged);
    break;
  case Step_CreateCueFile:
    buildStepMacroLineField(parentLay, parentWidget, QObject::tr("File Path"),
                            editor, 0, onChanged, nullptr, QString(),
                            nextConditionPathProvider);
    buildStepStringField(parentLay, parentWidget,
                         QObject::tr("Content (optional)"), editor, 1,
                         onChanged);
    break;
  case Step_SetSceneInfo:
    buildStepInfoNameField(parentLay, parentWidget, QObject::tr("Info Name"),
                           editor, 0, onChanged);
    buildStepStringField(parentLay, parentWidget, QObject::tr("Value"), editor,
                         1, onChanged);
    break;
  case Step_ConditionalBranchByInfo: {
    buildStepInfoNameField(parentLay, parentWidget, QObject::tr("Info Name"),
                           editor, 0, onChanged);
    buildStepStringField(
        parentLay, parentWidget, QObject::tr("Condition Value"), editor, 1,
        onChanged,
        QObject::tr("The step in \"If True\" runs when the scene info tag has "
                    "this value.\n"
                    "If empty: for a Bool tag, when the tag is true; for "
                    "other tags, when the tag is set."));

    QGroupBox* trueBox   = new QGroupBox(QObject::tr("If True"), parentWidget);
    QVBoxLayout* trueLay = new QVBoxLayout(trueBox);
    buildStepParamsForm(trueLay, trueBox, makeNestedStepEditor(editor, 2),
                        onChanged, nextConditionPathProvider, true);
    parentLay->addWidget(trueBox);

    QGroupBox* falseBox = new QGroupBox(QObject::tr("If False"), parentWidget);
    QVBoxLayout* falseLay = new QVBoxLayout(falseBox);
    buildStepParamsForm(falseLay, falseBox, makeNestedStepEditor(editor, 3),
                        onChanged, nextConditionPathProvider, true);
    parentLay->addWidget(falseBox);
    break;
  }
  case Step_ConditionalBranchByFileExistence: {
    buildStepMacroLineField(parentLay, parentWidget,
                            QObject::tr("Condition Path"), editor, 0,
                            onChanged);

    QGroupBox* existsBox =
        new QGroupBox(QObject::tr("If Exists"), parentWidget);
    QVBoxLayout* existsLay = new QVBoxLayout(existsBox);
    buildStepParamsForm(existsLay, existsBox, makeNestedStepEditor(editor, 1),
                        onChanged, nextConditionPathProvider, true);
    parentLay->addWidget(existsBox);

    QGroupBox* notExistsBox =
        new QGroupBox(QObject::tr("If Not Exists"), parentWidget);
    QVBoxLayout* notExistsLay = new QVBoxLayout(notExistsBox);
    buildStepParamsForm(notExistsLay, notExistsBox,
                        makeNestedStepEditor(editor, 2), onChanged,
                        nextConditionPathProvider, true);
    parentLay->addWidget(notExistsBox);
    break;
  }
  case Step_InputGoogleSpreadsheet:
    buildStepMacroLineField(
        parentLay, parentWidget, QObject::tr("Sheet Name (optional)"), editor,
        0, onChanged, []() { return QStringList{"[EPISODE]"}; },
        QObject::tr(
            "Leave this empty to write into the spreadsheet's first sheet. "
            "Type \"[\" to insert the special token [EPISODE]."));
    buildStepStringField(parentLay, parentWidget, QObject::tr("Flow Name"),
                         editor, 1, onChanged,
                         QObject::tr("Must match a unique flow name written in "
                                     "the sheet's flow-name row."));
    buildStepMacroLineField(
        parentLay, parentWidget, QObject::tr("Value"), editor, 2, onChanged,
        []() { return QStringList{"[CURRENT_DATE]", "[USER_NAME]"}; },
        QObject::tr("Type \"[\" to insert a special token: [CURRENT_DATE] or "
                    "[USER_NAME].\n"
                    "Separate multiple strings with a semicolon (;) to let the "
                    "user choose which one "
                    "to input from a dropdown when confirming the action."));
    break;
  case Step_DougaRetakeRouting:
    parentLay->addWidget(
        new QLabel(QObject::tr("This step type has no editable parameters."),
                   parentWidget));
    break;
  case Step_ReleaseProgress:
    buildStepWorkflowField(parentLay, parentWidget, QObject::tr("Workflow"),
                           editor, 0, onChanged);
    break;
  case Step_AddProgress:
    buildStepWorkflowField(parentLay, parentWidget, QObject::tr("Workflow"),
                           editor, 0, onChanged);
    parentLay->addWidget(new QLabel(
        QObject::tr("Adds the workflow to the next workflows of this action. "
                    "Use it inside a conditional branch step to add the "
                    "workflow only when the condition is met."),
        parentWidget));
    break;
  case Step_CreateEmptyFolder:
    // 通常のパス系マクロ（Path Macros +
    // 組み込みマクロ）に加え、実行時の日付に置き換わる
    // [CURRENT_DATE]も候補にする
    buildStepMacroLineField(
        parentLay, parentWidget, QObject::tr("Folder Path"), editor, 0,
        onChanged,
        []() {
          QStringList macros = {"[ROOT]", "[PREFIX]", "[CUTNUMBER]",
                                "[CURRENT_DATE]", "[USER_NAME]"};
          for (auto& macroInfo : tmpSettings.pathMacros)
            macros.append(macroInfo.macroStr);
          return macros;
        },
        QObject::tr(
            "Type \"[\" to insert a macro. Supports the special token "
            "[CURRENT_DATE] (replaced with the execution date, yyMMdd)."));
    break;
  case Step_ZipFolder:
    buildStepMacroLineField(parentLay, parentWidget,
                            QObject::tr("Source Folder (optional)"), editor, 0,
                            onChanged);
    buildStepMacroLineField(parentLay, parentWidget,
                            QObject::tr("Destination Zip Path"), editor, 1,
                            onChanged);
    break;
  case Step_NotifyGoogleChat: {
    const QString macroTooltip = QObject::tr(
        "The following macros can be used: [CUTNUMBER], [SEQUENCE], [EPISODE], "
        "[USER_NAME] (the user name), [CURRENT_DATE] (the execution date, "
        "yyyy/MM/dd), "
        "[CURRENT_DATETIME] (the execution date and time), [PROJECT_NAME] (the "
        "project title), "
        "[WORKFLOW_NAME] (the workflow name), [ACTION_NAME] (the action "
        "name).");
    // メッセージ欄で使えるマクロ（実行時に置き換えられるもの）。[SEQUENCE]は、プロジェクトで使う設定のときだけ
    std::function<QStringList()> chatMacros = []() {
      QStringList macros = {"[CUTNUMBER]",        "[EPISODE]",
                            "[USER_NAME]",        "[CURRENT_DATE]",
                            "[CURRENT_DATETIME]", "[PROJECT_NAME]",
                            "[WORKFLOW_NAME]",    "[ACTION_NAME]"};
      if (tmpSettings.useSequence) macros.insert(1, "[SEQUENCE]");
      return macros;
    };
    buildStepChatDestinationField(parentLay, parentWidget,
                                  QObject::tr("Destination"), editor, 0,
                                  onChanged);
    // 空欄のときは既定のメッセージが使われる。その内容を薄い文字（プレースホルダー）で表示する
    buildStepMultiLineField(
        parentLay, parentWidget, QObject::tr("Message"), editor, 1, onChanged,
        QObject::tr("The notification message. If left empty, the default "
                    "message shown in gray is used. ") +
            macroTooltip,
        StepExecutor_NotifyGoogleChat::defaultMessageTemplate(
            tmpSettings.hasEpisodes),
        130, chatMacros);
    buildStepBoolField(parentLay, parentWidget, QObject::tr("Allow Free Text"),
                       editor, 2, onChanged,
                       QObject::tr("When on, the user can append a message in "
                                   "the confirmation dialog."));
    buildStepBoolField(parentLay, parentWidget, QObject::tr("Notify On Cancel"),
                       editor, 3, onChanged);
    buildStepMultiLineField(
        parentLay, parentWidget, QObject::tr("Cancel Message (optional)"),
        editor, 4, onChanged,
        QObject::tr("The message sent when the action is canceled (undone). If "
                    "left empty, the original message is "
                    "re-posted after the line \"[The following operation was "
                    "canceled]\". ") +
            macroTooltip,
        QString(), 70, chatMacros);
    buildStepMacroLineField(
        parentLay, parentWidget, QObject::tr("Thread Key (optional)"), editor,
        5, onChanged,
        []() {
          QStringList macros = {"[CUTNUMBER]", "[EPISODE]"};
          if (tmpSettings.useSequence) macros.insert(1, "[SEQUENCE]");
          return macros;
        },
        QObject::tr("Messages that have the same thread key are grouped into "
                    "one thread in Google Chat. "
                    "For example, [CUTNUMBER] gathers the completion and "
                    "cancel notifications of a scene in one thread."));
    break;
  }
  case Step_Unidentified:
    // 「None」：分岐先のステップとして何も実行しない
    parentLay->addWidget(
        new QLabel(QObject::tr("This branch does nothing."), parentWidget));
    break;
  }
}
};  // namespace

MacroHighlighter::MacroHighlighter(QPlainTextEdit* edit,
                                   std::function<QStringList()> macroProvider)
    : QSyntaxHighlighter(edit->document())
    , m_edit(edit)
    , m_macroProvider(macroProvider) {
  setParent(edit);
}

QColor MacroHighlighter::macroColor(const QWidget* widget) {
  // 暗い背景では明るい青、明るい背景では濃い青にする
  const bool darkBackground =
      widget->palette().color(QPalette::Base).lightness() < 128;
  return darkBackground ? QColor(120, 180, 255) : QColor(0, 100, 200);
}

void MacroHighlighter::highlightBlock(const QString& text) {
  if (!m_macroProvider) return;
  QTextCharFormat format;
  format.setForeground(macroColor(m_edit));
  // 候補に無い"[...]"（タイプミス等）には色を付けない。マクロは大文字小文字を区別して置き換えられる
  for (const auto& range :
       MacroLineEdit::findMacroRanges(text, m_macroProvider()))
    setFormat(range.first, range.second, format);
}

QStringList MacroLineEdit::defaultMacroCandidates() {
  QStringList macros = {"[ROOT]", "[PREFIX]", "[CUTNUMBER]", "[USER_NAME]"};
  for (auto& macroInfo : tmpSettings.pathMacros)
    macros.append(macroInfo.macroStr);
  return macros;
}

QStringList MacroLineEdit::knownMacros(const QStringList& candidates) {
  // パスの解決（WorkflowManager::decodePath）で置き換えられる組み込みのマクロ
  QStringList macros = {
      "[ROOT]", "[PREFIX]", "[CUTNUMBER]", "[USER_NAME]",          "[EPISODE]",
      "[TAKE]", "[TAKE-1]", "[R_TAKE]",    "[PREV_EXISTING_TAKE]", "[R]"};
  if (tmpSettings.useSequence) macros.append("[SEQUENCE]");
  for (const QString& candidate : candidates)
    if (!macros.contains(candidate)) macros.append(candidate);
  return macros;
}

QList<QPair<int, int>> MacroLineEdit::findMacroRanges(
    const QString& text, const QStringList& macros) {
  QList<QPair<int, int>> ranges;
  for (const QString& macro : macros) {
    if (macro.isEmpty()) continue;
    int pos = 0;
    while ((pos = text.indexOf(macro, pos)) >= 0) {
      ranges.append({pos, (int)macro.size()});
      pos += macro.size();
    }
  }
  std::sort(ranges.begin(), ranges.end());
  return ranges;
}

namespace {
// 1行の入力欄にするため、改行は空白に置き換える
QString toSingleLine(QString text) {
  text.replace("\r\n", " ").replace('\n', ' ').replace('\r', ' ');
  return text;
}
}  // namespace

MacroLineEdit::MacroLineEdit(QWidget* parent)
    : QPlainTextEdit(parent), m_macroStartPos(-1) {
  // 1行専用の入力欄にする：折り返し・スクロールバーなし、Tabはフォーカス移動
  setLineWrapMode(QPlainTextEdit::NoWrap);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setTabChangesFocus(true);
  // 高さ・余白を、QLineEditに揃える
  const QSize lineEditSize = QLineEdit().sizeHint();
  setFixedHeight(lineEditSize.height());
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  const int margin = qMax(1, (lineEditSize.height() - 2 * frameWidth() -
                              fontMetrics().lineSpacing()) /
                                 2);
  document()->setDocumentMargin(margin);

  m_completer = new QCompleter(this);
  m_completer->setWidget(this);
  m_completer->setCompletionMode(QCompleter::PopupCompletion);
  m_completer->setCaseSensitivity(Qt::CaseInsensitive);
  connect(m_completer, QOverload<const QString&>::of(&QCompleter::activated),
          this, &MacroLineEdit::onCompleterActivated);
  setToolTip(
      tr("Type \"[\" to look up macros defined in the Path Macros page."));

  m_candidateProvider = &MacroLineEdit::defaultMacroCandidates;

  // マクロを文字色で強調表示する。既知のマクロ（補完候補＋組み込みのマクロ）だけに色が付く
  m_highlighter = new MacroHighlighter(this, [this]() {
    const QStringList candidates =
        m_candidateProvider ? m_candidateProvider() : QStringList();
    return m_highlightOnlyCandidates ? candidates : knownMacros(candidates);
  });
}

void MacroLineEdit::setHighlightOnlyCandidates(bool on) {
  if (m_highlightOnlyCandidates == on) return;
  m_highlightOnlyCandidates = on;
  m_highlighter->rehighlight();
}

void MacroLineEdit::setMacroCandidateProvider(
    std::function<QStringList()> provider) {
  m_candidateProvider = provider;
  m_highlighter->rehighlight();
}

void MacroLineEdit::setText(const QString& text) {
  setPlainText(toSingleLine(text));
}

void MacroLineEdit::setCursorPosition(int pos) {
  QTextCursor cursor = textCursor();
  cursor.setPosition(qBound(0, pos, toPlainText().size()));
  setTextCursor(cursor);
}

QSize MacroLineEdit::sizeHint() const {
  return QSize(QLineEdit().sizeHint().width(), height());
}

QSize MacroLineEdit::minimumSizeHint() const {
  return QSize(QLineEdit().minimumSizeHint().width(), height());
}

void MacroLineEdit::insertFromMimeData(const QMimeData* source) {
  // 貼り付けた文字列の改行は、空白に置き換える
  if (!source->hasText()) return;
  insertPlainText(toSingleLine(source->text()));
}

void MacroLineEdit::focusOutEvent(QFocusEvent* event) {
  QPlainTextEdit::focusOutEvent(event);
  // 補完ポップアップの表示やウィンドウの切り替えによるフォーカス移動では、編集を終えたとはみなさない
  if (event->reason() != Qt::PopupFocusReason &&
      event->reason() != Qt::ActiveWindowFocusReason)
    emit editingFinished();
}

void MacroLineEdit::keyPressEvent(QKeyEvent* event) {
  // 補完ポップアップの表示中は、上下キーやEnter/Escなどの操作を補完側に譲る
  if (m_completer->popup()->isVisible()) {
    switch (event->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
    case Qt::Key_Escape:
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
      event->ignore();
      return;
    default:
      break;
    }
  }

  // 1行の入力欄なので、Enterは改行ではなく、編集の確定にする
  if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
    event->accept();
    emit editingFinished();
    return;
  }

  QPlainTextEdit::keyPressEvent(event);

  if (event->text() == "[") {
    m_macroStartPos = cursorPosition() - 1;
    QStringList macros =
        m_candidateProvider ? m_candidateProvider() : QStringList();
    m_completer->setModel(new QStringListModel(macros, m_completer));
    m_completer->setCompletionPrefix("[");
    m_completer->complete();
    return;
  }

  if (m_macroStartPos >= 0) {
    if (cursorPosition() <= m_macroStartPos) {
      m_macroStartPos = -1;
      m_completer->popup()->hide();
    } else {
      QString prefix =
          text().mid(m_macroStartPos, cursorPosition() - m_macroStartPos);
      m_completer->setCompletionPrefix(prefix);
      if (m_completer->completionCount() > 0)
        m_completer->complete();
      else
        m_completer->popup()->hide();
    }
  }
}

void MacroLineEdit::onCompleterActivated(const QString& macroStr) {
  if (m_macroStartPos < 0) return;
  // "["から現在のカーソル位置までを、選ばれたマクロに置き換える（Undoできるよう、カーソル操作で置き換える）
  QTextCursor cursor = textCursor();
  cursor.setPosition(m_macroStartPos);
  cursor.setPosition(textCursor().position(), QTextCursor::KeepAnchor);
  cursor.insertText(macroStr);
  setTextCursor(cursor);
  m_macroStartPos = -1;
}

//--------------------------------

namespace {
// QTableWidgetItem::setBackground()で設定した背景色（Qt::BackgroundRole）があると、
// スタイル・スタイルシート経由の選択ハイライト（selection-background-color等）が
// プラットフォームやスタイルによっては効かないことがある。
// また、非選択時の描画だけQStyledItemDelegate::paint()（＝ネイティブスタイル）に任せると、
// Windows
// Vistaスタイル等が行う選択切り替え時のフェードアニメーションの中間フレームで
// スタイル既定のグレーが一瞬（クリックで切り替えた時だけ）挟まることがある
// （ドラッグでの連続切り替えではアニメーションが完了しないため表面化しない）。
// そのため選択・非選択のどちらもスタイルを介さず自前で描画し、アニメーションの影響を受けないようにする
class SelectionHighlightDelegate : public QStyledItemDelegate {
  QColor m_selectedBgColor;
  QColor m_selectedTextColor;

public:
  SelectionHighlightDelegate(const QColor& selectedBgColor,
                             const QColor& selectedTextColor,
                             QObject* parent = nullptr)
      : QStyledItemDelegate(parent)
      , m_selectedBgColor(selectedBgColor)
      , m_selectedTextColor(selectedTextColor) {}

protected:
  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override {
    bool selected = option.state & QStyle::State_Selected;

    QColor bgColor;
    QColor textColor;
    if (selected) {
      bgColor   = m_selectedBgColor;
      textColor = m_selectedTextColor;
    } else {
      QVariant bgData = index.data(Qt::BackgroundRole);
      bgColor         = bgData.canConvert<QBrush>()
                            ? bgData.value<QBrush>().color()
                            : option.palette.color(QPalette::Base);
      QVariant fgData = index.data(Qt::ForegroundRole);
      textColor       = fgData.canConvert<QBrush>()
                            ? fgData.value<QBrush>().color()
                            : option.palette.color(QPalette::Text);
    }

    painter->save();
    painter->fillRect(option.rect, bgColor);
    painter->setPen(textColor);
    QRect textRect = option.rect.adjusted(
        4, 0, -4, 0);  // QTableWidgetItemの既定の左右余白に合わせる
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                      index.data(Qt::DisplayRole).toString());
    painter->restore();
  }
};

// QTableWidget既定のInternalMoveドロップ処理は、セルの真上（行の境界線上ではない位置）に
// ドロップされると行単位ではなくセル単位でデータを移動してしまい、列の内容がずれたり
// 空セルができたりする不具合がある。dropEventを完全に上書きし、ドロップ位置から
// 挿入先の行だけを判定して、行全体（QTableWidgetItem群）を手動で移動する。
class ReorderableTableWidget : public QTableWidget {
public:
  std::function<void()> onRowsReordered;
  using QTableWidget::QTableWidget;

protected:
  void dropEvent(QDropEvent* event) override {
    if (event->source() != this || columnCount() == 0) {
      QTableWidget::dropEvent(event);
      return;
    }
    int srcRow = currentRow();
    if (srcRow < 0) {
      event->ignore();
      return;
    }

    int dropY    = event->position().toPoint().y();
    int hoverRow = rowAt(dropY);
    int destRow;
    if (hoverRow < 0) {
      destRow = rowCount();  // テーブル最後尾より下にドロップされた
    } else {
      QRect r = visualRect(model()->index(hoverRow, 0));
      destRow = (dropY - r.top() > r.height() / 2) ? hoverRow + 1 : hoverRow;
    }
    if (destRow > srcRow) destRow--;  // 移動元の行を抜いた後の挿入位置に補正

    // ここでdropActionをMoveActionのまま acceptすると、dropEvent終了後に
    // QAbstractItemView::startDrag()がdrag->exec()の戻り値をMoveActionと判定し、
    // 自前でのdropEvent処理とは別に、ドラッグ開始時に選択されていた行を
    // 重ねて自動削除してしまう（これが「セル上にドロップすると行が消える」原因）。
    // 行の移動は既にここで完結させているので、IgnoreActionにしてこの二重削除を防ぐ。
    event->setDropAction(Qt::IgnoreAction);
    event->accept();

    if (destRow == srcRow) return;

    QList<QTableWidgetItem*> items;
    for (int c = 0; c < columnCount(); c++) items.append(takeItem(srcRow, c));
    removeRow(srcRow);
    QTableWidget::insertRow(destRow);
    for (int c = 0; c < columnCount(); c++) setItem(destRow, c, items.at(c));
    setCurrentCell(destRow, 0);

    if (onRowsReordered) onRowsReordered();
  }
};
};  // namespace

WorkflowPage::WorkflowPage(QWidget* parent)
    : QWidget(parent)
    , m_currentWfIndex(-1)
    , m_currentGroupId(Group_None)
    , m_col2Container(nullptr)
    , m_protocolTable(nullptr)
    , m_currentProtocolIndex(-1)
    , m_col3Container(nullptr)
    , m_col3HostLayout(nullptr)
    , m_actionTable(nullptr)
    , m_currentActionIndex(-1)
    , m_col4Container(nullptr)
    , m_stepTable(nullptr)
    , m_nextFlowsList(nullptr)
    , m_stepDetailContainer(nullptr)
    , m_currentStepIndex(-1) {
  // カラム①：Workflow一覧
  ReorderableTableWidget* workflowListTable =
      new ReorderableTableWidget(0, 2, this);
  m_workflowList = workflowListTable;
  m_workflowList->setHorizontalHeaderLabels({tr("ID"), tr("Name")});
  m_workflowList->verticalHeader()->setVisible(false);
  m_workflowList->horizontalHeader()->setSectionResizeMode(
      0, QHeaderView::ResizeToContents);
  m_workflowList->horizontalHeader()->setSectionResizeMode(
      1, QHeaderView::Stretch);
  m_workflowList->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_workflowList->setSelectionMode(QAbstractItemView::SingleSelection);
  m_workflowList->setEditTriggers(
      QAbstractItemView::NoEditTriggers);  // 一覧では編集せず、②で編集する
  // 各行にWorkflow::colorの背景色を設定しているため、選択時のハイライトが隠れてしまう。
  // スタイルシートに頼らず、選択中のセルは自前で背景・文字色を描画するデリゲートを使う
  m_workflowList->setItemDelegate(new SelectionHighlightDelegate(
      QColor(0, 0, 136), Qt::white, m_workflowList));
  // 行のドラッグ＆ドロップによる並べ替えを可能にする
  m_workflowList->setDragEnabled(true);
  m_workflowList->setAcceptDrops(true);
  m_workflowList->setDropIndicatorShown(true);
  m_workflowList->setDragDropOverwriteMode(false);
  m_workflowList->setDragDropMode(QAbstractItemView::InternalMove);

  QPushButton* addWfButton    = new QPushButton(tr("Add"), this);
  QPushButton* removeWfButton = new QPushButton(tr("Remove"), this);

  m_col1Widget         = new QWidget(this);
  QWidget* col1        = m_col1Widget;
  QVBoxLayout* col1Lay = new QVBoxLayout(col1);
  col1Lay->setContentsMargins(5, 5, 5, 5);
  {
    col1Lay->addWidget(m_workflowList, 1);
    QHBoxLayout* buttonsLay = new QHBoxLayout();
    buttonsLay->setContentsMargins(0, 0, 0, 0);
    buttonsLay->addStretch(1);
    buttonsLay->addWidget(addWfButton, 0);
    buttonsLay->addWidget(removeWfButton, 0);
    col1Lay->addLayout(buttonsLay, 0);
  }
  col1->setFixedWidth(250);

  // カラム②（Workflow詳細＋Protocol一覧。選択中Protocolの詳細は一覧の下に表示する）と
  // カラム③（旧④。PPM_Action詳細＋ActionStep一覧＋Step詳細）：中身（コンテナ）は選択に応じて都度作り直す。
  // どちらも内容が縦に伸びるためQScrollAreaで包み、幅は固定せず均等に伸縮させる
  QScrollArea* col2Scroll = new QScrollArea(this);
  col2Scroll->setWidgetResizable(true);
  col2Scroll->setFrameShape(QFrame::NoFrame);
  QWidget* col2Host = new QWidget();
  m_col2HostLayout  = new QVBoxLayout(col2Host);
  m_col2HostLayout->setContentsMargins(5, 5, 5, 5);
  col2Scroll->setWidget(col2Host);

  QScrollArea* col4Scroll = new QScrollArea(this);
  col4Scroll->setWidgetResizable(true);
  col4Scroll->setFrameShape(QFrame::NoFrame);
  QWidget* col4Host = new QWidget();
  m_col4HostLayout  = new QVBoxLayout(col4Host);
  m_col4HostLayout->setContentsMargins(5, 5, 5, 5);
  col4Scroll->setWidget(col4Host);

  QHBoxLayout* mainLay = new QHBoxLayout();
  mainLay->setContentsMargins(5, 5, 5, 5);
  mainLay->setSpacing(5);
  {
    mainLay->addWidget(col1, 0);
    mainLay->addWidget(col2Scroll, 1);
    mainLay->addWidget(col4Scroll, 1);
  }
  setLayout(mainLay);

  connect(addWfButton, SIGNAL(clicked()), this, SLOT(onAddWorkflowClicked()));
  connect(removeWfButton, SIGNAL(clicked()), this,
          SLOT(onRemoveWorkflowClicked()));
  connect(m_workflowList, &QTableWidget::currentCellChanged, this,
          [this](int currentRow, int, int, int) {
            syncCurrentSelectionFromRow(currentRow);
            m_currentProtocolIndex = -1;
            m_currentActionIndex   = -1;
            m_currentStepIndex     = -1;
            rebuildColumn2();
            rebuildColumn3();
            rebuildColumn4();
          });
  // 行がドラッグ＆ドロップで入れ替わったら、tmpSettings.workflowsの順序も表示順に合わせる。
  // 見出し行はドラッグできない（Qt::ItemIsDragEnabledを外している）ため、srcRowは常にWorkflow行。
  // 移動先が別グループの見出しの配下だった場合は、そのWorkflowのgroupIdも更新する
  // （ReorderableTableWidget::dropEventが行の手動移動を終えた後に呼び出すコールバック）
  workflowListTable->onRowsReordered = [this]() {
    QList<Workflow> reordered;
    FlowGroupId currentGroupContext = Group_None;
    for (int row = 0; row < m_workflowList->rowCount(); row++) {
      if (isHeaderRow(row)) {
        currentGroupContext = headerRowGroupId(row);
        continue;
      }
      QTableWidgetItem* idItem = m_workflowList->item(row, 0);
      if (!idItem) continue;
      bool ok         = false;
      unsigned int id = idItem->text().toUInt(&ok);
      if (!ok) continue;
      for (auto& wf : tmpSettings.workflows) {
        if ((unsigned int)wf.workflowType == id) {
          Workflow moved = wf;
          moved.groupId  = currentGroupContext;
          reordered.append(moved);
          break;
        }
      }
    }
    if (reordered.size() != tmpSettings.workflows.size())
      return;  // 対応が取れない場合は何もしない

    tmpSettings.workflows = reordered;
    markDirty();
    rebuildWorkflowListRows();  // グループが変わった場合の再配置に備え、見出し行を含めて作り直す
  };
}

bool WorkflowPage::isHeaderRow(int row) const {
  QTableWidgetItem* item = m_workflowList->item(row, 0);
  return item && item->data(Qt::UserRole).toBool();
}

FlowGroupId WorkflowPage::headerRowGroupId(int row) const {
  QTableWidgetItem* item = m_workflowList->item(row, 0);
  return item ? (FlowGroupId)item->data(Qt::UserRole + 1).toUInt() : Group_None;
}

int WorkflowPage::tableRowForWorkflowIndex(int index) const {
  if (index < 0 || index >= tmpSettings.workflows.size()) return -1;
  QString idText =
      QString::number((uint)tmpSettings.workflows.at(index).workflowType);
  for (int row = 0; row < m_workflowList->rowCount(); row++) {
    if (isHeaderRow(row)) continue;
    QTableWidgetItem* item = m_workflowList->item(row, 0);
    if (item && item->text() == idText) return row;
  }
  return -1;
}

namespace {
// Workflow一覧の工程グループ見出し行を1行構築する。列0のアイテムにQt::UserRole/UserRole+1で
// 「見出し行であること」「対応するgroupId」を埋め込み、ドラッグ不可にする
void buildWorkflowGroupHeaderRow(QTableWidget* table, FlowGroupId groupId,
                                 const QString& labelText) {
  int row = table->rowCount();
  table->insertRow(row);
  QTableWidgetItem* headerItem = new QTableWidgetItem(labelText);
  headerItem->setData(Qt::UserRole, true);
  headerItem->setData(Qt::UserRole + 1, (uint)groupId);
  headerItem->setFlags(
      (headerItem->flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable) &
      ~Qt::ItemIsDragEnabled & ~Qt::ItemIsEditable);
  headerItem->setBackground(Qt::black);
  headerItem->setForeground(Qt::white);
  QFont headerFont = headerItem->font();
  headerFont.setBold(true);
  headerItem->setFont(headerFont);
  table->setItem(row, 0, headerItem);
  table->setSpan(row, 0, 1, 2);
}
};  // namespace

// 見出し行を含むm_workflowListの行を、tmpSettings.workflows/flowGroupInfosから作り直す
// （選択状態には触れない。呼び出し側で選択の保存・復元を行う）
void WorkflowPage::buildWorkflowListRows() {
  m_workflowList->setRowCount(0);

  QSet<unsigned int> renderedTypes;
  for (auto groupId : tmpSettings.flowGroupInfos.keys()) {  // QMapなので昇順
    buildWorkflowGroupHeaderRow(
        m_workflowList, groupId,
        tr("%1  %2").arg(groupId).arg(
            tmpSettings.flowGroupInfos.value(groupId).labelName));

    for (auto& wf : tmpSettings.workflows) {
      if (wf.groupId != groupId) continue;
      int row = m_workflowList->rowCount();
      m_workflowList->insertRow(row);
      m_workflowList->setItem(
          row, 0, new QTableWidgetItem(QString::number((uint)wf.workflowType)));
      m_workflowList->setItem(row, 1, new QTableWidgetItem(wf.name));
      m_workflowList->item(row, 1)->setBackground(wf.color);
      renderedTypes.insert((uint)wf.workflowType);
    }
  }

  // 現在存在しない工程グループに属する工程（グループが削除された等の異常系）を、
  // データを消さず末尾に表示するための保険
  QList<Workflow> orphaned;
  for (auto& wf : tmpSettings.workflows)
    if (!renderedTypes.contains((uint)wf.workflowType)) orphaned.append(wf);
  if (!orphaned.isEmpty()) {
    buildWorkflowGroupHeaderRow(m_workflowList, Group_None,
                                tr("(Unknown Group)"));
    for (auto& wf : orphaned) {
      int row = m_workflowList->rowCount();
      m_workflowList->insertRow(row);
      m_workflowList->setItem(
          row, 0, new QTableWidgetItem(QString::number((uint)wf.workflowType)));
      m_workflowList->setItem(row, 1, new QTableWidgetItem(wf.name));
      m_workflowList->item(row, 1)->setBackground(wf.color);
    }
  }
}

// 現在のm_workflowListの選択行から、m_currentWfIndex/m_currentGroupIdを算出する
// （行自体の選択操作は行わない。setCurrentCell等は呼び出し側で行ってから呼ぶ）
void WorkflowPage::syncCurrentSelectionFromRow(int row) {
  if (row < 0) {
    m_currentWfIndex = -1;
    m_currentGroupId = Group_None;
    return;
  }
  if (isHeaderRow(row)) {
    m_currentWfIndex = -1;
    m_currentGroupId = headerRowGroupId(row);
    return;
  }
  QTableWidgetItem* idItem = m_workflowList->item(row, 0);
  WorkflowType wfType =
      idItem ? (WorkflowType)idItem->text().toUInt() : Wf_Undefined;
  m_currentWfIndex = -1;
  for (int i = 0; i < tmpSettings.workflows.size(); i++) {
    if (tmpSettings.workflows.at(i).workflowType == wfType) {
      m_currentWfIndex = i;
      break;
    }
  }
  m_currentGroupId = (m_currentWfIndex >= 0)
                         ? tmpSettings.workflows.at(m_currentWfIndex).groupId
                         : Group_None;
}

void WorkflowPage::refresh() {
  buildWorkflowListRows();

  // Workflowが1つでもあれば先頭のWorkflow行を、無ければ先頭の見出し行を選択する
  int defaultRow = -1;
  for (int row = 0; row < m_workflowList->rowCount(); row++) {
    if (!isHeaderRow(row)) {
      defaultRow = row;
      break;
    }
  }
  if (defaultRow < 0 && m_workflowList->rowCount() > 0) defaultRow = 0;

  m_workflowList->blockSignals(true);
  if (defaultRow >= 0) m_workflowList->setCurrentCell(defaultRow, 0);
  m_workflowList->blockSignals(false);

  // selectRow等のシグナル発火に頼らず、常に明示的に各カラムを作り直す
  syncCurrentSelectionFromRow(defaultRow);
  m_currentProtocolIndex = -1;
  m_currentActionIndex   = -1;
  m_currentStepIndex     = -1;
  rebuildColumn2();
  rebuildColumn3();
  rebuildColumn4();
}

// tmpSettings.workflows/flowGroupInfosの変更後、見出し行を含むm_workflowListの表示を作り直す。
// 可能な限り選択中の項目（WorkflowならworkflowType、見出しならgroupId）を引き継いで選択し直す
void WorkflowPage::rebuildWorkflowListRows() {
  bool hadHeaderSelected =
      (m_currentWfIndex < 0 && m_currentGroupId != Group_None);
  WorkflowType selectedWfType =
      (m_currentWfIndex >= 0 && m_currentWfIndex < tmpSettings.workflows.size())
          ? tmpSettings.workflows.at(m_currentWfIndex).workflowType
          : Wf_Undefined;
  FlowGroupId selectedHeaderGroupId = m_currentGroupId;

  buildWorkflowListRows();

  int newRow = -1;
  if (hadHeaderSelected) {
    for (int row = 0; row < m_workflowList->rowCount(); row++) {
      if (isHeaderRow(row) && headerRowGroupId(row) == selectedHeaderGroupId) {
        newRow = row;
        break;
      }
    }
  } else if (selectedWfType != Wf_Undefined) {
    QString idText = QString::number((uint)selectedWfType);
    for (int row = 0; row < m_workflowList->rowCount(); row++) {
      if (isHeaderRow(row)) continue;
      QTableWidgetItem* item = m_workflowList->item(row, 0);
      if (item && item->text() == idText) {
        newRow = row;
        break;
      }
    }
  }
  if (newRow < 0 && m_workflowList->rowCount() > 0) newRow = 0;

  m_workflowList->blockSignals(true);
  if (newRow >= 0) m_workflowList->setCurrentCell(newRow, 0);
  m_workflowList->blockSignals(false);

  syncCurrentSelectionFromRow(newRow);
  m_currentProtocolIndex = -1;
  m_currentActionIndex   = -1;
  m_currentStepIndex     = -1;
  rebuildColumn2();
  rebuildColumn3();
  rebuildColumn4();
}

void WorkflowPage::setListColumnVisible(bool visible) {
  m_col1Widget->setVisible(visible);
}

void WorkflowPage::selectWorkflowByType(WorkflowType type) {
  int row        = -1;
  QString idText = QString::number((uint)type);
  for (int r = 0; r < m_workflowList->rowCount(); r++) {
    if (isHeaderRow(r)) continue;
    QTableWidgetItem* item = m_workflowList->item(r, 0);
    if (item && item->text() == idText) {
      row = r;
      break;
    }
  }
  if (row < 0) return;

  m_workflowList->blockSignals(true);
  m_workflowList->setCurrentCell(row, 0);
  m_workflowList->blockSignals(false);

  // currentCellChangedハンドラと同じ処理を明示的に行う
  syncCurrentSelectionFromRow(row);
  m_currentProtocolIndex = -1;
  m_currentActionIndex   = -1;
  m_currentStepIndex     = -1;
  rebuildColumn2();
  rebuildColumn3();
  rebuildColumn4();
}

void WorkflowPage::updateWorkflowListRow(int index) {
  if (index < 0 || index >= tmpSettings.workflows.size()) return;
  int row = tableRowForWorkflowIndex(index);
  if (row < 0) return;
  const Workflow& wf = tmpSettings.workflows.at(index);
  if (auto* item = m_workflowList->item(row, 0))
    item->setText(QString::number((uint)wf.workflowType));
  if (auto* item = m_workflowList->item(row, 1)) {
    item->setText(wf.name);
    item->setBackground(wf.color);
  }
}

// 無名名前空間内のremoveWorkflowReferencesを、フローチャート型ダイアログ側からも
// 呼び出せるように公開するラッパー（宣言はprojectsettings_common.h）
WorkflowReferenceCleanupResult cleanupWorkflowReferencesInTmpSettings(
    WorkflowType deletedId) {
  RemovedWorkflowReferenceInfo info = removeWorkflowReferences(deletedId);
  WorkflowReferenceCleanupResult result;
  result.nextFlowsCount       = info.nextFlowsCount;
  result.releaseProgressCount = info.releaseProgressCount;
  return result;
}

static void renameChatDestinationReferencesInTmpSettings(
    const QString& oldName, const QString& newName) {
  for (auto& wf : tmpSettings.workflows)
    for (auto& protocol : wf.protocols)
      for (auto& action : protocol.actions)
        renameChatDestinationInSteps(action.steps, oldName, newName);
}

static int countFlowGroupReferencesInTmpSettings(const QSet<FlowGroupId>& ids) {
  int count = 0;
  for (auto& wf : tmpSettings.workflows)
    for (auto& protocol : wf.protocols)
      for (auto& action : protocol.actions)
        count += countFlowGroupIdInSteps(action.steps, ids);
  return count;
}

static void remapFlowGroupReferencesInTmpSettings(FlowGroupId oldId,
                                                  FlowGroupId newId) {
  for (auto& wf : tmpSettings.workflows) {
    if (wf.groupId == oldId) wf.groupId = newId;
    for (auto& protocol : wf.protocols)
      for (auto& action : protocol.actions)
        remapFlowGroupIdInSteps(action.steps, oldId, newId);
  }
  for (auto& groupNode : tmpSettings.flowChartLayout.groupNodes)
    if (groupNode.groupId == oldId) groupNode.groupId = newId;

  // IDの変更で工程グループの順番が変わるため、tmpSettings.workflowsを
  // 工程グループ順のクラスタに並べ直す（グループ内の順番は保つ。未知のグループは末尾）
  QList<FlowGroupId> orderedGroupIds = tmpSettings.flowGroupInfos.keys();
  auto groupOrder = [&orderedGroupIds](FlowGroupId id) {
    int pos = orderedGroupIds.indexOf(id);
    return (pos < 0) ? orderedGroupIds.size() : pos;
  };
  std::stable_sort(tmpSettings.workflows.begin(), tmpSettings.workflows.end(),
                   [&groupOrder](const Workflow& a, const Workflow& b) {
                     return groupOrder(a.groupId) < groupOrder(b.groupId);
                   });
}

void WorkflowPage::onAddWorkflowClicked() {
  // IDはユーザーが考える必要のない内部管理用の値として、空いている番号を自動採番する
  unsigned int newId = 0;
  for (auto& wf : tmpSettings.workflows)
    newId = std::max(newId, (unsigned int)wf.workflowType + 1);

  // 選択中の工程グループ（見出し行、またはその配下のWorkflowが選択されていればそのグループ）の
  // 末尾に追加する。何も選択されていなければ先頭の工程グループに追加する
  FlowGroupId targetGroupId = m_currentGroupId;
  if (targetGroupId == Group_None && !tmpSettings.flowGroupInfos.isEmpty())
    targetGroupId = tmpSettings.flowGroupInfos.keys().first();

  Workflow wf;
  wf.workflowType = (WorkflowType)newId;
  wf.name         = tr("New Workflow");
  wf.color        = QColor(Qt::white);
  wf.groupId      = targetGroupId;

  int insertIndex = insertPositionForWorkflowGroupEnd(targetGroupId);
  tmpSettings.workflows.insert(insertIndex, wf);
  markDirty();

  m_currentWfIndex = insertIndex;
  m_currentGroupId = targetGroupId;
  rebuildWorkflowListRows();  // 新規Workflowを選択した状態で一覧を作り直す
}

void WorkflowPage::onRemoveWorkflowClicked() {
  // 選択された行を、tmpSettings.workflows内のインデックスへ変換する（見出し行は対象外）
  QSet<int> indices;
  for (auto* item : m_workflowList->selectedItems()) {
    int row = item->row();
    if (isHeaderRow(row)) continue;
    QTableWidgetItem* idItem = m_workflowList->item(row, 0);
    if (!idItem) continue;
    bool ok         = false;
    unsigned int id = idItem->text().toUInt(&ok);
    if (!ok) continue;
    for (int i = 0; i < tmpSettings.workflows.size(); i++) {
      if ((unsigned int)tmpSettings.workflows.at(i).workflowType == id) {
        indices.insert(i);
        break;
      }
    }
  }
  if (indices.isEmpty()) return;

  QList<int> sortedIndices = indices.values();
  std::sort(sortedIndices.begin(), sortedIndices.end(), std::greater<int>());

  int totalNextFlowsRemoved     = 0;
  int totalReleaseProgressReset = 0;
  for (int index : sortedIndices) {
    WorkflowType deletedId = tmpSettings.workflows.at(index).workflowType;
    RemovedWorkflowReferenceInfo info = removeWorkflowReferences(deletedId);
    totalNextFlowsRemoved += info.nextFlowsCount;
    totalReleaseProgressReset += info.releaseProgressCount;
    tmpSettings.workflows.removeAt(index);
  }

  if (totalNextFlowsRemoved > 0 || totalReleaseProgressReset > 0) {
    QStringList messages;
    if (totalNextFlowsRemoved > 0)
      messages.append(tr("%1 reference(s) in \"Next Flows\" were removed.")
                          .arg(totalNextFlowsRemoved));
    if (totalReleaseProgressReset > 0)
      messages.append(
          tr("%1 \"Release Progress\" / \"Add Next Workflow\" step(s) "
             "referencing the deleted workflow(s) were reset.")
              .arg(totalReleaseProgressReset));
    QMessageBox::information(this, tr("References Removed"),
                             messages.join("\n"));
  }

  markDirty();
  refresh();
}

void WorkflowPage::updateProtocolTableRow(int index) {
  if (!m_protocolTable) return;
  if (m_currentWfIndex < 0 || m_currentWfIndex >= tmpSettings.workflows.size())
    return;
  const QList<WorkflowProtocol>& protocols =
      tmpSettings.workflows.at(m_currentWfIndex).protocols;
  if (index < 0 || index >= protocols.size()) return;
  if (auto* item = m_protocolTable->item(index, 0))
    item->setText(mediaTypeDisplayName(protocols.at(index).mediaType));
  if (auto* item = m_protocolTable->item(index, 1))
    item->setText(protocols.at(index).conditionPath);
}

void WorkflowPage::rebuildColumn2() {
  if (m_col2Container) {
    // m_col3Container/m_actionTable/m_col3HostLayoutは、旧カラム③を統合した際に
    // m_col2Containerの子（protocolDetailHost経由）になったため、このdeleteで
    // まとめて破棄される。ダングリングポインタとして残らないよう明示的にnullにする
    delete m_col2Container;
    m_col2Container  = nullptr;
    m_protocolTable  = nullptr;
    m_col3Container  = nullptr;
    m_actionTable    = nullptr;
    m_col3HostLayout = nullptr;
  }

  m_col2Container  = new QWidget();
  QVBoxLayout* lay = new QVBoxLayout(m_col2Container);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(8);
  m_col2HostLayout->addWidget(m_col2Container);

  if (m_currentWfIndex < 0 ||
      m_currentWfIndex >= tmpSettings.workflows.size()) {
    lay->addWidget(new QLabel(tr("No workflow selected."), this));
    lay->addStretch(1);
    return;
  }

  int index    = m_currentWfIndex;
  Workflow& wf = tmpSettings.workflows[index];

  // このWorkflowが選択された際、Protocolが1つ以上あれば先頭を自動選択する
  if (m_currentProtocolIndex < 0 && !wf.protocols.isEmpty())
    m_currentProtocolIndex = 0;

  QFormLayout* formLay = newProjectFormLayout();
  formLay->setContentsMargins(0, 0, 0, 0);

  // IDはユーザーが編集できない内部管理用の値（自動採番）。参照用に表示のみ行う
  QLabel* idLabel = new QLabel(QString::number((unsigned int)wf.workflowType));
  idLabel->setToolTip(
      tr("This ID is assigned automatically and cannot be changed."));
  formLay->addRow(tr("ID"), idLabel);

  QLineEdit* nameEdit = new QLineEdit(wf.name);
  connect(nameEdit, &QLineEdit::editingFinished, this,
          [this, index, nameEdit]() {
            if (index < 0 || index >= tmpSettings.workflows.size()) return;
            tmpSettings.workflows[index].name = nameEdit->text();
            updateWorkflowListRow(index);
            markDirty();
          });
  formLay->addRow(tr("Name"), nameEdit);

  QLineEdit* suffixEdit = new QLineEdit(wf.suffix);
  suffixEdit->setToolTip(
      tr("The string appended to the end of the cue file name for the next "
         "workflow(s).\n"
         "Used by \"Auto-configure renaming chains for condition paths\".\n"
         "Often tied to a role (e.g. Director Check -> \"_k\", Enshutsu-Joshu "
         "Check -> \"_ej\")."));
  connect(suffixEdit, &QLineEdit::editingFinished, this,
          [this, index, suffixEdit]() {
            if (index < 0 || index >= tmpSettings.workflows.size()) return;
            tmpSettings.workflows[index].suffix = suffixEdit->text();
            markDirty();
          });
  formLay->addRow(tr("Suffix"), suffixEdit);

  QComboBox* groupCombo = new QComboBox();
  for (auto fgId : tmpSettings.flowGroupInfos.keys())
    groupCombo->addItem(tmpSettings.flowGroupInfos.value(fgId).labelName,
                        (uint)fgId);
  groupCombo->setCurrentIndex(groupCombo->findData((uint)wf.groupId));
  connect(groupCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this, index, groupCombo](int) {
            if (index < 0 || index >= tmpSettings.workflows.size()) return;
            FlowGroupId newGroupId =
                (FlowGroupId)groupCombo->currentData().toUInt();
            if (tmpSettings.workflows.at(index).groupId == newGroupId) return;
            WorkflowType movedType =
                tmpSettings.workflows.at(index).workflowType;
            // 工程グループのリスト順でクラスタリングされた状態を保つため、一覧上もそのグループの末尾へ移動する
            moveWorkflowToGroupEnd(index, newGroupId);
            markDirty();
            // groupCombo自身を含むcolumn2をこの場で（同期的に）作り直すと、コンボ自身の変更シグナル処理中に
            // 自分自身を削除することになりクラッシュするため、次のイベントループへ遅延する
            QTimer::singleShot(0, this, [this, movedType]() {
              m_currentWfIndex = -1;
              for (int i = 0; i < tmpSettings.workflows.size(); i++) {
                if (tmpSettings.workflows.at(i).workflowType == movedType) {
                  m_currentWfIndex = i;
                  break;
                }
              }
              m_currentGroupId =
                  (m_currentWfIndex >= 0)
                      ? tmpSettings.workflows.at(m_currentWfIndex).groupId
                      : Group_None;
              rebuildWorkflowListRows();
            });
          });
  formLay->addRow(tr("Group"), groupCombo);

  ColorButton* colorButton = new ColorButton();
  colorButton->setColor(wf.color);
  connect(colorButton, &ColorButton::colorChanged, this,
          [this, index](const QColor& color) {
            if (index < 0 || index >= tmpSettings.workflows.size()) return;
            tmpSettings.workflows[index].color = color;
            updateWorkflowListRow(index);
            markDirty();
          });
  formLay->addRow(tr("Color"), colorButton);

  QCheckBox* clearsRewindCheck = new QCheckBox();
  clearsRewindCheck->setChecked(wf.clearsRewindOnComplete);
  clearsRewindCheck->setToolTip(
      tr("Clear the REWIND scene info tag when this workflow completes."));
  connect(clearsRewindCheck, &QCheckBox::toggled, this,
          [this, index](bool checked) {
            if (index < 0 || index >= tmpSettings.workflows.size()) return;
            tmpSettings.workflows[index].clearsRewindOnComplete = checked;
            markDirty();
          });
  formLay->addRow(
      tr("The Last Workflow In Group\n( Clears Rewind On Complete )"),
      clearsRewindCheck);

  lay->addLayout(formLay);

  // WorkflowProtocol一覧
  QHBoxLayout* protocolHeaderLay = new QHBoxLayout();
  protocolHeaderLay->setContentsMargins(0, 0, 0, 0);
  protocolHeaderLay->addWidget(new QLabel(tr("Protocols:"), this), 0);
  protocolHeaderLay->addStretch(1);
  QPushButton* addProtocolButton    = new QPushButton(tr("Add"), this);
  QPushButton* removeProtocolButton = new QPushButton(tr("Remove"), this);
  protocolHeaderLay->addWidget(addProtocolButton, 0);
  protocolHeaderLay->addWidget(removeProtocolButton, 0);
  lay->addLayout(protocolHeaderLay);

  m_protocolTable = new QTableWidget((int)wf.protocols.size(), 2, this);
  m_protocolTable->setHorizontalHeaderLabels(
      {tr("Media"), tr("Condition Path")});
  m_protocolTable->verticalHeader()->setVisible(false);
  m_protocolTable->horizontalHeader()->setSectionResizeMode(
      0, QHeaderView::ResizeToContents);
  m_protocolTable->horizontalHeader()->setSectionResizeMode(
      1, QHeaderView::Stretch);
  m_protocolTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_protocolTable->setSelectionMode(QAbstractItemView::SingleSelection);
  m_protocolTable->setEditTriggers(
      QAbstractItemView::NoEditTriggers);  // 一覧では編集せず、③で編集する
  for (int i = 0; i < wf.protocols.size(); i++) {
    m_protocolTable->setItem(i, 0,
                             new QTableWidgetItem(mediaTypeDisplayName(
                                 wf.protocols.at(i).mediaType)));
    m_protocolTable->setItem(
        i, 1, new QTableWidgetItem(wf.protocols.at(i).conditionPath));
  }
  lay->addWidget(m_protocolTable, 1);

  connect(addProtocolButton, &QPushButton::clicked, this, [this, index]() {
    if (index < 0 || index >= tmpSettings.workflows.size()) return;
    WorkflowProtocol protocol;
    protocol.mediaType = Digital;
    tmpSettings.workflows[index].protocols.append(protocol);
    m_currentProtocolIndex = tmpSettings.workflows[index].protocols.size() - 1;
    m_currentActionIndex   = -1;
    m_currentStepIndex     = -1;
    markDirty();
    rebuildColumn2();
    rebuildColumn3();
    rebuildColumn4();
  });
  connect(removeProtocolButton, &QPushButton::clicked, this, [this, index]() {
    if (index < 0 || index >= tmpSettings.workflows.size()) return;
    if (!m_protocolTable) return;
    QSet<int> rows;
    for (auto* item : m_protocolTable->selectedItems())
      rows.insert(item->row());
    if (rows.isEmpty()) return;
    QList<int> sortedRows = rows.values();
    std::sort(sortedRows.begin(), sortedRows.end(), std::greater<int>());
    for (int row : sortedRows)
      tmpSettings.workflows[index].protocols.removeAt(row);
    m_currentProtocolIndex = -1;
    m_currentActionIndex   = -1;
    m_currentStepIndex     = -1;
    markDirty();
    rebuildColumn2();
    rebuildColumn3();
    rebuildColumn4();
  });

  connect(m_protocolTable, &QTableWidget::currentCellChanged, this,
          [this](int currentRow, int, int, int) {
            m_currentProtocolIndex = currentRow;
            m_currentActionIndex   = -1;
            m_currentStepIndex     = -1;
            rebuildColumn3();
            rebuildColumn4();
          });

  // 選択中Protocolの詳細（旧カラム③相当）は、Protocol一覧の下に表示する
  QWidget* protocolDetailHost = new QWidget();
  m_col3HostLayout            = new QVBoxLayout(protocolDetailHost);
  m_col3HostLayout->setContentsMargins(0, 0, 0, 0);
  lay->addWidget(protocolDetailHost);
  lay->addStretch(1);

  if (m_currentProtocolIndex >= 0 &&
      m_currentProtocolIndex < m_protocolTable->rowCount())
    m_protocolTable->selectRow(m_currentProtocolIndex);
  else
    rebuildColumn3();
}

void WorkflowPage::updateActionTableRow(int index) {
  if (!m_actionTable) return;
  if (m_currentWfIndex < 0 || m_currentWfIndex >= tmpSettings.workflows.size())
    return;
  const QList<WorkflowProtocol>& protocols =
      tmpSettings.workflows.at(m_currentWfIndex).protocols;
  if (m_currentProtocolIndex < 0 || m_currentProtocolIndex >= protocols.size())
    return;
  const QList<PPM_Action>& actions =
      protocols.at(m_currentProtocolIndex).actions;
  if (index < 0 || index >= actions.size()) return;
  if (auto* item = m_actionTable->item(index, 0))
    item->setText(actions.at(index).actionName);
}

void WorkflowPage::rebuildColumn3() {
  if (m_col3Container) {
    delete m_col3Container;
    m_col3Container = nullptr;
    m_actionTable   = nullptr;
  }

  if (!m_col3HostLayout) return;

  m_col3Container  = new QWidget();
  QVBoxLayout* lay = new QVBoxLayout(m_col3Container);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(8);
  m_col3HostLayout->addWidget(m_col3Container);

  bool hasValidProtocol =
      m_currentWfIndex >= 0 &&
      m_currentWfIndex < tmpSettings.workflows.size() &&
      m_currentProtocolIndex >= 0 &&
      m_currentProtocolIndex <
          tmpSettings.workflows.at(m_currentWfIndex).protocols.size();
  if (!hasValidProtocol) {
    lay->addWidget(new QLabel(tr("No protocol selected."), this));
    lay->addStretch(1);
    // rebuildColumn2()がProtocol未選択のままここへ直接分岐してきた場合など、
    // このままではカラム④が古い内容のまま残ってしまうため、明示的に作り直す
    m_currentActionIndex = -1;
    m_currentStepIndex   = -1;
    rebuildColumn4();
    return;
  }

  int wfIndex    = m_currentWfIndex;
  int protoIndex = m_currentProtocolIndex;
  WorkflowProtocol& protocol =
      tmpSettings.workflows[wfIndex].protocols[protoIndex];

  // このProtocolが選択された際、Actionが1つ以上あれば先頭を自動選択する
  if (m_currentActionIndex < 0 && !protocol.actions.isEmpty())
    m_currentActionIndex = 0;

  QFormLayout* formLay = newProjectFormLayout();
  formLay->setContentsMargins(0, 0, 0, 0);

  QComboBox* mediaCombo = new QComboBox();
  mediaCombo->addItem(mediaTypeDisplayName(Digital), Digital);
  mediaCombo->addItem(mediaTypeDisplayName(Paper), Paper);
  mediaCombo->addItem(mediaTypeDisplayName(Other), Other);
  mediaCombo->setCurrentIndex(mediaCombo->findData(protocol.mediaType));
  connect(mediaCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this, wfIndex, protoIndex, mediaCombo](int) {
            if (wfIndex < 0 || wfIndex >= tmpSettings.workflows.size()) return;
            QList<WorkflowProtocol>& protocols =
                tmpSettings.workflows[wfIndex].protocols;
            if (protoIndex < 0 || protoIndex >= protocols.size()) return;
            protocols[protoIndex].mediaType =
                (MediaType)mediaCombo->currentData().toInt();
            updateProtocolTableRow(protoIndex);
            markDirty();
          });
  formLay->addRow(tr("Media Type"), mediaCombo);

  MacroLineEdit* conditionPathEdit = new MacroLineEdit();
  conditionPathEdit->setText(protocol.conditionPath);
  connect(conditionPathEdit, &MacroLineEdit::editingFinished, this,
          [this, wfIndex, protoIndex, conditionPathEdit]() {
            if (wfIndex < 0 || wfIndex >= tmpSettings.workflows.size()) return;
            QList<WorkflowProtocol>& protocols =
                tmpSettings.workflows[wfIndex].protocols;
            if (protoIndex < 0 || protoIndex >= protocols.size()) return;
            protocols[protoIndex].conditionPath = conditionPathEdit->text();
            updateProtocolTableRow(protoIndex);
            markDirty();
          });

  // 前工程（このWorkflowをnextFlowsに持つアクション）が生成するキューファイルのパスを候補として表示する
  QWidget* conditionPathRow        = new QWidget();
  QHBoxLayout* conditionPathRowLay = new QHBoxLayout(conditionPathRow);
  conditionPathRowLay->setContentsMargins(0, 0, 0, 0);
  conditionPathRowLay->addWidget(conditionPathEdit, 1);
  QPushButton* conditionPathSuggestButton =
      new QPushButton(tr("Suggest..."), conditionPathRow);
  conditionPathSuggestButton->setToolTip(tr(
      "Pick from the cue file path(s) generated by the previous workflow(s)."));
  connect(
      conditionPathSuggestButton, &QPushButton::clicked, this,
      [this, wfIndex, protoIndex, conditionPathEdit,
       conditionPathSuggestButton]() {
        if (wfIndex < 0 || wfIndex >= tmpSettings.workflows.size()) return;
        WorkflowType wfType = tmpSettings.workflows.at(wfIndex).workflowType;

        QStringList candidates;
        for (auto& loc : findActionsLeadingTo(wfType)) {
          for (auto& wf : tmpSettings.workflows) {
            if (wf.workflowType != loc.workflowType) continue;
            if (loc.protocolIdx < 0 || loc.protocolIdx >= wf.protocols.size())
              continue;
            const QList<PPM_Action>& actions =
                wf.protocols.at(loc.protocolIdx).actions;
            if (loc.actionIdx < 0 || loc.actionIdx >= actions.size()) continue;
            candidates +=
                collectGeneratedCueFilePaths(actions.at(loc.actionIdx));
          }
        }
        candidates.removeDuplicates();

        if (candidates.isEmpty()) {
          QMenu emptyMenu;
          emptyMenu.addAction(tr("(No candidates)"))->setEnabled(false);
          emptyMenu.exec(conditionPathSuggestButton->mapToGlobal(
              QPoint(0, conditionPathSuggestButton->height())));
          return;
        }
        QMenu menu;
        for (auto& candidate : candidates) menu.addAction(candidate);
        QAction* chosen = menu.exec(conditionPathSuggestButton->mapToGlobal(
            QPoint(0, conditionPathSuggestButton->height())));
        if (!chosen) return;

        conditionPathEdit->setText(chosen->text());
        QList<WorkflowProtocol>& protocols =
            tmpSettings.workflows[wfIndex].protocols;
        if (protoIndex < 0 || protoIndex >= protocols.size()) return;
        protocols[protoIndex].conditionPath = chosen->text();
        updateProtocolTableRow(protoIndex);
        markDirty();
      });
  conditionPathRowLay->addWidget(conditionPathSuggestButton, 0);
  formLay->addRow(tr("Condition Path"), conditionPathRow);

  lay->addLayout(formLay);

  // PPM_Action一覧
  QHBoxLayout* actionHeaderLay = new QHBoxLayout();
  actionHeaderLay->setContentsMargins(0, 0, 0, 0);
  actionHeaderLay->addWidget(new QLabel(tr("Actions:"), this), 0);
  actionHeaderLay->addStretch(1);
  QPushButton* addActionButton    = new QPushButton(tr("Add"), this);
  QPushButton* removeActionButton = new QPushButton(tr("Remove"), this);
  actionHeaderLay->addWidget(addActionButton, 0);
  actionHeaderLay->addWidget(removeActionButton, 0);
  lay->addLayout(actionHeaderLay);

  m_actionTable = new QTableWidget((int)protocol.actions.size(), 1, this);
  m_actionTable->setHorizontalHeaderLabels({tr("Action Name")});
  m_actionTable->verticalHeader()->setVisible(false);
  m_actionTable->horizontalHeader()->setSectionResizeMode(0,
                                                          QHeaderView::Stretch);
  m_actionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_actionTable->setSelectionMode(QAbstractItemView::SingleSelection);
  m_actionTable->setEditTriggers(
      QAbstractItemView::NoEditTriggers);  // 一覧では編集せず、④で編集する
  for (int i = 0; i < protocol.actions.size(); i++)
    m_actionTable->setItem(
        i, 0, new QTableWidgetItem(protocol.actions.at(i).actionName));
  lay->addWidget(m_actionTable, 1);

  connect(addActionButton, &QPushButton::clicked, this,
          [this, wfIndex, protoIndex]() {
            if (wfIndex < 0 || wfIndex >= tmpSettings.workflows.size()) return;
            QList<WorkflowProtocol>& protocols =
                tmpSettings.workflows[wfIndex].protocols;
            if (protoIndex < 0 || protoIndex >= protocols.size()) return;
            PPM_Action action;
            action.actionName = tr("New Action");
            protocols[protoIndex].actions.append(action);
            m_currentActionIndex = protocols[protoIndex].actions.size() - 1;
            m_currentStepIndex   = -1;
            markDirty();
            rebuildColumn3();
            rebuildColumn4();
          });
  connect(removeActionButton, &QPushButton::clicked, this,
          [this, wfIndex, protoIndex]() {
            if (wfIndex < 0 || wfIndex >= tmpSettings.workflows.size()) return;
            QList<WorkflowProtocol>& protocols =
                tmpSettings.workflows[wfIndex].protocols;
            if (protoIndex < 0 || protoIndex >= protocols.size()) return;
            if (!m_actionTable) return;
            QSet<int> rows;
            for (auto* item : m_actionTable->selectedItems())
              rows.insert(item->row());
            if (rows.isEmpty()) return;
            QList<int> sortedRows = rows.values();
            std::sort(sortedRows.begin(), sortedRows.end(),
                      std::greater<int>());
            for (int row : sortedRows)
              protocols[protoIndex].actions.removeAt(row);
            m_currentActionIndex = -1;
            m_currentStepIndex   = -1;
            markDirty();
            rebuildColumn3();
            rebuildColumn4();
          });

  connect(m_actionTable, &QTableWidget::currentCellChanged, this,
          [this](int currentRow, int, int, int) {
            m_currentActionIndex = currentRow;
            m_currentStepIndex   = -1;
            rebuildColumn4();
          });

  if (m_currentActionIndex >= 0 &&
      m_currentActionIndex < m_actionTable->rowCount())
    m_actionTable->selectRow(m_currentActionIndex);
}

void WorkflowPage::updateStepTableRow(int index) {
  if (!m_stepTable) return;
  if (m_currentWfIndex < 0 || m_currentWfIndex >= tmpSettings.workflows.size())
    return;
  const QList<WorkflowProtocol>& protocols =
      tmpSettings.workflows.at(m_currentWfIndex).protocols;
  if (m_currentProtocolIndex < 0 || m_currentProtocolIndex >= protocols.size())
    return;
  const QList<PPM_Action>& actions =
      protocols.at(m_currentProtocolIndex).actions;
  if (m_currentActionIndex < 0 || m_currentActionIndex >= actions.size())
    return;
  const QList<ActionStep>& steps = actions.at(m_currentActionIndex).steps;
  if (index < 0 || index >= steps.size()) return;
  if (auto* item = m_stepTable->item(index, 0)) {
    item->setText(WorkflowManager::instance()->actionStepTypeLabelName(
        steps.at(index).type));
    item->setIcon(iconForActionStepType(steps.at(index).type));
  }
}

void WorkflowPage::updateNextFlowsMarks() {
  if (!m_nextFlowsList) return;
  if (m_currentWfIndex < 0 || m_currentWfIndex >= tmpSettings.workflows.size())
    return;
  const QList<WorkflowProtocol>& protocols =
      tmpSettings.workflows.at(m_currentWfIndex).protocols;
  if (m_currentProtocolIndex < 0 || m_currentProtocolIndex >= protocols.size())
    return;
  const QList<PPM_Action>& actions =
      protocols.at(m_currentProtocolIndex).actions;
  if (m_currentActionIndex < 0 || m_currentActionIndex >= actions.size())
    return;
  const PPM_Action& action        = actions.at(m_currentActionIndex);
  const QList<WorkflowType> added = action.stepAddedFlows();
  const QList<NextFlowLink> links = action.nextFlowLinks();

  // 文字・色を変えるだけなので、チェック状態の変更とみなされないようシグナルを止める
  const bool wasBlocked = m_nextFlowsList->blockSignals(true);
  for (int i = 0; i < m_nextFlowsList->count(); i++) {
    QListWidgetItem* item   = m_nextFlowsList->item(i);
    const WorkflowType type = (WorkflowType)item->data(Qt::UserRole).toUInt();
    const Workflow* wf      = nullptr;
    for (auto& w : tmpSettings.workflows)
      if (w.workflowType == type) {
        wf = &w;
        break;
      }
    if (!wf) continue;
    QFont font = item->font();
    if (added.contains(type)) {
      bool conditional = false;
      for (const NextFlowLink& l : links)
        if (l.type == type) {
          conditional = l.conditional;
          break;
        }
      item->setText(tr("%1   [added by Add Next Workflow]")
                        .arg(workflowDisplayLabel(*wf)));
      item->setForeground(QColor(0, 110, 205));
      font.setItalic(true);
      item->setToolTip(
          conditional ? tr("This workflow is added by an \"Add Next Workflow\" "
                           "step of this action, only when the "
                           "conditional branch it belongs to is met.")
                      : tr("This workflow is added by an \"Add Next Workflow\" "
                           "step of this action."));
    } else {
      item->setText(workflowDisplayLabel(*wf));
      item->setData(Qt::ForegroundRole, QVariant());
      font.setItalic(false);
      item->setToolTip(QString());
    }
    item->setFont(font);
  }
  m_nextFlowsList->blockSignals(wasBlocked);
}

void WorkflowPage::rebuildColumn4() {
  if (m_col4Container) {
    delete m_col4Container;  // 子であるm_stepTable/m_stepDetailContainerもまとめて破棄される
    m_col4Container       = nullptr;
    m_stepTable           = nullptr;
    m_nextFlowsList       = nullptr;
    m_stepDetailContainer = nullptr;
  }

  m_col4Container  = new QWidget();
  QVBoxLayout* lay = new QVBoxLayout(m_col4Container);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(8);
  m_col4HostLayout->addWidget(m_col4Container);

  bool hasValidAction =
      m_currentWfIndex >= 0 &&
      m_currentWfIndex < tmpSettings.workflows.size() &&
      m_currentProtocolIndex >= 0 &&
      m_currentProtocolIndex <
          tmpSettings.workflows.at(m_currentWfIndex).protocols.size();
  if (hasValidAction) {
    const QList<PPM_Action>& actions =
        tmpSettings.workflows.at(m_currentWfIndex)
            .protocols.at(m_currentProtocolIndex)
            .actions;
    hasValidAction =
        m_currentActionIndex >= 0 && m_currentActionIndex < actions.size();
  }
  if (!hasValidAction) {
    lay->addWidget(new QLabel(tr("No action selected."), this));
    lay->addStretch(1);
    m_currentStepIndex = -1;
    return;
  }

  int wfIndex     = m_currentWfIndex;
  int protoIndex  = m_currentProtocolIndex;
  int actionIndex = m_currentActionIndex;
  PPM_Action& action =
      tmpSettings.workflows[wfIndex].protocols[protoIndex].actions[actionIndex];

  // このActionが選択された際、Stepが1つ以上あれば先頭を自動選択する
  if (m_currentStepIndex < 0 && !action.steps.isEmpty()) m_currentStepIndex = 0;

  QFormLayout* formLay = newProjectFormLayout();
  formLay->setContentsMargins(0, 0, 0, 0);

  QLineEdit* nameEdit = new QLineEdit(action.actionName);
  connect(nameEdit, &QLineEdit::editingFinished, this,
          [this, wfIndex, protoIndex, actionIndex, nameEdit]() {
            if (wfIndex < 0 || wfIndex >= tmpSettings.workflows.size()) return;
            QList<WorkflowProtocol>& protocols =
                tmpSettings.workflows[wfIndex].protocols;
            if (protoIndex < 0 || protoIndex >= protocols.size()) return;
            QList<PPM_Action>& actions = protocols[protoIndex].actions;
            if (actionIndex < 0 || actionIndex >= actions.size()) return;
            actions[actionIndex].actionName = nameEdit->text();
            updateActionTableRow(actionIndex);
            markDirty();
          });
  formLay->addRow(tr("Action Name"), nameEdit);
  lay->addLayout(formLay);

  // Next Flows：他のWorkflowへの参照。チェックボックス付き一覧で複数選択する
  lay->addWidget(new QLabel(tr("Next Flows:"), this));
  QListWidget* nextFlowsList = new QListWidget(this);
  m_nextFlowsList            = nextFlowsList;
  nextFlowsList->setSelectionMode(QAbstractItemView::NoSelection);
  nextFlowsList->setMaximumHeight(120);
  for (auto& otherWf : tmpSettings.workflows) {
    QListWidgetItem* item = new QListWidgetItem(workflowDisplayLabel(otherWf));
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(action.nextFlows.contains(otherWf.workflowType)
                            ? Qt::Checked
                            : Qt::Unchecked);
    item->setData(Qt::UserRole, (uint)otherWf.workflowType);
    nextFlowsList->addItem(item);
  }
  connect(nextFlowsList, &QListWidget::itemChanged, this,
          [this, wfIndex, protoIndex, actionIndex](QListWidgetItem* item) {
            if (wfIndex < 0 || wfIndex >= tmpSettings.workflows.size()) return;
            QList<WorkflowProtocol>& protocols =
                tmpSettings.workflows[wfIndex].protocols;
            if (protoIndex < 0 || protoIndex >= protocols.size()) return;
            QList<PPM_Action>& actions = protocols[protoIndex].actions;
            if (actionIndex < 0 || actionIndex >= actions.size()) return;
            WorkflowType wfType =
                (WorkflowType)item->data(Qt::UserRole).toUInt();
            QList<WorkflowType>& nextFlows = actions[actionIndex].nextFlows;
            if (item->checkState() == Qt::Checked) {
              if (!nextFlows.contains(wfType)) nextFlows.append(wfType);
            } else {
              nextFlows.removeAll(wfType);
            }
            markDirty();
          });
  lay->addWidget(nextFlowsList);
  updateNextFlowsMarks();

  // ActionStep一覧
  QHBoxLayout* stepHeaderLay = new QHBoxLayout();
  stepHeaderLay->setContentsMargins(0, 0, 0, 0);
  stepHeaderLay->addWidget(new QLabel(tr("Steps:"), this), 0);
  stepHeaderLay->addStretch(1);
  QPushButton* addStepButton    = new QPushButton(tr("Add"), this);
  QPushButton* removeStepButton = new QPushButton(tr("Remove"), this);
  stepHeaderLay->addWidget(addStepButton, 0);
  stepHeaderLay->addWidget(removeStepButton, 0);
  lay->addLayout(stepHeaderLay);

  ReorderableTableWidget* stepTableWidget =
      new ReorderableTableWidget((int)action.steps.size(), 1, this);
  m_stepTable = stepTableWidget;
  m_stepTable->setHorizontalHeaderLabels({tr("Step Type")});
  m_stepTable->verticalHeader()->setVisible(false);
  m_stepTable->horizontalHeader()->setSectionResizeMode(0,
                                                        QHeaderView::Stretch);
  m_stepTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_stepTable->setSelectionMode(QAbstractItemView::SingleSelection);
  m_stepTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  // 行のドラッグ＆ドロップによる並べ替えを可能にする
  m_stepTable->setDragEnabled(true);
  m_stepTable->setAcceptDrops(true);
  m_stepTable->setDropIndicatorShown(true);
  m_stepTable->setDragDropOverwriteMode(false);
  m_stepTable->setDragDropMode(QAbstractItemView::InternalMove);
  for (int i = 0; i < action.steps.size(); i++) {
    ActionStepType stepType    = action.steps.at(i).type;
    QTableWidgetItem* stepItem = new QTableWidgetItem(
        WorkflowManager::instance()->actionStepTypeLabelName(stepType));
    stepItem->setIcon(iconForActionStepType(stepType));
    // 並べ替え後にaction.stepsの順序を再構築するため、元のインデックスを保持しておく
    stepItem->setData(Qt::UserRole, i);
    m_stepTable->setItem(i, 0, stepItem);
  }
  lay->addWidget(m_stepTable);

  connect(addStepButton, &QPushButton::clicked, this,
          [this, wfIndex, protoIndex, actionIndex]() {
            if (wfIndex < 0 || wfIndex >= tmpSettings.workflows.size()) return;
            QList<WorkflowProtocol>& protocols =
                tmpSettings.workflows[wfIndex].protocols;
            if (protoIndex < 0 || protoIndex >= protocols.size()) return;
            QList<PPM_Action>& actions = protocols[protoIndex].actions;
            if (actionIndex < 0 || actionIndex >= actions.size()) return;
            ActionStep step;
            step.type   = Step_Rename;
            step.params = defaultParamsForStepType(Step_Rename);
            actions[actionIndex].steps.append(step);
            m_currentStepIndex = actions[actionIndex].steps.size() - 1;
            markDirty();
            rebuildColumn4();
          });
  connect(removeStepButton, &QPushButton::clicked, this,
          [this, wfIndex, protoIndex, actionIndex]() {
            if (wfIndex < 0 || wfIndex >= tmpSettings.workflows.size()) return;
            QList<WorkflowProtocol>& protocols =
                tmpSettings.workflows[wfIndex].protocols;
            if (protoIndex < 0 || protoIndex >= protocols.size()) return;
            QList<PPM_Action>& actions = protocols[protoIndex].actions;
            if (actionIndex < 0 || actionIndex >= actions.size()) return;
            if (!m_stepTable) return;
            QSet<int> rows;
            for (auto* item : m_stepTable->selectedItems())
              rows.insert(item->row());
            if (rows.isEmpty()) return;
            QList<int> sortedRows = rows.values();
            std::sort(sortedRows.begin(), sortedRows.end(),
                      std::greater<int>());
            for (int row : sortedRows) actions[actionIndex].steps.removeAt(row);
            m_currentStepIndex = -1;
            markDirty();
            rebuildColumn4();
          });

  connect(m_stepTable, &QTableWidget::currentCellChanged, this,
          [this](int currentRow, int, int, int) {
            m_currentStepIndex = currentRow;
            rebuildStepDetail();
          });

  // 行がドラッグ＆ドロップで入れ替わったら、action.stepsの順序も表示順に合わせる
  stepTableWidget->onRowsReordered = [this, wfIndex, protoIndex,
                                      actionIndex]() {
    if (wfIndex < 0 || wfIndex >= tmpSettings.workflows.size()) return;
    QList<WorkflowProtocol>& protocols =
        tmpSettings.workflows[wfIndex].protocols;
    if (protoIndex < 0 || protoIndex >= protocols.size()) return;
    QList<PPM_Action>& actions = protocols[protoIndex].actions;
    if (actionIndex < 0 || actionIndex >= actions.size()) return;
    QList<ActionStep>& steps = actions[actionIndex].steps;

    QList<ActionStep> reordered;
    for (int row = 0; row < m_stepTable->rowCount(); row++) {
      QTableWidgetItem* item = m_stepTable->item(row, 0);
      if (!item) continue;
      int originalIndex = item->data(Qt::UserRole).toInt();
      if (originalIndex < 0 || originalIndex >= steps.size()) continue;
      reordered.append(steps.at(originalIndex));
    }
    if (reordered.size() != steps.size())
      return;  // 対応が取れない場合は何もしない
    steps = reordered;
    markDirty();

    // 各行のUserRoleを新しい並び順のインデックスに振り直す。これをしないと、
    // 連続してドラッグ操作を行った際に、既に一度並べ替えたsteps配列に対して
    // 古い（構築時点の）インデックスで参照してしまい、2回目以降の並べ替えが
    // 正しく反映されなくなる
    for (int row = 0; row < m_stepTable->rowCount(); row++) {
      if (auto* item = m_stepTable->item(row, 0))
        item->setData(Qt::UserRole, row);
    }

    // dropEvent内のsetCurrentCell()によってcurrentCellChangedが先に発火し、
    // 並べ替え前のaction.stepsに基づいてrebuildStepDetail()が呼ばれてしまっているため、
    // 正しい順序に更新した上で改めて詳細パネルを作り直す
    m_currentStepIndex = m_stepTable->currentRow();
    rebuildStepDetail();
  };

  // ステップ詳細（パラメータ編集）の土台
  QWidget* stepDetailHost = new QWidget();
  m_stepDetailHostLayout  = new QVBoxLayout(stepDetailHost);
  m_stepDetailHostLayout->setContentsMargins(0, 0, 0, 0);
  lay->addWidget(stepDetailHost);
  lay->addStretch(1);

  if (m_currentStepIndex >= 0 && m_currentStepIndex < m_stepTable->rowCount())
    m_stepTable->selectRow(m_currentStepIndex);
  else
    rebuildStepDetail();
}

void WorkflowPage::rebuildStepDetail() {
  if (m_stepDetailContainer) {
    delete m_stepDetailContainer;
    m_stepDetailContainer = nullptr;
  }

  m_stepDetailContainer = new QWidget();
  QVBoxLayout* lay      = new QVBoxLayout(m_stepDetailContainer);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(8);
  m_stepDetailHostLayout->addWidget(m_stepDetailContainer);

  bool hasValidStep =
      m_currentWfIndex >= 0 && m_currentWfIndex < tmpSettings.workflows.size();
  if (hasValidStep) {
    const QList<WorkflowProtocol>& protocols =
        tmpSettings.workflows.at(m_currentWfIndex).protocols;
    hasValidStep = m_currentProtocolIndex >= 0 &&
                   m_currentProtocolIndex < protocols.size();
    if (hasValidStep) {
      const QList<PPM_Action>& actions =
          protocols.at(m_currentProtocolIndex).actions;
      hasValidStep =
          m_currentActionIndex >= 0 && m_currentActionIndex < actions.size();
      if (hasValidStep) {
        const QList<ActionStep>& steps = actions.at(m_currentActionIndex).steps;
        hasValidStep =
            m_currentStepIndex >= 0 && m_currentStepIndex < steps.size();
      }
    }
  }
  if (!hasValidStep) {
    lay->addWidget(new QLabel(tr("No step selected."), this));
    return;
  }

  int wfIndex     = m_currentWfIndex;
  int protoIndex  = m_currentProtocolIndex;
  int actionIndex = m_currentActionIndex;
  int stepIndex   = m_currentStepIndex;
  StepEditor editor =
      makeTopStepEditor(wfIndex, protoIndex, actionIndex, stepIndex);

  // 現在編集中のActionのnextFlowsから、後工程各Workflowの全プロトコルのConditionPathを集める。
  // RenameTo/CopyTo/FileToBeCreatedフィールドの入力補助ボタンに使う
  QList<WorkflowType> nextFlows = tmpSettings.workflows.at(wfIndex)
                                      .protocols.at(protoIndex)
                                      .actions.at(actionIndex)
                                      .allNextFlows();
  std::function<QStringList()> nextConditionPathProvider = [nextFlows]() {
    QStringList result;
    for (auto next : nextFlows) {
      for (auto& wf : tmpSettings.workflows) {
        if (wf.workflowType != next) continue;
        for (auto& protocol : wf.protocols) {
          if (!protocol.conditionPath.isEmpty())
            result.append(protocol.conditionPath);
        }
      }
    }
    result.removeDuplicates();
    return result;
  };

  buildStepParamsForm(
      lay, m_stepDetailContainer, editor,
      [this, stepIndex]() {
        updateStepTableRow(stepIndex);
        updateNextFlowsMarks();  // Step_AddProgressの工程の指定が変わった場合に、Next
                                 // Flows一覧へ反映する
        markDirty();  // このonChangedはActionStepパラメータ編集フォームの全フィールドが共有する変更通知経路
        // このonChangedはStep
        // Typeコンボ等、m_stepDetailContainerの子であるQComboBoxの
        // currentIndexChangedからも直接呼ばれる。QComboBoxは自身が破棄されることを想定した
        // 安全策（QPushButtonのclickedにあるようなガード）を持たないため、シグナル処理の
        // 途中でrebuildStepDetail()（=コンボ自身を含むコンテナのdelete）を同期的に行うと
        // 解放済みメモリへのアクセスでクラッシュする。イベントループに戻ってから実行する。
        QTimer::singleShot(0, this, [this]() { rebuildStepDetail(); });
      },
      nextConditionPathProvider);
}

//--------------------------------

QStringList ProjectSettingsDialog::categoryNames() {
  return {tr("Basics"),    tr("Path Macros"),      tr("Workflow Groups"),
          tr("Info Tags"), tr("Workflows - List"), tr("Workflows - Flowchart")};
}

void ProjectSettingsDialog::selectCategory(int row) {
  m_categoryList->setCurrentRow(row);
}

ProjectSettingsDialog::ProjectSettingsDialog(QWidget* parent)
    : QDialog(parent) {
  m_categoryList = new QListWidget(this);
  m_categoryList->addItems(categoryNames());
  m_categoryList->setFixedWidth(160);
  selectCategory(0);

  m_projectNameEdit     = new QLineEdit(this);
  QPushButton* saveBtn  = new QPushButton(tr("Save"), this);
  QPushButton* closeBtn = new QPushButton(tr("Close"), this);

  m_basicsPage     = new BasicsPage(this);
  m_pathMacrosPage = new PathMacrosPage(this);
  m_wfGroupsPage   = new WfGroupsPage(this);
  m_infoTagsPage   = new InfoTagsPage(this);
  m_workflowPage   = new WorkflowPage(this);
  // カラム①（工程一覧）だけをフローチャートUIに差し替えたカテゴリ。カラム②③はm_workflowPageと
  // 同等のもの（WorkflowPage）を内部で再利用する
  m_flowchartPage = new FlowchartPage(this);

  QStackedWidget* stackedWidget = new QStackedWidget(this);
  stackedWidget->addWidget(m_basicsPage);
  stackedWidget->addWidget(m_pathMacrosPage);
  stackedWidget->addWidget(m_wfGroupsPage);
  stackedWidget->addWidget(m_infoTagsPage);
  stackedWidget->addWidget(m_workflowPage);
  stackedWidget->addWidget(m_flowchartPage);

  QHBoxLayout* mainLay = new QHBoxLayout();
  mainLay->setContentsMargins(0, 0, 0, 0);
  mainLay->setSpacing(0);
  {
    QVBoxLayout* leftLay = new QVBoxLayout();
    leftLay->setContentsMargins(3, 3, 3, 3);
    leftLay->setSpacing(5);
    {
      leftLay->addWidget(m_categoryList, 0);

      leftLay->addStretch(1);

      // プロジェクトID（ppmprjのファイル名）。基本情報の「プロジェクト名」とは別のもの
      QLabel* projectIdLabel = new QLabel(tr("Project ID:"), this);
      QString projectIdToolTip =
          tr("The ID of the project. The project is saved as "
             "\"<Project ID>.ppmprj\" in the projects folder of the Stuff "
             "folder.\n"
             "If you change it and save, the settings are saved as another "
             "project.");
      projectIdLabel->setToolTip(projectIdToolTip);
      m_projectNameEdit->setToolTip(projectIdToolTip);
      leftLay->addWidget(projectIdLabel, 0);
      leftLay->addWidget(m_projectNameEdit, 0);

      QHBoxLayout* buttonsLay = new QHBoxLayout();
      buttonsLay->setContentsMargins(0, 0, 0, 0);
      buttonsLay->setSpacing(5);
      {
        buttonsLay->addWidget(saveBtn, 1);
        buttonsLay->addWidget(closeBtn, 1);
      }
      leftLay->addLayout(buttonsLay, 0);
    }
    mainLay->addLayout(leftLay, 0);

    mainLay->addWidget(stackedWidget, 1);
  }
  setLayout(mainLay);

  connect(m_categoryList, &QListWidget::currentRowChanged, this,
          [this, stackedWidget](int index) {
            stackedWidget->setCurrentIndex(index);
            // "Workflow Groups"／"Workflows"／"Workflows -
            // Flowchart"の3カテゴリは同じtmpSettingsの
            // 内容（flowGroupInfos／workflows）を編集し合うため、切り替えるたびに表示先のページを
            // 最新の内容で作り直す（他のカテゴリはこの3つと編集内容が競合しないため対象外）
            QWidget* page = stackedWidget->widget(index);
            if (page == m_wfGroupsPage)
              m_wfGroupsPage->refresh();
            else if (page == m_workflowPage)
              m_workflowPage->refresh();
            else if (page == m_flowchartPage)
              m_flowchartPage->refresh();
          });

  connect(saveBtn, SIGNAL(clicked()), this, SLOT(onSaveClicked()));
  connect(closeBtn, SIGNAL(clicked()), this, SLOT(reject()));
  connect(m_projectNameEdit, &QLineEdit::editingFinished, this,
          [this]() { markDirty(); });

  // g_onDirtyChangedフックの設定は、フローチャート型ダイアログと共存するため
  // コンストラクタではなくrefresh()（開かれるたび）に行う
}

void ProjectSettingsDialog::updateTitle() {
  QString title = tr("Project: %1 (%2.ppmprj)")
                      .arg(tmpSettings.projectTitle, m_projectNameEdit->text());
  if (tmpSettings.dirtyFlag) title += "*";
  setWindowTitle(title);
}

bool ProjectSettingsDialog::doSave() {
  if (!saveTmpSettingsToProject(this, m_projectNameEdit->text())) return false;

  // 保存先のプロジェクトID名を記録しておく。呼び出し元（MyWindow）はこれを見て
  // カレントプロジェクトの切り替えや画面の再構築を行う
  m_savedProjectName = m_projectNameEdit->text().trimmed();

  updateTitle();

  return true;
}

void ProjectSettingsDialog::onSaveClicked() {
  // Saveはダイアログを閉じない（不具合でアプリが落ちた場合に備え、
  // 閉じずに随時保存できるようにするための変更）
  doSave();
}

void ProjectSettingsDialog::reject() {
  if (tmpSettings.dirtyFlag) {
    QMessageBox::StandardButton ret = QMessageBox::question(
        this, tr("Unsaved Changes"),
        tr("There are unsaved changes to the project settings.\nDo you want to "
           "save them before closing?"),
        QMessageBox::Save | QMessageBox::Discard, QMessageBox::Save);
    if (ret == QMessageBox::Save) {
      // 保存に失敗した場合（プロジェクト名が空等）は閉じずに編集を続けられるようにする
      if (!doSave()) return;
    }
  }
  QDialog::reject();
}

void ProjectSettingsDialog::refresh() {
  m_savedProjectName.clear();
  m_projectNameEdit->setText(WorkflowManager::instance()->currentProjectName());

  // tmpSettingsの変更をこのダイアログのタイトルバーへ即時反映する。
  // また、"Workflows"/"Workflow
  // Groups"側での編集（フローチャート自身の編集操作を除く）を "Workflows -
  // Flowchart"カテゴリのフローチャート表示へも反映する
  g_onDirtyChanged = [this]() {
    updateTitle();
    m_flowchartPage->rebuildIfNotSelfEditing();
  };

  refreshTmpSettingsFromProject();

  m_basicsPage->refresh();
  m_pathMacrosPage->refresh();
  m_wfGroupsPage->refresh();
  m_infoTagsPage->refresh();
  m_workflowPage->refresh();
  m_flowchartPage->refresh();

  // 読み込み直後は未変更の状態から開始する
  tmpSettings.dirtyFlag = false;
  updateTitle();
}