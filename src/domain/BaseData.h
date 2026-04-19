#pragma once
// domain/BaseData.h – Basis-Datenmodell (Station, Planet)
// TODO Phase 10
#include <QString>
#include <QVector>
namespace flatlas::domain {
struct RoomData { QString nickname; QString type; };
struct BaseData { QString nickname; QString system; int idsName = 0; QVector<RoomData> rooms; };
} // namespace flatlas::domain
