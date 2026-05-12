// test_HelpBrowser.cpp – Phase 23 Help-System tests

#include <QtTest/QtTest>
#include "tools/HelpBrowser.h"

#include "domain/guide/GuideArticle.h"

#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QTextBrowser>

using namespace flatlas::tools;

class TestHelpBrowser : public QObject {
    Q_OBJECT
private slots:
    void testStartsWithoutTopics();
    void testShowTopicDisplaysEmptyStateWithoutTopics();
    void testBuiltinTopicsLoaded();
    void testExpectedTopicIds();
    void testRegisterCustomTopic();
    void testShowTopicSetsContent();
    void testTopicForContextSystemEditor();
    void testTopicForContextUniverseEditor();
    void testTopicForContextBaseEditor();
    void testTopicForContextUnknownReturnsOverview();
    void testTopicForContextAllMappings();
    void testLoadGuideArticlesReplacesBuiltinTopics();
    void testContextAliasDisplaysGuideArticle();
    void testGuideCategoriesAreAvailable();
    void testGuideCategoryFiltersTopics();
    void testGuideSearchFiltersTopics();
    void testGuideLanguageFilterSwitchesTopics();
};

namespace {

flatlas::domain::guide::GuideArticle makeGuideArticle(const QString &id,
                                                      const QString &title,
                                                      const QString &category,
                                                      const QString &text,
                                                      const QString &language = QStringLiteral("de"))
{
    flatlas::domain::guide::GuideArticle article;
    article.id = id;
    article.language = language;
    article.title = title;
    article.summary = QStringLiteral("Guide aus dem Online-Katalog.");
    article.category = category;
    article.contexts = {QStringLiteral("system-editor")};

    flatlas::domain::guide::GuideBlock block;
    block.type = QStringLiteral("paragraph");
    block.text = text;
    article.blocks.append(block);
    return article;
}

} // namespace

void TestHelpBrowser::testStartsWithoutTopics()
{
    HelpBrowser browser;
    QVERIFY(browser.topicIds().isEmpty());
}

void TestHelpBrowser::testShowTopicDisplaysEmptyStateWithoutTopics()
{
    HelpBrowser browser;
    browser.showTopic(QStringLiteral("overview"));

    auto *textBrowser = browser.findChild<QTextBrowser *>();
    QVERIFY(textBrowser);
    QVERIFY(textBrowser->toPlainText().contains(QStringLiteral("online help has not been downloaded")));

    browser.close();
}

void TestHelpBrowser::testBuiltinTopicsLoaded()
{
    HelpBrowser browser;
    browser.loadBuiltinTopics();
    QStringList ids = browser.topicIds();
    QVERIFY(ids.size() >= 13);
}

void TestHelpBrowser::testExpectedTopicIds()
{
    HelpBrowser browser;
    browser.loadBuiltinTopics();
    QStringList ids = browser.topicIds();
    QVERIFY(ids.contains(QStringLiteral("overview")));
    QVERIFY(ids.contains(QStringLiteral("system-editor")));
    QVERIFY(ids.contains(QStringLiteral("universe-editor")));
    QVERIFY(ids.contains(QStringLiteral("base-editor")));
    QVERIFY(ids.contains(QStringLiteral("ini-editor")));
    QVERIFY(ids.contains(QStringLiteral("trade-routes")));
    QVERIFY(ids.contains(QStringLiteral("ids-editor")));
    QVERIFY(ids.contains(QStringLiteral("mod-manager")));
    QVERIFY(ids.contains(QStringLiteral("npc-editor")));
    QVERIFY(ids.contains(QStringLiteral("infocard-editor")));
    QVERIFY(ids.contains(QStringLiteral("news-rumor-editor")));
    QVERIFY(ids.contains(QStringLiteral("model-viewer")));
    QVERIFY(ids.contains(QStringLiteral("jump-connections")));
}

void TestHelpBrowser::testRegisterCustomTopic()
{
    HelpBrowser browser;
    browser.loadBuiltinTopics();
    int before = browser.topicIds().size();
    browser.registerTopic({QStringLiteral("custom-topic"),
                           QStringLiteral("Custom"),
                           QStringLiteral("<p>Custom content</p>")});
    QCOMPARE(browser.topicIds().size(), before + 1);
    QVERIFY(browser.topicIds().contains(QStringLiteral("custom-topic")));
}

void TestHelpBrowser::testShowTopicSetsContent()
{
    HelpBrowser browser;
    browser.loadBuiltinTopics();
    // showTopic should not crash for known or unknown topics
    browser.showTopic(QStringLiteral("overview"));
    browser.showTopic(QStringLiteral("system-editor"));
    browser.showTopic(QStringLiteral("nonexistent"));
    browser.close();
}

