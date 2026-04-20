// test_ShortestPath.cpp – Phase 24 Shortest-Path-Generator tests

#include <QtTest/QtTest>
#include "tools/ShortestPathGenerator.h"
#include "domain/UniverseData.h"

using namespace flatlas::tools;
using namespace flatlas::domain;

class TestShortestPath : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<UniverseData> createTestUniverse();

private slots:
    void testEmptyUniverse();
    void testSameSystem();
    void testDirectConnection();
    void testMultiHopPath();
    void testUnreachableSystem();
    void testPathDistance();
    void testPathJumps();
    void testSystemNames();
    void testCaseInsensitive();
    void testBidirectional();
    void testLinearChain();
};

std::unique_ptr<UniverseData> TestShortestPath::createTestUniverse()
{
    // Erstelle ein kleines Test-Universum:
    //   Li01 (0,0,0) -- Li02 (1000,0,0) -- Li03 (2000,0,0)
    //           \                                 /
    //            Li04 (500,0,1000) ------------- /
    auto data = std::make_unique<UniverseData>();

    data->addSystem({"Li01", "Liberty", "", QVector3D(0, 0, 0), 0, 0});
    data->addSystem({"Li02", "New York", "", QVector3D(1000, 0, 0), 0, 0});
    data->addSystem({"Li03", "California", "", QVector3D(2000, 0, 0), 0, 0});
    data->addSystem({"Li04", "Texas", "", QVector3D(500, 0, 1000), 0, 0});

    data->connections.append({"Li01", "Gate01", "Li02", "Gate02"});
    data->connections.append({"Li02", "Gate03", "Li03", "Gate04"});
    data->connections.append({"Li01", "Gate05", "Li04", "Gate06"});
    data->connections.append({"Li04", "Gate07", "Li03", "Gate08"});

    return data;
}

void TestShortestPath::testEmptyUniverse()
{
    UniverseData empty;
    ShortestPathGenerator gen(&empty);
    QVERIFY(gen.findShortestPath("Li01", "Li02").isEmpty());
}

void TestShortestPath::testSameSystem()
{
    auto data = createTestUniverse();
    ShortestPathGenerator gen(data.get());
    QStringList path = gen.findShortestPath("Li01", "Li01");
    QCOMPARE(path.size(), 1);
    QCOMPARE(path.first(), QStringLiteral("Li01"));
}

void TestShortestPath::testDirectConnection()
{
    auto data = createTestUniverse();
    ShortestPathGenerator gen(data.get());
    QStringList path = gen.findShortestPath("Li01", "Li02");
    QCOMPARE(path.size(), 2);
    QCOMPARE(path.first(), QStringLiteral("li01"));
    QCOMPARE(path.last(), QStringLiteral("li02"));
}

void TestShortestPath::testMultiHopPath()
{
    auto data = createTestUniverse();
    ShortestPathGenerator gen(data.get());
    QStringList path = gen.findShortestPath("Li01", "Li03");
    // Should find a path (either Li01→Li02→Li03 or Li01→Li04→Li03)
    QVERIFY(path.size() >= 2);
    QCOMPARE(path.first(), QStringLiteral("li01"));
    QCOMPARE(path.last(), QStringLiteral("li03"));
}

void TestShortestPath::testUnreachableSystem()
{
    auto data = createTestUniverse();
    // Isoliertes System hinzufügen
    data->addSystem({"Bw01", "Bretonia", "", QVector3D(5000, 0, 5000), 0, 0});

    ShortestPathGenerator gen(data.get());
    QStringList path = gen.findShortestPath("Li01", "Bw01");
    QVERIFY(path.isEmpty());
}

void TestShortestPath::testPathDistance()
{
    auto data = createTestUniverse();
    ShortestPathGenerator gen(data.get());
    gen.findShortestPath("Li01", "Li02");
    QVERIFY(gen.lastPathDistance() > 0.0);
    // Distanz sollte ~1000 sein (Euklidischer Abstand)
    QVERIFY(qAbs(gen.lastPathDistance() - 1000.0) < 1.0);
}

void TestShortestPath::testPathJumps()
{
    auto data = createTestUniverse();
    ShortestPathGenerator gen(data.get());
    gen.findShortestPath("Li01", "Li02");
    QCOMPARE(gen.lastPathJumps(), 1);

    gen.findShortestPath("Li01", "Li03");
    QVERIFY(gen.lastPathJumps() >= 2);
}

void TestShortestPath::testSystemNames()
{
    auto data = createTestUniverse();
    ShortestPathGenerator gen(data.get());
    QStringList names = gen.systemNames();
    QCOMPARE(names.size(), 4);
    // Sortiert, lowercase
    QVERIFY(names.contains(QStringLiteral("li01")));
    QVERIFY(names.contains(QStringLiteral("li02")));
    QVERIFY(names.contains(QStringLiteral("li03")));
    QVERIFY(names.contains(QStringLiteral("li04")));
}

void TestShortestPath::testCaseInsensitive()
{
    auto data = createTestUniverse();
    ShortestPathGenerator gen(data.get());
    QStringList path1 = gen.findShortestPath("li01", "li02");
    QStringList path2 = gen.findShortestPath("LI01", "LI02");
    QStringList path3 = gen.findShortestPath("Li01", "Li02");
    QCOMPARE(path1, path2);
    QCOMPARE(path2, path3);
}

void TestShortestPath::testBidirectional()
{
    auto data = createTestUniverse();
    ShortestPathGenerator gen(data.get());
    QStringList forward = gen.findShortestPath("Li01", "Li03");
    double fwdDist = gen.lastPathDistance();

    QStringList backward = gen.findShortestPath("Li03", "Li01");
    double bwdDist = gen.lastPathDistance();

    QCOMPARE(forward.size(), backward.size());
    QVERIFY(qAbs(fwdDist - bwdDist) < 0.001);
}

void TestShortestPath::testLinearChain()
{
    // A → B → C → D → E (linear)
    UniverseData data;
    data.addSystem({"A", "A", "", QVector3D(0, 0, 0), 0, 0});
    data.addSystem({"B", "B", "", QVector3D(100, 0, 0), 0, 0});
    data.addSystem({"C", "C", "", QVector3D(200, 0, 0), 0, 0});
    data.addSystem({"D", "D", "", QVector3D(300, 0, 0), 0, 0});
    data.addSystem({"E", "E", "", QVector3D(400, 0, 0), 0, 0});
    data.connections.append({"A", "", "B", ""});
    data.connections.append({"B", "", "C", ""});
    data.connections.append({"C", "", "D", ""});
    data.connections.append({"D", "", "E", ""});

    ShortestPathGenerator gen(&data);
    QStringList path = gen.findShortestPath("A", "E");
    QCOMPARE(path.size(), 5);
    QCOMPARE(gen.lastPathJumps(), 4);
    // Distanz sollte 400 sein (4 × 100)
    QVERIFY(qAbs(gen.lastPathDistance() - 400.0) < 1.0);
}

QTEST_MAIN(TestShortestPath)
#include "test_ShortestPath.moc"
