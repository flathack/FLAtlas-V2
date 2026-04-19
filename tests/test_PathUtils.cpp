#include <QtTest>
#include "core/PathUtils.h"

class TestPathUtils : public QObject {
    Q_OBJECT
private slots:
    void parsePositionValid()
    {
        auto pos = flatlas::core::PathUtils::parsePosition(QStringLiteral("100, -200, 300"));
        QCOMPARE(pos.x(), 100.0);
        QCOMPARE(pos.y(), -200.0);
        QCOMPARE(pos.z(), 300.0);
    }
    void parsePositionEmpty()
    {
        auto pos = flatlas::core::PathUtils::parsePosition(QString());
        QVERIFY(pos.isNull());
    }
};

QTEST_MAIN(TestPathUtils)
#include "test_PathUtils.moc"
