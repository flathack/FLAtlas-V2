// test_FlightMode.cpp – Phase 15 Flight Mode tests

#include <QtTest/QtTest>
#include <QVector3D>

#include "rendering/flight/FlightController.h"
#include "rendering/flight/FlightCamera.h"
#include "rendering/flight/FlightHud.h"
#include "rendering/flight/Autopilot.h"
#include "rendering/flight/DustParticles.h"
#include "rendering/flight/FlightNavigation.h"

using namespace flatlas::rendering;

class TestFlightMode : public QObject {
    Q_OBJECT
private slots:
    void testFlightControllerStates();
    void testFlightControllerPhysics();
    void testFlightControllerSteering();
    void testFlightCameraFollow();
    void testAutopilotGoto();
    void testAutopilotDock();
    void testFlightNavigation();
    void testFlightNavigationCycle();
    void testFlightNavigationDockRange();
    void testDustParticlesBasic();
    void testFlightHudCreation();
};

void TestFlightMode::testFlightControllerStates()
{
    FlightController ctrl;
    QCOMPARE(ctrl.state(), FlightController::Docked);

    ctrl.undock();
    QCOMPARE(ctrl.state(), FlightController::Normal);

    ctrl.toggleCruise();
    QCOMPARE(ctrl.state(), FlightController::Cruise);

    ctrl.toggleCruise();
    QCOMPARE(ctrl.state(), FlightController::Normal);

    ctrl.dock();
    QCOMPARE(ctrl.state(), FlightController::Docked);

    // Can't toggle cruise while docked
    ctrl.toggleCruise();
    QCOMPARE(ctrl.state(), FlightController::Docked);
}

void TestFlightMode::testFlightControllerPhysics()
{
    FlightController ctrl;
    ctrl.undock();
    ctrl.setThrottle(1.0f);
    ctrl.setForward(QVector3D(0, 0, -1));

    // Simulate several frames
    for (int i = 0; i < 100; ++i)
        ctrl.update(0.016f); // ~60fps

    // Should have moved forward (negative Z)
    QVERIFY(ctrl.position().z() < -10.0f);
    QVERIFY(ctrl.speed() > 0.0f);
    QVERIFY(ctrl.speed() <= ctrl.maxSpeed() + 1.0f);
}

void TestFlightMode::testFlightControllerSteering()
{
    FlightController ctrl;
    ctrl.undock();
    ctrl.setForward(QVector3D(0, 0, -1));

    // Steer right
    ctrl.setSteering(1.0f, 0.0f);
    ctrl.update(0.5f);

    // Forward vector should have rotated (X component changed)
    QVERIFY(qAbs(ctrl.forward().x()) > 0.01f);
}

void TestFlightMode::testFlightCameraFollow()
{
    FlightController ctrl;
    ctrl.undock();
    ctrl.setPosition(QVector3D(1000, 0, 0));
    ctrl.setForward(QVector3D(0, 0, -1));

    FlightCamera cam;
    cam.setController(&ctrl);

    // Update several frames
    for (int i = 0; i < 60; ++i)
        cam.update(0.016f);

    // Camera should be behind the ship (positive Z relative to ship)
    QVERIFY(cam.cameraPosition().z() > ctrl.position().z());
    // Camera should be above the ship
    QVERIFY(cam.cameraPosition().y() > ctrl.position().y());
}

void TestFlightMode::testAutopilotGoto()
{
    FlightController ctrl;
    ctrl.undock();
    ctrl.setPosition(QVector3D(0, 0, 0));
    ctrl.setForward(QVector3D(0, 0, -1));
    ctrl.setMaxSpeed(300.0f);

    Autopilot ap;
    ap.setController(&ctrl);

    QVector3D target(0, 0, -10000);
    ap.gotoTarget(target, "TestStation");
    QCOMPARE(ap.mode(), Autopilot::Goto);

    // Simulate flight
    for (int i = 0; i < 2000; ++i) {
        ap.update(0.016f);
        ctrl.update(0.016f);
    }

    // Should have moved towards target
    float dist = (ctrl.position() - target).length();
    QVERIFY2(dist < ap.distanceToTarget() + 10000.0f,
             qPrintable(QStringLiteral("dist=%1").arg(dist)));
}

