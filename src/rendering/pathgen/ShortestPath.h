#pragma once
// rendering/pathgen/ShortestPath.h – Dijkstra über Jump-Connections
// TODO Phase 24
#include <QString>
#include <QStringList>
namespace flatlas::domain { struct UniverseData; }
namespace flatlas::rendering {
class ShortestPath {
public:
    static QStringList find(const flatlas::domain::UniverseData &universe,
                            const QString &fromSystem, const QString &toSystem);
};
} // namespace flatlas::rendering
