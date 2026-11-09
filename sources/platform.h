#pragma once

#include <QStringList>

namespace Platform {

enum class OpenMode { Document, CommandLine };

bool openFile(const QString& application, const QString& file,
              OpenMode mode              = OpenMode::Document,
              const QStringList& options = {});

}  // namespace Platform
