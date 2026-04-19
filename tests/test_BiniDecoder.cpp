#include <QtTest>
#include "infrastructure/parser/BiniDecoder.h"

class TestBiniDecoder : public QObject {
    Q_OBJECT
private slots:
    void detectNonBini()
    {
        QByteArray data("NOT_BINI_DATA");
        QVERIFY(!flatlas::infrastructure::BiniDecoder::isBini(data));
    }
    void detectBiniMagic()
    {
        QByteArray data("BINI\x01\x00\x00\x00", 8);
        QVERIFY(flatlas::infrastructure::BiniDecoder::isBini(data));
    }
};

QTEST_MAIN(TestBiniDecoder)
#include "test_BiniDecoder.moc"
