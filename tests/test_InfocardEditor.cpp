// test_InfocardEditor.cpp – Phase 17 Infocard-Handling tests

#include <QtTest/QtTest>
#include "infrastructure/parser/XmlInfocard.h"
#include "editors/infocard/InfocardEditor.h"

using namespace flatlas::infrastructure;
using namespace flatlas::editors;

class TestInfocardEditor : public QObject {
    Q_OBJECT
private slots:
    void testParseSimpleInfocard();
    void testParseToPlainText();
    void testParseMalformedAmpersand();
    void testParseUnclosedVoidTags();
    void testToHtmlBasic();
    void testValidateValid();
    void testValidateEmpty();
    void testWrapAsInfocard();
    void testEditorCreation();
    void testEditorLoadInfocard();
};

void TestInfocardEditor::testParseSimpleInfocard()
{
    const QString xml = QStringLiteral(
        "<RDL><PUSH/>"
        "<TEXT>Hello World</TEXT><PARA/>"
        "<TEXT>Second line</TEXT><PARA/>"
        "<POP/></RDL>");

    auto data = XmlInfocard::parse(xml);
    QCOMPARE(data.paragraphs.size(), 2);
    QCOMPARE(data.paragraphs[0].size(), 1);
    QCOMPARE(data.paragraphs[0][0].text, QStringLiteral("Hello World"));
    QCOMPARE(data.paragraphs[1][0].text, QStringLiteral("Second line"));
    QCOMPARE(data.title, QStringLiteral("Hello World"));
}

void TestInfocardEditor::testParseToPlainText()
{
    const QString xml = QStringLiteral(
        "<RDL><PUSH/>"
        "<TEXT>Line A</TEXT><PARA/>"
        "<TEXT>Line B</TEXT><PARA/>"
        "<POP/></RDL>");

    const QString plain = XmlInfocard::parseToPlainText(xml);
    QCOMPARE(plain, QStringLiteral("Line A\nLine B"));
}

void TestInfocardEditor::testParseMalformedAmpersand()
{
    // Freelancer infocards often have unescaped &
    const QString xml = QStringLiteral(
        "<RDL><PUSH/>"
        "<TEXT>Guns & Ammo</TEXT><PARA/>"
        "<POP/></RDL>");

    auto data = XmlInfocard::parse(xml);
    QCOMPARE(data.paragraphs.size(), 1);
    QCOMPARE(data.paragraphs[0][0].text, QStringLiteral("Guns & Ammo"));
}

void TestInfocardEditor::testParseUnclosedVoidTags()
{
    // Freelancer sometimes has <PARA> instead of <PARA/>
    const QString xml = QStringLiteral(
        "<RDL><PUSH>"
        "<TEXT>Test</TEXT><PARA>"
        "<POP></RDL>");

    auto data = XmlInfocard::parse(xml);
    QVERIFY(data.paragraphs.size() >= 1);
    QCOMPARE(data.paragraphs[0][0].text, QStringLiteral("Test"));
}

void TestInfocardEditor::testToHtmlBasic()
{
    const QString xml = QStringLiteral(
        "<RDL><PUSH/><TEXT>Hello</TEXT><PARA/><POP/></RDL>");

    auto data = XmlInfocard::parse(xml);
    const QString html = XmlInfocard::toHtml(data);
    QVERIFY(html.contains(QStringLiteral("Hello")));
    QVERIFY(html.contains(QStringLiteral("<p>")));
}

void TestInfocardEditor::testValidateValid()
{
    const QString xml = QStringLiteral(
        "<RDL><PUSH/><TEXT>Valid</TEXT><PARA/><POP/></RDL>");
    QString error;
    QVERIFY(XmlInfocard::validate(xml, error));
    QVERIFY(error.isEmpty());
}

void TestInfocardEditor::testValidateEmpty()
{
    QString error;
    QVERIFY(!XmlInfocard::validate(QStringLiteral(""), error));
    QCOMPARE(error, QStringLiteral("Infocard is empty"));
}

void TestInfocardEditor::testWrapAsInfocard()
{
    const QString plain = QStringLiteral("Hello\nWorld");
    const QString xml = XmlInfocard::wrapAsInfocard(plain);
    QVERIFY(xml.contains(QStringLiteral("<RDL>")));
    QVERIFY(xml.contains(QStringLiteral("<TEXT>Hello</TEXT>")));
    QVERIFY(xml.contains(QStringLiteral("<TEXT>World</TEXT>")));

    // Round-trip: parse back and check
    const QString back = XmlInfocard::parseToPlainText(xml);
    QCOMPARE(back, plain);
}

void TestInfocardEditor::testEditorCreation()
{
    InfocardEditor editor;
    QVERIFY(!editor.isModified());
    QVERIFY(editor.xmlSource().isEmpty());
}

void TestInfocardEditor::testEditorLoadInfocard()
{
    const QString xml = QStringLiteral(
        "<RDL><PUSH/><TEXT>Test Card</TEXT><PARA/><POP/></RDL>");

    InfocardEditor editor;
    QSignalSpy spy(&editor, &InfocardEditor::titleChanged);
    editor.loadInfocard(xml);

    QCOMPARE(editor.xmlSource(), xml);
    QVERIFY(!editor.isModified());  // loadInfocard resets modified flag
    QVERIFY(spy.count() >= 1);
}

QTEST_MAIN(TestInfocardEditor)
#include "test_InfocardEditor.moc"
