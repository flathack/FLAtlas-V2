#include <QtTest>
#include <QTemporaryDir>
#include <QFile>

#include "editors/system/BaseEditService.h"
#include "domain/SystemDocument.h"
#include "domain/SolarObject.h"

using namespace flatlas::editors;
using namespace flatlas::domain;

class TestBaseEditService : public QObject {
    Q_OBJECT
private slots:
    void createStagesFilesAndObject();
    void loadTemplateStateReadsRoomsAndNpcs();
    void archetypeDefaultsPreferExistingBaseObject();
    void createNormalizesDisplayedReputationNickname();
    void createFromTemplateCopiesRoomContentAndNpcData();
};

static void writeTextFile(const QString &path, const QString &text)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
    file.write(text.toUtf8());
}

void TestBaseEditService::createStagesFilesAndObject()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString gameRoot = dir.path();
    QVERIFY(QDir().mkpath(dir.path() + "/DATA/UNIVERSE"));
    QVERIFY(QDir().mkpath(dir.path() + "/DATA/UNIVERSE/SYSTEMS/LI01"));
    QVERIFY(QDir().mkpath(dir.path() + "/DATA/MISSIONS"));

    const QString systemPath = dir.path() + "/DATA/UNIVERSE/SYSTEMS/LI01/li01.ini";
    const QString universePath = dir.path() + "/DATA/UNIVERSE/universe.ini";
    const QString mbasesPath = dir.path() + "/DATA/MISSIONS/mbases.ini";
    writeTextFile(systemPath, "[SystemInfo]\nnickname = Li01\n");
    writeTextFile(universePath, "[Base]\nnickname = Li01_01_Base\nsystem = Li01\nstrid_name = 0\nfile = Universe\\Systems\\LI01\\Bases\\li01_01_base.ini\n");
    writeTextFile(mbasesPath, QString());

    SystemDocument document;
    document.setName("Li01");
    document.setFilePath(systemPath);

    BaseEditState state = BaseEditService::makeCreateState(document, gameRoot);
    state.displayName.clear();
    state.infocardXml.clear();
    for (BaseRoomState &room : state.rooms) {
        if (room.roomName.compare("Bar", Qt::CaseInsensitive) == 0) {
            room.enabled = true;
            BaseRoomNpcState bartender;
            bartender.nickname = QStringLiteral("test_bartender");
            bartender.role = QStringLiteral("bartender");
            room.npcs.append(bartender);
            BaseRoomNpcState newsVendor;
            newsVendor.nickname = QStringLiteral("test_news");
            newsVendor.role = QStringLiteral("NewsVendor");
            room.npcs.append(newsVendor);
        }
    }

    BaseApplyResult result;
    QString errorMessage;
    QVERIFY2(BaseEditService::applyCreate(state, QPointF(100.0, 200.0), gameRoot, {}, &result, &errorMessage), qPrintable(errorMessage));
    QVERIFY(result.createdObject != nullptr);
    QCOMPARE(result.createdObject->nickname(), state.objectNickname);
    QCOMPARE(result.createdObject->base(), state.baseNickname);
    QVERIFY(result.stagedWrites.size() >= 4);

    const QVector<QPair<QString, QString>> rawEntries = result.createdObject->rawEntries();
    QStringList orderedKeys;
    for (const auto &entry : rawEntries)
        orderedKeys.append(entry.first);
    QVERIFY(orderedKeys.indexOf(QStringLiteral("nickname")) == 0);
    QVERIFY(orderedKeys.indexOf(QStringLiteral("pos")) == 1);
    QVERIFY(orderedKeys.indexOf(QStringLiteral("rotate")) == 2);
    QCOMPARE(rawEntries.at(2).second.trimmed(), QStringLiteral("0, 0, 0"));

    bool sawUniverse = false;
    bool sawBaseIni = false;
    bool sawMbase = false;
    bool sawRoom = false;
    for (const BaseStagedWrite &write : result.stagedWrites) {
        if (write.absolutePath.endsWith("universe.ini", Qt::CaseInsensitive)) {
            sawUniverse = true;
            QVERIFY(write.content.contains(state.baseNickname));
        } else if (write.absolutePath.endsWith("mbases.ini", Qt::CaseInsensitive)) {
            sawMbase = true;
            QVERIFY(write.content.contains("[MBase]"));
            QVERIFY(write.content.contains("diff = 1"));
            QVERIFY(write.content.contains(QStringLiteral("msg_id_prefix = gcs_refer_base_%1").arg(state.baseNickname)));
            QVERIFY(write.content.contains("num_offers = 0, 0"));
            QVERIFY(write.content.contains("fixture = test_bartender"));
            QVERIFY(write.content.contains("fixture = test_news"));
        } else if (write.absolutePath.endsWith("_base.ini", Qt::CaseInsensitive)) {
            sawBaseIni = true;
            QVERIFY(write.content.contains("[BaseInfo]"));
            QVERIFY(write.content.contains("[Room]"));
        } else if (write.absolutePath.endsWith(".ini", Qt::CaseInsensitive)) {
            sawRoom = true;
            QVERIFY(write.content.contains("[Room_Info]"));
        }
    }

    QVERIFY(sawUniverse);
    QVERIFY(sawBaseIni);
    QVERIFY(sawMbase);
    QVERIFY(sawRoom);
}

