#include "XmlInfocard.h"

#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QTextStream>

namespace flatlas::infrastructure {

// ---------------------------------------------------------------------------
// sanitize – make non-standard Freelancer XML parseable
// ---------------------------------------------------------------------------
QString XmlInfocard::sanitize(const QString &xml)
{
    QString s = xml.trimmed();

    // 1. Wrap in <RDL> root if missing
    if (!s.startsWith(QStringLiteral("<RDL"), Qt::CaseInsensitive)
        && !s.startsWith(QStringLiteral("<?xml"), Qt::CaseInsensitive)) {
        s = QStringLiteral("<RDL>") + s + QStringLiteral("</RDL>");
    }

    // 2. Fix unescaped '&' (but leave already-escaped &amp; &lt; etc.)
    static const QRegularExpression reAmp(QStringLiteral("&(?!amp;|lt;|gt;|quot;|apos;|#)"));
    s.replace(reAmp, QStringLiteral("&amp;"));

    // 3. Self-close known void tags that Freelancer sometimes leaves unclosed
    static const QRegularExpression reVoid(
        QStringLiteral("<(PARA|JUST|TRA|PUSH|POP)\\b([^/>]*)>"),
        QRegularExpression::CaseInsensitiveOption);
    s.replace(reVoid, QStringLiteral("<\\1\\2/>"));

    return s;
}

// ---------------------------------------------------------------------------
// parse – structured parse into InfocardData
// ---------------------------------------------------------------------------
InfocardData XmlInfocard::parse(const QString &xml)
{
    InfocardData result;
    if (xml.trimmed().isEmpty())
        return result;

    const QString cleaned = sanitize(xml);
    QXmlStreamReader reader(cleaned);

    QVector<InfocardRun> currentParagraph;
    InfocardRun currentRun;
    bool inText = false;
    bool firstText = true;

    while (!reader.atEnd()) {
        reader.readNext();

        if (reader.isStartElement()) {
            const auto name = reader.name().toString().toUpper();

            if (name == QLatin1String("TEXT")) {
                inText = true;
                currentRun = {};
                // Parse TEXT attributes for style
                const auto attrs = reader.attributes();
                if (attrs.hasAttribute(QStringLiteral("bold")))
                    currentRun.bold = attrs.value(QStringLiteral("bold")) == QLatin1String("true");
                if (attrs.hasAttribute(QStringLiteral("italic")))
                    currentRun.italic = attrs.value(QStringLiteral("italic")) == QLatin1String("true");
                if (attrs.hasAttribute(QStringLiteral("font_size")))
                    currentRun.fontSize = attrs.value(QStringLiteral("font_size")).toInt();
                if (attrs.hasAttribute(QStringLiteral("color")))
                    currentRun.color = attrs.value(QStringLiteral("color")).toString();
            } else if (name == QLatin1String("PARA") || name == QLatin1String("POP")) {
                // Paragraph break
                if (!currentParagraph.isEmpty()) {
                    result.paragraphs.append(currentParagraph);
                    currentParagraph.clear();
                }
            }
            // PUSH, TRA, JUST, RDL – ignored structurally
        } else if (reader.isCharacters() && inText) {
            currentRun.text += reader.text().toString();
        } else if (reader.isEndElement()) {
            const auto name = reader.name().toString().toUpper();
            if (name == QLatin1String("TEXT")) {
                inText = false;
                if (!currentRun.text.isEmpty()) {
                    if (firstText && result.title.isEmpty())
                        result.title = currentRun.text.trimmed();
                    firstText = false;
                    currentParagraph.append(currentRun);
                }
            }
        }
    }

    // Flush last paragraph
    if (!currentParagraph.isEmpty())
        result.paragraphs.append(currentParagraph);

    return result;
}

// ---------------------------------------------------------------------------
// parseToPlainText – strip all tags, return text only
// ---------------------------------------------------------------------------
QString XmlInfocard::parseToPlainText(const QString &xml)
{
    const auto data = parse(xml);
    QStringList lines;
    for (const auto &para : data.paragraphs) {
        QString line;
        for (const auto &run : para)
            line += run.text;
        lines.append(line);
    }
    return lines.join(QChar('\n'));
}

// ---------------------------------------------------------------------------
// toHtml – convert InfocardData to HTML for QTextBrowser
// ---------------------------------------------------------------------------
QString XmlInfocard::toHtml(const InfocardData &data)
{
    QString html;
    QTextStream ts(&html);
    ts << QStringLiteral("<html><body>");

    for (const auto &para : data.paragraphs) {
        ts << QStringLiteral("<p>");
        for (const auto &run : para) {
            QString style;
            if (run.bold)
                style += QStringLiteral("font-weight:bold;");
            if (run.italic)
                style += QStringLiteral("font-style:italic;");
            if (run.fontSize > 0)
                style += QStringLiteral("font-size:%1pt;").arg(run.fontSize);
            if (!run.color.isEmpty())
                style += QStringLiteral("color:%1;").arg(run.color);

            if (!style.isEmpty())
                ts << QStringLiteral("<span style=\"%1\">").arg(style);
            ts << run.text.toHtmlEscaped();
            if (!style.isEmpty())
                ts << QStringLiteral("</span>");
        }
        ts << QStringLiteral("</p>");
    }

    ts << QStringLiteral("</body></html>");
    return html;
}

// ---------------------------------------------------------------------------
// validate – check if infocard XML is parseable
// ---------------------------------------------------------------------------
bool XmlInfocard::validate(const QString &xml, QString &error)
{
    if (xml.trimmed().isEmpty()) {
        error = QStringLiteral("Infocard is empty");
        return false;
    }

    const QString cleaned = sanitize(xml);
    QXmlStreamReader reader(cleaned);

    while (!reader.atEnd())
        reader.readNext();

    if (reader.hasError()) {
        error = reader.errorString();
        return false;
    }

    error.clear();
    return true;
}

// ---------------------------------------------------------------------------
// wrapAsInfocard – create minimal valid infocard from plain text
// ---------------------------------------------------------------------------
QString XmlInfocard::wrapAsInfocard(const QString &plainText)
{
    QString escaped = plainText;
    escaped.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    escaped.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    escaped.replace(QLatin1Char('>'), QStringLiteral("&gt;"));

    const QStringList lines = escaped.split(QLatin1Char('\n'));
    QString result = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-16\"?>\n<RDL><PUSH/>\n");
    for (const auto &line : lines) {
        result += QStringLiteral("  <TEXT>") + line + QStringLiteral("</TEXT><PARA/>\n");
    }
    result += QStringLiteral("<POP/></RDL>");
    return result;
}

} // namespace flatlas::infrastructure
