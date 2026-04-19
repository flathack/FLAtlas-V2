#pragma once
// domain/NpcData.h – NPC-Datenmodell

#include <QString>
#include <QVector>

namespace flatlas::domain {

struct FactionInfo {
    QString nickname;
    QString displayName;
    int idsName = 0;
    float rep = 0.0f;
};

struct NpcData {
    QString nickname;
    QString room;
    QString faction;
    float density = 1.0f;
};

class NpcDataStore {
public:
    void addFaction(const FactionInfo &info) { m_factions.append(info); }
    const QVector<FactionInfo> &factions() const { return m_factions; }
    int factionCount() const { return m_factions.size(); }

    const FactionInfo *findFaction(const QString &nickname) const {
        for (const auto &f : m_factions) {
            if (f.nickname.compare(nickname, Qt::CaseInsensitive) == 0)
                return &f;
        }
        return nullptr;
    }

private:
    QVector<FactionInfo> m_factions;
};

} // namespace flatlas::domain
