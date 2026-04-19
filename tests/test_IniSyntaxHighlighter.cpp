// tests/test_IniSyntaxHighlighter.cpp – Phase 6: INI Syntax Highlighter tests

#include <QTest>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextLayout>
#include <QPlainTextEdit>
#include <QApplication>

#include "editors/ini/IniSyntaxHighlighter.h"

using namespace flatlas::editors;

class TestIniSyntaxHighlighter : public QObject
{
    Q_OBJECT

private:
    QList<QTextLayout::FormatRange> formatsForBlock(const QString &text, int blockIndex = 0)
    {
        QPlainTextEdit edit;
        IniSyntaxHighlighter hl(edit.document());
        edit.setPlainText(text);
        edit.show();
        QTEST_ASSERT(QTest::qWaitForWindowExposed(&edit, 2000));
        QCoreApplication::processEvents();

        QTextBlock block = edit.document()->findBlockByNumber(blockIndex);
        return block.layout()->formats();
    }

private slots:
    void highlightsSection()
    {
        auto formats = formatsForBlock(QStringLiteral("[SystemInfo]"));
        QVERIFY(!formats.isEmpty());
        bool foundBold = false;
        for (const auto &f : formats) {
            if (f.format.fontWeight() == QFont::Bold)
                foundBold = true;
        }
        QVERIFY(foundBold);
    }

    void highlightsComment()
    {
        auto formats = formatsForBlock(QStringLiteral("; this is a comment"));
        QVERIFY(!formats.isEmpty());
        bool foundItalic = false;
        for (const auto &f : formats) {
            if (f.format.fontItalic())
                foundItalic = true;
        }
        QVERIFY(foundItalic);
    }

    void highlightsSlashComment()
    {
        auto formats = formatsForBlock(QStringLiteral("// another comment"));
        QVERIFY(!formats.isEmpty());
        bool foundItalic = false;
        for (const auto &f : formats) {
            if (f.format.fontItalic())
                foundItalic = true;
        }
        QVERIFY(foundItalic);
    }

    void highlightsKeyValue()
    {
        auto formats = formatsForBlock(QStringLiteral("nickname = Li01"));
        QVERIFY(formats.size() >= 2);
        bool foundBold = false;
        for (const auto &f : formats) {
            if (f.format.fontWeight() == QFont::Bold)
                foundBold = true;
        }
        QVERIFY(foundBold);
    }

    void emptyLineNoFormats()
    {
        QTextDocument doc;
        IniSyntaxHighlighter hl(&doc);
        doc.setPlainText(QStringLiteral(""));

        QTextBlock block = doc.firstBlock();
        auto formats = block.layout()->formats();
        QVERIFY(formats.isEmpty());
    }

    void multipleLines()
    {
        QPlainTextEdit edit;
        IniSyntaxHighlighter hl(edit.document());
        edit.setPlainText(QStringLiteral(
            "[SystemInfo]\n"
            "nickname = Li01\n"
            "; comment\n"
            "ids_name = 12345\n"
        ));
        edit.show();
        QVERIFY(QTest::qWaitForWindowExposed(&edit, 2000));
        QCoreApplication::processEvents();

        int blockCount = edit.document()->blockCount();
        QVERIFY(blockCount >= 4);

        // First block (section) should have formats
        QTextBlock b0 = edit.document()->findBlockByNumber(0);
        QVERIFY(!b0.layout()->formats().isEmpty());

        // Second block (key=value) should have formats
        QTextBlock b1 = edit.document()->findBlockByNumber(1);
        QVERIFY(!b1.layout()->formats().isEmpty());

        // Third block (comment) should have formats
        QTextBlock b2 = edit.document()->findBlockByNumber(2);
        QVERIFY(!b2.layout()->formats().isEmpty());
    }
};

QTEST_MAIN(TestIniSyntaxHighlighter)
#include "test_IniSyntaxHighlighter.moc"
