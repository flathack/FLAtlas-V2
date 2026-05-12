#pragma once

#include "domain/guide/GuideArticle.h"

#include <QString>
#include <QVector>

namespace flatlas::infrastructure::guide {

struct GuideSearchResult {
    flatlas::domain::guide::GuideCatalogEntry entry;
    int score = 0;
};

class GuideSearchIndex
{
public:
    void setCatalog(const flatlas::domain::guide::GuideCatalog &catalog);
    void addArticle(const flatlas::domain::guide::GuideArticle &article);
    QVector<GuideSearchResult> search(const QString &query, const QString &language = QString()) const;

private:
    flatlas::domain::guide::GuideCatalog m_catalog;
    QVector<flatlas::domain::guide::GuideArticle> m_articles;
};

} // namespace flatlas::infrastructure::guide
