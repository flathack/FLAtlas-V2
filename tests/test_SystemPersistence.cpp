// tests/test_SystemPersistence.cpp – Phase 5: SystemPersistence roundtrip tests

#include <QTest>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>

#include "editors/system/SystemPersistence.h"
#include "domain/SystemDocument.h"
#include "domain/SolarObject.h"
#include "domain/ZoneItem.h"
#include "infrastructure/parser/IniParser.h"

using namespace flatlas::editors;
using namespace flatlas::domain;
using namespace flatlas::infrastructure;

class TestSystemPersistence : public QObject
{
    Q_OBJECT

private:
    QString writeTemp(const QString &content)
    {
        static int counter = 0;
        QTemporaryDir dir;
        dir.setAutoRemove(false);
        QString path = dir.path() + QStringLiteral("/test_%1.ini").arg(++counter);
        QFile f(path);
        f.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream out(&f);
        out << content;
        return path;
    }

private slots:
    void loadEmpty()
    {
        QString path = writeTemp(QString());
        auto doc = SystemPersistence::load(path);
        QVERIFY(!doc); // empty file → nullptr
    }

    void loadSystemInfo()
    {
        QString ini = QStringLiteral(
            "[SystemInfo]\n"
            "nickname = Li01\n"
        );
        QString path = writeTemp(ini);
        auto doc = SystemPersistence::load(path);
        QVERIFY(doc);
        QCOMPARE(doc->name(), QStringLiteral("Li01"));
        QCOMPARE(doc->objects().size(), 0);
        QCOMPARE(doc->zones().size(), 0);
        QVERIFY(!doc->isDirty());
    }

    void loadObjects()
    {
        QString ini = QStringLiteral(
            "[SystemInfo]\n"
            "nickname = Li01\n"
            "\n"
            "[Object]\n"
            "nickname = Li01_01_Base\n"
            "pos = 100, 200, -300\n"
            "archetype = space_station\n"
            "base = Li01_01_Base\n"
            "ids_name = 12345\n"
            "ids_info = 12346\n"
            "rotate = 10, 20, 30\n"
            "loadout = station_loadout\n"
            "dock_with = Li01_01_Base\n"
            "comment = Test station\n"
            "\n"
            "[Object]\n"
            "nickname = Li01_Jump_Gate\n"
            "pos = 500, 0, 1000\n"
            "archetype = li_jump_gate\n"
            "goto = Li02, Li02_Jump_Gate, li_jump_gate\n"
        );
        QString path = writeTemp(ini);
        auto doc = SystemPersistence::load(path);
        QVERIFY(doc);
        QCOMPARE(doc->objects().size(), 2);

        auto &station = *doc->objects()[0];
        QCOMPARE(station.nickname(), QStringLiteral("Li01_01_Base"));
        QCOMPARE(station.position(), QVector3D(100, 200, -300));
        QCOMPARE(station.archetype(), QStringLiteral("space_station"));
        QCOMPARE(station.base(), QStringLiteral("Li01_01_Base"));
        QCOMPARE(station.type(), SolarObject::Station);
        QCOMPARE(station.idsName(), 12345);
        QCOMPARE(station.idsInfo(), 12346);
        QCOMPARE(station.rotation(), QVector3D(10, 20, 30));
        QCOMPARE(station.loadout(), QStringLiteral("station_loadout"));
        QCOMPARE(station.dockWith(), QStringLiteral("Li01_01_Base"));
        QCOMPARE(station.comment(), QStringLiteral("Test station"));

        auto &gate = *doc->objects()[1];
        QCOMPARE(gate.nickname(), QStringLiteral("Li01_Jump_Gate"));
        QCOMPARE(gate.type(), SolarObject::JumpGate);
        QCOMPARE(gate.gotoTarget(), QStringLiteral("Li02, Li02_Jump_Gate, li_jump_gate"));
    }

    void loadZones()
    {
        QString ini = QStringLiteral(
            "[SystemInfo]\n"
            "nickname = Li01\n"
            "\n"
            "[Zone]\n"
            "nickname = Zone_Li01_trade\n"
            "pos = 1000, 0, 2000\n"
            "size = 5000, 5000, 5000\n"
            "shape = SPHERE\n"
            "zone_type = TRADE\n"
            "comment = trade zone\n"
            "damage = 100\n"
            "interference = 0.5\n"
            "drag_modifier = 1.5\n"
            "sort = 99\n"
        );
        QString path = writeTemp(ini);
        auto doc = SystemPersistence::load(path);
        QVERIFY(doc);
        QCOMPARE(doc->zones().size(), 1);

        auto &zone = *doc->zones()[0];
        QCOMPARE(zone.nickname(), QStringLiteral("Zone_Li01_trade"));
        QCOMPARE(zone.position(), QVector3D(1000, 0, 2000));
        QCOMPARE(zone.size(), QVector3D(5000, 5000, 5000));
        QCOMPARE(zone.shape(), ZoneItem::Sphere);
        QCOMPARE(zone.zoneType(), QStringLiteral("TRADE"));
        QCOMPARE(zone.comment(), QStringLiteral("trade zone"));
        QCOMPARE(zone.damage(), 100);
        QVERIFY(qFuzzyCompare(zone.interference(), 0.5f));
        QVERIFY(qFuzzyCompare(zone.dragScale(), 1.5f));
        QCOMPARE(zone.sortKey(), 99);
    }