void TestHelpBrowser::testTopicForContextSystemEditor()
{
    QCOMPARE(HelpBrowser::topicForContext(QStringLiteral("SystemEditorPage")),
             QStringLiteral("system-editor"));
}

void TestHelpBrowser::testTopicForContextUniverseEditor()
{
    QCOMPARE(HelpBrowser::topicForContext(QStringLiteral("UniverseEditorPage")),
             QStringLiteral("universe-editor"));
}

void TestHelpBrowser::testTopicForContextBaseEditor()
{
    QCOMPARE(HelpBrowser::topicForContext(QStringLiteral("BaseEditorPage")),
             QStringLiteral("base-editor"));
}

void TestHelpBrowser::testTopicForContextUnknownReturnsOverview()
{
    QCOMPARE(HelpBrowser::topicForContext(QStringLiteral("SomeRandomWidget")),
             QStringLiteral("overview"));
    QCOMPARE(HelpBrowser::topicForContext(QString()),
             QStringLiteral("overview"));
}

void TestHelpBrowser::testTopicForContextAllMappings()
{
    const QMap<QString, QString> expected = {
        {"SystemEditorPage",    "system-editor"},
        {"UniverseEditorPage",  "universe-editor"},
        {"BaseEditorPage",      "base-editor"},
        {"IniEditorPage",       "ini-editor"},
        {"TradeRoutePage",      "trade-routes"},
        {"IdsEditorPage",       "ids-editor"},
        {"ModManagerPage",      "mod-manager"},
        {"NpcEditorPage",       "npc-editor"},
        {"InfocardEditor",      "infocard-editor"},
        {"NewsRumorEditor",     "news-rumor-editor"},
        {"ModelPreview",        "model-viewer"},
        {"JumpConnectionDialog","jump-connections"},
    };
    for (auto it = expected.cbegin(); it != expected.cend(); ++it) {
        QCOMPARE(HelpBrowser::topicForContext(it.key()), it.value());
    }
}

void TestHelpBrowser::testLoadGuideArticlesReplacesBuiltinTopics()
{
    HelpBrowser browser;

    flatlas::domain::guide::GuideArticle article;
    article.id = QStringLiteral("atlas.system-editor");
    article.language = QStringLiteral("de");
    article.title = QStringLiteral("System-Editor benutzen");
    article.summary = QStringLiteral("Guide aus dem Online-Katalog.");
    article.contexts = {QStringLiteral("system-editor")};

    QVERIFY(browser.loadGuideArticles({article}));

    const QStringList ids = browser.topicIds();
    QCOMPARE(ids.size(), 1);
    QVERIFY(ids.contains(QStringLiteral("de:atlas.system-editor")));
    QVERIFY(!ids.contains(QStringLiteral("overview")));
}

void TestHelpBrowser::testContextAliasDisplaysGuideArticle()
{
    HelpBrowser browser;

    flatlas::domain::guide::GuideArticle article;
    article.id = QStringLiteral("atlas.system-editor");
    article.language = QStringLiteral("de");
    article.title = QStringLiteral("System-Editor benutzen");
    article.summary = QStringLiteral("Guide aus dem Online-Katalog.");
    article.contexts = {QStringLiteral("system-editor")};

    flatlas::domain::guide::GuideBlock block;
    block.type = QStringLiteral("paragraph");
    block.text = QStringLiteral("Dieser Text kommt aus dem strukturierten Guide-Artikel.");
    article.blocks.append(block);

    QVERIFY(browser.loadGuideArticles({article}));
    browser.showTopic(QStringLiteral("system-editor"));

    auto *textBrowser = browser.findChild<QTextBrowser *>();
    QVERIFY(textBrowser);
    QVERIFY(textBrowser->toPlainText().contains(QStringLiteral("Dieser Text kommt aus dem strukturierten Guide-Artikel.")));

    browser.close();
}

void TestHelpBrowser::testGuideCategoriesAreAvailable()
{
    HelpBrowser browser;
    QVERIFY(browser.loadGuideArticles({
        makeGuideArticle(QStringLiteral("atlas.system-editor"),
                         QStringLiteral("System-Editor benutzen"),
                         QStringLiteral("FLAtlas Bedienung"),
                         QStringLiteral("Systemkarte und Properties.")),
        makeGuideArticle(QStringLiteral("modding.ini.basics"),
                         QStringLiteral("Freelancer-INI-Grundlagen"),
                         QStringLiteral("Modding Grundlagen"),
                         QStringLiteral("Sections und Keys."))
    }));

    auto *categoryFilter = browser.findChild<QComboBox *>(QStringLiteral("guideCategoryFilter"));
    QVERIFY(categoryFilter);
    QVERIFY(categoryFilter->findText(QStringLiteral("FLAtlas Bedienung")) >= 0);
    QVERIFY(categoryFilter->findText(QStringLiteral("Modding Grundlagen")) >= 0);
}

