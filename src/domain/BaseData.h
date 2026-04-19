#pragma once
// domain/BaseData.h – Basis-Datenmodell (Station, Planet)

#include <QString>
#include <QStringList>
#include <QVector>
#include <QVector3D>

namespace flatlas::domain {

struct RoomData {
    QString nickname;
    QString type;
};

struct BaseData {
    QString nickname;
    QString system;
    QString archetype;
    int idsName = 0;
    int idsInfo = 0;
    QVector3D position;
    QStringList dockWith;
    QVector<RoomData> rooms;
};

} // namespace flatlas::domain
