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
