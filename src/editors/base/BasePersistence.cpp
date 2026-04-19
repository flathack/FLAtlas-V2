// editors/base/BasePersistence.cpp – Laden/Speichern von Basis-Daten (Phase 10)

#include "BasePersistence.h"
#include "infrastructure/parser/IniParser.h"

#include <QFile>
#include <QHash>
#include <QTextStream>

using namespace flatlas::domain;
using namespace flatlas::infrastructure;

namespace flatlas::editors {

// ─── Parse helpers ───────────────────────────────────────

static QVector3D parsePos(const QString &val)
{
    const auto parts = val.split(',');
    if (parts.size() >= 3)
        return QVector3D(parts[0].trimmed().toFloat(),
                         parts[1].trimmed().toFloat(),
                         parts[2].trimmed().toFloat());
    return {};
}

static QString posToString(const QVector3D &v)
{
    return QStringLiteral("%1, %2, %3")
        .arg(v.x(), 0, 'f', 0)
        .arg(v.y(), 0, 'f', 0)
        .arg(v.z(), 0, 'f', 0);
}

// ─── Load single base by nickname ────────────────────────

std::unique_ptr<BaseData> BasePersistence::loadFromIni(const QString &filePath,
                                                        const QString &baseNickname)
{
    auto doc = IniParser::parseFile(filePath);
    if (doc.isEmpty())
        return nullptr;

    // Find the [Object] section with matching nickname and base archetype indicators
    for (const auto &sec : doc) {
        if (sec.name.compare(QStringLiteral("Object"), Qt::CaseInsensitive) != 0)
            continue;

        QString nickname = sec.value(QStringLiteral("nickname"));
        if (nickname.compare(baseNickname, Qt::CaseInsensitive) != 0)
            continue;

        auto base = std::make_unique<BaseData>();
        base->nickname = nickname;
        base->archetype = sec.value(QStringLiteral("archetype"));
        base->idsName = sec.value(QStringLiteral("ids_name")).toInt();
        base->idsInfo = sec.value(QStringLiteral("ids_info")).toInt();
        base->position = parsePos(sec.value(QStringLiteral("pos")));
        base->system = sec.value(QStringLiteral("system"));

        // dock_with entries
        for (const auto &dw : sec.values(QStringLiteral("dock_with")))
            base->dockWith.append(dw);

        // Collect rooms: look for matching [BaseRoom] sections following
        // a base= nickname pattern or simply from the whole file
        QString baseLower = baseNickname.toLower();
        for (const auto &roomSec : doc) {
            if (roomSec.name.compare(QStringLiteral("BaseRoom"), Qt::CaseInsensitive) != 0)
                continue;
            QString roomBase = roomSec.value(QStringLiteral("base"));
            if (roomBase.compare(baseNickname, Qt::CaseInsensitive) != 0)
                continue;

            RoomData room;
            room.nickname = roomSec.value(QStringLiteral("nickname"));
            room.type = roomSec.value(QStringLiteral("type"));
            base->rooms.append(room);
        }

        return base;
    }
    return nullptr;
}

// ─── Load all bases ──────────────────────────────────────

QVector<BaseData> BasePersistence::loadAllBases(const QString &filePath)
{
    auto doc = IniParser::parseFile(filePath);
    QVector<BaseData> result;

    // First pass: collect all [Object] sections that look like bases
    // (have a base= property or dock_with, or archetype contains "station"/"planet")
    QHash<QString, int> baseIndex; // nickname → index in result

    for (const auto &sec : doc) {
        if (sec.name.compare(QStringLiteral("Object"), Qt::CaseInsensitive) != 0)
            continue;

        QString nickname = sec.value(QStringLiteral("nickname"));
        if (nickname.isEmpty()) continue;

        // Check if this is a base (has dock_with or base= in rooms, or is_base flag)
        QString base = sec.value(QStringLiteral("base"));
        bool hasDock = !sec.values(QStringLiteral("dock_with")).isEmpty();
        bool hasBase = !base.isEmpty();

        // We include objects that have dock_with or a base reference
        if (!hasDock && !hasBase) continue;

        BaseData bd;
        bd.nickname = nickname;
        bd.archetype = sec.value(QStringLiteral("archetype"));
        bd.idsName = sec.value(QStringLiteral("ids_name")).toInt();
        bd.idsInfo = sec.value(QStringLiteral("ids_info")).toInt();
        bd.position = parsePos(sec.value(QStringLiteral("pos")));
        bd.system = sec.value(QStringLiteral("system"));

        for (const auto &dw : sec.values(QStringLiteral("dock_with")))
            bd.dockWith.append(dw);

        baseIndex.insert(nickname.toLower(), result.size());
        result.append(std::move(bd));
    }

    // Second pass: attach rooms to bases
    for (const auto &sec : doc) {
        if (sec.name.compare(QStringLiteral("BaseRoom"), Qt::CaseInsensitive) != 0)
            continue;
        QString roomBase = sec.value(QStringLiteral("base"));
        auto it = baseIndex.find(roomBase.toLower());
        if (it == baseIndex.end()) continue;

        RoomData room;
        room.nickname = sec.value(QStringLiteral("nickname"));
        room.type = sec.value(QStringLiteral("type"));
        result[it.value()].rooms.append(room);
    }

    return result;
}

// ─── Save ────────────────────────────────────────────────

bool BasePersistence::save(const BaseData &base, const QString &filePath)
{
    // Build IniDocument for the base
    IniDocument doc;

    // [Object] section
    IniSection objSec;
    objSec.name = QStringLiteral("Object");
    objSec.entries.append({QStringLiteral("nickname"), base.nickname});
    if (!base.archetype.isEmpty())
        objSec.entries.append({QStringLiteral("archetype"), base.archetype});
    if (base.idsName > 0)
        objSec.entries.append({QStringLiteral("ids_name"), QString::number(base.idsName)});
    if (base.idsInfo > 0)
        objSec.entries.append({QStringLiteral("ids_info"), QString::number(base.idsInfo)});
    if (!base.position.isNull())
        objSec.entries.append({QStringLiteral("pos"), posToString(base.position)});
    if (!base.system.isEmpty())
        objSec.entries.append({QStringLiteral("system"), base.system});
    for (const auto &dw : base.dockWith)
        objSec.entries.append({QStringLiteral("dock_with"), dw});
    if (!base.nickname.isEmpty())
        objSec.entries.append({QStringLiteral("base"), base.nickname});
    doc.append(objSec);

    // [BaseRoom] sections
    for (const auto &room : base.rooms) {
        IniSection roomSec;
        roomSec.name = QStringLiteral("BaseRoom");
        roomSec.entries.append({QStringLiteral("base"), base.nickname});
        roomSec.entries.append({QStringLiteral("nickname"), room.nickname});
        if (!room.type.isEmpty())
            roomSec.entries.append({QStringLiteral("type"), room.type});
        doc.append(roomSec);
    }

    QString text = IniParser::serialize(doc);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream out(&file);
    out << text;
    return true;
}

} // namespace flatlas::editors
