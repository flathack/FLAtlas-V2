#include <QtTest>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include "editors/base/BasePersistence.h"
#include "editors/base/BaseBuilder.h"
#include "editors/base/BaseEditorPage.h"
#include "domain/BaseData.h"

using namespace flatlas::editors;
using namespace flatlas::domain;

class TestBaseEditor : public QObject {
    Q_OBJECT
private slots:

    // ─── BasePersistence ──────────────────────────────────

    void loadNonExistent()
    {
        auto base = BasePersistence::loadFromIni(QStringLiteral("nonexistent.ini"),
                                                  QStringLiteral("foo"));
        QVERIFY(!base);
    }

    void loadSingleBase()
    {
        QTemporaryFile file;
        file.setAutoRemove(true);
        QVERIFY(file.open());
        file.write("[Object]\n"
                   "nickname = Li01_01_Base\n"
                   "archetype = space_station\n"
                   "ids_name = 12345\n"
                   "ids_info = 12346\n"
                   "pos = 100, 200, 300\n"
                   "dock_with = Li01_01_Base\n"
                   "base = Li01_01_Base\n"
                   "\n"
                   "[BaseRoom]\n"
                   "base = Li01_01_Base\n"
                   "nickname = Li01_01_Base_bar\n"
                   "type = Bar\n"
                   "\n"
                   "[BaseRoom]\n"
                   "base = Li01_01_Base\n"
                   "nickname = Li01_01_Base_equip\n"
                   "type = Equip\n");
        file.close();

        auto base = BasePersistence::loadFromIni(file.fileName(),
                                                  QStringLiteral("Li01_01_Base"));
        QVERIFY(base);
        QCOMPARE(base->nickname, QStringLiteral("Li01_01_Base"));
        QCOMPARE(base->archetype, QStringLiteral("space_station"));
        QCOMPARE(base->idsName, 12345);
        QCOMPARE(base->idsInfo, 12346);
        QCOMPARE(base->position.x(), 100.0f);
        QCOMPARE(base->position.y(), 200.0f);
        QCOMPARE(base->position.z(), 300.0f);
        QCOMPARE(base->rooms.size(), 2);
        QCOMPARE(base->rooms[0].type, QStringLiteral("Bar"));
        QCOMPARE(base->rooms[1].type, QStringLiteral("Equip"));
    }

    void loadNonExistentBase()
    {
        QTemporaryFile file;
        file.setAutoRemove(true);
        QVERIFY(file.open());
        file.write("[Object]\n"
                   "nickname = some_other\n"
                   "archetype = nothing\n");
        file.close();

        auto base = BasePersistence::loadFromIni(file.fileName(),
                                                  QStringLiteral("missing_base"));
        QVERIFY(!base);
    }

    void loadAllBases()
    {
        QTemporaryFile file;
        file.setAutoRemove(true);
        QVERIFY(file.open());
        file.write("[Object]\n"
                   "nickname = Base_A\n"
                   "dock_with = Base_A\n"
                   "\n"
                   "[Object]\n"
                   "nickname = Base_B\n"
                   "dock_with = Base_B\n"
                   "\n"
                   "[Object]\n"
                   "nickname = NonBase\n"
                   "archetype = asteroid\n"
                   "\n"
                   "[BaseRoom]\n"
                   "base = Base_A\n"
                   "nickname = Base_A_bar\n"
                   "type = Bar\n");
        file.close();

        auto bases = BasePersistence::loadAllBases(file.fileName());
        QCOMPARE(bases.size(), 2); // Only Base_A and Base_B have dock_with
        QCOMPARE(bases[0].nickname, QStringLiteral("Base_A"));
        QCOMPARE(bases[0].rooms.size(), 1);
        QCOMPARE(bases[1].nickname, QStringLiteral("Base_B"));
        QCOMPARE(bases[1].rooms.size(), 0);
    }

    void saveAndReload()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString filePath = dir.path() + "/test_base.ini";

        BaseData base;
        base.nickname = "TestBase";
        base.archetype = "space_station";
        base.system = "Li01";
        base.idsName = 100;
        base.position = QVector3D(1000, 0, -500);

        RoomData room1;
        room1.nickname = "TestBase_bar";
        room1.type = "Bar";
        base.rooms.append(room1);

        RoomData room2;
        room2.nickname = "TestBase_equip";
        room2.type = "Equip";
        base.rooms.append(room2);

