#include "platform.h"

#include <QProcess>
#ifdef Q_OS_MACOS
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <cerrno>
#include <unistd.h>
#endif

namespace Platform {

bool openFile(const QString& application, const QString& file, OpenMode mode,
              const QStringList& options) {
#ifdef Q_OS_MACOS
  const QFileInfo app(QDir::cleanPath(application));
  if (app.isDir() && app.suffix().compare("app", Qt::CaseInsensitive) == 0) {
    QStringList arguments{"-a", app.absoluteFilePath()};
    // A document-open event can go to a running app. OpenToonz and XDTS
    // viewers receive the file via argv in a newly launched process.
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

bool zipFolder(const QString& source, const QString& destination) {
  QProcess process;
#ifdef Q_OS_MACOS
  const QFileInfo folder(source);
  const QFileInfo zip(destination);
  const QString parent = zip.dir().canonicalPath();
  if (source.isEmpty() || !folder.isDir() || destination.isEmpty() ||
      parent.isEmpty() || zip.isSymLink() || (zip.exists() && !zip.isFile()))
    return false;

  // The temporary output must be outside the input tree, including when the
  // destination is reached through a symlink or a case-insensitive volume.
  QDir ancestor(parent);
  do {
    if (QFileInfo(ancestor.path()) == folder) return false;
  } while (ancestor.cdUp());

  QSaveFile archive(destination);
  if (!archive.open(QIODevice::WriteOnly)) return false;
  // Write straight into QSaveFile so even a large ZIP needs only one copy.
  const int output = archive.handle();
  process.setChildProcessModifier([&process, output] {
    if (::dup2(output, STDOUT_FILENO) < 0)
      process.failChildProcessModifier("dup2", errno);
  });
  // A named output avoids tar's block padding on stdout. Strip the leading
  // "." directory from entry names only, and mark UTF-8 names explicitly.
  process.start("/usr/bin/tar",
                {"-cf", "/dev/fd/1", "--format", "zip", "--options",
                 "zip:hdrcharset=UTF-8", "--no-mac-metadata", "-s",
                 ",^\\./*,,S", "-C", folder.absoluteFilePath(), "."});
  return process.waitForFinished(-1) &&
         process.exitStatus() == QProcess::NormalExit &&
         process.exitCode() == 0 && archive.commit();
#elif defined(Q_OS_WIN)
  process.start("powershell.exe", {"-Command", "compress-archive", "-Force",
                                   source + "/*", destination});
  return process.waitForFinished(-1);
#else
  Q_UNUSED(source);
  Q_UNUSED(destination);
  return false;
#endif
}

}  // namespace Platform
