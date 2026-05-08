// tests/test_SceneView3D.cpp – Phase 7 tests for 3D view components

#include <QtTest/QtTest>
#include "domain/SystemDocument.h"
#include "domain/SolarObject.h"

#ifdef FLATLAS_HAS_QT3D
#include "rendering/view3d/FreeCameraController.h"
#include "rendering/view3d/MaterialFactory.h"
#include "rendering/view3d/OrbitCamera.h"
#include "rendering/view3d/ModelGeometryBuilder.h"
#include "rendering/view3d/SelectionManager.h"
#include <QMouseEvent>
#include <QPointingDevice>
#include <QWheelEvent>
#include <Qt3DExtras/QPhongAlphaMaterial>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QColorMask>
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QNoDepthMask>
#include <Qt3DRender/QRenderPass>
#include <Qt3DRender/QTechnique>
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
    void testOrbitCameraDistanceLimits();
    void testTriangleRendererBuildsDoubleSidedIndices();
    void testSelectionManagerSelectEmits();
    void testSelectionManagerReselect();
    void testSelectionManagerClear();
    void testAlphaMaterialsDisableFramebufferAlphaWrites();
    void testFreeCameraWheelChangesSpeed();
    void testFreeCameraKeyboardMovement();
    void testFreeCameraMouseLook();
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

    QVERIFY(orbit.azimuth() < 45.0f);
    QVERIFY(orbit.elevation() < 30.0f);
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

void TestSceneView3D::testOrbitCameraDistanceLimits()
{
    Qt3DRender::QCamera camera;
    flatlas::rendering::OrbitCamera orbit(&camera);
    orbit.setDistanceLimits(0.5f, 2500.0f);

    orbit.setDistance(0.1f);
    QCOMPARE(orbit.distance(), 0.5f);

    orbit.setDistance(3000.0f);
    QCOMPARE(orbit.distance(), 2500.0f);

    orbit.setResetState(QVector3D(0.0f, 0.0f, 0.0f), 0.2f, 45.0f, 30.0f);
    orbit.resetView();
    QCOMPARE(orbit.distance(), 0.5f);
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

void TestSceneView3D::testAlphaMaterialsDisableFramebufferAlphaWrites()
{
    Qt3DExtras::QPhongAlphaMaterial material;
    material.setAlpha(0.35f);

    flatlas::rendering::MaterialFactory::preventFramebufferAlphaWrites(&material);

    bool foundColorMask = false;
    bool foundDepthMask = false;
    for (Qt3DRender::QTechnique *technique : material.effect()->techniques()) {
        QVERIFY(technique);
        for (Qt3DRender::QRenderPass *pass : technique->renderPasses()) {
            QVERIFY(pass);
            for (Qt3DRender::QRenderState *state : pass->renderStates()) {
                const auto *mask = qobject_cast<const Qt3DRender::QColorMask *>(state);
                if (mask) {
                    foundColorMask = true;
                    QVERIFY(mask->isRedMasked());
                    QVERIFY(mask->isGreenMasked());
                    QVERIFY(mask->isBlueMasked());
                    QVERIFY(!mask->isAlphaMasked());
                }
                foundDepthMask = foundDepthMask || qobject_cast<const Qt3DRender::QNoDepthMask *>(state);
            }
        }
    }
    QVERIFY(foundColorMask);
    QVERIFY(foundDepthMask);
}

void TestSceneView3D::testFreeCameraWheelChangesSpeed()
{
    Qt3DRender::QCamera camera;
    flatlas::rendering::FreeCameraController freeCam(&camera);
    freeCam.setEnabled(true);
    freeCam.setSpeed(1000.0f);

    QWheelEvent faster(QPointF(10.0, 10.0), QPointF(10.0, 10.0),
                       QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                       Qt::NoScrollPhase, false, Qt::MouseEventNotSynthesized,
                       QPointingDevice::primaryPointingDevice());
    freeCam.handleWheel(&faster);
    QVERIFY(freeCam.speed() > 1000.0f);

    QWheelEvent slower(QPointF(10.0, 10.0), QPointF(10.0, 10.0),
                       QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier,
                       Qt::NoScrollPhase, false, Qt::MouseEventNotSynthesized,
                       QPointingDevice::primaryPointingDevice());
    freeCam.handleWheel(&slower);
    QVERIFY(freeCam.speed() < 1250.0f);
}

void TestSceneView3D::testFreeCameraKeyboardMovement()
{
    Qt3DRender::QCamera camera;
    camera.setPosition(QVector3D(0.0f, 0.0f, 0.0f));
    camera.setViewCenter(QVector3D(0.0f, 0.0f, 1.0f));

    flatlas::rendering::FreeCameraController freeCam(&camera);
    freeCam.setEnabled(true);
    freeCam.setSpeed(1000.0f);

    QKeyEvent pressForward(QEvent::KeyPress, Qt::Key_W, Qt::NoModifier);
    freeCam.handleKeyPress(&pressForward);
    freeCam.update(1.0f);
    QKeyEvent releaseForward(QEvent::KeyRelease, Qt::Key_W, Qt::NoModifier);
    freeCam.handleKeyRelease(&releaseForward);
    QVERIFY(camera.position().z() > 900.0f);

    QKeyEvent pressRight(QEvent::KeyPress, Qt::Key_D, Qt::NoModifier);
    freeCam.handleKeyPress(&pressRight);
    freeCam.update(1.0f);
    const float xAfterRight = camera.position().x();
    QVERIFY(qAbs(xAfterRight) > 900.0f);

    QKeyEvent releaseRight(QEvent::KeyRelease, Qt::Key_D, Qt::NoModifier);
    freeCam.handleKeyRelease(&releaseRight);
    const float xBeforeLeft = camera.position().x();
    QKeyEvent pressLeft(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier);
    freeCam.handleKeyPress(&pressLeft);
    freeCam.update(1.0f);
    QVERIFY(qAbs(camera.position().x()) < qAbs(xBeforeLeft));
}

void TestSceneView3D::testFreeCameraMouseLook()
{
    Qt3DRender::QCamera camera;
    camera.setPosition(QVector3D(0.0f, 0.0f, 0.0f));
    camera.setViewCenter(QVector3D(0.0f, 0.0f, 1.0f));

    flatlas::rendering::FreeCameraController freeCam(&camera);
    freeCam.setEnabled(true);
    const QVector3D originalView = camera.viewVector();

    QMouseEvent press(QEvent::MouseButtonPress,
                      QPointF(10.0, 10.0), QPointF(10.0, 10.0),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier,
                      QPointingDevice::primaryPointingDevice());
    freeCam.handleMousePress(&press);

    QMouseEvent move(QEvent::MouseMove,
                     QPointF(60.0, 0.0), QPointF(60.0, 0.0),
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier,
                     QPointingDevice::primaryPointingDevice());
    freeCam.handleMouseMove(&move);

    QVERIFY(camera.viewVector() != originalView);
    QVERIFY(camera.viewVector().y() > originalView.y());
}

#endif // FLATLAS_HAS_QT3D

QTEST_MAIN(TestSceneView3D)
#include "test_SceneView3D.moc"
