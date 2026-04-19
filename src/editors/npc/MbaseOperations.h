#pragma once
// editors/npc/MbaseOperations.h – MBases.ini parsen und schreiben (Phase 14)

#include <QString>
#include <QVector>
#include <QMap>

namespace flatlas::editors {

struct MbaseNpc {
    QString nickname;
    QString base;         // base this NPC belongs to
    QString room;         // room in the base
    QString faction;
    float density = 1.0f;
    int idsName = 0;
    int idsInfo = 0;
    QString type;         // mission_giver, bartender, etc.
    QVector<QPair<QString, QString>> rawEntries; // preserve unknown keys
};

struct MbaseRoom {
    QString nickname;
    QString base;
    QStringList npcs;     // npc nicknames in this room
};

struct MbaseData {
    QString baseName;     // [MBase] nickname
    QVector<MbaseRoom> rooms;
    QVector<MbaseNpc> npcs;
};

class MbaseOperations {
public:
    /// Parse an MBases.ini file and extract all bases with rooms and NPCs.
    static QVector<MbaseData> parseFile(const QString &filePath);

    /// Serialize MBase data back to INI text.
    static QString serialize(const QVector<MbaseData> &bases);

    /// Find all NPCs for a given base nickname.
    static QVector<MbaseNpc> npcsForBase(const QVector<MbaseData> &bases,
                                          const QString &baseNickname);
};

} // namespace flatlas::editors
