#include "infrastructure/guide/GuideCache.h"
#include "infrastructure/guide/GuideRemoteFetcher.h"
#include "infrastructure/guide/GuideRepository.h"
#include "infrastructure/guide/GuideSyncService.h"

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>

using flatlas::infrastructure::guide::GuideCache;
using flatlas::infrastructure::guide::GuideRemoteFetcher;
using flatlas::infrastructure::guide::GuideRepository;
using flatlas::infrastructure::guide::GuideSyncService;

class TestGuideRemoteFetcher : public QObject
{
    Q_OBJECT

private slots:
    void fetchesPackageFromFileUrl();
    void reportsManifestParseErrors();
};

namespace {

void writeFile(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        qFatal("Failed to write fixture file %s: %s", qPrintable(path), qPrintable(file.errorString()));
    file.write(data);
}

QByteArray articleJson()
{
    return R"({
        "schema": 1,
        "id": "modding.factions.introduction",
        "language": "de",
        "title": "Fraktionen ins Spiel einbauen",
        "summary": "Welche Dateien beteiligt sind.",
        "category": "Modding Grundlagen",
        "tags": ["faction"],
        "contexts": ["faction-editor", "system-editor"],
        "updatedAt": "2026-05-12T00:00:00Z",
        "blocks": [
            { "type": "paragraph", "text": "Eine neue Fraktion besteht nicht aus einer einzelnen Datei." }
        ]
    })";
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

} // namespace

void TestGuideRemoteFetcher::fetchesPackageFromFileUrl()
{
    QTemporaryDir remoteDir;
    QVERIFY(remoteDir.isValid());

    const QByteArray article = articleJson();
    writeFile(QDir(remoteDir.path()).filePath(QStringLiteral("articles/de/modding.factions.introduction.json")), article);
    writeFile(QDir(remoteDir.path()).filePath(QStringLiteral("guide-index.json")), manifestJson(GuideCache::sha256Hex(article)));

    GuideRemoteFetcher fetcher;
    QSignalSpy finished(&fetcher, &GuideRemoteFetcher::finished);
    QSignalSpy failed(&fetcher, &GuideRemoteFetcher::failed);

    fetcher.fetch(QUrl::fromLocalFile(QDir(remoteDir.path()).filePath(QStringLiteral("guide-index.json"))));

    QVERIFY(finished.wait(5000) || finished.count() == 1);
    QCOMPARE(failed.count(), 0);

    const auto packageData = fetcher.package();
    QVERIFY(!packageData.manifestJson.isEmpty());
    QVERIFY(packageData.articlesByPath.contains(QStringLiteral("articles/de/modding.factions.introduction.json")));

    QTemporaryDir cacheDir;
    QVERIFY(cacheDir.isValid());
    QString error;
    GuideSyncService sync(GuideCache(cacheDir.path()));
    QVERIFY2(sync.applyPackage(packageData, &error), qPrintable(error));

    GuideRepository repository(GuideCache(cacheDir.path()));
    const auto loadedArticle = repository.loadArticle(QStringLiteral("modding.factions.introduction"), QStringLiteral("de"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(loadedArticle.title, QStringLiteral("Fraktionen ins Spiel einbauen"));
}

void TestGuideRemoteFetcher::reportsManifestParseErrors()
{
    QTemporaryDir remoteDir;
    QVERIFY(remoteDir.isValid());

    const QString manifestPath = QDir(remoteDir.path()).filePath(QStringLiteral("guide-index.json"));
    writeFile(manifestPath, QByteArrayLiteral("{"));

    GuideRemoteFetcher fetcher;
    QSignalSpy finished(&fetcher, &GuideRemoteFetcher::finished);
    QSignalSpy failed(&fetcher, &GuideRemoteFetcher::failed);

    fetcher.fetch(QUrl::fromLocalFile(manifestPath));

    QVERIFY(failed.wait(5000) || failed.count() == 1);
    QCOMPARE(finished.count(), 0);
    QVERIFY(!failed.first().first().toString().isEmpty());
}

QTEST_GUILESS_MAIN(TestGuideRemoteFetcher)
#include "test_GuideRemoteFetcher.moc"
