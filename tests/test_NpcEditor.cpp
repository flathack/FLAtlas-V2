// test_NpcEditor.cpp – Phase 14 NPC Editor tests

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>

#include "editors/npc/MbaseOperations.h"
#include "editors/npc/NpcEditorPage.h"

using namespace flatlas::editors;

class TestNpcEditor : public QObject {
    Q_OBJECT
private slots:
    void testParseMbases();
    void testSerializeRoundtrip();
    void testNpcsForBase();
    void testEmptyFile();
    void testEditorPageCreation();
};

static QString sampleMbases()
{
    return QStringLiteral(
        "[MBase]\n"
        "nickname = li01_01_base\n"
        "\n"
        "[MRoom]\n"
        "nickname = bar\n"
        "fixture = npc_bartender, stand, Fixer\n"
        "\n"
        "[GF_NPC]\n"
        "nickname = npc_bartender\n"
        "room = bar\n"
        "affiliation = li_navy\n"
        "individual_name = 12345\n"
        "npc_type = bartender\n"
        "\n"
        "[GF_NPC]\n"
        "nickname = npc_dealer\n"
        "room = bar\n"
        "affiliation = li_traders\n"
        "npc_type = mission_giver\n"
        "\n"
        "[MBase]\n"
        "nickname = li02_01_base\n"
        "\n"
        "[GF_NPC]\n"
        "nickname = npc_guard\n"
        "room = lobby\n"
        "affiliation = li_police\n"
    );
}

void TestNpcEditor::testParseMbases()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString path = dir.filePath("mbases.ini");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(sampleMbases().toUtf8());
    f.close();

    auto bases = MbaseOperations::parseFile(path);
    QCOMPARE(bases.size(), 2);

    // First base
    QCOMPARE(bases[0].baseName, QStringLiteral("li01_01_base"));
    QCOMPARE(bases[0].rooms.size(), 1);
    QCOMPARE(bases[0].rooms[0].nickname, QStringLiteral("bar"));
    QCOMPARE(bases[0].npcs.size(), 2);

    QCOMPARE(bases[0].npcs[0].nickname, QStringLiteral("npc_bartender"));
    QCOMPARE(bases[0].npcs[0].faction, QStringLiteral("li_navy"));
    QCOMPARE(bases[0].npcs[0].idsName, 12345);
    QCOMPARE(bases[0].npcs[0].type, QStringLiteral("bartender"));

    QCOMPARE(bases[0].npcs[1].nickname, QStringLiteral("npc_dealer"));

    // Second base
    QCOMPARE(bases[1].baseName, QStringLiteral("li02_01_base"));
    QCOMPARE(bases[1].npcs.size(), 1);
}

void TestNpcEditor::testSerializeRoundtrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Parse
    QString path = dir.filePath("mbases.ini");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(sampleMbases().toUtf8());
    f.close();

    auto bases = MbaseOperations::parseFile(path);

    // Serialize
    QString serialized = MbaseOperations::serialize(bases);
    QVERIFY(serialized.contains(QStringLiteral("li01_01_base")));
    QVERIFY(serialized.contains(QStringLiteral("npc_bartender")));
    QVERIFY(serialized.contains(QStringLiteral("li_navy")));

    // Write serialized and re-parse
    QString path2 = dir.filePath("mbases2.ini");
    QFile f2(path2);
    QVERIFY(f2.open(QIODevice::WriteOnly | QIODevice::Text));
    f2.write(serialized.toUtf8());
    f2.close();

    auto bases2 = MbaseOperations::parseFile(path2);
    QCOMPARE(bases2.size(), 2);
    QCOMPARE(bases2[0].npcs.size(), 2);
    QCOMPARE(bases2[0].npcs[0].nickname, QStringLiteral("npc_bartender"));
}

void TestNpcEditor::testNpcsForBase()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString path = dir.filePath("mbases.ini");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(sampleMbases().toUtf8());
    f.close();

    auto bases = MbaseOperations::parseFile(path);
    auto npcs = MbaseOperations::npcsForBase(bases, QStringLiteral("li01_01_base"));
    QCOMPARE(npcs.size(), 2);

    auto npcs2 = MbaseOperations::npcsForBase(bases, QStringLiteral("nonexistent"));
    QCOMPARE(npcs2.size(), 0);
}

void TestNpcEditor::testEmptyFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString path = dir.filePath("empty.ini");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.close();

    auto bases = MbaseOperations::parseFile(path);
    QCOMPARE(bases.size(), 0);
}

void TestNpcEditor::testEditorPageCreation()
{
    NpcEditorPage page;
    QCOMPARE(page.baseCount(), 0);
    QCOMPARE(page.npcCount(), 0);
}

QTEST_MAIN(TestNpcEditor)
#include "test_NpcEditor.moc"
