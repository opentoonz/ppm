#include "usersettings.h"
#include "configurationpaths.h"

#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QLocale>

#include <iostream>

namespace {

QString userSettingsPath() {
  return ConfigurationPaths::userSettingsFilePath();
}

}  // namespace

UserSettings::UserSettings()
    : m_tvpBinaryPath("")
    , m_cspBinaryPath("")
    , m_xdtsViewerBinaryPath("")
    , m_openToonzBinaryPath("")
    , m_openToonzArgument("")
    , m_languageCode() {
  // 既定のユーザ設定・ユーザ設定のどちらかで言語が指定されたか
  // （空文字列＝英語の指定もありうるため、値ではなくキーの有無で判定する）
  bool languageSpecified = false;
  m_stuffFolderPath      = ConfigurationPaths::stuffFolderPath(
      QCoreApplication::applicationDirPath());
  if (!m_stuffFolderPath.isEmpty()) {
    std::cout << "m_stuffFolderPath = " << m_stuffFolderPath.toStdString()
              << std::endl;

    // 既定のユーザ設定を読み込む
    QSettings defaultSettings(m_stuffFolderPath + "/defaultusersettings.ini",
                              QSettings::IniFormat);
    defaultSettings.beginGroup("UserSettings");
    m_tvpBinaryPath =
        defaultSettings.value("TvpBinaryPath", m_tvpBinaryPath).toString();
    m_cspBinaryPath =
        defaultSettings.value("CspBinaryPath", m_cspBinaryPath).toString();
    m_xdtsViewerBinaryPath =
        defaultSettings.value("XdtsViewerBinaryPath", m_xdtsViewerBinaryPath)
            .toString();
    m_openToonzBinaryPath =
        defaultSettings.value("OpenToonzBinaryPath", m_openToonzBinaryPath)
            .toString();
    m_openToonzArgument =
        defaultSettings.value("OpenToonzArgument", m_openToonzArgument)
            .toString();
    if (defaultSettings.contains("LanguageCode")) {
      m_languageCode    = defaultSettings.value("LanguageCode").toString();
      languageSpecified = true;
    }
    defaultSettings.endGroup();
  }

  if (QFileInfo(userSettingsPath()).exists()) {
    QSettings settings(userSettingsPath(), QSettings::IniFormat);
    settings.beginGroup("UserSettings");
    m_tvpBinaryPath =
        settings.value("TvpBinaryPath", m_tvpBinaryPath).toString();
    m_cspBinaryPath =
        settings.value("CspBinaryPath", m_cspBinaryPath).toString();
    m_xdtsViewerBinaryPath =
        settings.value("XdtsViewerBinaryPath", m_xdtsViewerBinaryPath)
            .toString();
    m_openToonzBinaryPath =
        settings.value("OpenToonzBinaryPath", m_openToonzBinaryPath).toString();
    m_openToonzArgument =
        settings.value("OpenToonzArgument", m_openToonzArgument).toString();
    m_userNameOverride =
        settings.value("UserNameOverride", m_userNameOverride).toString();
    if (settings.contains("LanguageCode")) {
      m_languageCode    = settings.value("LanguageCode").toString();
      languageSpecified = true;
    }
    settings.endGroup();
  }

  // どちらにも言語の指定が無い場合は、OSの表示言語のうち、翻訳ファイルがあるものを使う
  // （見つからなければ英語）
  if (!languageSpecified) m_languageCode = systemLanguageCode();

  // 設定された言語の翻訳ファイルが無ければ、警告する（翻訳なし＝英語のUIで起動する）
  if (!m_languageCode.isEmpty() &&
      !QFileInfo(translationFilePath(m_languageCode)).exists())
    QMessageBox::warning(
        nullptr, tr("Warning: Translation file not found"),
        tr("The translation file for the language \"%1\" specified in the user "
           "settings does not exist.\n"
           "The application will start in English.\n(Check %2 .)")
            .arg(m_languageCode, translationFilePath(m_languageCode)));
}

