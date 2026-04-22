#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "editors/trade/TradeRouteDataService.h"

using namespace flatlas::editors;

class TestTradeRouteDataService : public QObject {
    Q_OBJECT
private slots:
    void testLoadWorkspace();
    void testSaveWorkspace();
};

namespace {

void writeTextFile(const QString &filePath, const QString &content)
{
    QFile file(filePath);
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate), qPrintable(filePath));
    file.write(content.toUtf8());
}

QString createDataTree(QTemporaryDir &tempDir)
{
    const QString dataPath = tempDir.filePath(QStringLiteral("DATA"));
    if (!QDir().mkpath(tempDir.filePath(QStringLiteral("DATA/EQUIPMENT"))))
        return {};
    if (!QDir().mkpath(tempDir.filePath(QStringLiteral("DATA/UNIVERSE/SYSTEMS/SYS_A"))))
        return {};
    if (!QDir().mkpath(tempDir.filePath(QStringLiteral("DATA/UNIVERSE/SYSTEMS/SYS_B"))))
        return {};

    writeTextFile(tempDir.filePath(QStringLiteral("DATA/EQUIPMENT/goods.ini")),
                  QStringLiteral("[Good]\n"
                                 "nickname = commodity_gold\n"
                                 "category = commodity\n"
                                 "price = 100\n"
                                 "volume = 2\n\n"
                                 "[Good]\n"
                                 "nickname = commodity_silver\n"
                                 "category = commodity\n"
                                 "price = 50\n"
                                 "volume = 1\n"));

    writeTextFile(tempDir.filePath(QStringLiteral("DATA/EQUIPMENT/market_commodities.ini")),
                  QStringLiteral("[BaseGood]\n"
                                 "base = base_a\n"
                                 "MarketGood = commodity_gold, 0, 0, 0, 0, 0, 1.0\n\n"
                                 "[BaseGood]\n"
                                 "base = base_b\n"
                                 "MarketGood = commodity_gold, 0, 0, 0, 0, 1, 2.5\n"));

    writeTextFile(tempDir.filePath(QStringLiteral("DATA/UNIVERSE/universe.ini")),
                  QStringLiteral("[System]\n"
                                 "nickname = sys_a\n"
                                 "file = systems/sys_a/sys_a.ini\n"
                                 "pos = 0, 0\n\n"
                                 "[System]\n"
                                 "nickname = sys_b\n"
                                 "file = systems/sys_b/sys_b.ini\n"
                                 "pos = 1000, 0\n"));

    writeTextFile(tempDir.filePath(QStringLiteral("DATA/UNIVERSE/SYSTEMS/SYS_A/sys_a.ini")),
                  QStringLiteral("[Object]\n"
                                 "nickname = base_a_obj\n"
                                 "base = base_a\n"
                                 "pos = 0, 0, 0\n\n"
                                 "[Object]\n"
                                 "nickname = sys_a_to_sys_b_gate\n"
                                 "goto = sys_b, sys_b_to_sys_a_gate, tunnel\n"
                                 "archetype = jumpgate\n"
                                 "pos = 100, 0, 0\n"));

    writeTextFile(tempDir.filePath(QStringLiteral("DATA/UNIVERSE/SYSTEMS/SYS_B/sys_b.ini")),
                  QStringLiteral("[Object]\n"
                                 "nickname = base_b_obj\n"
                                 "base = base_b\n"
                                 "pos = 0, 0, 0\n\n"
                                 "[Object]\n"
                                 "nickname = sys_b_to_sys_a_gate\n"
                                 "goto = sys_a, sys_a_to_sys_b_gate, tunnel\n"
                                 "archetype = jumpgate\n"
                                 "pos = 120, 0, 0\n"));
    return dataPath;
}

} // namespace

void TestTradeRouteDataService::testLoadWorkspace()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dataPath = createDataTree(tempDir);
    QVERIFY(!dataPath.isEmpty());

    const TradeRouteWorkspaceData workspace = TradeRouteDataService::loadFromDataPath(dataPath);
    QCOMPARE(workspace.commodities.size(), 2);
    QCOMPARE(workspace.bases.size(), 2);
    QVERIFY(workspace.universe);
    QCOMPARE(workspace.universe->connections.size(), 1);

    const auto goldIt = std::find_if(workspace.commodities.begin(), workspace.commodities.end(), [](const TradeCommodityRecord &commodity) {
        return commodity.nickname == QStringLiteral("commodity_gold");
    });
    QVERIFY(goldIt != workspace.commodities.end());
    QCOMPARE(goldIt->basePrice, 100);

    const auto explicitPriceCount = std::count_if(workspace.prices.begin(), workspace.prices.end(), [](const TradePriceRecord &price) {
        return !price.implicit;
    });
    QCOMPARE(explicitPriceCount, 2);
}

void TestTradeRouteDataService::testSaveWorkspace()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dataPath = createDataTree(tempDir);
    QVERIFY(!dataPath.isEmpty());

    TradeRouteWorkspaceData workspace = TradeRouteDataService::loadFromDataPath(dataPath);
    QVERIFY(!workspace.commodities.isEmpty());
    workspace.commodities[0].basePrice = 175;
    for (auto &price : workspace.prices) {
        if (!price.implicit && price.commodityNickname == QStringLiteral("commodity_gold")
            && price.baseNickname == QStringLiteral("base_b")) {
            price.price = 700;
            price.multiplier = 4.0;
        }
    }

    QString errorMessage;
    QVERIFY2(TradeRouteDataService::saveWorkspace(workspace, &errorMessage), qPrintable(errorMessage));

    const TradeRouteWorkspaceData reloaded = TradeRouteDataService::loadFromDataPath(dataPath);
    const auto goldIt = std::find_if(reloaded.commodities.begin(), reloaded.commodities.end(), [](const TradeCommodityRecord &commodity) {
        return commodity.nickname == QStringLiteral("commodity_gold");
    });
    QVERIFY(goldIt != reloaded.commodities.end());
    QCOMPARE(goldIt->basePrice, 175);

    const auto priceIt = std::find_if(reloaded.prices.begin(), reloaded.prices.end(), [](const TradePriceRecord &price) {
        return !price.implicit
            && price.commodityNickname == QStringLiteral("commodity_gold")
            && price.baseNickname == QStringLiteral("base_b");
    });
    QVERIFY(priceIt != reloaded.prices.end());
    QCOMPARE(priceIt->price, 700);
}

QTEST_GUILESS_MAIN(TestTradeRouteDataService)
#include "test_TradeRouteDataService.moc"