#include "MainWindow.h"
#include "WelcomePage.h"
#include "BrowserPanel.h"
#include "PropertiesPanel.h"
#include "CenterTabWidget.h"
#include "SettingsDialog.h"
#include "core/Config.h"
#include "core/Theme.h"
#include "core/I18n.h"
#include "core/UndoManager.h"
#include "editors/system/SystemEditorPage.h"
#include "editors/system/SystemCreationWizard.h"
#include "editors/ini/IniEditorPage.h"
#include "editors/universe/UniverseEditorPage.h"
#include "editors/base/BaseEditorPage.h"
#include "editors/base/BaseBuilder.h"
#include "editors/trade/TradeRoutePage.h"
#include "domain/SystemDocument.h"
#include "domain/UniverseData.h"

#include <QCloseEvent>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QSplitter>
#include <QStatusBar>
#include <QSettings>
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("FLAtlas V2"));
    setMinimumSize(960, 620);
    resize(1600, 900);

    createMenus();
    createPanels();
    createStatusBar();
    restoreWindowState();
}

MainWindow::~MainWindow() = default;

void MainWindow::createMenus()
{
    // --- File ---
    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&New System..."), this, [this]() { newSystem(); });
    fileMenu->addAction(tr("Open &System..."), this, [this]() { openSystemFile(); });
    fileMenu->addAction(tr("Open &Universe..."), this, [this]() { openUniverseFile(); });
    fileMenu->addAction(tr("Open &Base Editor"), this, [this]() { openBaseEditor(); });
    fileMenu->addAction(tr("&Trade Routes"), this, [this]() { openTradeRoutes(); });
    fileMenu->addAction(tr("Open &INI..."), this, [this]() { openIniFile(); });
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Save"), QKeySequence::Save, this, [this]() { saveCurrentFile(); });
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Settings..."), this, [this]() {
        flatlas::ui::SettingsDialog dlg(this);
        dlg.exec();
    });
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    // --- Edit ---
    auto *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(tr("&Undo"), QKeySequence::Undo, this, []() {
        flatlas::core::UndoManager::instance().undo();
    });
    editMenu->addAction(tr("&Redo"), QKeySequence::Redo, this, []() {
        flatlas::core::UndoManager::instance().redo();
    });

    // --- View ---
    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(tr("System &Names"), this, []() { /* TODO Phase 4 */ });
    viewMenu->addAction(tr("&Grid"), this, []() { /* TODO Phase 4 */ });

    // --- Tools ---
    auto *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    toolsMenu->addAction(tr("&Trade Route Analysis..."), this, []() { /* TODO Phase 11 */ });
    toolsMenu->addAction(tr("&Model Viewer..."), this, []() { /* TODO Phase 16 */ });
    toolsMenu->addAction(tr("&Character Preview..."), this, []() { /* TODO Phase 16 */ });

    // --- Settings ---
    auto *settingsMenu = menuBar()->addMenu(tr("&Settings"));
    auto *themeMenu = settingsMenu->addMenu(tr("&Theme"));
    for (const auto &theme : flatlas::core::Theme::instance().availableThemes()) {
        themeMenu->addAction(theme, this, [theme]() {
            flatlas::core::Theme::instance().apply(theme);
            flatlas::core::Config::instance().setString("theme", theme);
            flatlas::core::Config::instance().save();
        });
    }
    auto *langMenu = settingsMenu->addMenu(tr("&Language"));
    for (const auto &lang : flatlas::core::I18n::availableLanguages()) {
        langMenu->addAction(lang, this, [lang]() {
            flatlas::core::I18n::instance().setLanguage(lang);
            flatlas::core::Config::instance().setString("language", lang);
            flatlas::core::Config::instance().save();
        });
    }

    // --- Help ---
    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About FLAtlas..."), this, [this]() {
        // TODO Phase 23
    });
}

void MainWindow::createPanels()
{
    // 3-Panel-Layout: Browser | Center Tabs | Properties
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);

    m_browserPanel = new flatlas::ui::BrowserPanel(this);
    m_centerTabs = new flatlas::ui::CenterTabWidget(this);
    m_propertiesPanel = new flatlas::ui::PropertiesPanel(this);

    m_mainSplitter->addWidget(m_browserPanel);
    m_mainSplitter->addWidget(m_centerTabs);
    m_mainSplitter->addWidget(m_propertiesPanel);

    // Proportionen: 240 | stretch | 320
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setStretchFactor(2, 0);
    m_mainSplitter->setSizes({240, 1040, 320});

    setCentralWidget(m_mainSplitter);

    // Welcome-Page als ersten Tab
    auto *welcomePage = new flatlas::ui::WelcomePage(this);
    m_centerTabs->addTab(welcomePage, tr("Welcome"));
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveWindowState();
    event->accept();
}

