// test_CsvImporter.cpp – Phase 12 CSV Importer tests

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>

#include "infrastructure/io/CsvImporter.h"

using namespace flatlas::infrastructure;

class TestCsvImporter : public QObject {
    Q_OBJECT
private slots:
    void testSimpleParse();
    void testQuotedFields();
    void testCustomSeparator();
    void testEmptyFile();
};

void TestCsvImporter::testSimpleParse()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("test.csv");

    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&f);
        out << "id,name,value\n"
            << "1,Alpha,100\n"
            << "2,Beta,200\n";
    }

    auto rows = CsvImporter::parse(path);
    QCOMPARE(rows.size(), 3); // header + 2 data rows
    QCOMPARE(rows[0][0], QStringLiteral("id"));
    QCOMPARE(rows[1][1], QStringLiteral("Alpha"));
    QCOMPARE(rows[2][2], QStringLiteral("200"));
}

void TestCsvImporter::testQuotedFields()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("quoted.csv");

    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&f);
        out << "1,\"Hello, World\",test\n"
            << "2,\"She said \"\"hi\"\"\",ok\n";
    }

    auto rows = CsvImporter::parse(path);
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows[0][1], QStringLiteral("Hello, World"));
    QCOMPARE(rows[1][1], QStringLiteral("She said \"hi\""));
}

void TestCsvImporter::testCustomSeparator()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("semi.csv");

    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&f);
        out << "100;System Alpha\n"
            << "200;System Beta\n";
    }

    auto rows = CsvImporter::parse(path, QLatin1Char(';'));
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows[0][0], QStringLiteral("100"));
    QCOMPARE(rows[0][1], QStringLiteral("System Alpha"));
}

void TestCsvImporter::testEmptyFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("empty.csv");

    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.close();

    auto rows = CsvImporter::parse(path);
    QCOMPARE(rows.size(), 0);
}

QTEST_GUILESS_MAIN(TestCsvImporter)
#include "test_CsvImporter.moc"
