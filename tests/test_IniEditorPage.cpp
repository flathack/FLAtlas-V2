#include <QtTest>

#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QFileSystemModel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QTabBar>
#include <QTextCursor>
#include <QTimer>
#include <QTreeView>

#include "editors/ini/IniEditorPage.h"

using namespace flatlas::editors;

class TestIniEditorPage : public QObject {
    Q_OBJECT
private slots:
    void opensWorkspaceWithoutFile();
    void loadsWorkspacePanelsFromIniFile();
    void opensFilesInInternalTabs();
    void selectingSectionDoesNotMoveCursor();
    void opensFilesWithoutDirtyState();
    void closesDirtyBackgroundTabAfterDiscard();
    void treeRootStartsAboveDataFolder();
    void minimapTracksActiveFileAndUsesSmallerFont();
    void copySectionPlacesTextOnClipboard();
};

void TestIniEditorPage::opensWorkspaceWithoutFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    IniEditorPage page;
    page.openWorkspace(dir.path());

    QCOMPARE(page.filePath(), QString());
    QCOMPARE(page.fileName(), QStringLiteral("Untitled"));

    auto *sectionList = page.findChild<QListWidget *>(QStringLiteral("iniSectionList"));
    QVERIFY(sectionList != nullptr);
    QCOMPARE(sectionList->count(), 0);

    auto *historyList = page.findChild<QListWidget *>(QStringLiteral("iniHistoryList"));
    QVERIFY(historyList != nullptr);
    QCOMPARE(historyList->count(), 0);
}

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
    QCOMPARE(historyList->count(), 0);

    auto *fileTabs = page.findChild<QTabBar *>(QStringLiteral("iniFileTabs"));
    QVERIFY(fileTabs != nullptr);
    QCOMPARE(fileTabs->count(), 1);
}

void TestIniEditorPage::opensFilesInInternalTabs()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString firstPath = dir.filePath(QStringLiteral("first.ini"));
    const QString secondPath = dir.filePath(QStringLiteral("second.ini"));

    QFile firstFile(firstPath);
    QVERIFY(firstFile.open(QIODevice::WriteOnly | QIODevice::Text));
    firstFile.write("[System]\nnickname = Li01\n");
    firstFile.close();

    QFile secondFile(secondPath);
    QVERIFY(secondFile.open(QIODevice::WriteOnly | QIODevice::Text));
    secondFile.write("[Base]\nnickname = Li01_01_Base\n");
    secondFile.close();

    IniEditorPage page;
    page.openWorkspace(dir.path());
    QVERIFY(page.openFile(firstPath));
    QVERIFY(page.openFile(secondPath));

    auto *fileTabs = page.findChild<QTabBar *>(QStringLiteral("iniFileTabs"));
    QVERIFY(fileTabs != nullptr);
    QCOMPARE(fileTabs->count(), 2);
    QCOMPARE(fileTabs->tabText(0), QStringLiteral("first.ini"));
    QCOMPARE(fileTabs->tabText(1), QStringLiteral("second.ini"));
}

void TestIniEditorPage::selectingSectionDoesNotMoveCursor()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString iniPath = dir.filePath(QStringLiteral("sections.ini"));
    QFile iniFile(iniPath);
    QVERIFY(iniFile.open(QIODevice::WriteOnly | QIODevice::Text));
    iniFile.write(
        "[System]\n"
        "nickname = Li01\n"
        "file = systems\\li01\\li01.ini\n"
        "[Base]\n"
        "nickname = Li01_01_Base\n");
    iniFile.close();

    IniEditorPage page;
    QVERIFY(page.openFile(iniPath));

    auto *editor = page.findChild<QPlainTextEdit *>(QStringLiteral("iniCodeEditor"));
    QVERIFY(editor != nullptr);
    page.goToLine(2);
    const int originalBlock = editor->textCursor().blockNumber();

    auto *sectionList = page.findChild<QListWidget *>(QStringLiteral("iniSectionList"));
    QVERIFY(sectionList != nullptr);
    QVERIFY(sectionList->count() >= 2);
    sectionList->setCurrentRow(1);

    QCOMPARE(editor->textCursor().blockNumber(), originalBlock);
}

void TestIniEditorPage::opensFilesWithoutDirtyState()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString iniPath = dir.filePath(QStringLiteral("crlf.ini"));
    QFile iniFile(iniPath);
    QVERIFY(iniFile.open(QIODevice::WriteOnly | QIODevice::Text));
    iniFile.write("[System]\r\nnickname = Li01\r\n");
    iniFile.close();

    IniEditorPage page;
    QVERIFY(page.openFile(iniPath));

    QVERIFY(!page.isDirty());

    auto *fileTabs = page.findChild<QTabBar *>(QStringLiteral("iniFileTabs"));
    QVERIFY(fileTabs != nullptr);
    QCOMPARE(fileTabs->count(), 1);
    QCOMPARE(fileTabs->tabText(0), QStringLiteral("crlf.ini"));
}

