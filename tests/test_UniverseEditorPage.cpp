#include <QtTest>
#include "editors/universe/UniverseEditorPage.h"
#include "domain/UniverseData.h"
#include <QTemporaryFile>

using namespace flatlas::editors;

class TestUniverseEditorPage : public QObject {
    Q_OBJECT
private slots:
    void createDefault()
    {
        UniverseEditorPage editor;
        QVERIFY(editor.data() == nullptr);
        QVERIFY(editor.filePath().isEmpty());
    }

    void loadInvalidFile()
    {
        UniverseEditorPage editor;
        bool ok = editor.loadFile(QStringLiteral("nonexistent.ini"));
        QVERIFY(!ok);
        QVERIFY(editor.data() == nullptr);
    }

    void loadValidFile()
    {
        QTemporaryFile file;
        file.setAutoRemove(true);
        QVERIFY(file.open());
        file.write("[System]\n"
                   "nickname = TestSys\n"
                   "file = TestSys.ini\n"
                   "pos = 1000, -2000\n");
        file.close();

        UniverseEditorPage editor;
        QSignalSpy spy(&editor, &UniverseEditorPage::titleChanged);

        bool ok = editor.loadFile(file.fileName());
        QVERIFY(ok);
        QVERIFY(editor.data() != nullptr);
        QCOMPARE(editor.data()->systemCount(), 1);
        QCOMPARE(editor.filePath(), file.fileName());
        QCOMPARE(spy.count(), 1);
    }

    void saveRoundtrip()
    {
        QTemporaryFile file;
        file.setAutoRemove(true);
        QVERIFY(file.open());
        file.write("[System]\n"
                   "nickname = Alpha\n"
                   "file = Alpha.ini\n"
                   "pos = 500, 700\n"
                   "[System]\n"
                   "nickname = Beta\n"
                   "file = Beta.ini\n"
                   "pos = -100, 300\n");
        file.close();

        UniverseEditorPage editor;
        QVERIFY(editor.loadFile(file.fileName()));
        QCOMPARE(editor.data()->systemCount(), 2);

        // Save and reload
        QVERIFY(editor.save());

        UniverseEditorPage editor2;
        QVERIFY(editor2.loadFile(file.fileName()));
        QCOMPARE(editor2.data()->systemCount(), 2);
        QCOMPARE(editor2.data()->systems[0].nickname, QStringLiteral("Alpha"));
    }
};

QTEST_MAIN(TestUniverseEditorPage)
#include "test_UniverseEditorPage.moc"
