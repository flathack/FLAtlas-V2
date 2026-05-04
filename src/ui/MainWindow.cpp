#include "MainWindow.h"
#include "WelcomePage.h"
#include "PropertiesPanel.h"
#include "CenterTabWidget.h"
#include "SettingsDialog.h"
#include "core/Config.h"
#include "core/EditingContext.h"
#include "core/Theme.h"
#include "core/Theme.h"
#include "core/I18n.h"
#include "core/UndoManager.h"
#include "editors/system/SystemEditorPage.h"
#include "editors/system/SystemDisplayFilterDialog.h"
#include "editors/ini/IniEditorPage.h"
#include "editors/universe/UniverseEditorPage.h"
#include "editors/base/BaseBuilder.h"
#include "editors/trade/TradeRoutePage.h"
#include "editors/ids/IdsEditorPage.h"
#include "editors/modmanager/ModManagerPage.h"
#include "editors/modsettings/ModSettingsPage.h"
#include "editors/npc/NpcEditorPage.h"
#include "editors/news/NewsRumorEditor.h"
#include "tools/UpdateChecker.h"
#include "tools/UpdateDownloader.h"
#include "tools/UpdateInstaller.h"
#include "tools/HelpBrowser.h"
#include "tools/KeyboardShortcutOverviewDialog.h"
#include "tools/PathFinderDialog.h"
#include "rendering/preview/ModelViewerPage.h"
#include "rendering/view3d/SceneView3D.h"
#include "domain/SystemDocument.h"
#include "domain/ZoneItem.h"
#include "domain/UniverseData.h"
#include "infrastructure/freelancer/UniverseScanner.h"
#include "core/PathUtils.h"

#include <QCloseEvent>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QSplitter>
#include <QStatusBar>
#include <QSettings>
#include <QApplication>
#include <QCheckBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QJsonObject>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabBar>
#include <QStackedWidget>
#include <QDesktopServices>
#include <QEventLoop>
#include <QProcess>
#include <QUrl>
#include <QSignalBlocker>
#include <QSlider>
#include <QSet>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QAbstractItemView>
#include <QBrush>
#include <QFont>
#include <QPalette>
#include <QTimer>

#include <exception>
#include <memory>

