// editors/base/BaseBuilder.cpp – Neue Basis mit Templates erstellen (Phase 10)

#include "BaseBuilder.h"

using namespace flatlas::domain;

namespace flatlas::editors {

QVector<RoomData> BaseBuilder::defaultRooms(BaseTemplate tmpl)
{
    QVector<RoomData> rooms;
    switch (tmpl) {
    case BaseTemplate::Station:
        rooms.append({QStringLiteral("bar"), QStringLiteral("Bar")});
        rooms.append({QStringLiteral("equip"), QStringLiteral("Equip")});
        rooms.append({QStringLiteral("commodity"), QStringLiteral("Commodity")});
        rooms.append({QStringLiteral("shipdealer"), QStringLiteral("ShipDealer")});
        break;
    case BaseTemplate::Planet:
        rooms.append({QStringLiteral("bar"), QStringLiteral("Bar")});
        rooms.append({QStringLiteral("equip"), QStringLiteral("Equip")});
        rooms.append({QStringLiteral("commodity"), QStringLiteral("Commodity")});
        break;
    case BaseTemplate::Outpost:
        rooms.append({QStringLiteral("bar"), QStringLiteral("Bar")});
        rooms.append({QStringLiteral("equip"), QStringLiteral("Equip")});
        break;
    case BaseTemplate::Wreck:
        break; // No rooms
    }
    return rooms;
}

BaseData BaseBuilder::create(BaseTemplate tmpl, const QString &nickname,
                              const QString &system)
{
    BaseData base;
    base.nickname = nickname;
    base.system = system;

    switch (tmpl) {
    case BaseTemplate::Station:
        base.archetype = QStringLiteral("space_station");
        break;
    case BaseTemplate::Planet:
        base.archetype = QStringLiteral("planet");
        break;
    case BaseTemplate::Outpost:
        base.archetype = QStringLiteral("space_outpost");
        break;
    case BaseTemplate::Wreck:
        base.archetype = QStringLiteral("space_wreck");
        break;
    }

    // Populate rooms from template
    auto rooms = defaultRooms(tmpl);
    for (auto &room : rooms) {
        // Prefix room nicknames with base nickname
        room.nickname = nickname + QStringLiteral("_") + room.nickname;
    }
    base.rooms = rooms;

    return base;
}

QStringList BaseBuilder::templateNames()
{
    return {
        QStringLiteral("Station"),
        QStringLiteral("Planet"),
        QStringLiteral("Outpost"),
        QStringLiteral("Wreck"),
    };
}

BaseTemplate BaseBuilder::templateFromName(const QString &name)
{
    if (name.compare(QStringLiteral("Planet"), Qt::CaseInsensitive) == 0)
        return BaseTemplate::Planet;
    if (name.compare(QStringLiteral("Outpost"), Qt::CaseInsensitive) == 0)
        return BaseTemplate::Outpost;
    if (name.compare(QStringLiteral("Wreck"), Qt::CaseInsensitive) == 0)
        return BaseTemplate::Wreck;
    return BaseTemplate::Station; // default
}

} // namespace flatlas::editors
