#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include "rendering/view3d/RingPreviewSceneBuilder.h"

using namespace flatlas::rendering;

class TestRingPreviewSceneBuilder : public QObject
{
    Q_OBJECT

private slots:
    void buildsRingOnlySceneWhenHostMissing();
    void addsPlanetScaleSphereWhenRequested();
    void fallsBackForInvalidGeometry();
};

void TestRingPreviewSceneBuilder::buildsRingOnlySceneWhenHostMissing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString ringDir = dir.path() + QStringLiteral("/DATA/SOLAR/RINGS");
    QVERIFY(QDir().mkpath(ringDir));

    QFile ringPreset(ringDir + QStringLiteral("/test_ring.ini"));
    QVERIFY(ringPreset.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&ringPreset);
    out << "[Exterior]\n";
    out << "color = 120, 180, 240\n";
    out << "opacity = 0.5\n";
    out << "num_segments = 48\n";
    ringPreset.close();

    RingPreviewSceneRequest request;
    request.gameRoot = dir.path();
    request.hostModelPath = dir.path() + QStringLiteral("/missing_model.3db");
    request.ringPreset = QStringLiteral("solar\\rings\\test_ring.ini");
    request.innerRadius = 1500.0;
    request.outerRadius = 3000.0;
    request.thickness = 500.0;
    request.rotationEuler = QVector3D(10.0f, 20.0f, 30.0f);

    const RingPreviewSceneResult result = RingPreviewSceneBuilder::build(request);
    QVERIFY(result.hasRenderableScene);
    QVERIFY(result.hasRingGeometry);
    QVERIFY(!result.hasHostModel);
    QCOMPARE(result.visualStyle.color, QColor(120, 180, 240));
    QCOMPARE(result.visualStyle.segments, 48);
    QCOMPARE(result.sceneRoot.children.size(), 1);
    QCOMPARE(result.sceneRoot.children.first().meshes.size(), 1);
}

void TestRingPreviewSceneBuilder::addsPlanetScaleSphereWhenRequested()
{
    RingPreviewSceneRequest request;
    request.showHostRadiusSphere = true;
    request.hostRadius = 2000.0;
    request.innerRadius = 2500.0;
    request.outerRadius = 4000.0;
    request.thickness = 300.0;

    const RingPreviewSceneResult result = RingPreviewSceneBuilder::build(request);
    QVERIFY(result.hasRenderableScene);
    QVERIFY(result.hasRingGeometry);
    QCOMPARE(result.sceneRoot.children.size(), 2);
    QCOMPARE(result.sceneRoot.children.first().meshes.size(), 1);
    QCOMPARE(result.sceneRoot.children.first().name, QStringLiteral("ring-preview-host-radius"));
}

void TestRingPreviewSceneBuilder::fallsBackForInvalidGeometry()
{
    RingPreviewSceneRequest request;
    request.innerRadius = 2000.0;
    request.outerRadius = 1000.0;
    request.thickness = 250.0;

    const RingPreviewSceneResult result = RingPreviewSceneBuilder::build(request);
    QVERIFY(!result.hasRenderableScene);
    QVERIFY(!result.geometryInputsValid);
    QVERIFY(!result.hasRingGeometry);
    QVERIFY(!result.statusMessage.trimmed().isEmpty());
}

QTEST_GUILESS_MAIN(TestRingPreviewSceneBuilder)
#include "test_RingPreviewSceneBuilder.moc"