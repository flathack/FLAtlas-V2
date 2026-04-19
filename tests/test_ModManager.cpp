// test_ModManager.cpp – Phase 13 Mod Manager tests

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "editors/modmanager/ConflictDetector.h"
#include "editors/modmanager/ModWorkflow.h"
#include "domain/ModProfile.h"

using namespace flatlas::editors;
using namespace flatlas::domain;

class TestModManager : public QObject {
    Q_OBJECT
private slots:
    void testScanMods();
    void testDetectConflicts();
    void testNoConflicts();
    void testModWorkflowActivateDeactivate();
    void testProfileSaveLoad();
};

static void createFile(const QString &path, const QString &content = QStringLiteral("test"))
{
    QFileInfo info(path);
    QDir().mkpath(info.path());
    QFile f(path);
    f.open(QIODevice::WriteOnly | QIODevice::Text);
    f.write(content.toUtf8());
}

void TestModManager::testScanMods()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Create two mod directories with files
    createFile(dir.filePath("ModA/data/equipment.ini"));
    createFile(dir.filePath("ModA/data/ships.ini"));
    createFile(dir.filePath("ModB/data/universe.ini"));

    ConflictDetector detector;
    auto mods = detector.scanMods(dir.path());
    QCOMPARE(mods.size(), 2);

    // Find ModA
    bool foundA = false, foundB = false;
    for (const auto &m : mods) {
        if (m.name == "ModA") { foundA = true; QCOMPARE(m.files.size(), 2); }
        if (m.name == "ModB") { foundB = true; QCOMPARE(m.files.size(), 1); }
    }
    QVERIFY(foundA);
    QVERIFY(foundB);
}

void TestModManager::testDetectConflicts()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Both mods have data/equipment.ini
    createFile(dir.filePath("ModA/data/equipment.ini"));
    createFile(dir.filePath("ModB/data/equipment.ini"));
    createFile(dir.filePath("ModB/data/ships.ini"));

    ConflictDetector detector;
    auto mods = detector.scanMods(dir.path());
    auto conflicts = detector.detectConflicts(mods);

    QCOMPARE(conflicts.size(), 1);
    QVERIFY(conflicts[0].relativePath.contains("equipment.ini"));
    QCOMPARE(conflicts[0].modNames.size(), 2);
}

void TestModManager::testNoConflicts()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    createFile(dir.filePath("ModA/data/a.ini"));
    createFile(dir.filePath("ModB/data/b.ini"));

    ConflictDetector detector;
    auto mods = detector.scanMods(dir.path());
    auto conflicts = detector.detectConflicts(mods);

    QCOMPARE(conflicts.size(), 0);
}

void TestModManager::testModWorkflowActivateDeactivate()
{
    QTemporaryDir gameDir;
    QTemporaryDir modDir;
    QTemporaryDir backupDir;
    QVERIFY(gameDir.isValid());
    QVERIFY(modDir.isValid());
    QVERIFY(backupDir.isValid());

    // Create original game file
    createFile(gameDir.filePath("data/equipment.ini"), "original");

    // Create mod file
    createFile(modDir.filePath("data/equipment.ini"), "modded");

    ModWorkflow workflow;
    workflow.setGamePath(gameDir.path());
    workflow.setBackupPath(backupDir.path());

    // Scan mod
    ConflictDetector detector;
    auto mod = detector.scanSingleMod(modDir.path());

    // Activate
    auto copied = workflow.activateMod(mod);
    QCOMPARE(copied.size(), 1);

    // Verify game file is now modded
    QFile gameFile(gameDir.filePath("data/equipment.ini"));
    QVERIFY(gameFile.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(gameFile.readAll()), QStringLiteral("modded"));
    gameFile.close();

    // Deactivate
    auto restored = workflow.deactivateMod(mod);
    QCOMPARE(restored.size(), 1);

    // Verify game file is restored
    QVERIFY(gameFile.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(gameFile.readAll()), QStringLiteral("original"));
}

void TestModManager::testProfileSaveLoad()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    ModProfile profile;
    profile.name = QStringLiteral("Test Profile");
    profile.id = QStringLiteral("test-123");
    profile.gamePath = QStringLiteral("C:/Games/Freelancer");
    profile.activeMods = {QStringLiteral("ModA"), QStringLiteral("ModB")};
    profile.isActive = true;

    QString path = dir.filePath("profile.json");

    ModWorkflow workflow;
    QVERIFY(workflow.saveProfile(profile, path));

    auto loaded = workflow.loadProfile(path);
    QCOMPARE(loaded.name, profile.name);
    QCOMPARE(loaded.id, profile.id);
    QCOMPARE(loaded.gamePath, profile.gamePath);
    QCOMPARE(loaded.activeMods.size(), 2);
    QCOMPARE(loaded.isActive, true);
}

QTEST_GUILESS_MAIN(TestModManager)
#include "test_ModManager.moc"
