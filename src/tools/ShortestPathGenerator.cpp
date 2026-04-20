// tools/ShortestPathGenerator.cpp – Dijkstra über Jump-Connection-Graph

#include "ShortestPathGenerator.h"
#include "domain/UniverseData.h"

#include <QMap>
#include <QPair>
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace flatlas::tools {

ShortestPathGenerator::ShortestPathGenerator(const flatlas::domain::UniverseData *universe)
    : m_universe(universe)
{
    if (m_universe)
        buildGraph();
}

void ShortestPathGenerator::buildGraph()
{
    m_adjacency.clear();
    if (!m_universe)
        return;

    // Positionen sammeln
    QHash<QString, QVector3D> positions;
    for (const auto &sys : m_universe->systems)
        positions.insert(sys.nickname.toLower(), sys.position);

    // Kanten aus Jump-Connections erzeugen (bidirektional)
    for (const auto &conn : m_universe->connections) {
        const QString a = conn.fromSystem.toLower();
        const QString b = conn.toSystem.toLower();

        double weight = 1.0; // Default: Hop-Gewicht
        if (positions.contains(a) && positions.contains(b)) {
            const QVector3D diff = positions.value(a) - positions.value(b);
            weight = static_cast<double>(diff.length());
            if (weight < 1.0)
                weight = 1.0; // Mindestgewicht
        }

        m_adjacency[a].append({b, weight});
        m_adjacency[b].append({a, weight});
    }
}

QStringList ShortestPathGenerator::findShortestPath(const QString &fromSystem,
                                                     const QString &toSystem) const
{
    m_lastDistance = 0.0;
    m_lastJumps = 0;

    const QString start = fromSystem.toLower();
    const QString goal = toSystem.toLower();

    if (start == goal)
        return {fromSystem};

    if (!m_adjacency.contains(start) || !m_adjacency.contains(goal))
        return {};

    // Dijkstra
    QHash<QString, double> dist;
    QHash<QString, QString> prev;
    using PQItem = QPair<double, QString>;

    // Min-Heap
    std::priority_queue<PQItem, std::vector<PQItem>, std::greater<PQItem>> pq;

    dist[start] = 0.0;
    pq.push({0.0, start});

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();

        if (d > dist.value(u, std::numeric_limits<double>::max()))
            continue;

        if (u == goal)
            break;

        for (const auto &edge : m_adjacency.value(u)) {
            const double newDist = d + edge.weight;
            if (newDist < dist.value(edge.target, std::numeric_limits<double>::max())) {
                dist[edge.target] = newDist;
                prev[edge.target] = u;
                pq.push({newDist, edge.target});
            }
        }
    }

    // Pfad rekonstruieren
    if (!prev.contains(goal))
        return {};

    QStringList path;
    QString current = goal;
    while (!current.isEmpty()) {
        path.prepend(current);
        current = prev.value(current);
    }

    m_lastDistance = dist.value(goal, 0.0);
    m_lastJumps = path.size() - 1;

    return path;
}

QStringList ShortestPathGenerator::systemNames() const
{
    QStringList names = m_adjacency.keys();
    names.sort();
    return names;
}

} // namespace flatlas::tools
