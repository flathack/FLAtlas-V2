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

QTEST_GUILESS_MAIN(TestTradeScoring)
#include "test_TradeScoring.moc"
