#include <QtTest/QtTest>

#include "editors/universe/DeleteSystemService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

using namespace flatlas::editors;

namespace {

void writeTextFile(const QString &path, const QString &text)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate), qPrintable(path));
    file.write(text.toUtf8());
}

QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

QString makeUniverseIni(const QString &extraBaseSection = QString())
{
    QString text =
        "[System]\n"
        "nickname = AA01\n"
        "file = systems\\AA01\\AA01.ini\n"
        "\n"
        "[System]\n"
        "nickname = BB01\n"
        "file = systems\\BB01\\BB01.ini\n";
    if (!extraBaseSection.trimmed().isEmpty())
        text += QStringLiteral("\n") + extraBaseSection;
    return text;
}

} // namespace

class TestDeleteSystemService : public QObject
{
    Q_OBJECT

private slots:
    void precheck_handles_shared_fields_and_jump_cleanup()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString root = dir.path();
        const QString universeIni = root + "/DATA/UNIVERSE/universe.ini";
        const QString aaIni = root + "/DATA/UNIVERSE/systems/AA01/AA01.ini";
        const QString bbIni = root + "/DATA/UNIVERSE/systems/BB01/BB01.ini";
        const QString uniqueAst = root + "/DATA/solar/asteroids/aa_field.ini";
        const QString sharedNeb = root + "/DATA/solar/nebula/shared_cloud.ini";

        writeTextFile(universeIni, makeUniverseIni());
        writeTextFile(aaIni,
                      "[SystemInfo]\n"
                      "nickname = AA01\n"
                      "\n"
                      "[Asteroids]\n"
                      "file = solar\\asteroids\\aa_field.ini\n"
                      "zone = Zone_AA_field\n"
                      "\n"
                      "[Nebula]\n"
                      "file = solar\\nebula\\shared_cloud.ini\n"
                      "zone = Zone_AA_neb\n");
        writeTextFile(bbIni,
                      "[SystemInfo]\n"
                      "nickname = BB01\n"
                      "\n"
                      "[Nebula]\n"
                      "file = solar\\nebula\\shared_cloud.ini\n"
                      "zone = Zone_BB_neb\n"
                      "\n"
                      "[Object]\n"
                      "nickname = BB01_to_AA01\n"
                      "archetype = jumpgate\n"
                      "goto = AA01, AA01_to_BB01, gate_tunnel_bretonia\n"
                      "\n"
                      "[Zone]\n"
                      "nickname = Zone_BB01_to_AA01\n"
                      "shape = SPHERE\n"
                      "size = 1000\n");
        writeTextFile(uniqueAst, "[Field]\nnickname = aa_field\n");
        writeTextFile(sharedNeb, "[Nebula]\nnickname = shared_cloud\n");

        const DeleteSystemPrecheckReport report = DeleteSystemService::precheck(universeIni, nullptr, "AA01");
        QVERIFY(report.precheckCompleted);
        QVERIFY(report.canDelete);
        QCOMPARE(report.jumpCleanups.size(), 1);

