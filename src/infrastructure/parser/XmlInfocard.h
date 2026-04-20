#pragma once
// infrastructure/parser/XmlInfocard.h – Freelancer XML-Infocard parser/validator
//
// Freelancer infocards use a simplified XML/RDL dialect:
//   <RDL><PUSH/><TEXT>content</TEXT><PARA/><POP/></RDL>
// They are often not well-formed (missing root, unescaped &, unclosed tags).
// This parser handles those quirks robustly.

#include <QString>
#include <QStringList>

namespace flatlas::infrastructure {

/// Represents a single styled text run inside an infocard.
struct InfocardRun {
    QString text;
    bool    bold   = false;
    bool    italic = false;
    int     fontSize = 0;     // 0 = default
    QString color;            // e.g. "#FFFFFF", empty = default
};

/// Parsed infocard – a sequence of paragraphs, each containing styled runs.
struct InfocardData {
    QVector<QVector<InfocardRun>> paragraphs;
    QString title;   // extracted from first TEXT block if present
};

class XmlInfocard {
public:
    /// Parse raw Freelancer infocard XML to plain text (strips all tags).
    static QString parseToPlainText(const QString &xml);

    /// Parse into structured InfocardData with style information.
    static InfocardData parse(const QString &xml);

    /// Convert InfocardData to HTML for rich-text display.
    static QString toHtml(const InfocardData &data);

    /// Validate infocard XML. Returns true if valid; sets error on failure.
    static bool validate(const QString &xml, QString &error);

    /// Wrap bare text content into a minimal valid Freelancer infocard.
    static QString wrapAsInfocard(const QString &plainText);

private:
    /// Sanitise non-standard Freelancer XML so QXmlStreamReader can handle it.
    static QString sanitize(const QString &xml);
};

} // namespace flatlas::infrastructure
