#include "MainWindow.h"
#include "WelcomePage.h"
#include "BrowserPanel.h"
#include "PropertiesPanel.h"
#include "CenterTabWidget.h"

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
    fileMenu->addAction(tr("&Save"), this, []() { /* TODO Phase 5 */ }, QKeySequence::Save);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Settings..."), this, []() { /* TODO Phase 3 */ });
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), this, &QWidget::close, QKeySequence::Quit);

    // --- Edit ---
    auto *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(tr("&Undo"), this, []() { /* TODO Phase 1 */ }, QKeySequence::Undo);
    editMenu->addAction(tr("&Redo"), this, []() { /* TODO Phase 1 */ }, QKeySequence::Redo);

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
    for (const auto &theme : {tr("Dark"), tr("Light"), tr("Founder"), tr("XP")}) {
        themeMenu->addAction(theme, this, [theme]() {
            Q_UNUSED(theme); // TODO Phase 1
        });
    }
    auto *langMenu = settingsMenu->addMenu(tr("&Language"));
    langMenu->addAction(QStringLiteral("Deutsch"), this, []() { /* TODO Phase 1 */ });
    langMenu->addAction(QStringLiteral("English"), this, []() { /* TODO Phase 1 */ });

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
