#include <QtTest>

#include "domain/SolarObject.h"
#include "domain/SystemDocument.h"
#include "editors/system/TradeLaneEditService.h"

using namespace flatlas::domain;
using namespace flatlas::editors;

namespace {

std::shared_ptr<SolarObject> makeRing(const QString &nickname,
                                      const QVector3D &position,
                                      const QString &prevRing,
                                      const QString &nextRing,
                                      const QString &loadout,
                                      int idsName,
                                      int endpointIds = 0)
{
    auto ring = std::make_shared<SolarObject>();
    ring->setNickname(nickname);
    ring->setType(SolarObject::TradeLane);
    ring->setArchetype(QStringLiteral("Trade_Lane_Ring"));
    ring->setPosition(position);
    ring->setRotation(QVector3D(0.0f, 180.0f, 0.0f));
    ring->setIdsName(idsName);
    ring->setIdsInfo(66170);
    ring->setLoadout(loadout);

    QVector<QPair<QString, QString>> entries{
        {QStringLiteral("nickname"), nickname},
        {QStringLiteral("pos"), QStringLiteral("%1, 0, %2").arg(position.x()).arg(position.z())},
        {QStringLiteral("rotate"), QStringLiteral("0, 180, 0")},
        {QStringLiteral("archetype"), QStringLiteral("Trade_Lane_Ring")},
        {QStringLiteral("ids_name"), QString::number(idsName)},
        {QStringLiteral("ids_info"), QStringLiteral("66170")},
        {QStringLiteral("behavior"), QStringLiteral("NOTHING")},
        {QStringLiteral("difficulty_level"), QStringLiteral("1")},
        {QStringLiteral("loadout"), loadout},
        {QStringLiteral("pilot"), QStringLiteral("pilot_solar_easiest")},
    };
    if (!prevRing.isEmpty())
        entries.append({QStringLiteral("prev_ring"), prevRing});
    if (!nextRing.isEmpty())
        entries.append({QStringLiteral("next_ring"), nextRing});
    if (endpointIds > 0)
        entries.append({QStringLiteral("tradelane_space_name"), QString::number(endpointIds)});
    ring->setRawEntries(entries);
    return ring;
}

} // namespace

class TestTradeLaneEditService : public QObject {
    Q_OBJECT
private slots:
    void detectsOrderedChain();
    void detectsRepairableMissingRing();
    void rebuildsLaneConsistently();
};

void TestTradeLaneEditService::detectsOrderedChain()
{
    SystemDocument document;
    document.setFilePath(QStringLiteral("C:/Tmp/Rh05.ini"));
    document.addObject(makeRing(QStringLiteral("Rh05_Trade_Lane_Ring_1"),
                                QVector3D(0.0f, 0.0f, 0.0f),
                                QString(),
                                QStringLiteral("Rh05_Trade_Lane_Ring_2"),
                                QStringLiteral("trade_lane_ring_rh_01"),
                                123,
                                1001));
    document.addObject(makeRing(QStringLiteral("Rh05_Trade_Lane_Ring_2"),
                                QVector3D(5000.0f, 0.0f, 0.0f),
                                QStringLiteral("Rh05_Trade_Lane_Ring_1"),
                                QStringLiteral("Rh05_Trade_Lane_Ring_3"),
                                QStringLiteral("trade_lane_ring_rh_01"),
                                123));
    document.addObject(makeRing(QStringLiteral("Rh05_Trade_Lane_Ring_3"),
                                QVector3D(10000.0f, 0.0f, 0.0f),
                                QStringLiteral("Rh05_Trade_Lane_Ring_2"),
                                QString(),
                                QStringLiteral("trade_lane_ring_rh_01"),
                                123,
                                1002));

    QString errorMessage;
    const auto chainOpt = TradeLaneEditService::detectChain(document,
                                                            QStringLiteral("Rh05_Trade_Lane_Ring_2"),
                                                            &errorMessage);
    QVERIFY2(chainOpt.has_value(), qPrintable(errorMessage));

    const TradeLaneChain chain = *chainOpt;
    QCOMPARE(chain.rings.size(), 3);
    QCOMPARE(chain.systemNickname, QStringLiteral("Rh05"));
    QCOMPARE(chain.startNumber, 1);
    QCOMPARE(chain.routeIdsName, 123);
    QCOMPARE(chain.startSpaceNameId, 1001);
    QCOMPARE(chain.endSpaceNameId, 1002);
    QCOMPARE(chain.loadout, QStringLiteral("trade_lane_ring_rh_01"));
    QCOMPARE(TradeLaneEditService::memberNicknames(chain),
             QStringList({QStringLiteral("Rh05_Trade_Lane_Ring_1"),
                          QStringLiteral("Rh05_Trade_Lane_Ring_2"),
                          QStringLiteral("Rh05_Trade_Lane_Ring_3")}));
}