        QVERIFY(BasePersistence::save(base, filePath));

        auto loaded = BasePersistence::loadFromIni(filePath, QStringLiteral("TestBase"));
        QVERIFY(loaded);
        QCOMPARE(loaded->nickname, QStringLiteral("TestBase"));
        QCOMPARE(loaded->archetype, QStringLiteral("space_station"));
        QCOMPARE(loaded->system, QStringLiteral("Li01"));
        QCOMPARE(loaded->idsName, 100);
        QCOMPARE(loaded->rooms.size(), 2);
        QCOMPARE(loaded->rooms[0].type, QStringLiteral("Bar"));
        QCOMPARE(loaded->rooms[1].type, QStringLiteral("Equip"));
    }

    // ─── BaseBuilder ─────────────────────────────────────

    void templateNames()
    {
        auto names = BaseBuilder::templateNames();
        QCOMPARE(names.size(), 4);
        QVERIFY(names.contains(QStringLiteral("Station")));
        QVERIFY(names.contains(QStringLiteral("Planet")));
        QVERIFY(names.contains(QStringLiteral("Outpost")));
        QVERIFY(names.contains(QStringLiteral("Wreck")));
    }

    void createStation()
    {
        auto base = BaseBuilder::create(BaseTemplate::Station,
                                         QStringLiteral("MyStation"),
                                         QStringLiteral("Li01"));
        QCOMPARE(base.nickname, QStringLiteral("MyStation"));
        QCOMPARE(base.system, QStringLiteral("Li01"));
        QCOMPARE(base.archetype, QStringLiteral("space_station"));
        QCOMPARE(base.rooms.size(), 4); // Bar, Equip, Commodity, ShipDealer
        QCOMPARE(base.rooms[0].nickname, QStringLiteral("MyStation_bar"));
        QCOMPARE(base.rooms[0].type, QStringLiteral("Bar"));
    }

    void createPlanet()
    {
        auto base = BaseBuilder::create(BaseTemplate::Planet,
                                         QStringLiteral("Earth"),
                                         QStringLiteral("Sol"));
        QCOMPARE(base.rooms.size(), 3); // Bar, Equip, Commodity
        QCOMPARE(base.archetype, QStringLiteral("planet"));
    }

    void createOutpost()
    {
        auto base = BaseBuilder::create(BaseTemplate::Outpost,
                                         QStringLiteral("Post1"),
                                         QStringLiteral("Li02"));
        QCOMPARE(base.rooms.size(), 2); // Bar, Equip
    }

    void createWreck()
    {
        auto base = BaseBuilder::create(BaseTemplate::Wreck,
                                         QStringLiteral("Wreck1"),
                                         QStringLiteral("Li03"));
        QCOMPARE(base.rooms.size(), 0);
        QCOMPARE(base.archetype, QStringLiteral("space_wreck"));
    }

    void templateFromName()
    {
        QCOMPARE(BaseBuilder::templateFromName(QStringLiteral("Station")),
                 BaseTemplate::Station);
        QCOMPARE(BaseBuilder::templateFromName(QStringLiteral("planet")),
                 BaseTemplate::Planet);
        QCOMPARE(BaseBuilder::templateFromName(QStringLiteral("outpost")),
                 BaseTemplate::Outpost);
        QCOMPARE(BaseBuilder::templateFromName(QStringLiteral("WRECK")),
                 BaseTemplate::Wreck);
        // Unknown defaults to Station
        QCOMPARE(BaseBuilder::templateFromName(QStringLiteral("unknown")),
                 BaseTemplate::Station);
    }

    // ─── BaseEditorPage (smoke) ──────────────────────────

    void editorPageDefault()
    {
        flatlas::editors::BaseEditorPage page;
        QVERIFY(page.data() == nullptr);
        QVERIFY(page.baseNickname().isEmpty());
    }

    void editorPageSetBase()
    {
        flatlas::editors::BaseEditorPage page;
        auto base = std::make_unique<BaseData>();
        base->nickname = "Test";
        base->archetype = "space_station";

        QSignalSpy spy(&page, &flatlas::editors::BaseEditorPage::titleChanged);

        page.setBase(std::move(base));
        QVERIFY(page.data() != nullptr);
        QCOMPARE(page.baseNickname(), QStringLiteral("Test"));
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TestBaseEditor)
#include "test_BaseEditor.moc"