void TestIniEditorPage::closesDirtyBackgroundTabAfterDiscard()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString firstPath = dir.filePath(QStringLiteral("first.ini"));
    const QString secondPath = dir.filePath(QStringLiteral("second.ini"));

    QFile firstFile(firstPath);
    QVERIFY(firstFile.open(QIODevice::WriteOnly | QIODevice::Text));
    firstFile.write("[System]\nnickname = Li01\n");
    firstFile.close();

    QFile secondFile(secondPath);
    QVERIFY(secondFile.open(QIODevice::WriteOnly | QIODevice::Text));
    secondFile.write("[Base]\nnickname = Li01_01_Base\n");
    secondFile.close();

    IniEditorPage page;
    page.openWorkspace(dir.path());
    QVERIFY(page.openFile(firstPath));

    auto *editor = page.findChild<QPlainTextEdit *>(QStringLiteral("iniCodeEditor"));
    QVERIFY(editor != nullptr);
    editor->moveCursor(QTextCursor::End);
    editor->insertPlainText(QStringLiteral("; dirty\n"));
    QVERIFY(page.isDirty());

    QVERIFY(page.openFile(secondPath));

    auto *fileTabs = page.findChild<QTabBar *>(QStringLiteral("iniFileTabs"));
    QVERIFY(fileTabs != nullptr);
    QCOMPARE(fileTabs->count(), 2);
    QCOMPARE(fileTabs->currentIndex(), 1);

    QTimer dialogCloser;
    dialogCloser.setInterval(10);
    connect(&dialogCloser, &QTimer::timeout, []() {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            auto *box = qobject_cast<QMessageBox *>(widget);
            if (!box)
                continue;
            const auto buttons = box->buttons();
            for (QAbstractButton *button : buttons) {
                if (box->buttonRole(button) == QMessageBox::DestructiveRole) {
                    button->click();
                    return;
                }
            }
        }
    });
    dialogCloser.start();

    QMetaObject::invokeMethod(fileTabs, "tabCloseRequested", Qt::QueuedConnection, Q_ARG(int, 0));

    QTRY_COMPARE(fileTabs->count(), 1);
    dialogCloser.stop();
    QCOMPARE(page.filePath(), secondPath);
    QVERIFY(!page.isDirty());
}

void TestIniEditorPage::treeRootStartsAboveDataFolder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QDir rootDir(dir.path());
    QVERIFY(rootDir.mkpath(QStringLiteral("DATA/UNIVERSE")));

    IniEditorPage page;
    page.openWorkspace(dir.filePath(QStringLiteral("DATA")));

    auto *tree = page.findChild<QTreeView *>(QStringLiteral("iniFileTree"));
    QVERIFY(tree != nullptr);
    auto *model = qobject_cast<QFileSystemModel *>(tree->model());
    QVERIFY(model != nullptr);

    QCOMPARE(QDir::fromNativeSeparators(model->filePath(tree->rootIndex())),
             QDir::fromNativeSeparators(dir.path()));
}

void TestIniEditorPage::minimapTracksActiveFileAndUsesSmallerFont()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString firstPath = dir.filePath(QStringLiteral("first.ini"));
    const QString secondPath = dir.filePath(QStringLiteral("second.ini"));

    QFile firstFile(firstPath);
    QVERIFY(firstFile.open(QIODevice::WriteOnly | QIODevice::Text));
    firstFile.write("[System]\nnickname = Li01\n");
    firstFile.close();

    QFile secondFile(secondPath);
    QVERIFY(secondFile.open(QIODevice::WriteOnly | QIODevice::Text));
    secondFile.write("[Base]\nnickname = Li01_01_Base\n");
    secondFile.close();

    IniEditorPage page;
    page.openWorkspace(dir.path());
    QVERIFY(page.openFile(firstPath));
    QVERIFY(page.openFile(secondPath));

    auto *editor = page.findChild<QPlainTextEdit *>(QStringLiteral("iniCodeEditor"));
    auto *minimap = page.findChild<QPlainTextEdit *>(QStringLiteral("iniMiniMap"));
    auto *fileTabs = page.findChild<QTabBar *>(QStringLiteral("iniFileTabs"));
    QVERIFY(editor != nullptr);
    QVERIFY(minimap != nullptr);
    QVERIFY(fileTabs != nullptr);

    QVERIFY(minimap->font().pointSizeF() < editor->font().pointSizeF());
    QCOMPARE(minimap->toPlainText(), QStringLiteral("[Base]\nnickname = Li01_01_Base\n"));

    fileTabs->setCurrentIndex(0);
    QTRY_COMPARE(page.filePath(), firstPath);
    QTRY_COMPARE(minimap->toPlainText(), QStringLiteral("[System]\nnickname = Li01\n"));
}

void TestIniEditorPage::copySectionPlacesTextOnClipboard()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString iniPath = dir.filePath(QStringLiteral("section.ini"));
    QFile iniFile(iniPath);
    QVERIFY(iniFile.open(QIODevice::WriteOnly | QIODevice::Text));
    iniFile.write(
        "[System]\n"
        "nickname = Li01\n"
        "file = systems\\li01\\li01.ini\n"
        "[Base]\n"
        "nickname = Li01_01_Base\n");
    iniFile.close();

    IniEditorPage page;
    QVERIFY(page.openFile(iniPath));
    page.goToLine(1);

    const auto actions = page.findChildren<QAction *>();
    QAction *copySectionAction = nullptr;
    for (QAction *action : actions) {
        if (action && action->text() == QStringLiteral("Copy Section")) {
            copySectionAction = action;
            break;
        }
    }

    QVERIFY(copySectionAction != nullptr);
    copySectionAction->trigger();

    QCOMPARE(QApplication::clipboard()->text(),
             QStringLiteral("[System]\nnickname = Li01\nfile = systems\\li01\\li01.ini"));
}

QTEST_MAIN(TestIniEditorPage)
#include "test_IniEditorPage.moc"