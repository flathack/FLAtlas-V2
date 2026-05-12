#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <algorithm>

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

    writeTextFile(tempDir.filePath(QStringLiteral("DATA/constants.ini")),
                  QStringLiteral("[Constants]\n"
                                 "CRUISE_SPEED = 330\n"));

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
                                 "volume = 1\n\n"
                                 "[Good]\n"
                                 "nickname = li_gun01_mark01\n"
                                 "category = equipment\n"
                                 "price = 500\n"));

    writeTextFile(tempDir.filePath(QStringLiteral("DATA/EQUIPMENT/select_equip.ini")),
                  QStringLiteral("; gold commodity comment\n"
                                 "[Commodity]\n"
                                 "nickname = commodity_gold\n"
                                 "ids_name = 261900\n"
                                 "; keep this comment inside commodity_gold\n"
                                 "ids_info = 65900\n"
                                 "units_per_container = 30\n"
                                 "pod_appearance = cargopod_grey\n"
                                 "loot_appearance = lootcrate_grey\n"
                                 "decay_per_second = 0\n"
                                 "volume = 2\n"
                                 "hit_pts = 250\n\n"
                                 "[Commodity]\n"
                                 "nickname = commodity_silver\n"
                                 "ids_name = 261901\n"
                                 "ids_info = 65901\n"
                                 "units_per_container = 30\n"
                                 "pod_appearance = cargopod_grey\n"
                                 "loot_appearance = lootcrate_grey\n"
                                 "decay_per_second = 0\n"
                                 "volume = 1\n"
                                 "hit_pts = 250\n\n"
                                 "[Munition]\n"
                                 "nickname = li_gun01_mark01_ammo\n"
                                 "hp_type = hp_gun\n"));

    writeTextFile(tempDir.filePath(QStringLiteral("DATA/EQUIPMENT/market_commodities.ini")),
                  QStringLiteral("[BaseGood]\n"
                                 "base = base_a\n"
                                 "MarketGood = commodity_gold, 0, 0, 0, 0, 0, 1.0\n"
                                 "MarketGood = commodity_silver, 0, 0, 0, 0, 1, 1.0\n\n"
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
                                 "nickname = sys_a_docking_fixture_1\n"
                                 "archetype = docking_fixture\n"
                                 "base = base_a\n"
                                 "pos = 20, 0, 0\n\n"
                                 "[Object]\n"
                                 "nickname = sys_a_to_sys_b_gate\n"
                                 "goto = sys_b, sys_b_to_sys_a_gate, tunnel\n"
                                 "archetype = jumpgate\n"
                                 "pos = 100, 0, 0\n\n"
                                 "[Object]\n"
                                 "nickname = sys_a_trade_lane_ring_1\n"
                                 "archetype = trade_lane_ring\n"
                                 "next_ring = sys_a_trade_lane_ring_2\n"
                                 "pos = 10, 0, 0\n\n"
                                 "[Object]\n"
                                 "nickname = sys_a_trade_lane_ring_2\n"
                                 "archetype = trade_lane_ring\n"
                                 "prev_ring = sys_a_trade_lane_ring_1\n"
                                 "pos = 100, 0, 0\n\n"
                                 "[Object]\n"
                                 "nickname = sys_a_local_hole_1\n"
                                 "goto = sys_a, sys_a_local_hole_2, tunnel\n"
                                 "archetype = jumphole\n"
                                 "pos = 25, 0, 0\n\n"
                                 "[Object]\n"
                                 "nickname = sys_a_local_hole_2\n"
                                 "goto = sys_a, sys_a_local_hole_1, tunnel\n"
                                 "archetype = jumphole\n"
                                 "pos = 85, 0, 0\n"));

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
    QCOMPARE(workspace.cruiseSpeed, 330.0);
    QCOMPARE(workspace.tradeLanes.size(), 1);
    QVERIFY(workspace.universe);
    QCOMPARE(workspace.universe->connections.size(), 1);
    QVERIFY(std::any_of(workspace.jumps.begin(), workspace.jumps.end(), [](const TradeJumpRecord &jump) {
        return jump.systemNickname == QStringLiteral("sys_a")
            && jump.targetSystemNickname == QStringLiteral("sys_a")
            && jump.targetObjectNickname == QStringLiteral("sys_a_local_hole_2");
    }));

    const auto baseAIt = std::find_if(workspace.bases.begin(), workspace.bases.end(), [](const TradeBaseRecord &base) {
        return base.nickname == QStringLiteral("base_a");
    });
    QVERIFY(baseAIt != workspace.bases.end());
    QCOMPARE(baseAIt->displayName, QStringLiteral("base_a_obj"));

    const auto goldIt = std::find_if(workspace.commodities.begin(), workspace.commodities.end(), [](const TradeCommodityRecord &commodity) {
        return commodity.nickname == QStringLiteral("commodity_gold");
    });
    QVERIFY(goldIt != workspace.commodities.end());
    QCOMPARE(goldIt->basePrice, 100);
    QCOMPARE(goldIt->idsName, 261900);
    QCOMPARE(goldIt->idsInfo, 65900);

    const auto explicitPriceCount = std::count_if(workspace.prices.begin(), workspace.prices.end(), [](const TradePriceRecord &price) {
        return !price.implicit;
    });
    QCOMPARE(explicitPriceCount, 3);
}

