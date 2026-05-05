#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace flatlas::infrastructure {

struct FreelancerShipPackage {
    QString nickname;
    QString shipArchetype;
    QString hullGood;
    QString engineNickname;
    QString loadoutNickname;
};

struct FreelancerEngineStats {
    QString nickname;
    float maxForce = 0.0f;
    float linearDrag = 0.0f;
    float maxSpeed = 0.0f;
    float cruiseChargeTime = 5.0f;
};

struct FreelancerFlightStats {
    FreelancerShipPackage ship;
    FreelancerEngineStats engine;
    float cruiseSpeed = 300.0f;
};

class FreelancerFlightResolver
{
public:
    static QVector<FreelancerShipPackage> loadShipPackages(const QString &gameRoot);
    static FreelancerShipPackage resolveShipPackage(const QString &gameRoot, const QString &packageNickname);
    static FreelancerEngineStats resolveEngine(const QString &gameRoot, const QString &engineNickname);
    static float resolveCruiseSpeed(const QString &gameRoot);
    static FreelancerFlightStats resolveFlightStats(const QString &gameRoot, const QString &packageNickname);

private:
    static QString dataPath(const QString &gameRoot, const QString &relativePath);
};

} // namespace flatlas::infrastructure