void TestBaseEditService::loadTemplateStateReadsRoomsAndNpcs()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString gameRoot = dir.path();
    QVERIFY(QDir().mkpath(dir.path() + "/DATA/UNIVERSE/SYSTEMS/LI01/BASES/ROOMS"));
    QVERIFY(QDir().mkpath(dir.path() + "/DATA/MISSIONS"));

    writeTextFile(dir.path() + "/DATA/UNIVERSE/universe.ini",
                  "[Base]\n"
                  "nickname = Li01_01_Base\n"
                  "system = Li01\n"
                  "file = Universe\\Systems\\LI01\\BASES\\li01_01_base.ini\n"
                  "BGCS_base_run_by = li_grp\n");
    writeTextFile(dir.path() + "/DATA/UNIVERSE/SYSTEMS/LI01/BASES/li01_01_base.ini",
                  "[BaseInfo]\n"
                  "start_room = Bar\n"
                  "price_variance = 0.30\n\n"
                  "[Room]\n"
                  "nickname = Bar\n"
                  "file = Universe\\Systems\\LI01\\BASES\\ROOMS\\li01_01_bar.ini\n\n"
                  "[Room]\n"
                  "nickname = Deck\n"
                  "file = Universe\\Systems\\LI01\\BASES\\ROOMS\\li01_01_deck.ini\n");
    writeTextFile(dir.path() + "/DATA/UNIVERSE/SYSTEMS/LI01/BASES/ROOMS/li01_01_bar.ini",
                  "[Room_Info]\n"
                  "scene = all, ambient, Scripts\\Bases\\Li_09_bar_ambi_int_s020x.thn\n");
    writeTextFile(dir.path() + "/DATA/UNIVERSE/SYSTEMS/LI01/BASES/ROOMS/li01_01_deck.ini",
                  "[Room_Info]\n"
                  "scene = all, ambient, Scripts\\Bases\\Li_08_Deck_ambi_int_01.thn\n");
    writeTextFile(dir.path() + "/DATA/MISSIONS/mbases.ini",
                  "[MBase]\n"
                  "nickname = Li01_01_Base\n\n"
                  "[MRoom]\n"
                  "nickname = Bar\n"
                  "fixture = bar_npc, scripts\\vendors\\li_host_fidget.thn, stand, bartender\n\n"
                  "[MRoom]\n"
                  "nickname = Deck\n"
                  "fixture = deck_npc, scripts\\vendors\\li_commtrader_fidget.thn, stand, trader\n");

    BaseEditState state;
    QString errorMessage;
    QVERIFY2(BaseEditService::loadTemplateState(QStringLiteral("Li01_01_Base"), gameRoot, {}, &state, &errorMessage), qPrintable(errorMessage));
    QCOMPARE(state.bgcsBaseRunBy, QStringLiteral("li_grp"));
    QCOMPARE(state.startRoom, QStringLiteral("Bar"));
    QCOMPARE(state.rooms.size(), 2);
    QCOMPARE(state.rooms.at(0).roomName, QStringLiteral("Bar"));
    QCOMPARE(state.rooms.at(0).npcs.size(), 1);
    QCOMPARE(state.rooms.at(0).npcs.at(0).nickname, QStringLiteral("bar_npc"));
}

