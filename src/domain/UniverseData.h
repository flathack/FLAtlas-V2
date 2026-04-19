#pragma once
// domain/UniverseData.h – Universe-Datenmodell

#include <QString>
#include <QVector>
#include <QVector3D>

namespace flatlas::domain {

struct SystemInfo {
    QString nickname;
    QString displayName;
    QString filePath;
    QVector3D position;
    int idsName = 0;
    int stridName = 0;
};

struct JumpConnection {
    QString fromSystem;
    QString fromObject;
    QString toSystem;
    QString toObject;
};

class UniverseData {
public:
    void addSystem(const SystemInfo &info);
    const SystemInfo *findSystem(const QString &nickname) const;
    int systemCount() const;

    QVector<SystemInfo> systems;
    QVector<JumpConnection> connections;
};

} // namespace flatlas::domain
