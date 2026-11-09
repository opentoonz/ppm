#include "usersettingspopup.h"
#include "usersettings.h"
#include "workflow.h"

#include <QLineEdit>
#include <QComboBox>
#include <QLocale>
#include <QMessageBox>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#ifdef Q_OS_MACOS
#include <QFileInfo>
#endif

UserSettingsPopup::UserSettingsPopup(QWidget* parent, Qt::WindowFlags f)
    : QDialog(parent, f) {
  setWindowTitle(tr("Preferences"));
  m_tvpPath            = new QLineEdit(this);
  m_cspPath            = new QLineEdit(this);
  m_xdtsViewerPath     = new QLineEdit(this);
  m_otPath             = new QLineEdit(this);
  m_otArg              = new QLineEdit(this);
  m_userName           = new QLineEdit(this);
  m_languageCombo      = new QComboBox(this);
  QPushButton* tvpBtn  = new QPushButton("...", this);
  QPushButton* cspBtn  = new QPushButton("...", this);
  QPushButton* xdtsBtn = new QPushButton("...", this);
  QPushButton* otBtn   = new QPushButton("...", this);

  QPushButton* okButton = new QPushButton(tr("OK"), this);

  m_tvpPath->setText(UserSettings::instance()->getTvpBinaryPath());
  m_cspPath->setText(UserSettings::instance()->getCspBinaryPath());
  m_xdtsViewerPath->setText(
      UserSettings::instance()->getXdtsViewerBinaryPath());
  m_otPath->setText(UserSettings::instance()->getOpenToonzBinaryPath());
  m_otArg->setText(UserSettings::instance()->getOpenToonzArgument());
  m_userName->setText(UserSettings::instance()->userNameOverride());
  m_userName->setPlaceholderText(UserSettings::instance()->userName());

  // UIの言語：先頭は翻訳なし（English）。以降は、Stuffフォルダのlocフォルダにある翻訳ファイルの言語
  m_languageCombo->addItem("English", QString());
  for (const QString& code :
       UserSettings::instance()->availableLanguageCodes()) {
    QString name = QLocale(code).nativeLanguageName();
    m_languageCombo->addItem(name.isEmpty() ? code : name, code);
  }
  {
    // 設定されている言語の翻訳ファイルが見つからない場合も、設定内容が分かるよう項目を残す
    const QString currentCode = UserSettings::instance()->languageCode();
    if (!currentCode.isEmpty() && m_languageCombo->findData(currentCode) < 0) {
      QString name = QLocale(currentCode).nativeLanguageName();
      m_languageCombo->addItem(tr("%1 (translation file not found)")
                                   .arg(name.isEmpty() ? currentCode : name),
                               currentCode);
    }
    m_languageCombo->setCurrentIndex(
        qMax(0, m_languageCombo->findData(currentCode)));
  }

  // アプリのパスが未設定の場合は、拡張子に関連付けられた既定のアプリで開く
  // （MyTable::onOpenButtonClicked()／onOpenXdtsButtonClicked()）
  const QString defaultAppPlaceholder =
      tr("Not set: opens with the default application");
  m_tvpPath->setPlaceholderText(defaultAppPlaceholder);
  m_cspPath->setPlaceholderText(defaultAppPlaceholder);
  m_xdtsViewerPath->setPlaceholderText(defaultAppPlaceholder);
  m_otPath->setPlaceholderText(defaultAppPlaceholder);

  tvpBtn->setFixedSize(20, 20);
  cspBtn->setFixedSize(20, 20);
  xdtsBtn->setFixedSize(20, 20);
  otBtn->setFixedSize(20, 20);
  okButton->setFixedWidth(100);

  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setContentsMargins(15, 15, 15, 15);
  mainLay->setSpacing(10);
  {
    QGridLayout* uiLay = new QGridLayout();
    uiLay->setContentsMargins(0, 0, 0, 0);
    uiLay->setHorizontalSpacing(5);
    uiLay->setVerticalSpacing(10);
    {
      QLabel* languageLabel = new QLabel(tr("Language:"), this);
      QString languageTooltip =
          tr("The language of the user interface. The translation files are "
             "placed in the \"loc\" folder in the Stuff folder.\n"
             "The change is applied the next time the application is started.");
      languageLabel->setToolTip(languageTooltip);
      m_languageCombo->setToolTip(languageTooltip);
      uiLay->addWidget(languageLabel, 0, 0, Qt::AlignRight | Qt::AlignVCenter);
      uiLay->addWidget(m_languageCombo, 0, 1);

      uiLay->addWidget(new QLabel(tr("TVPaint executable path:"), this), 1, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
      uiLay->addWidget(m_tvpPath, 1, 1);
      uiLay->addWidget(tvpBtn, 1, 2);

      uiLay->addWidget(new QLabel(tr("Clip Studio executable path:"), this), 2,
                       0, Qt::AlignRight | Qt::AlignVCenter);
      uiLay->addWidget(m_cspPath, 2, 1);
      uiLay->addWidget(cspBtn, 2, 2);

      QLabel* xdtsLavel = new QLabel(tr("XDTS application path:"), this);
      QString xdtsTooltip =
          tr("Specify the path to an application for viewing XDTS files, such "
             "as XDTS Viewer or Toei Animation Digital Exposure Sheet.");
      xdtsLavel->setToolTip(xdtsTooltip);
      m_xdtsViewerPath->setToolTip(xdtsTooltip);
      uiLay->addWidget(xdtsLavel, 3, 0, Qt::AlignRight | Qt::AlignVCenter);
      uiLay->addWidget(m_xdtsViewerPath, 3, 1);
      uiLay->addWidget(xdtsBtn, 3, 2);

      uiLay->addWidget(new QLabel(tr("OpenToonz executable path:"), this), 4, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
      uiLay->addWidget(m_otPath, 4, 1);
      uiLay->addWidget(otBtn, 4, 2);
      uiLay->addWidget(new QLabel(tr("OpenToonz arguments:"), this), 5, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
      uiLay->addWidget(m_otArg, 5, 1);

      QLabel* defaultAppNote = new QLabel(
          tr("If an application path is left empty, the file is opened with "
             "the default application associated with its file extension.\n"
             "(In that case, the OpenToonz arguments are not used.)"),
          this);
      defaultAppNote->setWordWrap(true);
      defaultAppNote->setStyleSheet("QLabel { color: gray; }");
      uiLay->addWidget(defaultAppNote, 6, 1, 1, 2);

      QLabel* userNameLabel =
          new QLabel(tr("User Name [USER_NAME] (optional):"), this);
      QString userNameTooltip =
          tr("Overrides the value substituted for the [USER_NAME] macro in "
             "file paths.\n"
             "Leave this empty to use the OS user name (shown as placeholder "
             "text).");
      userNameLabel->setToolTip(userNameTooltip);
      m_userName->setToolTip(userNameTooltip);
      uiLay->addWidget(userNameLabel, 7, 0, Qt::AlignRight | Qt::AlignVCenter);
      uiLay->addWidget(m_userName, 7, 1);
    }
    mainLay->addLayout(uiLay);

    QHBoxLayout* btnLay = new QHBoxLayout();
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(0);
    {
      btnLay->addStretch(1);
      btnLay->addWidget(okButton, 0);
      btnLay->addStretch(1);
    }
    mainLay->addLayout(btnLay);
  }
  setLayout(mainLay);

  connect(tvpBtn, SIGNAL(clicked()), this, SLOT(onTvpBtnClicked()));
  connect(cspBtn, SIGNAL(clicked()), this, SLOT(onCspBtnClicked()));
  connect(xdtsBtn, SIGNAL(clicked()), this, SLOT(onXdtsBtnClicked()));
  connect(otBtn, SIGNAL(clicked()), this, SLOT(onOtBtnClicked()));

  connect(m_tvpPath, SIGNAL(editingFinished()), this, SLOT(onTvpPathEdited()));
  connect(m_cspPath, SIGNAL(editingFinished()), this, SLOT(onCspPathEdited()));
  connect(m_xdtsViewerPath, SIGNAL(editingFinished()), this,
          SLOT(onXdtsPathEdited()));
  connect(m_otPath, SIGNAL(editingFinished()), this, SLOT(onOtPathEdited()));
  connect(m_otArg, SIGNAL(editingFinished()), this, SLOT(onOtArgEdited()));
  connect(m_userName, SIGNAL(editingFinished()), this,
          SLOT(onUserNameEdited()));
  connect(m_languageCombo, SIGNAL(activated(int)), this,
          SLOT(onLanguageActivated()));
  connect(okButton, SIGNAL(clicked()), this, SLOT(accept()));
}

QString UserSettingsPopup::chooseApplication(const QString& title,
                                             const QString& currentPath) {
#ifdef Q_OS_MACOS
  const QFileInfo current(currentPath);
  QString initial = "/Applications";
  if (current.exists())
    initial = current.isBundle() ? current.absolutePath()
                                 : current.absoluteFilePath();
  return QFileDialog::getOpenFileName(
      this, title, initial, tr("Applications (*.app);;All files (*)"));
#elif defined(Q_OS_WIN)
  return QFileDialog::getOpenFileName(this, title, currentPath,
                                      tr("Executable (*.exe)"));
#else
  return QFileDialog::getOpenFileName(this, title, currentPath);
#endif
}

void UserSettingsPopup::onTvpBtnClicked() {
  QString exePath =
      chooseApplication(tr("Select TVPaint executable"),
                        UserSettings::instance()->getTvpBinaryPath());
  if (exePath.isEmpty()) return;
  m_tvpPath->setText(exePath);
  UserSettings::instance()->setTvpBinaryPath(exePath);
}

void UserSettingsPopup::onTvpPathEdited() {
  UserSettings::instance()->setTvpBinaryPath(m_tvpPath->text());
}

void UserSettingsPopup::onCspBtnClicked() {
  QString exePath =
      chooseApplication(tr("Select Clip Studio executable"),
                        UserSettings::instance()->getCspBinaryPath());
  if (exePath.isEmpty()) return;
  m_cspPath->setText(exePath);
  UserSettings::instance()->setCspBinaryPath(exePath);
}

void UserSettingsPopup::onCspPathEdited() {
  UserSettings::instance()->setCspBinaryPath(m_cspPath->text());
}

void UserSettingsPopup::onXdtsBtnClicked() {
  QString exePath =
      chooseApplication(tr("Select XDTS Viewer executable"),
                        UserSettings::instance()->getXdtsViewerBinaryPath());
  if (exePath.isEmpty()) return;
  m_xdtsViewerPath->setText(exePath);
  UserSettings::instance()->setXdtsViewerBinaryPath(exePath);
}

void UserSettingsPopup::onXdtsPathEdited() {
  UserSettings::instance()->setXdtsViewerBinaryPath(m_xdtsViewerPath->text());
}

void UserSettingsPopup::onOtBtnClicked() {
  QString exePath =
      chooseApplication(tr("Select OpenToonz executable"),
                        UserSettings::instance()->getOpenToonzBinaryPath());
  if (exePath.isEmpty()) return;
  m_otPath->setText(exePath);
  UserSettings::instance()->setOpenToonzBinaryPath(exePath);
}

void UserSettingsPopup::onOtPathEdited() {
  UserSettings::instance()->setOpenToonzBinaryPath(m_otPath->text());
}
void UserSettingsPopup::onOtArgEdited() {
  UserSettings::instance()->setOpenToonzArgument(m_otArg->text());
}

void UserSettingsPopup::onLanguageActivated() {
  const QString code = m_languageCombo->currentData().toString();
  if (code == UserSettings::instance()->languageCode()) return;
  UserSettings::instance()->setLanguageCode(code);
  // UIの文字列は、その場では切り替えない
  QMessageBox::information(this, tr("Language"),
                           tr("The change will be applied the next time the "
                              "application is started."));
}

void UserSettingsPopup::onUserNameEdited() {
  UserSettings::instance()->setUserNameOverride(m_userName->text());
  // 空にした場合、プレースホルダーに表示中の既定値（OSのユーザ名）は変わらないため再設定は不要
}