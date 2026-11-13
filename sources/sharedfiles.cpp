#include "sharedfiles.h"

#include <QDir>
#include <filesystem>
#include <algorithm>

namespace SharedFiles {
namespace {
constexpr auto allEntries =
    QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System;
}

QString actualPath(const QString& path) {
  // Only components with a canonical decomposition can have another
  // composed/decomposed spelling. Avoid directory reads for ASCII paths.
  if (path.isEmpty() || std::all_of(path.cbegin(), path.cend(),
                                    [](QChar c) { return c.unicode() < 128; }))
    return path;
  QStringList parts = QDir::fromNativeSeparators(path).split('/');
  for (int i = 0; i < parts.size(); ++i) {
    const QString wanted = parts[i].normalized(QString::NormalizationForm_C);
    if (wanted == parts[i].normalized(QString::NormalizationForm_D) &&
        wanted == parts[i])
      continue;
    QString parent = parts.mid(0, i).join('/');
#ifdef Q_OS_WIN
    if (parent.size() == 2 && parent.endsWith(':')) parent += '/';
#endif
    QDir dir(i && parent.isEmpty() ? "/" : parent);
    const auto entries = dir.entryList(allEntries, QDir::Unsorted);
    QString found;
    for (const QString& entry : entries) {
      if (entry.normalized(QString::NormalizationForm_C) != wanted) continue;
      if (!found.isEmpty()) return {};
      found = entry;
    }
    if (!found.isEmpty()) parts[i] = found;
  }
  return parts.join('/');
}

bool sameEntry(const QString& a, const QString& b) {
  if (a.isEmpty() || b.isEmpty()) return false;
  std::error_code error;
#ifdef Q_OS_MACOS
  // Qt passes decomposed UTF-8 to the native filesystem on macOS, including
  // SMB.
  return std::filesystem::equivalent(QFile::encodeName(a).constData(),
                                     QFile::encodeName(b).constData(), error);
#else
  return std::filesystem::equivalent(QFileInfo(a).filesystemFilePath(),
                                     QFileInfo(b).filesystemFilePath(), error);
#endif
}

bool insideSource(const QString& source, const QString& destination) {
  QString ancestor = QFileInfo(destination).absoluteFilePath();
  while (!ancestor.isEmpty()) {
    const QString canonical = QFileInfo(ancestor).canonicalFilePath();
    if (!canonical.isEmpty()) {
      ancestor = canonical;
      if (sameEntry(source, ancestor)) return true;
    }
    const QString parent = QFileInfo(ancestor).absolutePath();
    if (parent == ancestor) break;
    ancestor = parent;
  }
  return false;
}

bool isMetadata(const QFileInfo& entry) {
  return !entry.isSymLink() && entry.isFile() &&
         (entry.fileName() == ".DS_Store" || entry.fileName().startsWith("._"));
}

Cleanup removeEmptyFolder(const QString& path) {
  if (path.isEmpty() || QFileInfo(path).isSymLink()) return Cleanup::Kept;
  QDir dir(path);
  const auto entries = dir.entryInfoList(allEntries, QDir::Unsorted);
  for (const auto& entry : entries)
    if (!isMetadata(entry)) return Cleanup::Kept;
  for (const auto& entry : entries)
    if (!QFile::remove(entry.filePath())) return Cleanup::Failed;
  return QDir().rmdir(path) ? Cleanup::Removed : Cleanup::Failed;
}
}  // namespace SharedFiles