void MainWindow::restoreWindowState()
{
    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("mainwindow/geometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("mainwindow/state")).toByteArray());
}

void MainWindow::saveWindowState()
{
    QSettings settings;
    settings.setValue(QStringLiteral("mainwindow/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("mainwindow/state"), saveState());
}

void MainWindow::openSystemFile()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("Open System INI"), QString(),
        tr("INI Files (*.ini);;All Files (*)"));
    if (filePath.isEmpty())
        return;

    auto *editor = new flatlas::editors::SystemEditorPage(this);
    if (!editor->loadFile(filePath)) {
        QMessageBox::warning(this, tr("Error"),
                             tr("Could not load system file:\n%1").arg(filePath));
        delete editor;
        return;
    }

    int idx = m_centerTabs->addTab(editor, editor->document()->name());
    m_centerTabs->setCurrentIndex(idx);

    connect(editor, &flatlas::editors::SystemEditorPage::titleChanged,
            this, [this, editor](const QString &title) {
        int i = m_centerTabs->indexOf(editor);
        if (i >= 0)
            m_centerTabs->setTabText(i, title);
    });

    statusBar()->showMessage(tr("Loaded: %1").arg(filePath), 3000);
}

void MainWindow::saveCurrentSystem()
{
    auto *editor = currentSystemEditor();
    if (!editor)
        return;

    if (editor->filePath().isEmpty()) {
        const QString filePath = QFileDialog::getSaveFileName(
            this, tr("Save System INI"), QString(),
            tr("INI Files (*.ini);;All Files (*)"));
        if (filePath.isEmpty())
            return;
        if (editor->saveAs(filePath))
            statusBar()->showMessage(tr("Saved: %1").arg(filePath), 3000);
        else
            QMessageBox::warning(this, tr("Error"), tr("Could not save file."));
    } else {
        if (editor->save())
            statusBar()->showMessage(tr("Saved"), 3000);
        else
            QMessageBox::warning(this, tr("Error"), tr("Could not save file."));
    }
}

void MainWindow::newSystem()
{
    flatlas::editors::SystemCreationWizard wizard(this);
    if (wizard.exec() != QDialog::Accepted)
        return;

    auto doc = wizard.createDocument();
    if (!doc)
        return;

    auto *editor = new flatlas::editors::SystemEditorPage(this);
    editor->setDocument(std::move(doc));

    int idx = m_centerTabs->addTab(editor, editor->document()->name());
    m_centerTabs->setCurrentIndex(idx);

    connect(editor, &flatlas::editors::SystemEditorPage::titleChanged,
            this, [this, editor](const QString &title) {
        int i = m_centerTabs->indexOf(editor);
        if (i >= 0)
            m_centerTabs->setTabText(i, title);
    });

    statusBar()->showMessage(tr("New system created"), 3000);
}

flatlas::editors::SystemEditorPage *MainWindow::currentSystemEditor() const
{
    return qobject_cast<flatlas::editors::SystemEditorPage *>(
        m_centerTabs->currentWidget());
}

void MainWindow::openIniFile()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("Open INI File"), QString(),
        tr("INI Files (*.ini);;All Files (*)"));
    if (filePath.isEmpty())
        return;

    auto *editor = new flatlas::editors::IniEditorPage(this);
    if (!editor->openFile(filePath)) {
        QMessageBox::warning(this, tr("Error"),
                             tr("Could not open file:\n%1").arg(filePath));
        delete editor;
        return;
    }

    int idx = m_centerTabs->addTab(editor, editor->fileName());
    m_centerTabs->setCurrentIndex(idx);

    connect(editor, &flatlas::editors::IniEditorPage::titleChanged,
            this, [this, editor](const QString &title) {
        int i = m_centerTabs->indexOf(editor);
        if (i >= 0)
            m_centerTabs->setTabText(i, title);
    });

    statusBar()->showMessage(tr("Opened: %1").arg(filePath), 3000);
}

