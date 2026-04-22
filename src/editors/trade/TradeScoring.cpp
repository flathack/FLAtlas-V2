// editors/trade/TradeScoring.cpp – Profitable Routen berechnen (Phase 11)

#include "TradeScoring.h"

#include "tools/ShortestPathGenerator.h"

#include <QObject>
#include <QQueue>
#include <QSet>
#include <algorithm>
#include <cmath>

using namespace flatlas::domain;

namespace flatlas::editors {

namespace {

constexpr double kCruiseSpeed = 300.0;
constexpr int kGateTimeSeconds = 10;
constexpr int kBuyAndLaunchSeconds = 15;
constexpr int kLandAndSellSeconds = 20;
constexpr double kFallbackLegDistance = 35000.0;

QString normalized(const QString &value)
{
    return value.trimmed().toLower();
}

bool hasPosition(const QVector3D &position)
{
    return !qFuzzyIsNull(position.x()) || !qFuzzyIsNull(position.y()) || !qFuzzyIsNull(position.z());
}

double legDistance(const QVector3D &from, const QVector3D &to, bool *usedFallback)
{
    if (hasPosition(from) && hasPosition(to)) {
        if (usedFallback)
            *usedFallback = false;
        return static_cast<double>((to - from).length());
    }
    if (usedFallback)
        *usedFallback = true;
    return kFallbackLegDistance;
}

QString displaySystemName(const UniverseData *universe, const QString &nickname)
{
    if (!universe)
        return nickname;
    if (const auto *system = universe->findSystem(nickname))
        return system->displayName.isEmpty() ? system->nickname : system->displayName;
    return nickname;
}

} // namespace

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

void TradeScoring::setWorkspaceData(const TradeRouteWorkspaceData *workspace)
{
    m_workspace = workspace;
    if (workspace && workspace->universe)
        m_universe = workspace->universe.get();
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
    if (m_workspace) {
        TradeRouteFilter filter;
        filter.maxResults = maxResults;
        return calculateRoutes(filter);
    }

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

QVector<TradeRouteCandidate> TradeScoring::calculateRoutes(const TradeRouteFilter &filter) const
{
    QVector<TradeRouteCandidate> results;
    if (!m_workspace)
        return results;

    const QHash<QString, TradeCommodityRecord> commoditiesByNickname = [this]() {
        QHash<QString, TradeCommodityRecord> map;
        for (const auto &commodity : m_workspace->commodities)
            map.insert(normalized(commodity.nickname), commodity);
        return map;
    }();

    const QHash<QString, TradeBaseRecord> basesByNickname = [this]() {
        QHash<QString, TradeBaseRecord> map;
        for (const auto &base : m_workspace->bases)
            map.insert(normalized(base.nickname), base);
        return map;
    }();

    QHash<QString, QVector<TradePriceRecord>> sourcesByCommodity;
    QHash<QString, QVector<TradePriceRecord>> sinksByCommodity;
    for (const auto &price : m_workspace->prices) {
        const QString commodityKey = normalized(price.commodityNickname);
        if (price.isSource)
            sourcesByCommodity[commodityKey].append(price);
        else
            sinksByCommodity[commodityKey].append(price);
    }

    flatlas::tools::ShortestPathGenerator pathGenerator(m_universe);
    pathGenerator.buildGraph();

    auto jumpKindBetween = [this](const QString &fromSystem, const QString &toSystem) {
        if (!m_universe)
            return QStringLiteral("other");
        for (const auto &connection : m_universe->connections) {
            const bool forward = connection.fromSystem.compare(fromSystem, Qt::CaseInsensitive) == 0
                && connection.toSystem.compare(toSystem, Qt::CaseInsensitive) == 0;
            const bool reverse = connection.fromSystem.compare(toSystem, Qt::CaseInsensitive) == 0
                && connection.toSystem.compare(fromSystem, Qt::CaseInsensitive) == 0;
            if (forward || reverse)
                return connection.kind;
        }
        return QStringLiteral("other");
    };

    auto firstJumpFor = [this](const QString &systemNickname, const QString &targetSystemNickname) {
        for (const auto &jump : m_workspace->jumps) {
            if (jump.systemNickname.compare(systemNickname, Qt::CaseInsensitive) == 0
                && jump.targetSystemNickname.compare(targetSystemNickname, Qt::CaseInsensitive) == 0) {
                return jump;
            }
        }
        return TradeJumpRecord{};
    };

    auto buildSegments = [&](const TradeBaseRecord &buyBase,
                             const TradeBaseRecord &sellBase,
                             const QStringList &systemPath,
                             QStringList *warnings,
                             double *distanceOut,
                             int *secondsOut) {
        QVector<TradeRouteSegment> segments;
        double totalDistance = 0.0;
        int totalSeconds = 0;

        segments.append({QStringLiteral("buy_start"), buyBase.systemNickname, buyBase.systemDisplayName,
                         buyBase.displayName, buyBase.displayName, 0.0, kBuyAndLaunchSeconds});
        totalSeconds += kBuyAndLaunchSeconds;

        if (systemPath.size() <= 1) {
            bool usedFallback = false;
            const double distance = legDistance(buyBase.position, sellBase.position, &usedFallback);
            segments.append({QStringLiteral("open_space"), buyBase.systemNickname, buyBase.systemDisplayName,
                             buyBase.displayName, sellBase.displayName,
                             distance, qRound(distance / kCruiseSpeed)});
            totalDistance += distance;
            totalSeconds += qRound(distance / kCruiseSpeed);
            if (usedFallback)
                warnings->append(QObject::tr("In-system distance was approximated because object positions are incomplete."));
        } else {
            for (int i = 0; i < systemPath.size() - 1; ++i) {
                const QString currentSystem = systemPath.at(i);
                const QString nextSystem = systemPath.at(i + 1);
                const TradeJumpRecord departureJump = firstJumpFor(currentSystem, nextSystem);
                const TradeJumpRecord arrivalJump = firstJumpFor(nextSystem, currentSystem);

                QVector3D fromPosition;
                QString fromLabel;
                if (i == 0) {
                    fromPosition = buyBase.position;
                    fromLabel = buyBase.displayName;
                } else {
                    const TradeJumpRecord previousArrival = firstJumpFor(currentSystem, systemPath.at(i - 1));
                    fromPosition = previousArrival.position;
                    fromLabel = previousArrival.objectNickname;
                }

                bool usedFallback = false;
                const double distance = legDistance(fromPosition, departureJump.position, &usedFallback);
                const int travelSeconds = qRound(distance / kCruiseSpeed);
                segments.append({QStringLiteral("open_space"), currentSystem, displaySystemName(m_universe, currentSystem),
                                 fromLabel,
                                 departureJump.objectNickname.isEmpty() ? nextSystem : departureJump.objectNickname,
                                 distance,
                                 travelSeconds});
                totalDistance += distance;
                totalSeconds += travelSeconds;
                if (usedFallback)
                    warnings->append(QObject::tr("Jump approach distance was approximated in %1.").arg(displaySystemName(m_universe, currentSystem)));

                segments.append({QStringLiteral("jump"), currentSystem, displaySystemName(m_universe, currentSystem),
                                 departureJump.objectNickname.isEmpty() ? currentSystem : departureJump.objectNickname,
                                 arrivalJump.objectNickname.isEmpty() ? nextSystem : arrivalJump.objectNickname,
                                 0.0,
                                 kGateTimeSeconds});
                totalSeconds += kGateTimeSeconds;
            }

            const TradeJumpRecord arrivalJump = firstJumpFor(systemPath.last(), systemPath.at(systemPath.size() - 2));
            bool usedFallback = false;
            const double distance = legDistance(arrivalJump.position, sellBase.position, &usedFallback);
            const int travelSeconds = qRound(distance / kCruiseSpeed);
            segments.append({QStringLiteral("open_space"), sellBase.systemNickname, sellBase.systemDisplayName,
                             arrivalJump.objectNickname.isEmpty() ? sellBase.systemNickname : arrivalJump.objectNickname,
                             sellBase.displayName,
                             distance,
                             travelSeconds});
            totalDistance += distance;
            totalSeconds += travelSeconds;
            if (usedFallback)
                warnings->append(QObject::tr("Final docking approach distance was approximated in %1.").arg(sellBase.systemDisplayName));
        }

        segments.append({QStringLiteral("dock_sell"), sellBase.systemNickname, sellBase.systemDisplayName,
                         sellBase.displayName, sellBase.displayName, 0.0, kLandAndSellSeconds});
        totalSeconds += kLandAndSellSeconds;

        *distanceOut = totalDistance;
        *secondsOut = totalSeconds;
        return segments;
    };

    for (auto commodityIt = sourcesByCommodity.constBegin(); commodityIt != sourcesByCommodity.constEnd(); ++commodityIt) {
        const QString commodityKey = commodityIt.key();
        if (!filter.commodityNickname.isEmpty()
            && commodityKey != normalized(filter.commodityNickname)) {
            continue;
        }

        const auto commodity = commoditiesByNickname.value(commodityKey);
        const auto sinks = sinksByCommodity.value(commodityKey);
        if (sinks.isEmpty())
            continue;

        for (const auto &source : commodityIt.value()) {
            const QString sourceBaseKey = normalized(source.baseNickname);
            if (!basesByNickname.contains(sourceBaseKey))
                continue;

            const auto buyBase = basesByNickname.value(sourceBaseKey);
            for (const auto &sink : sinks) {
                const QString sinkBaseKey = normalized(sink.baseNickname);
                if (sourceBaseKey == sinkBaseKey || !basesByNickname.contains(sinkBaseKey))
                    continue;

                const auto sellBase = basesByNickname.value(sinkBaseKey);
                if (filter.localRoutesOnly
                    && buyBase.systemNickname.compare(sellBase.systemNickname, Qt::CaseInsensitive) != 0) {
                    continue;
                }
                if (!filter.sourceSystemNickname.isEmpty()
                    && buyBase.systemNickname.compare(filter.sourceSystemNickname, Qt::CaseInsensitive) != 0) {
                    continue;
                }
                if (!filter.targetSystemNickname.isEmpty()
                    && sellBase.systemNickname.compare(filter.targetSystemNickname, Qt::CaseInsensitive) != 0) {
                    continue;
                }

                const int unitProfit = sink.price - source.price;
                if (unitProfit < filter.minimumProfit)
                    continue;

                QStringList systemPath = pathGenerator.findShortestPath(buyBase.systemNickname, sellBase.systemNickname);
                if (systemPath.isEmpty()) {
                    if (buyBase.systemNickname.compare(sellBase.systemNickname, Qt::CaseInsensitive) != 0)
                        continue;
                    systemPath = {buyBase.systemNickname};
                }

                const int jumps = qMax(0, systemPath.size() - 1);
                if (jumps > filter.maxJumps)
                    continue;

                QStringList warnings;
                double totalDistance = 0.0;
                int travelSeconds = 0;
                QVector<TradeRouteSegment> segments = buildSegments(buyBase, sellBase, systemPath, &warnings, &totalDistance, &travelSeconds);
                const int cargoUnits = qMax(1, filter.cargoCapacity / qMax(1, commodity.volume));
                const int totalProfit = cargoUnits * unitProfit;
                const double profitPerMinute = travelSeconds > 0
                    ? static_cast<double>(totalProfit) / (static_cast<double>(travelSeconds) / 60.0)
                    : 0.0;
                const double profitPerDistance = totalDistance > 0.0
                    ? static_cast<double>(totalProfit) / totalDistance
                    : 0.0;

                double risk = jumps * 3.0;
                for (int i = 0; i < systemPath.size() - 1; ++i) {
                    const QString kind = jumpKindBetween(systemPath.at(i), systemPath.at(i + 1));
                    if (kind.compare(QStringLiteral("hole"), Qt::CaseInsensitive) == 0)
                        risk += 12.0;
                }
                if (sink.implicit)
                    risk += 8.0;
                if (!warnings.isEmpty())
                    risk += 10.0;
                if (source.multiplier < 0.25 || sink.multiplier > 3.0)
                    warnings.append(QObject::tr("Price multipliers are far away from the commodity base price and may be implausible in-game."));

                const double profitComponent = qMin(100.0, static_cast<double>(totalProfit) / 1000.0);
                const double timeComponent = qMin(100.0, profitPerMinute / 50.0);
                const double distanceComponent = qMin(100.0, profitPerDistance * 10000.0);
                const double score = qBound(0.0,
                                            (profitComponent * 0.35) + (timeComponent * 0.45) + (distanceComponent * 0.20) - risk,
                                            100.0);
                const double legacyScore = qMin(100.0,
                                                static_cast<double>(unitProfit * cargoUnits)
                                                    / static_cast<double>(qMax(1, jumps + 1) * 1000));
                const double plausibilityDelta = std::abs(score - legacyScore);
                if (plausibilityDelta > 30.0)
                    warnings.append(QObject::tr("The time-weighted score deviates strongly from the classic profit-per-jump expectation."));
                if (profitPerMinute < filter.minimumProfitPerMinute)
                    continue;

                const QString combinedSearch = QStringLiteral("%1 %2 %3 %4 %5")
                    .arg(commodity.displayName,
                         buyBase.displayName,
                         sellBase.displayName,
                         buyBase.systemDisplayName,
                         sellBase.systemDisplayName)
                    .toLower();
                if (!filter.searchText.isEmpty() && !combinedSearch.contains(filter.searchText.toLower()))
                    continue;

                TradeRouteCandidate candidate;
                candidate.commodity = commodity.nickname;
                candidate.commodityDisplayName = commodity.displayName;
                candidate.fromBase = buyBase.nickname;
                candidate.toBase = sellBase.nickname;
                candidate.fromSystem = buyBase.systemNickname;
                candidate.toSystem = sellBase.systemNickname;
                candidate.pathSystemNicknames = systemPath;
                for (const auto &systemNickname : systemPath)
                    candidate.pathSystemNames.append(displaySystemName(m_universe, systemNickname));
                candidate.buyPrice = source.price;
                candidate.sellPrice = sink.price;
                candidate.profit = unitProfit;
                candidate.jumps = jumps;
                candidate.cargoUnits = cargoUnits;
                candidate.totalProfit = totalProfit;
                candidate.travelTimeSeconds = travelSeconds;
                candidate.totalDistance = totalDistance;
                candidate.profitPerMinute = profitPerMinute;
                candidate.profitPerDistance = profitPerDistance;
                candidate.riskScore = risk;
                candidate.plausibilityDelta = plausibilityDelta;
                candidate.score = score;
                candidate.warnings = warnings;
                candidate.segments = segments;
                results.append(candidate);
            }
        }
    }

    std::sort(results.begin(), results.end(), [](const TradeRouteCandidate &left, const TradeRouteCandidate &right) {
        if (!qFuzzyCompare(left.score + 1.0, right.score + 1.0))
            return left.score > right.score;
        if (left.totalProfit != right.totalProfit)
            return left.totalProfit > right.totalProfit;
        return left.profitPerMinute > right.profitPerMinute;
    });

    if (results.size() > filter.maxResults)
        results.resize(filter.maxResults);
    return results;
}

} // namespace flatlas::editors