namespace {

QStringList defaultPinnedTools()
{
    return {QStringLiteral("modManager"), QStringLiteral("universe")};
}

QString toolKeyForWidget(QWidget *widget)
{
    if (!widget)
        return {};
    if (qobject_cast<flatlas::editors::ModManagerPage *>(widget))
        return QStringLiteral("modManager");
    if (qobject_cast<flatlas::editors::UniverseEditorPage *>(widget))
        return QStringLiteral("universe");
    if (qobject_cast<flatlas::editors::TradeRoutePage *>(widget))
        return QStringLiteral("tradeRoutes");
    if (qobject_cast<flatlas::editors::IdsEditorPage *>(widget))
        return QStringLiteral("idsEditor");
    if (qobject_cast<flatlas::editors::ModSettingsPage *>(widget))
        return QStringLiteral("modSettings");
    if (qobject_cast<flatlas::editors::NpcEditorPage *>(widget))
        return QStringLiteral("npcEditor");
    if (qobject_cast<flatlas::editors::NewsRumorEditor *>(widget))
        return QStringLiteral("newsRumorEditor");
    if (qobject_cast<flatlas::rendering::ModelViewerPage *>(widget))
        return QStringLiteral("modelViewer");
    return {};
}

QJsonObject installedExternalTools()
{
    return flatlas::core::Config::instance().getJsonObject(QStringLiteral("externalTools"));
}

bool isContextBoundTab(QWidget *widget)
{
    if (widget && widget->objectName().startsWith(QStringLiteral("system3d:")))
        return true;
    return qobject_cast<flatlas::editors::UniverseEditorPage *>(widget)
        || qobject_cast<flatlas::editors::SystemEditorPage *>(widget)
        || qobject_cast<flatlas::editors::IniEditorPage *>(widget)
        || qobject_cast<flatlas::rendering::SceneView3D *>(widget)
        || qobject_cast<flatlas::editors::TradeRoutePage *>(widget)
        || qobject_cast<flatlas::editors::IdsEditorPage *>(widget)
        || qobject_cast<flatlas::editors::NpcEditorPage *>(widget)
        || qobject_cast<flatlas::editors::NewsRumorEditor *>(widget);
}

bool objectVisibleForFilter(const flatlas::rendering::SystemDisplayFilterSettings &settings,
                            const flatlas::domain::SolarObject &obj)
{
    flatlas::rendering::SolarObjectDisplayContext context;
    context.nickname = obj.nickname();
    context.archetype = obj.archetype();
    context.type = obj.type();

    bool visible = settings.objectVisibleForType(obj.type());
    for (const auto &rule : settings.rules) {
        if (!flatlas::rendering::matchesDisplayFilterRule(rule, context))
            continue;
        if (rule.target == flatlas::rendering::DisplayFilterTarget::Label)
            continue;
        visible = (rule.action == flatlas::rendering::DisplayFilterAction::Show);
    }
    return visible;
}

bool zoneVisibleForFilter(const flatlas::rendering::SystemDisplayFilterSettings &settings,
                          const flatlas::domain::ZoneItem &zone)
{
    flatlas::rendering::SolarObjectDisplayContext context;
    context.nickname = zone.nickname();
    context.archetype = QStringList{
        zone.zoneType(),
        zone.usage(),
        zone.popType(),
        zone.pathLabel(),
        zone.comment(),
    }.join(QLatin1Char(' '));
    context.typeNameOverride = QStringLiteral("Zone");

    bool visible = true;
    for (const auto &rule : settings.rules) {
        if (!flatlas::rendering::matchesDisplayFilterRule(rule, context))
            continue;
        if (rule.target == flatlas::rendering::DisplayFilterTarget::Label)
            continue;
        visible = (rule.action == flatlas::rendering::DisplayFilterAction::Show);
    }
    return visible;
}

QWidget *createSystem3DPage(flatlas::domain::SystemDocument *document,
                            const QHash<QString, QString> &modelPaths,
                            const QHash<QString, float> &displayRadii,
                            const QHash<QString, QStringList> &textureSourcePaths,
                            const flatlas::rendering::SystemDisplayFilterSettings &initialFilterSettings,
                            const QString &tabKey,
                            flatlas::editors::SystemEditorPage *sourceEditor,
                            QWidget *parent)
{
    auto *page = new QWidget(parent);
    page->setObjectName(tabKey);
    auto *layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *splitter = new QSplitter(Qt::Horizontal, page);
    layout->addWidget(splitter, 1);

    auto *leftSidebar = new QWidget(splitter);
    leftSidebar->setMinimumWidth(220);
    auto *leftLayout = new QVBoxLayout(leftSidebar);
    leftLayout->setContentsMargins(8, 8, 8, 8);
    leftLayout->setSpacing(6);

    auto *topRow = new QHBoxLayout();
    auto *searchEdit = new QLineEdit(leftSidebar);
    searchEdit->setPlaceholderText(QObject::tr("Search objects / zones"));
    topRow->addWidget(searchEdit, 1);
    auto *filterButton = new QPushButton(QObject::tr("Display Filters"), leftSidebar);
    topRow->addWidget(filterButton);
    leftLayout->addLayout(topRow);

    auto *tree = new QTreeWidget(leftSidebar);
    tree->setHeaderLabels({QObject::tr("Nickname"), QObject::tr("Type")});
    tree->setAlternatingRowColors(true);
    tree->setRootIsDecorated(true);
    tree->setSelectionMode(QAbstractItemView::SingleSelection);
    leftLayout->addWidget(tree, 1);
    splitter->addWidget(leftSidebar);

    auto *view = new flatlas::rendering::SceneView3D(splitter);
    view->setArchetypeModelPaths(modelPaths);
    view->setArchetypeDisplayRadii(displayRadii);
    view->setArchetypeTextureSourcePaths(textureSourcePaths);
    view->setGameRoot(flatlas::core::EditingContext::instance().primaryGamePath());
    view->setDisplayFilterSettings(initialFilterSettings);
    view->loadDocument(document);
    splitter->addWidget(view);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({280, 1000});

    auto *objectsRoot = new QTreeWidgetItem(tree, {QObject::tr("Objects")});
    objectsRoot->setFlags(objectsRoot->flags() & ~Qt::ItemIsSelectable);
    objectsRoot->setExpanded(true);
    auto *zonesRoot = new QTreeWidgetItem(tree, {QObject::tr("Zones")});
    zonesRoot->setFlags(zonesRoot->flags() & ~Qt::ItemIsSelectable);
    zonesRoot->setExpanded(true);

    if (document) {
        for (const auto &obj : document->objects()) {
            if (!obj)
                continue;
            auto *item = new QTreeWidgetItem(objectsRoot, {obj->nickname(), obj->archetype()});
            item->setData(0, Qt::UserRole, obj->nickname());
            item->setData(0, Qt::UserRole + 1, QStringLiteral("object"));
        }
        for (const auto &zone : document->zones()) {
            if (!zone)
                continue;
            auto *item = new QTreeWidgetItem(zonesRoot, {zone->nickname(), zone->zoneType()});
            item->setData(0, Qt::UserRole, zone->nickname());
            item->setData(0, Qt::UserRole + 1, QStringLiteral("zone"));
        }
    }
    tree->resizeColumnToContents(0);

    auto *zoomLabel = new QLabel(QObject::tr("Zoom"), leftSidebar);
    leftLayout->insertWidget(1, zoomLabel);
    auto *zoomSlider = new QSlider(Qt::Horizontal, leftSidebar);
    zoomSlider->setRange(0, 100);
    zoomSlider->setValue(50);
    zoomSlider->setToolTip(QObject::tr("Zoom"));
    leftLayout->insertWidget(2, zoomSlider);

    auto *wireframesCheck = new QCheckBox(QObject::tr("Show Wireframes"), leftSidebar);
    wireframesCheck->setChecked(true);
    leftLayout->insertWidget(3, wireframesCheck);

    auto *centerButton = new QPushButton(QObject::tr("Center to Object"), leftSidebar);
    centerButton->setEnabled(false);
    leftLayout->insertWidget(4, centerButton);

    QObject::connect(zoomSlider, &QSlider::valueChanged, view, [view](int value) {
        view->setZoomLevel(value);
    });
    QObject::connect(wireframesCheck, &QCheckBox::toggled, view, [view](bool checked) {
        view->setZoneWireframesVisible(checked);
    });

    auto filterSettings = std::make_shared<flatlas::rendering::SystemDisplayFilterSettings>(initialFilterSettings);
    auto applyTreeFilter = [tree, searchEdit, document, filterSettings]() {
        const QString needle = searchEdit ? searchEdit->text().trimmed().toLower() : QString();
        const QPalette palette = tree->palette();
        const QColor normalText = palette.color(QPalette::Active, QPalette::Text);
        const QColor mutedText(120, 128, 140);
        for (int rootIndex = 0; rootIndex < tree->topLevelItemCount(); ++rootIndex) {
            QTreeWidgetItem *root = tree->topLevelItem(rootIndex);
            bool anySearchVisible = false;
            for (int childIndex = 0; root && childIndex < root->childCount(); ++childIndex) {
                QTreeWidgetItem *child = root->child(childIndex);
                const QString nickname = child->data(0, Qt::UserRole).toString();
                bool displayVisible = true;
                if (document) {
                    displayVisible = false;
                    for (const auto &obj : document->objects()) {
                        if (obj && obj->nickname() == nickname) {
                            displayVisible = objectVisibleForFilter(*filterSettings, *obj);
                            break;
                        }
                    }
                    for (const auto &zone : document->zones()) {
                        if (zone && zone->nickname() == nickname) {
                            displayVisible = zoneVisibleForFilter(*filterSettings, *zone);
                            break;
                        }
                    }
                }
                const bool searchVisible = needle.isEmpty()
                    || child->text(0).toLower().contains(needle)
                    || child->text(1).toLower().contains(needle);
                child->setHidden(!searchVisible);
                anySearchVisible = anySearchVisible || searchVisible;
                for (int column = 0; column < child->columnCount(); ++column) {
                    child->setForeground(column, displayVisible ? QBrush(normalText) : QBrush(mutedText));
                    QFont font = child->font(column);
                    font.setItalic(!displayVisible);
                    child->setFont(column, font);
                }
                const QString tooltip = displayVisible
                    ? QString()
                    : QObject::tr("Durch den Sichtbarkeitsfilter aktuell ausgeblendet");
                child->setToolTip(0, tooltip);
                child->setToolTip(1, tooltip);
            }
            if (root)
                root->setHidden(!anySearchVisible);
        }
    };

    QObject::connect(searchEdit, &QLineEdit::textChanged, tree, [applyTreeFilter](const QString &) {
        applyTreeFilter();
    });
    applyTreeFilter();

    QObject::connect(filterButton, &QPushButton::clicked, page, [page, view, tree, sourceEditor, filterSettings, applyTreeFilter]() {
        flatlas::editors::SystemDisplayFilterDialog dialog(*filterSettings, page);
        if (dialog.exec() != QDialog::Accepted)
            return;
        *filterSettings = dialog.settings();
        if (sourceEditor)
            sourceEditor->applyDisplayFilterSettingsFrom3DView(*filterSettings);
        view->setDisplayFilterSettings(*filterSettings);
        applyTreeFilter();
    });

    QObject::connect(tree, &QTreeWidget::itemSelectionChanged, view, [tree, view, centerButton]() {
        const auto selected = tree->selectedItems();
        if (selected.isEmpty()) {
            centerButton->setEnabled(false);
            return;
        }
        const QString nickname = selected.first()->data(0, Qt::UserRole).toString();
        const bool isObject = selected.first()->data(0, Qt::UserRole + 1).toString() == QStringLiteral("object");
        centerButton->setEnabled(isObject && !nickname.isEmpty());
        if (!nickname.isEmpty())
            view->selectObject(nickname);
    });

    QObject::connect(centerButton, &QPushButton::clicked, view, [view]() {
        view->centerOnSelectedObject();
    });

    QObject::connect(view, &flatlas::rendering::SceneView3D::objectSelected, tree, [tree, centerButton](const QString &nickname) {
        if (nickname.isEmpty()) {
            centerButton->setEnabled(false);
            return;
        }
        QSignalBlocker blocker(tree);
        const auto matches = tree->findItems(nickname, Qt::MatchExactly | Qt::MatchRecursive, 0);
        if (!matches.isEmpty()) {
            tree->setCurrentItem(matches.first());
            tree->scrollToItem(matches.first());
            centerButton->setEnabled(matches.first()->data(0, Qt::UserRole + 1).toString() == QStringLiteral("object"));
        }
    });

    applyTreeFilter();

    return page;
}

}

