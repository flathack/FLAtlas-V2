#include "infrastructure/guide/GuideCache.h"
#include "infrastructure/guide/GuideRepository.h"

#include <QtTest/QtTest>

#include <QTemporaryDir>

using flatlas::infrastructure::guide::GuideCache;
using flatlas::infrastructure::guide::GuideRepository;

class TestGuideRepository : public QObject
{
    Q_OBJECT

private slots:
    void loadsActiveCatalog();
    void loadsArticleById();
    void findsEntriesForContext();
    void reportsMissingActiveCatalog();
};

namespace {

QByteArray factionArticle()
{
    return R"({
        "schema": 1,
        "id": "modding.factions.introduction",
        "language": "de",
        "title": "Fraktionen ins Spiel einbauen",
        "summary": "Welche Freelancer-Dateien an einer neuen Faction beteiligt sind.",
        "category": "Modding Grundlagen",
        "tags": ["faction", "initialworld"],
        "contexts": ["faction-editor", "npc-editor", "system-editor"],
        "updatedAt": "2026-05-12T00:00:00Z",
        "blocks": [
            { "type": "paragraph", "text": "Eine neue Fraktion besteht in Freelancer nicht aus einer einzelnen Datei." }
        ]
    })";
}

QByteArray iniArticle()
{
    return R"({
        "schema": 1,
        "id": "modding.ini.basics",
        "language": "de",
        "title": "Freelancer-INI-Grundlagen",
        "summary": "Sections, Keys und Werte verstehen.",
        "category": "Modding Grundlagen",
        "tags": ["ini"],
        "contexts": ["ini-editor"],
        "updatedAt": "2026-05-12T00:00:00Z",
        "blocks": [
            { "type": "paragraph", "text": "Freelancer nutzt INI-ähnliche Dateien." }
        ]
    })";
}

QByteArray catalogForArticles(const QByteArray &factionHash, const QByteArray &iniHash)
{
    return QByteArray(R"({
        "schema": 1,
        "catalogVersion": "2026.05.12",
        "defaultLanguage": "de",
        "languages": ["de"],
        "generatedAt": "2026-05-12T00:00:00Z",
        "articles": [
            {
                "id": "modding.factions.introduction",
                "language": "de",
                "title": "Fraktionen ins Spiel einbauen",
                "summary": "Welche Freelancer-Dateien an einer neuen Faction beteiligt sind.",
                "category": "Modding Grundlagen",
                "tags": ["faction", "initialworld"],
                "contexts": ["faction-editor", "npc-editor", "system-editor"],
                "level": "intermediate",
                "path": "articles/de/modding.factions.introduction.json",
                "version": 1,
                "updatedAt": "2026-05-12T00:00:00Z",
                "sha256": ")")
        + factionHash + R"("
            },
            {
                "id": "modding.ini.basics",
                "language": "de",
                "title": "Freelancer-INI-Grundlagen",
                "summary": "Sections, Keys und Werte verstehen.",
                "category": "Modding Grundlagen",
                "tags": ["ini"],
                "contexts": ["ini-editor"],
                "level": "beginner",
                "path": "articles/de/modding.ini.basics.json",
                "version": 1,
                "updatedAt": "2026-05-12T00:00:00Z",
                "sha256": ")"
        + iniHash + R"("
            }
        ]
    })";
}

GuideRepository makeRepositoryWithFixture(QTemporaryDir &dir)
{
    const GuideCache cache(dir.path());
    QString error;
    const QByteArray factions = factionArticle();
    const QByteArray ini = iniArticle();
    const QString factionHash = GuideCache::sha256Hex(factions);
    const QString iniHash = GuideCache::sha256Hex(ini);
    const QByteArray catalog = catalogForArticles(factionHash.toLatin1(), iniHash.toLatin1());

    if (!cache.storeCatalog(QStringLiteral("2026.05.12"), catalog, &error))
        qFatal("Failed to store guide catalog fixture: %s", qPrintable(error));
    if (!cache.storeArticle(QStringLiteral("2026.05.12"),
                            QStringLiteral("articles/de/modding.factions.introduction.json"),
                            factions,
                            factionHash,
                            &error))
        qFatal("Failed to store faction article fixture: %s", qPrintable(error));
    if (!cache.storeArticle(QStringLiteral("2026.05.12"),
                            QStringLiteral("articles/de/modding.ini.basics.json"),
                            ini,
                            iniHash,
                            &error))
        qFatal("Failed to store INI article fixture: %s", qPrintable(error));
    if (!cache.activateCatalogVersion(QStringLiteral("2026.05.12"), &error))
        qFatal("Failed to activate guide catalog fixture: %s", qPrintable(error));

    return GuideRepository(GuideCache(dir.path()));
}

} // namespace

void TestGuideRepository::loadsActiveCatalog()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    auto repository = makeRepositoryWithFixture(dir);
    QString error;
    const auto catalog = repository.loadActiveCatalog(&error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(catalog.catalogVersion, QStringLiteral("2026.05.12"));
    QCOMPARE(catalog.articles.size(), 2);
}

void TestGuideRepository::loadsArticleById()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    auto repository = makeRepositoryWithFixture(dir);
    QString error;
    const auto article = repository.loadArticle(QStringLiteral("modding.factions.introduction"), QStringLiteral("de"), &error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(article.id, QStringLiteral("modding.factions.introduction"));
    QCOMPARE(article.title, QStringLiteral("Fraktionen ins Spiel einbauen"));
    QCOMPARE(article.blocks.size(), 1);
}

void TestGuideRepository::findsEntriesForContext()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    auto repository = makeRepositoryWithFixture(dir);
    const auto entries = repository.entriesForContext(QStringLiteral("system-editor"), QStringLiteral("de"));

    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().id, QStringLiteral("modding.factions.introduction"));
}

void TestGuideRepository::reportsMissingActiveCatalog()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const GuideRepository repository(GuideCache(dir.path()));
    QString error;
    const auto catalog = repository.loadActiveCatalog(&error);

    QVERIFY(catalog.catalogVersion.isEmpty());
    QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(TestGuideRepository)
#include "test_GuideRepository.moc"
