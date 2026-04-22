#pragma once
// domain/TradeRoute.h – Trade-Route-Modell

#include <QString>
#include <QStringList>
#include <QVector>
#include <QVector3D>

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
    double multiplier = 1.0;
    bool implicit = false;
};

struct TradeRouteSegment {
    QString type;
    QString systemNickname;
    QString systemDisplayName;
    QString fromLabel;
    QString toLabel;
    double distance = 0.0;
    int seconds = 0;
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
    QString commodityDisplayName;
    QString fromSystem;
    QString toSystem;
    QStringList pathSystemNicknames;
    QStringList pathSystemNames;
    QVector<TradeRouteSegment> segments;
    QStringList warnings;
    int buyPrice = 0;
    int sellPrice = 0;
    int profit = 0;
    int jumps = 0;
    int cargoUnits = 0;
    int totalProfit = 0;
    int travelTimeSeconds = 0;
    double totalDistance = 0.0;
    double profitPerMinute = 0.0;
    double profitPerDistance = 0.0;
    double riskScore = 0.0;
    double plausibilityDelta = 0.0;
    double score = 0.0;
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
