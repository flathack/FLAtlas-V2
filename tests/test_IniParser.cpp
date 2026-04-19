#include <QtTest>
#include <QTemporaryFile>
#include "infrastructure/parser/IniParser.h"

using namespace flatlas::infrastructure;

class TestIniParser : public QObject {
    Q_OBJECT
private slots:
    void parseEmptyInput()
    {
        auto doc = IniParser::parseText(QString());
        QVERIFY(doc.isEmpty());
    }

    void parseSingleSection()
    {
        auto doc = IniParser::parseText(QStringLiteral("[System]\nnickname = Li01\n"));
        QCOMPARE(doc.size(), 1);
        QCOMPARE(doc[0].name, QStringLiteral("System"));
        QCOMPARE(doc[0].entries.size(), 1);
        QCOMPARE(doc[0].entries[0].first, QStringLiteral("nickname"));
        QCOMPARE(doc[0].entries[0].second, QStringLiteral("Li01"));
    }

    void parseMultipleSections()
    {
        const QString text = QStringLiteral(
            "[System]\nnickname = Li01\nfile = universe\\systems\\Li01\\Li01.ini\n"
            "[System]\nnickname = Li02\nfile = universe\\systems\\Li02\\Li02.ini\n");
        auto doc = IniParser::parseText(text);
        QCOMPARE(doc.size(), 2);
        QCOMPARE(doc[0].value(QStringLiteral("nickname")), QStringLiteral("Li01"));
        QCOMPARE(doc[1].value(QStringLiteral("nickname")), QStringLiteral("Li02"));
    }

    void parseDuplicateKeys()
    {
        const QString text = QStringLiteral(
            "[Zone]\nnickname = Zone_Li01\n"
            "faction = li_n_grp, 0.9\nfaction = li_p_grp, 0.5\n");
        auto doc = IniParser::parseText(text);
        QCOMPARE(doc.size(), 1);
        auto vals = doc[0].values(QStringLiteral("faction"));
        QCOMPARE(vals.size(), 2);
        QCOMPARE(vals[0], QStringLiteral("li_n_grp, 0.9"));
        QCOMPARE(vals[1], QStringLiteral("li_p_grp, 0.5"));
    }

    void parseSemicolonComments()
    {
        const QString text = QStringLiteral(
            "; This is a comment\n[Object]\nnickname = Planet\n; Another comment\n");
        auto doc = IniParser::parseText(text);
        QCOMPARE(doc.size(), 1);
        QCOMPARE(doc[0].entries.size(), 1);
    }

    void parseDoubleSlashComments()
    {
        const QString text = QStringLiteral(
            "// C++ style comment\n[Object]\nnickname = Sun\n// end\n");
        auto doc = IniParser::parseText(text);
        QCOMPARE(doc.size(), 1);
        QCOMPARE(doc[0].entries.size(), 1);
    }

    void parseInlineComments()
    {
        const QString text = QStringLiteral(
            "[Ship]\nnickname = CSG_Patriot ; small fighter\n");
        auto doc = IniParser::parseText(text);
        QCOMPARE(doc[0].value(QStringLiteral("nickname")), QStringLiteral("CSG_Patriot"));
    }

    void parseEmptyLinesIgnored()
    {
        const QString text = QStringLiteral(
            "\n\n[Ship]\n\nnickname = CSG_Eagle\n\n");
        auto doc = IniParser::parseText(text);
        QCOMPARE(doc.size(), 1);
        QCOMPARE(doc[0].entries.size(), 1);
    }

    void parseKeyBeforeSectionIgnored()
    {
        const QString text = QStringLiteral(
            "orphan = value\n[System]\nnickname = Li01\n");
        auto doc = IniParser::parseText(text);
        QCOMPARE(doc.size(), 1);
        QCOMPARE(doc[0].entries.size(), 1);
    }

    void valueCaseInsensitiveLookup()
    {
        const QString text = QStringLiteral("[System]\nNickname = Li01\n");
        auto doc = IniParser::parseText(text);
        QCOMPARE(doc[0].value(QStringLiteral("nickname")), QStringLiteral("Li01"));
        QCOMPARE(doc[0].value(QStringLiteral("NICKNAME")), QStringLiteral("Li01"));
    }

    void valueDefaultOnMissing()
    {
        const QString text = QStringLiteral("[System]\nnickname = Li01\n");
        auto doc = IniParser::parseText(text);
        QCOMPARE(doc[0].value(QStringLiteral("missing"), QStringLiteral("default")),
                 QStringLiteral("default"));
    }

    void serializeRoundTrip()
    {
        const QString text = QStringLiteral(
            "[System]\nnickname = Li01\nfile = systems\\Li01.ini\n\n"
            "[System]\nnickname = Li02\n");
        auto doc = IniParser::parseText(text);
        QString serialized = IniParser::serialize(doc);
        auto doc2 = IniParser::parseText(serialized);
        QCOMPARE(doc2.size(), doc.size());
        for (int i = 0; i < doc.size(); ++i) {
            QCOMPARE(doc2[i].name, doc[i].name);
            QCOMPARE(doc2[i].entries.size(), doc[i].entries.size());
        }
    }

    void parseFileFromDisk()
    {
        QTemporaryFile tmp;
        tmp.setAutoRemove(true);
        QVERIFY(tmp.open());
        tmp.write("[Base]\nnickname = Li01_01_Base\nsystem = Li01\n");
        tmp.close();
        auto doc = IniParser::parseFile(tmp.fileName());
        QCOMPARE(doc.size(), 1);
        QCOMPARE(doc[0].value(QStringLiteral("system")), QStringLiteral("Li01"));
    }
};

QTEST_MAIN(TestIniParser)
#include "test_IniParser.moc"
