// test_TradeScoring.cpp – Phase 11 Trade Scoring tests

#include <QtTest/QtTest>
#include "editors/trade/TradeScoring.h"
#include "domain/TradeRoute.h"
#include "domain/UniverseData.h"

#include <algorithm>

using namespace flatlas::editors;
using namespace flatlas::domain;

class TestTradeScoring : public QObject {
    Q_OBJECT
private slots:
    void testProfitCalculation();
    void testJumpsBetween();
    void testFindTopRoutes();
    void testAdvancedRouteMetrics();
    void testTravelUsesTradeLanes();
    void testTravelUsesIntraSystemJump();
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
    TradeJumpRecord gateA;
    gateA.objectNickname = QStringLiteral("gate_a");
    gateA.systemNickname = QStringLiteral("sys_a");
    gateA.targetSystemNickname = QStringLiteral("sys_b");
    gateA.kind = QStringLiteral("gate");
    gateA.position = QVector3D(100.0f, 0.0f, 0.0f);
    workspace.jumps.append(gateA);
    TradeJumpRecord gateB;
    gateB.objectNickname = QStringLiteral("gate_b");
    gateB.systemNickname = QStringLiteral("sys_b");
    gateB.targetSystemNickname = QStringLiteral("sys_a");
    gateB.kind = QStringLiteral("gate");
    gateB.position = QVector3D(200.0f, 0.0f, 0.0f);
    workspace.jumps.append(gateB);
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

void TestTradeScoring::testTravelUsesTradeLanes()
{
    TradeRouteWorkspaceData workspace;
    workspace.cruiseSpeed = 300.0;
    workspace.universe = std::make_shared<UniverseData>();
    workspace.universe->addSystem({"sys_a", "System A", "", QVector3D(0.0f, 0.0f, 0.0f), 0, 0});
    workspace.commodities.append({"commodity_gold", "Gold", 100, 1, 0, 0, {}});
    workspace.bases.append({"base_a", "Base A", "sys_a", "System A", QVector3D(1000.0f, 0.0f, 0.0f)});
    workspace.bases.append({"base_b", "Base B", "sys_a", "System A", QVector3D(11000.0f, 0.0f, 0.0f)});
    TradeLaneRecord lane;
    lane.systemNickname = QStringLiteral("sys_a");
    lane.ringNicknames = {QStringLiteral("tl_1"), QStringLiteral("tl_2")};
    lane.ringPositions = {QVector3D(1000.0f, 0.0f, 0.0f), QVector3D(11000.0f, 0.0f, 0.0f)};
    workspace.tradeLanes.append(lane);
    workspace.prices.append({"base_a", "Base A", "sys_a", "commodity_gold", 100, 1.0, true, false, {}});
    workspace.prices.append({"base_b", "Base B", "sys_a", "commodity_gold", 200, 2.0, false, false, {}});

    TradeScoring scoring;
    scoring.setWorkspaceData(&workspace);
    TradeRouteFilter filter;
    filter.maxResults = 10;
    const auto routes = scoring.calculateRoutes(filter);
    QCOMPARE(routes.size(), 1);
    QVERIFY(routes.first().travelTimeSeconds <= 40);
    QVERIFY(std::any_of(routes.first().segments.begin(), routes.first().segments.end(), [](const TradeRouteSegment &segment) {
        return segment.type == QStringLiteral("trade_lane");
    }));
}

void TestTradeScoring::testTravelUsesIntraSystemJump()
{
    TradeRouteWorkspaceData workspace;
    workspace.cruiseSpeed = 300.0;
    workspace.universe = std::make_shared<UniverseData>();
    workspace.universe->addSystem({"sys_a", "System A", "", QVector3D(0.0f, 0.0f, 0.0f), 0, 0});
    workspace.commodities.append({"commodity_gold", "Gold", 100, 1, 0, 0, {}});
    workspace.bases.append({"base_a", "Base A", "sys_a", "System A", QVector3D(5000.0f, 0.0f, 0.0f)});
    workspace.bases.append({"base_b", "Base B", "sys_a", "System A", QVector3D(105000.0f, 0.0f, 0.0f)});
    TradeJumpRecord jumpA;
    jumpA.objectNickname = QStringLiteral("jump_a");
    jumpA.systemNickname = QStringLiteral("sys_a");
    jumpA.targetSystemNickname = QStringLiteral("sys_a");
    jumpA.targetObjectNickname = QStringLiteral("jump_b");
    jumpA.kind = QStringLiteral("hole");
    jumpA.position = QVector3D(6000.0f, 0.0f, 0.0f);
    workspace.jumps.append(jumpA);
    TradeJumpRecord jumpB;
    jumpB.objectNickname = QStringLiteral("jump_b");
    jumpB.systemNickname = QStringLiteral("sys_a");
    jumpB.targetSystemNickname = QStringLiteral("sys_a");
    jumpB.targetObjectNickname = QStringLiteral("jump_a");
    jumpB.kind = QStringLiteral("hole");
    jumpB.position = QVector3D(104000.0f, 0.0f, 0.0f);
    workspace.jumps.append(jumpB);
    workspace.prices.append({"base_a", "Base A", "sys_a", "commodity_gold", 100, 1.0, true, false, {}});
    workspace.prices.append({"base_b", "Base B", "sys_a", "commodity_gold", 200, 2.0, false, false, {}});

    TradeScoring scoring;
    scoring.setWorkspaceData(&workspace);
    TradeRouteFilter filter;
    filter.maxResults = 10;
    const auto routes = scoring.calculateRoutes(filter);
    QCOMPARE(routes.size(), 1);
    QVERIFY(routes.first().travelTimeSeconds < 100);
    QVERIFY(std::any_of(routes.first().segments.begin(), routes.first().segments.end(), [](const TradeRouteSegment &segment) {
        return segment.type == QStringLiteral("intra_jump");
    }));
}

QTEST_GUILESS_MAIN(TestTradeScoring)
#include "test_TradeScoring.moc"