QString MainWindow::formatSystemTabTitle(const QString &editorTitle, const QString &ingameName)
{
    const QString trimmedTitle = editorTitle.trimmed();
    const bool dirty = trimmedTitle.endsWith(QLatin1Char('*'));
    const QString baseTitle = dirty ? trimmedTitle.left(trimmedTitle.size() - 1).trimmed() : trimmedTitle;
    const QString trimmedIngame = ingameName.trimmed();

    QString result = baseTitle;
    if (!trimmedIngame.isEmpty() && baseTitle.compare(trimmedIngame, Qt::CaseInsensitive) != 0)
        result = QStringLiteral("%1 - %2").arg(baseTitle, trimmedIngame);

    if (dirty)
        result.append(QLatin1Char('*'));
    return result;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("FL Atlas V2 v%1").arg(qApp->applicationVersion()));
    setMinimumSize(960, 620);
    resize(1600, 900);

    // Editing-Kontext vor dem UI-Aufbau laden, damit z.B. der Mod Manager
    // bereits beim ersten Rendern die gespeicherten Installationen sieht.
    auto &ctx = flatlas::core::EditingContext::instance();
    ctx.restore();

    createMenus();
    createPanels();
    createStatusBar();
    restoreWindowState();
    applyThemeStyling();
    connect(&flatlas::core::I18n::instance(), &flatlas::core::I18n::languageChanged,
            this, [this]() {
        menuBar()->clear();
        createMenus();
        const auto profile = flatlas::core::EditingContext::instance().editingProfile();
        if (profile.isValid()) {
            if (m_editingLabel)
                m_editingLabel->setText(tr("Currently Editing: %1").arg(profile.name));
            setWindowTitle(tr("FL Atlas V2 v%1 - %2").arg(qApp->applicationVersion(), profile.name));
        } else {
            if (m_editingLabel)
                m_editingLabel->setText(tr("Currently Editing: -"));
            setWindowTitle(tr("FL Atlas V2 v%1").arg(qApp->applicationVersion()));
        }
    });
    connect(&flatlas::core::Theme::instance(), &flatlas::core::Theme::themeChanged,
            this, [this](const QString &) { applyThemeStyling(); });

    // Editing-Kontext-Änderungen mit der UI verbinden
    connect(&ctx, &flatlas::core::EditingContext::contextChanged,
            this, [this](const QString &) { handleEditingContextChanged(); });
    QMetaObject::invokeMethod(this, &MainWindow::handleEditingContextChanged, Qt::QueuedConnection);

    // Auto-Update-Check bei Start
    if (flatlas::core::Config::instance().getBool(QStringLiteral("updateCheckEnabled"), true)) {
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
}

MainWindow::~MainWindow() = default;

