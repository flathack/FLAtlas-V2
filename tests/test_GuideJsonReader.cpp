#include "infrastructure/guide/GuideJsonReader.h"

#include <QtTest/QtTest>

using flatlas::infrastructure::guide::GuideJsonReader;

class TestGuideJsonReader : public QObject
{
    Q_OBJECT

private slots:
    void readsCatalogManifest();
    void readsStructuredArticle();
    void reportsInvalidJson();
};

void TestGuideJsonReader::readsCatalogManifest()
{
    const QByteArray json = R"({
        "schema": 1,
        "catalogVersion": "2026.05.12",
        "defaultLanguage": "de",
        "languages": ["de", "en"],
        "generatedAt": "2026-05-12T00:00:00Z",
        "articles": [
            {
                "id": "modding.factions.introduction",
                "language": "de",
                "title": "Fraktionen ins Spiel einbauen",
                "summary": "Welche Freelancer-Dateien an einer neuen Faction beteiligt sind.",
                "category": "Modding Grundlagen",
                "tags": ["faction", "initialworld", "empathy"],
                "contexts": ["faction-editor", "npc-editor", "system-editor"],
                "level": "intermediate",
                "path": "articles/de/modding.factions.introduction.json",
                "version": 3,
                "updatedAt": "2026-05-12T00:00:00Z",
                "sha256": "abc123"
            }
        ]
    })";

    QString error;
    const auto catalog = GuideJsonReader().readCatalog(json, &error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(catalog.schema, 1);
    QCOMPARE(catalog.catalogVersion, QStringLiteral("2026.05.12"));
    QCOMPARE(catalog.defaultLanguage, QStringLiteral("de"));
    QCOMPARE(catalog.languages, QStringList({QStringLiteral("de"), QStringLiteral("en")}));
    QCOMPARE(catalog.articles.size(), 1);

    const auto &entry = catalog.articles.first();
    QCOMPARE(entry.id, QStringLiteral("modding.factions.introduction"));
    QCOMPARE(entry.title, QStringLiteral("Fraktionen ins Spiel einbauen"));
    QCOMPARE(entry.tags, QStringList({QStringLiteral("faction"), QStringLiteral("initialworld"), QStringLiteral("empathy")}));
    QCOMPARE(entry.contexts, QStringList({QStringLiteral("faction-editor"), QStringLiteral("npc-editor"), QStringLiteral("system-editor")}));
    QCOMPARE(entry.version, 3);
    QCOMPARE(entry.sha256, QStringLiteral("abc123"));
}

void TestGuideJsonReader::readsStructuredArticle()
{
    const QByteArray json = R"({
        "schema": 1,
        "id": "modding.factions.introduction",
        "language": "de",
        "title": "Fraktionen ins Spiel einbauen",
        "summary": "Welche Freelancer-Dateien an einer neuen Faction beteiligt sind und wie sie zusammenhängen.",
        "category": "Modding Grundlagen",
        "tags": ["faction", "initialworld", "ids", "npc"],
        "contexts": ["faction-editor", "npc-editor", "system-editor"],
        "updatedAt": "2026-05-12T00:00:00Z",
        "blocks": [
            {
                "type": "paragraph",
                "text": "Eine neue Fraktion besteht in Freelancer nicht aus einer einzelnen Datei."
            },
            {
                "type": "checklist",
                "title": "Beteiligte Bereiche",
                "items": [
                    "IDS-Einträge für Namen und Kurzbezeichnungen anlegen",
                    "Faction in initialworld.ini definieren"
                ]
            },
            {
                "type": "file-map",
                "title": "Typische Dateien",
                "items": [
                    { "path": "DATA/INITIALWORLD.INI", "purpose": "Faction-Definitionen und Initialbeziehungen" },
                    { "path": "DATA/MISSIONS/mbases.ini", "purpose": "NPCs, Räume und Base-Verhalten" }
                ]
            },
            {
                "type": "related",
                "articleIds": ["modding.ids.infocards", "modding.npc.encounters"]
            }
        ]
    })";

    QString error;
    const auto article = GuideJsonReader().readArticle(json, &error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(article.schema, 1);
    QCOMPARE(article.id, QStringLiteral("modding.factions.introduction"));
    QCOMPARE(article.language, QStringLiteral("de"));
    QCOMPARE(article.title, QStringLiteral("Fraktionen ins Spiel einbauen"));
    QCOMPARE(article.blocks.size(), 4);

    QCOMPARE(article.blocks.at(0).type, QStringLiteral("paragraph"));
    QVERIFY(article.blocks.at(0).text.contains(QStringLiteral("Fraktion")));

    QCOMPARE(article.blocks.at(1).type, QStringLiteral("checklist"));
    QCOMPARE(article.blocks.at(1).items.size(), 2);
    QCOMPARE(article.blocks.at(1).items.first(), QStringLiteral("IDS-Einträge für Namen und Kurzbezeichnungen anlegen"));

    QCOMPARE(article.blocks.at(2).type, QStringLiteral("file-map"));
    QCOMPARE(article.blocks.at(2).fileItems.size(), 2);
    QCOMPARE(article.blocks.at(2).fileItems.first().path, QStringLiteral("DATA/INITIALWORLD.INI"));
    QCOMPARE(article.blocks.at(2).fileItems.first().purpose, QStringLiteral("Faction-Definitionen und Initialbeziehungen"));

    QCOMPARE(article.blocks.at(3).articleIds, QStringList({QStringLiteral("modding.ids.infocards"), QStringLiteral("modding.npc.encounters")}));
}

void TestGuideJsonReader::reportsInvalidJson()
{
    QString error;
    const auto catalog = GuideJsonReader().readCatalog(QByteArrayLiteral("{"), &error);

    QVERIFY(!error.isEmpty());
    QCOMPARE(catalog.schema, 0);
    QVERIFY(catalog.articles.isEmpty());
}

QTEST_GUILESS_MAIN(TestGuideJsonReader)
#include "test_GuideJsonReader.moc"
