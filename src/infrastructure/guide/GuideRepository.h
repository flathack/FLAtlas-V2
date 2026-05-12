#pragma once

#include "domain/guide/GuideArticle.h"
#include "infrastructure/guide/GuideCache.h"
#include "infrastructure/guide/GuideJsonReader.h"

#include <QVector>

namespace flatlas::infrastructure::guide {

class GuideRepository
{
public:
    explicit GuideRepository(GuideCache cache);

    flatlas::domain::guide::GuideCatalog loadActiveCatalog(QString *errorMessage = nullptr) const;
    QVector<flatlas::domain::guide::GuideCatalogEntry> entriesForContext(const QString &contextId,
                                                                         const QString &language = QString()) const;
    flatlas::domain::guide::GuideArticle loadArticle(const QString &articleId,
                                                     const QString &language = QString(),
                                                     QString *errorMessage = nullptr) const;

private:
    flatlas::domain::guide::GuideCatalogEntry findEntry(const flatlas::domain::guide::GuideCatalog &catalog,
                                                        const QString &articleId,
                                                        const QString &language) const;

    GuideCache m_cache;
    GuideJsonReader m_reader;
};

} // namespace flatlas::infrastructure::guide