void TestBaseEditService::archetypeDefaultsPreferExistingBaseObject()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString gameRoot = dir.path();
    QVERIFY(QDir().mkpath(dir.path() + "/DATA/UNIVERSE/SYSTEMS/LI01"));
    QVERIFY(QDir().mkpath(dir.path() + "/DATA/SOLAR"));

    writeTextFile(dir.path() + "/DATA/UNIVERSE/universe.ini",
                  "[System]\n"
                  "nickname = Li01\n"
                  "file = Universe\\Systems\\LI01\\li01.ini\n");
    writeTextFile(dir.path() + "/DATA/UNIVERSE/SYSTEMS/LI01/li01.ini",
                  "[Object]\n"
                  "nickname = li01_01\n"
                  "base = Li01_01_Base\n"
                  "archetype = station\n"
                  "loadout = loadout_from_existing\n"
                  "ids_info = 12345\n");
    writeTextFile(dir.path() + "/DATA/SOLAR/solararch.ini",
                  "[Solar]\n"
                  "nickname = station\n"
                  "loadout = loadout_from_solararch\n");

    QHash<QString, QString> textOverrides;
    textOverrides.insert(QDir::cleanPath(dir.path() + "/DATA/UNIVERSE/SYSTEMS/LI01/li01.ini").toLower(),
                         QStringLiteral("[Object]\n"
                                        "nickname = li01_01\n"
                                        "base = Li01_01_Base\n"
                                        "archetype = station\n"
                                        "loadout = loadout_from_existing\n"
                                        "ids_info = 12345\n"));
    textOverrides.insert(QDir::cleanPath(dir.path() + "/DATA/UNIVERSE/universe.ini").toLower(),
                         QStringLiteral("[System]\n"
                                        "nickname = Li01\n"
                                        "file = Universe\\Systems\\LI01\\li01.ini\n"));

    const BaseArchetypeDefaults defaults = BaseEditService::archetypeDefaults(QStringLiteral("station"), gameRoot, textOverrides);
    QCOMPARE(defaults.loadout, QStringLiteral("loadout_from_existing"));
    QCOMPARE(defaults.sourceBaseNickname, QStringLiteral("Li01_01_Base"));
}

void TestBaseEditService::createNormalizesDisplayedReputationNickname()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString gameRoot = dir.path();
    QVERIFY(QDir().mkpath(dir.path() + "/DATA/UNIVERSE"));
    QVERIFY(QDir().mkpath(dir.path() + "/DATA/UNIVERSE/SYSTEMS/LI01"));
    QVERIFY(QDir().mkpath(dir.path() + "/DATA/MISSIONS"));
    QVERIFY(QDir().mkpath(dir.path() + "/DATA/SOLAR"));

    const QString systemPath = dir.path() + "/DATA/UNIVERSE/SYSTEMS/LI01/li01.ini";
    writeTextFile(systemPath, "[SystemInfo]\nnickname = Li01\n");
    writeTextFile(dir.path() + "/DATA/UNIVERSE/universe.ini",
                  "[Base]\n"
                  "nickname = Li01_01_Base\n"
                  "system = Li01\n"
                  "strid_name = 0\n"
                  "file = Universe\\Systems\\LI01\\Bases\\li01_01_base.ini\n");
    writeTextFile(dir.path() + "/DATA/MISSIONS/mbases.ini", QString());
    writeTextFile(dir.path() + "/DATA/SOLAR/solararch.ini",
                  "[Solar]\n"
                  "nickname = station\n"
                  "loadout = station_loadout\n");

    SystemDocument document;
    document.setName("Li01");
    document.setFilePath(systemPath);

    BaseEditState state = BaseEditService::makeCreateState(document, gameRoot);
    state.displayName.clear();
    state.infocardXml.clear();
    state.reputation = QStringLiteral("li_n_grp - Liberty Navy");

    BaseApplyResult result;
    QString errorMessage;
    QVERIFY2(BaseEditService::applyCreate(state, QPointF(0.0, 0.0), gameRoot, {}, &result, &errorMessage), qPrintable(errorMessage));
    QVERIFY(result.createdObject != nullptr);

    const QVector<QPair<QString, QString>> rawEntries = result.createdObject->rawEntries();
    QString reputationValue;
    for (const auto &entry : rawEntries) {
        if (entry.first.compare(QStringLiteral("reputation"), Qt::CaseInsensitive) == 0) {
            reputationValue = entry.second.trimmed();
            break;
        }
    }
    QCOMPARE(reputationValue, QStringLiteral("li_n_grp"));

    bool sawNormalizedFaction = false;
    for (const BaseStagedWrite &write : result.stagedWrites) {
        if (!write.absolutePath.endsWith("mbases.ini", Qt::CaseInsensitive))
            continue;
        QVERIFY(!write.content.contains(QStringLiteral("li_n_grp - Liberty Navy")));
        QVERIFY(write.content.contains(QStringLiteral("local_faction = li_n_grp")));
        sawNormalizedFaction = true;
    }
    QVERIFY(sawNormalizedFaction);
}

