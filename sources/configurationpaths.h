#pragma once

#include <QString>

namespace ConfigurationPaths {

// conf.ini lives in per-user Application Support on macOS, or next to the
// executable.
QString configurationDirectory(const QString& executableDirectory);
QString stuffFolderPath(const QString& executableDirectory);
QString userSettingsFilePath();

}  // namespace ConfigurationPaths
