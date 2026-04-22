// test_TradeScoring.cpp – Phase 11 Trade Scoring tests

#include <QtTest/QtTest>
#include "editors/trade/TradeScoring.h"
#include "domain/TradeRoute.h"
#include "domain/UniverseData.h"

using namespace flatlas::editors;
using namespace flatlas::domain;

class TestTradeScoring : public QObject {
    Q_OBJECT
private slots:
    void testProfitCalculation();
    void testJumpsBetween();
    void testFindTopRoutes();
    void testAdvancedRouteMetrics();
};

void TestTradeScoring::testProfitCalculation()
{
    TradeScoring scoring;

    QVector<BaseMarketEntry> entries;
    entries.append({"base_a", "gold", 100, true});   // sells gold at 100
    entries.append({"base_b", "gold", 300, false});   // buys gold at 300

    scoring.setMarketData(entries);

    QCOMPARE(scoring.profitFor("gold", "base_a", "base_b"), 200);
    QCOMPARE(scoring.profitFor("gold", "base_b", "base_a"), 0); // wrong direction
    QCOMPARE(scoring.profitFor("silver", "base_a", "base_b"), 0); // no data
}

void TestTradeScoring::testJumpsBetween()
{
    UniverseData universe;
    universe.addSystem({"sys_a", "System A", "", {}, 0, 0});
    universe.addSystem({"sys_b", "System B", "", {}, 0, 0});
    universe.addSystem({"sys_c", "System C", "", {}, 0, 0});

    universe.connections.append({"sys_a", "", "sys_b", ""});
    universe.connections.append({"sys_b", "", "sys_c", ""});

    TradeScoring scoring;
    scoring.setUniverseData(&universe);

    QCOMPARE(scoring.jumpsBetween("sys_a", "sys_a"), 0);
    QCOMPARE(scoring.jumpsBetween("sys_a", "sys_b"), 1);
    QCOMPARE(scoring.jumpsBetween("sys_a", "sys_c"), 2);
    QCOMPARE(scoring.jumpsBetween("sys_c", "sys_a"), 2);
    QCOMPARE(scoring.jumpsBetween("sys_a", "unknown"), -1);
}

void TestTradeScoring::testFindTopRoutes()
{
    TradeScoring scoring;

    QVector<BaseMarketEntry> entries;
    entries.append({"base_a", "gold", 100, true});
    entries.append({"base_b", "gold", 500, false});
    entries.append({"base_a", "silver", 50, true});
    entries.append({"base_c", "silver", 120, false});

    scoring.setMarketData(entries);

    auto routes = scoring.findTopRoutes(10);
    QVERIFY(routes.size() >= 2);

    // Most profitable should be gold (400 profit)
    QCOMPARE(routes[0].commodity, QStringLiteral("gold"));
    QCOMPARE(routes[0].profit, 400);

    // Second should be silver (70 profit)
    QCOMPARE(routes[1].commodity, QStringLiteral("silver"));
    QCOMPARE(routes[1].profit, 70);
}

void TestTradeScoring::testAdvancedRouteMetrics()
{
    TradeRouteWorkspaceData workspace;
    workspace.universe = std::make_shared<UniverseData>();
    workspace.universe->addSystem({"sys_a", "System A", "", QVector3D(0.0f, 0.0f, 0.0f), 0, 0});
    workspace.universe->addSystem({"sys_b", "System B", "", QVector3D(1000.0f, 0.0f, 0.0f), 0, 0});
    workspace.universe->connections.append({"sys_a", "gate_a", "sys_b", "gate_b", "gate"});

    workspace.commodities.append({"commodity_gold", "Gold", 100, 5, 0, 0, {}});
    workspace.bases.append({"base_a", "Base A", "sys_a", "System A", QVector3D(0.0f, 0.0f, 0.0f)});
    workspace.bases.append({"base_b", "Base B", "sys_b", "System B", QVector3D(500.0f, 0.0f, 0.0f)});
    workspace.jumps.append({"gate_a", "sys_a", "sys_b", "gate", QVector3D(100.0f, 0.0f, 0.0f)});
    workspace.jumps.append({"gate_b", "sys_b", "sys_a", "gate", QVector3D(200.0f, 0.0f, 0.0f)});
    workspace.prices.append({"base_a", "Base A", "sys_a", "commodity_gold", 100, 1.0, true, false, {}});
    workspace.prices.append({"base_b", "Base B", "sys_b", "commodity_gold", 240, 2.4, false, false, {}});

    TradeScoring scoring;
    scoring.setWorkspaceData(&workspace);

    TradeRouteFilter filter;
    filter.cargoCapacity = 100;
    filter.maxResults = 10;
    const auto routes = scoring.calculateRoutes(filter);
    QCOMPARE(routes.size(), 1);
    QCOMPARE(routes.first().commodityDisplayName, QStringLiteral("Gold"));
    QCOMPARE(routes.first().profit, 140);
    QVERIFY(routes.first().travelTimeSeconds > 0);
    QVERIFY(routes.first().totalDistance > 0.0);
    QVERIFY(routes.first().profitPerMinute > 0.0);
    QVERIFY(routes.first().score > 0.0);
}

QTEST_GUILESS_MAIN(TestTradeScoring)
#include "test_TradeScoring.moc"