void MainWindow::createMenus()
{
    // --- File ---
    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&Save"), QKeySequence::Save, this, [this]() { saveCurrentFile(); });
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Settings..."), this, [this]() { openSettingsDialog(); });
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

    // -- Editors --
    toolsMenu->addAction(tr("Open &File Editor..."), this, [this]() { openIniFile(); });
    toolsMenu->addAction(tr("&Trade Routes"), this, [this]() { openTradeRoutes(); });
    toolsMenu->addAction(tr("&IDS Editor"), this, [this]() { openIdsEditor(); });
    toolsMenu->addAction(tr("&Mod Manager"), this, [this]() { openModManager(); });
    toolsMenu->addAction(tr("Mod &Settings"), this, [this]() { openModSettings(); });
    toolsMenu->addAction(tr("&NPC Editor"), this, [this]() { openNpcEditor(); });
    toolsMenu->addAction(tr("&News Editor"), this, [this]() { openNewsRumorEditor(); });
    toolsMenu->addSeparator();

    // -- Tools --
    toolsMenu->addAction(tr("&3D Model Viewer"), this, [this]() { openModelViewer(); });
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
    toolsMenu->addAction(tr("&Launch Freelancer..."), this, &MainWindow::launchFreelancerFromContext);

    auto *externalToolsMenu = menuBar()->addMenu(tr("Externe Tools"));
    const QJsonObject externalTools = installedExternalTools();
    bool hasExternalTools = false;
    for (auto it = externalTools.constBegin(); it != externalTools.constEnd(); ++it) {
        const QJsonObject tool = it.value().toObject();
        const QString name = tool.value(QStringLiteral("name")).toString(it.key());
        const QString exePath = tool.value(QStringLiteral("exePath")).toString();
        if (exePath.isEmpty())
            continue;
        hasExternalTools = true;
        externalToolsMenu->addAction(name, this, [this, exePath, name]() {
            if (!QFileInfo::exists(exePath)) {
                QMessageBox::warning(this, tr("Externe Tools"), tr("%1 wurde nicht gefunden:\n%2").arg(name, exePath));
                return;
            }
            QProcess::startDetached(exePath, {}, QFileInfo(exePath).absolutePath());
        });
    }
    if (!hasExternalTools) {
        auto *empty = externalToolsMenu->addAction(tr("Keine Tools installiert"));
        empty->setEnabled(false);
    }

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
        langMenu->addAction(lang, this, [this, lang]() {
            flatlas::core::I18n::instance().setLanguage(lang);
            flatlas::core::Config::instance().setString("language", lang);
            flatlas::core::Config::instance().save();
            statusBar()->showMessage(tr("Language set to '%1'. Open tabs may need to be reopened.").arg(lang), 5000);
        });
    }

    // --- Help ---
    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&Help Contents"), QKeySequence::HelpContents, this, [this]() {
        showContextHelp();
    });
    helpMenu->addAction(tr("Keyboard &Shortcuts"), this, [this]() {
        showShortcutOverview();
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
    helpMenu->addAction(tr("&Über FL Atlas..."), this, [this]() {
        QMessageBox::about(this, tr("Über FL Atlas"),
            tr("<h2>FL Atlas V2</h2>"
               "<p><b>Version:</b> v%1</p>"
               "<p><b>Autor:</b> Steven</p>"
               "<p><b>Lizenz:</b> MIT License</p>"
               "<hr>"
               "<p>Ein visueller Editor für Freelancer-Systemdateien (INI). FL Atlas ist zusätzlich kompatibel mit FLMM-Mods, damit diese auch hier genutzt werden können.</p>"
               "<p>Zeigt Systeme als interaktive 2-D/3-D-Karte an. Objekte, Zonen, Bases, Docking Rings, Tradelanes und Verbindungen können erstellt, bearbeitet und verschoben werden.</p>"
               "<p>Vielen Dank an IGx89 für Freelancer Mod Manager (FLMM) und seine Arbeit für die Modding-Community.</p>"
               "<hr>"
               "<p><b>Technologie:</b> C++ · Qt 6 · Qt3D</p>"
               "<p><b>Spiel:</b> Freelancer (2003, Digital Anvil / Microsoft)</p>"
               "<p>&copy; 2024–2025 flathack</p>")
                .arg(qApp->applicationVersion()));
    });

    // --- Menu bar corner: Currently Editing + Launch FL + Give Feedback ---
    auto *cornerWidget = new QWidget(this);
    auto *cornerLayout = new QHBoxLayout(cornerWidget);
    cornerLayout->setContentsMargins(0, 0, 8, 0);
    cornerLayout->setSpacing(8);

    m_editingLabel = new QLabel(tr("Currently Editing: -"), this);
    cornerLayout->addWidget(m_editingLabel);

    auto *launchBtn = new QPushButton(tr("Launch FL"), this);
    launchBtn->setStyleSheet(
        QStringLiteral("QPushButton { background: #28a745; color: white; border: none;"
                        " padding: 5px 16px; border-radius: 3px; font-weight: bold; }"
                        "QPushButton:hover { background: #2fc553; }"));
    cornerLayout->addWidget(launchBtn);
    connect(launchBtn, &QPushButton::clicked, this, &MainWindow::launchFreelancerFromContext);

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
    tabBarLayout->addWidget(m_centerTabs->tabBar(), 1);
    connect(m_centerTabs, &flatlas::ui::CenterTabWidget::closeRequested,
            this, [this](int index) { closeTabWithPrompt(index); });
    connect(m_centerTabs, &flatlas::ui::CenterTabWidget::currentChanged,
            this, [this](int) { saveOpenToolTabs(); });
    connect(m_centerTabs, &flatlas::ui::CenterTabWidget::currentChanged,
            this, [this](int) {
        if (!m_centerTabs)
            return;
        auto sync3DViews = [this]() {
            QWidget *current = m_centerTabs ? m_centerTabs->currentWidget() : nullptr;
            for (int i = 0; m_centerTabs && i < m_centerTabs->count(); ++i) {
                QWidget *page = m_centerTabs->widget(i);
                if (!page)
                    continue;
                const bool active = (page == current);
                const auto views = page->findChildren<flatlas::rendering::SceneView3D *>();
                for (auto *view : views)
                    view->setViewportActive(active);
            }
        };
        sync3DViews();
        QTimer::singleShot(0, this, sync3DViews);
    });

    // Right panel: FLAtlas Settings + indicators
    auto *rightPanel = new QWidget(this);
    rightPanel->setFixedWidth(160);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(4, 2, 8, 2);
    rightLayout->setSpacing(2);

    m_settingsButton = new QPushButton(tr("FLAtlas Settings"), this);
    connect(m_settingsButton, &QPushButton::clicked, this, [this]() { openSettingsDialog(); });
    rightLayout->addWidget(m_settingsButton);

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

    // --- Content splitter: Pages | Properties ---
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);

    m_propertiesPanel = new flatlas::ui::PropertiesPanel(this);

    m_mainSplitter->addWidget(m_centerTabs->contentWidget());
    m_mainSplitter->addWidget(m_propertiesPanel);

    m_mainSplitter->setStretchFactor(0, 1);
    m_mainSplitter->setStretchFactor(1, 0);
    m_mainSplitter->setSizes({1100, 0});
    m_propertiesPanel->setVisible(false);

    mainLayout->addWidget(m_mainSplitter, 1);
    setCentralWidget(central);

    // Mod Manager as pinned tab (always visible, not closable)
    auto *modManagerPage = new flatlas::editors::ModManagerPage(this);
    m_centerTabs->addPinnedTab(modManagerPage, tr("Mod Manager"));
    connect(modManagerPage, &flatlas::editors::ModManagerPage::titleChanged,
            this, [this, modManagerPage](const QString &title) {
        int i = m_centerTabs->indexOf(modManagerPage);
        if (i >= 0)
            m_centerTabs->setTabText(i, title);
    });
    // Welcome page or skip directly to Mod Manager
    if (flatlas::core::Config::instance().getBool(QStringLiteral("skipWelcome"), false)) {
        m_centerTabs->setCurrentIndex(0); // Mod Manager is index 0
    } else if (flatlas::core::Config::instance().getBool(QStringLiteral("restoreOpenTabs"), false)) {
        m_centerTabs->setCurrentIndex(0);
    } else {
        auto *welcomePage = new flatlas::ui::WelcomePage(this);
        int welcomeIdx = m_centerTabs->addTab(welcomePage, tr("Welcome"));
        m_centerTabs->setCurrentIndex(welcomeIdx);
        connect(welcomePage, &flatlas::ui::WelcomePage::openModManagerRequested,
                this, [this]() { m_centerTabs->setCurrentIndex(0); });
    }
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

void MainWindow::showShortcutOverview()
{
    if (!m_shortcutOverviewDialog) {
        m_shortcutOverviewDialog = new flatlas::tools::KeyboardShortcutOverviewDialog(this);
        connect(m_shortcutOverviewDialog, &QObject::destroyed, this, [this]() {
            m_shortcutOverviewDialog = nullptr;
        });
    }

    m_shortcutOverviewDialog->show();
    m_shortcutOverviewDialog->raise();
    m_shortcutOverviewDialog->activateWindow();
}

void MainWindow::applyThemeStyling()
{
    if (!m_centerTabs)
        return;

    const QPalette pal = palette();
    const QColor base = pal.color(QPalette::Base);
    const QColor alt = pal.color(QPalette::AlternateBase);
    const QColor tabBg = pal.color(QPalette::Button);
    const QColor tabHover = pal.color(QPalette::AlternateBase);
    const QColor tabText = pal.color(QPalette::ButtonText);
    const QColor selectedBg(230, 126, 34);
    const QColor accent(230, 126, 34);
    const QColor border = pal.color(QPalette::Mid);
    const QColor disabledBg = pal.color(QPalette::Midlight);
    const QColor dimText = pal.color(QPalette::PlaceholderText);
    const QColor selectedText = selectedBg.lightness() >= 170 ? QColor(36, 28, 8) : QColor(255, 255, 255);
    const QColor dangerHover = pal.color(QPalette::Base).lightness() >= 170
        ? QColor(232, 210, 210)
        : QColor(68, 51, 51);

    m_centerTabs->tabBar()->setStyleSheet(
        QStringLiteral("QTabBar { background: transparent; }"
                       "QTabBar::tab {"
                       " padding: 8px 16px;"
                       " margin-right: 2px;"
                       " min-width: 130px;"
                       " border: 1px solid %3;"
                       " border-bottom: 2px solid transparent;"
                       " background: %1; color: %2; }"
                                             "QTabBar::tab:selected { background: %4; color: %5; border-bottom: 2px solid %6; }"
                                             "QTabBar::tab:!selected:hover { background: %7; color: %2; }"
                                             "QTabBar::tab:disabled { background: %8; color: %9; border: 1px solid %3; }"
                       "QTabBar::close-button { subcontrol-origin: padding; subcontrol-position: right; }"
                                             "QTabBar::close-button:hover { background: %10; border-radius: 3px; }")
                        .arg(tabBg.name())
                        .arg(tabText.name())
                        .arg(border.name())
                        .arg(selectedBg.name())
                        .arg(selectedText.name())
                        .arg(accent.name())
                        .arg(tabHover.name())
                        .arg(disabledBg.name())
                        .arg(dimText.name())
                        .arg(dangerHover.name()));

    if (m_editingLabel) {
        m_editingLabel->setStyleSheet(
            QStringLiteral("QLabel { color:%1; background:%2; border:1px solid %3;"
                           " border-radius:3px; padding:4px 12px; }")
                .arg(dimText.name(), base.name(), border.name()));
    }

    if (m_settingsButton) {
        m_settingsButton->setStyleSheet(
            QStringLiteral("QPushButton { background:%1; color:%2; border:1px solid %3;"
                           " padding:4px 8px; border-radius:2px; }"
                           "QPushButton:hover { background:%4; color:%2; }")
                .arg(tabBg.name(), tabText.name(), border.name(), alt.name()));
    }
}

void MainWindow::launchFreelancerFromContext()
{
    auto &ctx = flatlas::core::EditingContext::instance();
    QString gamePath = ctx.primaryGamePath();
    if (gamePath.isEmpty()) {
        QMessageBox::information(
            this,
            tr("Launch Freelancer"),
            tr("No editing context set. Select an installation first."));
        return;
    }

    QDir dir(gamePath);
    QString exe;
    if (dir.exists(QStringLiteral("EXE/Freelancer.exe")))
        exe = dir.filePath(QStringLiteral("EXE/Freelancer.exe"));
    else if (dir.exists(QStringLiteral("Freelancer.exe")))
        exe = dir.filePath(QStringLiteral("Freelancer.exe"));

    if (exe.isEmpty()) {
        exe = QFileDialog::getOpenFileName(
            this, tr("Select Freelancer.exe"), gamePath, tr("Executable (*.exe)"));
        if (exe.isEmpty())
            return;
    }

    if (QProcess::startDetached(exe, {}, QFileInfo(exe).absolutePath()))
        statusBar()->showMessage(tr("Freelancer launched."), 3000);
    else
        statusBar()->showMessage(tr("Failed to launch Freelancer."), 5000);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    for (int i = m_centerTabs ? (m_centerTabs->count() - 1) : -1; i >= 0; --i) {
        if (!confirmCloseDirtyWidget(m_centerTabs->widget(i), m_centerTabs->tabBar()->tabText(i))) {
            event->ignore();
            return;
        }
    }
    saveOpenToolTabs();
    saveWindowState();
    event->accept();
}

bool MainWindow::closeTabWithPrompt(int index, bool force)
{
    if (!m_centerTabs || index < 0 || index >= m_centerTabs->count())
        return false;

    QWidget *widget = m_centerTabs->widget(index);
    if (!confirmCloseDirtyWidget(widget, m_centerTabs->tabBar()->tabText(index)))
        return false;

    m_centerTabs->removeTab(index, force);
    saveOpenToolTabs();
    return true;
}

bool MainWindow::confirmCloseDirtyWidget(QWidget *widget, const QString &titleForUser)
{
    if (!widget || !isWidgetDirty(widget))
        return true;

    const QString cleanTitle = titleForUser.trimmed().endsWith(QLatin1Char('*'))
        ? titleForUser.trimmed().left(titleForUser.trimmed().size() - 1).trimmed()
        : titleForUser.trimmed();

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Ungespeicherte Änderungen"));
    box.setText(tr("Im Tab \"%1\" gibt es ungespeicherte Änderungen.").arg(cleanTitle.isEmpty() ? tr("Unbenannt") : cleanTitle));
    box.setInformativeText(tr("Möchtest du die Änderungen speichern, bevor geschlossen wird?"));
    auto *saveButton = box.addButton(tr("Speichern"), QMessageBox::AcceptRole);
    auto *discardButton = box.addButton(tr("Verwerfen"), QMessageBox::DestructiveRole);
    auto *cancelButton = box.addButton(tr("Abbrechen"), QMessageBox::RejectRole);
    box.setDefaultButton(qobject_cast<QPushButton *>(saveButton));
    box.exec();

    if (box.clickedButton() == cancelButton)
        return false;
    if (box.clickedButton() == discardButton)
        return true;
    if (box.clickedButton() == saveButton)
        return saveWidgetWithPrompt(widget) && !isWidgetDirty(widget);
    return false;
}

bool MainWindow::saveWidgetWithPrompt(QWidget *widget)
{
    if (!widget)
        return true;

    if (auto *editor = qobject_cast<flatlas::editors::SystemEditorPage *>(widget)) {
        QString targetPath = editor->filePath();
        if (targetPath.isEmpty()) {
            targetPath = QFileDialog::getSaveFileName(
                this, tr("Save System INI"), QString(),
                tr("INI Files (*.ini);;All Files (*)"));
            if (targetPath.isEmpty())
                return false;
            return editor->saveAs(targetPath);
        }
        return editor->save();
    }

    if (auto *editor = qobject_cast<flatlas::editors::IniEditorPage *>(widget)) {
        return editor->saveAllDirtyTabs(this);
    }

    if (auto *editor = qobject_cast<flatlas::editors::UniverseEditorPage *>(widget)) {
        const QString targetPath = editor->filePath();
        if (!targetPath.isEmpty())
            return editor->save();
        return false;
    }

    if (auto *editor = qobject_cast<flatlas::editors::NewsRumorEditor *>(widget)) {
        return editor->save();
    }

    return true;
}

bool MainWindow::isWidgetDirty(QWidget *widget) const
{
    if (!widget)
        return false;

    if (auto *editor = qobject_cast<flatlas::editors::SystemEditorPage *>(widget))
        return editor->isDirty();
    if (auto *editor = qobject_cast<flatlas::editors::UniverseEditorPage *>(widget))
        return editor->isDirty();
    if (auto *editor = qobject_cast<flatlas::editors::IniEditorPage *>(widget))
        return editor->isDirty();
    if (auto *editor = qobject_cast<flatlas::editors::NewsRumorEditor *>(widget))
        return editor->isModified();

    return false;
}

QString MainWindow::tabTitleForWidget(QWidget *widget) const
{
    if (!m_centerTabs || !widget)
        return {};
    const int index = m_centerTabs->indexOf(widget);
    if (index < 0)
        return {};
    return m_centerTabs->tabBar()->tabText(index);
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

void MainWindow::openSettingsDialog()
{
    flatlas::ui::SettingsDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted && !dlg.resetRequested())
        return;
    menuBar()->clear();
    createMenus();
    if (dlg.requiresPinnedToolRefresh() || dlg.resetRequested())
        applyPinnedToolSettings();
    saveOpenToolTabs();
}

void MainWindow::applyPinnedToolSettings()
{
    if (!m_centerTabs)
        return;

    QStringList pinned = flatlas::core::Config::instance().getStringList(QStringLiteral("pinnedTools"), defaultPinnedTools());
    if (!pinned.contains(QStringLiteral("modManager")))
        pinned.prepend(QStringLiteral("modManager"));

    QSet<QString> pinnedSet;
    for (const QString &key : std::as_const(pinned))
        pinnedSet.insert(key);
    for (int i = m_centerTabs->count() - 1; i >= 0; --i) {
        QWidget *widget = m_centerTabs->widget(i);
        const QString key = toolKeyForWidget(widget);
        if (key.isEmpty() || key == QStringLiteral("modManager"))
            continue;
        if (!pinnedSet.contains(key))
            m_centerTabs->removeTab(i, true);
    }

    const bool previousSuppress = m_suppressTabStateSave;
    m_suppressTabStateSave = true;
    const int current = m_centerTabs->currentIndex();
    for (const QString &key : pinned) {
        if (key == QStringLiteral("modManager"))
            continue;
        openToolByKey(key, true);
    }
    if (current >= 0 && current < m_centerTabs->count())
        m_centerTabs->setCurrentIndex(current);
    m_suppressTabStateSave = previousSuppress;
}

void MainWindow::restoreOpenToolTabs()
{
    const bool previousSuppress = m_suppressTabStateSave;
    m_suppressTabStateSave = true;
    const QStringList openTools = flatlas::core::Config::instance().getStringList(QStringLiteral("openToolTabs"));
    for (const QString &entry : openTools) {
        if (entry.startsWith(QStringLiteral("system:"), Qt::CaseInsensitive)) {
            const QString filePath = entry.mid(QStringLiteral("system:").size()).trimmed();
            if (filePath.isEmpty() || !QFileInfo::exists(filePath))
                continue;
            bool alreadyOpen = false;
            for (int i = 0; i < m_centerTabs->count(); ++i) {
                auto *existing = qobject_cast<flatlas::editors::SystemEditorPage *>(m_centerTabs->widget(i));
                if (existing && existing->filePath().compare(filePath, Qt::CaseInsensitive) == 0) {
                    alreadyOpen = true;
                    break;
                }
            }
            if (alreadyOpen)
                continue;
            auto *editor = new flatlas::editors::SystemEditorPage(this);
            if (!editor->loadFile(filePath)) {
                delete editor;
                continue;
            }
            const QString ingameName;
            int idx = m_centerTabs->addTab(editor, formatSystemTabTitle(editor->document()->name(), ingameName));
            connect(editor, &flatlas::editors::SystemEditorPage::titleChanged,
                    this, [this, editor, ingameName](const QString &title) {
                int i = m_centerTabs->indexOf(editor);
                if (i >= 0)
                    m_centerTabs->setTabText(i, formatSystemTabTitle(title, ingameName));
            });
            connect(editor, &flatlas::editors::SystemEditorPage::open3DSystemViewRequested,
                    this, [this, editor]() { open3DSystemEditorFor(editor); });
            m_centerTabs->setCurrentIndex(idx);
            continue;
        }

        const QString key = entry.startsWith(QStringLiteral("tool:"), Qt::CaseInsensitive)
            ? entry.mid(QStringLiteral("tool:").size()).trimmed()
            : entry.trimmed();
        openToolByKey(key, false);
    }
    m_suppressTabStateSave = previousSuppress;
    saveOpenToolTabs();
}

void MainWindow::saveOpenToolTabs()
{
    if (m_suppressTabStateSave)
        return;
    auto &config = flatlas::core::Config::instance();
    if (!config.getBool(QStringLiteral("restoreOpenTabs"), false))
        return;
    QStringList openTools;
    const QStringList pinned = config.getStringList(QStringLiteral("pinnedTools"), defaultPinnedTools());
    for (int i = 0; m_centerTabs && i < m_centerTabs->count(); ++i) {
        QWidget *widget = m_centerTabs->widget(i);
        if (auto *systemEditor = qobject_cast<flatlas::editors::SystemEditorPage *>(widget)) {
            const QString filePath = systemEditor->filePath().trimmed();
            if (!filePath.isEmpty())
                openTools.append(QStringLiteral("system:%1").arg(QDir::cleanPath(filePath)));
            continue;
        }
        const QString key = toolKeyForWidget(widget);
        if (!key.isEmpty() && key != QStringLiteral("modManager") && !pinned.contains(key))
            openTools.append(QStringLiteral("tool:%1").arg(key));
    }
    openTools.removeDuplicates();
    config.setStringList(QStringLiteral("openToolTabs"), openTools);
    config.save();
}

bool MainWindow::openToolByKey(const QString &key, bool pinned)
{
    if (!m_centerTabs)
        return false;
    if (key == QStringLiteral("universe")) {
        if (!flatlas::core::EditingContext::instance().hasContext())
            return false;
        openUniverseFromContext();
        return true;
    }
    for (int i = 0; i < m_centerTabs->count(); ++i) {
        if (toolKeyForWidget(m_centerTabs->widget(i)) == key) {
            if (key == QStringLiteral("idsEditor")) {
                auto *ids = qobject_cast<flatlas::editors::IdsEditorPage *>(m_centerTabs->widget(i));
                auto &ctx = flatlas::core::EditingContext::instance();
                if (ids && ctx.hasContext()) {
                    const QString exeDir = flatlas::core::PathUtils::ciResolvePath(
                        ctx.primaryGamePath(), QStringLiteral("EXE"));
                    if (!exeDir.isEmpty())
                        ids->loadFreelancerDir(exeDir);
                }
            }
            return true;
        }
    }

    auto addToolTab = [this, pinned](QWidget *widget, const QString &title) {
        return pinned ? m_centerTabs->addPinnedTab(widget, title) : m_centerTabs->addTab(widget, title);
    };

    if (key == QStringLiteral("tradeRoutes")) {
        auto *page = new flatlas::editors::TradeRoutePage(this);
        if (auto *universeEditor = qobject_cast<flatlas::editors::UniverseEditorPage *>(m_centerTabs->currentWidget()))
            page->setUniverseData(universeEditor->data());
        const int idx = addToolTab(page, tr("Trade Routes"));
        connect(page, &flatlas::editors::TradeRoutePage::titleChanged,
                this, [this, page](const QString &title) {
            int i = m_centerTabs->indexOf(page);
            if (i >= 0)
                m_centerTabs->setTabText(i, title);
        });
        if (!pinned)
            m_centerTabs->setCurrentIndex(idx);
        return true;
    }
    if (key == QStringLiteral("idsEditor")) {
        auto *editor = new flatlas::editors::IdsEditorPage(this);
        auto &ctx = flatlas::core::EditingContext::instance();
        if (ctx.hasContext()) {
            const QString exeDir = flatlas::core::PathUtils::ciResolvePath(
                ctx.primaryGamePath(), QStringLiteral("EXE"));
            if (!exeDir.isEmpty())
                editor->loadFreelancerDir(exeDir);
        }
        const int idx = addToolTab(editor, tr("IDS Editor"));
        connect(editor, &flatlas::editors::IdsEditorPage::titleChanged,
                this, [this, editor](const QString &title) {
            int i = m_centerTabs->indexOf(editor);
            if (i >= 0)
                m_centerTabs->setTabText(i, title);
        });
        connect(editor, &flatlas::editors::IdsEditorPage::openIniRequested,
                this, [this](const QString &filePath, const QString &searchText) {
            openIniFile(filePath, searchText, 0);
        });
        if (!pinned)
            m_centerTabs->setCurrentIndex(idx);
        return true;
    }
    if (key == QStringLiteral("modSettings")) {
        auto *editor = new flatlas::editors::ModSettingsPage(this);
        const int idx = addToolTab(editor, tr("Mod Settings"));
        connect(editor, &flatlas::editors::ModSettingsPage::titleChanged,
                this, [this, editor](const QString &title) {
            int i = m_centerTabs->indexOf(editor);
            if (i >= 0)
                m_centerTabs->setTabText(i, title);
        });
        if (!pinned)
            m_centerTabs->setCurrentIndex(idx);
        return true;
    }
    if (key == QStringLiteral("npcEditor")) {
        auto *editor = new flatlas::editors::NpcEditorPage(this);
        const int idx = addToolTab(editor, tr("NPC Editor"));
        connect(editor, &flatlas::editors::NpcEditorPage::titleChanged,
                this, [this, editor](const QString &title) {
            int i = m_centerTabs->indexOf(editor);
            if (i >= 0)
                m_centerTabs->setTabText(i, title);
        });
        if (!pinned)
            m_centerTabs->setCurrentIndex(idx);
        return true;
    }
    if (key == QStringLiteral("newsRumorEditor")) {
        auto *editor = new flatlas::editors::NewsRumorEditor(this);
        const int idx = addToolTab(editor, tr("News Editor"));
        connect(editor, &flatlas::editors::NewsRumorEditor::titleChanged,
                this, [this, editor](const QString &title) {
            int i = m_centerTabs->indexOf(editor);
            if (i >= 0)
                m_centerTabs->setTabText(i, title);
        });
        if (!pinned)
            m_centerTabs->setCurrentIndex(idx);
        return true;
    }
    if (key == QStringLiteral("modelViewer")) {
        auto *page = new flatlas::rendering::ModelViewerPage(this);
        const int idx = addToolTab(page, tr("3D Model Viewer"));
        if (!pinned)
            m_centerTabs->setCurrentIndex(idx);
        return true;
    }
    return false;
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
            QMessageBox::warning(this,
                                 tr("Error"),
                                 editor->lastSaveError().trimmed().isEmpty()
                                     ? tr("Could not save file.")
                                     : editor->lastSaveError());
    } else {
        if (editor->save())
            statusBar()->showMessage(tr("Saved"), 3000);
        else
            QMessageBox::warning(this,
                                 tr("Error"),
                                 editor->lastSaveError().trimmed().isEmpty()
                                     ? tr("Could not save file.")
                                     : editor->lastSaveError());
    }
}

