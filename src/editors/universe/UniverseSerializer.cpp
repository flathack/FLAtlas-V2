// editors/universe/UniverseSerializer.cpp – Universe.ini Laden/Speichern (Phase 9)

#include "UniverseSerializer.h"
#include "core/PathUtils.h"
#include "infrastructure/parser/IniParser.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSet>
#include <QRegularExpression>
#include <QHash>
#include <QPointF>

namespace flatlas::editors {

using namespace flatlas::infrastructure;
using namespace flatlas::domain;

// ─── helpers ──────────────────────────────────────────────

static QVector3D parsePos(const QString &val)
{
    QStringList parts = val.split(',');
    if (parts.size() >= 2) {
        return QVector3D(parts[0].trimmed().toFloat(),
                         parts.size() >= 3 ? parts[1].trimmed().toFloat() : 0,
                         parts.last().trimmed().toFloat());
    }
    return {};
}

static QString posToString(const QVector3D &v)
{
    return QStringLiteral("%1, %2").arg(v.x(), 0, 'f', 0).arg(v.z(), 0, 'f', 0);
}

static QString sectorDisplayName(const QString &sectorKey)
{
    if (sectorKey.compare(QStringLiteral("universe"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("Sirius");

    static const QRegularExpression sectorRe(QStringLiteral("^sector(\\d+)$"),
                                             QRegularExpression::CaseInsensitiveOption);
    const auto match = sectorRe.match(sectorKey.trimmed());
    if (match.hasMatch())
        return QStringLiteral("Sector %1").arg(match.captured(1));

    return sectorKey.trimmed();
}

struct MultiUniverseParseResult {
    QHash<QString, QHash<QString, QPointF>> systemSectorPositions;
    QVector<SectorDefinition> sectors;
};

static MultiUniverseParseResult parseMultiUniverse(const QString &universeDir)
{
    MultiUniverseParseResult result;

    const QString multiUniversePath =
        flatlas::core::PathUtils::ciResolvePath(universeDir, QStringLiteral("multiuniverse.ini"));
    if (multiUniversePath.isEmpty())
        return result;

    const IniDocument doc = IniParser::parseFile(multiUniversePath);
    QHash<QString, int> sectorIndexByKey;

    for (const auto &section : doc) {
        if (section.name.compare(QStringLiteral("sector"), Qt::CaseInsensitive) != 0)
            continue;

        QString sectorKey;
        for (const auto &entry : section.entries) {
            if (entry.first.compare(QStringLiteral("mapping"), Qt::CaseInsensitive) == 0) {
                sectorKey = entry.second.split(QLatin1Char(',')).value(0).trimmed().toLower();
                break;
            }
        }
        if (sectorKey.isEmpty())
            continue;

        if (!sectorIndexByKey.contains(sectorKey)) {
            SectorDefinition def;
            def.key = sectorKey;
            def.displayName = sectorDisplayName(sectorKey);
            def.sourceMap = sectorKey;
            sectorIndexByKey.insert(sectorKey, result.sectors.size());
            result.sectors.append(def);
        }

        SectorDefinition &sectorDef = result.sectors[sectorIndexByKey.value(sectorKey)];
        for (const auto &entry : section.entries) {
            if (entry.first.compare(QStringLiteral("label"), Qt::CaseInsensitive) == 0) {
                const QString labelId = entry.second.split(QLatin1Char(',')).value(0).trimmed();
                if (!labelId.isEmpty() && !sectorDef.labelIds.contains(labelId))
                    sectorDef.labelIds.append(labelId);
                continue;
            }
            if (entry.first.compare(QStringLiteral("system"), Qt::CaseInsensitive) != 0)
                continue;

            const QStringList parts = entry.second.split(QLatin1Char(','));
            if (parts.size() < 3)
                continue;

            const QString nickname = parts[0].trimmed().toUpper();
            bool okX = false;
            bool okY = false;
            const double x = parts[1].trimmed().toDouble(&okX);
            const double y = parts[2].trimmed().toDouble(&okY);
            if (nickname.isEmpty() || !okX || !okY)
                continue;

            result.systemSectorPositions[nickname].insert(sectorKey, QPointF(x, y));
        }
    }

    return result;
}

static IniSection buildSystemSection(const SystemInfo &sys)
{
    IniSection section;
    section.name = QStringLiteral("System");

    if (sys.rawEntries.isEmpty()) {
        section.entries.append({QStringLiteral("nickname"), sys.nickname});
        section.entries.append({QStringLiteral("file"), sys.filePath});
        if (sys.idsName > 0)
            section.entries.append({QStringLiteral("ids_name"), QString::number(sys.idsName)});
        if (sys.stridName > 0)
            section.entries.append({QStringLiteral("strid_name"), QString::number(sys.stridName)});
        section.entries.append({QStringLiteral("pos"), posToString(sys.position)});
        return section;
    }

    bool wroteNickname = false;
    bool wroteFile = false;
    bool wrotePos = false;
    bool wroteIdsName = false;
    bool wroteStridName = false;

    for (const auto &entry : sys.rawEntries) {
        const QString lowered = entry.first.trimmed().toLower();
        if (lowered == QStringLiteral("nickname")) {
            section.entries.append({entry.first, sys.nickname});
            wroteNickname = true;
        } else if (lowered == QStringLiteral("file")) {
            section.entries.append({entry.first, sys.filePath});
            wroteFile = true;
        } else if (lowered == QStringLiteral("pos")) {
            section.entries.append({entry.first, posToString(sys.position)});
            wrotePos = true;
        } else if (lowered == QStringLiteral("ids_name")) {
            if (sys.idsName > 0) {
                section.entries.append({entry.first, QString::number(sys.idsName)});
                wroteIdsName = true;
            }
        } else if (lowered == QStringLiteral("strid_name")) {
            if (sys.stridName > 0) {
                section.entries.append({entry.first, QString::number(sys.stridName)});
                wroteStridName = true;
            }
        } else {
            section.entries.append(entry);
        }
    }

    if (!wroteNickname)
        section.entries.append({QStringLiteral("nickname"), sys.nickname});
    if (!wroteFile)
        section.entries.append({QStringLiteral("file"), sys.filePath});
    if (sys.idsName > 0 && !wroteIdsName)
        section.entries.append({QStringLiteral("ids_name"), QString::number(sys.idsName)});
    if (sys.stridName > 0 && !wroteStridName)
        section.entries.append({QStringLiteral("strid_name"), QString::number(sys.stridName)});
    if (!wrotePos)
        section.entries.append({QStringLiteral("pos"), posToString(sys.position)});

    return section;
}

/// Classify an [Object] with goto as gate/hole/other based on archetype.
static QString classifyJumpKind(const QString &archetype, const QString &gotoVal)
{
    QString arch = archetype.toLower().trimmed();

    if (arch.contains(QStringLiteral("jumpgate")) ||
        arch.contains(QStringLiteral("jump_gate")) ||
        arch.contains(QStringLiteral("jumppoint_gate")) ||
        arch.contains(QStringLiteral("nomad_gate")))
        return QStringLiteral("gate");

    if (arch.contains(QStringLiteral("jumphole")) ||
        arch.contains(QStringLiteral("jump_hole")))
        return QStringLiteral("hole");

    if (!gotoVal.isEmpty())
        return QStringLiteral("other");

    return {};
}

/// Scan a single system INI file for jump objects, return connections.
static QVector<JumpConnection> scanSystemConnections(const QString &systemNickname,
                                                      const QString &systemFilePath)
{
    QVector<JumpConnection> result;
    if (systemFilePath.isEmpty() || !QFile::exists(systemFilePath))
        return result;

    IniDocument doc = IniParser::parseFile(systemFilePath);
    for (const IniSection &section : doc) {
        if (section.name.compare(QStringLiteral("Object"), Qt::CaseInsensitive) != 0)
            continue;

        QString archetype = section.value(QStringLiteral("archetype"));
        QString gotoVal = section.value(QStringLiteral("goto"));
        QString objNickname = section.value(QStringLiteral("nickname"));

        QString kind = classifyJumpKind(archetype, gotoVal);
        if (kind.isEmpty())
            continue;

        // Determine destination system from goto field: "target_system, target_object, tunnel"
        QString destSystem;
        QString destObject;
        if (!gotoVal.isEmpty()) {
            QStringList gotoParts = gotoVal.split(QLatin1Char(','));
            if (!gotoParts.isEmpty())
                destSystem = gotoParts[0].trimmed();
            if (gotoParts.size() >= 2)
                destObject = gotoParts[1].trimmed();
        }

        // Fallback: parse nickname like "Li01_to_Li02_jumphole"
        if (destSystem.isEmpty() && !objNickname.isEmpty()) {
            static QRegularExpression re(QStringLiteral("to_([A-Za-z0-9]+)"),
                                          QRegularExpression::CaseInsensitiveOption);
            auto match = re.match(objNickname);
            if (match.hasMatch())
                destSystem = match.captured(1);
        }

        if (destSystem.isEmpty() ||
            destSystem.compare(systemNickname, Qt::CaseInsensitive) == 0)
            continue;

        JumpConnection conn;
        conn.fromSystem = systemNickname;
        conn.fromObject = objNickname;
        conn.toSystem = destSystem;
        conn.toObject = destObject;
        conn.kind = kind;
        result.append(conn);
    }

    return result;
}

// ─── load ─────────────────────────────────────────────────

std::unique_ptr<UniverseData> UniverseSerializer::load(const QString &filePath)
{
    auto doc = IniParser::parseFile(filePath);
    if (doc.isEmpty())
        return nullptr;

    auto universe = std::make_unique<UniverseData>();
    const QString universeDir = QFileInfo(filePath).absolutePath();
    const MultiUniverseParseResult multiUniverse = parseMultiUniverse(universeDir);
    QSet<QString> reservedSectorNicknames;
    for (const auto &sector : multiUniverse.sectors)
        reservedSectorNicknames.insert(sector.key.trimmed().toLower());

    SectorDefinition baseSector;
    baseSector.key = QStringLiteral("universe");
    baseSector.displayName = QStringLiteral("Sirius");
    baseSector.sourceMap = QStringLiteral("universe");
    universe->sectors.append(baseSector);
    for (const auto &sector : multiUniverse.sectors)
        universe->sectors.append(sector);
    universe->multiverseDetected = !multiUniverse.sectors.isEmpty();

    for (const auto &section : doc) {
        if (section.name.compare(QStringLiteral("System"), Qt::CaseInsensitive) == 0) {
            SystemInfo sys;
            sys.nickname = section.value(QStringLiteral("nickname"));
            if (reservedSectorNicknames.contains(sys.nickname.trimmed().toLower()))
                continue;
            sys.filePath = section.value(QStringLiteral("file"));
            sys.idsName = section.value(QStringLiteral("ids_name"), QStringLiteral("0")).toInt();
            sys.stridName = section.value(QStringLiteral("strid_name"), QStringLiteral("0")).toInt();
            sys.rawEntries = section.entries;

            // Parse pos (2D: x, z)
            QString posStr = section.value(QStringLiteral("pos"));
            if (!posStr.isEmpty())
                sys.position = parsePos(posStr);

            // Display name defaults to nickname
            sys.displayName = sys.nickname;

            universe->addSystem(sys);

            if (SystemInfo *stored = universe->findSystem(sys.nickname)) {
                stored->sectorPositions.insert(QStringLiteral("universe"),
                                               QPointF(sys.position.x(), sys.position.z()));
                const auto sectorIt = multiUniverse.systemSectorPositions.constFind(sys.nickname.toUpper());
                if (sectorIt != multiUniverse.systemSectorPositions.constEnd()) {
                    for (auto it = sectorIt.value().constBegin(); it != sectorIt.value().constEnd(); ++it)
                        stored->sectorPositions.insert(it.key(), it.value());
                }
            }
        }
    }

    // Build jump connections by scanning system files for jump objects.
    // In universe.ini, system file paths are relative to the universe.ini folder,
    // typically .../DATA/UNIVERSE.

    QSet<QString> seenEdges; // deduplicate bidirectional connections per kind
    for (const auto &sys : universe->systems) {
        QString sysFilePath;
        if (!sys.filePath.isEmpty()) {
            sysFilePath = flatlas::core::PathUtils::ciResolvePath(universeDir, sys.filePath);
            if (sysFilePath.isEmpty())
                sysFilePath = QDir(universeDir).filePath(sys.filePath);
        }

        auto conns = scanSystemConnections(sys.nickname, sysFilePath);
        for (const auto &conn : conns) {
            const QString a = conn.fromSystem.toLower();
            const QString b = conn.toSystem.toLower();
            const QString left = (a < b) ? a : b;
            const QString right = (a < b) ? b : a;
            const QString key = left + QStringLiteral("|") + right +
                                QStringLiteral("|") + conn.kind.toLower();
            if (!seenEdges.contains(key)) {
                seenEdges.insert(key);
                universe->connections.append(conn);
            }
        }
    }

    return universe;
}

// ─── save ─────────────────────────────────────────────────

bool UniverseSerializer::save(const UniverseData &data, const QString &filePath)
{
    IniDocument doc;
    QHash<QString, const SystemInfo *> systemsByNickname;
    for (const auto &sys : data.systems)
        systemsByNickname.insert(sys.nickname.trimmed().toLower(), &sys);

    if (QFile::exists(filePath)) {
        const IniDocument existing = IniParser::parseFile(filePath);
        QSet<QString> writtenSystems;

        for (const auto &section : existing) {
            if (section.name.compare(QStringLiteral("System"), Qt::CaseInsensitive) != 0) {
                doc.append(section);
                continue;
            }

            const QString nickname = section.value(QStringLiteral("nickname")).trimmed().toLower();
            const auto it = systemsByNickname.constFind(nickname);
            if (it == systemsByNickname.constEnd())
                continue;

            doc.append(buildSystemSection(*it.value()));
            writtenSystems.insert(nickname);
        }

        for (const auto &sys : data.systems) {
            const QString nickname = sys.nickname.trimmed().toLower();
            if (!writtenSystems.contains(nickname))
                doc.append(buildSystemSection(sys));
        }
    } else {
        for (const auto &sys : data.systems)
            doc.append(buildSystemSection(sys));
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    file.write(IniParser::serialize(doc).toUtf8());
    return true;
}

} // namespace flatlas::editors
