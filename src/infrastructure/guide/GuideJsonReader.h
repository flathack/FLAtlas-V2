#pragma once

#include "domain/guide/GuideArticle.h"

#include <QByteArray>
#include <QString>

namespace flatlas::infrastructure::guide {

class GuideJsonReader
{
public:
    flatlas::domain::guide::GuideCatalog readCatalog(const QByteArray &json, QString *errorMessage = nullptr) const;
    flatlas::domain::guide::GuideArticle readArticle(const QByteArray &json, QString *errorMessage = nullptr) const;
};

} // namespace flatlas::infrastructure::guide
