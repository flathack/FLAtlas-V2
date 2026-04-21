#pragma once
// domain/UniverseData.h – Universe-Datenmodell

#include <QString>
#include <QPair>
#include <QVector>
#include <QVector3D>
#include <QHash>
#include <QPointF>
#include <QStringList>

namespace flatlas::domain {

struct SystemInfo {
    QString nickname;
    QString displayName;
    QString filePath;
    QVector3D position;
    int visit = 0;
    int idsName = 0;
    int stridName = 0;
    int idsInfo = 0;
    double navMapScale = 1.36;
    QVector<QPair<QString, QString>> rawEntries;
    QHash<QString, QPointF> sectorPositions;
};

struct JumpConnection {
    QString fromSystem;
    QString fromObject;
    QString toSystem;
    QString toObject;
    QString kind = QStringLiteral("other");   // "gate", "hole", or "other"
};

struct SectorDefinition {
    QString key;
    QString displayName;
    QString sourceMap = QStringLiteral("universe");
    QStringList labelIds;
};

class UniverseData {
public:
    void addSystem(const SystemInfo &info);
    const SystemInfo *findSystem(const QString &nickname) const;
    SystemInfo *findSystem(const QString &nickname);
    int systemCount() const;

    QVector<SystemInfo> systems;
    QVector<JumpConnection> connections;
    QVector<SectorDefinition> sectors;
    bool multiverseDetected = false;
};

} // namespace flatlas::domain
