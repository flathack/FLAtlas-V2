// test_SpStarter.cpp – Phase 21 Scripting/Patching tests
// Note: ScriptPatcher binary-patch functions are tested manually to avoid
// Windows Defender false-positive blocking. This test covers SpStarter.

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "tools/SpStarter.h"

using namespace flatlas::tools;

class TestSpStarter : public QObject {
    Q_OBJECT
private slots:
    void testStarterLaunchInvalidPath();
    void testStarterLaunchWindowedInvalidPath();
    void testStarterIsFreelancerExe();
    void testStarterIsFreelancerExeCaseInsensitive();
    void testStarterIsFreelancerExeNonExistent();
    void testStarterIsFreelancerExeWrongName();
    void testStarterIsFreelancerExeSubdir();
};

void TestSpStarter::testStarterLaunchInvalidPath()
{
    QVERIFY(!SpStarter::launch(QStringLiteral("nonexistent_path/something")));
}

void TestSpStarter::testStarterLaunchWindowedInvalidPath()
{
    QVERIFY(!SpStarter::launchWindowed(QStringLiteral("nonexistent"), 1024, 768));
}

void TestSpStarter::testStarterIsFreelancerExe()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString flExe = dir.path() + "/Freelancer.exe";
    QFile f(flExe);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("test");
    f.close();
    QVERIFY(SpStarter::isFreelancerExe(flExe));
}

void TestSpStarter::testStarterIsFreelancerExeCaseInsensitive()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString flExe = dir.path() + "/freelancer.EXE";
    QFile f(flExe);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("test");
    f.close();
    QVERIFY(SpStarter::isFreelancerExe(flExe));
}

void TestSpStarter::testStarterIsFreelancerExeNonExistent()
{
    QVERIFY(!SpStarter::isFreelancerExe(QStringLiteral("nonexistent")));
}

void TestSpStarter::testStarterIsFreelancerExeWrongName()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString other = dir.path() + "/notepad.exe";
    QFile f(other);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("test");
    f.close();
    QVERIFY(!SpStarter::isFreelancerExe(other));
}

void TestSpStarter::testStarterIsFreelancerExeSubdir()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Freelancer.exe in a subdirectory
    QDir(dir.path()).mkdir("EXE");
    const QString flExe = dir.path() + "/EXE/Freelancer.exe";
    QFile f(flExe);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("test");
    f.close();
    QVERIFY(SpStarter::isFreelancerExe(flExe));
}

QTEST_MAIN(TestSpStarter)
#include "test_SpStarter.moc"
