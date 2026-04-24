#include <QtTest>

#include "domain/SystemDocument.h"
#include "editors/system/SystemPersistence.h"
#include "editors/system/SystemSettingsService.h"

using namespace flatlas::domain;
using namespace flatlas::editors;
using namespace flatlas::infrastructure;

class TestSystemSettingsService : public QObject
{
    Q_OBJECT

private slots:
    void loadsSystemSections();
    void appliesAndPreservesUnrelatedEntries();
    void clearsControlledKeysWhenEmpty();
    void normalizesRgbText();
    void parsesFactionDisplay();
};

void TestSystemSettingsService::loadsSystemSections()
{
    SystemDocument document;
    document.setName(QStringLiteral("Li01"));

    IniSection systemInfo;
    systemInfo.name = QStringLiteral("SystemInfo");
    systemInfo.entries = {
        {QStringLiteral("nickname"), QStringLiteral("Li01")},
        {QStringLiteral("space_color"), QStringLiteral("10, 20, 30")},
        {QStringLiteral("local_faction"), QStringLiteral("li_n_grp")},
    };
    SystemPersistence::setSystemInfoSection(&document, systemInfo);

    IniDocument extras;
    extras.append({QStringLiteral("Music"), {{QStringLiteral("space"), QStringLiteral("music_space")},
                                              {QStringLiteral("danger"), QStringLiteral("music_danger")},
                                              {QStringLiteral("battle"), QStringLiteral("music_battle")}}});
    extras.append({QStringLiteral("Ambient"), {{QStringLiteral("color"), QStringLiteral("1, 2, 3")}}});
    extras.append({QStringLiteral("Dust"), {{QStringLiteral("spacedust"), QStringLiteral("radioactivedust")}}});
    extras.append({QStringLiteral("Background"), {{QStringLiteral("basic_stars"), QStringLiteral("solar\\starsphere\\basic.cmp")},
                                                   {QStringLiteral("complex_stars"), QStringLiteral("solar\\starsphere\\complex.3db")},
                                                   {QStringLiteral("nebulae"), QStringLiteral("solar\\starsphere\\nebula.cmp")}}});
    SystemPersistence::setExtraSections(&document, extras);

    const SystemSettingsState state = SystemSettingsService::load(&document);
    QCOMPARE(state.systemNickname, QStringLiteral("Li01"));
    QCOMPARE(state.musicSpace, QStringLiteral("music_space"));
    QCOMPARE(state.musicDanger, QStringLiteral("music_danger"));
    QCOMPARE(state.musicBattle, QStringLiteral("music_battle"));
    QCOMPARE(state.spaceColor, QStringLiteral("10, 20, 30"));
    QCOMPARE(state.localFaction, QStringLiteral("li_n_grp"));
    QCOMPARE(state.ambientColor, QStringLiteral("1, 2, 3"));
    QCOMPARE(state.dust, QStringLiteral("radioactivedust"));
    QCOMPARE(state.backgroundBasicStars, QStringLiteral("solar\\starsphere\\basic.cmp"));
    QCOMPARE(state.backgroundComplexStars, QStringLiteral("solar\\starsphere\\complex.3db"));
    QCOMPARE(state.backgroundNebulae, QStringLiteral("solar\\starsphere\\nebula.cmp"));
}

