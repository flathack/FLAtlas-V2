#pragma once
// tools/ShortestPathGenerator.h – Dijkstra über Jump-Connection-Graph

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace flatlas::domain { class UniverseData; }

namespace flatlas::tools {

/// Berechnet kürzeste Pfade zwischen Systemen mittels Dijkstra.
class ShortestPathGenerator
{
public:
    explicit ShortestPathGenerator(const flatlas::domain::UniverseData *universe);

    /// Baut den Adjazenz-Graphen aus den Jump-Connections.
    void buildGraph();

    /// Berechnet den kürzesten Pfad (gewichtet nach Distanz).
    /// Gibt die System-Nicknames von Start bis Ziel zurück (leer wenn unerreichbar).
    QStringList findShortestPath(const QString &fromSystem, const QString &toSystem) const;

    /// Distanz des zuletzt berechneten Pfades (Summe der Kantengewichte).
    double lastPathDistance() const { return m_lastDistance; }

    /// Anzahl der Jumps des zuletzt berechneten Pfades.
    int lastPathJumps() const { return m_lastJumps; }

    /// Gibt die verfügbaren Systemnamen zurück (sortiert).
    QStringList systemNames() const;

private:
    struct Edge {
        QString target;
        double weight;
    };

    const flatlas::domain::UniverseData *m_universe = nullptr;
    QHash<QString, QVector<Edge>> m_adjacency;
    mutable double m_lastDistance = 0.0;
    mutable int m_lastJumps = 0;
};

} // namespace flatlas::tools
