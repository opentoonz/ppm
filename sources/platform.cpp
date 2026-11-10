#include "platform.h"

#include <QProcess>
#ifdef Q_OS_MACOS
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryFile>
#include <cerrno>
#include <filesystem>
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

#ifdef Q_OS_MACOS
// Freeze the member list before QSaveFile creates its output in the source
// tree. NUL separators preserve filenames containing newlines or spaces.
static bool writeZipMembers(const QFileInfo& folder, const QFileInfo& zip,
                            QTemporaryFile& members) {
  namespace fs           = std::filesystem;
  const fs::path root    = folder.filesystemAbsoluteFilePath();
  const bool overwriting = zip.exists();
  std::error_code error;
  fs::recursive_directory_iterator it(root, error), end;
  for (; !error && it != end; it.increment(error)) {
    const fs::path& path = it->path();
    if (path.filename().native().front() == '.') {
      it.disable_recursion_pending();
      continue;
    }
    if (overwriting) {
      const QFileInfo entry(path);
      if (!entry.isSymLink() && entry == zip) continue;
    }
    const std::string name = "./" + path.lexically_relative(root).native();
    const qint64 length    = static_cast<qint64>(name.size()) + 1;
    if (members.write(name.c_str(), length) != length) return false;
  }
  return !error && members.flush();
}
#endif

bool zipFolder(const QString& source, const QString& destination) {
  QProcess process;
#ifdef Q_OS_MACOS
  const QFileInfo folder(source);
  const QFileInfo zip(destination);
  const QString parent = zip.dir().canonicalPath();
  if (source.isEmpty() || !folder.isDir() || destination.isEmpty() ||
      parent.isEmpty() || zip.isSymLink() || (zip.exists() && !zip.isFile()))
    return false;

  QTemporaryFile members(QDir::temp().filePath(".ppm-zip-members-XXXXXX"));
  if (!members.open() || !writeZipMembers(folder, zip, members)) return false;

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
                 ",^\\./*,,S", "-C", folder.absoluteFilePath(),
                 "--no-recursion", "--null", "-T", members.fileName()});
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
