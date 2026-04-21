// tests/test_SceneView3D.cpp – Phase 7 tests for 3D view components

#include <QtTest/QtTest>
#include "domain/SystemDocument.h"
#include "domain/SolarObject.h"

#ifdef FLATLAS_HAS_QT3D
#include "rendering/view3d/OrbitCamera.h"
#include "rendering/view3d/ModelGeometryBuilder.h"
#include "rendering/view3d/SelectionManager.h"
#include <QMouseEvent>
#include <QPointingDevice>
#include <QWheelEvent>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QGeometryRenderer>
#endif

using namespace flatlas::domain;

class TestSceneView3D : public QObject {
    Q_OBJECT

private slots:
#ifdef FLATLAS_HAS_QT3D
    void testOrbitCameraDefaults();
    void testOrbitCameraSetTarget();
    void testOrbitCameraDistance();
    void testOrbitCameraElevationClamp();
    void testOrbitCameraReset();
    void testOrbitCameraUpdatesCameraPosition();
    void testOrbitCameraMouseRotate();
    void testOrbitCameraMousePan();
    void testOrbitCameraWheelZoom();
    void testTriangleRendererBuildsDoubleSidedIndices();
    void testSelectionManagerSelectEmits();
    void testSelectionManagerReselect();
    void testSelectionManagerClear();
#endif
};

#ifdef FLATLAS_HAS_QT3D

void TestSceneView3D::testOrbitCameraDefaults()
{
    Qt3DRender::QCamera camera;
    flatlas::rendering::OrbitCamera orbit(&camera);
    QCOMPARE(orbit.target(), QVector3D(0, 0, 0));
    QCOMPARE(orbit.distance(), 50000.0f);
    QCOMPARE(orbit.azimuth(), 45.0f);
    QCOMPARE(orbit.elevation(), 30.0f);
}

void TestSceneView3D::testOrbitCameraSetTarget()
{
    Qt3DRender::QCamera camera;
    flatlas::rendering::OrbitCamera orbit(&camera);
    orbit.setTarget(QVector3D(100, 200, 300));
    QCOMPARE(orbit.target(), QVector3D(100, 200, 300));
}

void TestSceneView3D::testOrbitCameraDistance()
{
    Qt3DRender::QCamera camera;
    flatlas::rendering::OrbitCamera orbit(&camera);
    orbit.setDistance(10000.0f);
    QCOMPARE(orbit.distance(), 10000.0f);

    // Below minimum
    orbit.setDistance(1.0f);
    QCOMPARE(orbit.distance(), 100.0f);

    // Above maximum
    orbit.setDistance(999999.0f);
    QCOMPARE(orbit.distance(), 500000.0f);
}

void TestSceneView3D::testOrbitCameraElevationClamp()
{
    Qt3DRender::QCamera camera;
    flatlas::rendering::OrbitCamera orbit(&camera);
    orbit.setElevation(100.0f);
    QVERIFY(orbit.elevation() <= 89.0f);
    orbit.setElevation(-100.0f);
    QVERIFY(orbit.elevation() >= -89.0f);
}

void TestSceneView3D::testOrbitCameraReset()
{
    Qt3DRender::QCamera camera;
    flatlas::rendering::OrbitCamera orbit(&camera);
    orbit.setTarget(QVector3D(999, 999, 999));
    orbit.setDistance(1000.0f);
    orbit.setAzimuth(180.0f);
    orbit.setElevation(-45.0f);

    orbit.resetView();

    QCOMPARE(orbit.target(), QVector3D(0, 0, 0));
    QCOMPARE(orbit.distance(), 50000.0f);
    QCOMPARE(orbit.azimuth(), 45.0f);
    QCOMPARE(orbit.elevation(), 30.0f);
}

void TestSceneView3D::testOrbitCameraUpdatesCameraPosition()
{
    Qt3DRender::QCamera camera;
    flatlas::rendering::OrbitCamera orbit(&camera);
    orbit.setTarget(QVector3D(0, 0, 0));
    orbit.setDistance(1000.0f);
    orbit.setElevation(0.0f);
    orbit.setAzimuth(0.0f);

    // At azimuth=0, elevation=0: camera should be at (0, 0, distance)
    QVector3D pos = camera.position();
    QCOMPARE(pos.x(), 0.0f);
    QVERIFY(qAbs(pos.y()) < 1.0f);
    QVERIFY(qAbs(pos.z() - 1000.0f) < 1.0f);
}

void TestSceneView3D::testOrbitCameraMouseRotate()
{
    Qt3DRender::QCamera camera;
    flatlas::rendering::OrbitCamera orbit(&camera);
    orbit.setRotateButton(Qt::LeftButton);
    orbit.setAzimuth(45.0f);
    orbit.setElevation(30.0f);

    QMouseEvent press(QEvent::MouseButtonPress,
                      QPointF(10.0, 10.0), QPointF(10.0, 10.0),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier,
                      QPointingDevice::primaryPointingDevice());
    orbit.handleMousePress(&press);

    QMouseEvent move(QEvent::MouseMove,
                     QPointF(30.0, 0.0), QPointF(30.0, 0.0),
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier,
                     QPointingDevice::primaryPointingDevice());
    orbit.handleMouseMove(&move);

    QMouseEvent release(QEvent::MouseButtonRelease,
                        QPointF(30.0, 0.0), QPointF(30.0, 0.0),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier,
                        QPointingDevice::primaryPointingDevice());
    orbit.handleMouseRelease(&release);

    QVERIFY(orbit.azimuth() > 45.0f);
    QVERIFY(orbit.elevation() > 30.0f);
}

