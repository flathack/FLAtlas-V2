#pragma once
// domain/UniverseData.h – Universe-Datenmodell
// TODO Phase 2
#include <QString>
#include <QVector>
namespace flatlas::domain {
struct SystemInfo { QString nickname; QString file; float x = 0; float y = 0; int idsName = 0; };
struct JumpConnection { QString fromSystem; QString fromObject; QString toSystem; QString toObject; };
class UniverseData {
public:
    QVector<SystemInfo> systems;
    QVector<JumpConnection> connections;
};
} // namespace flatlas::domain
