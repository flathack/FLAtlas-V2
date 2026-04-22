#include <QtTest>

#include <QFile>
#include <QListWidget>
#include <QTemporaryDir>
#include <QTreeView>

#include "editors/ini/IniEditorPage.h"

using namespace flatlas::editors;

class TestIniEditorPage : public QObject {
    Q_OBJECT
private slots:
    void loadsWorkspacePanelsFromIniFile();
};

void TestIniEditorPage::loadsWorkspacePanelsFromIniFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString iniPath = dir.filePath(QStringLiteral("sample.ini"));
    QFile iniFile(iniPath);
    QVERIFY(iniFile.open(QIODevice::WriteOnly | QIODevice::Text));
    iniFile.write(
        "[System]\n"
        "nickname = Li01\n"
        "nickname = Li01_alt\n"
        "[Base]\n"
        "nickname = Li01_01_Base\n"
        "broken line\n");
    iniFile.close();

    IniEditorPage page;
    QVERIFY(page.openFile(iniPath));

    auto *sectionList = page.findChild<QListWidget *>(QStringLiteral("iniSectionList"));
    QVERIFY(sectionList != nullptr);
    QCOMPARE(sectionList->count(), 2);

    auto *diagnosticsView = page.findChild<QTreeView *>(QStringLiteral("iniDiagnosticsView"));
    QVERIFY(diagnosticsView != nullptr);
    QVERIFY(diagnosticsView->model() != nullptr);
    QVERIFY(diagnosticsView->model()->rowCount() >= 2);

    auto *historyList = page.findChild<QListWidget *>(QStringLiteral("iniHistoryList"));
    QVERIFY(historyList != nullptr);
    QVERIFY(historyList->count() >= 1);
}

QTEST_MAIN(TestIniEditorPage)
#include "test_IniEditorPage.moc"