flatlas::editors::SystemEditorPage *MainWindow::currentSystemEditor() const
{
    return qobject_cast<flatlas::editors::SystemEditorPage *>(
        m_centerTabs->currentWidget());
}

void MainWindow::openIniFile()
{
    const QString preferredRoot = flatlas::core::EditingContext::instance().primaryGamePath();

    for (int i = 0; i < m_centerTabs->count(); ++i) {
        auto *editor = qobject_cast<flatlas::editors::IniEditorPage *>(m_centerTabs->widget(i));
        if (!editor)
            continue;
        editor->openWorkspace(preferredRoot);
        m_centerTabs->setCurrentIndex(i);
        statusBar()->showMessage(tr("File Editor workspace opened"), 3000);
        return;
    }

    auto *editor = new flatlas::editors::IniEditorPage(this);
    editor->openWorkspace(preferredRoot);

    int idx = m_centerTabs->addTab(editor, tr("File Editor"));
    m_centerTabs->setCurrentIndex(idx);

    connect(editor, &flatlas::editors::IniEditorPage::titleChanged,
            this, [this, editor](const QString &title) {
        int i = m_centerTabs->indexOf(editor);
        if (i >= 0)
            m_centerTabs->setTabText(i, title);
    });
    connect(editor, &flatlas::editors::IniEditorPage::openFileRequested,
            this, [this](const QString &requestedPath, const QString &requestedSearchText, int requestedLineNumber) {
        openIniFile(requestedPath, requestedSearchText, requestedLineNumber);
    });

    statusBar()->showMessage(tr("File Editor workspace opened"), 3000);
}

