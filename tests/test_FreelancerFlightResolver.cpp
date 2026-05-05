#include <QtTest/QtTest>

#include "infrastructure/freelancer/FreelancerFlightResolver.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

using namespace flatlas::infrastructure;

class TestFreelancerFlightResolver : public QObject {
    Q_OBJECT
private slots:
    void resolvesShipEngineAndCruiseStats();
};

namespace {

void writeText(const QString &path, const QString &text)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(text.toUtf8());
}

} // namespace

void TestFreelancerFlightResolver::resolvesShipEngineAndCruiseStats()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    writeText(dir.filePath(QStringLiteral("DATA/EQUIPMENT/goods.ini")),
              QStringLiteral("[Good]\n"
                             "nickname = li_elite_package\n"
                             "category = ship\n"
                             "ship = li_elite\n"
                             "hull = li_elite_hull\n"
                             "addon = ge_gf1_engine_01, internal, 1\n"));
    writeText(dir.filePath(QStringLiteral("DATA/EQUIPMENT/engine_equip.ini")),
              QStringLiteral("[Engine]\n"
                             "nickname = ge_gf1_engine_01\n"
                             "max_force = 48000\n"
                             "linear_drag = 600\n"
                             "cruise_charge_time = 4.5\n"));
    writeText(dir.filePath(QStringLiteral("DATA/constants.ini")),
              QStringLiteral("[EngineEquipConsts]\n"
                             "CRUISE_SPEED = 320\n"));

    const FreelancerFlightStats stats =
        FreelancerFlightResolver::resolveFlightStats(dir.path(), QStringLiteral("li_elite_package"));

    QCOMPARE(stats.ship.nickname, QStringLiteral("li_elite_package"));
    QCOMPARE(stats.ship.shipArchetype, QStringLiteral("li_elite"));
    QCOMPARE(stats.ship.engineNickname, QStringLiteral("ge_gf1_engine_01"));
    QCOMPARE(stats.engine.nickname, QStringLiteral("ge_gf1_engine_01"));
    QCOMPARE(stats.engine.maxSpeed, 80.0f);
    QCOMPARE(stats.engine.cruiseChargeTime, 4.5f);
    QCOMPARE(stats.cruiseSpeed, 320.0f);
}

QTEST_MAIN(TestFreelancerFlightResolver)
#include "test_FreelancerFlightResolver.moc"