void TestTradeLaneEditService::detectsRepairableMissingRing()
{
    SystemDocument document;
    document.setFilePath(QStringLiteral("C:/Tmp/Hi01.ini"));
    document.addObject(makeRing(QStringLiteral("Hi01_Trade_Lane_Ring_5"),
                                QVector3D(0.0f, 0.0f, 0.0f),
                                QString(),
                                QStringLiteral("Hi01_Trade_Lane_Ring_6"),
                                QStringLiteral("trade_lane_ring_li_01"),
                                456,
                                2001));
    document.addObject(makeRing(QStringLiteral("Hi01_Trade_Lane_Ring_6"),
                                QVector3D(5000.0f, 0.0f, 0.0f),
                                QStringLiteral("Hi01_Trade_Lane_Ring_5"),
                                QStringLiteral("Hi01_Trade_Lane_Ring_7"),
                                QStringLiteral("trade_lane_ring_li_01"),
                                456));
    document.addObject(makeRing(QStringLiteral("Hi01_Trade_Lane_Ring_7"),
                                QVector3D(10000.0f, 0.0f, 0.0f),
                                QStringLiteral("Hi01_Trade_Lane_Ring_6"),
                                QStringLiteral("Hi01_Trade_Lane_Ring_8"),
                                QStringLiteral("trade_lane_ring_li_01"),
                                456,
                                2002));

    const TradeLaneChainDetection detection = TradeLaneEditService::inspectChain(
        document,
        QStringLiteral("Hi01_Trade_Lane_Ring_6"));

    QVERIFY(detection.chain.has_value());
    QCOMPARE(detection.issue, TradeLaneChainIssue::MissingNextRing);
    QCOMPARE(detection.referencedNickname, QStringLiteral("hi01_trade_lane_ring_8"));
    QVERIFY(detection.boundaryRing);
    QCOMPARE(detection.boundaryRing->nickname(), QStringLiteral("Hi01_Trade_Lane_Ring_7"));
    QCOMPARE(detection.chain->rings.size(), 3);
}

void TestTradeLaneEditService::rebuildsLaneConsistently()
{
    SystemDocument document;
    document.setFilePath(QStringLiteral("C:/Tmp/Rh05.ini"));
    document.addObject(makeRing(QStringLiteral("Rh05_Trade_Lane_Ring_1"),
                                QVector3D(0.0f, 0.0f, 0.0f),
                                QString(),
                                QStringLiteral("Rh05_Trade_Lane_Ring_2"),
                                QStringLiteral("trade_lane_ring_rh_01"),
                                123,
                                1001));
    document.addObject(makeRing(QStringLiteral("Rh05_Trade_Lane_Ring_2"),
                                QVector3D(5000.0f, 0.0f, 0.0f),
                                QStringLiteral("Rh05_Trade_Lane_Ring_1"),
                                QStringLiteral("Rh05_Trade_Lane_Ring_3"),
                                QStringLiteral("trade_lane_ring_rh_01"),
                                123));
    document.addObject(makeRing(QStringLiteral("Rh05_Trade_Lane_Ring_3"),
                                QVector3D(10000.0f, 0.0f, 0.0f),
                                QStringLiteral("Rh05_Trade_Lane_Ring_2"),
                                QString(),
                                QStringLiteral("trade_lane_ring_rh_01"),
                                123,
                                1002));
    document.addObject(makeRing(QStringLiteral("Rh05_Trade_Lane_Ring_99"),
                                QVector3D(30000.0f, 0.0f, 0.0f),
                                QString(),
                                QString(),
                                QStringLiteral("trade_lane_ring_rh_01"),
                                0));

    const auto chainOpt = TradeLaneEditService::detectChain(document, QStringLiteral("Rh05_Trade_Lane_Ring_1"));
    QVERIFY(chainOpt.has_value());

    TradeLaneEditRequest request;
    request.systemNickname = QStringLiteral("Rh05");
    request.startNumber = 1;
    request.startPosition = QVector3D(0.0f, 0.0f, 0.0f);
    request.endPosition = QVector3D(15000.0f, 0.0f, 0.0f);
    request.ringCount = 4;
    request.archetype = QStringLiteral("Trade_Lane_Ring");
    request.loadout = QStringLiteral("trade_lane_ring_rh_01");
    request.behavior = QStringLiteral("NOTHING");
    request.pilot = QStringLiteral("pilot_solar_easiest");
    request.difficultyLevel = 1;
    request.routeIdsName = 123;
    request.startSpaceNameId = 1001;
    request.endSpaceNameId = 1002;
    request.idsInfo = 66170;

    QSet<QString> occupiedNicknames{QStringLiteral("rh05_trade_lane_ring_99")};
    QVector<std::shared_ptr<SolarObject>> replacementObjects;
    QString errorMessage;
    QVERIFY2(TradeLaneEditService::buildReplacementObjects(*chainOpt,
                                                           request,
                                                           occupiedNicknames,
                                                           &replacementObjects,
                                                           &errorMessage),
             qPrintable(errorMessage));

    QCOMPARE(replacementObjects.size(), 4);
    QCOMPARE(replacementObjects[0]->nickname(), QStringLiteral("Rh05_Trade_Lane_Ring_1"));
    QCOMPARE(replacementObjects[3]->nickname(), QStringLiteral("Rh05_Trade_Lane_Ring_4"));
    QCOMPARE(replacementObjects[0]->rawEntries().contains({QStringLiteral("next_ring"), QStringLiteral("Rh05_Trade_Lane_Ring_2")}), true);
    QCOMPARE(replacementObjects[1]->rawEntries().contains({QStringLiteral("prev_ring"), QStringLiteral("Rh05_Trade_Lane_Ring_1")}), true);
    QCOMPARE(replacementObjects[3]->rawEntries().contains({QStringLiteral("prev_ring"), QStringLiteral("Rh05_Trade_Lane_Ring_3")}), true);
    QCOMPARE(replacementObjects[0]->position().x(), 0.0f);
    QCOMPARE(replacementObjects[1]->position().x(), 5000.0f);
    QCOMPARE(replacementObjects[2]->position().x(), 10000.0f);
    QCOMPARE(replacementObjects[3]->position().x(), 15000.0f);
}

QTEST_GUILESS_MAIN(TestTradeLaneEditService)
#include "test_TradeLaneEditService.moc"