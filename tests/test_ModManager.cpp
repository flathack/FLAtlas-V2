// test_ModManager.cpp – Phase 13 Mod Manager tests

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "editors/modmanager/ConflictDetector.h"
#include "editors/modmanager/ModExportService.h"
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
    void testModExportDetectsNewAndModifiedOnly();
    void testModExportIgnoresFlatlasRuntimeFiles();
    void testModExportZipAndFlmod();
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

void TestModManager::testModExportDetectsNewAndModifiedOnly()
{
    QTemporaryDir referenceDir;
    QTemporaryDir modDir;
    QVERIFY(referenceDir.isValid());
    QVERIFY(modDir.isValid());

    createFile(referenceDir.filePath("DATA/EQUIPMENT/goods.ini"), "old");
    createFile(modDir.filePath("DATA/EQUIPMENT/goods.ini"), "new");
    createFile(referenceDir.filePath("DATA/EQUIPMENT/market.ini"), "same");
    createFile(modDir.filePath("DATA/EQUIPMENT/market.ini"), "same");
    createFile(modDir.filePath("DATA/UNIVERSE/new_system.ini"), "new system");

    const ModExportPlan plan = ModExportService::collectChangedFiles(modDir.path(), referenceDir.path());
    QCOMPARE(plan.exportFiles().size(), 2);
    QCOMPARE(plan.newCount(), 1);
    QCOMPARE(plan.modifiedCount(), 1);
    QCOMPARE(plan.unchangedCount, 1);
}

void TestModManager::testModExportIgnoresFlatlasRuntimeFiles()
{
    QTemporaryDir referenceDir;
    QTemporaryDir modDir;
    QVERIFY(referenceDir.isValid());
    QVERIFY(modDir.isValid());

    createFile(modDir.filePath(".flatlas/history.json"), "{}");
    createFile(modDir.filePath(".FLAtlasLauncher/state.json"), "{}");
    createFile(modDir.filePath("FLAtlas-Change.log"), "history");
    createFile(modDir.filePath("ReShade.log"), "runtime");

    const ModExportPlan plan = ModExportService::collectChangedFiles(modDir.path(), referenceDir.path());
    QCOMPARE(plan.exportFiles().size(), 0);
}

void TestModManager::testModExportZipAndFlmod()
{
    QTemporaryDir referenceDir;
    QTemporaryDir modDir;
    QTemporaryDir outDir;
    QVERIFY(referenceDir.isValid());
    QVERIFY(modDir.isValid());
    QVERIFY(outDir.isValid());

    createFile(referenceDir.filePath("DATA/a.ini"), "old");
    createFile(modDir.filePath("DATA/a.ini"), "new");
    createFile(modDir.filePath("script.xml"), "<script><data method=\"copyfile\" /></script>");

    const ModExportPlan plan = ModExportService::collectChangedFiles(modDir.path(), referenceDir.path());
    const QString zipPath = outDir.filePath("export.zip");
    QString error;
    QVERIFY2(ModExportService::writeZip(plan, zipPath, &error), qPrintable(error));
    QFile zip(zipPath);
    QVERIFY(zip.open(QIODevice::ReadOnly));
    const QByteArray zipBytes = zip.readAll();
    QVERIFY(zipBytes.contains("DATA/a.ini"));
    QVERIFY(zipBytes.contains("FLAtlas-export-manifest.json"));

    QSet<QString> excluded;
    excluded.insert(QStringLiteral("script.xml"));
    const ModExportPlan filtered = ModExportService::filterPlan(plan, excluded);
    const QString flmodPath = outDir.filePath("export.flmod");
    const QString scriptXml = ModExportService::defaultScriptXml(QStringLiteral("Test Mod"),
                                                                 QStringLiteral("Tester"),
                                                                 QStringLiteral("Description"),
                                                                 true);
    QVERIFY2(ModExportService::writeFlmod(filtered, flmodPath, scriptXml, &error), qPrintable(error));
    QFile flmod(flmodPath);
    QVERIFY(flmod.open(QIODevice::ReadOnly));
    const QByteArray flmodBytes = flmod.readAll();
    QVERIFY(flmodBytes.contains("script.xml"));
    QVERIFY(flmodBytes.contains("DATA/a.ini"));
    QVERIFY(flmodBytes.contains("Test Mod"));
}

QTEST_GUILESS_MAIN(TestModManager)
#include "test_ModManager.moc"
