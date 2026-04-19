#include <QtTest>
#include <QTemporaryFile>
#include <QtEndian>
#include <cstring>
#include "infrastructure/parser/BiniDecoder.h"

using namespace flatlas::infrastructure;

class TestBiniDecoder : public QObject {
    Q_OBJECT

    // Helper: build a minimal BINI blob
    static QByteArray buildBini(const QByteArray &sectionData, const QByteArray &stringTable)
    {
        QByteArray header("BINI", 4);
        quint32 version = 1;
        quint32 strOff = 12 + sectionData.size();
        QByteArray vBuf(4, '\0'), oBuf(4, '\0');
        qToLittleEndian(version, vBuf.data());
        qToLittleEndian(strOff, oBuf.data());
        return header + vBuf + oBuf + sectionData + stringTable;
    }

    static QByteArray makeU16(quint16 v) {
        QByteArray b(2, '\0');
        qToLittleEndian(v, b.data());
        return b;
    }
    static QByteArray makeU32(quint32 v) {
        QByteArray b(4, '\0');
        qToLittleEndian(v, b.data());
        return b;
    }
    static QByteArray makeI32(qint32 v) {
        QByteArray b(4, '\0');
        qToLittleEndian(v, b.data());
        return b;
    }
    static QByteArray makeF32(float v) {
        QByteArray b(4, '\0');
        quint32 bits;
        std::memcpy(&bits, &v, 4);
        qToLittleEndian(bits, b.data());
        return b;
    }

private slots:
    void detectNonBini()
    {
        QByteArray data("NOT_BINI_DATA");
        QVERIFY(!BiniDecoder::isBini(data));
    }

    void detectBiniMagic()
    {
        QByteArray data("BINI\x01\x00\x00\x00\x0c\x00\x00\x00", 12);
        QVERIFY(BiniDecoder::isBini(data));
    }

    void detectTooShort()
    {
        QByteArray data("BINI\x01\x00", 6);
        QVERIFY(!BiniDecoder::isBini(data));
    }

    void decodeEmptyBini()
    {
        // BINI with no sections - string table starts at offset 12
        QByteArray bini = buildBini({}, QByteArray(1, '\0'));
        QString text = BiniDecoder::decode(bini);
        // No sections → empty or just newline
        QVERIFY(text.trimmed().isEmpty());
    }

    void decodeSectionNoEntries()
    {
        // String table: "TestSec\0"
        QByteArray strTab = QByteArray("TestSec\0", 8);
        // Section header: nameOff=0, entryCount=0
        QByteArray secData = makeU16(0) + makeU16(0);
        QByteArray bini = buildBini(secData, strTab);
        QString text = BiniDecoder::decode(bini);
        QVERIFY(text.contains(QStringLiteral("[TestSec]")));
    }

    void decodeIntValue()
    {
        // String table: "Sec\0key\0"
        QByteArray strTab = QByteArray("Sec\0key\0", 8);
        // Section: nameOff=0, 1 entry
        QByteArray secData = makeU16(0) + makeU16(1);
        // Entry: keyOff=4, 1 value
        secData += makeU16(4) + QByteArray(1, char(1));
        // Value: type=1 (int), payload=42
        secData += QByteArray(1, char(1)) + makeI32(42);
        QByteArray bini = buildBini(secData, strTab);
        QString text = BiniDecoder::decode(bini);
        QVERIFY(text.contains(QStringLiteral("[Sec]")));
        QVERIFY(text.contains(QStringLiteral("key = 42")));
    }

    void decodeFloatValue()
    {
        QByteArray strTab = QByteArray("S\0k\0", 4);
        QByteArray secData = makeU16(0) + makeU16(1);
        secData += makeU16(2) + QByteArray(1, char(1));
        secData += QByteArray(1, char(2)) + makeF32(3.14f);
        QByteArray bini = buildBini(secData, strTab);
        QString text = BiniDecoder::decode(bini);
        QVERIFY(text.contains(QStringLiteral("k = ")));
        // Should contain a float representation close to 3.14
        QVERIFY(text.contains(QStringLiteral("3.14")));
    }

    void decodeStringValue()
    {
        // String table: "Sec\0key\0hello\0"
        QByteArray strTab = QByteArray("Sec\0key\0hello\0", 14);
        QByteArray secData = makeU16(0) + makeU16(1);
        secData += makeU16(4) + QByteArray(1, char(1));
        // Value: type=3 (string ref), offset=8 ("hello")
        secData += QByteArray(1, char(3)) + makeU32(8);
        QByteArray bini = buildBini(secData, strTab);
        QString text = BiniDecoder::decode(bini);
        QVERIFY(text.contains(QStringLiteral("key = hello")));
    }

    void decodeMultipleValues()
    {
        // key = 10, 3.5
        QByteArray strTab = QByteArray("S\0k\0", 4);
        QByteArray secData = makeU16(0) + makeU16(1);
        secData += makeU16(2) + QByteArray(1, char(2)); // 2 values
        secData += QByteArray(1, char(1)) + makeI32(10);  // int 10
        secData += QByteArray(1, char(2)) + makeF32(3.5f); // float 3.5
        QByteArray bini = buildBini(secData, strTab);
        QString text = BiniDecoder::decode(bini);
        QVERIFY(text.contains(QStringLiteral("k = 10, 3.5")));
    }

    void decodeMultipleSections()
    {
        // String table: "A\0B\0x\0"
        QByteArray strTab = QByteArray("A\0B\0x\0", 6);
        // Section A: 0 entries
        QByteArray secData = makeU16(0) + makeU16(0);
        // Section B: 1 entry (key="x", int val 7)
        secData += makeU16(2) + makeU16(1);
        secData += makeU16(4) + QByteArray(1, char(1));
        secData += QByteArray(1, char(1)) + makeI32(7);
        QByteArray bini = buildBini(secData, strTab);
        QString text = BiniDecoder::decode(bini);
        QVERIFY(text.contains(QStringLiteral("[A]")));
        QVERIFY(text.contains(QStringLiteral("[B]")));
        QVERIFY(text.contains(QStringLiteral("x = 7")));
    }

    void isBiniFile_nonExistent()
    {
        QVERIFY(!BiniDecoder::isBiniFile(QStringLiteral("/nonexistent/path.ini")));
    }

    void isBiniFile_validFile()
    {
        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        QByteArray bini = buildBini({}, QByteArray(1, '\0'));
        tmp.write(bini);
        tmp.close();
        QVERIFY(BiniDecoder::isBiniFile(tmp.fileName()));
    }

    void decodeNonBiniReturnsEmpty()
    {
        QByteArray data("NOT_BINI_AT_ALL_NO_MAGIC");
        QVERIFY(BiniDecoder::decode(data).isEmpty());
    }

    void floatWithoutDotGetsDotZero()
    {
        // Value = 5.0f → "%.7g" = "5" → should become "5.0"
        QByteArray strTab = QByteArray("S\0k\0", 4);
        QByteArray secData = makeU16(0) + makeU16(1);
        secData += makeU16(2) + QByteArray(1, char(1));
        secData += QByteArray(1, char(2)) + makeF32(5.0f);
        QByteArray bini = buildBini(secData, strTab);
        QString text = BiniDecoder::decode(bini);
        QVERIFY(text.contains(QStringLiteral("k = 5.0")));
    }
};

QTEST_MAIN(TestBiniDecoder)
#include "test_BiniDecoder.moc"
