// editors/universe/UniverseSerializer.cpp – Universe.ini Laden/Speichern (Phase 9)

#include "UniverseSerializer.h"
#include "infrastructure/parser/IniParser.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSet>
#include <QRegularExpression>

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

/// Classify whether an [Object] is a jump gate/hole/alien_gate based on archetype.
static QString classifyJumpKind(const QString &archetype, const QString &gotoVal,
                                 const QString &jumpEffect)
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

    // Alien gates: have jump_effect + goto but non-standard archetype
    if (!jumpEffect.isEmpty() && !gotoVal.isEmpty())
        return QStringLiteral("alien_gate");

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
        QString jumpEffect = section.value(QStringLiteral("jump_effect"));
        QString objNickname = section.value(QStringLiteral("nickname"));

        QString kind = classifyJumpKind(archetype, gotoVal, jumpEffect);
        if (kind.isEmpty())
            continue;

        // Determine destination system from goto field: "target_system, target_object, tunnel"
        QString destSystem;
        if (!gotoVal.isEmpty()) {
            QStringList gotoParts = gotoVal.split(QLatin1Char(','));
            if (!gotoParts.isEmpty())
                destSystem = gotoParts[0].trimmed();
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
    QString baseDir = QFileInfo(filePath).absolutePath();
    // Base dir is typically DATA/UNIVERSE; we need the DATA dir parent
    QDir universeDir(baseDir);

    for (const auto &section : doc) {
        if (section.name.compare(QStringLiteral("System"), Qt::CaseInsensitive) == 0) {
            SystemInfo sys;
            sys.nickname = section.value(QStringLiteral("nickname"));
            sys.filePath = section.value(QStringLiteral("file"));
            sys.idsName = section.value(QStringLiteral("ids_name"), QStringLiteral("0")).toInt();
            sys.stridName = section.value(QStringLiteral("strid_name"), QStringLiteral("0")).toInt();

            // Parse pos (2D: x, z)
            QString posStr = section.value(QStringLiteral("pos"));
            if (!posStr.isEmpty())
                sys.position = parsePos(posStr);

            // Display name defaults to nickname
            sys.displayName = sys.nickname;

            universe->addSystem(sys);
        }
    }

    // Build jump connections by scanning system files for jump objects
    // System file paths in universe.ini are relative to the DATA directory
    // baseDir is typically .../DATA/UNIVERSE, so go up one level
    QString dataDir = QFileInfo(baseDir).absolutePath();

    QSet<QPair<QString, QString>> seenEdges; // deduplicate bidirectional connections
    for (const auto &sys : universe->systems) {
        QString sysFilePath;
        if (!sys.filePath.isEmpty()) {
            // Resolve relative to DATA dir
            sysFilePath = QDir(dataDir).filePath(sys.filePath);
            if (!QFile::exists(sysFilePath)) {
                // Try case-insensitive
                sysFilePath = QDir(dataDir).filePath(sys.filePath.toLower());
            }
        }

        auto conns = scanSystemConnections(sys.nickname, sysFilePath);
        for (const auto &conn : conns) {
            // Deduplicate: only keep one direction per pair
            auto key = qMakePair(conn.fromSystem.toLower(), conn.toSystem.toLower());
            auto reverseKey = qMakePair(conn.toSystem.toLower(), conn.fromSystem.toLower());
            if (!seenEdges.contains(key) && !seenEdges.contains(reverseKey)) {
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

    for (const auto &sys : data.systems) {
        IniSection section;
        section.name = QStringLiteral("System");
        section.entries.append({QStringLiteral("nickname"), sys.nickname});
        section.entries.append({QStringLiteral("file"), sys.filePath});

        if (sys.idsName > 0)
            section.entries.append({QStringLiteral("ids_name"), QString::number(sys.idsName)});
        if (sys.stridName > 0)
            section.entries.append({QStringLiteral("strid_name"), QString::number(sys.stridName)});

        section.entries.append({QStringLiteral("pos"), posToString(sys.position)});

        doc.append(section);
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    file.write(IniParser::serialize(doc).toUtf8());
    return true;
}

} // namespace flatlas::editors
