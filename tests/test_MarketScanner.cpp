// test_MarketScanner.cpp – Phase 11 Market Scanner tests

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>

#include "editors/trade/MarketScanner.h"

using namespace flatlas::editors;
using namespace flatlas::domain;

class TestMarketScanner : public QObject {
    Q_OBJECT
private slots:
    void testScanFile();
    void testScanEmptyFile();
    void testScanDirectory();
};

void TestMarketScanner::testScanFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString filePath = dir.filePath("market_misc.ini");
    {
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&f);
        out << "[BaseGood]\n"
            << "base = li01_01_base\n"
            << "MarketGood = commodity_gold, 0, 0, 0, 500, 0, 0, 1\n"
            << "MarketGood = commodity_silver, 0, 0, 0, 200, 0, 0, 0\n"
            << "\n"
            << "[BaseGood]\n"
            << "base = li02_01_base\n"
            << "MarketGood = commodity_gold, 0, 0, 0, 700, 0, 0, 0\n";
    }

    auto entries = MarketScanner::scanFile(filePath);
    QCOMPARE(entries.size(), 3);

    // First entry: gold sold at li01_01_base for 500
    QCOMPARE(entries[0].base, QStringLiteral("li01_01_base"));
    QCOMPARE(entries[0].commodity, QStringLiteral("commodity_gold"));
    QCOMPARE(entries[0].price, 500);
    QCOMPARE(entries[0].sells, true);

    // Second entry: silver bought at li01_01_base for 200
    QCOMPARE(entries[1].commodity, QStringLiteral("commodity_silver"));
    QCOMPARE(entries[1].sells, false);

    // Third: gold bought at li02_01_base for 700
    QCOMPARE(entries[2].base, QStringLiteral("li02_01_base"));
    QCOMPARE(entries[2].sells, false);
}

void TestMarketScanner::testScanEmptyFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString filePath = dir.filePath("market_empty.ini");
    QFile f(filePath);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.close();

    auto entries = MarketScanner::scanFile(filePath);
    QCOMPARE(entries.size(), 0);
}

void TestMarketScanner::testScanDirectory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Create two market files
    for (const auto &name : {QStringLiteral("market_a.ini"), QStringLiteral("market_b.ini")}) {
        QFile f(dir.filePath(name));
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&f);
        out << "[BaseGood]\n"
            << "base = test_base\n"
            << "MarketGood = commodity_water, 0, 0, 0, 100, 0, 0, 1\n";
    }

    auto entries = MarketScanner::scanDirectory(dir.path());
    QCOMPARE(entries.size(), 2);
}

QTEST_GUILESS_MAIN(TestMarketScanner)
#include "test_MarketScanner.moc"