void TestSystemSettingsService::appliesAndPreservesUnrelatedEntries()
{
    SystemDocument document;

    IniSection systemInfo;
    systemInfo.name = QStringLiteral("SystemInfo");
    systemInfo.entries = {
        {QStringLiteral("nickname"), QStringLiteral("Li01")},
        {QStringLiteral("space_color"), QStringLiteral("1, 1, 1")},
        {QStringLiteral("local_faction"), QStringLiteral("old_grp")},
        {QStringLiteral("space_farclip"), QStringLiteral("60000")},
    };
    SystemPersistence::setSystemInfoSection(&document, systemInfo);

    IniDocument extras;
    extras.append({QStringLiteral("Music"), {{QStringLiteral("space"), QStringLiteral("old_space")},
                                              {QStringLiteral("danger"), QStringLiteral("old_danger")},
                                              {QStringLiteral("battle"), QStringLiteral("old_battle")},
                                              {QStringLiteral("unrelated_music_key"), QStringLiteral("keep_me")}}});
    extras.append({QStringLiteral("Ambient"), {{QStringLiteral("color"), QStringLiteral("5, 5, 5")},
                                                {QStringLiteral("oddity"), QStringLiteral("preserve")}}});
    extras.append({QStringLiteral("Dust"), {{QStringLiteral("spacedust"), QStringLiteral("old_dust")},
                                             {QStringLiteral("note"), QStringLiteral("keep")}}});
    extras.append({QStringLiteral("Background"), {{QStringLiteral("basic_stars"), QStringLiteral("old_basic")},
                                                   {QStringLiteral("complex_stars"), QStringLiteral("old_complex")},
                                                   {QStringLiteral("nebulae"), QStringLiteral("old_nebulae")},
                                                   {QStringLiteral("odd_bg"), QStringLiteral("keep_bg")}}});
    extras.append({QStringLiteral("EncounterParameters"), {{QStringLiteral("nickname"), QStringLiteral("untouched")}}});
    SystemPersistence::setExtraSections(&document, extras);

    SystemSettingsState state;
    state.spaceColor = QStringLiteral("10,20,30");
    state.localFaction = QStringLiteral("rh_n_grp - Rheinland Navy");
    state.musicSpace = QStringLiteral("new_space");
    state.musicDanger = QStringLiteral("new_danger");
    state.musicBattle = QStringLiteral("new_battle");
    state.ambientColor = QStringLiteral("40, 50, 60");
    state.dust = QStringLiteral("new_dust");
    state.backgroundBasicStars = QStringLiteral("new_basic");
    state.backgroundComplexStars = QStringLiteral("new_complex");
    state.backgroundNebulae = QStringLiteral("new_nebulae");

    QString errorMessage;
    QVERIFY2(SystemSettingsService::apply(&document, state, &errorMessage), qPrintable(errorMessage));
    QVERIFY(document.isDirty());

    const IniSection storedSystemInfo = SystemPersistence::systemInfoSection(&document);
    QCOMPARE(storedSystemInfo.value(QStringLiteral("space_color")), QStringLiteral("10, 20, 30"));
    QCOMPARE(storedSystemInfo.value(QStringLiteral("local_faction")), QStringLiteral("rh_n_grp"));
    QCOMPARE(storedSystemInfo.value(QStringLiteral("space_farclip")), QStringLiteral("60000"));

    const IniDocument storedExtras = SystemPersistence::extraSections(&document);
    auto findSection = [&storedExtras](const QString &name) -> IniSection {
        for (const IniSection &section : storedExtras) {
            if (section.name.compare(name, Qt::CaseInsensitive) == 0)
                return section;
        }
        return {};
    };

    const IniSection music = findSection(QStringLiteral("Music"));
    QCOMPARE(music.value(QStringLiteral("space")), QStringLiteral("new_space"));
    QCOMPARE(music.value(QStringLiteral("danger")), QStringLiteral("new_danger"));
    QCOMPARE(music.value(QStringLiteral("battle")), QStringLiteral("new_battle"));
    QCOMPARE(music.value(QStringLiteral("unrelated_music_key")), QStringLiteral("keep_me"));

    const IniSection ambient = findSection(QStringLiteral("Ambient"));
    QCOMPARE(ambient.value(QStringLiteral("color")), QStringLiteral("40, 50, 60"));
    QCOMPARE(ambient.value(QStringLiteral("oddity")), QStringLiteral("preserve"));

    const IniSection dust = findSection(QStringLiteral("Dust"));
    QCOMPARE(dust.value(QStringLiteral("spacedust")), QStringLiteral("new_dust"));
    QCOMPARE(dust.value(QStringLiteral("note")), QStringLiteral("keep"));

    const IniSection background = findSection(QStringLiteral("Background"));
    QCOMPARE(background.value(QStringLiteral("basic_stars")), QStringLiteral("new_basic"));
    QCOMPARE(background.value(QStringLiteral("complex_stars")), QStringLiteral("new_complex"));
    QCOMPARE(background.value(QStringLiteral("nebulae")), QStringLiteral("new_nebulae"));
    QCOMPARE(background.value(QStringLiteral("odd_bg")), QStringLiteral("keep_bg"));

    const IniSection untouched = findSection(QStringLiteral("EncounterParameters"));
    QCOMPARE(untouched.value(QStringLiteral("nickname")), QStringLiteral("untouched"));
}