void MainWindow::saveCurrentFile()
{
    // Try system editor first
    auto *sysEditor = currentSystemEditor();
    if (sysEditor) {
        saveCurrentSystem();
        return;
    }

    // Try INI editor
    auto *iniEditor = qobject_cast<flatlas::editors::IniEditorPage *>(
        m_centerTabs->currentWidget());
    if (iniEditor) {
        if (iniEditor->filePath().isEmpty()) {
            const QString filePath = QFileDialog::getSaveFileName(
                this, tr("Save INI File"), QString(),
                tr("INI Files (*.ini);;All Files (*)"));
            if (filePath.isEmpty())
                return;
            if (iniEditor->saveAs(filePath))
                statusBar()->showMessage(tr("Saved: %1").arg(filePath), 3000);
            else
                QMessageBox::warning(this, tr("Error"), tr("Could not save file."));
        } else {
            if (iniEditor->save())
                statusBar()->showMessage(tr("Saved"), 3000);
            else
                QMessageBox::warning(this, tr("Error"), tr("Could not save file."));
        }
    }

    // Try universe editor
    auto *universeEditor = qobject_cast<flatlas::editors::UniverseEditorPage *>(
        m_centerTabs->currentWidget());
    if (universeEditor) {
        if (universeEditor->save())
            statusBar()->showMessage(tr("Saved"), 3000);
        else
            QMessageBox::warning(this, tr("Error"), tr("Could not save file."));
    }

    // Try base editor
    auto *baseEditor = qobject_cast<flatlas::editors::BaseEditorPage *>(
        m_centerTabs->currentWidget());
    if (baseEditor) {
        const QString filePath = QFileDialog::getSaveFileName(
            this, tr("Save Base INI"), QString(),
            tr("INI Files (*.ini);;All Files (*)"));
        if (!filePath.isEmpty()) {
            if (baseEditor->save(filePath))
                statusBar()->showMessage(tr("Saved: %1").arg(filePath), 3000);
            else
                QMessageBox::warning(this, tr("Error"), tr("Could not save file."));
        }
    }
}

void MainWindow::openUniverseFile()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("Open Universe.ini"), QString(),
        tr("INI Files (*.ini);;All Files (*)"));
    if (filePath.isEmpty())
        return;

    auto *editor = new flatlas::editors::UniverseEditorPage(this);
    if (!editor->loadFile(filePath)) {
        QMessageBox::warning(this, tr("Error"),
                             tr("Could not load Universe file:\n%1").arg(filePath));
        delete editor;
        return;
    }

    int idx = m_centerTabs->addTab(editor,
        QStringLiteral("Universe (%1)").arg(editor->data()->systemCount()));
    m_centerTabs->setCurrentIndex(idx);

    connect(editor, &flatlas::editors::UniverseEditorPage::titleChanged,
            this, [this, editor](const QString &title) {
        int i = m_centerTabs->indexOf(editor);
        if (i >= 0)
            m_centerTabs->setTabText(i, title);
    });

    connect(editor, &flatlas::editors::UniverseEditorPage::openSystemRequested,
            this, &MainWindow::openSystemFromUniverse);

    statusBar()->showMessage(tr("Loaded Universe: %1").arg(filePath), 3000);
}

void MainWindow::openSystemFromUniverse(const QString &nickname, const QString &systemFile)
{
    // Resolve relative path from universe file context
    QString resolvedPath = systemFile;

    // Check if already open
    for (int i = 0; i < m_centerTabs->count(); ++i) {
        auto *sysEditor = qobject_cast<flatlas::editors::SystemEditorPage *>(
            m_centerTabs->widget(i));
        if (sysEditor && sysEditor->document() &&
            sysEditor->document()->name().compare(nickname, Qt::CaseInsensitive) == 0) {
            m_centerTabs->setCurrentIndex(i);
            return;
        }
    }

    auto *editor = new flatlas::editors::SystemEditorPage(this);
    if (!editor->loadFile(resolvedPath)) {
        QMessageBox::warning(this, tr("Error"),
                             tr("Could not load system file:\n%1").arg(resolvedPath));
        delete editor;
        return;
    }

    int idx = m_centerTabs->addTab(editor, editor->document()->name());
    m_centerTabs->setCurrentIndex(idx);

    connect(editor, &flatlas::editors::SystemEditorPage::titleChanged,
            this, [this, editor](const QString &title) {
        int i = m_centerTabs->indexOf(editor);
        if (i >= 0)
            m_centerTabs->setTabText(i, title);
    });

    statusBar()->showMessage(tr("Opened system: %1").arg(nickname), 3000);
}

void MainWindow::openBaseEditor()
{
    auto *editor = new flatlas::editors::BaseEditorPage(this);

    int idx = m_centerTabs->addTab(editor, tr("Base Editor (new)"));
    m_centerTabs->setCurrentIndex(idx);

    connect(editor, &flatlas::editors::BaseEditorPage::titleChanged,
            this, [this, editor](const QString &title) {
        int i = m_centerTabs->indexOf(editor);
        if (i >= 0)
            m_centerTabs->setTabText(i, title);
    });

    statusBar()->showMessage(tr("Base Editor opened"), 3000);
}

void MainWindow::openTradeRoutes()
{
    auto *page = new flatlas::editors::TradeRoutePage(this);

    int idx = m_centerTabs->addTab(page, tr("Trade Routes"));
    m_centerTabs->setCurrentIndex(idx);

    connect(page, &flatlas::editors::TradeRoutePage::titleChanged,
            this, [this, page](const QString &title) {
        int i = m_centerTabs->indexOf(page);
        if (i >= 0)
            m_centerTabs->setTabText(i, title);
    });

    statusBar()->showMessage(tr("Trade Routes opened"), 3000);
}
