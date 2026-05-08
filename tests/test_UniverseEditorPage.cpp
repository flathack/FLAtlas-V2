#include <QtTest>
#include "editors/universe/UniverseEditorPage.h"
#include "domain/UniverseData.h"
#include <QFile>
#include <QTabBar>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QTreeWidget>

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

    void multiverseShowsSectorSystemsOnlyInTheirSector()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString universePath = dir.filePath(QStringLiteral("universe.ini"));
        const QString multiPath = dir.filePath(QStringLiteral("multiuniverse.ini"));

        QFile universeFile(universePath);
        QVERIFY(universeFile.open(QIODevice::WriteOnly | QIODevice::Text));
        universeFile.write("[System]\n"
                           "nickname = Li01\n"
                           "file = Li01.ini\n"
                           "pos = 100, 200\n"
                           "\n"
                           "[System]\n"
                           "nickname = Sol\n"
                           "file = Sol.ini\n"
                           "pos = 300, 400\n");
        universeFile.close();

        QFile multiFile(multiPath);
        QVERIFY(multiFile.open(QIODevice::WriteOnly | QIODevice::Text));
        multiFile.write("[Sector]\n"
                        "mapping = sector1\n"
                        "system = Sol, 10, 20\n");
        multiFile.close();

        UniverseEditorPage editor;
        QVERIFY(editor.loadFile(universePath));

        auto *tree = editor.findChild<QTreeWidget *>();
        QVERIFY(tree);
        QCOMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->data(0, Qt::UserRole).toString(), QStringLiteral("Li01"));

        auto *tabs = editor.findChild<QTabBar *>();
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 2);
        QVERIFY(tabs->tabText(0).contains(QStringLiteral("(1)")));
        QVERIFY(tabs->tabText(1).contains(QStringLiteral("(1)")));

        tabs->setCurrentIndex(1);
        QCOMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->data(0, Qt::UserRole).toString(), QStringLiteral("Sol"));
    }
};

QTEST_MAIN(TestUniverseEditorPage)
#include "test_UniverseEditorPage.moc"
