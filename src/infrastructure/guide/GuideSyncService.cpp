#include "GuideSyncService.h"

#include <utility>

namespace flatlas::infrastructure::guide {

GuideSyncService::GuideSyncService(GuideCache cache)
    : m_cache(std::move(cache))
{
}

bool GuideSyncService::applyPackage(const SyncPackage &packageData, QString *errorMessage) const
{
    QString parseError;
    const auto catalog = m_reader.readCatalog(packageData.manifestJson, &parseError);
    if (!parseError.isEmpty()) {
        if (errorMessage)
            *errorMessage = parseError;
        return false;
    }

    if (catalog.catalogVersion.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Guide manifest does not contain a catalogVersion.");
        return false;
    }

    for (const auto &entry : catalog.articles) {
        if (!packageData.articlesByPath.contains(entry.path)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Guide package is missing article: %1").arg(entry.path);
            return false;
        }

        const QByteArray articleJson = packageData.articlesByPath.value(entry.path);
        if (GuideCache::sha256Hex(articleJson).compare(entry.sha256, Qt::CaseInsensitive) != 0) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Guide package article hash does not match manifest: %1").arg(entry.path);
            return false;
        }

        QString articleError;
        const auto article = m_reader.readArticle(articleJson, &articleError);
        if (!articleError.isEmpty()) {
            if (errorMessage)
                *errorMessage = articleError;
            return false;
        }

        if (article.id != entry.id || article.language != entry.language) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Guide article identity does not match manifest entry: %1").arg(entry.path);
            return false;
        }
    }

    QString cacheError;
    if (!m_cache.storeCatalog(catalog.catalogVersion, packageData.manifestJson, &cacheError)) {
        if (errorMessage)
            *errorMessage = cacheError;
        return false;
    }

    for (const auto &entry : catalog.articles) {
        if (!m_cache.storeArticle(catalog.catalogVersion,
                                  entry.path,
                                  packageData.articlesByPath.value(entry.path),
                                  entry.sha256,
                                  &cacheError)) {
            if (errorMessage)
                *errorMessage = cacheError;
            return false;
        }
    }

    if (!m_cache.activateCatalogVersion(catalog.catalogVersion, &cacheError)) {
        if (errorMessage)
            *errorMessage = cacheError;
        return false;
    }

    return true;
}

} // namespace flatlas::infrastructure::guide
