#pragma once
// infrastructure/parser/XmlInfocard.h – XML-Infocard-Parser
// TODO Phase 17
#include <QString>
namespace flatlas::infrastructure {
class XmlInfocard {
public:
    static QString parseToPlainText(const QString &xml);
    static bool validate(const QString &xml, QString &error);
};
} // namespace flatlas::infrastructure
