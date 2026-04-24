#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVector3D>

#include "domain/SolarObject.h"
#include "domain/SystemDocument.h"
#include "domain/ZoneItem.h"
#include "editors/system/RingEditService.h"

using namespace flatlas::domain;
using namespace flatlas::editors;

class TestRingEditService : public QObject
{
    Q_OBJECT

private slots:
    void loadsRingPresetsFromGameRoot();
    void createsAndRemovesLinkedRingZone();
};

void TestRingEditService::loadsRingPresetsFromGameRoot()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString ringsDir = dir.path() + QStringLiteral("/DATA/SOLAR/RINGS/sub");
    QVERIFY(QDir().mkpath(ringsDir));

    QFile preset(ringsDir + QStringLiteral("/custom_ring.ini"));
    QVERIFY(preset.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&preset);
    out << "[TexturePanels]\nfile = solar\\rings\\shapes.ini\n";
    preset.close();

    const RingEditOptions options = RingEditService::loadOptions(dir.path());
    QVERIFY(options.ringPresets.contains(QStringLiteral("solar\\rings\\sub\\custom_ring.ini")));
}

void TestRingEditService::createsAndRemovesLinkedRingZone()
{
    SystemDocument document;
    auto host = std::make_shared<SolarObject>();
    host->setNickname(QStringLiteral("Test_planet_1"));
    host->setType(SolarObject::Planet);
    host->setArchetype(QStringLiteral("planet_ice_200"));
    host->setPosition(QVector3D(1000.0f, 0.0f, -500.0f));
    host->setRawEntries({
        {QStringLiteral("nickname"), QStringLiteral("Test_planet_1")},
        {QStringLiteral("pos"), QStringLiteral("1000, 0, -500")},
        {QStringLiteral("rotate"), QStringLiteral("0, 0, 0")},
        {QStringLiteral("archetype"), QStringLiteral("planet_ice_200")},
    });
    document.addObject(host);

    RingEditState state;
    state.enabled = true;
    state.ringIni = QStringLiteral("solar\\rings\\ice.ini");
    state.zoneNickname = QStringLiteral("Zone_Test_planet_1_ring");
    state.outerRadius = 3000.0;
    state.innerRadius = 700.0;
    state.thickness = 200.0;
    state.rotateX = -29.0;
    state.rotateY = 16.0;
    state.rotateZ = 3.0;

    QString error;
    QVERIFY(RingEditService::apply(&document, host.get(), state, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(RingEditService::linkedZoneNickname(*host), QStringLiteral("Zone_Test_planet_1_ring"));
    QCOMPARE(document.zones().size(), 1);
    QCOMPARE(document.zones().first()->shape(), ZoneItem::Ring);
    QCOMPARE(document.zones().first()->size(), QVector3D(3000.0f, 700.0f, 200.0f));

    RingEditState removeState = state;
    removeState.enabled = false;
    QVERIFY(RingEditService::apply(&document, host.get(), removeState, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(RingEditService::linkedZoneNickname(*host).isEmpty());
    QCOMPARE(document.zones().size(), 0);
}

QTEST_GUILESS_MAIN(TestRingEditService)
#include "test_RingEditService.moc"