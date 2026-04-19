#include <QtTest>
#include "core/PathUtils.h"

class TestPathUtils : public QObject {
    Q_OBJECT
private slots:
    void parsePositionValid()
    {
        float x = 0, y = 0, z = 0;
        bool ok = flatlas::core::PathUtils::parsePosition(QStringLiteral("100, -200, 300"), x, y, z);
        QVERIFY(ok);
        QCOMPARE(x, 100.0f);
        QCOMPARE(y, -200.0f);
        QCOMPARE(z, 300.0f);
    }
    void parsePositionEmpty()
    {
        float x = 0, y = 0, z = 0;
        bool ok = flatlas::core::PathUtils::parsePosition(QString(), x, y, z);
        QVERIFY(!ok);
    }
};

QTEST_MAIN(TestPathUtils)
#include "test_PathUtils.moc"
