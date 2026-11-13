#pragma once

#include <QFileInfo>

namespace SharedFiles {
// Return the on-disk spelling. A null result means that more than one
// directory entry has the requested canonical Unicode spelling.
QString actualPath(const QString& path);
bool sameEntry(const QString& a, const QString& b);
bool insideSource(const QString& source, const QString& destination);
bool isMetadata(const QFileInfo& entry);
enum class Cleanup { Removed, Kept, Failed };
Cleanup removeEmptyFolder(const QString& path);
}  // namespace SharedFiles
