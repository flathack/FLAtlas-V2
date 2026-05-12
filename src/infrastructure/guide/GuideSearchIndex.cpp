#include "GuideSearchIndex.h"

#include <algorithm>

namespace flatlas::infrastructure::guide {
namespace {

bool languageMatches(const QString &entryLanguage, const QString &requestedLanguage)
{
    return requestedLanguage.isEmpty() || entryLanguage.compare(requestedLanguage, Qt::CaseInsensitive) == 0;
}

bool containsTerm(const QString &text, const QString &term)
{
    return text.contains(term, Qt::CaseInsensitive);
}

int scoreStringList(const QStringList &values, const QString &term, int weight)
{
    for (const QString &value : values) {
        if (containsTerm(value, term))
            return weight;
    }
    return 0;
}

int scoreBlock(const flatlas::domain::guide::GuideBlock &block, const QString &term)
{
    int score = 0;
    if (containsTerm(block.title, term))
        score += 25;
    if (containsTerm(block.text, term))
        score += 15;
    score += scoreStringList(block.items, term, 12);
    score += scoreStringList(block.articleIds, term, 4);

    for (const auto &item : block.fileItems) {
        if (containsTerm(item.path, term))
            score += 12;
        if (containsTerm(item.purpose, term))
            score += 12;
        if (containsTerm(item.text, term))
            score += 8;
    }

    return score;
}

int scoreArticle(const flatlas::domain::guide::GuideArticle &article, const QString &term)
{
    int score = 0;
    if (containsTerm(article.title, term))
        score += 100;
    if (containsTerm(article.summary, term))
        score += 60;
    if (containsTerm(article.category, term))
        score += 30;
    score += scoreStringList(article.tags, term, 80);
    score += scoreStringList(article.contexts, term, 50);

    for (const auto &block : article.blocks)
        score += scoreBlock(block, term);

    return score;
}

int scoreEntry(const flatlas::domain::guide::GuideCatalogEntry &entry, const QString &term)
{
    int score = 0;
    if (containsTerm(entry.title, term))
        score += 100;
    if (containsTerm(entry.summary, term))
        score += 60;
    if (containsTerm(entry.category, term))
        score += 30;
    if (containsTerm(entry.id, term))
        score += 20;
    score += scoreStringList(entry.tags, term, 80);
    score += scoreStringList(entry.contexts, term, 50);
    return score;
}

const flatlas::domain::guide::GuideArticle *findArticle(const QVector<flatlas::domain::guide::GuideArticle> &articles,
                                                        const QString &id,
                                                        const QString &language)
{
    for (const auto &article : articles) {
        if (article.id == id && languageMatches(article.language, language))
            return &article;
    }
    return nullptr;
}

QStringList queryTerms(const QString &query)
{
    return query.split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

} // namespace

void GuideSearchIndex::setCatalog(const flatlas::domain::guide::GuideCatalog &catalog)
{
    m_catalog = catalog;
}

void GuideSearchIndex::addArticle(const flatlas::domain::guide::GuideArticle &article)
{
    m_articles.append(article);
}

QVector<GuideSearchResult> GuideSearchIndex::search(const QString &query, const QString &language) const
{
    const QStringList terms = queryTerms(query.trimmed());
    if (terms.isEmpty())
        return {};

    QVector<GuideSearchResult> results;
    for (const auto &entry : m_catalog.articles) {
        if (!languageMatches(entry.language, language))
            continue;

        int score = 0;
        const auto *article = findArticle(m_articles, entry.id, entry.language);
        for (const QString &term : terms) {
            score += scoreEntry(entry, term);
            if (article)
                score += scoreArticle(*article, term);
        }

        if (score > 0)
            results.append({entry, score});
    }

    std::sort(results.begin(), results.end(), [](const GuideSearchResult &left, const GuideSearchResult &right) {
        if (left.score == right.score)
            return left.entry.title < right.entry.title;
        return left.score > right.score;
    });

    return results;
}

} // namespace flatlas::infrastructure::guide
