#include <QtTest>
#include "infrastructure/parser/IniParser.h"

class TestIniParser : public QObject {
    Q_OBJECT
private slots:
    void parseEmptyInput()
    {
        auto sections = flatlas::infrastructure::IniParser::parseText(QString());
        QVERIFY(sections.isEmpty());
    }
    void parseSingleSection()
    {
        const QString text = QStringLiteral("[System]\nnickname = Li01\n");
        auto sections = flatlas::infrastructure::IniParser::parseText(text);
        // TODO: Implement parseText, then verify result
        Q_UNUSED(sections);
    }
};

QTEST_MAIN(TestIniParser)
#include "test_IniParser.moc"
