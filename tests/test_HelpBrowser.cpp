// test_HelpBrowser.cpp – Phase 23 Help-System tests

#include <QtTest/QtTest>
#include "tools/HelpBrowser.h"

using namespace flatlas::tools;

class TestHelpBrowser : public QObject {
    Q_OBJECT
private slots:
    void testBuiltinTopicsLoaded();
    void testExpectedTopicIds();
    void testRegisterCustomTopic();
    void testShowTopicSetsContent();
    void testTopicForContextSystemEditor();
    void testTopicForContextUniverseEditor();
    void testTopicForContextBaseEditor();
    void testTopicForContextUnknownReturnsOverview();
    void testTopicForContextAllMappings();
};

void TestHelpBrowser::testBuiltinTopicsLoaded()
{
    HelpBrowser browser;
    QStringList ids = browser.topicIds();
    QVERIFY(ids.size() >= 13);
}

void TestHelpBrowser::testExpectedTopicIds()
{
    HelpBrowser browser;
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

QTEST_MAIN(TestHelpBrowser)
#include "test_HelpBrowser.moc"