void MainWindow::openIniFile(const QString &filePath, const QString &searchText, int lineNumber)
{
    if (filePath.isEmpty())
        return;

    flatlas::editors::IniEditorPage *editor = nullptr;
    for (int i = 0; i < m_centerTabs->count(); ++i) {
        editor = qobject_cast<flatlas::editors::IniEditorPage *>(m_centerTabs->widget(i));
        if (editor) {
            m_centerTabs->setCurrentIndex(i);
            break;
        }
    }

    if (!editor) {
        editor = new flatlas::editors::IniEditorPage(this);
        const QString preferredRoot = flatlas::core::EditingContext::instance().primaryGamePath();
        editor->openWorkspace(preferredRoot);

        int idx = m_centerTabs->addTab(editor, tr("File Editor"));
        m_centerTabs->setCurrentIndex(idx);

        connect(editor, &flatlas::editors::IniEditorPage::titleChanged,
                this, [this, editor](const QString &title) {
            int i = m_centerTabs->indexOf(editor);
            if (i >= 0)
                m_centerTabs->setTabText(i, title);
        });
        connect(editor, &flatlas::editors::IniEditorPage::openFileRequested,
                this, [this](const QString &requestedPath, const QString &requestedSearchText, int requestedLineNumber) {
            openIniFile(requestedPath, requestedSearchText, requestedLineNumber);
        });
    }

    if (!editor->openFile(filePath)) {
        QMessageBox::warning(this, tr("Error"),
                             tr("Could not open file:\n%1").arg(filePath));
        return;
    }

    if (!searchText.trimmed().isEmpty())
        editor->focusSearch(searchText);
    if (lineNumber > 0)
        editor->goToLine(lineNumber);

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

    // Try file editor
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

}

