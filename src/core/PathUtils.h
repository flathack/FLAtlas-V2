#pragma once
// core/PathUtils.h – Case-insensitive Pfadauflösung (Freelancer-spezifisch)
// TODO Phase 1

#include <QString>

namespace flatlas::core {

class PathUtils
{
public:
    /// Case-insensitive Datei finden in Verzeichnis.
    static QString ciFindFile(const QString &directory, const QString &filename);

    /// Case-insensitive Pfad auflösen (mehrstufig).
    static QString ciResolvePath(const QString &basePath, const QString &relativePath);

    /// Position aus INI-String parsen ("x, y, z").
    static bool parsePosition(const QString &value, float &x, float &y, float &z);
};

} // namespace flatlas::core
