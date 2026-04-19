#pragma once
// domain/TradeRoute.h – Trade-Route-Modell

#include <QString>
#include <QVector>

namespace flatlas::domain {

struct Commodity {
    QString nickname;
    QString baseName;
    QString displayName;
    int idsName = 0;
};

struct BaseMarketEntry {
    QString base;
    QString commodity;
    int price = 0;
    bool sells = false;
};

struct TradeRouteEntry {
    QString commodity;
    QString buyBase;
    QString sellBase;
    int buyPrice = 0;
    int sellPrice = 0;
    int profit = 0;
};

struct TradeRouteCandidate {
    QString fromBase;
    QString toBase;
    QString commodity;
    int profit = 0;
    int jumps = 0;
};

class TradeRoute {
public:
    void addEntry(const TradeRouteEntry &entry) { m_entries.append(entry); }
    const QVector<TradeRouteEntry> &entries() const { return m_entries; }
    int entryCount() const { return m_entries.size(); }
    void clear() { m_entries.clear(); }

private:
    QVector<TradeRouteEntry> m_entries;
};

} // namespace flatlas::domain