void TestBaseEditService::createFromTemplateCopiesRoomContentAndNpcData()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString gameRoot = dir.path();
    QVERIFY(QDir().mkpath(dir.path() + "/DATA/UNIVERSE/SYSTEMS/HI01/BASES/ROOMS"));
    QVERIFY(QDir().mkpath(dir.path() + "/DATA/UNIVERSE/SYSTEMS/HI01/BASES"));
    QVERIFY(QDir().mkpath(dir.path() + "/DATA/UNIVERSE/SYSTEMS/LI01/BASES/ROOMS"));
    QVERIFY(QDir().mkpath(dir.path() + "/DATA/UNIVERSE/SYSTEMS/LI01/BASES"));
    QVERIFY(QDir().mkpath(dir.path() + "/DATA/MISSIONS"));
    QVERIFY(QDir().mkpath(dir.path() + "/DATA/SOLAR"));

    const QString hi01SystemPath = dir.path() + "/DATA/UNIVERSE/SYSTEMS/HI01/hi01.ini";
    writeTextFile(hi01SystemPath, "[SystemInfo]\nnickname = Hi01\n");
    writeTextFile(dir.path() + "/DATA/UNIVERSE/universe.ini",
                  "[Base]\n"
                  "nickname = Li01_03_Base\n"
                  "system = Li01\n"
                  "file = Universe\\Systems\\LI01\\BASES\\li01_03_base.ini\n");
    writeTextFile(dir.path() + "/DATA/UNIVERSE/SYSTEMS/LI01/BASES/li01_03_base.ini",
                  "[BaseInfo]\n"
                  "nickname = Li01_03_Base\n"
                  "start_room = Deck\n"
                  "price_variance = 0.30\n\n"
                  "[Room]\n"
                  "nickname = Bar\n"
                  "file = Universe\\Systems\\LI01\\BASES\\ROOMS\\li01_03_bar.ini\n\n"
                  "[Room]\n"
                  "nickname = Deck\n"
                  "file = Universe\\Systems\\LI01\\BASES\\ROOMS\\li01_03_deck.ini\n\n"
                  "[Room]\n"
                  "nickname = ShipDealer\n"
                  "file = Universe\\Systems\\LI01\\BASES\\ROOMS\\li01_03_shipdealer.ini\n");
    writeTextFile(dir.path() + "/DATA/UNIVERSE/SYSTEMS/LI01/BASES/ROOMS/li01_03_bar.ini",
                  "[Room_Info]\n"
                  "set_script = Scripts\\Bases\\Li_05_Bar_hardpoint_01.thn\n"
                  "scene = all, ambient, Scripts\\Bases\\Li_05_Bar_ambi_int_01.thn\n\n"
                  "[Hotspot]\n"
                  "name = IDS_HOTSPOT_DECK\n"
                  "behavior = ExitDoor\n"
                  "room_switch = Deck\n\n"
                  "[Hotspot]\n"
                  "name = IDS_HOTSPOT_BAR\n"
                  "behavior = ExitDoor\n"
                  "room_switch = Bar\n\n"
                  "[Hotspot]\n"
                  "name = IDS_HOTSPOT_COMMODITYTRADER_ROOM\n"
                  "behavior = ExitDoor\n"
                  "room_switch = Deck\n"
                  "set_virtual_room = Trader\n\n"
                  "[Hotspot]\n"
                  "name = IDS_HOTSPOT_EQUIPMENTDEALER_ROOM\n"
                  "behavior = ExitDoor\n"
                  "room_switch = Deck\n"
                  "set_virtual_room = Equipment\n\n"
                  "[Hotspot]\n"
                  "name = IDS_HOTSPOT_SHIPDEALER_ROOM\n"
                  "behavior = ExitDoor\n"
                  "room_switch = ShipDealer\n\n"
                  "[Hotspot]\n"
                  "name = IDS_HOTSPOT_NEWSVENDOR\n"
                  "behavior = NewsVendor\n\n"
                  "[Hotspot]\n"
                  "name = IDS_HOTSPOT_MISSIONVENDOR\n"
                  "behavior = MissionVendor\n");
    writeTextFile(dir.path() + "/DATA/UNIVERSE/SYSTEMS/LI01/BASES/ROOMS/li01_03_deck.ini",
                  "[Room_Info]\n"
                  "set_script = Scripts\\Bases\\Li_08_Deck_hardpoint_01.thn\n"
                  "scene = all, ambient, Scripts\\Bases\\Li_08_Deck_ambi_int_01.thn\n");
    writeTextFile(dir.path() + "/DATA/UNIVERSE/SYSTEMS/LI01/BASES/ROOMS/li01_03_shipdealer.ini",
                  "[Room_Info]\n"
                  "set_script = Scripts\\Bases\\Li_01_shipdealer_hardpoint_01.thn\n"
                  "scene = all, ambient, Scripts\\Bases\\Li_01_shipdealer_ambi_int_01.thn\n");
    writeTextFile(dir.path() + "/DATA/MISSIONS/mbases.ini",
                  "[MBase]\n"
                  "nickname = Li01_03_Base\n"
                  "local_faction = li_n_grp\n"
                  "diff = 1\n"
                  "msg_id_prefix = gcs_refer_base_Li01_03_Base\n\n"
                  "[MVendor]\n"
                  "num_offers = 2, 4\n\n"
                  "[BaseFaction]\n"
                  "faction = li_n_grp\n"
                  "weight = 64\n"
                  "npc = li0103_lnavy_001_m\n\n"
                  "[GF_NPC]\n"
                  "nickname = li0103_lnavy_001_m\n"
                  "body = li_male_guard_body\n"
                  "head = ge_male3_head\n"
                  "lefthand = benchmark_male_hand_left\n"
                  "righthand = benchmark_male_hand_right\n"
                  "individual_name = 220010\n"
                  "affiliation = li_n_grp\n"
                  "voice = rvp111\n"
                  "room = bar\n\n"
                  "[MRoom]\n"
                  "nickname = Bar\n"
                  "character_density = 7\n"
                  "fixture = li0103_lnavy_001_m, Zs/NPC/Bartender/01/A/Stand, scripts\\vendors\\li_host_fidget.thn, bartender\n");
    writeTextFile(dir.path() + "/DATA/SOLAR/solararch.ini",
                  "[Solar]\n"
                  "nickname = station\n"
                  "loadout = station_loadout\n");

    BaseEditState templateState;
    QString errorMessage;
    QVERIFY2(BaseEditService::loadTemplateState(QStringLiteral("Li01_03_Base"), gameRoot, {}, &templateState, &errorMessage), qPrintable(errorMessage));

    SystemDocument document;
    document.setName("Hi01");
    document.setFilePath(hi01SystemPath);
    BaseEditState createState = BaseEditService::makeCreateState(document, gameRoot);
    createState.displayName.clear();
    createState.infocardXml.clear();
    createState.reputation = QStringLiteral("fc_or_grp");
    createState.rooms = BaseEditService::applyTemplateRoomsForCreate(createState, templateState, true);
    createState.startRoom = templateState.startRoom;
    createState.priceVariance = templateState.priceVariance;

    BaseApplyResult result;
    QVERIFY2(BaseEditService::applyCreate(createState, QPointF(0.0, 0.0), gameRoot, {}, &result, &errorMessage), qPrintable(errorMessage));

    bool sawRoomCopy = false;
    bool sawNpcCopy = false;
    for (const BaseStagedWrite &write : result.stagedWrites) {
        if (write.absolutePath.endsWith("_bar.ini", Qt::CaseInsensitive)) {
            sawRoomCopy = true;
            QVERIFY(write.content.contains("Li_05_Bar_hardpoint_01.thn"));
            QVERIFY(write.content.contains("set_virtual_room = Trader"));
            QVERIFY(write.content.contains("set_virtual_room = Equipment"));
            QVERIFY(write.content.contains("room_switch = ShipDealer"));
            QVERIFY(write.content.contains("name = IDS_HOTSPOT_NEWSVENDOR"));
            QVERIFY(write.content.contains("name = IDS_HOTSPOT_MISSIONVENDOR"));
            const int deckIndex = write.content.indexOf("name = IDS_HOTSPOT_DECK");
            const int barIndex = write.content.indexOf("name = IDS_HOTSPOT_BAR");
            const int traderIndex = write.content.indexOf("name = IDS_HOTSPOT_COMMODITYTRADER_ROOM");
            const int equipmentIndex = write.content.indexOf("name = IDS_HOTSPOT_EQUIPMENTDEALER_ROOM");
            const int shipDealerIndex = write.content.indexOf("name = IDS_HOTSPOT_SHIPDEALER_ROOM");
            QVERIFY(deckIndex >= 0);
            QVERIFY(barIndex > deckIndex);
            QVERIFY(traderIndex > barIndex);
            QVERIFY(equipmentIndex > traderIndex);
            QVERIFY(shipDealerIndex > equipmentIndex);
            QVERIFY(!write.content.contains("Li01_03_Base"));
        }
        if (write.absolutePath.endsWith("_deck.ini", Qt::CaseInsensitive)) {
            const int launchpadIndex = write.content.indexOf("name = IDS_HOTSPOT_DECK");
            const int barIndex = write.content.indexOf("name = IDS_HOTSPOT_BAR");
            const int traderIndex = write.content.indexOf("name = IDS_HOTSPOT_COMMODITYTRADER_ROOM");
            const int equipmentIndex = write.content.indexOf("name = IDS_HOTSPOT_EQUIPMENTDEALER_ROOM");
            const int shipDealerIndex = write.content.indexOf("name = IDS_HOTSPOT_SHIPDEALER_ROOM");
            QVERIFY(launchpadIndex >= 0);
            QVERIFY(barIndex > launchpadIndex);
            QVERIFY(traderIndex > barIndex);
            QVERIFY(equipmentIndex > traderIndex);
            QVERIFY(shipDealerIndex > equipmentIndex);
            QCOMPARE(write.content.count("name = IDS_HOTSPOT_DECK"), 1);
            QCOMPARE(write.content.count("behavior = ExitDoor\nroom_switch = Deck"), 1);
        }
        if (write.absolutePath.endsWith("mbases.ini", Qt::CaseInsensitive)) {
            sawNpcCopy = true;
            QVERIFY(write.content.contains("[GF_NPC]"));
            QVERIFY(write.content.contains("[BaseFaction]"));
            const QString newBlockAnchor = QStringLiteral("nickname = %1").arg(createState.baseNickname);
            const int newBlockStart = write.content.indexOf(newBlockAnchor);
            QVERIFY(newBlockStart >= 0);
            const QString createdBlock = write.content.mid(newBlockStart);
            QVERIFY(createdBlock.contains("hi01_01_base_bar_npc_01"));
            QVERIFY(createdBlock.contains("voice = rvp111"));
            QVERIFY(!createdBlock.contains("li0103_lnavy_001_m"));
        }
    }

    QVERIFY(sawRoomCopy);
    QVERIFY(sawNpcCopy);
}

QTEST_MAIN(TestBaseEditService)
#include "test_BaseEditService.moc"
