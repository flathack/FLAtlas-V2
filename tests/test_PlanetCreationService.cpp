#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include "editors/system/PlanetCreationService.h"

using namespace flatlas::editors;

class TestPlanetCreationService : public QObject
{
    Q_OBJECT

private slots:
    void loadsPlanetArchetypesFromSolarArch();
    void derivesDefaultRadiiFromPlanetRadius();
};

void TestPlanetCreationService::loadsPlanetArchetypesFromSolarArch()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString solarDir = dir.path() + QStringLiteral("/DATA/SOLAR");
    QVERIFY(QDir().mkpath(solarDir));

    QFile solarArch(solarDir + QStringLiteral("/solararch.ini"));
    QVERIFY(solarArch.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&solarArch);
    out << "[Solar]\n"
           "nickname = planet_earth_3000\n"
           "type = PLANET\n"
           "solar_radius = 3000\n\n"
           "[Solar]\n"
           "nickname = planet_unknown_1500\n"
           "type = PLANET\n\n"
           "[Solar]\n"
           "nickname = sun_1000\n"
           "type = SUN\n"
           "solar_radius = 1000\n";
    solarArch.close();

    const PlanetCreationCatalog catalog = PlanetCreationService::loadCatalog(nullptr, dir.path());
    const QStringList archetypes = catalog.archetypeNicknames();

    QCOMPARE(archetypes.size(), 2);
    QVERIFY(archetypes.contains(QStringLiteral("planet_earth_3000")));
    QVERIFY(archetypes.contains(QStringLiteral("planet_unknown_1500")));
    QCOMPARE(catalog.solarRadiusForArchetype(QStringLiteral("planet_earth_3000")), 3000);
    QCOMPARE(catalog.solarRadiusForArchetype(QStringLiteral("planet_unknown_1500")), 1500);
}

void TestPlanetCreationService::derivesDefaultRadiiFromPlanetRadius()
{
    PlanetCreationCatalog catalog;
    catalog.options.append({QStringLiteral("planet_moonblu_1000"), 1000, {}, {}, 0});

    QCOMPARE(PlanetCreationService::derivePlanetRadius(QStringLiteral("planet_moonblu_1000"), catalog), 1000);
    QCOMPARE(PlanetCreationService::defaultDeathZoneRadius(1000), 1100);
    QCOMPARE(PlanetCreationService::defaultAtmosphereRange(1000), 1200);
    QCOMPARE(PlanetCreationService::defaultDeathZoneRadius(0), 1100);
    QCOMPARE(PlanetCreationService::defaultAtmosphereRange(0), 1200);
}

QTEST_GUILESS_MAIN(TestPlanetCreationService)
#include "test_PlanetCreationService.moc"