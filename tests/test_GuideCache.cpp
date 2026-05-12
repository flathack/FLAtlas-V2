#include "infrastructure/guide/GuideCache.h"

#include <QtTest/QtTest>

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

using flatlas::infrastructure::guide::GuideCache;

class TestGuideCache : public QObject
{
    Q_OBJECT

private slots:
    void storesAndLoadsCatalog();
    void storesAndLoadsArticleWithMatchingHash();
    void rejectsArticleWithWrongHash();
    void rejectsUnsafeRelativePath();
    void tracksActiveCatalogVersion();
};

void TestGuideCache::storesAndLoadsCatalog()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const GuideCache cache(dir.path());
    QString error;
    const QByteArray catalog = R"({"schema":1,"catalogVersion":"2026.05.12"})";

    QVERIFY2(cache.storeCatalog(QStringLiteral("2026.05.12"), catalog, &error), qPrintable(error));
    QCOMPARE(cache.loadCatalog(QStringLiteral("2026.05.12"), &error), catalog);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(QFileInfo::exists(cache.catalogPath(QStringLiteral("2026.05.12"))));
}

void TestGuideCache::storesAndLoadsArticleWithMatchingHash()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const GuideCache cache(dir.path());
    QString error;
    const QByteArray article = R"({"id":"modding.factions.introduction"})";
    const QString hash = GuideCache::sha256Hex(article);

    QVERIFY2(cache.storeArticle(QStringLiteral("2026.05.12"),
                                QStringLiteral("articles/de/modding.factions.introduction.json"),
                                article,
                                hash,
                                &error),
             qPrintable(error));

    const QByteArray loaded = cache.loadArticle(QStringLiteral("2026.05.12"),
                                                QStringLiteral("articles/de/modding.factions.introduction.json"),
                                                hash,
                                                &error);
    QCOMPARE(loaded, article);
    QVERIFY2(error.isEmpty(), qPrintable(error));
}

void TestGuideCache::rejectsArticleWithWrongHash()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const GuideCache cache(dir.path());
    QString error;
    const QByteArray article = R"({"id":"modding.factions.introduction"})";

    QVERIFY(!cache.storeArticle(QStringLiteral("2026.05.12"),
                                QStringLiteral("articles/de/modding.factions.introduction.json"),
                                article,
                                QStringLiteral("wrong"),
                                &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!QFileInfo::exists(cache.articlePath(QStringLiteral("2026.05.12"),
                                                QStringLiteral("articles/de/modding.factions.introduction.json"))));
}

void TestGuideCache::rejectsUnsafeRelativePath()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const GuideCache cache(dir.path());
    QString error;
    const QByteArray article = R"({"id":"unsafe"})";

    QVERIFY(!cache.storeArticle(QStringLiteral("2026.05.12"),
                                QStringLiteral("../outside.json"),
                                article,
                                GuideCache::sha256Hex(article),
                                &error));
    QVERIFY(!error.isEmpty());
}

void TestGuideCache::tracksActiveCatalogVersion()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const GuideCache cache(dir.path());
    QString error;

    QCOMPARE(cache.activeCatalogVersion(), QString());
    QVERIFY2(cache.activateCatalogVersion(QStringLiteral("2026.05.12"), &error), qPrintable(error));
    QCOMPARE(cache.activeCatalogVersion(), QStringLiteral("2026.05.12"));
}

QTEST_GUILESS_MAIN(TestGuideCache)
#include "test_GuideCache.moc"
