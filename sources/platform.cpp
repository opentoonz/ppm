#include "platform.h"

#include <QProcess>
#ifdef Q_OS_MACOS
#include <QDir>
#include <QFileInfo>
#endif

namespace Platform {

bool openFile(const QString& application, const QString& file, OpenMode mode,
              const QStringList& options) {
#ifdef Q_OS_MACOS
  const QFileInfo app(QDir::cleanPath(application));
  if (app.isDir() && app.suffix().compare("app", Qt::CaseInsensitive) == 0) {
    QStringList arguments{"-a", app.absoluteFilePath()};
    // A document-open event can go to a running app. OpenToonz reads its
    // scene and options from argv, which requires a newly launched process.
    if (mode == OpenMode::CommandLine) arguments << "-n" << "--args" << options;
    arguments << QFileInfo(file).absoluteFilePath();
    return QProcess::execute("/usr/bin/open", arguments) == 0;
  }
#else
  Q_UNUSED(mode);
#endif
  QStringList arguments = options;
  arguments << file;
  return QProcess::startDetached(application, arguments);
}

}  // namespace Platform