void TestSystemSettingsService::clearsControlledKeysWhenEmpty()
{
    SystemDocument document;

    IniSection systemInfo;
    systemInfo.name = QStringLiteral("SystemInfo");
    systemInfo.entries = {
        {QStringLiteral("space_color"), QStringLiteral("1, 2, 3")},
        {QStringLiteral("local_faction"), QStringLiteral("li_n_grp")},
        {QStringLiteral("nickname"), QStringLiteral("Li01")},
    };
    SystemPersistence::setSystemInfoSection(&document, systemInfo);

    IniDocument extras;
    extras.append({QStringLiteral("Music"), {{QStringLiteral("space"), QStringLiteral("space")}}});
    extras.append({QStringLiteral("Dust"), {{QStringLiteral("spacedust"), QStringLiteral("dust")}}});
    extras.append({QStringLiteral("Ambient"), {{QStringLiteral("color"), QStringLiteral("5, 5, 5")}}});
    extras.append({QStringLiteral("Background"), {{QStringLiteral("basic_stars"), QStringLiteral("basic")}}});
    SystemPersistence::setExtraSections(&document, extras);

    SystemSettingsState state;
    QString errorMessage;
    QVERIFY2(SystemSettingsService::apply(&document, state, &errorMessage), qPrintable(errorMessage));

    const IniSection storedSystemInfo = SystemPersistence::systemInfoSection(&document);
    QCOMPARE(storedSystemInfo.value(QStringLiteral("space_color")), QString());
    QCOMPARE(storedSystemInfo.value(QStringLiteral("local_faction")), QString());
    QCOMPARE(storedSystemInfo.value(QStringLiteral("nickname")), QStringLiteral("Li01"));

    const IniDocument storedExtras = SystemPersistence::extraSections(&document);
    for (const IniSection &section : storedExtras) {
        QVERIFY2(section.name.compare(QStringLiteral("Music"), Qt::CaseInsensitive) != 0,
                 "Music section should have been removed when all controlled keys were cleared.");
        QVERIFY2(section.name.compare(QStringLiteral("Dust"), Qt::CaseInsensitive) != 0,
                 "Dust section should have been removed when all controlled keys were cleared.");
        QVERIFY2(section.name.compare(QStringLiteral("Ambient"), Qt::CaseInsensitive) != 0,
                 "Ambient section should have been removed when all controlled keys were cleared.");
        QVERIFY2(section.name.compare(QStringLiteral("Background"), Qt::CaseInsensitive) != 0,
                 "Background section should have been removed when all controlled keys were cleared.");
    }
}

void TestSystemSettingsService::normalizesRgbText()
{
    QString normalized;
    QString errorMessage;
    QVERIFY(SystemSettingsService::normalizeRgbText(QStringLiteral(" 1,2, 3 "), &normalized, &errorMessage));
    QCOMPARE(normalized, QStringLiteral("1, 2, 3"));

    QVERIFY(!SystemSettingsService::normalizeRgbText(QStringLiteral("300, 0, 0"), &normalized, &errorMessage));
    QVERIFY(!errorMessage.isEmpty());

    QVERIFY(!SystemSettingsService::normalizeRgbText(QStringLiteral("1, 2"), &normalized, &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
}

void TestSystemSettingsService::parsesFactionDisplay()
{
    QCOMPARE(SystemSettingsService::factionNicknameFromDisplay(QStringLiteral("li_n_grp - Liberty Navy")),
             QStringLiteral("li_n_grp"));
    QCOMPARE(SystemSettingsService::factionNicknameFromDisplay(QStringLiteral("rh_n_grp")),
             QStringLiteral("rh_n_grp"));

    const QStringList options = {
        QStringLiteral("li_n_grp - Liberty Navy"),
        QStringLiteral("rh_n_grp - Rheinland Navy"),
    };
    QCOMPARE(SystemSettingsService::factionDisplayForNickname(QStringLiteral("RH_N_GRP"), options),
             QStringLiteral("rh_n_grp - Rheinland Navy"));
    QCOMPARE(SystemSettingsService::factionDisplayForNickname(QStringLiteral("unknown_grp"), options),
             QStringLiteral("unknown_grp"));
}

QTEST_GUILESS_MAIN(TestSystemSettingsService)
#include "test_SystemSettingsService.moc"