void TestFlightMode::testAutopilotDock()
{
    FlightController ctrl;
    ctrl.undock();
    ctrl.setPosition(QVector3D(0, 0, 0));
    ctrl.setForward(QVector3D(1, 0, 0));
    ctrl.setMaxSpeed(300.0f);

    Autopilot ap;
    ap.setController(&ctrl);

    QVector3D dockPos(500, 0, 0);
    ap.dockAt(dockPos, "Base01");
    QCOMPARE(ap.mode(), Autopilot::DockApproach);

    // Simulate
    bool arrived = false;
    QObject::connect(&ap, &Autopilot::arrivedAtTarget, [&](const QString &name) {
        arrived = true;
        QCOMPARE(name, QStringLiteral("Base01"));
    });

    for (int i = 0; i < 3000 && !arrived; ++i) {
        ap.update(0.016f);
        ctrl.update(0.016f);
    }

    QVERIFY(arrived);
    QCOMPARE(ctrl.state(), FlightController::Docked);
}

void TestFlightMode::testFlightNavigation()
{
    FlightNavigation nav;

    QVector<NavPoint> points = {
        {QStringLiteral("station01"), QStringLiteral("Manhattan"), NavPoint::Station,
         QVector3D(1000, 0, 0), {}},
        {QStringLiteral("gate_ny_co"), QStringLiteral("NY-CO Gate"), NavPoint::JumpGate,
         QVector3D(-5000, 0, 3000), QStringLiteral("co01")},
        {QStringLiteral("tl_ring_01"), QStringLiteral("TL Ring 1"), NavPoint::TradeLane,
         QVector3D(2000, 0, -1000), {}},
    };
    nav.setNavPoints(points);

    QCOMPARE(nav.navPointCount(), 3);
    QCOMPARE(nav.activeTarget(), 0);

    // Find by nickname
    QCOMPARE(nav.findNavPoint("gate_ny_co"), 1);
    QCOMPARE(nav.findNavPoint("nonexistent"), -1);

    // Nearest
    int nearest = nav.nearestNavPoint(QVector3D(900, 0, 0));
    QCOMPARE(nearest, 0); // station01
}

void TestFlightMode::testFlightNavigationCycle()
{
    FlightNavigation nav;
    QVector<NavPoint> points = {
        {QStringLiteral("a"), {}, NavPoint::Station, {}, {}},
        {QStringLiteral("b"), {}, NavPoint::JumpGate, {}, {}},
    };
    nav.setNavPoints(points);

    QCOMPARE(nav.activeTarget(), 0);
    nav.cycleTarget();
    QCOMPARE(nav.activeTarget(), 1);
    nav.cycleTarget();
    QCOMPARE(nav.activeTarget(), 0); // wraps
}

void TestFlightMode::testFlightNavigationDockRange()
{
    FlightNavigation nav;
    QVector<NavPoint> points = {
        {QStringLiteral("station01"), {}, NavPoint::Station, QVector3D(0, 0, 0), {}},
        {QStringLiteral("tl01"), {}, NavPoint::TradeLane, QVector3D(100, 0, 0), {}},
    };
    nav.setNavPoints(points);

    // In range of station
    QVERIFY(nav.isInDockRange(QVector3D(100, 0, 0), 500.0f));
    auto *np = nav.dockableNavPoint(QVector3D(100, 0, 0), 500.0f);
    QVERIFY(np != nullptr);
    QCOMPARE(np->nickname, QStringLiteral("station01"));

    // Trade lanes are not dockable
    auto *np2 = nav.dockableNavPoint(QVector3D(100, 0, 0), 50.0f);
    // Only station in range at 50m? station is 100m away
    QVERIFY(np2 == nullptr);
}

void TestFlightMode::testDustParticlesBasic()
{
    DustParticles dust;
    FlightController ctrl;
    ctrl.undock();
    dust.setController(&ctrl);

    QCOMPARE(dust.particleCount(), 60);
    QCOMPARE(dust.activeParticles(), 0);

    // Without Qt3D entities, update should not crash
    ctrl.setThrottle(1.0f);
    for (int i = 0; i < 10; ++i)
        ctrl.update(0.016f);
    dust.update(0.016f);
}

void TestFlightMode::testFlightHudCreation()
{
    FlightHud hud;
    FlightController ctrl;
    hud.setController(&ctrl);
    hud.setTargetName("Manhattan");
    hud.setTargetPosition(QVector3D(1000, 0, 0));
    hud.setMessage("Welcome to New York", 5000);
    hud.resize(800, 600);
    // tick should not crash
    hud.tick();
}

QTEST_MAIN(TestFlightMode)
#include "test_FlightMode.moc"
