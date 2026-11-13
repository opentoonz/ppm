#include "pathmapping.h"

namespace {
QString separators(QString path) { return path.replace('\\', '/'); }

QString prefix(QString path) {
  path = separators(path);
  while (path.size() > 1 && path.endsWith('/')) path.chop(1);
  return path;
}

// Compare components, so Unicode normalization never changes the offset used
// to append the original suffix. Preserve the two leading separators of UNC.
bool substitute(const QString& path, const QString& from, const QString& to,
                QString& result) {
  QString input      = separators(path);
  QString key        = prefix(from);
  QStringList parts  = input.split('/');
  QStringList wanted = key.split('/');
  if (key == "/") {
    if (!input.startsWith('/') || input.startsWith("//")) return false;
    wanted = QStringList{QString()};
  }
  if (parts.size() < wanted.size()) return false;
  for (int i = 0; i < wanted.size(); ++i)
    if (parts[i]
            .normalized(QString::NormalizationForm_C)
            .compare(wanted[i].normalized(QString::NormalizationForm_C),
                     Qt::CaseInsensitive) != 0)
      return false;
  result               = separators(to);
  const QString suffix = parts.mid(wanted.size()).join('/');
  if (!suffix.isEmpty()) {
    if (!result.endsWith('/')) result += '/';
    result += suffix;
  }
  return true;
}
}  // namespace

bool PathMappings::isAbsolute(const QString& path) {
  const QString p = separators(path);
  return p.startsWith('/') ||
         (p.size() >= 3 && p[0].isLetter() && p[1] == ':' && p[2] == '/');
}

QString PathMappings::toLocal(const QString& stored, bool* matched) const {
  QString result    = stored;
  qsizetype longest = -1;
  if (isAbsolute(stored)) {
    for (const auto& row : rows) {
      const auto length =
          prefix(row.stored).normalized(QString::NormalizationForm_C).size();
      QString candidate;
      if (length > longest &&
          substitute(stored, row.stored, row.local, candidate)) {
        longest = length;
        result  = candidate;
      }
    }
  }
  if (matched) *matched = longest >= 0;
  return result;
}

QString PathMappings::filesystemPath(const QString& stored) const {
  QString result = toLocal(stored);
  if (!isAbsolute(result)) return result;
  QString path = separators(result);
#ifdef Q_OS_WIN
  if (path.startsWith('/') && !path.startsWith("//")) return {};
#else
  if (!path.startsWith('/') || path.startsWith("//")) return {};
#endif
  return path;
}

QStringList PathMappings::storedCandidates(const QString& local) const {
  QStringList candidates;
  if (isAbsolute(local)) {
    for (const auto& row : rows) {
      QString candidate;
      if (substitute(local, row.local, row.stored, candidate) &&
          prefix(toLocal(candidate))
                  .normalized(QString::NormalizationForm_C)
                  .compare(
                      prefix(local).normalized(QString::NormalizationForm_C),
                      Qt::CaseInsensitive) == 0 &&
          !candidates.contains(candidate))
        candidates.append(candidate);
    }
  }
  return candidates;
}

bool PathMappings::accepts(const PathMapping& row) const {
  if (!isAbsolute(row.stored) || !isAbsolute(row.local)) return false;
  const QString key =
      prefix(row.stored).normalized(QString::NormalizationForm_C);
  for (const auto& previous : rows)
    if (key.compare(
            prefix(previous.stored).normalized(QString::NormalizationForm_C),
            Qt::CaseInsensitive) == 0)
      return false;
  return true;
}

PathMappings PathMappings::read(QSettings& ini) {
  PathMappings mappings;
  const int count = ini.beginReadArray("PathMappings");
  for (int i = 0; i < count; ++i) {
    ini.setArrayIndex(i);
    PathMapping row{ini.value("stored").toString(),
                    ini.value("local").toString()};
    if (mappings.accepts(row)) mappings.rows.append(row);
  }
  ini.endArray();
  return mappings;
}

void PathMappings::write(QSettings& ini) const {
  ini.remove("PathMappings");
  ini.beginWriteArray("PathMappings", rows.size());
  for (int i = 0; i < rows.size(); ++i) {
    ini.setArrayIndex(i);
    ini.setValue("stored", rows[i].stored);
    ini.setValue("local", rows[i].local);
  }
  ini.endArray();
}
