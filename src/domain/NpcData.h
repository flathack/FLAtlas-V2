#pragma once
// domain/NpcData.h – NPC-Datenmodell
// TODO Phase 14
#include <QString>
#include <QVector>
namespace flatlas::domain {
struct NpcData { QString nickname; QString room; QString faction; float density = 1.0f; };
} // namespace flatlas::domain
