#include "infrastructure/guide/GuideCache.h"
#include "infrastructure/guide/GuideJsonReader.h"

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>

using flatlas::infrastructure::guide::GuideCache;
using flatlas::infrastructure::guide::GuideJsonReader;

class TestGuideCatalogFiles : public QObject
{
    Q_OBJECT

private slots:
    void manifestAndArticlesAreReadable();
};

namespace {

QByteArray readAll(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        qFatal("Failed to open fixture file %s: %s", qPrintable(path), qPrintable(file.errorString()));
    return file.readAll();
}

QString guidesRoot()
{
    QDir dir(QCoreApplication::applicationDirPath());
    if (!dir.cd(QStringLiteral("../..")))
        qFatal("Failed to resolve repository root from test binary path.");
    return dir.filePath(QStringLiteral("guides"));
}

} // namespace

void TestGuideCatalogFiles::manifestAndArticlesAreReadable()
{
    const QString root = guidesRoot();
    const QByteArray manifestJson = readAll(QDir(root).filePath(QStringLiteral("guide-index.json")));

    QString error;
    const auto catalog = GuideJsonReader().readCatalog(manifestJson, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(catalog.schema, 1);
    QCOMPARE(catalog.catalogVersion, QStringLiteral("2026.05.12"));
    QVERIFY(catalog.articles.size() >= 3);

    for (const auto &entry : catalog.articles) {
        QVERIFY2(!QDir::isAbsolutePath(entry.path), qPrintable(entry.path));
        QVERIFY2(!QDir::cleanPath(entry.path).startsWith(QStringLiteral("../")), qPrintable(entry.path));

        const QString articlePath = QDir(root).filePath(entry.path);
        const QByteArray articleJson = readAll(articlePath);
        QCOMPARE(GuideCache::sha256Hex(articleJson), entry.sha256);

        const auto article = GuideJsonReader().readArticle(articleJson, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(article.id, entry.id);
        QCOMPARE(article.language, entry.language);
        QCOMPARE(article.title, entry.title);
        QVERIFY(!article.blocks.isEmpty());
    }
}

QTEST_GUILESS_MAIN(TestGuideCatalogFiles)
#include "test_GuideCatalogFiles.moc"