void TestSceneView3D::testOrbitCameraMousePan()
{
    Qt3DRender::QCamera camera;
    flatlas::rendering::OrbitCamera orbit(&camera);
    orbit.setRotateButton(Qt::LeftButton);
    orbit.setPanButton(Qt::RightButton);
    orbit.setDistance(1000.0f);

    const QVector3D originalTarget = orbit.target();

    QMouseEvent press(QEvent::MouseButtonPress,
                      QPointF(10.0, 10.0), QPointF(10.0, 10.0),
                      Qt::RightButton, Qt::RightButton, Qt::NoModifier,
                      QPointingDevice::primaryPointingDevice());
    orbit.handleMousePress(&press);

    QMouseEvent move(QEvent::MouseMove,
                     QPointF(60.0, 30.0), QPointF(60.0, 30.0),
                     Qt::NoButton, Qt::RightButton, Qt::NoModifier,
                     QPointingDevice::primaryPointingDevice());
    orbit.handleMouseMove(&move);

    QMouseEvent release(QEvent::MouseButtonRelease,
                        QPointF(60.0, 30.0), QPointF(60.0, 30.0),
                        Qt::RightButton, Qt::NoButton, Qt::NoModifier,
                        QPointingDevice::primaryPointingDevice());
    orbit.handleMouseRelease(&release);

    QVERIFY(orbit.target() != originalTarget);
}

void TestSceneView3D::testOrbitCameraWheelZoom()
{
    Qt3DRender::QCamera camera;
    flatlas::rendering::OrbitCamera orbit(&camera);
    orbit.setDistance(1000.0f);

    QWheelEvent zoomIn(QPointF(10.0, 10.0), QPointF(10.0, 10.0),
                       QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                       Qt::NoScrollPhase, false, Qt::MouseEventNotSynthesized,
                       QPointingDevice::primaryPointingDevice());
    orbit.handleWheel(&zoomIn);
    const float zoomedInDistance = orbit.distance();
    QVERIFY(zoomedInDistance < 1000.0f);

    QWheelEvent zoomOut(QPointF(10.0, 10.0), QPointF(10.0, 10.0),
                        QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier,
                        Qt::NoScrollPhase, false, Qt::MouseEventNotSynthesized,
                        QPointingDevice::primaryPointingDevice());
    orbit.handleWheel(&zoomOut);
    QVERIFY(orbit.distance() > zoomedInDistance);
}

void TestSceneView3D::testTriangleRendererBuildsDoubleSidedIndices()
{
    flatlas::infrastructure::MeshData mesh;
    mesh.vertices.append({QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0.0f, 0.0f, 1.0f), 0.0f, 0.0f});
    mesh.vertices.append({QVector3D(1.0f, 0.0f, 0.0f), QVector3D(0.0f, 0.0f, 1.0f), 1.0f, 0.0f});
    mesh.vertices.append({QVector3D(0.0f, 1.0f, 0.0f), QVector3D(0.0f, 0.0f, 1.0f), 0.0f, 1.0f});
    mesh.indices = {0u, 1u, 2u};

    auto *renderer = flatlas::rendering::ModelGeometryBuilder::buildTriangleRenderer(mesh, nullptr);
    QVERIFY(renderer);
    QCOMPARE(renderer->vertexCount(), 6);
    delete renderer;
}

void TestSceneView3D::testSelectionManagerSelectEmits()
{
    flatlas::rendering::SelectionManager mgr;
    QSignalSpy spy(&mgr, &flatlas::rendering::SelectionManager::objectSelected);
    mgr.select("test_obj");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toString(), "test_obj");
    QCOMPARE(mgr.selectedNickname(), "test_obj");
}

void TestSceneView3D::testSelectionManagerReselect()
{
    flatlas::rendering::SelectionManager mgr;
    mgr.select("obj_a");
    QSignalSpy spy(&mgr, &flatlas::rendering::SelectionManager::objectSelected);
    mgr.select("obj_a");  // same again – should NOT emit
    QCOMPARE(spy.count(), 0);
    mgr.select("obj_b");  // different – should emit
    QCOMPARE(spy.count(), 1);
}

void TestSceneView3D::testSelectionManagerClear()
{
    flatlas::rendering::SelectionManager mgr;
    mgr.select("test_obj");
    mgr.clear();
    QVERIFY(mgr.selectedNickname().isEmpty());
}

#endif // FLATLAS_HAS_QT3D

QTEST_MAIN(TestSceneView3D)
#include "test_SceneView3D.moc"
