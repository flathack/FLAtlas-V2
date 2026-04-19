#include "UniverseScanner.h"
#include "core/PathUtils.h"
#include "infrastructure/parser/IniParser.h"
#include <QDir>

namespace flatlas::infrastructure {

flatlas::domain::UniverseData UniverseScanner::scan(const QString &dataDir)
{
    flatlas::domain::UniverseData result;
    if (dataDir.isEmpty())
        return result;

    // Case-insensitive: find UNIVERSE/universe.ini
    QString universeIni = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("UNIVERSE/universe.ini"));
    if (universeIni.isEmpty())
        return result;

    IniDocument doc = IniParser::parseFile(universeIni);

    for (const IniSection &section : doc) {
        if (section.name.compare(QStringLiteral("system"), Qt::CaseInsensitive) != 0)
            continue;

        flatlas::domain::SystemInfo sys;
        sys.nickname = section.value(QStringLiteral("nickname"));
        sys.filePath = section.value(QStringLiteral("file"));

        // Parse position "x, y" → (x, y) with z=0
        QString posStr = section.value(QStringLiteral("pos"));
        if (!posStr.isEmpty()) {
            float px = 0, py = 0, pz = 0;
            if (flatlas::core::PathUtils::parsePosition(posStr, px, py, pz)) {
                sys.position = QVector3D(px, py, pz);
            } else {
                // Try simpler "x, y" format
                QStringList parts = posStr.split(QLatin1Char(','));
                if (parts.size() >= 2) {
                    sys.position = QVector3D(parts[0].trimmed().toFloat(),
                                             parts[1].trimmed().toFloat(), 0.0f);
                }
            }
        }

        // ids_name / strid_name
        QString idsStr = section.value(QStringLiteral("ids_name"));
        if (!idsStr.isEmpty())
            sys.idsName = idsStr.toInt();
        QString stridStr = section.value(QStringLiteral("strid_name"));
        if (!stridStr.isEmpty())
            sys.stridName = stridStr.toInt();

        result.systems.append(sys);
    }

    return result;
}

} // namespace flatlas::infrastructure
