// test_ExeHandling.cpp – Phase 20 PE version info & debug capture tests

#include <QtTest/QtTest>
#include <QCoreApplication>
#include "infrastructure/io/PeVersionInfo.h"
#include "infrastructure/io/DebugCapture.h"
#include "infrastructure/io/DllResources.h"

using namespace flatlas::infrastructure;

class TestExeHandling : public QObject {
    Q_OBJECT
private slots:
    void testPeVersionInfoInvalid();
    void testPeVersionInfoOnSelf();
    void testGetExeVersionDelegates();
    void testDebugCaptureInitial();
    void testDebugCaptureStartInvalid();
    void testDebugCaptureStopSafe();
    void testDebugCaptureClearLines();
    void testPeVersionInfoComponents();
};

void TestExeHandling::testPeVersionInfoInvalid()
{
    PeVersionInfo info(QStringLiteral("nonexistent_file.exe"));
    QVERIFY(!info.isValid());
    QVERIFY(info.fileVersion().isEmpty());
    QCOMPARE(info.majorVersion(), 0);
}

void TestExeHandling::testPeVersionInfoOnSelf()
{
    // Read version from our own test executable (built by Qt/CMake).
    // Qt executables may or may not have version resources,
    // but the code path should not crash.
    const QString self = QCoreApplication::applicationFilePath();
    PeVersionInfo info(self);
    // May or may not be valid depending on whether version info was embedded,
    // but we verify it doesn't crash and returns consistent data.
    if (info.isValid()) {
        QVERIFY(!info.fileVersion().isEmpty());
        QVERIFY(info.majorVersion() >= 0);
    }
}

void TestExeHandling::testGetExeVersionDelegates()
{
    // getExeVersion on invalid file should return empty
    const QString ver = DllResources::getExeVersion(QStringLiteral("no_such.exe"));
    QVERIFY(ver.isEmpty());
}

void TestExeHandling::testDebugCaptureInitial()
{
    DebugCapture cap;
    QVERIFY(!cap.isRunning());
    QVERIFY(cap.capturedLines().isEmpty());
}

void TestExeHandling::testDebugCaptureStartInvalid()
{
    DebugCapture cap;
    // Starting a nonexistent executable should fail
    bool ok = cap.start(QStringLiteral("nonexistent_program_xyz_123"));
    QVERIFY(!ok);
    QVERIFY(!cap.isRunning());
}

void TestExeHandling::testDebugCaptureStopSafe()
{
    DebugCapture cap;
    // Stopping when nothing is running should be safe
    cap.stop();
    QVERIFY(!cap.isRunning());
}

void TestExeHandling::testDebugCaptureClearLines()
{
    DebugCapture cap;
    cap.clearLines();
    QVERIFY(cap.capturedLines().isEmpty());
}

void TestExeHandling::testPeVersionInfoComponents()
{
    // Test that an invalid file returns zeroed components
    PeVersionInfo info(QStringLiteral("nonexistent.dll"));
    QCOMPARE(info.majorVersion(), 0);
    QCOMPARE(info.minorVersion(), 0);
    QCOMPARE(info.patchVersion(), 0);
    QCOMPARE(info.buildNumber(), 0);
    QVERIFY(info.productName().isEmpty());
    QVERIFY(info.companyName().isEmpty());
    QVERIFY(info.fileDescription().isEmpty());
}

QTEST_MAIN(TestExeHandling)
#include "test_ExeHandling.moc"