void MainWindow::openUniverseFromContext()
{
    auto &ctx = flatlas::core::EditingContext::instance();
    if (!ctx.hasContext())
        return;

    QString dataDir = flatlas::core::PathUtils::ciResolvePath(ctx.primaryGamePath(), QStringLiteral("DATA"));
    if (dataDir.isEmpty())
        dataDir = QDir(ctx.primaryGamePath()).absoluteFilePath(QStringLiteral("DATA"));
    QString universeIni = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("UNIVERSE/universe.ini"));
    if (universeIni.isEmpty()) {
        const QString direct = QDir(dataDir).absoluteFilePath(QStringLiteral("UNIVERSE/universe.ini"));
        if (QFileInfo::exists(direct))
            universeIni = direct;
    }
    if (universeIni.isEmpty()) {
        statusBar()->showMessage(tr("Universe.ini not found in editing context"), 5000);
        return;
    }

    // Reuse an already-open Universe tab if present.
    for (int i = 0; i < m_centerTabs->count(); ++i) {
        if (auto *editor = qobject_cast<flatlas::editors::UniverseEditorPage *>(m_centerTabs->widget(i))) {
            if (!editor->loadFile(universeIni)) {
                statusBar()->showMessage(tr("Could not reload Universe from editing context"), 5000);
                return;
            }
            const int systemCount = editor->data() ? editor->data()->systemCount() : 0;
            m_centerTabs->setTabText(i, QStringLiteral("Universe (%1)").arg(systemCount));
            m_centerTabs->setCurrentIndex(i);
            statusBar()->showMessage(tr("Universe reloaded from editing context"), 3000);
            return;
        }
    }

    auto *editor = new flatlas::editors::UniverseEditorPage(this);
    if (!editor->loadFile(universeIni)) {
        statusBar()->showMessage(tr("Could not load Universe from editing context"), 5000);
        delete editor;
        return;
    }

    const int systemCount = editor->data() ? editor->data()->systemCount() : 0;
    int idx = m_centerTabs->addPinnedTab(editor,
        QStringLiteral("Universe (%1)").arg(systemCount));
    m_centerTabs->setCurrentIndex(idx);

    connect(editor, &flatlas::editors::UniverseEditorPage::titleChanged,
            this, [this, editor](const QString &title) {
        int i = m_centerTabs->indexOf(editor);
        if (i >= 0)
            m_centerTabs->setTabText(i, title);
    });

    connect(editor, &flatlas::editors::UniverseEditorPage::openSystemRequested,
            this, &MainWindow::openSystemFromUniverse);

    statusBar()->showMessage(tr("Universe loaded from editing context"), 3000);
}

void MainWindow::handleEditingContextChanged()
{
    auto &ctx = flatlas::core::EditingContext::instance();
    const auto profile = ctx.editingProfile();

    if (profile.isValid()) {
        m_editingLabel->setText(tr("Currently Editing: %1").arg(profile.name));
        setWindowTitle(tr("FL Atlas V2 v%1 - %2").arg(qApp->applicationVersion(), profile.name));
    } else {
        m_editingLabel->setText(tr("Currently Editing: -"));
        setWindowTitle(tr("FL Atlas V2 v%1").arg(qApp->applicationVersion()));
    }

    closeContextBoundTabs();

    if (profile.isValid()) {
        applyPinnedToolSettings();
        statusBar()->showMessage(tr("Editing context switched to %1").arg(profile.name), 5000);
    } else {
        statusBar()->showMessage(tr("Editing context cleared"), 3000);
    }

    if (!m_openToolTabsRestored && flatlas::core::Config::instance().getBool(QStringLiteral("restoreOpenTabs"), false)) {
        m_openToolTabsRestored = true;
        restoreOpenToolTabs();
    }
    m_suppressTabStateSave = false;
    saveOpenToolTabs();
}

void MainWindow::closeContextBoundTabs()
{
    if (!m_centerTabs)
        return;

    for (int i = m_centerTabs->count() - 1; i >= 0; --i) {
        QWidget *widget = m_centerTabs->widget(i);
        if (!widget || !isContextBoundTab(widget))
            continue;
        m_centerTabs->removeTab(i, true);
    }
}

