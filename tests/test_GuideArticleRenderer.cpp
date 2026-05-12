#include "infrastructure/guide/GuideArticleRenderer.h"

#include <QtTest/QtTest>

using flatlas::domain::guide::GuideArticle;
using flatlas::domain::guide::GuideBlock;
using flatlas::domain::guide::GuideBlockItem;
using flatlas::infrastructure::guide::GuideArticleRenderer;

class TestGuideArticleRenderer : public QObject
{
    Q_OBJECT

private slots:
    void rendersArticleChromeAndParagraphs();
    void rendersChecklistFileMapCodeAndRelatedBlocks();
    void escapesHtmlFromGuideContent();
};

namespace {

GuideArticle baseArticle()
{
    GuideArticle article;
    article.id = QStringLiteral("modding.factions.introduction");
    article.language = QStringLiteral("de");
    article.title = QStringLiteral("Fraktionen ins Spiel einbauen");
    article.summary = QStringLiteral("Welche Freelancer-Dateien beteiligt sind.");
    article.category = QStringLiteral("Modding Grundlagen");
    return article;
}

} // namespace

void TestGuideArticleRenderer::rendersArticleChromeAndParagraphs()
{
    auto article = baseArticle();
    GuideBlock paragraph;
    paragraph.type = QStringLiteral("paragraph");
    paragraph.title = QStringLiteral("Überblick");
    paragraph.text = QStringLiteral("Eine neue Fraktion besteht nicht aus einer einzelnen Datei.");
    article.blocks.append(paragraph);

    const QString html = GuideArticleRenderer().renderHtml(article);

    QVERIFY(html.contains(QStringLiteral("<h1>Fraktionen ins Spiel einbauen</h1>")));
    QVERIFY(html.contains(QStringLiteral("<i>Welche Freelancer-Dateien beteiligt sind.</i>")));
    QVERIFY(html.contains(QStringLiteral("<h2>Überblick</h2>")));
    QVERIFY(html.contains(QStringLiteral("<p>Eine neue Fraktion besteht nicht aus einer einzelnen Datei.</p>")));
}

void TestGuideArticleRenderer::rendersChecklistFileMapCodeAndRelatedBlocks()
{
    auto article = baseArticle();

    GuideBlock checklist;
    checklist.type = QStringLiteral("checklist");
    checklist.title = QStringLiteral("Beteiligte Bereiche");
    checklist.items = {
        QStringLiteral("IDS-Einträge anlegen"),
        QStringLiteral("Faction in initialworld.ini definieren")
    };
    article.blocks.append(checklist);

    GuideBlock fileMap;
    fileMap.type = QStringLiteral("file-map");
    fileMap.title = QStringLiteral("Typische Dateien");
    GuideBlockItem item;
    item.path = QStringLiteral("DATA/INITIALWORLD.INI");
    item.purpose = QStringLiteral("Faction-Definitionen und Initialbeziehungen");
    fileMap.fileItems.append(item);
    article.blocks.append(fileMap);

    GuideBlock code;
    code.type = QStringLiteral("code");
    code.title = QStringLiteral("Beispielstruktur");
    code.language = QStringLiteral("ini");
    code.text = QStringLiteral("[Group]\nnickname = fc_example_grp");
    article.blocks.append(code);

    GuideBlock related;
    related.type = QStringLiteral("related");
    related.articleIds = {
        QStringLiteral("modding.ids.infocards"),
        QStringLiteral("modding.npc.encounters")
    };
    article.blocks.append(related);

    const QString html = GuideArticleRenderer().renderHtml(article);

    QVERIFY(html.contains(QStringLiteral("<li>IDS-Einträge anlegen</li>")));
    QVERIFY(html.contains(QStringLiteral("<td><code>DATA/INITIALWORLD.INI</code></td>")));
    QVERIFY(html.contains(QStringLiteral("<pre><code>[Group]\nnickname = fc_example_grp</code></pre>")));
    QVERIFY(html.contains(QStringLiteral("<li><code>modding.ids.infocards</code></li>")));
}

void TestGuideArticleRenderer::escapesHtmlFromGuideContent()
{
    auto article = baseArticle();
    article.title = QStringLiteral("<script>alert(1)</script>");

    GuideBlock paragraph;
    paragraph.type = QStringLiteral("paragraph");
    paragraph.text = QStringLiteral("Use <b>bold</b> & raw text.");
    article.blocks.append(paragraph);

    const QString html = GuideArticleRenderer().renderHtml(article);

    QVERIFY(!html.contains(QStringLiteral("<script>")));
    QVERIFY(!html.contains(QStringLiteral("<b>bold</b>")));
    QVERIFY(html.contains(QStringLiteral("&lt;script&gt;alert(1)&lt;/script&gt;")));
    QVERIFY(html.contains(QStringLiteral("Use &lt;b&gt;bold&lt;/b&gt; &amp; raw text.")));
}

QTEST_GUILESS_MAIN(TestGuideArticleRenderer)
#include "test_GuideArticleRenderer.moc"
