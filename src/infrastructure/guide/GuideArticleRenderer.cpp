#include "GuideArticleRenderer.h"

namespace flatlas::infrastructure::guide {
namespace {

QString escaped(const QString &text)
{
    return text.toHtmlEscaped();
}

void appendHeading(QString &html, const QString &title)
{
    if (!title.trimmed().isEmpty())
        html += QStringLiteral("<h2>%1</h2>").arg(escaped(title));
}

void appendParagraph(QString &html, const flatlas::domain::guide::GuideBlock &block)
{
    appendHeading(html, block.title);
    if (!block.text.trimmed().isEmpty())
        html += QStringLiteral("<p>%1</p>").arg(escaped(block.text).replace(QLatin1Char('\n'), QStringLiteral("<br>")));
}

void appendChecklist(QString &html, const flatlas::domain::guide::GuideBlock &block)
{
    appendHeading(html, block.title);
    html += QStringLiteral("<ul>");
    for (const QString &item : block.items)
        html += QStringLiteral("<li>%1</li>").arg(escaped(item));
    html += QStringLiteral("</ul>");
}

void appendFileMap(QString &html, const flatlas::domain::guide::GuideBlock &block)
{
    appendHeading(html, block.title);
    html += QStringLiteral("<table cellspacing=\"0\" cellpadding=\"4\" border=\"1\">"
                           "<thead><tr><th>Path</th><th>Purpose</th></tr></thead><tbody>");
    for (const auto &item : block.fileItems) {
        html += QStringLiteral("<tr><td><code>%1</code></td><td>%2</td></tr>")
                    .arg(escaped(item.path), escaped(item.purpose.isEmpty() ? item.text : item.purpose));
    }
    html += QStringLiteral("</tbody></table>");
}

void appendCode(QString &html, const flatlas::domain::guide::GuideBlock &block)
{
    appendHeading(html, block.title);
    if (!block.language.trimmed().isEmpty())
        html += QStringLiteral("<p><small>%1</small></p>").arg(escaped(block.language));
    html += QStringLiteral("<pre><code>%1</code></pre>").arg(escaped(block.text));
}

void appendRelated(QString &html, const flatlas::domain::guide::GuideBlock &block)
{
    appendHeading(html, block.title.isEmpty() ? QStringLiteral("Related") : block.title);
    html += QStringLiteral("<ul>");
    for (const QString &articleId : block.articleIds)
        html += QStringLiteral("<li><code>%1</code></li>").arg(escaped(articleId));
    html += QStringLiteral("</ul>");
}

void appendUnknownBlock(QString &html, const flatlas::domain::guide::GuideBlock &block)
{
    appendHeading(html, block.title);
    if (!block.text.trimmed().isEmpty())
        html += QStringLiteral("<p>%1</p>").arg(escaped(block.text));
}

} // namespace

QString GuideArticleRenderer::renderHtml(const flatlas::domain::guide::GuideArticle &article) const
{
    QString html;
    html += QStringLiteral("<html><body>");
    html += QStringLiteral("<h1>%1</h1>").arg(escaped(article.title));
    if (!article.summary.trimmed().isEmpty())
        html += QStringLiteral("<p><i>%1</i></p>").arg(escaped(article.summary));

    if (!article.category.trimmed().isEmpty())
        html += QStringLiteral("<p><small>%1</small></p>").arg(escaped(article.category));

    for (const auto &block : article.blocks) {
        if (block.type == QStringLiteral("paragraph")) {
            appendParagraph(html, block);
        } else if (block.type == QStringLiteral("checklist")) {
            appendChecklist(html, block);
        } else if (block.type == QStringLiteral("file-map")) {
            appendFileMap(html, block);
        } else if (block.type == QStringLiteral("code")) {
            appendCode(html, block);
        } else if (block.type == QStringLiteral("related")) {
            appendRelated(html, block);
        } else {
            appendUnknownBlock(html, block);
        }
    }

    html += QStringLiteral("</body></html>");
    return html;
}

} // namespace flatlas::infrastructure::guide