void UserSettings::warnAboutMissingBinaries() {
  // check existence
  if (!m_tvpBinaryPath.isEmpty() && !QFileInfo(m_tvpBinaryPath).exists())
    QMessageBox::warning(
        nullptr, tr("Warning: TVPaint binary not found"),
        tr("TVPaint binary specified in the user settings does not exist.\n"
           "(Check %1 .)")
            .arg(m_tvpBinaryPath));
  if (!m_cspBinaryPath.isEmpty() && !QFileInfo(m_cspBinaryPath).exists())
    QMessageBox::warning(
        nullptr, tr("Warning: Clip Studio binary not found"),
        tr("Clip Studio binary specified in the user settings does not exist.\n"
           "(Check %1 .)")
            .arg(m_cspBinaryPath));
  if (!m_xdtsViewerBinaryPath.isEmpty() &&
      !QFileInfo(m_xdtsViewerBinaryPath).exists())
    QMessageBox::warning(
        nullptr, tr("Warning: XDTS Viewer binary not found"),
        tr("XDTS Viewer binary specified in the user settings does not exist.\n"
           "(Check %1 .)")
            .arg(m_xdtsViewerBinaryPath));
  if (!m_openToonzBinaryPath.isEmpty() &&
      !QFileInfo(m_openToonzBinaryPath).exists())
    QMessageBox::warning(
        nullptr, tr("Warning: OpenToonz binary not found"),
        tr("OpenToonz binary specified in the user settings does not exist.\n"
           "(Check %1 .)")
            .arg(m_openToonzBinaryPath));
}

UserSettings* UserSettings::instance() {
  static UserSettings _instance;
  return &_instance;
}

QString UserSettings::translationFolderPath() const {
  return m_stuffFolderPath + "/loc";
}

QString UserSettings::translationFilePath(const QString& languageCode) const {
  return translationFolderPath() + "/PPM_" + languageCode + ".qm";
}

QString UserSettings::systemLanguageCode() const {
  // uiLanguages()は優先順に"ja-JP"、"ja"のような形で返る。
  // 翻訳ファイル名の形（"ja_JP"）にして、そのままの形と言語だけの形（"ja"）の順に探す
  for (QString lang : QLocale::system().uiLanguages()) {
    lang.replace('-', '_');
    if (QFileInfo(translationFilePath(lang)).exists()) return lang;
    QString langOnly = lang.section('_', 0, 0);
    if (QFileInfo(translationFilePath(langOnly)).exists()) return langOnly;
  }
  return QString();
}

QStringList UserSettings::availableLanguageCodes() const {
  QStringList codes;
  for (const QString& fileName :
       QDir(translationFolderPath())
           .entryList({"PPM_*.qm"}, QDir::Files, QDir::Name)) {
    // "PPM_ja.qm" -> "ja"
    const QString code = QFileInfo(fileName).completeBaseName().mid(4);
    if (!code.isEmpty()) codes.append(code);
  }
  return codes;
}

