// editors/trade/TradeScoring.cpp – Profitable Routen berechnen (Phase 11)

#include "TradeScoring.h"

#include <QQueue>
#include <algorithm>

using namespace flatlas::domain;

namespace flatlas::editors {

void TradeScoring::setMarketData(const QVector<BaseMarketEntry> &entries)
{
    m_entries = entries;
}

void TradeScoring::setUniverseData(const UniverseData *universe)
{
    m_universe = universe;
}

void TradeScoring::setBaseSystemMap(const QHash<QString, QString> &map)
{
    m_baseToSystem = map;
}

// ─── Adjacency / BFS ────────────────────────────────────

QHash<QString, QVector<QString>> TradeScoring::buildAdjacency() const
{
    QHash<QString, QVector<QString>> adj;
    if (!m_universe) return adj;

    for (const auto &conn : m_universe->connections) {
        QString a = conn.fromSystem.toLower();
        QString b = conn.toSystem.toLower();
        adj[a].append(b);
        adj[b].append(a);
    }
    return adj;
}

int TradeScoring::jumpsBetween(const QString &systemA, const QString &systemB) const
{
    if (systemA.compare(systemB, Qt::CaseInsensitive) == 0)
        return 0;

    auto adj = buildAdjacency();
    QString start = systemA.toLower();
    QString end = systemB.toLower();

    // BFS
    QHash<QString, int> dist;
    QQueue<QString> queue;
    dist[start] = 0;
    queue.enqueue(start);

    while (!queue.isEmpty()) {
        QString cur = queue.dequeue();
        int d = dist[cur];

        for (const auto &neighbor : adj.value(cur)) {
            if (dist.contains(neighbor)) continue;
            dist[neighbor] = d + 1;
            if (neighbor == end)
                return d + 1;
            queue.enqueue(neighbor);
        }
    }

    return -1; // unreachable
}

// ─── Profit calculation ──────────────────────────────────

int TradeScoring::profitFor(const QString &commodity,
                             const QString &buyBase, const QString &sellBase) const
{
    int buyPrice = -1;
    int sellPrice = -1;

    for (const auto &e : m_entries) {
        if (e.commodity.compare(commodity, Qt::CaseInsensitive) != 0)
            continue;

        if (e.base.compare(buyBase, Qt::CaseInsensitive) == 0 && e.sells) {
            buyPrice = e.price;
        }
        if (e.base.compare(sellBase, Qt::CaseInsensitive) == 0 && !e.sells) {
            sellPrice = e.price;
        }
    }

    if (buyPrice < 0 || sellPrice < 0)
        return 0;
    return sellPrice - buyPrice;
}

// ─── Top routes ──────────────────────────────────────────

QVector<TradeRouteCandidate> TradeScoring::findTopRoutes(int maxResults) const
{
    // Build sell map: commodity → list of (base, price) that sell
    QHash<QString, QVector<QPair<QString, int>>> sellers; // commodity → [(base, price)]
    // Build buy map: commodity → list of (base, price) that buy (don't sell)
    QHash<QString, QVector<QPair<QString, int>>> buyers;

    for (const auto &e : m_entries) {
        QString key = e.commodity.toLower();
        if (e.sells) {
            sellers[key].append({e.base, e.price});
        } else {
            buyers[key].append({e.base, e.price});
        }
    }

    // Generate all profitable pairs
    QVector<TradeRouteCandidate> candidates;

    for (auto it = sellers.constBegin(); it != sellers.constEnd(); ++it) {
        const QString &commodity = it.key();
        const auto &sellList = it.value();
        auto buyIt = buyers.constFind(commodity);
        if (buyIt == buyers.constEnd()) continue;
        const auto &buyList = buyIt.value();

        for (const auto &[sellBase, sellPrice] : sellList) {
            for (const auto &[buyBase, buyPrice] : buyList) {
                int profit = buyPrice - sellPrice; // buyer pays more than seller asks
                if (profit <= 0) continue;

                TradeRouteCandidate c;
                c.fromBase = sellBase;
                c.toBase = buyBase;
                c.commodity = commodity;
                c.profit = profit;

                // Calculate jumps if we have system mapping
                QString sysFrom = m_baseToSystem.value(sellBase.toLower());
                QString sysTo = m_baseToSystem.value(buyBase.toLower());
                if (!sysFrom.isEmpty() && !sysTo.isEmpty()) {
                    c.jumps = jumpsBetween(sysFrom, sysTo);
                }

                candidates.append(c);
            }
        }
    }

    // Sort by profit descending
    std::sort(candidates.begin(), candidates.end(),
              [](const TradeRouteCandidate &a, const TradeRouteCandidate &b) {
                  return a.profit > b.profit;
              });

    // Return top N
    if (candidates.size() > maxResults)
        candidates.resize(maxResults);

    return candidates;
}

} // namespace flatlas::editors