void MainWindow::openSystemFromUniverse(const QString &nickname,
                                        const QString &systemFile,
                                        const QString &ingameName)
{
    const QString resolvedPath = QDir::cleanPath(systemFile);
    if (resolvedPath.isEmpty() || !QFileInfo::exists(resolvedPath)) {
        QMessageBox::warning(
            this,
            tr("Error"),
            tr("Could not resolve system file for '%1':\n%2").arg(nickname, systemFile));
        return;
    }

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
    const auto updateLoadProgress = [this](int percent, const QString &message) {
        if (m_progressBar) {
            m_progressBar->setValue(std::clamp(percent, 0, 100));
            m_progressBar->update();
        }
        if (!message.trimmed().isEmpty())
            statusBar()->showMessage(message);
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    };
    connect(editor, &flatlas::editors::SystemEditorPage::loadingProgressChanged,
            this, updateLoadProgress);
    updateLoadProgress(0, tr("Opening system: %1").arg(nickname));
    if (!editor->loadFile(resolvedPath)) {
        updateLoadProgress(100, tr("Could not load system file"));
        QMessageBox::warning(this, tr("Error"),
                             tr("Could not load system file:\n%1").arg(resolvedPath));
        delete editor;
        return;
    }

    int idx = m_centerTabs->addTab(editor, formatSystemTabTitle(editor->document()->name(), ingameName));
    m_centerTabs->setCurrentIndex(idx);

    connect(editor, &flatlas::editors::SystemEditorPage::titleChanged,
            this, [this, editor, ingameName](const QString &title) {
        int i = m_centerTabs->indexOf(editor);
        if (i >= 0)
            m_centerTabs->setTabText(i, formatSystemTabTitle(title, ingameName));
    });
    connect(editor, &flatlas::editors::SystemEditorPage::selectionStatusChanged,
            this, [this](const QString &message) {
        statusBar()->showMessage(message);
    });
    connect(editor, &flatlas::editors::SystemEditorPage::open3DSystemViewRequested,
            this, [this, editor]() {
        open3DSystemEditorFor(editor);
    });

    updateLoadProgress(100, tr("Opened system: %1").arg(nickname));
    statusBar()->showMessage(tr("Opened system: %1").arg(nickname), 3000);
}

void MainWindow::open3DSystemEditorFor(flatlas::editors::SystemEditorPage *editor)
{
    if (!editor || !editor->document())
        return;

    const QString systemName = editor->document()->name().trimmed().isEmpty()
        ? QFileInfo(editor->filePath()).completeBaseName()
        : editor->document()->name().trimmed();
    const QString tabKey = QStringLiteral("system3d:%1").arg(QDir::cleanPath(editor->filePath()).toLower());

    for (int i = 0; i < m_centerTabs->count(); ++i) {
        QWidget *widget = m_centerTabs->widget(i);
        if (widget && widget->objectName() == tabKey) {
            m_centerTabs->setCurrentIndex(i);
            return;
        }
    }

    auto *view = createSystem3DPage(editor->document(),
                                    editor->archetypeModelPathsFor3DView(),
                                    editor->archetypeDisplayRadiiFor3DView(),
                                    editor->archetypeTextureSourcePathsFor3DView(),
                                    editor->displayFilterSettingsFor3DView(),
                                    tabKey,
                                    editor,
                                    this);

    const int idx = m_centerTabs->addTab(view, tr("3D: %1").arg(systemName));
    m_centerTabs->setCurrentIndex(idx);
    statusBar()->showMessage(tr("3D system view opened: %1").arg(systemName), 3000);
}

void MainWindow::openTradeRoutes()
{
    auto *page = new flatlas::editors::TradeRoutePage(this);
    auto &ctx = flatlas::core::EditingContext::instance();
    if (ctx.hasContext()) {
        const QString dataDir = flatlas::core::PathUtils::ciResolvePath(
            ctx.primaryGamePath(), QStringLiteral("DATA"));
        if (!dataDir.isEmpty()) {
            page->setDataPath(dataDir);
            auto *universeEditor = qobject_cast<flatlas::editors::UniverseEditorPage *>(m_centerTabs->currentWidget());
            if (universeEditor && universeEditor->data())
                page->setUniverseData(universeEditor->data());
            page->scanAndCalculate();
        }
    }

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
    auto &ctx = flatlas::core::EditingContext::instance();
    if (ctx.hasContext()) {
        const QString exeDir = flatlas::core::PathUtils::ciResolvePath(
            ctx.primaryGamePath(), QStringLiteral("EXE"));
        if (!exeDir.isEmpty())
            editor->loadFreelancerDir(exeDir);
    }

    int idx = m_centerTabs->addTab(editor, tr("IDS Editor"));
    m_centerTabs->setCurrentIndex(idx);

    connect(editor, &flatlas::editors::IdsEditorPage::titleChanged,
            this, [this, editor](const QString &title) {
        int i = m_centerTabs->indexOf(editor);
        if (i >= 0)
            m_centerTabs->setTabText(i, title);
    });
    connect(editor, &flatlas::editors::IdsEditorPage::openIniRequested,
            this, [this](const QString &filePath, const QString &searchText) {
        openIniFile(filePath, searchText, 0);
    });

    statusBar()->showMessage(tr("IDS Editor opened"), 3000);
}

void MainWindow::openModManager()
{
    // Mod Manager is always pinned at index 0 — just switch to it
    m_centerTabs->setCurrentIndex(0);
}

void MainWindow::openModSettings()
{
    auto *editor = new flatlas::editors::ModSettingsPage(this);

    int idx = m_centerTabs->addTab(editor, tr("Mod Settings"));
    m_centerTabs->setCurrentIndex(idx);

    connect(editor, &flatlas::editors::ModSettingsPage::titleChanged,
            this, [this, editor](const QString &title) {
        int i = m_centerTabs->indexOf(editor);
        if (i >= 0)
            m_centerTabs->setTabText(i, title);
    });

    statusBar()->showMessage(tr("Mod Settings opened"), 3000);
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

void MainWindow::openNewsRumorEditor()
{
    auto *editor = new flatlas::editors::NewsRumorEditor(this);

    int idx = m_centerTabs->addTab(editor, tr("News Editor"));
    m_centerTabs->setCurrentIndex(idx);

    connect(editor, &flatlas::editors::NewsRumorEditor::titleChanged,
            this, [this, editor](const QString &title) {
        int i = m_centerTabs->indexOf(editor);
        if (i >= 0)
            m_centerTabs->setTabText(i, title);
    });

    statusBar()->showMessage(tr("News Editor opened"), 3000);
}

void MainWindow::openModelViewer()
{
    if (ensureModelViewerPage())
        statusBar()->showMessage(tr("3D Model Viewer opened"), 3000);
}

flatlas::rendering::ModelViewerPage *MainWindow::ensureModelViewerPage()
{
    for (int i = 0; i < m_centerTabs->count(); ++i) {
        if (qobject_cast<flatlas::rendering::ModelViewerPage *>(m_centerTabs->widget(i))) {
            m_centerTabs->setCurrentIndex(i);
            return qobject_cast<flatlas::rendering::ModelViewerPage *>(m_centerTabs->widget(i));
        }
    }

    try {
        auto *page = new flatlas::rendering::ModelViewerPage(this);
        const int idx = m_centerTabs->addTab(page, tr("3D Model Viewer"));
        m_centerTabs->setCurrentIndex(idx);
        return page;
    } catch (const std::exception &ex) {
        QMessageBox::critical(this,
                              tr("3D Model Viewer"),
                              tr("The 3D Model Viewer could not be opened.\n\n%1")
                                  .arg(QString::fromLocal8Bit(ex.what())));
    } catch (...) {
        QMessageBox::critical(this,
                              tr("3D Model Viewer"),
                              tr("The 3D Model Viewer could not be opened due to an unexpected initialization error."));
    }
    return nullptr;
}

bool MainWindow::showModelInViewer(const QString &modelPath, const QString &displayLabel)
{
    auto *page = ensureModelViewerPage();
    if (!page)
        return false;
    const bool scheduled = page->loadModelPath(modelPath, displayLabel);
    if (scheduled)
        statusBar()->showMessage(displayLabel.trimmed().isEmpty() ? tr("3D model loaded") : displayLabel.trimmed(), 3000);
    return scheduled;
}
