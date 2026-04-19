#include <QtTest>
#include <QJsonDocument>
#include "domain/UniverseData.h"
#include "domain/SolarObject.h"
#include "domain/ZoneItem.h"
#include "domain/BaseData.h"
#include "domain/TradeRoute.h"
#include "domain/NpcData.h"
#include "domain/ModProfile.h"

using namespace flatlas::domain;

class TestDomainModels : public QObject {
    Q_OBJECT
private slots:

    // --- UniverseData ---
    void universeAddAndFind()
    {
        UniverseData ud;
        SystemInfo s;
        s.nickname = QStringLiteral("Li01");
        s.displayName = QStringLiteral("New York");
        s.position = QVector3D(-32000, 0, 0);
        s.idsName = 196612;
        ud.addSystem(s);
        QCOMPARE(ud.systemCount(), 1);
        auto *found = ud.findSystem(QStringLiteral("Li01"));
        QVERIFY(found != nullptr);
        QCOMPARE(found->displayName, QStringLiteral("New York"));
    }

    void universeFindMissing()
    {
        UniverseData ud;
        QVERIFY(ud.findSystem(QStringLiteral("missing")) == nullptr);
        QCOMPARE(ud.systemCount(), 0);
    }

    void universeMultipleSystems()
    {
        UniverseData ud;
        SystemInfo s1;
        s1.nickname = QStringLiteral("Li01");
        ud.addSystem(s1);
        SystemInfo s2;
        s2.nickname = QStringLiteral("Li02");
        ud.addSystem(s2);
        QCOMPARE(ud.systemCount(), 2);
        QVERIFY(ud.findSystem(QStringLiteral("Li02")) != nullptr);
    }

    // --- SolarObject ---
    void solarObjectProperties()
    {
        SolarObject obj;
        obj.setNickname(QStringLiteral("Li01_Planet"));
        obj.setType(SolarObject::Planet);
        obj.setPosition(QVector3D(1000, 2000, 3000));
        obj.setBase(QStringLiteral("Li01_01_Base"));
        obj.setLoadout(QStringLiteral("loadout_01"));
        QCOMPARE(obj.nickname(), QStringLiteral("Li01_Planet"));
        QCOMPARE(obj.type(), SolarObject::Planet);
        QCOMPARE(obj.base(), QStringLiteral("Li01_01_Base"));
    }

    void solarObjectSignalEmitted()
    {
        SolarObject obj;
        QSignalSpy spy(&obj, &SolarObject::changed);
        obj.setNickname(QStringLiteral("test"));
        QCOMPARE(spy.count(), 1);
        obj.setType(SolarObject::Station);
        QCOMPARE(spy.count(), 2);
    }

    // --- ZoneItem ---
    void zoneItemProperties()
    {
        ZoneItem zone;
        zone.setNickname(QStringLiteral("Zone_Li01_destroy"));
        zone.setShape(ZoneItem::Ellipsoid);
        zone.setDamage(100);
        zone.setInterference(0.5f);
        zone.setDragScale(1.2f);
        zone.setSortKey(5);
        zone.setZoneType(QStringLiteral("exclusion"));
        QCOMPARE(zone.damage(), 100);
        QCOMPARE(zone.shape(), ZoneItem::Ellipsoid);
        QCOMPARE(zone.zoneType(), QStringLiteral("exclusion"));
        QVERIFY(qFuzzyCompare(zone.interference(), 0.5f));
    }

    // --- BaseData ---
    void baseDataFields()
    {
        BaseData bd;
        bd.nickname = QStringLiteral("Li01_01_Base");
        bd.system = QStringLiteral("Li01");
        bd.idsName = 196613;
        bd.position = QVector3D(100, 200, 0);
        bd.dockWith = {QStringLiteral("Li01_01"), QStringLiteral("Li01_02")};
        QCOMPARE(bd.dockWith.size(), 2);
        QCOMPARE(bd.system, QStringLiteral("Li01"));
    }

    // --- TradeRoute ---
    void tradeRouteAddEntries()
    {
        TradeRoute tr;
        TradeRouteEntry e;
        e.commodity = QStringLiteral("commodity_gold");
        e.buyBase = QStringLiteral("Li01_01_Base");
        e.sellBase = QStringLiteral("Li02_01_Base");
        e.buyPrice = 500;
        e.sellPrice = 1200;
        e.profit = 700;
        tr.addEntry(e);
        QCOMPARE(tr.entryCount(), 1);
        QCOMPARE(tr.entries()[0].profit, 700);
    }

    void tradeRouteClear()
    {
        TradeRoute tr;
        TradeRouteEntry e;
        e.commodity = QStringLiteral("test");
        tr.addEntry(e);
        tr.addEntry(e);
        QCOMPARE(tr.entryCount(), 2);
        tr.clear();
        QCOMPARE(tr.entryCount(), 0);
    }

    // --- NpcDataStore ---
    void npcDataStoreFactions()
    {
        NpcDataStore store;
        FactionInfo f;
        f.nickname = QStringLiteral("li_n_grp");
        f.displayName = QStringLiteral("Liberty Navy");
        f.idsName = 11000;
        f.rep = 0.9f;
        store.addFaction(f);
        QCOMPARE(store.factionCount(), 1);
        auto *found = store.findFaction(QStringLiteral("li_n_grp"));
        QVERIFY(found != nullptr);
        QCOMPARE(found->displayName, QStringLiteral("Liberty Navy"));
        QVERIFY(store.findFaction(QStringLiteral("missing")) == nullptr);
    }

    // --- ModProfile ---
    void modProfileJsonRoundTrip()
    {
        ModProfile p;
        p.name = QStringLiteral("Crossfire");
        p.gamePath = QStringLiteral("C:/Freelancer");
        p.modPath = QStringLiteral("C:/Crossfire");
        p.isActive = true;
        p.description = QStringLiteral("Crossfire mod");
        QJsonObject json = p.toJson();
        ModProfile p2 = ModProfile::fromJson(json);
        QCOMPARE(p2.name, p.name);
        QCOMPARE(p2.gamePath, p.gamePath);
        QCOMPARE(p2.isActive, true);
        QCOMPARE(p2.description, p.description);
    }
};

QTEST_MAIN(TestDomainModels)
#include "test_DomainModels.moc"
