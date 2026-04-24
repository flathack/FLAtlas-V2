#include <QtTest/QtTest>

#include "domain/UniverseData.h"

using namespace flatlas::domain;

class TestJumpConnection : public QObject {
    Q_OBJECT
private slots:
    void testDefaultKind();
    void testRoundtripFields();
};

void TestJumpConnection::testDefaultKind()
{
    JumpConnection connection;
    QCOMPARE(connection.kind, QStringLiteral("other"));
    QVERIFY(connection.fromSystem.isEmpty());
    QVERIFY(connection.fromObject.isEmpty());
    QVERIFY(connection.toSystem.isEmpty());
    QVERIFY(connection.toObject.isEmpty());
}

void TestJumpConnection::testRoundtripFields()
{
    JumpConnection connection;
    connection.fromSystem = QStringLiteral("Li01");
    connection.fromObject = QStringLiteral("Li01_to_Li02_hole");
    connection.toSystem = QStringLiteral("Li02");
    connection.toObject = QStringLiteral("Li02_to_Li01_hole");
    connection.kind = QStringLiteral("hole");

    QCOMPARE(connection.fromSystem, QStringLiteral("Li01"));
    QCOMPARE(connection.fromObject, QStringLiteral("Li01_to_Li02_hole"));
    QCOMPARE(connection.toSystem, QStringLiteral("Li02"));
    QCOMPARE(connection.toObject, QStringLiteral("Li02_to_Li01_hole"));
    QCOMPARE(connection.kind, QStringLiteral("hole"));
}

QTEST_APPLESS_MAIN(TestJumpConnection)
#include "test_JumpConnection.moc"
