// test_IdsEditorPage.cpp – Phase 12 IDS Editor Page tests

#include <QtTest/QtTest>
#include "editors/ids/IdsEditorPage.h"

#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>

using namespace flatlas::editors;

class TestIdsEditorPage : public QObject {
    Q_OBJECT
private slots:
    void testCreation();
    void testImportCsv();
};

void TestIdsEditorPage::testCreation()
{
    IdsEditorPage page;
    QCOMPARE(page.stringCount(), 0);
}

void TestIdsEditorPage::testImportCsv()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString csvPath = dir.filePath("strings.csv");
    {
        QFile f(csvPath);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&f);
        out << "id;string\n"
            << "100;System Alpha\n"
            << "200;Base Beta\n"
            << "300;Planet Gamma\n";
    }

    IdsEditorPage page;
    page.importCsv(csvPath);
    QCOMPARE(page.stringCount(), 3);
}

QTEST_MAIN(TestIdsEditorPage)
#include "test_IdsEditorPage.moc"
