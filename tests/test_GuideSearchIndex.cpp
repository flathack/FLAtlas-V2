#include "infrastructure/guide/GuideSearchIndex.h"

#include <QtTest/QtTest>

using flatlas::domain::guide::GuideArticle;
using flatlas::domain::guide::GuideBlock;
using flatlas::domain::guide::GuideBlockItem;
using flatlas::domain::guide::GuideCatalog;
using flatlas::domain::guide::GuideCatalogEntry;
using flatlas::infrastructure::guide::GuideSearchIndex;

class TestGuideSearchIndex : public QObject
{
    Q_OBJECT

private slots:
    void findsTitleAndTagMatches();
    void searchesLoadedArticleBlocks();
    void filtersByLanguage();
    void ranksTitleBeforeBodyOnlyMatch();
};

namespace {

GuideCatalogEntry makeEntry(const QString &id,
                            const QString &language,
                            const QString &title,
                            const QStringList &tags,
                            const QStringList &contexts)
{
    GuideCatalogEntry entry;
    entry.id = id;
    entry.language = language;
    entry.title = title;
    entry.summary = QStringLiteral("Guide summary");
    entry.category = QStringLiteral("Modding Grundlagen");
    entry.tags = tags;
    entry.contexts = contexts;
    entry.path = QStringLiteral("articles/%1/%2.json").arg(language, id);
    return entry;
}

GuideCatalog makeCatalog()
{
    GuideCatalog catalog;
    catalog.schema = 1;
    catalog.catalogVersion = QStringLiteral("2026.05.12");
    catalog.defaultLanguage = QStringLiteral("de");
    catalog.articles = {
        makeEntry(QStringLiteral("modding.factions.introduction"),
                  QStringLiteral("de"),
                  QStringLiteral("Fraktionen ins Spiel einbauen"),
                  {QStringLiteral("faction"), QStringLiteral("initialworld")},
                  {QStringLiteral("faction-editor"), QStringLiteral("system-editor")}),
        makeEntry(QStringLiteral("modding.ini.basics"),
                  QStringLiteral("de"),
                  QStringLiteral("Freelancer-INI-Grundlagen"),
                  {QStringLiteral("ini")},
                  {QStringLiteral("ini-editor")}),
        makeEntry(QStringLiteral("modding.factions.introduction"),
                  QStringLiteral("en"),
                  QStringLiteral("Adding factions to the game"),
                  {QStringLiteral("faction")},
                  {QStringLiteral("faction-editor")})
    };
    return catalog;
}

GuideArticle makeFactionArticle()
{
    GuideArticle article;
    article.id = QStringLiteral("modding.factions.introduction");
    article.language = QStringLiteral("de");
    article.title = QStringLiteral("Fraktionen ins Spiel einbauen");
    article.summary = QStringLiteral("Welche Dateien beteiligt sind.");
    article.tags = {QStringLiteral("faction")};
    article.contexts = {QStringLiteral("faction-editor"), QStringLiteral("system-editor")};

    GuideBlock paragraph;
    paragraph.type = QStringLiteral("paragraph");
    paragraph.text = QStringLiteral("Reputation und Empathy-Beziehungen müssen gepflegt werden.");
    article.blocks.append(paragraph);

    GuideBlock fileMap;
    fileMap.type = QStringLiteral("file-map");
    GuideBlockItem item;
    item.path = QStringLiteral("DATA/INITIALWORLD.INI");
    item.purpose = QStringLiteral("Faction-Definitionen und Initialbeziehungen");
    fileMap.fileItems.append(item);
    article.blocks.append(fileMap);

    return article;
}

} // namespace

void TestGuideSearchIndex::findsTitleAndTagMatches()
{
    GuideSearchIndex index;
    index.setCatalog(makeCatalog());

    const auto results = index.search(QStringLiteral("faction"), QStringLiteral("de"));

    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().entry.id, QStringLiteral("modding.factions.introduction"));
    QVERIFY(results.first().score > 0);
}

void TestGuideSearchIndex::searchesLoadedArticleBlocks()
{
    GuideSearchIndex index;
    index.setCatalog(makeCatalog());
    index.addArticle(makeFactionArticle());

    const auto results = index.search(QStringLiteral("Empathy"), QStringLiteral("de"));

    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().entry.id, QStringLiteral("modding.factions.introduction"));
}

void TestGuideSearchIndex::filtersByLanguage()
{
    GuideSearchIndex index;
    index.setCatalog(makeCatalog());

    const auto results = index.search(QStringLiteral("faction"), QStringLiteral("en"));

    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().entry.language, QStringLiteral("en"));
}

void TestGuideSearchIndex::ranksTitleBeforeBodyOnlyMatch()
{
    GuideSearchIndex index;
    auto catalog = makeCatalog();
    catalog.articles.append(makeEntry(QStringLiteral("modding.reputation.empathy"),
                                      QStringLiteral("de"),
                                      QStringLiteral("Empathy-Beziehungen"),
                                      {QStringLiteral("reputation")},
                                      {QStringLiteral("faction-editor")}));
    index.setCatalog(catalog);
    index.addArticle(makeFactionArticle());

    const auto results = index.search(QStringLiteral("Empathy"), QStringLiteral("de"));

    QVERIFY(results.size() >= 2);
    QCOMPARE(results.first().entry.id, QStringLiteral("modding.reputation.empathy"));
}

QTEST_GUILESS_MAIN(TestGuideSearchIndex)
#include "test_GuideSearchIndex.moc"