    void loadZoneShapes()
    {
        QString ini = QStringLiteral(
            "[SystemInfo]\n"
            "nickname = test\n"
            "[Zone]\nnickname = z1\nshape = ELLIPSOID\n"
            "[Zone]\nnickname = z2\nshape = CYLINDER\n"
            "[Zone]\nnickname = z3\nshape = BOX\n"
            "[Zone]\nnickname = z4\nshape = RING\n"
        );
        auto doc = SystemPersistence::load(writeTemp(ini));
        QVERIFY(doc);
        QCOMPARE(doc->zones().size(), 4);
        QCOMPARE(doc->zones()[0]->shape(), ZoneItem::Ellipsoid);
        QCOMPARE(doc->zones()[1]->shape(), ZoneItem::Cylinder);
        QCOMPARE(doc->zones()[2]->shape(), ZoneItem::Box);
        QCOMPARE(doc->zones()[3]->shape(), ZoneItem::Ring);
    }

    void loadZoneCommentFromLeadingIniComment()
    {
        QString ini = QStringLiteral(
            "[SystemInfo]\n"
            "nickname = test\n"
            "\n"
            "; First line\n"
            "; Second line\n"
            "[Zone]\n"
            "nickname = zone_comment\n"
            "shape = SPHERE\n"
            "size = 1000\n"
        );
        auto doc = SystemPersistence::load(writeTemp(ini));
        QVERIFY(doc);
        QCOMPARE(doc->zones().size(), 1);
        QCOMPARE(doc->zones()[0]->comment(), QStringLiteral("First line\nSecond line"));
    }

    void typeDetection()
    {
        QString ini = QStringLiteral(
            "[SystemInfo]\n"
            "nickname = test\n"
            "[Object]\nnickname = sun1\nstar = orbit_sun\narchetype = sun01\n"
            "[Object]\nnickname = jg1\narchetype = jump_gate_01\ngoto = sys2, jg2, jg\n"
            "[Object]\nnickname = jh1\narchetype = jump_hole_01\ngoto = sys2, jh2, jh\n"
            "[Object]\nnickname = tl1\narchetype = trade_lane_ring\n"
            "[Object]\nnickname = p1\narchetype = planet_earth\n"
            "[Object]\nnickname = sat1\narchetype = satellite_01\n"
            "[Object]\nnickname = wp1\narchetype = waypoint_01\n"
            "[Object]\nnickname = wp2\narchetype = weapons_platform_01\n"
            "[Object]\nnickname = dep1\narchetype = depot_01\n"
            "[Object]\nnickname = dr1\narchetype = docking_ring_01\n"
            "[Object]\nnickname = wr1\narchetype = wreck_01\n"
            "[Object]\nnickname = ast1\narchetype = asteroid_field\n"
        );
        auto doc = SystemPersistence::load(writeTemp(ini));
        QVERIFY(doc);
        QCOMPARE(doc->objects().size(), 12);
        QCOMPARE(doc->objects()[0]->type(), SolarObject::Sun);
        QCOMPARE(doc->objects()[1]->type(), SolarObject::JumpGate);
        QCOMPARE(doc->objects()[2]->type(), SolarObject::JumpHole);
        QCOMPARE(doc->objects()[3]->type(), SolarObject::TradeLane);
        QCOMPARE(doc->objects()[4]->type(), SolarObject::Planet);
        QCOMPARE(doc->objects()[5]->type(), SolarObject::Satellite);
        QCOMPARE(doc->objects()[6]->type(), SolarObject::Waypoint);
        QCOMPARE(doc->objects()[7]->type(), SolarObject::Weapons_Platform);
        QCOMPARE(doc->objects()[8]->type(), SolarObject::Depot);
        QCOMPARE(doc->objects()[9]->type(), SolarObject::DockingRing);
        QCOMPARE(doc->objects()[10]->type(), SolarObject::Wreck);
        QCOMPARE(doc->objects()[11]->type(), SolarObject::Asteroid);
    }

