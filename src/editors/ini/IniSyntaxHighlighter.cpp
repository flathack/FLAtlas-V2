#include "IniSyntaxHighlighter.h"

namespace flatlas::editors {

IniSyntaxHighlighter::IniSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // Section: [Name] – SeaGreen, bold
    m_sectionFormat.setForeground(QColor("#2e8b57"));
    m_sectionFormat.setFontWeight(QFont::Bold);

    // Key (before '='): RoyalBlue, bold
    m_keyFormat.setForeground(QColor("#4169e1"));
    m_keyFormat.setFontWeight(QFont::Bold);

    // Comment: gray, italic
    m_commentFormat.setForeground(QColor("#808080"));
    m_commentFormat.setFontItalic(true);

    // Number: DarkOrchid
    m_numberFormat.setForeground(QColor("#9932cc"));

    // Equals sign: DimGray
    m_equalsFormat.setForeground(QColor("#696969"));
}

void IniSyntaxHighlighter::highlightBlock(const QString &text)
{
    const QString trimmed = text.trimmed();

    // Empty line: nothing to highlight
    if (trimmed.isEmpty())
        return;

    // Comment line: ; or // to end of line
    if (trimmed.startsWith(QLatin1Char(';')) || trimmed.startsWith(QLatin1String("//"))) {
        setFormat(0, text.length(), m_commentFormat);
        return;
    }

    // Section header: [SectionName]
    if (trimmed.startsWith(QLatin1Char('[')) && trimmed.contains(QLatin1Char(']'))) {
        const int start = text.indexOf(QLatin1Char('['));
        const int end   = text.indexOf(QLatin1Char(']'));
        setFormat(start, end - start + 1, m_sectionFormat);
        // Inline comment after ]
        const int commentPos = text.indexOf(QLatin1Char(';'), end + 1);
        if (commentPos >= 0)
            setFormat(commentPos, text.length() - commentPos, m_commentFormat);
        return;
    }

    // Key = Value line
    const int eqPos = text.indexOf(QLatin1Char('='));
    if (eqPos > 0) {
        // Key part (before '=')
        setFormat(0, eqPos, m_keyFormat);

        // Equals sign
        setFormat(eqPos, 1, m_equalsFormat);

        // Value part (after '=')
        const int valueStart = eqPos + 1;

        // Check for inline comment in value
        const int commentPos = text.indexOf(QLatin1Char(';'), valueStart);
        if (commentPos >= 0) {
            setFormat(valueStart, commentPos - valueStart, m_valueFormat);
            setFormat(commentPos, text.length() - commentPos, m_commentFormat);
        } else {
            setFormat(valueStart, text.length() - valueStart, m_valueFormat);
        }

        // Highlight numbers within the value portion
        static const QRegularExpression numRe(QStringLiteral("\\b-?\\d+(\\.\\d+)?\\b"));
        const int searchEnd = (commentPos >= 0) ? commentPos : text.length();
        QRegularExpressionMatchIterator it = numRe.globalMatch(text, valueStart);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            if (match.capturedStart() < searchEnd)
                setFormat(match.capturedStart(), match.capturedLength(), m_numberFormat);
        }
        return;
    }
}

} // namespace flatlas::editors
