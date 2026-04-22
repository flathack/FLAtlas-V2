#pragma once
// editors/trade/TradeScoring.h – Profitable Routen berechnen (Phase 11)

#include <QString>
#include <QVector>
#include <QHash>

#include "domain/TradeRoute.h"
#include "domain/UniverseData.h"
#include "TradeRouteDataService.h"

namespace flatlas::editors {

struct TradeRouteFilter {
    QString searchText;
    QString commodityNickname;
    QString sourceSystemNickname;
    QString targetSystemNickname;
    int cargoCapacity = 100;
    int maxJumps = 12;
    int maxResults = 250;
    int minimumProfit = 1;
    double minimumProfitPerMinute = 0.0;
    bool localRoutesOnly = false;
};

class TradeScoring {
public:
    /// Set market data for scoring.
    void setMarketData(const QVector<flatlas::domain::BaseMarketEntry> &entries);

    /// Set universe data for jump distance calculation.
    void setUniverseData(const flatlas::domain::UniverseData *universe);

    /// Set base-to-system mapping (base nickname → system nickname).
    void setBaseSystemMap(const QHash<QString, QString> &map);

    /// Set the full trade workspace for advanced route calculation.
    void setWorkspaceData(const TradeRouteWorkspaceData *workspace);

    /// Calculate top-N most profitable trade routes.
    QVector<flatlas::domain::TradeRouteCandidate> findTopRoutes(int maxResults = 50) const;

    /// Calculate filtered routes with travel time, distance, scoring and warnings.
    QVector<flatlas::domain::TradeRouteCandidate> calculateRoutes(const TradeRouteFilter &filter = {}) const;

    /// Calculate profit for a specific commodity between two bases.
    int profitFor(const QString &commodity,
                  const QString &buyBase, const QString &sellBase) const;

    /// Get the number of jumps between two systems.
    int jumpsBetween(const QString &systemA, const QString &systemB) const;

private:
    QVector<flatlas::domain::BaseMarketEntry> m_entries;
    const flatlas::domain::UniverseData *m_universe = nullptr;
    QHash<QString, QString> m_baseToSystem; // base → system
    const TradeRouteWorkspaceData *m_workspace = nullptr;

    // Build adjacency for BFS
    QHash<QString, QVector<QString>> buildAdjacency() const;
};

} // namespace flatlas::editors
