// editors/npc/MbaseOperations.cpp – MBases.ini parsen und schreiben (Phase 14)

#include "MbaseOperations.h"
#include "infrastructure/parser/IniParser.h"

using namespace flatlas::infrastructure;

namespace flatlas::editors {

QVector<MbaseData> MbaseOperations::parseFile(const QString &filePath)
{
    QVector<MbaseData> result;
    auto doc = IniParser::parseFile(filePath);

    MbaseData *currentBase = nullptr;

    for (const auto &sec : doc) {
        if (sec.name.compare(QStringLiteral("MBase"), Qt::CaseInsensitive) == 0) {
            MbaseData base;
            base.baseName = sec.value(QStringLiteral("nickname"));
            result.append(base);
            currentBase = &result.last();

        } else if (sec.name.compare(QStringLiteral("MRoom"), Qt::CaseInsensitive) == 0) {
            if (!currentBase) continue;

            MbaseRoom room;
            room.nickname = sec.value(QStringLiteral("nickname"));
            room.base = currentBase->baseName;

            // Collect NPC references
            for (const auto &entry : sec.entries) {
                if (entry.first.compare(QStringLiteral("character_density"), Qt::CaseInsensitive) == 0) {
                    // room-level density, skip for now
                } else if (entry.first.compare(QStringLiteral("fixture"), Qt::CaseInsensitive) == 0) {
                    // fixture = npc_name, stand_point, ...
                    auto parts = entry.second.split(',');
                    if (!parts.isEmpty())
                        room.npcs.append(parts[0].trimmed());
                }
            }

            currentBase->rooms.append(room);

        } else if (sec.name.compare(QStringLiteral("GF_NPC"), Qt::CaseInsensitive) == 0) {
            if (!currentBase) continue;

            MbaseNpc npc;
            npc.base = currentBase->baseName;

            for (const auto &entry : sec.entries) {
                const QString &key = entry.first;
                const QString &val = entry.second;

                if (key.compare(QStringLiteral("nickname"), Qt::CaseInsensitive) == 0) {
                    npc.nickname = val;
                } else if (key.compare(QStringLiteral("room"), Qt::CaseInsensitive) == 0) {
                    npc.room = val;
                } else if (key.compare(QStringLiteral("affiliation"), Qt::CaseInsensitive) == 0) {
                    npc.faction = val;
                } else if (key.compare(QStringLiteral("individual_name"), Qt::CaseInsensitive) == 0) {
                    npc.idsName = val.toInt();
                } else if (key.compare(QStringLiteral("info"), Qt::CaseInsensitive) == 0) {
                    npc.idsInfo = val.toInt();
                } else if (key.compare(QStringLiteral("npc_type"), Qt::CaseInsensitive) == 0) {
                    npc.type = val;
                } else {
                    npc.rawEntries.append({key, val});
                }
            }

            currentBase->npcs.append(npc);
        }
    }

    return result;
}

QString MbaseOperations::serialize(const QVector<MbaseData> &bases)
{
    QString out;

    for (const auto &base : bases) {
        out += QStringLiteral("[MBase]\n");
        out += QStringLiteral("nickname = %1\n\n").arg(base.baseName);

        for (const auto &room : base.rooms) {
            out += QStringLiteral("[MRoom]\n");
            out += QStringLiteral("nickname = %1\n").arg(room.nickname);
            for (const auto &npcRef : room.npcs) {
                out += QStringLiteral("fixture = %1, stand, Fixer\n").arg(npcRef);
            }
            out += QLatin1Char('\n');
        }

        for (const auto &npc : base.npcs) {
            out += QStringLiteral("[GF_NPC]\n");
            out += QStringLiteral("nickname = %1\n").arg(npc.nickname);
            if (!npc.room.isEmpty())
                out += QStringLiteral("room = %1\n").arg(npc.room);
            if (!npc.faction.isEmpty())
                out += QStringLiteral("affiliation = %1\n").arg(npc.faction);
            if (npc.idsName > 0)
                out += QStringLiteral("individual_name = %1\n").arg(npc.idsName);
            if (npc.idsInfo > 0)
                out += QStringLiteral("info = %1\n").arg(npc.idsInfo);
            if (!npc.type.isEmpty())
                out += QStringLiteral("npc_type = %1\n").arg(npc.type);

            for (const auto &[key, val] : npc.rawEntries) {
                out += QStringLiteral("%1 = %2\n").arg(key, val);
            }
            out += QLatin1Char('\n');
        }
    }

    return out;
}

QVector<MbaseNpc> MbaseOperations::npcsForBase(const QVector<MbaseData> &bases,
                                                const QString &baseNickname)
{
    for (const auto &base : bases) {
        if (base.baseName.compare(baseNickname, Qt::CaseInsensitive) == 0)
            return base.npcs;
    }
    return {};
}

} // namespace flatlas::editors
