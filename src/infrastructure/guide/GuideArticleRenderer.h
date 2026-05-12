#pragma once

#include "domain/guide/GuideArticle.h"

#include <QString>

namespace flatlas::infrastructure::guide {

class GuideArticleRenderer
{
public:
    QString renderHtml(const flatlas::domain::guide::GuideArticle &article) const;
};

} // namespace flatlas::infrastructure::guide