        bool sawUniqueAstDelete = false;
        bool sawSharedNebKeep = false;
        for (const auto &entry : report.filesToDelete) {
            if (entry.path.endsWith("solar/asteroids/aa_field.ini", Qt::CaseInsensitive))
                sawUniqueAstDelete = true;
        }
        for (const auto &entry : report.filesToKeep) {
            if (entry.path.endsWith("solar/nebula/shared_cloud.ini", Qt::CaseInsensitive))
                sawSharedNebKeep = true;
        }
        QVERIFY(sawUniqueAstDelete);
        QVERIFY(sawSharedNebKeep);
    }

    void precheck_blocks_external_base_file()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString root = dir.path();
        const QString universeIni = root + "/DATA/UNIVERSE/universe.ini";
        const QString aaIni = root + "/DATA/UNIVERSE/systems/AA01/AA01.ini";
        const QString bbIni = root + "/DATA/UNIVERSE/systems/BB01/BB01.ini";
        const QString externalBase = root + "/DATA/BASES/shared_base.ini";

        writeTextFile(universeIni,
                      makeUniverseIni(
                          "[Base]\n"
                          "nickname = AA01_01_Base\n"
                          "system = AA01\n"
                          "file = BASES\\shared_base.ini\n"));
        writeTextFile(aaIni,
                      "[SystemInfo]\n"
                      "nickname = AA01\n"
                      "\n"
                      "[Object]\n"
                      "nickname = AA01_01\n"
                      "base = AA01_01_Base\n"
                      "dock_with = AA01_01_Base\n");
        writeTextFile(bbIni, "[SystemInfo]\nnickname = BB01\n");
        writeTextFile(externalBase, "[BaseInfo]\nnickname = AA01_01_Base\n");

        const DeleteSystemPrecheckReport report = DeleteSystemService::precheck(universeIni, nullptr, "AA01");
        QVERIFY(report.precheckCompleted);
        QVERIFY(!report.canDelete);
        QVERIFY(!report.blockers.isEmpty());
    }

    void execute_removes_system_bases_folder_and_jump_counterparts()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString root = dir.path();
        const QString universeIni = root + "/DATA/UNIVERSE/universe.ini";
        const QString aaIni = root + "/DATA/UNIVERSE/systems/AA01/AA01.ini";
        const QString bbIni = root + "/DATA/UNIVERSE/systems/BB01/BB01.ini";
        const QString baseIni = root + "/DATA/UNIVERSE/systems/AA01/Bases/AA01_01_Base.ini";
        const QString roomIni = root + "/DATA/UNIVERSE/systems/AA01/Bases/Rooms/AA01_01_Bar.ini";
        const QString uniqueNeb = root + "/DATA/solar/nebula/aa_cloud.ini";

        writeTextFile(universeIni,
                      makeUniverseIni(
                          "[Base]\n"
                          "nickname = AA01_01_Base\n"
                          "system = AA01\n"
                          "file = Universe\\Systems\\AA01\\Bases\\AA01_01_Base.ini\n"));
        writeTextFile(aaIni,
                      "[SystemInfo]\n"
                      "nickname = AA01\n"
                      "\n"
                      "[Nebula]\n"
                      "file = solar\\nebula\\aa_cloud.ini\n"
                      "zone = Zone_AA_cloud\n"
                      "\n"
                      "[Object]\n"
                      "nickname = AA01_01\n"
                      "base = AA01_01_Base\n"
                      "dock_with = AA01_01_Base\n");
        writeTextFile(bbIni,
                      "[SystemInfo]\n"
                      "nickname = BB01\n"
                      "\n"
                      "[Object]\n"
                      "nickname = BB01_to_AA01\n"
                      "archetype = jumpgate\n"
                      "goto = AA01, AA01_to_BB01, gate_tunnel_bretonia\n"
                      "\n"
                      "[Zone]\n"
                      "nickname = Zone_BB01_to_AA01\n"
                      "shape = SPHERE\n"
                      "size = 1000\n");
        writeTextFile(baseIni,
                      "[BaseInfo]\n"
                      "nickname = AA01_01_Base\n"
                      "start_room = Bar\n"
                      "\n"
                      "[Room]\n"
                      "nickname = Bar\n"
                      "file = Universe\\Systems\\AA01\\Bases\\Rooms\\AA01_01_Bar.ini\n");
        writeTextFile(roomIni, "[Room_Info]\nscene = ambient, foo.thn\n");
        writeTextFile(uniqueNeb, "[Nebula]\nnickname = aa_cloud\n");

        const DeleteSystemPrecheckReport report = DeleteSystemService::precheck(universeIni, nullptr, "AA01");
        QVERIFY(report.canDelete);

        QString errorMessage;
        QVERIFY2(DeleteSystemService::execute(report, nullptr, &errorMessage), qPrintable(errorMessage));

        const QString updatedUniverse = readTextFile(universeIni);
        QVERIFY(!updatedUniverse.contains("nickname = AA01\n"));
        QVERIFY(!updatedUniverse.contains("nickname = AA01_01_Base\n"));
        QVERIFY(!QFileInfo::exists(root + "/DATA/UNIVERSE/systems/AA01"));
        QVERIFY(!QFileInfo::exists(uniqueNeb));

        const QString updatedBb = readTextFile(bbIni);
        QVERIFY(!updatedBb.contains("BB01_to_AA01"));
        QVERIFY(!updatedBb.contains("Zone_BB01_to_AA01"));
    }
};

QTEST_GUILESS_MAIN(TestDeleteSystemService)
#include "test_DeleteSystemService.moc"
