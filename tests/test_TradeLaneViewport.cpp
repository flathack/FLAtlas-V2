// test_TradeLaneViewport.cpp - reproduces loading TLR_lod.3db into the Qt3D viewport

#include <QtTest/QtTest>

#include "rendering/view3d/ModelViewport3D.h"

class TestTradeLaneViewport : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void testLoadTradeLaneViewport();
};

void TestTradeLaneViewport::initTestCase()
{
    qputenv("QT3D_RENDERER", "opengl");
}

void TestTradeLaneViewport::testLoadTradeLaneViewport()
{
    const QString filePath =
        QStringLiteral("C:/Users/steve/Github/FL-Installationen/TESTMOD1/DATA/SOLAR/DOCKABLE/TLR_lod.3db");
    if (!QFileInfo::exists(filePath))
        QSKIP("Trade lane model not present in local test installation");

    flatlas::rendering::ModelViewport3D viewport;
    viewport.resize(800, 600);
    viewport.show();
    QVERIFY(QTest::qWaitForWindowExposed(&viewport, 5000));

    QString errorMessage;
    const bool loaded = viewport.loadModelFile(filePath, &errorMessage);
    QVERIFY2(loaded, qPrintable(errorMessage));
    QVERIFY(viewport.hasModel());

    QTest::qWait(250);
}

QTEST_MAIN(TestTradeLaneViewport)
#include "test_TradeLaneViewport.moc"
