// test_NewsRumorEditor.cpp – Phase 18 News/Rumor Editor tests

#include <QtTest/QtTest>
#include <QTemporaryFile>
#include <QTextStream>
#include "editors/news/NewsRumorEditor.h"

using namespace flatlas::editors;

class TestNewsRumorEditor : public QObject {
    Q_OBJECT
private slots:
    void testEditorCreation();
    void testSetEntries();
    void testModifiedFlag();
    void testAddEntry();
    void testRemoveEntry();
    void testLoadFromIni();
    void testFilterByType();
};

void TestNewsRumorEditor::testEditorCreation()
{
    NewsRumorEditor editor;
    QCOMPARE(editor.entryCount(), 0);
    QVERIFY(!editor.isModified());
}

void TestNewsRumorEditor::testSetEntries()
{
    NewsRumorEditor editor;

    QVector<NewsEntry> entries;
    entries.append({NewsEntry::News, "test.ini", 0, "Breaking news", false});
    entries.append({NewsEntry::Rumor, "mbase.ini", 12345, "A rumor", false});
    entries.append({NewsEntry::Rumor, "mbase.ini", 0, "Inline rumor", false});

    editor.setEntries(entries);
    QCOMPARE(editor.entryCount(), 3);
    QVERIFY(!editor.isModified());
}

void TestNewsRumorEditor::testModifiedFlag()
{
    NewsRumorEditor editor;

    QVector<NewsEntry> entries;
    entries.append({NewsEntry::News, "test.ini", 0, "Text", false});
    editor.setEntries(entries);
    QVERIFY(!editor.isModified());

    // Modify an entry
    auto e = editor.entries();
    e[0].modified = true;
    e[0].text = "Changed";
    editor.setEntries(e);
    // setEntries preserves the modified flag
    QVERIFY(editor.isModified());
}

void TestNewsRumorEditor::testAddEntry()
{
    NewsRumorEditor editor;
    QCOMPARE(editor.entryCount(), 0);

    // Simulate add via the public API (setEntries)
    QVector<NewsEntry> entries;
    entries.append({NewsEntry::News, "<new>", 0, "", true});
    editor.setEntries(entries);
    QCOMPARE(editor.entryCount(), 1);
}

void TestNewsRumorEditor::testRemoveEntry()
{
    NewsRumorEditor editor;

    QVector<NewsEntry> entries;
    entries.append({NewsEntry::News, "a", 0, "First", false});
    entries.append({NewsEntry::Rumor, "b", 0, "Second", false});
    entries.append({NewsEntry::News, "c", 0, "Third", false});
    editor.setEntries(entries);
    QCOMPARE(editor.entryCount(), 3);

    // Remove middle entry manually
    entries.removeAt(1);
    editor.setEntries(entries);
    QCOMPARE(editor.entryCount(), 2);
    QCOMPARE(editor.entries()[0].text, QStringLiteral("First"));
    QCOMPARE(editor.entries()[1].text, QStringLiteral("Third"));
}

void TestNewsRumorEditor::testLoadFromIni()
{
    // Create a temporary INI file with news/rumor entries
    QTemporaryFile tmp;
    tmp.setAutoRemove(true);
    QVERIFY(tmp.open());
    {
        QTextStream ts(&tmp);
        ts << "[NewsItem]\n"
           << "news = Breaking headline\n"
           << "text = More news here\n"
           << "\n"
           << "[MBNpc]\n"
           << "rumor = A wild rumor\n"
           << "rumor_string = Another rumor\n";
    }
    tmp.close();

    NewsRumorEditor editor;
    editor.loadFromFile(tmp.fileName());

    // Should have parsed entries (news + rumors)
    QVERIFY(editor.entryCount() >= 2);

    // Check that at least one rumor was parsed
    bool hasRumor = false;
    for (const auto &e : editor.entries()) {
        if (e.type == NewsEntry::Rumor)
            hasRumor = true;
    }
    QVERIFY(hasRumor);
}

void TestNewsRumorEditor::testFilterByType()
{
    NewsRumorEditor editor;

    QVector<NewsEntry> entries;
    entries.append({NewsEntry::News, "a", 0, "News one", false});
    entries.append({NewsEntry::Rumor, "b", 0, "Rumor one", false});
    entries.append({NewsEntry::News, "c", 0, "News two", false});
    editor.setEntries(entries);

    QCOMPARE(editor.entryCount(), 3);

    // Count types
    int newsCount = 0, rumorCount = 0;
    for (const auto &e : editor.entries()) {
        if (e.type == NewsEntry::News) ++newsCount;
        else ++rumorCount;
    }
    QCOMPARE(newsCount, 2);
    QCOMPARE(rumorCount, 1);
}

QTEST_MAIN(TestNewsRumorEditor)
#include "test_NewsRumorEditor.moc"
