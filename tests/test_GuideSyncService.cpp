#include "infrastructure/guide/GuideCache.h"
#include "infrastructure/guide/GuideRepository.h"
#include "infrastructure/guide/GuideSyncService.h"

#include <QtTest/QtTest>

#include <QTemporaryDir>

using flatlas::infrastructure::guide::GuideCache;
using flatlas::infrastructure::guide::GuideRepository;
using flatlas::infrastructure::guide::GuideSyncService;

class TestGuideSyncService : public QObject
{
    Q_OBJECT

private slots:
    void appliesValidPackageAndActivatesCatalog();
    void rejectsMissingArticle();
    void rejectsHashMismatch();
    void rejectsArticleIdentityMismatch();
};

namespace {

QByteArray articleJson(const QString &id = QStringLiteral("modding.factions.introduction"),
                       const QString &language = QStringLiteral("de"))
{
    return QStringLiteral(R"({
        "schema": 1,
        "id": "%1",
        "language": "%2",
        "title": "Fraktionen ins Spiel einbauen",
        "summary": "Welche Dateien beteiligt sind.",
        "category": "Modding Grundlagen",
        "tags": ["faction"],
        "contexts": ["faction-editor", "system-editor"],
        "updatedAt": "2026-05-12T00:00:00Z",
        "blocks": [
            { "type": "paragraph", "text": "Eine neue Fraktion besteht nicht aus einer einzelnen Datei." }
        ]
    })").arg(id, language).toUtf8();
}

QByteArray manifestJson(const QString &hash)
{
    return QStringLiteral(R"({
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
                "summary": "Welche Dateien beteiligt sind.",
                "category": "Modding Grundlagen",
                "tags": ["faction"],
                "contexts": ["faction-editor", "system-editor"],
                "level": "intermediate",
                "path": "articles/de/modding.factions.introduction.json",
                "version": 1,
                "updatedAt": "2026-05-12T00:00:00Z",
                "sha256": "%1"
            }
        ]
    })").arg(hash).toUtf8();
}

GuideSyncService::SyncPackage validPackage()
{
    const QByteArray article = articleJson();
    GuideSyncService::SyncPackage packageData;
    packageData.manifestJson = manifestJson(GuideCache::sha256Hex(article));
    packageData.articlesByPath.insert(QStringLiteral("articles/de/modding.factions.introduction.json"), article);
    return packageData;
}

} // namespace

void TestGuideSyncService::appliesValidPackageAndActivatesCatalog()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString error;
    GuideSyncService service(GuideCache(dir.path()));
    QVERIFY2(service.applyPackage(validPackage(), &error), qPrintable(error));

    GuideRepository repository(GuideCache(dir.path()));
    const auto catalog = repository.loadActiveCatalog(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(catalog.catalogVersion, QStringLiteral("2026.05.12"));

    const auto article = repository.loadArticle(QStringLiteral("modding.factions.introduction"), QStringLiteral("de"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(article.id, QStringLiteral("modding.factions.introduction"));
}

void TestGuideSyncService::rejectsMissingArticle()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    auto packageData = validPackage();
    packageData.articlesByPath.clear();

    QString error;
    GuideSyncService service(GuideCache(dir.path()));
    QVERIFY(!service.applyPackage(packageData, &error));
    QVERIFY(error.contains(QStringLiteral("missing article")));
    QCOMPARE(GuideCache(dir.path()).activeCatalogVersion(), QString());
}

void TestGuideSyncService::rejectsHashMismatch()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    auto packageData = validPackage();
    packageData.articlesByPath[QStringLiteral("articles/de/modding.factions.introduction.json")] = articleJson()
        + QByteArrayLiteral("\n");

    QString error;
    GuideSyncService service(GuideCache(dir.path()));
    QVERIFY(!service.applyPackage(packageData, &error));
    QVERIFY(error.contains(QStringLiteral("hash")));
    QCOMPARE(GuideCache(dir.path()).activeCatalogVersion(), QString());
}

void TestGuideSyncService::rejectsArticleIdentityMismatch()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QByteArray article = articleJson(QStringLiteral("modding.other"));
    GuideSyncService::SyncPackage packageData;
    packageData.manifestJson = manifestJson(GuideCache::sha256Hex(article));
    packageData.articlesByPath.insert(QStringLiteral("articles/de/modding.factions.introduction.json"), article);

    QString error;
    GuideSyncService service(GuideCache(dir.path()));
    QVERIFY(!service.applyPackage(packageData, &error));
    QVERIFY(error.contains(QStringLiteral("identity")));
    QCOMPARE(GuideCache(dir.path()).activeCatalogVersion(), QString());
}

QTEST_GUILESS_MAIN(TestGuideSyncService)
#include "test_GuideSyncService.moc"