void UserSettings::setLanguageCode(const QString& code) {
  if (m_languageCode == code) return;
  m_languageCode = code;

  QSettings settings(userSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("UserSettings");
  settings.setValue("LanguageCode", m_languageCode);
  settings.endGroup();
}

void UserSettings::setTvpBinaryPath(const QString& path) {
  if (m_tvpBinaryPath == path) return;
  m_tvpBinaryPath = path;

  QSettings settings(userSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("UserSettings");
  settings.setValue("TvpBinaryPath", m_tvpBinaryPath);
  settings.endGroup();
}

void UserSettings::setCspBinaryPath(const QString& path) {
  if (m_cspBinaryPath == path) return;
  m_cspBinaryPath = path;

  QSettings settings(userSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("UserSettings");
  settings.setValue("CspBinaryPath", m_cspBinaryPath);
  settings.endGroup();
}

void UserSettings::setXdtsViewerBinaryPath(const QString& path) {
  if (m_xdtsViewerBinaryPath == path) return;
  m_xdtsViewerBinaryPath = path;

  QSettings settings(userSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("UserSettings");
  settings.setValue("XdtsViewerBinaryPath", m_xdtsViewerBinaryPath);
  settings.endGroup();
}

void UserSettings::setOpenToonzBinaryPath(const QString& path) {
  if (m_openToonzBinaryPath == path) return;
  m_openToonzBinaryPath = path;

  QSettings settings(userSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("UserSettings");
  settings.setValue("OpenToonzBinaryPath", m_openToonzBinaryPath);
  settings.endGroup();
}
void UserSettings::setOpenToonzArgument(const QString& arg) {
  if (m_openToonzArgument == arg) return;
  m_openToonzArgument = arg;

  QSettings settings(userSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("UserSettings");
  settings.setValue("OpenToonzArgument", m_openToonzArgument);
  settings.endGroup();
}

QString UserSettings::userName() const {
  if (!m_userNameOverride.isEmpty()) return m_userNameOverride;

  QString sysUserName = QString::fromLocal8Bit(qgetenv("USER"));  // Unix系
  if (sysUserName.isEmpty())
    sysUserName = QString::fromLocal8Bit(qgetenv("USERNAME"));  // Windows
  return sysUserName;
}

void UserSettings::setUserNameOverride(const QString& name) {
  if (m_userNameOverride == name) return;
  m_userNameOverride = name;

  QSettings settings(userSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("UserSettings");
  settings.setValue("UserNameOverride", m_userNameOverride);
  settings.endGroup();
}

QString UserSettings::getLicenseFolderPath() const {
  return stuffFolderPath() + "/LICENSE";
}

void UserSettings::saveListIndices(const QString& prjName,
                                   const QList<int>& checkedFlowTypes,
                                   const QList<int>& expandedFlowGroupIds,
                                   const QList<int>& unlockedFlowTypes) {
  QSettings settings(userSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("UserSettings");

  settings.beginGroup(prjName);
  QStringList strList;
  for (auto id : checkedFlowTypes) strList.append(QString::number(id));
  settings.setValue("CheckedFlowTypes", strList.join(","));

  strList.clear();
  for (auto id : expandedFlowGroupIds) strList.append(QString::number(id));
  settings.setValue("ExpandedFlowGroupIds", strList.join(","));

  strList.clear();
  for (auto id : unlockedFlowTypes) strList.append(QString::number(id));
  settings.setValue("UnlockedFlowTypes", strList.join(","));

  settings.endGroup();
  settings.endGroup();
  /*
  QStringList strList;
  for (auto id : ids)
    strList.append(QString::number(id));
  settings.setValue("VisibleIndices", strList.join(","));

  strList.clear();
  for (auto id : expandedFlowIds)
    strList.append(QString::number(id));
  settings.setValue("ExpandedFlowIds", strList.join(","));

  strList.clear();
  for (auto id : lockedIds)
    strList.append(QString::number(id));
  settings.setValue("LockedIds", strList.join(","));

  settings.endGroup();
  */
}

void UserSettings::loadListIndices(const QString& prjName,
                                   QList<int>& checkedFlowTypes,
                                   QList<int>& expandedFlowGroupIds,
                                   QList<int>& unlockedFlowTypes) {
  if (QFileInfo(userSettingsPath()).exists()) {
    QSettings settings(userSettingsPath(), QSettings::IniFormat);
    settings.beginGroup("UserSettings");

    settings.beginGroup(prjName);

    QStringList strList = settings.value("CheckedFlowTypes")
                              .toString()
                              .split(",", Qt::SkipEmptyParts);
    for (auto str : strList) checkedFlowTypes.append(str.toInt());

    strList.clear();
    strList = settings.value("ExpandedFlowGroupIds", "")
                  .toString()
                  .split(",", Qt::SkipEmptyParts);
    for (auto str : strList) expandedFlowGroupIds.append(str.toInt());

    strList.clear();
    strList = settings.value("UnlockedFlowTypes")
                  .toString()
                  .split(",", Qt::SkipEmptyParts);
    for (auto str : strList) unlockedFlowTypes.append(str.toInt());

    settings.endGroup();
    /*
    QStringList strList = settings.value("VisibleIndices").toString().split(",",
    Qt::SkipEmptyParts); for (auto str : strList)
      checkedIds.append(str.toInt());

    strList.clear();
    strList = settings.value("ExpandedFlowIds", "0,1,2,3").toString().split(",",
    Qt::SkipEmptyParts); for (auto str : strList)
      expandedFlowIds.append(str.toInt());

    strList.clear();
    if (settings.contains("LockedIds")) {
      lockedIds.clear();
      strList = settings.value("LockedIds").toString().split(",",
    Qt::SkipEmptyParts); for (auto str : strList) lockedIds.append(str.toInt());
    }
    */
    settings.endGroup();
  }
}

void UserSettings::saveCurrentEpisode(const QString& prjName,
                                      const QString& episode) {
  QSettings settings(userSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("UserSettings");
  settings.beginGroup(prjName);
  settings.setValue("CurrentEpisode", episode);
  settings.endGroup();
  settings.endGroup();
}

QString UserSettings::loadCurrentEpisode(const QString& prjName) const {
  QSettings settings(userSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("UserSettings");
  settings.beginGroup(prjName);
  QString ret = settings.value("CurrentEpisode", "").toString();
  settings.endGroup();
  settings.endGroup();
  return ret;
}

void UserSettings::saveCurrentProjectName(const QString& prjName) {
  QSettings settings(userSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("UserSettings");
  settings.setValue("CurrentProjectName", prjName);
  settings.endGroup();
}

QString UserSettings::loadCurrentProjectName(
    const QString& defaultPrjName) const {
  QSettings settings(userSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("UserSettings");
  QString ret = settings.value("CurrentProjectName", defaultPrjName).toString();
  settings.endGroup();
  return ret;
}

void UserSettings::saveWindowGeometry(const QByteArray& geom) {
  QSettings settings(userSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("UserSettings");
  settings.setValue("WindowGeometry", geom);
  settings.endGroup();
}

QByteArray UserSettings::loadWindowGeometry() const {
  QByteArray ret;
  QSettings settings(userSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("UserSettings");
  ret = settings.value("WindowGeometry").toByteArray();
  settings.endGroup();
  return ret;
}

void UserSettings::saveLeftPanelFlowchartMode(bool isFlowchart) {
  QSettings settings(userSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("UserSettings");
  settings.setValue("LeftPanelFlowchartMode", isFlowchart);
  settings.endGroup();
}

bool UserSettings::loadLeftPanelFlowchartMode() const {
  QSettings settings(userSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("UserSettings");
  bool ret = settings.value("LeftPanelFlowchartMode", false).toBool();
  settings.endGroup();
  return ret;
}

void UserSettings::saveMainSplitterState(const QByteArray& state) {
  QSettings settings(userSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("UserSettings");
  settings.setValue("MainSplitterState", state);
  settings.endGroup();
}

QByteArray UserSettings::loadMainSplitterState() const {
  QByteArray ret;
  QSettings settings(userSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("UserSettings");
  ret = settings.value("MainSplitterState").toByteArray();
  settings.endGroup();
  return ret;
}

void UserSettings::saveColumnVisible(const QString& columnKey, bool visible) {
  QSettings settings(userSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("TableColumnVisibility");
  settings.setValue(columnKey, visible);
  settings.endGroup();
}

bool UserSettings::loadColumnVisible(const QString& columnKey,
                                     bool defaultVisible) const {
  QSettings settings(userSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("TableColumnVisibility");
  bool ret = settings.value(columnKey, defaultVisible).toBool();
  settings.endGroup();
  return ret;
}

void UserSettings::saveThumbnailSizeLarge(bool isLarge) {
  QSettings settings(userSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("UserSettings");
  settings.setValue("ThumbnailSizeLarge", isLarge);
  settings.endGroup();
}

bool UserSettings::loadThumbnailSizeLarge() const {
  QSettings settings(userSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("UserSettings");
  bool ret = settings.value("ThumbnailSizeLarge", false).toBool();
  settings.endGroup();
  return ret;
}
