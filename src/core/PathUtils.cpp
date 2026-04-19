#include "PathUtils.h"
#include <QDir>
// TODO Phase 1: Vollständige Implementierung

namespace flatlas::core {

QString PathUtils::ciFindFile(const QString &directory, const QString &filename)
{
    QDir dir(directory);
    const auto entries = dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &entry : entries) {
        if (entry.compare(filename, Qt::CaseInsensitive) == 0)
            return dir.absoluteFilePath(entry);
    }
    return {};
}

QString PathUtils::ciResolvePath(const QString &basePath, const QString &relativePath)
{
    QString result = basePath;
    const auto parts = relativePath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const auto &part : parts) {
        QString found = ciFindFile(result, part);
        if (found.isEmpty())
            return {};
        result = found;
    }
    return result;
}

bool PathUtils::parsePosition(const QString &value, float &x, float &y, float &z)
{
    const auto parts = value.split(QLatin1Char(','));
    if (parts.size() < 3)
        return false;
    bool okX, okY, okZ;
    x = parts[0].trimmed().toFloat(&okX);
    y = parts[1].trimmed().toFloat(&okY);
    z = parts[2].trimmed().toFloat(&okZ);
    return okX && okY && okZ;
}

} // namespace flatlas::core
