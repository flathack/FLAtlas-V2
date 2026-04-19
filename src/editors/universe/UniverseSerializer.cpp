// editors/universe/UniverseSerializer.cpp – Universe.ini Laden/Speichern (Phase 9)

#include "UniverseSerializer.h"
#include "infrastructure/parser/IniParser.h"
#include <QFile>
#include <QFileInfo>

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

// ─── load ─────────────────────────────────────────────────

std::unique_ptr<UniverseData> UniverseSerializer::load(const QString &filePath)
{
    auto doc = IniParser::parseFile(filePath);
    if (doc.isEmpty())
        return nullptr;

    auto universe = std::make_unique<UniverseData>();
    QString baseDir = QFileInfo(filePath).absolutePath();

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
    // (This is done lazily or on demand; for now store what we have from universe.ini)
    // Jump connections are typically derived from system files, not universe.ini itself.
    // We'll populate them when systems are opened.

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