    void saveRoundtrip()
    {
        // Load → Save → Load again → Compare
        QString ini = QStringLiteral(
            "[SystemInfo]\n"
            "nickname = Li01\n"
            "\n"
            "[Object]\n"
            "nickname = station1\n"
            "ids_name = 100\n"
            "pos = 1000, 2000, -3000\n"
            "archetype = space_station\n"
            "base = Li01_01\n"
            "\n"
            "[Zone]\n"
            "nickname = zone1\n"
            "pos = 500, 0, 500\n"
            "size = 1000, 1000, 1000\n"
            "shape = SPHERE\n"
            "zone_type = TRADE\n"
        );
        QString path1 = writeTemp(ini);
        auto doc1 = SystemPersistence::load(path1);
        QVERIFY(doc1);

        // Save to new file
        QTemporaryDir dir;
        QString path2 = dir.path() + QStringLiteral("/roundtrip.ini");
        QVERIFY(SystemPersistence::save(*doc1, path2));

        // Load again
        auto doc2 = SystemPersistence::load(path2);
        QVERIFY(doc2);
        QCOMPARE(doc2->name(), doc1->name());
        QCOMPARE(doc2->objects().size(), doc1->objects().size());
        QCOMPARE(doc2->zones().size(), doc1->zones().size());

        // Verify objects survived roundtrip
        QCOMPARE(doc2->objects()[0]->nickname(), QStringLiteral("station1"));
        QCOMPARE(doc2->objects()[0]->base(), QStringLiteral("Li01_01"));
        QCOMPARE(doc2->objects()[0]->idsName(), 100);

        // Verify zones survived roundtrip
        QCOMPARE(doc2->zones()[0]->nickname(), QStringLiteral("zone1"));
        QCOMPARE(doc2->zones()[0]->shape(), ZoneItem::Sphere);
        QCOMPARE(doc2->zones()[0]->zoneType(), QStringLiteral("TRADE"));
    }

    void extraSectionsPreserved()
    {
        QString ini = QStringLiteral(
            "[SystemInfo]\n"
            "nickname = Li01\n"
            "\n"
            "[LightSource]\n"
            "nickname = Li01_Sun_light\n"
            "color = 255, 255, 200\n"
            "\n"
            "[Ambient]\n"
            "color = 80, 80, 100\n"
            "\n"
            "[Object]\n"
            "nickname = obj1\n"
            "archetype = test\n"
        );
        QString path = writeTemp(ini);
        auto doc = SystemPersistence::load(path);
        QVERIFY(doc);

        // Extra sections should be stored
        auto extras = SystemPersistence::extraSections(doc.get());
        QCOMPARE(extras.size(), 2);
        QCOMPARE(extras[0].name, QStringLiteral("LightSource"));
        QCOMPARE(extras[1].name, QStringLiteral("Ambient"));

        // Save and verify extras are written
        QTemporaryDir dir;
        QString path2 = dir.path() + QStringLiteral("/extras.ini");
        QVERIFY(SystemPersistence::save(*doc, path2));

        QFile f(path2);
        f.open(QIODevice::ReadOnly);
        QString content = QString::fromUtf8(f.readAll());
        QVERIFY(content.contains(QStringLiteral("[LightSource]")));
        QVERIFY(content.contains(QStringLiteral("[Ambient]")));
    }

    void saveZoneCommentAsIniComment()
    {
        auto doc = std::make_unique<SystemDocument>();
        doc->setName(QStringLiteral("Li01"));

        auto zone = std::make_shared<ZoneItem>();
        zone->setNickname(QStringLiteral("zone_comment"));
        zone->setShape(ZoneItem::Sphere);
        zone->setSize(QVector3D(1000, 1000, 1000));
        zone->setComment(QStringLiteral("Devon Field"));
        zone->setRawEntries({
            {QStringLiteral("nickname"), QStringLiteral("zone_comment")},
            {QStringLiteral("shape"), QStringLiteral("SPHERE")},
            {QStringLiteral("size"), QStringLiteral("1000, 1000, 1000")}
        });
        doc->addZone(zone);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.path() + QStringLiteral("/zone_comment.ini");
        QVERIFY(SystemPersistence::save(*doc, path));

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString written = QString::fromUtf8(file.readAll());
        QVERIFY(written.contains(QStringLiteral("; Devon Field\n[Zone]\n")));
        QVERIFY(!written.contains(QStringLiteral("comment = Devon Field")));

        auto reloaded = SystemPersistence::load(path);
        QVERIFY(reloaded);
        QCOMPARE(reloaded->zones().size(), 1);
        QCOMPARE(reloaded->zones()[0]->comment(), QStringLiteral("Devon Field"));
    }

    void saveNewDocument()
    {
        // Create document programmatically, save, then load
        auto doc = std::make_unique<SystemDocument>();
        doc->setName(QStringLiteral("TestSystem"));

        auto obj = std::make_shared<SolarObject>();
        obj->setNickname(QStringLiteral("test_planet"));
        obj->setPosition(QVector3D(100, 200, 300));
        obj->setArchetype(QStringLiteral("planet_earth"));
        doc->addObject(obj);

        QTemporaryDir dir;
        QString path = dir.path() + QStringLiteral("/new_system.ini");
        QVERIFY(SystemPersistence::save(*doc, path));

        auto loaded = SystemPersistence::load(path);
        QVERIFY(loaded);
        QCOMPARE(loaded->name(), QStringLiteral("TestSystem"));
        QCOMPARE(loaded->objects().size(), 1);
        QCOMPARE(loaded->objects()[0]->nickname(), QStringLiteral("test_planet"));
    }
};

QTEST_GUILESS_MAIN(TestSystemPersistence)
#include "test_SystemPersistence.moc"
