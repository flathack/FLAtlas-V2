#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QtEndian>
#include "infrastructure/parser/BiniConverter.h"
#include "infrastructure/parser/BiniDecoder.h"

using namespace flatlas::infrastructure;

class TestBiniConverter : public QObject {
    Q_OBJECT

    static QByteArray makeMinimalBini()
    {
        // "BINI" + version(4) + strOff(4=12) + string table "S\0k\0"(4)
        // Section: nameOff=0, 1 entry; Entry: keyOff=2, 1 value; Value: int=99
        QByteArray hdr("BINI", 4);
        quint32 ver = 1, strOff = 12 + 4 + 3 + 5; // 12 + secHdr(4) + entry(3) + value(5) = 24
        QByteArray vBuf(4, '\0'), oBuf(4, '\0');
        qToLittleEndian(ver, vBuf.data());
        qToLittleEndian(strOff, oBuf.data());

        // Section header: nameOff=0, entryCount=1
        QByteArray sec(4, '\0');
        qToLittleEndian(quint16(0), sec.data());
        qToLittleEndian(quint16(1), sec.data() + 2);

        // Entry: keyOff=2, valueCount=1
        QByteArray entry(3, '\0');
        qToLittleEndian(quint16(2), entry.data());
        entry[2] = char(1);

        // Value: type=1(int), payload=99
        QByteArray val(5, '\0');
        val[0] = char(1);
        qToLittleEndian(qint32(99), val.data() + 1);

        // String table: "S\0k\0"
        QByteArray strTab("S\0k\0", 4);

        return hdr + vBuf + oBuf + sec + entry + val + strTab;
    }

private slots:
    void convertSingleFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        QString inPath = dir.filePath(QStringLiteral("test.ini"));
        QString outPath = dir.filePath(QStringLiteral("test_out.ini"));

        QFile f(inPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(makeMinimalBini());
        f.close();

        QVERIFY(BiniConverter::convertFile(inPath, outPath));
        QFile out(outPath);
        QVERIFY(out.open(QIODevice::ReadOnly));
        QString text = QString::fromUtf8(out.readAll());
        QVERIFY(text.contains(QStringLiteral("[S]")));
        QVERIFY(text.contains(QStringLiteral("k = 99")));
    }

    void countBiniFilesInDir()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        // Write 2 BINI files and 1 plain text file
        for (const QString &name : {QStringLiteral("a.ini"), QStringLiteral("b.ini")}) {
            QFile f(dir.filePath(name));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(makeMinimalBini());
            f.close();
        }
        {
            QFile f(dir.filePath(QStringLiteral("c.ini")));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("[Plain]\nkey = val\n");
            f.close();
        }

        QCOMPARE(BiniConverter::countBiniFiles(dir.path()), 2);
    }

    void convertDirectory()
    {
        QTemporaryDir inDir, outDir;
        QVERIFY(inDir.isValid());
        QVERIFY(outDir.isValid());

        QFile f(inDir.filePath(QStringLiteral("data.ini")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(makeMinimalBini());
        f.close();

        int count = BiniConverter::convertDirectory(inDir.path(), outDir.path());
        QCOMPARE(count, 1);

        QFile out(outDir.filePath(QStringLiteral("data.ini")));
        QVERIFY(out.exists());
    }

    void convertNonBiniSkipped()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString inPath = dir.filePath(QStringLiteral("plain.ini"));
        QString outPath = dir.filePath(QStringLiteral("plain_out.ini"));

        QFile f(inPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("[Section]\nkey = value\n");
        f.close();

        // convertFile should return false for non-BINI
        QVERIFY(!BiniConverter::convertFile(inPath, outPath));
    }
};

QTEST_MAIN(TestBiniConverter)
#include "test_BiniConverter.moc"
