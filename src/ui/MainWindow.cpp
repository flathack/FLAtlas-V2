#include "MainWindow.h"
#include "WelcomePage.h"
#include "BrowserPanel.h"
#include "PropertiesPanel.h"
#include "CenterTabWidget.h"
#include "SettingsDialog.h"
#include "core/Config.h"
#include "core/EditingContext.h"
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
#include "editors/ids/IdsEditorPage.h"
#include "editors/modmanager/ModManagerPage.h"
#include "editors/npc/NpcEditorPage.h"
#include "editors/infocard/InfocardEditor.h"
#include "editors/news/NewsRumorEditor.h"
#include "editors/jump/JumpConnectionDialog.h"
#include "tools/ScriptPatcher.h"
#include "tools/SpStarter.h"
#include "tools/UpdateChecker.h"
#include "tools/UpdateDownloader.h"
#include "tools/UpdateInstaller.h"
#include "tools/HelpBrowser.h"
#include "tools/PathFinderDialog.h"
#include "rendering/preview/ModelPreview.h"
#include "rendering/preview/CharacterPreview.h"
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
#include <QProgressDialog>
#include <QStandardPaths>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabBar>
#include <QStackedWidget>
#include <QDesktopServices>
#include <QUrl>

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

    // Editing-Kontext wiederherstellen und UI verbinden
    auto &ctx = flatlas::core::EditingContext::instance();
    ctx.restore();
    connect(&ctx, &flatlas::core::EditingContext::contextChanged,
            this, [this](const QString &profileId) {
        auto profile = flatlas::core::EditingContext::instance().editingProfile();
        if (profile.isValid()) {
            m_editingLabel->setText(tr("Currently Editing: %1").arg(profile.name));
            setWindowTitle(QStringLiteral("FLAtlas V2 – %1").arg(profile.name));
            statusBar()->showMessage(tr("Editing context: %1").arg(profile.name), 5000);
        } else {
            m_editingLabel->setText(tr("Currently Editing: -"));
            setWindowTitle(QStringLiteral("FLAtlas V2"));
        }
        Q_UNUSED(profileId)
    });
    // Apply restored context to UI
    if (ctx.hasContext()) {
        auto profile = ctx.editingProfile();
        m_editingLabel->setText(tr("Currently Editing: %1").arg(profile.name));
        setWindowTitle(QStringLiteral("FLAtlas V2 – %1").arg(profile.name));
    }

    // Beim ersten Start automatisch den Mod Manager öffnen
    if (!flatlas::core::Config::instance().getBool(QStringLiteral("firstRunDone"), false)) {
        flatlas::core::Config::instance().setBool(QStringLiteral("firstRunDone"), true);
        flatlas::core::Config::instance().save();
        QMetaObject::invokeMethod(this, &MainWindow::openModManager, Qt::QueuedConnection);
    }

    // Auto-Update-Check bei Start
    auto *checker = new flatlas::tools::UpdateChecker(this);
    connect(checker, &flatlas::tools::UpdateChecker::updateCheckFinished,
            this, [this, checker](const flatlas::tools::UpdateInfo &info) {
        checker->deleteLater();
        if (!info.errorMessage.isEmpty()) {
            statusBar()->showMessage(tr("Update check failed: %1").arg(info.errorMessage), 5000);
            return;
        }
        if (!info.available)
            return;

        auto answer = QMessageBox::question(
            this,
            tr("Update Available"),
            tr("Version %1 is available (current: %2).\n\n%3\n\nDownload and install?")
                .arg(info.latestVersion, info.currentVersion,
                     info.releaseNotes.left(500)),
            QMessageBox::Yes | QMessageBox::No);

        if (answer != QMessageBox::Yes || !info.downloadUrl.isValid())
            return;

        // Download starten
        const QString zipPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                                + QStringLiteral("/flatlas_update.zip");
        auto *dl = new flatlas::tools::UpdateDownloader(this);
        auto *progress = new QProgressDialog(tr("Downloading update..."), tr("Cancel"), 0, 100, this);
        progress->setWindowModality(Qt::WindowModal);
        progress->setMinimumDuration(0);

        connect(dl, &flatlas::tools::UpdateDownloader::progressChanged,
                progress, &QProgressDialog::setValue);
        connect(progress, &QProgressDialog::canceled, dl, &flatlas::tools::UpdateDownloader::cancel);
        connect(dl, &flatlas::tools::UpdateDownloader::downloadFailed,
                this, [this, dl, progress](const QString &err) {
            progress->close();
            progress->deleteLater();
            dl->deleteLater();
            QMessageBox::warning(this, tr("Download Failed"), err);
        });
        connect(dl, &flatlas::tools::UpdateDownloader::downloadFinished,
                this, [this, dl, progress](const QString &filePath) {
            progress->close();
            progress->deleteLater();
            dl->deleteLater();

            auto *installer = new flatlas::tools::UpdateInstaller(this);
            auto result = installer->prepare(filePath, QCoreApplication::applicationDirPath());
            if (!result.success) {
                QMessageBox::warning(this, tr("Update Failed"), result.errorMessage);
                installer->deleteLater();
                return;
            }
            auto restart = QMessageBox::question(
                this, tr("Update Ready"),
                tr("Update prepared. Restart now to apply?"),
                QMessageBox::Yes | QMessageBox::No);
            if (restart == QMessageBox::Yes) {
                installer->executeAndRestart();
            } else {
                installer->deleteLater();
            }
        });

        dl->download(info.downloadUrl, zipPath);
    });
    checker->checkForUpdates();
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
    fileMenu->addAction(tr("&IDS Editor"), this, [this]() { openIdsEditor(); });
    fileMenu->addAction(tr("&Mod Manager"), this, [this]() { openModManager(); });
    fileMenu->addAction(tr("&NPC Editor"), this, [this]() { openNpcEditor(); });
    fileMenu->addAction(tr("Info&card Editor"), this, [this]() { openInfocardEditor(); });
    fileMenu->addAction(tr("Ne&ws/Rumor Editor"), this, [this]() { openNewsRumorEditor(); });
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
    toolsMenu->addAction(tr("&Model Viewer..."), this, [this]() {
        auto *dlg = new flatlas::rendering::ModelPreview(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
    toolsMenu->addAction(tr("&Character Preview..."), this, [this]() {
        auto *dlg = new flatlas::rendering::CharacterPreview(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
    toolsMenu->addAction(tr("&Jump Connection..."), this, [this]() {
        auto *dlg = new flatlas::editors::JumpConnectionDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
    toolsMenu->addAction(tr("&Shortest Path..."), this, [this]() {
        // Versuche UniverseData vom aktiven UniverseEditorPage zu holen
        const flatlas::domain::UniverseData *udata = nullptr;
        if (m_centerTabs) {
            for (int i = 0; i < m_centerTabs->count(); ++i) {
                if (auto *uep = qobject_cast<flatlas::editors::UniverseEditorPage *>(m_centerTabs->widget(i))) {
                    udata = uep->data();
                    break;
                }
            }
        }
        if (!udata) {
            QMessageBox::information(this, tr("Shortest Path"),
                tr("Please open a Universe file first."));
            return;
        }
        auto *dlg = new flatlas::tools::PathFinderDialog(udata, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
    toolsMenu->addSeparator();
    toolsMenu->addAction(tr("Apply &OpenSP Patch..."), this, [this]() {
        const QString exe = QFileDialog::getOpenFileName(
            this, tr("Select Freelancer.exe"), {}, tr("Executable (*.exe)"));
        if (exe.isEmpty()) return;
        auto r = flatlas::tools::ScriptPatcher::applyOpenSpPatch(exe);
        if (r.success)
            statusBar()->showMessage(tr("OpenSP patch applied. Backup: %1").arg(r.backupPath), 5000);
        else
            statusBar()->showMessage(tr("Patch failed: %1").arg(r.errorMessage), 5000);
    });
    toolsMenu->addAction(tr("Apply &Resolution Patch..."), this, [this]() {
        const QString exe = QFileDialog::getOpenFileName(
            this, tr("Select Freelancer.exe"), {}, tr("Executable (*.exe)"));
        if (exe.isEmpty()) return;
        auto r = flatlas::tools::ScriptPatcher::applyResolutionPatch(exe, 1920, 1080);
        if (r.success)
            statusBar()->showMessage(tr("Resolution patch applied. Backup: %1").arg(r.backupPath), 5000);
        else
            statusBar()->showMessage(tr("Patch failed: %1").arg(r.errorMessage), 5000);
    });
    toolsMenu->addAction(tr("&Launch Freelancer..."), this, [this]() {
        const QString exe = QFileDialog::getOpenFileName(
            this, tr("Select Freelancer.exe"), {}, tr("Executable (*.exe)"));
        if (exe.isEmpty()) return;
        if (flatlas::tools::SpStarter::launch(exe))
            statusBar()->showMessage(tr("Freelancer launched."), 3000);
        else
            statusBar()->showMessage(tr("Failed to launch Freelancer."), 5000);
    });

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
    helpMenu->addAction(tr("&Help Contents"), QKeySequence::HelpContents, this, [this]() {
        showContextHelp();
    });
    helpMenu->addSeparator();
    helpMenu->addAction(tr("Check for &Updates..."), this, [this]() {
        statusBar()->showMessage(tr("Checking for updates..."), 3000);
        auto *checker = new flatlas::tools::UpdateChecker(this);
        connect(checker, &flatlas::tools::UpdateChecker::updateCheckFinished,
                this, [this, checker](const flatlas::tools::UpdateInfo &info) {
            checker->deleteLater();
            if (!info.errorMessage.isEmpty()) {
                QMessageBox::warning(this, tr("Update Check"), info.errorMessage);
                return;
            }
            if (info.available) {
                QMessageBox::information(
                    this, tr("Update Available"),
                    tr("Version %1 is available (current: %2).\n\n%3")
                        .arg(info.latestVersion, info.currentVersion,
                             info.releaseNotes.left(500)));
            } else {
                QMessageBox::information(
                    this, tr("No Update"),
                    tr("You are running the latest version (%1).").arg(info.currentVersion));
            }
        });
        checker->checkForUpdates();
    });
    helpMenu->addSeparator();
    helpMenu->addAction(tr("&About FLAtlas..."), this, [this]() {
        QMessageBox::about(this, tr("About FLAtlas"),
            tr("<h2>FLAtlas V2</h2>"
               "<p>Version %1</p>"
               "<p>A comprehensive editor for Freelancer game data.</p>"
               "<p>&copy; 2024–2025 flathack</p>")
                .arg(flatlas::tools::UpdateChecker::currentVersion()));
    });

    // --- Menu bar corner: Currently Editing + Launch FL + Give Feedback ---
    auto *cornerWidget = new QWidget(this);
    auto *cornerLayout = new QHBoxLayout(cornerWidget);
    cornerLayout->setContentsMargins(0, 0, 8, 0);
    cornerLayout->setSpacing(8);

    m_editingLabel = new QLabel(tr("Currently Editing: -"), this);
    m_editingLabel->setStyleSheet(
        QStringLiteral("QLabel { color: #8899aa; background: #0d1117; border: 1px solid #2a3444;"
                        " border-radius: 3px; padding: 4px 12px; }"));
    cornerLayout->addWidget(m_editingLabel);

    auto *launchBtn = new QPushButton(tr("Launch FL"), this);
    launchBtn->setStyleSheet(
        QStringLiteral("QPushButton { background: #28a745; color: white; border: none;"
                        " padding: 5px 16px; border-radius: 3px; font-weight: bold; }"
                        "QPushButton:hover { background: #2fc553; }"));
    cornerLayout->addWidget(launchBtn);
    connect(launchBtn, &QPushButton::clicked, this, [this]() {
        const QString exe = QFileDialog::getOpenFileName(
            this, tr("Select Freelancer.exe"), {}, tr("Executable (*.exe)"));
        if (exe.isEmpty()) return;
        if (flatlas::tools::SpStarter::launch(exe))
            statusBar()->showMessage(tr("Freelancer launched."), 3000);
        else
            statusBar()->showMessage(tr("Failed to launch Freelancer."), 5000);
    });

    auto *feedbackBtn = new QPushButton(tr("Discord"), this);
    feedbackBtn->setStyleSheet(
        QStringLiteral("QPushButton { background: #5865F2; color: white; border: none;"
                        " padding: 5px 16px; border-radius: 3px; font-weight: bold; }"
                        "QPushButton:hover { background: #6975F3; }"));
    cornerLayout->addWidget(feedbackBtn);
    connect(feedbackBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://discord.gg/fY9qweRWGn")));
    });

    menuBar()->setCornerWidget(cornerWidget);
}

void MainWindow::createPanels()
{
    auto *central = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // --- Tab bar row: [tabs ...] [FLAtlas Settings / 100% / Activity] ---
    auto *tabBarRow = new QWidget(this);
    tabBarRow->setObjectName(QStringLiteral("tabBarRow"));
    auto *tabBarLayout = new QHBoxLayout(tabBarRow);
    tabBarLayout->setContentsMargins(0, 0, 0, 0);
    tabBarLayout->setSpacing(0);

    m_centerTabs = new flatlas::ui::CenterTabWidget(this);
    m_centerTabs->tabBar()->setStyleSheet(
        QStringLiteral("QTabBar { background: transparent; }"
                        "QTabBar::tab { padding: 6px 18px; margin: 0; border: none;"
                        "  background: #1a2030; color: #8899aa; }"
                        "QTabBar::tab:selected { background: #232d3f;"
                        "  color: #e0a030; border-bottom: 2px solid #e67e22; }"
                        "QTabBar::tab:hover { background: #222a3a; color: #bbccdd; }"
                        "QTabBar::close-button { image: url(close.png); }"
                        "QTabBar::close-button:hover { background: #443333; }"));
    tabBarLayout->addWidget(m_centerTabs->tabBar(), 1);

    // Right panel: FLAtlas Settings + indicators
    auto *rightPanel = new QWidget(this);
    rightPanel->setFixedWidth(160);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(4, 2, 8, 2);
    rightLayout->setSpacing(2);

    auto *settingsBtn = new QPushButton(tr("FLAtlas Settings"), this);
    settingsBtn->setStyleSheet(
        QStringLiteral("QPushButton { background: #1e2a3a; color: #aabbcc; border: 1px solid #2a3a4a;"
                        " padding: 4px 8px; border-radius: 2px; }"
                        "QPushButton:hover { background: #253548; color: #ddeeff; }"));
    connect(settingsBtn, &QPushButton::clicked, this, [this]() {
        flatlas::ui::SettingsDialog dlg(this);
        dlg.exec();
    });
    rightLayout->addWidget(settingsBtn);

    auto *indicatorRow = new QHBoxLayout();
    indicatorRow->setSpacing(8);
    auto *percentLabel = new QLabel(QStringLiteral("100%"), this);
    percentLabel->setStyleSheet(QStringLiteral("color: #44aa88; font-weight: bold; font-size: 11px;"));
    percentLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    indicatorRow->addWidget(percentLabel);
    auto *activityBtn = new QPushButton(tr("Activity"), this);
    activityBtn->setStyleSheet(
        QStringLiteral("QPushButton { background: transparent; color: #7788aa; border: none;"
                        " padding: 2px 6px; font-size: 11px; }"
                        "QPushButton:hover { color: #aabbdd; }"));
    indicatorRow->addWidget(activityBtn);
    rightLayout->addLayout(indicatorRow);

    tabBarLayout->addWidget(rightPanel);
    mainLayout->addWidget(tabBarRow);

    // --- Orange progress bar ---
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(100);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(4);
    m_progressBar->setStyleSheet(
        QStringLiteral("QProgressBar { background: #151c28; border: none; }"
                        "QProgressBar::chunk { background: #e67e22; }"));
    mainLayout->addWidget(m_progressBar);

    // --- Content splitter: Browser | Pages | Properties ---
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);

    m_browserPanel = new flatlas::ui::BrowserPanel(this);
    m_propertiesPanel = new flatlas::ui::PropertiesPanel(this);

    m_mainSplitter->addWidget(m_browserPanel);
    m_mainSplitter->addWidget(m_centerTabs->contentWidget());
    m_mainSplitter->addWidget(m_propertiesPanel);

    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setStretchFactor(2, 0);
    m_mainSplitter->setSizes({220, 1100, 0});
    m_propertiesPanel->setVisible(false);

    mainLayout->addWidget(m_mainSplitter, 1);
    setCentralWidget(central);

    // Welcome page as first tab
    auto *welcomePage = new flatlas::ui::WelcomePage(this);
    m_centerTabs->addTab(welcomePage, tr("Welcome"));
    connect(welcomePage, &flatlas::ui::WelcomePage::openModManagerRequested,
            this, &MainWindow::openModManager);
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::showContextHelp()
{
    if (!m_helpBrowser)
        m_helpBrowser = new flatlas::tools::HelpBrowser(this);

    QString topicId = QStringLiteral("overview");
    if (m_centerTabs && m_centerTabs->currentWidget()) {
        const QString className = QString::fromUtf8(m_centerTabs->currentWidget()->metaObject()->className());
        // Strip namespace prefix if present
        const QString shortName = className.contains(QLatin1String("::"))
            ? className.mid(className.lastIndexOf(QLatin1String("::")) + 2)
            : className;
        topicId = flatlas::tools::HelpBrowser::topicForContext(shortName);
    }
    m_helpBrowser->showTopic(topicId);
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

void MainWindow::openIdsEditor()
{
    auto *editor = new flatlas::editors::IdsEditorPage(this);

    int idx = m_centerTabs->addTab(editor, tr("IDS Editor"));
    m_centerTabs->setCurrentIndex(idx);

    connect(editor, &flatlas::editors::IdsEditorPage::titleChanged,
            this, [this, editor](const QString &title) {
        int i = m_centerTabs->indexOf(editor);
        if (i >= 0)
            m_centerTabs->setTabText(i, title);
    });

    statusBar()->showMessage(tr("IDS Editor opened"), 3000);
}

void MainWindow::openModManager()
{
    auto *page = new flatlas::editors::ModManagerPage(this);

    int idx = m_centerTabs->addTab(page, tr("Mod Manager"));
    m_centerTabs->setCurrentIndex(idx);

    connect(page, &flatlas::editors::ModManagerPage::titleChanged,
            this, [this, page](const QString &title) {
        int i = m_centerTabs->indexOf(page);
        if (i >= 0)
            m_centerTabs->setTabText(i, title);
    });

    statusBar()->showMessage(tr("Mod Manager opened"), 3000);
}

void MainWindow::openNpcEditor()
{
    auto *editor = new flatlas::editors::NpcEditorPage(this);

    int idx = m_centerTabs->addTab(editor, tr("NPC Editor"));
    m_centerTabs->setCurrentIndex(idx);

    connect(editor, &flatlas::editors::NpcEditorPage::titleChanged,
            this, [this, editor](const QString &title) {
        int i = m_centerTabs->indexOf(editor);
        if (i >= 0)
            m_centerTabs->setTabText(i, title);
    });

    statusBar()->showMessage(tr("NPC Editor opened"), 3000);
}

void MainWindow::openInfocardEditor()
{
    auto *editor = new flatlas::editors::InfocardEditor(this);

    int idx = m_centerTabs->addTab(editor, tr("Infocard Editor"));
    m_centerTabs->setCurrentIndex(idx);

    connect(editor, &flatlas::editors::InfocardEditor::titleChanged,
            this, [this, editor](const QString &title) {
        int i = m_centerTabs->indexOf(editor);
        if (i >= 0)
            m_centerTabs->setTabText(i, title);
    });

    statusBar()->showMessage(tr("Infocard Editor opened"), 3000);
}

void MainWindow::openNewsRumorEditor()
{
    auto *editor = new flatlas::editors::NewsRumorEditor(this);

    int idx = m_centerTabs->addTab(editor, tr("News/Rumor Editor"));
    m_centerTabs->setCurrentIndex(idx);

    connect(editor, &flatlas::editors::NewsRumorEditor::titleChanged,
            this, [this, editor](const QString &title) {
        int i = m_centerTabs->indexOf(editor);
        if (i >= 0)
            m_centerTabs->setTabText(i, title);
    });

    statusBar()->showMessage(tr("News/Rumor Editor opened"), 3000);
}
