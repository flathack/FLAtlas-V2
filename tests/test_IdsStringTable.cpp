// test_IdsStringTable.cpp – Phase 12 IDS String Table tests

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>

#include "infrastructure/freelancer/IdsStringTable.h"

using namespace flatlas::infrastructure;

class TestIdsStringTable : public QObject {
    Q_OBJECT
private slots:
    void testEmptyTable();
    void testMerge();
    void testCsvExportImport();
    void testCsvRoundtrip();
    void testFilterPerformance();
};

void TestIdsStringTable::testEmptyTable()
{
    IdsStringTable table;
    QCOMPARE(table.count(), 0);
    QVERIFY(!table.hasString(1));
    QVERIFY(table.getString(1).isEmpty());
    QVERIFY(table.allIds().isEmpty());
}

void TestIdsStringTable::testMerge()
{
    IdsStringTable table;

    QMap<int, QString> data;
    data.insert(1, QStringLiteral("Hello"));
    data.insert(2, QStringLiteral("World"));
    table.merge(data);

    QCOMPARE(table.count(), 2);
    QVERIFY(table.hasString(1));
    QCOMPARE(table.getString(1), QStringLiteral("Hello"));
    QCOMPARE(table.getString(2), QStringLiteral("World"));

    // Overwrite
    QMap<int, QString> extra;
    extra.insert(2, QStringLiteral("Updated"));
    extra.insert(3, QStringLiteral("New"));
    table.merge(extra);

    QCOMPARE(table.count(), 3);
    QCOMPARE(table.getString(2), QStringLiteral("Updated"));
}

void TestIdsStringTable::testCsvExportImport()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString csvPath = dir.filePath("test.csv");

    // Create table and export
    IdsStringTable table;
    QMap<int, QString> data;
    data.insert(100, QStringLiteral("System Name"));
    data.insert(200, QStringLiteral("Base \"Alpha\""));
    data.insert(300, QStringLiteral("Line1\nLine2"));
    table.merge(data);

    QVERIFY(table.exportCsv(csvPath));

    // Import into fresh table
    IdsStringTable table2;
    int imported = table2.importCsv(csvPath);
    QCOMPARE(imported, 3);
    QCOMPARE(table2.getString(100), QStringLiteral("System Name"));
    QCOMPARE(table2.getString(200), QStringLiteral("Base \"Alpha\""));
}

void TestIdsStringTable::testCsvRoundtrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString csvPath = dir.filePath("roundtrip.csv");

    IdsStringTable original;
    QMap<int, QString> data;
    for (int i = 0; i < 100; ++i)
        data.insert(i * 10, QStringLiteral("String_%1").arg(i));
    original.merge(data);

    QVERIFY(original.exportCsv(csvPath, QLatin1Char(';')));

    IdsStringTable loaded;
    int count = loaded.importCsv(csvPath, QLatin1Char(';'));
    QCOMPARE(count, 100);
    QCOMPARE(loaded.count(), 100);

    for (int i = 0; i < 100; ++i)
        QCOMPARE(loaded.getString(i * 10), QStringLiteral("String_%1").arg(i));
}

void TestIdsStringTable::testFilterPerformance()
{
    // Verify large table operations are fast
    IdsStringTable table;
    QMap<int, QString> data;
    for (int i = 0; i < 10000; ++i)
        data.insert(i, QStringLiteral("Test string number %1").arg(i));
    table.merge(data);

    QElapsedTimer timer;
    timer.start();

    // Search-like operation: iterate all strings
    int matches = 0;
    for (int id : table.allIds()) {
        if (table.getString(id).contains(QStringLiteral("999")))
            ++matches;
    }

    qint64 elapsed = timer.elapsed();
    QVERIFY(matches > 0);
    QVERIFY2(elapsed < 100, qPrintable(QStringLiteral("Search took %1 ms").arg(elapsed)));
}

QTEST_GUILESS_MAIN(TestIdsStringTable)
#include "test_IdsStringTable.moc"
