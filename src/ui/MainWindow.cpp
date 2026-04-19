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

#include <QCloseEvent>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QSplitter>
#include <QStatusBar>
#include <QSettings>
#include <QApplication>

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
    fileMenu->addAction(tr("&New System..."), this, []() { /* TODO Phase 5 */ });
    fileMenu->addAction(tr("&Open..."), this, []() { /* TODO Phase 5 */ });
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Save"), QKeySequence::Save, this, []() { /* TODO Phase 5 */ });
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
