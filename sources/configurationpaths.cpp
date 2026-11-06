#include "configurationpaths.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace ConfigurationPaths {

QString configurationDirectory(const QString& executableDirectory) {
#ifdef Q_OS_MACOS
  Q_UNUSED(executableDirectory);
  return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
  return QDir(executableDirectory).absolutePath();
#endif
}

QString stuffFolderPath(const QString& executableDirectory) {
  const QString configDir = configurationDirectory(executableDirectory);
  QSettings settings(configDir + "/conf.ini", QSettings::IniFormat);
  const QString path = settings.value("StuffFolderPath").toString();
  if (path.isEmpty()) return QString();
  QDir dir(path);
  if (dir.isRelative()) dir.setPath(configDir + "/" + dir.path());
  return dir.absolutePath();
}

QString userSettingsFilePath() {
#ifdef Q_OS_MACOS
  const auto location = QStandardPaths::AppDataLocation;
#else
  const auto location = QStandardPaths::ConfigLocation;
#endif
  return QStandardPaths::writableLocation(location) + "/usersettings.ini";
}

}  // namespace ConfigurationPaths
