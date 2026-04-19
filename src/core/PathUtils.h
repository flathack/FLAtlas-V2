#pragma once
// core/PathUtils.h – Case-insensitive Pfadauflösung (Freelancer-spezifisch)

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

    /// Pfad normalisieren: Backslashes zu Forward-Slashes, trailing Slash entfernen.
    static QString normalizePath(const QString &path);
};

} // namespace flatlas::core
