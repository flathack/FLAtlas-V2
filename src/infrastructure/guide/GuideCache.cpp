#include "GuideCache.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

#include <utility>

namespace flatlas::infrastructure::guide {
namespace {

constexpr auto kCatalogFileName = "guide-index.json";
constexpr auto kActiveVersionFileName = "active-version.txt";

bool writeFileAtomically(const QString &path, const QByteArray &data, QString *errorMessage)
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }

    if (file.write(data) != data.size()) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }

    if (!file.commit()) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }

    return true;
}

QByteArray readFile(const QString &path, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return {};
    }

    return file.readAll();
}

bool hasExpectedHash(const QByteArray &data, const QString &expectedSha256)
{
    if (expectedSha256.trimmed().isEmpty())
        return true;

    return GuideCache::sha256Hex(data).compare(expectedSha256.trimmed(), Qt::CaseInsensitive) == 0;
}

bool isSafeRelativePath(const QString &relativePath)
{
    if (relativePath.trimmed().isEmpty())
        return false;

    const QString cleaned = QDir::cleanPath(relativePath);
    return !QDir::isAbsolutePath(cleaned)
        && !cleaned.startsWith(QStringLiteral("../"))
        && cleaned != QStringLiteral("..");
}

} // namespace

GuideCache::GuideCache(QString rootPath)
    : m_rootPath(std::move(rootPath))
{
}

GuideCache GuideCache::forAppDataLocation()
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return GuideCache(QDir(appData).filePath(QStringLiteral("guides")));
}

QString GuideCache::rootPath() const
{
    return m_rootPath;
}

QString GuideCache::catalogPath(const QString &catalogVersion) const
{
    return QDir(QDir(m_rootPath).filePath(catalogVersion)).filePath(QString::fromLatin1(kCatalogFileName));
}

QString GuideCache::articlePath(const QString &catalogVersion, const QString &relativePath) const
{
    return QDir(QDir(m_rootPath).filePath(catalogVersion)).filePath(QDir::cleanPath(relativePath));
}

QString GuideCache::activeCatalogVersion() const
{
    QString error;
    const QByteArray data = readFile(QDir(m_rootPath).filePath(QString::fromLatin1(kActiveVersionFileName)), &error);
    if (!error.isEmpty())
        return {};

    return QString::fromUtf8(data).trimmed();
}

bool GuideCache::storeCatalog(const QString &catalogVersion, const QByteArray &data, QString *errorMessage) const
{
    return writeFileAtomically(catalogPath(catalogVersion), data, errorMessage);
}

bool GuideCache::storeArticle(const QString &catalogVersion,
                              const QString &relativePath,
                              const QByteArray &data,
                              const QString &expectedSha256,
                              QString *errorMessage) const
{
    if (!isSafeRelativePath(relativePath)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Guide article path must be relative to the guide cache.");
        return false;
    }

    if (!hasExpectedHash(data, expectedSha256)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Guide article hash does not match expected SHA-256.");
        return false;
    }

    return writeFileAtomically(articlePath(catalogVersion, relativePath), data, errorMessage);
}

QByteArray GuideCache::loadCatalog(const QString &catalogVersion, QString *errorMessage) const
{
    return readFile(catalogPath(catalogVersion), errorMessage);
}

QByteArray GuideCache::loadArticle(const QString &catalogVersion,
                                   const QString &relativePath,
                                   const QString &expectedSha256,
                                   QString *errorMessage) const
{
    if (!isSafeRelativePath(relativePath)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Guide article path must be relative to the guide cache.");
        return {};
    }

    QString readError;
    const QByteArray data = readFile(articlePath(catalogVersion, relativePath), &readError);
    if (!readError.isEmpty()) {
        if (errorMessage)
            *errorMessage = readError;
        return {};
    }

    if (!hasExpectedHash(data, expectedSha256)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cached guide article hash does not match expected SHA-256.");
        return {};
    }

    return data;
}

bool GuideCache::activateCatalogVersion(const QString &catalogVersion, QString *errorMessage) const
{
    return writeFileAtomically(QDir(m_rootPath).filePath(QString::fromLatin1(kActiveVersionFileName)),
                               catalogVersion.toUtf8(),
                               errorMessage);
}

QString GuideCache::sha256Hex(const QByteArray &data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

} // namespace flatlas::infrastructure::guide