void TestHelpBrowser::testGuideCategoryFiltersTopics()
{
    HelpBrowser browser;
    QVERIFY(browser.loadGuideArticles({
        makeGuideArticle(QStringLiteral("atlas.system-editor"),
                         QStringLiteral("System-Editor benutzen"),
                         QStringLiteral("FLAtlas Bedienung"),
                         QStringLiteral("Systemkarte und Properties.")),
        makeGuideArticle(QStringLiteral("modding.ini.basics"),
                         QStringLiteral("Freelancer-INI-Grundlagen"),
                         QStringLiteral("Modding Grundlagen"),
                         QStringLiteral("Sections und Keys."))
    }));

    auto *categoryFilter = browser.findChild<QComboBox *>(QStringLiteral("guideCategoryFilter"));
    auto *topicList = browser.findChild<QListWidget *>(QStringLiteral("guideTopicList"));
    QVERIFY(categoryFilter);
    QVERIFY(topicList);

    categoryFilter->setCurrentText(QStringLiteral("FLAtlas Bedienung"));
    QCOMPARE(topicList->count(), 1);
    QCOMPARE(topicList->item(0)->text(), QStringLiteral("System-Editor benutzen"));

    categoryFilter->setCurrentIndex(0);
    QCOMPARE(topicList->count(), 2);
}

void TestHelpBrowser::testGuideSearchFiltersTopics()
{
    HelpBrowser browser;
    QVERIFY(browser.loadGuideArticles({
        makeGuideArticle(QStringLiteral("atlas.system-editor"),
                         QStringLiteral("System-Editor benutzen"),
                         QStringLiteral("FLAtlas Bedienung"),
                         QStringLiteral("Systemkarte und Properties.")),
        makeGuideArticle(QStringLiteral("modding.ini.basics"),
                         QStringLiteral("Freelancer-INI-Grundlagen"),
                         QStringLiteral("Modding Grundlagen"),
                         QStringLiteral("Sections und Keys."))
    }));

    auto *searchEdit = browser.findChild<QLineEdit *>(QStringLiteral("guideSearchEdit"));
    auto *topicList = browser.findChild<QListWidget *>(QStringLiteral("guideTopicList"));
    QVERIFY(searchEdit);
    QVERIFY(topicList);
    QCOMPARE(topicList->count(), 2);

    searchEdit->setText(QStringLiteral("Sections"));
    QCOMPARE(topicList->count(), 1);
    QCOMPARE(topicList->item(0)->text(), QStringLiteral("Freelancer-INI-Grundlagen"));

    searchEdit->clear();
    QCOMPARE(topicList->count(), 2);
}

void TestHelpBrowser::testGuideLanguageFilterSwitchesTopics()
{
    HelpBrowser browser;
    QVERIFY(browser.loadGuideArticles({
        makeGuideArticle(QStringLiteral("atlas.system-editor"),
                         QStringLiteral("System-Editor benutzen"),
                         QStringLiteral("FLAtlas Bedienung"),
                         QStringLiteral("Systemkarte und Properties."),
                         QStringLiteral("de")),
        makeGuideArticle(QStringLiteral("atlas.system-editor"),
                         QStringLiteral("Using the System Editor"),
                         QStringLiteral("FLAtlas Usage"),
                         QStringLiteral("System map and properties."),
                         QStringLiteral("en"))
    }));

    auto *languageFilter = browser.findChild<QComboBox *>(QStringLiteral("guideLanguageFilter"));
    auto *topicList = browser.findChild<QListWidget *>(QStringLiteral("guideTopicList"));
    QVERIFY(languageFilter);
    QVERIFY(topicList);
    QVERIFY(languageFilter->findText(QStringLiteral("de")) >= 0);
    QVERIFY(languageFilter->findText(QStringLiteral("en")) >= 0);

    languageFilter->setCurrentText(QStringLiteral("en"));
    QCOMPARE(topicList->count(), 1);
    QCOMPARE(topicList->item(0)->text(), QStringLiteral("Using the System Editor"));

    languageFilter->setCurrentText(QStringLiteral("de"));
    QCOMPARE(topicList->count(), 1);
    QCOMPARE(topicList->item(0)->text(), QStringLiteral("System-Editor benutzen"));
}

QTEST_MAIN(TestHelpBrowser)
#include "test_HelpBrowser.moc"