void TestTradeRouteDataService::testSaveWorkspace()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dataPath = createDataTree(tempDir);
    QVERIFY(!dataPath.isEmpty());

    const QString selectEquipPath = tempDir.filePath(QStringLiteral("DATA/EQUIPMENT/select_equip.ini"));
    QFile pollutedSelectFile(selectEquipPath);
    QVERIFY(pollutedSelectFile.open(QIODevice::ReadOnly));
    QByteArray pollutedSelectText = pollutedSelectFile.readAll();
    pollutedSelectFile.close();
    pollutedSelectText.replace("\r\n", "\n");
    pollutedSelectText.replace("\n", "\r\r\n");
    QVERIFY(pollutedSelectFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(pollutedSelectFile.write(pollutedSelectText), pollutedSelectText.size());
    pollutedSelectFile.close();

    TradeRouteWorkspaceData workspace = TradeRouteDataService::loadFromDataPath(dataPath);
    QVERIFY(!workspace.commodities.isEmpty());
    workspace.commodities[0].basePrice = 175;
    TradeCommodityRecord newCommodity;
    newCommodity.nickname = QStringLiteral("commodity_platinum");
    newCommodity.displayName = QStringLiteral("Platinum");
    newCommodity.basePrice = 250;
    newCommodity.volume = 1;
    newCommodity.idsName = 261902;
    newCommodity.idsInfo = 65902;
    newCommodity.equipment = QStringLiteral("commodity_platinum");
    newCommodity.msgIdPrefix = QStringLiteral("gcs_gen_commodity_platinum");
    workspace.commodities.append(newCommodity);
    for (auto &price : workspace.prices) {
        if (!price.implicit && price.commodityNickname == QStringLiteral("commodity_gold")
            && price.baseNickname == QStringLiteral("base_b")) {
            price.price = 700;
            price.multiplier = 4.0;
        }
    }

    QString errorMessage;
    QVERIFY2(TradeRouteDataService::saveWorkspace(workspace, &errorMessage), qPrintable(errorMessage));
    QVERIFY2(TradeRouteDataService::saveWorkspace(workspace, &errorMessage), qPrintable(errorMessage));

    const TradeRouteWorkspaceData reloaded = TradeRouteDataService::loadFromDataPath(dataPath);
    const auto goldIt = std::find_if(reloaded.commodities.begin(), reloaded.commodities.end(), [](const TradeCommodityRecord &commodity) {
        return commodity.nickname == QStringLiteral("commodity_gold");
    });
    QVERIFY(goldIt != reloaded.commodities.end());
    QCOMPARE(goldIt->basePrice, 175);
    QCOMPARE(goldIt->idsName, 261900);

    QFile goodsFile(tempDir.filePath(QStringLiteral("DATA/EQUIPMENT/goods.ini")));
    QVERIFY(goodsFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString goodsText = QString::fromUtf8(goodsFile.readAll());
    QVERIFY(!goodsText.contains(QStringLiteral("ids_name = 261900")));
    QVERIFY(goodsText.indexOf(QStringLiteral("nickname = commodity_platinum"))
            < goodsText.indexOf(QStringLiteral("nickname = li_gun01_mark01")));

    QFile marketFile(tempDir.filePath(QStringLiteral("DATA/EQUIPMENT/market_commodities.ini")));
    QVERIFY(marketFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString marketText = QString::fromUtf8(marketFile.readAll());
    QVERIFY(marketText.contains(QStringLiteral("MarketGood = commodity_gold, 0, 0, 0, 0, 0, 1.000000")));
    QVERIFY(marketText.contains(QStringLiteral("MarketGood = commodity_gold, 0, 0, 0, 0, 1, 4.000000")));
    QVERIFY(!marketText.contains(QStringLiteral("MarketGood = commodity_silver")));

    QFile selectFile(tempDir.filePath(QStringLiteral("DATA/EQUIPMENT/select_equip.ini")));
    QVERIFY(selectFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString selectText = QString::fromUtf8(selectFile.readAll());
    QVERIFY(selectText.contains(QStringLiteral("[Commodity]")));
    QVERIFY(selectText.contains(QStringLiteral("; gold commodity comment")));
    QVERIFY(selectText.contains(QStringLiteral("; keep this comment inside commodity_gold")));
    QVERIFY(selectText.contains(QStringLiteral("nickname = commodity_gold")));
    QVERIFY(selectText.contains(QStringLiteral("ids_name = 261900")));
    QVERIFY(selectText.contains(QStringLiteral("nickname = commodity_platinum")));
    QVERIFY(selectText.indexOf(QStringLiteral("nickname = commodity_platinum"))
            < selectText.indexOf(QStringLiteral("[Munition]")));
    QVERIFY(!selectText.contains(QStringLiteral("\n\n\n")));

    selectFile.close();
    QVERIFY(selectFile.open(QIODevice::ReadOnly));
    const QByteArray rawSelectText = selectFile.readAll();
    QVERIFY(!rawSelectText.contains("\r\r\n"));

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
