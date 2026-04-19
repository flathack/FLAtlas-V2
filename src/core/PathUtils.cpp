#include "PathUtils.h"
#include <QDir>
#include <QRegularExpression>

namespace flatlas::core {

QString PathUtils::ciFindFile(const QString &directory, const QString &filename)
{
    QDir dir(directory);
    if (!dir.exists())
        return {};
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
    static const QRegularExpression separator(QStringLiteral("[/\\\\]"));
    const auto parts = relativePath.split(separator, Qt::SkipEmptyParts);
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

QString PathUtils::normalizePath(const QString &path)
{
    QString result = path;
    result.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (result.endsWith(QLatin1Char('/')))
        result.chop(1);
    return result;
}

} // namespace flatlas::core
