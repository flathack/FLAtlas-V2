#include "GuideRepository.h"

#include <utility>

namespace flatlas::infrastructure::guide {
namespace {

bool languageMatches(const QString &entryLanguage, const QString &requestedLanguage)
{
    return requestedLanguage.isEmpty() || entryLanguage.compare(requestedLanguage, Qt::CaseInsensitive) == 0;
}

} // namespace

GuideRepository::GuideRepository(GuideCache cache)
    : m_cache(std::move(cache))
{
}

flatlas::domain::guide::GuideCatalog GuideRepository::loadActiveCatalog(QString *errorMessage) const
{
    const QString version = m_cache.activeCatalogVersion();
    if (version.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No active guide catalog version is cached.");
        return {};
    }

    QString loadError;
    const QByteArray data = m_cache.loadCatalog(version, &loadError);
    if (!loadError.isEmpty()) {
        if (errorMessage)
            *errorMessage = loadError;
        return {};
    }

    return m_reader.readCatalog(data, errorMessage);
}

QVector<flatlas::domain::guide::GuideCatalogEntry> GuideRepository::entriesForContext(const QString &contextId,
                                                                                      const QString &language) const
{
    QString error;
    const auto catalog = loadActiveCatalog(&error);
    if (!error.isEmpty())
        return {};

    QVector<flatlas::domain::guide::GuideCatalogEntry> matches;
    for (const auto &entry : catalog.articles) {
        if (languageMatches(entry.language, language) && entry.contexts.contains(contextId, Qt::CaseInsensitive))
            matches.append(entry);
    }
    return matches;
}

flatlas::domain::guide::GuideArticle GuideRepository::loadArticle(const QString &articleId,
                                                                  const QString &language,
                                                                  QString *errorMessage) const
{
    const auto catalog = loadActiveCatalog(errorMessage);
    if (catalog.catalogVersion.isEmpty())
        return {};

    const auto entry = findEntry(catalog, articleId, language);
    if (entry.id.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Guide article is not listed in the active catalog.");
        return {};
    }

    QString loadError;
    const QByteArray data = m_cache.loadArticle(catalog.catalogVersion, entry.path, entry.sha256, &loadError);
    if (!loadError.isEmpty()) {
        if (errorMessage)
            *errorMessage = loadError;
        return {};
    }

    return m_reader.readArticle(data, errorMessage);
}

flatlas::domain::guide::GuideCatalogEntry GuideRepository::findEntry(const flatlas::domain::guide::GuideCatalog &catalog,
                                                                     const QString &articleId,
                                                                     const QString &language) const
{
    for (const auto &entry : catalog.articles) {
        if (entry.id == articleId && languageMatches(entry.language, language))
            return entry;
    }

    return {};
}

} // namespace flatlas::infrastructure::guide
