// editors/system/SystemEditorPage.cpp – System-Editor (Phase 5)

#include "SystemEditorPage.h"
#include "SystemDisplayFilterDialog.h"
#include "SystemPersistence.h"
#include "SystemUndoCommands.h"
#include "core/Theme.h"
#include "editors/ini/IniCodeEditor.h"
#include "editors/ini/IniSyntaxHighlighter.h"

#include "core/Config.h"
#include "core/EditingContext.h"
#include "domain/SystemDocument.h"
#include "domain/SolarObject.h"
#include "domain/ZoneItem.h"
#include "rendering/view2d/MapScene.h"
#include "rendering/view2d/SystemMapView.h"
#include "rendering/view2d/items/SolarObjectItem.h"
#include "rendering/view2d/items/ZoneItem2D.h"
#include "rendering/view3d/SceneView3D.h"
#include "core/UndoManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QAction>
#include <QMessageBox>
#include <QInputDialog>
#include <QMetaEnum>
#include <QPixmap>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QGroupBox>
#include <QSet>
#include <QBrush>
#include <QFrame>
#include <QFileInfo>
#include <QDesktopServices>
#include <QFont>
#include <QPalette>
#include <QSignalBlocker>
#include <QUrl>

using namespace flatlas::domain;
using namespace flatlas::rendering;
using namespace flatlas::infrastructure;

namespace flatlas::editors {

namespace {

QPushButton *makeSidebarButton(const QString &text, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setMinimumHeight(28);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setStyleSheet(QStringLiteral("text-align:left; padding:5px 10px;"));
    return button;
}

SystemDisplayFilterRule makeDefaultDisplayRule(const QString &id,
                                               const QString &name,
                                               DisplayFilterTarget target,
                                               const QString &pattern)
{
    SystemDisplayFilterRule rule;
    rule.id = id;
    rule.name = name;
    rule.enabled = true;
    rule.target = target;
    rule.field = DisplayFilterField::Nickname;
    rule.matchMode = DisplayFilterMatchMode::Contains;
    rule.action = DisplayFilterAction::Hide;
    rule.pattern = pattern;
    return rule;
}

SystemDisplayFilterSettings defaultDisplayFilterSettings()
{
    SystemDisplayFilterSettings settings;
    settings.rules = {
        makeDefaultDisplayRule(QStringLiteral("default-hide-tradelane-label"),
                               QStringLiteral("HIDE Trade_Lane_Ring LABEL"),
                               DisplayFilterTarget::Label,
                               QStringLiteral("Trade_Lane_Ring")),
        makeDefaultDisplayRule(QStringLiteral("default-hide-patrol"),
                               QStringLiteral("HIDE patrol LABEL + ZONE"),
                               DisplayFilterTarget::Both,
                               QStringLiteral("_path_")),
        makeDefaultDisplayRule(QStringLiteral("default-hide-destroy-vignette"),
                               QStringLiteral("Hide destroy_vignette LABEL + ZONE"),
                               DisplayFilterTarget::Both,
                               QStringLiteral("destroy_vignette"))
    };
    return settings;
}

}

SystemEditorPage::SystemEditorPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    applyThemeStyling();
    connect(&flatlas::core::Theme::instance(), &flatlas::core::Theme::themeChanged,
            this, [this](const QString &) { applyThemeStyling(); });
}

SystemEditorPage::~SystemEditorPage()
{
    m_isShuttingDown = true;
    if (m_mapScene)
        disconnect(m_mapScene, nullptr, this, nullptr);
    if (m_mapView)
        disconnect(m_mapView, nullptr, this, nullptr);
    if (m_objectTree)
        disconnect(m_objectTree, nullptr, this, nullptr);
    if (m_sceneView3D)
        disconnect(m_sceneView3D, nullptr, this, nullptr);

    if (m_document)
        SystemPersistence::clearExtras(m_document.get());
}

void SystemEditorPage::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    setupToolBar();
    mainLayout->addWidget(m_toolBar);

    m_splitter = new QSplitter(Qt::Horizontal, this);

    auto *leftSidebar = new QWidget(this);
    auto *leftSidebarLayout = new QVBoxLayout(leftSidebar);
    leftSidebarLayout->setContentsMargins(0, 0, 0, 0);
    leftSidebarLayout->setSpacing(0);

    m_leftSidebarSplitter = new QSplitter(Qt::Vertical, leftSidebar);
    leftSidebarLayout->addWidget(m_leftSidebarSplitter);

    // Object tree (left top)
    auto *objectListHost = new QWidget(m_leftSidebarSplitter);
    auto *objectListLayout = new QVBoxLayout(objectListHost);
    objectListLayout->setContentsMargins(0, 0, 0, 0);
    objectListLayout->setSpacing(6);

    m_objectSearchEdit = new QLineEdit(objectListHost);
    m_objectSearchEdit->setPlaceholderText(tr("Objekte durchsuchen..."));
    m_objectSearchEdit->setClearButtonEnabled(true);
    objectListLayout->addWidget(m_objectSearchEdit);

    m_objectSearchHintLabel = new QLabel(tr("Keine Objekte gefunden"), objectListHost);
    m_objectSearchHintLabel->setVisible(false);
    m_objectSearchHintLabel->setStyleSheet(QStringLiteral("color:#9ca3af; padding:0 2px 4px 2px;"));
    objectListLayout->addWidget(m_objectSearchHintLabel);

    m_objectTree = new QTreeWidget(objectListHost);
    m_objectTree->setHeaderLabels({tr("Nickname"), tr("Type")});
    m_objectTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_objectTree->setMinimumWidth(200);
    objectListLayout->addWidget(m_objectTree, 1);
    m_leftSidebarSplitter->addWidget(objectListHost);

    auto *editorHost = new QWidget(m_leftSidebarSplitter);
    auto *editorLayout = new QVBoxLayout(editorHost);
    editorLayout->setContentsMargins(8, 8, 8, 8);
    editorLayout->setSpacing(6);

    auto *iniEditorTitle = new QLabel(tr("Objekt-Editor"), editorHost);
    iniEditorTitle->setStyleSheet(QStringLiteral("font-size:18px; font-weight:600;"));
    editorLayout->addWidget(iniEditorTitle);

    m_editorStack = new QStackedWidget(editorHost);
    editorLayout->addWidget(m_editorStack, 1);
    m_emptyEditorPage = new QWidget(m_editorStack);
    auto *emptyLayout = new QVBoxLayout(m_emptyEditorPage);
    emptyLayout->setContentsMargins(0, 0, 0, 0);
    emptyLayout->setSpacing(6);
    m_emptyEditorLabel = new QLabel(tr("Wähle links ein Objekt oder eine Zone aus."), m_emptyEditorPage);
    m_emptyEditorLabel->setWordWrap(true);
    m_emptyEditorLabel->setStyleSheet(QStringLiteral("color:#9ca3af; padding:6px 2px;"));
    emptyLayout->addWidget(m_emptyEditorLabel);
    emptyLayout->addStretch(1);
    m_editorStack->addWidget(m_emptyEditorPage);

    m_singleEditorPage = new QWidget(m_editorStack);
    auto *singleEditorLayout = new QVBoxLayout(m_singleEditorPage);
    singleEditorLayout->setContentsMargins(0, 0, 0, 0);
    singleEditorLayout->setSpacing(6);
    auto *iniEditorHost = editorHost;
    auto *iniEditorLayout = singleEditorLayout;
    m_iniEditor = new IniCodeEditor(editorHost);
    m_iniEditorHighlighter = new IniSyntaxHighlighter(m_iniEditor->document());
    m_iniEditor->setPlaceholderText(tr("Wähle links ein Objekt oder eine Zone aus."));
    iniEditorLayout->addWidget(m_iniEditor, 1);

    m_applyIniButton = new QPushButton(tr("Übernehmen"), iniEditorHost);
    iniEditorLayout->addWidget(m_applyIniButton);

    m_openSystemIniButton = new QPushButton(tr("System ini öffnen"), iniEditorHost);
    iniEditorLayout->addWidget(m_openSystemIniButton);
    m_editorStack->addWidget(m_singleEditorPage);

    m_multiSelectionPage = new QWidget(m_editorStack);
    auto *multiLayout = new QVBoxLayout(m_multiSelectionPage);
    multiLayout->setContentsMargins(0, 0, 0, 0);
    multiLayout->setSpacing(6);
    m_multiSelectionLabel = new QLabel(tr("Mehrfachauswahl"), m_multiSelectionPage);
    m_multiSelectionLabel->setStyleSheet(QStringLiteral("font-size:15px; font-weight:600;"));
    multiLayout->addWidget(m_multiSelectionLabel);
    m_multiSelectionScrollArea = new QScrollArea(m_multiSelectionPage);
    m_multiSelectionScrollArea->setWidgetResizable(true);
    m_multiSelectionScrollArea->setFrameShape(QFrame::NoFrame);
    m_multiSelectionListHost = new QWidget(m_multiSelectionScrollArea);
    m_multiSelectionListLayout = new QVBoxLayout(m_multiSelectionListHost);
    m_multiSelectionListLayout->setContentsMargins(0, 0, 0, 0);
    m_multiSelectionListLayout->setSpacing(6);
    m_multiSelectionListLayout->addStretch(1);
    m_multiSelectionScrollArea->setWidget(m_multiSelectionListHost);
    multiLayout->addWidget(m_multiSelectionScrollArea, 1);
    m_editorStack->addWidget(m_multiSelectionPage);

    m_leftSidebarSplitter->addWidget(iniEditorHost);
    m_leftSidebarSplitter->setStretchFactor(0, 1);
    m_leftSidebarSplitter->setStretchFactor(1, 1);
    m_leftSidebarSplitter->setSizes({360, 420});

    m_splitter->addWidget(leftSidebar);

    // Map view (center)
    m_mapScene = new MapScene(this);
    m_mapView = new SystemMapView(this);
    m_mapView->setMapScene(m_mapScene);
    m_mapView->setBackgroundPixmap(QPixmap(QStringLiteral(":/images/star-background.png")),
                                   palette().color(QPalette::Base));
    loadDisplayFilterSettings();
    m_mapView->setDisplayFilterSettings(m_displayFilterSettings);
    m_splitter->addWidget(m_mapView);

    setupRightSidebar();
    m_splitter->addWidget(m_rightSidebar);

    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setStretchFactor(2, 0);
    m_splitter->setSizes({220, 1000, 340});

    mainLayout->addWidget(m_splitter);

    connectSignals();
    updateSelectionSummary();
    updateSidebarButtons();
}

void SystemEditorPage::setupToolBar()
{
    m_toolBar = new QToolBar(this);
    m_toolBar->setIconSize(QSize(16, 16));

    auto *displayFiltersAction = m_toolBar->addAction(tr("Display Filters"));
    connect(displayFiltersAction, &QAction::triggered, this, &SystemEditorPage::openDisplayFilterDialog);

    auto *moveAction = m_toolBar->addAction(tr("MOVE"));
    moveAction->setCheckable(true);
    moveAction->setChecked(false);
    connect(moveAction, &QAction::toggled, this, [this](bool enabled) {
        if (m_mapScene)
            m_mapScene->setMoveEnabled(enabled);
    });

    m_toolBar->addSeparator();

    auto *zoomFitAction = m_toolBar->addAction(tr("Zoom Fit"));
    connect(zoomFitAction, &QAction::triggered, this, [this]() {
        if (m_mapView)
            m_mapView->zoomToFit();
    });

    auto *gridAction = m_toolBar->addAction(tr("Grid"));
    gridAction->setCheckable(true);
    gridAction->setChecked(true);
    connect(gridAction, &QAction::toggled, this, [this](bool checked) {
        if (m_mapScene)
            m_mapScene->setGridVisible(checked);
    });

}

void SystemEditorPage::connectSignals()
{
    connect(m_mapScene, &MapScene::selectionNicknamesChanged, this, &SystemEditorPage::onCanvasSelectionChanged);
    connect(m_applyIniButton, &QPushButton::clicked, this, &SystemEditorPage::applyIniEditorChanges);
    connect(m_openSystemIniButton, &QPushButton::clicked, this, &SystemEditorPage::openSystemIniExternally);
    connect(m_mapView, &SystemMapView::itemsMoved, this, &SystemEditorPage::onItemsMoved);

    // 3D → 2D selection sync

    connect(m_objectTree, &QTreeWidget::itemSelectionChanged,
            this, &SystemEditorPage::onTreeSelectionChanged);
    connect(m_objectSearchEdit, &QLineEdit::textChanged,
            this, [this]() { applyObjectListSearchFilter(); });
}

void SystemEditorPage::applyThemeStyling()
{
    const QPalette pal = palette();
    const bool lightTheme = pal.color(QPalette::Base).lightness() >= 170;
    const QColor dim = pal.color(QPalette::PlaceholderText);
    const QColor title = pal.color(QPalette::Text);
    const QColor border = pal.color(QPalette::Mid);

    if (m_objectSearchHintLabel)
        m_objectSearchHintLabel->setStyleSheet(QStringLiteral("color:%1; padding:0 2px 4px 2px;").arg(dim.name()));
    if (m_emptyEditorLabel)
        m_emptyEditorLabel->setStyleSheet(QStringLiteral("color:%1; padding:6px 2px;").arg(dim.name()));
    if (m_multiSelectionLabel)
        m_multiSelectionLabel->setStyleSheet(QStringLiteral("font-size:15px; font-weight:600; color:%1;").arg(title.name()));
    if (m_selectionTitleLabel)
        m_selectionTitleLabel->setStyleSheet(QStringLiteral("font-size:24px; font-weight:700; color:%1;").arg(title.name()));
    if (m_selectionSubtitleLabel)
        m_selectionSubtitleLabel->setStyleSheet(QStringLiteral("color:%1;").arg(dim.name()));
    if (m_deleteSidebarButton) {
        const QColor deleteBg = QColor::fromRgb(160, 46, 46);
        const QColor deleteHover = QColor::fromRgb(184, 58, 58);
        m_deleteSidebarButton->setStyleSheet(
            QStringLiteral("QPushButton { text-align:left; padding:5px 10px; background-color:%1; color:white; font-weight:700;"
                           " border:1px solid %2; border-radius:3px; }"
                           "QPushButton:hover { background-color:%3; }")
                .arg(deleteBg.name(), border.name(), deleteHover.name()));
    }
    refreshSidebarVisibilityState();
    if (m_mapView) {
        m_mapView->setBackgroundPixmap(lightTheme ? QPixmap() : QPixmap(QStringLiteral(":/images/star-background.png")),
                                       pal.color(QPalette::Base));
        m_mapView->applyTheme();
    }
}

bool SystemEditorPage::loadFile(const QString &filePath)
{
    auto doc = SystemPersistence::load(filePath);
    if (!doc)
        return false;

    m_document = std::move(doc);
    bindDocumentSignals();
    m_selectedNicknames.clear();
    loadDisplayFilterSettings();
    m_mapScene->loadDocument(m_document.get());
    m_mapView->setSystemName(m_document->name());
    m_mapView->setDisplayFilterSettings(m_displayFilterSettings);
    if (m_is3DViewEnabled) {
        ensureSceneView3D();
        m_sceneView3D->loadDocument(m_document.get());
    }
    refreshObjectList();
    m_mapView->scheduleInitialFit();
    refreshTitle();
    return true;
}

void SystemEditorPage::setDocument(std::unique_ptr<SystemDocument> doc)
{
    if (m_document)
        SystemPersistence::clearExtras(m_document.get());
    m_document = std::move(doc);
    bindDocumentSignals();
    m_selectedNicknames.clear();
    loadDisplayFilterSettings();
    m_mapScene->loadDocument(m_document.get());
    m_mapView->setSystemName(m_document->name());
    m_mapView->setDisplayFilterSettings(m_displayFilterSettings);
    if (m_is3DViewEnabled) {
        ensureSceneView3D();
        m_sceneView3D->loadDocument(m_document.get());
    }
    refreshObjectList();
    m_mapView->scheduleInitialFit();
    refreshTitle();
}

bool SystemEditorPage::save()
{
    if (!m_document || m_document->filePath().isEmpty())
        return false;
    bool ok = SystemPersistence::save(*m_document);
    if (ok)
        m_document->setDirty(false);
    return ok;
}

bool SystemEditorPage::saveAs(const QString &filePath)
{
    if (!m_document)
        return false;
    m_document->setFilePath(filePath);
    return save();
}

SystemDocument *SystemEditorPage::document() const
{
    return m_document.get();
}

QString SystemEditorPage::filePath() const
{
    return m_document ? m_document->filePath() : QString();
}

bool SystemEditorPage::isDirty() const
{
    return m_document && m_document->isDirty();
}

void SystemEditorPage::bindDocumentSignals()
{
    if (!m_document)
        return;

    connect(m_document.get(), &SystemDocument::dirtyChanged,
            this, &SystemEditorPage::refreshTitle, Qt::UniqueConnection);
    connect(m_document.get(), &SystemDocument::nameChanged,
            this, &SystemEditorPage::refreshTitle, Qt::UniqueConnection);
}

void SystemEditorPage::refreshTitle()
{
    if (!m_document)
        return;

    QString title = m_document->name().trimmed();
    if (title.isEmpty())
        title = tr("System");
    if (m_document->isDirty())
        title += QLatin1Char('*');
    emit titleChanged(title);
}

void SystemEditorPage::ensureSceneView3D()
{
    if (m_sceneView3D)
        return;

    m_sceneView3D = new SceneView3D(m_rightSidebar);

    connect(m_sceneView3D, &SceneView3D::objectSelected,
            this, [this](const QString &nickname) {
        onCanvasSelectionChanged({nickname});
    });
}

void SystemEditorPage::set3DViewEnabled(bool enabled)
{
    m_is3DViewEnabled = enabled;

    if (!enabled) {
        return;
    }

    ensureSceneView3D();
    if (m_document)
        m_sceneView3D->loadDocument(m_document.get());
}

void SystemEditorPage::openDisplayFilterDialog()
{
    SystemDisplayFilterDialog dialog(m_displayFilterSettings, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_displayFilterSettings = dialog.settings();
    saveDisplayFilterSettings();
    if (m_mapView)
        m_mapView->setDisplayFilterSettings(m_displayFilterSettings);
    pruneSelectionByCurrentFilter();
    refreshSidebarVisibilityState();
    syncTreeSelectionFromNicknames(m_selectedNicknames);
    syncSceneSelectionFromNicknames(m_selectedNicknames);
    updateSelectionSummary();
    updateIniEditorForSelection();
    updateSidebarButtons();
}

void SystemEditorPage::loadDisplayFilterSettings()
{
    const QJsonObject rawSettings = flatlas::core::Config::instance().getJsonObject(displayFilterConfigKey());
    if (!rawSettings.contains(QStringLiteral("rules"))) {
        m_displayFilterSettings = defaultDisplayFilterSettings();
        return;
    }

    m_displayFilterSettings = flatlas::rendering::SystemDisplayFilterSettings::fromJson(rawSettings);
}

void SystemEditorPage::saveDisplayFilterSettings() const
{
    auto &config = flatlas::core::Config::instance();
    config.setJsonObject(displayFilterConfigKey(), m_displayFilterSettings.toJson());
    config.save();
}

QString SystemEditorPage::displayFilterConfigKey() const
{
    const QString profileId = flatlas::core::EditingContext::instance().editingProfileId();
    return profileId.isEmpty()
        ? QStringLiteral("systemDisplayFilters.default")
        : QStringLiteral("systemDisplayFilters.%1").arg(profileId);
}

void SystemEditorPage::setupRightSidebar()
{
    m_rightSidebar = new QWidget(this);
    m_rightSidebar->setMinimumWidth(320);
    auto *layout = new QVBoxLayout(m_rightSidebar);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    m_selectionTitleLabel = new QLabel(tr("Kein Objekt ausgewählt"), m_rightSidebar);
    m_selectionTitleLabel->setStyleSheet(QStringLiteral("font-size:24px; font-weight:700;"));
    layout->addWidget(m_selectionTitleLabel);

    m_selectionSubtitleLabel = new QLabel(tr("Wähle ein Objekt oder eine Zone in der Karte oder Liste aus."), m_rightSidebar);
    m_selectionSubtitleLabel->setWordWrap(true);
    m_selectionSubtitleLabel->setStyleSheet(QStringLiteral("color:#9ca3af;"));
    layout->addWidget(m_selectionSubtitleLabel);

    auto *groupsHost = new QWidget(m_rightSidebar);
    auto *groupsLayout = new QGridLayout(groupsHost);
    groupsLayout->setContentsMargins(0, 0, 0, 0);
    groupsLayout->setHorizontalSpacing(8);
    groupsLayout->setVerticalSpacing(0);

    auto *creationGroup = new QGroupBox(tr("Erstellung"), groupsHost);
    auto *creationLayout = new QVBoxLayout(creationGroup);
    creationLayout->setContentsMargins(6, 6, 6, 6);
    creationLayout->setSpacing(4);

    m_createObjectButton = makeSidebarButton(tr("Objekt"), creationGroup);
    connect(m_createObjectButton, &QPushButton::clicked, this, &SystemEditorPage::onAddObject);
    creationLayout->addWidget(m_createObjectButton);

    m_createAsteroidNebulaButton = makeSidebarButton(tr("Asteroid / Nebel"), creationGroup);
    connect(m_createAsteroidNebulaButton, &QPushButton::clicked, this, [this]() {
        showNotYetPorted(tr("Asteroid / Nebel"), QStringLiteral("FLAtlas/fl_editor/main_window.py::_start_zone_creation"));
    });
    creationLayout->addWidget(m_createAsteroidNebulaButton);

    m_createZoneButton = makeSidebarButton(tr("Zone"), creationGroup);
    connect(m_createZoneButton, &QPushButton::clicked, this, [this]() {
        showNotYetPorted(tr("Zone"), QStringLiteral("FLAtlas/fl_editor/main_window.py::_start_simple_zone_creation"));
    });
    creationLayout->addWidget(m_createZoneButton);

    m_createPatrolZoneButton = makeSidebarButton(tr("Patrol-Zone"), creationGroup);
    connect(m_createPatrolZoneButton, &QPushButton::clicked, this, [this]() {
        showNotYetPorted(tr("Patrol-Zone"), QStringLiteral("FLAtlas/fl_editor/main_window.py::_start_patrol_zone_creation"));
    });
    creationLayout->addWidget(m_createPatrolZoneButton);

    m_createJumpButton = makeSidebarButton(tr("Jump"), creationGroup);
    connect(m_createJumpButton, &QPushButton::clicked, this, [this]() {
        showNotYetPorted(tr("Jump"), QStringLiteral("FLAtlas/fl_editor/main_window.py::_start_connection_dialog"));
    });
    creationLayout->addWidget(m_createJumpButton);

    m_createSunButton = makeSidebarButton(tr("Sonne"), creationGroup);
    connect(m_createSunButton, &QPushButton::clicked, this, &SystemEditorPage::onCreateSun);
    creationLayout->addWidget(m_createSunButton);

    m_createPlanetButton = makeSidebarButton(tr("Planet"), creationGroup);
    connect(m_createPlanetButton, &QPushButton::clicked, this, &SystemEditorPage::onCreatePlanet);
    creationLayout->addWidget(m_createPlanetButton);

    m_createLightButton = makeSidebarButton(tr("Lichtquelle"), creationGroup);
    connect(m_createLightButton, &QPushButton::clicked, this, [this]() {
        showNotYetPorted(tr("Lichtquelle"), QStringLiteral("FLAtlas/fl_editor/main_window.py::_start_light_source_creation"));
    });
    creationLayout->addWidget(m_createLightButton);

    m_createWreckButton = makeSidebarButton(tr("Wrack/Surprise"), creationGroup);
    connect(m_createWreckButton, &QPushButton::clicked, this, [this]() {
        createQuickObject(SolarObject::Wreck, QStringLiteral("new_wreck"), QStringLiteral("surprise_object"));
    });
    creationLayout->addWidget(m_createWreckButton);

    m_createBuoyButton = makeSidebarButton(tr("Boje"), creationGroup);
    connect(m_createBuoyButton, &QPushButton::clicked, this, &SystemEditorPage::onCreateBuoy);
    creationLayout->addWidget(m_createBuoyButton);

    m_createWeaponPlatformButton = makeSidebarButton(tr("Waffenplattform"), creationGroup);
    connect(m_createWeaponPlatformButton, &QPushButton::clicked, this, &SystemEditorPage::onCreateWeaponPlatform);
    creationLayout->addWidget(m_createWeaponPlatformButton);

    m_createDepotButton = makeSidebarButton(tr("Depot"), creationGroup);
    connect(m_createDepotButton, &QPushButton::clicked, this, &SystemEditorPage::onCreateDepot);
    creationLayout->addWidget(m_createDepotButton);

    m_createTradelaneButton = makeSidebarButton(tr("Tradelane"), creationGroup);
    connect(m_createTradelaneButton, &QPushButton::clicked, this, &SystemEditorPage::onCreateTradeLane);
    creationLayout->addWidget(m_createTradelaneButton);

    m_createBaseButton = makeSidebarButton(tr("Base"), creationGroup);
    connect(m_createBaseButton, &QPushButton::clicked, this, &SystemEditorPage::onCreateBase);
    creationLayout->addWidget(m_createBaseButton);

    m_createDockingRingButton = makeSidebarButton(tr("Docking Ring"), creationGroup);
    connect(m_createDockingRingButton, &QPushButton::clicked, this, &SystemEditorPage::onCreateDockingRing);
    creationLayout->addWidget(m_createDockingRingButton);

    m_createRingButton = makeSidebarButton(tr("Ring"), creationGroup);
    connect(m_createRingButton, &QPushButton::clicked, this, [this]() {
        showNotYetPorted(tr("Ring"), QStringLiteral("FLAtlas/fl_editor/main_window.py::_start_ring_attach"));
    });
    creationLayout->addWidget(m_createRingButton);
    creationLayout->addStretch(1);

    auto *editingGroup = new QGroupBox(tr("Bearbeitung"), groupsHost);
    auto *editingLayout = new QVBoxLayout(editingGroup);
    editingLayout->setContentsMargins(6, 6, 6, 6);
    editingLayout->setSpacing(4);

    m_editTradelaneButton = makeSidebarButton(tr("Tradelane"), editingGroup);
    connect(m_editTradelaneButton, &QPushButton::clicked, this, [this]() {
        showNotYetPorted(tr("Tradelane bearbeiten"), QStringLiteral("FLAtlas/fl_editor/main_window.py::_edit_tradelane"));
    });
    editingLayout->addWidget(m_editTradelaneButton);

    m_editZonePopulationButton = makeSidebarButton(tr("Zone Population"), editingGroup);
    connect(m_editZonePopulationButton, &QPushButton::clicked, this, [this]() {
        showNotYetPorted(tr("Zone Population"), QStringLiteral("FLAtlas/fl_editor/main_window.py::_edit_zone_population"));
    });
    editingLayout->addWidget(m_editZonePopulationButton);

    m_editRingButton = makeSidebarButton(tr("Ring"), editingGroup);
    connect(m_editRingButton, &QPushButton::clicked, this, [this]() {
        showNotYetPorted(tr("Ring bearbeiten"), QStringLiteral("FLAtlas/fl_editor/main_window.py::_open_ring_manager_dialog"));
    });
    editingLayout->addWidget(m_editRingButton);

    m_addExclusionZoneButton = makeSidebarButton(tr("Exclusion Zone hinzufügen..."), editingGroup);
    connect(m_addExclusionZoneButton, &QPushButton::clicked, this, [this]() {
        showNotYetPorted(tr("Exclusion Zone"), QStringLiteral("FLAtlas/fl_editor/main_window.py::_start_exclusion_zone_creation"));
    });
    editingLayout->addWidget(m_addExclusionZoneButton);

    m_editBaseButton = makeSidebarButton(tr("Base"), editingGroup);
    connect(m_editBaseButton, &QPushButton::clicked, this, [this]() {
        showNotYetPorted(tr("Base bearbeiten"), QStringLiteral("FLAtlas/fl_editor/main_window.py::_edit_base"));
    });
    editingLayout->addWidget(m_editBaseButton);

    m_baseBuilderButton = makeSidebarButton(tr("Base Builder"), editingGroup);
    connect(m_baseBuilderButton, &QPushButton::clicked, this, [this]() {
        showNotYetPorted(tr("Base Builder"), QStringLiteral("FLAtlas/fl_editor/main_window.py::_open_base_builder_for_object"));
    });
    editingLayout->addWidget(m_baseBuilderButton);

    editingLayout->addSpacing(12);
    m_deleteSidebarButton = makeSidebarButton(tr("DELETE"), editingGroup);
    m_deleteSidebarButton->setStyleSheet(QStringLiteral("text-align:left; padding:5px 10px; background-color:#7f1d1d; color:white; font-weight:700;"));
    connect(m_deleteSidebarButton, &QPushButton::clicked, this, &SystemEditorPage::onDeleteSelected);
    editingLayout->addWidget(m_deleteSidebarButton);
    editingLayout->addStretch(1);

    groupsLayout->addWidget(creationGroup, 0, 0);
    groupsLayout->addWidget(editingGroup, 0, 1);
    layout->addWidget(groupsHost);

    m_systemSettingsButton = new QPushButton(tr("System-Einstellungen"), m_rightSidebar);
    m_systemSettingsButton->setMinimumHeight(30);
    connect(m_systemSettingsButton, &QPushButton::clicked, this, [this]() {
        showNotYetPorted(tr("System-Einstellungen"), QStringLiteral("FLAtlas/fl_editor/main_window.py::_open_system_settings"));
    });
    layout->addWidget(m_systemSettingsButton);

    auto *jumpRow = new QHBoxLayout();
    m_objectJumpCombo = new QComboBox(m_rightSidebar);
    m_objectJumpCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    jumpRow->addWidget(m_objectJumpCombo, 1);
    m_objectJumpButton = new QPushButton(tr("Springen"), m_rightSidebar);
    connect(m_objectJumpButton, &QPushButton::clicked, this, &SystemEditorPage::jumpToSelectedFromSidebar);
    jumpRow->addWidget(m_objectJumpButton);
    layout->addLayout(jumpRow);

    auto *divider = new QFrame(m_rightSidebar);
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Sunken);
    layout->addWidget(divider);

    m_systemFileInfoLabel = new QLabel(m_rightSidebar);
    m_systemFileInfoLabel->setWordWrap(true);
    layout->addWidget(m_systemFileInfoLabel);

    m_systemStatsLabel = new QLabel(m_rightSidebar);
    m_systemStatsLabel->setWordWrap(true);
    layout->addWidget(m_systemStatsLabel);

    m_saveFileButton = new QPushButton(tr("Änderungen in Datei schreiben"), m_rightSidebar);
    m_saveFileButton->setMinimumHeight(34);
    connect(m_saveFileButton, &QPushButton::clicked, this, [this]() {
        if (!save())
            QMessageBox::warning(this, tr("Speichern fehlgeschlagen"), tr("Die Systemdatei konnte nicht gespeichert werden."));
    });
    layout->addWidget(m_saveFileButton);
    layout->addStretch(1);
}

void SystemEditorPage::refreshObjectList()
{
    const QStringList previousSelection = m_selectedNicknames;
    m_objectTree->clear();
    if (!m_document) {
        updateIniEditorForSelection();
        return;
    }

    auto *objRoot = new QTreeWidgetItem(m_objectTree, {tr("Objects")});
    objRoot->setFlags(objRoot->flags() & ~Qt::ItemIsSelectable);
    objRoot->setExpanded(true);
    for (const auto &obj : m_document->objects()) {
        auto *item = new QTreeWidgetItem(objRoot);
        item->setText(0, obj->nickname());
        item->setText(1, QMetaEnum::fromType<SolarObject::Type>().valueToKey(obj->type()));
    }

    auto *zoneRoot = new QTreeWidgetItem(m_objectTree, {tr("Zones")});
    zoneRoot->setFlags(zoneRoot->flags() & ~Qt::ItemIsSelectable);
    zoneRoot->setExpanded(true);
    for (const auto &zone : m_document->zones()) {
        auto *item = new QTreeWidgetItem(zoneRoot);
        item->setText(0, zone->nickname());
        item->setText(1, zone->zoneType());
    }

    refreshObjectJumpList();
    m_systemFileInfoLabel->setText(QStringLiteral("%1").arg(QFileInfo(m_document->filePath()).fileName()));
    m_systemStatsLabel->setText(tr("Objekte: %1\nZonen: %2")
                                    .arg(m_document->objects().size())
                                    .arg(m_document->zones().size()));
    applyObjectListSearchFilter();
    refreshSidebarVisibilityState();
    syncTreeSelectionFromNicknames(previousSelection);
    if (m_mapScene)
        syncSceneSelectionFromNicknames(previousSelection);
    updateIniEditorForSelection();
    updateSidebarButtons();
    updateSelectionSummary();
}

void SystemEditorPage::onCanvasSelectionChanged(const QStringList &nicknames)
{
    if (m_isShuttingDown)
        return;
    m_selectedNicknames = nicknames;
    pruneSelectionByCurrentFilter();
    syncTreeSelectionFromNicknames(m_selectedNicknames);

    const QString primaryNickname = primarySelectedNickname();
    if (m_sceneView3D && m_is3DViewEnabled && m_selectedNicknames.size() == 1)
        m_sceneView3D->selectObject(primaryNickname);

    if (m_objectJumpCombo && !primaryNickname.isEmpty())
        m_objectJumpCombo->setCurrentText(primaryNickname);

    if (m_document && m_selectedNicknames.size() == 1) {
        for (const auto &obj : m_document->objects()) {
            if (obj->nickname() == primaryNickname) {
                showObjectProperties(obj.get());
                break;
            }
        }
        for (const auto &zone : m_document->zones()) {
            if (zone->nickname() == primaryNickname) {
                showZoneProperties(zone.get());
                break;
            }
        }
    }

    updateSelectionSummary();
    updateIniEditorForSelection();
    updateSidebarButtons();
}

void SystemEditorPage::onTreeSelectionChanged()
{
    if (m_isShuttingDown)
        return;
    QStringList nicknames;
    const auto selectedItems = m_objectTree->selectedItems();
    nicknames.reserve(selectedItems.size());
    for (QTreeWidgetItem *item : selectedItems) {
        if (item && item->parent())
            nicknames.append(item->text(0));
    }
    nicknames.removeDuplicates();

    m_selectedNicknames = nicknames;
    pruneSelectionByCurrentFilter();
    syncSceneSelectionFromNicknames(m_selectedNicknames);

    if (m_sceneView3D && m_is3DViewEnabled && m_selectedNicknames.size() == 1)
        m_sceneView3D->selectObject(primarySelectedNickname());

    updateSelectionSummary();
    updateIniEditorForSelection();
    updateSidebarButtons();
}

void SystemEditorPage::onAddObject()
{
    if (!m_document)
        return;

    bool ok = false;
    QString nickname = QInputDialog::getText(this, tr("New Object"),
                                              tr("Nickname:"), QLineEdit::Normal,
                                              QStringLiteral("new_object"), &ok);
    if (!ok || nickname.isEmpty())
        return;

    auto obj = std::make_shared<SolarObject>();
    obj->setNickname(nickname);
    obj->setType(SolarObject::Other);

    auto *cmd = new AddObjectCommand(m_document.get(), obj);
    flatlas::core::UndoManager::instance().push(cmd);
    refreshObjectList();
}

void SystemEditorPage::onDeleteSelected()
{
    if (!m_document || m_selectedNicknames.isEmpty())
        return;

    QVector<std::shared_ptr<SolarObject>> objectsToRemove;
    QVector<std::shared_ptr<ZoneItem>> zonesToRemove;
    for (const QString &nickname : m_selectedNicknames) {
        for (const auto &obj : m_document->objects()) {
            if (obj->nickname() == nickname) {
                objectsToRemove.append(obj);
                break;
            }
        }
        for (const auto &zone : m_document->zones()) {
            if (zone->nickname() == nickname) {
                zonesToRemove.append(zone);
                break;
            }
        }
    }

    if (objectsToRemove.isEmpty() && zonesToRemove.isEmpty())
        return;

    auto *stack = flatlas::core::UndoManager::instance().stack();
    stack->beginMacro(tr("Delete Selection"));
    for (const auto &obj : objectsToRemove)
        stack->push(new RemoveObjectCommand(m_document.get(), obj));
    for (const auto &zone : zonesToRemove)
        stack->push(new RemoveZoneCommand(m_document.get(), zone));
    stack->endMacro();

    m_selectedNicknames.clear();
    refreshObjectList();
}

void SystemEditorPage::onDuplicateSelected()
{
    if (!m_document || m_selectedNicknames.size() != 1)
        return;
    const QString nickname = m_selectedNicknames.first();

    for (const auto &obj : m_document->objects()) {
        if (obj->nickname() == nickname) {
            auto dup = std::make_shared<SolarObject>();
            dup->setNickname(nickname + QStringLiteral("_copy"));
            dup->setArchetype(obj->archetype());
            dup->setPosition(obj->position() + QVector3D(500, 0, 0));
            dup->setRotation(obj->rotation());
            dup->setType(obj->type());
            dup->setBase(obj->base());
            dup->setDockWith(obj->dockWith());
            dup->setGotoTarget(obj->gotoTarget());
            dup->setLoadout(obj->loadout());
            dup->setComment(obj->comment());
            dup->setIdsName(obj->idsName());
            dup->setIdsInfo(obj->idsInfo());
            dup->setRawEntries(obj->rawEntries());

            auto *cmd = new AddObjectCommand(m_document.get(), dup,
                                              tr("Duplicate Object"));
            flatlas::core::UndoManager::instance().push(cmd);
            refreshObjectList();
            return;
        }
    }

    for (const auto &zone : m_document->zones()) {
        if (zone->nickname() == nickname) {
            auto dup = std::make_shared<ZoneItem>();
            dup->setNickname(nickname + QStringLiteral("_copy"));
            dup->setPosition(zone->position() + QVector3D(500, 0, 0));
            dup->setSize(zone->size());
            dup->setShape(zone->shape());
            dup->setRotation(zone->rotation());
            dup->setZoneType(zone->zoneType());
            dup->setComment(zone->comment());
            dup->setUsage(zone->usage());
            dup->setPopType(zone->popType());
            dup->setPathLabel(zone->pathLabel());
            dup->setTightnessXyz(zone->tightnessXyz());
            dup->setDamage(zone->damage());
            dup->setInterference(zone->interference());
            dup->setDragScale(zone->dragScale());
            dup->setSortKey(zone->sortKey());
            dup->setRawEntries(zone->rawEntries());

            auto *cmd = new AddZoneCommand(m_document.get(), dup,
                                            tr("Duplicate Zone"));
            flatlas::core::UndoManager::instance().push(cmd);
            refreshObjectList();
            return;
        }
    }
}

void SystemEditorPage::showObjectProperties(SolarObject *obj)
{
    Q_UNUSED(obj);
    // TODO Phase 5+: Populate PropertiesPanel with object data
}

void SystemEditorPage::showZoneProperties(ZoneItem *zone)
{
    Q_UNUSED(zone);
    // TODO Phase 5+: Populate PropertiesPanel with zone data
}

QString SystemEditorPage::serializeSelectionToIni() const
{
    if (!m_document || m_selectedNicknames.size() != 1)
        return {};
    const QString nickname = m_selectedNicknames.first().trimmed();

    for (const auto &obj : m_document->objects()) {
        if (obj->nickname() == nickname) {
            IniDocument doc;
            doc.append(SystemPersistence::serializeObjectSection(*obj));
            return IniParser::serialize(doc).trimmed();
        }
    }

    for (const auto &zone : m_document->zones()) {
        if (zone->nickname() == nickname) {
            IniDocument doc;
            doc.append(SystemPersistence::serializeZoneSection(*zone));
            return IniParser::serialize(doc).trimmed();
        }
    }

    return {};
}

void SystemEditorPage::updateIniEditorForSelection()
{
    if (m_isShuttingDown)
        return;
    if (!m_editorStack || !m_iniEditor || !m_applyIniButton || !m_openSystemIniButton) {
        return;
    }

    const QString iniText = serializeSelectionToIni();
    const bool hasSelection = !iniText.isEmpty();

    m_iniEditor->setPlainText(hasSelection ? iniText : QString());
    m_iniEditor->setEnabled(hasSelection);
    m_applyIniButton->setEnabled(hasSelection);
    if (!hasSelection) {
        if (m_selectedNicknames.isEmpty())
            m_iniEditor->setPlaceholderText(tr("Wähle links ein Objekt oder eine Zone aus."));
        else
            m_iniEditor->setPlaceholderText(tr("Mehrfachauswahl aktiv. Der Objekt-Editor unterstützt nur genau einen Eintrag."));
    }
    m_openSystemIniButton->setEnabled(m_document && !m_document->filePath().trimmed().isEmpty());
    updateEditorModeUi();
}

void SystemEditorPage::updateEditorModeUi()
{
    if (m_isShuttingDown)
        return;
    if (!m_editorStack)
        return;

    if (m_selectedNicknames.isEmpty()) {
        if (m_emptyEditorLabel)
            m_emptyEditorLabel->setText(tr("Wähle links ein Objekt oder eine Zone aus."));
        m_editorStack->setCurrentWidget(m_emptyEditorPage);
        return;
    }

    if (m_selectedNicknames.size() == 1) {
        m_editorStack->setCurrentWidget(m_singleEditorPage);
        return;
    }

    if (m_multiSelectionLabel) {
        m_multiSelectionLabel->setText(tr("%1 markierte Einträge").arg(m_selectedNicknames.size()));
    }
    rebuildMultiSelectionEditorList();
    m_editorStack->setCurrentWidget(m_multiSelectionPage);
}

void SystemEditorPage::rebuildMultiSelectionEditorList()
{
    if (m_isShuttingDown)
        return;
    if (!m_multiSelectionListLayout)
        return;

    while (m_multiSelectionListLayout->count() > 1) {
        QLayoutItem *item = m_multiSelectionListLayout->takeAt(0);
        if (!item)
            continue;
        if (QWidget *widget = item->widget())
            delete widget;
        delete item;
    }

    for (const QString &nickname : m_selectedNicknames) {
        QString kindText = tr("Objekt");
        if (m_document) {
            for (const auto &zone : m_document->zones()) {
                if (zone->nickname() == nickname) {
                    kindText = tr("Zone");
                    break;
                }
            }
        }
        m_multiSelectionListLayout->insertWidget(m_multiSelectionListLayout->count() - 1,
                                                 buildMultiSelectionRow(nickname, kindText));
    }
}

QWidget *SystemEditorPage::buildMultiSelectionRow(const QString &nickname, const QString &kindText)
{
    const QPalette pal = palette();
    const QColor border = pal.color(QPalette::Mid);
    const QColor bg = pal.color(QPalette::AlternateBase);
    const QColor dim = pal.color(QPalette::PlaceholderText);

    auto *row = new QFrame(m_multiSelectionListHost);
    row->setFrameShape(QFrame::StyledPanel);
    row->setStyleSheet(QStringLiteral("QFrame { border: 1px solid %1; border-radius: 4px; background:%2; }")
                           .arg(border.name(), bg.name()));
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(8);

    auto *textHost = new QWidget(row);
    auto *textLayout = new QVBoxLayout(textHost);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);
    auto *title = new QLabel(nickname, textHost);
    title->setStyleSheet(QStringLiteral("font-weight:600;"));
    auto *subtitle = new QLabel(kindText, textHost);
    subtitle->setStyleSheet(QStringLiteral("color:%1; font-size:11px;").arg(dim.name()));
    textLayout->addWidget(title);
    textLayout->addWidget(subtitle);
    layout->addWidget(textHost, 1);

    auto *removeButton = new QPushButton(tr("Aus Auswahl"), row);
    removeButton->setMinimumHeight(26);
    connect(removeButton, &QPushButton::clicked, this, [this, nickname]() {
        removeNicknameFromSelection(nickname);
    });
    layout->addWidget(removeButton);

    return row;
}

void SystemEditorPage::removeNicknameFromSelection(const QString &nickname)
{
    if (m_isShuttingDown)
        return;
    m_selectedNicknames.removeAll(nickname);
    m_selectedNicknames.removeDuplicates();
    syncTreeSelectionFromNicknames(m_selectedNicknames);
    syncSceneSelectionFromNicknames(m_selectedNicknames);
    updateSelectionSummary();
    updateIniEditorForSelection();
    updateSidebarButtons();
}

void SystemEditorPage::pruneSelectionByCurrentFilter()
{
    if (!m_document)
        return;

    QStringList filtered;
    filtered.reserve(m_selectedNicknames.size());
    for (const QString &nickname : m_selectedNicknames) {
        if (isNicknameVisibleUnderCurrentFilter(nickname))
            filtered.append(nickname);
    }
    filtered.removeDuplicates();
    m_selectedNicknames = filtered;
}

bool SystemEditorPage::isNicknameVisibleUnderCurrentFilter(const QString &nickname) const
{
    if (!m_document)
        return false;

    for (const auto &obj : m_document->objects()) {
        if (obj->nickname() == nickname)
            return isObjectVisibleUnderCurrentFilter(*obj);
    }
    for (const auto &zone : m_document->zones()) {
        if (zone->nickname() == nickname)
            return isZoneVisibleUnderCurrentFilter(*zone);
    }
    return false;
}

bool SystemEditorPage::isObjectVisibleUnderCurrentFilter(const SolarObject &obj) const
{
    SolarObjectDisplayContext context;
    context.nickname = obj.nickname();
    context.archetype = obj.archetype();
    context.type = obj.type();

    bool visible = m_displayFilterSettings.objectVisibleForType(obj.type());
    for (const SystemDisplayFilterRule &rule : m_displayFilterSettings.rules) {
        if (!matchesDisplayFilterRule(rule, context))
            continue;
        if (rule.target == DisplayFilterTarget::Label)
            continue;
        visible = (rule.action == DisplayFilterAction::Show);
    }
    return visible;
}

bool SystemEditorPage::isZoneVisibleUnderCurrentFilter(const ZoneItem &zone) const
{
    SolarObjectDisplayContext context;
    context.nickname = zone.nickname();
    context.typeNameOverride = QStringLiteral("Zone");

    bool visible = true;
    for (const SystemDisplayFilterRule &rule : m_displayFilterSettings.rules) {
        if (!matchesDisplayFilterRule(rule, context))
            continue;
        if (rule.target == DisplayFilterTarget::Label)
            continue;
        visible = (rule.action == DisplayFilterAction::Show);
    }
    return visible;
}

void SystemEditorPage::applyObjectListSearchFilter()
{
    if (!m_objectTree)
        return;

    const QString needle = m_objectSearchEdit ? m_objectSearchEdit->text().trimmed() : QString();
    const bool hasSearch = !needle.isEmpty();
    int totalVisibleChildren = 0;
    const QColor hitBackground(57, 104, 168, 70);

    for (int i = 0; i < m_objectTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *root = m_objectTree->topLevelItem(i);
        if (!root)
            continue;

        int visibleChildren = 0;
        for (int row = 0; row < root->childCount(); ++row) {
            QTreeWidgetItem *child = root->child(row);
            if (!child)
                continue;

            const bool matches = !hasSearch || child->text(0).contains(needle, Qt::CaseInsensitive);
            child->setHidden(!matches);
            for (int column = 0; column < child->columnCount(); ++column)
                child->setBackground(column, matches && hasSearch ? QBrush(hitBackground) : QBrush());
            if (matches) {
                ++visibleChildren;
                ++totalVisibleChildren;
            }
        }

        root->setHidden(hasSearch && visibleChildren == 0);
        if (!root->isHidden())
            root->setExpanded(true);
    }

    if (m_objectSearchHintLabel)
        m_objectSearchHintLabel->setVisible(hasSearch && totalVisibleChildren == 0);
}

void SystemEditorPage::refreshSidebarVisibilityState()
{
    if (!m_objectTree || !m_document)
        return;

    const QList<QTreeWidgetItem *> matches = m_objectTree->findItems(QStringLiteral("*"),
                                                                     Qt::MatchWildcard | Qt::MatchRecursive,
                                                                     0);
    for (QTreeWidgetItem *item : matches) {
        if (!item->parent())
            continue;

        const QString nickname = item->text(0);
        bool visible = true;
        bool found = false;

        for (const auto &obj : m_document->objects()) {
            if (obj->nickname() == nickname) {
                visible = isObjectVisibleUnderCurrentFilter(*obj);
                found = true;
                break;
            }
        }

        if (!found) {
            for (const auto &zone : m_document->zones()) {
                if (zone->nickname() == nickname) {
                    visible = isZoneVisibleUnderCurrentFilter(*zone);
                    found = true;
                    break;
                }
            }
        }

        applySidebarFilteredStyle(item, visible);
    }
}

void SystemEditorPage::applySidebarFilteredStyle(QTreeWidgetItem *item, bool visible)
{
    if (!item)
        return;

    const QPalette palette = this->palette();
    const QColor normalText = palette.color(QPalette::Active, QPalette::Text);
    const QColor mutedText = QColor(120, 128, 140);

    for (int column = 0; column < item->columnCount(); ++column) {
        item->setForeground(column, visible ? QBrush(normalText) : QBrush(mutedText));
        QFont font = item->font(column);
        font.setItalic(!visible);
        item->setFont(column, font);
    }

    item->setData(0, Qt::UserRole + 1, !visible);
    const QString tooltip = visible
        ? QString()
        : tr("Durch den Sichtbarkeitsfilter aktuell ausgeblendet");
    item->setToolTip(0, tooltip);
    item->setToolTip(1, tooltip);
}

void SystemEditorPage::applyIniEditorChanges()
{
    if (!m_document || !m_iniEditor)
        return;

    const QString text = m_iniEditor->toPlainText().trimmed();
    if (text.isEmpty()) {
        QMessageBox::warning(this, tr("Leerer Inhalt"),
                             tr("Der Objekt-Editor enthält keine INI-Section."));
        return;
    }

    const IniDocument parsed = IniParser::parseText(text);
    if (parsed.size() != 1) {
        QMessageBox::warning(this, tr("Ungültige Section"),
                             tr("Bitte genau eine [Object]- oder [Zone]-Section einfügen."));
        return;
    }

    const IniSection &section = parsed.first();
    const QString sectionName = section.name.trimmed().toLower();
    if (m_selectedNicknames.size() != 1) {
        QMessageBox::information(this, tr("Mehrfachauswahl"),
                                 tr("Der Objekt-Editor kann nur genau einen Eintrag gleichzeitig bearbeiten."));
        return;
    }

    const QString previousNickname = m_selectedNicknames.first();

    if (sectionName == QStringLiteral("object")) {
        for (const auto &obj : m_document->objects()) {
            if (obj->nickname() == previousNickname) {
                IniDocument beforeDoc;
                beforeDoc.append(SystemPersistence::serializeObjectSection(*obj));
                const QString beforeText = IniParser::serialize(beforeDoc).trimmed();
                SystemPersistence::applyObjectSection(*obj, section);
                IniDocument afterDoc;
                afterDoc.append(SystemPersistence::serializeObjectSection(*obj));
                const QString afterText = IniParser::serialize(afterDoc).trimmed();
                if (beforeText != afterText)
                    m_document->setDirty(true);
                m_selectedNicknames = {obj->nickname()};
                refreshObjectList();
                onCanvasSelectionChanged(m_selectedNicknames);
                return;
            }
        }
    }

    if (sectionName == QStringLiteral("zone")) {
        for (const auto &zone : m_document->zones()) {
            if (zone->nickname() == previousNickname) {
                IniDocument beforeDoc;
                beforeDoc.append(SystemPersistence::serializeZoneSection(*zone));
                const QString beforeText = IniParser::serialize(beforeDoc).trimmed();
                SystemPersistence::applyZoneSection(*zone, section);
                IniDocument afterDoc;
                afterDoc.append(SystemPersistence::serializeZoneSection(*zone));
                const QString afterText = IniParser::serialize(afterDoc).trimmed();
                if (beforeText != afterText)
                    m_document->setDirty(true);
                m_selectedNicknames = {zone->nickname()};
                refreshObjectList();
                onCanvasSelectionChanged(m_selectedNicknames);
                return;
            }
        }
    }

    QMessageBox::warning(this, tr("Section passt nicht"),
                         tr("Die Section passt nicht zum aktuell ausgewählten Eintrag."));
}

void SystemEditorPage::openSystemIniExternally() const
{
    if (!m_document || m_document->filePath().trimmed().isEmpty())
        return;

    QDesktopServices::openUrl(QUrl::fromLocalFile(m_document->filePath()));
}

void SystemEditorPage::updateSelectionSummary()
{
    if (m_isShuttingDown)
        return;
    if (!m_selectionTitleLabel || !m_selectionSubtitleLabel || !m_document) {
        if (m_selectionTitleLabel)
            m_selectionTitleLabel->setText(tr("Kein Objekt ausgewählt"));
        if (m_selectionSubtitleLabel)
            m_selectionSubtitleLabel->setText(tr("Wähle ein Objekt oder eine Zone in der Karte oder Liste aus."));
        emit selectionStatusChanged(tr("0 Objekte markiert"));
        return;
    }

    if (m_selectedNicknames.isEmpty()) {
        m_selectionTitleLabel->setText(tr("Kein Objekt ausgewählt"));
        m_selectionSubtitleLabel->setText(tr("Wähle ein Objekt oder eine Zone in der Karte oder Liste aus."));
        emit selectionStatusChanged(tr("0 Objekte markiert"));
        return;
    }

    if (m_selectedNicknames.size() > 1) {
        m_selectionTitleLabel->setText(tr("%1 Objekte markiert").arg(m_selectedNicknames.size()));
        m_selectionSubtitleLabel->setText(tr("Mehrfachauswahl aktiv. Bearbeiten und Löschen wirken auf die aktuelle Auswahl."));
        emit selectionStatusChanged(tr("%1 Objekte markiert").arg(m_selectedNicknames.size()));
        return;
    }

    const QString nickname = m_selectedNicknames.first();
    for (const auto &obj : m_document->objects()) {
        if (obj->nickname() == nickname) {
            m_selectionTitleLabel->setText(obj->nickname());
            m_selectionSubtitleLabel->setText(tr("Objekt · %1")
                                                  .arg(QString::fromLatin1(QMetaEnum::fromType<SolarObject::Type>().valueToKey(obj->type()))));
            emit selectionStatusChanged(tr("1 Objekt markiert"));
            return;
        }
    }

    for (const auto &zone : m_document->zones()) {
        if (zone->nickname() == nickname) {
            m_selectionTitleLabel->setText(zone->nickname());
            const QString zoneType = zone->zoneType().trimmed().isEmpty() ? tr("Zone") : zone->zoneType().trimmed();
            m_selectionSubtitleLabel->setText(tr("Zone · %1").arg(zoneType));
            emit selectionStatusChanged(tr("1 Objekt markiert"));
            return;
        }
    }

    m_selectionTitleLabel->setText(tr("Kein Objekt ausgewählt"));
    m_selectionSubtitleLabel->setText(tr("Wähle ein Objekt oder eine Zone in der Karte oder Liste aus."));
    emit selectionStatusChanged(tr("0 Objekte markiert"));
}

void SystemEditorPage::updateSidebarButtons()
{
    if (m_isShuttingDown)
        return;
    const QString selectedNickname = primarySelectedNickname();
    const bool hasSingleSelection = m_selectedNicknames.size() == 1;
    bool isObject = false;
    bool isZone = false;
    SolarObject::Type objectType = SolarObject::Other;

    if (m_document && hasSingleSelection) {
        for (const auto &obj : m_document->objects()) {
            if (obj->nickname() == selectedNickname) {
                isObject = true;
                objectType = obj->type();
                break;
            }
        }
        if (!isObject) {
            for (const auto &zone : m_document->zones()) {
                if (zone->nickname() == selectedNickname) {
                    isZone = true;
                    break;
                }
            }
        }
    }

    if (m_editTradelaneButton)
        m_editTradelaneButton->setEnabled(hasSingleSelection && isObject && objectType == SolarObject::TradeLane);
    if (m_deleteSidebarButton)
        m_deleteSidebarButton->setEnabled(!m_selectedNicknames.isEmpty());
    if (m_editZonePopulationButton)
        m_editZonePopulationButton->setEnabled(hasSingleSelection && isZone);
    if (m_editRingButton)
        m_editRingButton->setEnabled(hasSingleSelection && isObject && (objectType == SolarObject::Planet || objectType == SolarObject::DockingRing));
    if (m_addExclusionZoneButton)
        m_addExclusionZoneButton->setEnabled(hasSingleSelection && isZone);
    if (m_editBaseButton)
        m_editBaseButton->setEnabled(hasSingleSelection && isObject && objectType == SolarObject::Station);
    if (m_baseBuilderButton)
        m_baseBuilderButton->setEnabled(hasSingleSelection && isObject && objectType == SolarObject::Station);
    if (m_saveFileButton)
        m_saveFileButton->setEnabled(m_document != nullptr);
    if (m_objectJumpButton)
        m_objectJumpButton->setEnabled(m_objectJumpCombo && m_objectJumpCombo->count() > 0);
}

void SystemEditorPage::refreshObjectJumpList()
{
    if (!m_objectJumpCombo) {
        return;
    }

    m_objectJumpCombo->clear();
    if (!m_document)
        return;

    for (const auto &obj : m_document->objects())
        m_objectJumpCombo->addItem(obj->nickname());
    for (const auto &zone : m_document->zones())
        m_objectJumpCombo->addItem(zone->nickname());
}

void SystemEditorPage::jumpToSelectedFromSidebar()
{
    if (!m_mapScene || !m_mapView || !m_objectJumpCombo)
        return;

    const QString nickname = m_objectJumpCombo->currentText().trimmed();
    if (nickname.isEmpty())
        return;
    syncSceneSelectionFromNicknames({nickname});
    for (QGraphicsItem *item : m_mapScene->items()) {
        if (auto *solarItem = dynamic_cast<flatlas::rendering::SolarObjectItem *>(item); solarItem && solarItem->nickname() == nickname) {
            m_mapView->centerOn(solarItem);
            return;
        }
        if (auto *zoneItem = dynamic_cast<flatlas::rendering::ZoneItem2D *>(item); zoneItem && zoneItem->nickname() == nickname) {
            m_mapView->centerOn(zoneItem);
            return;
        }
    }
}

void SystemEditorPage::onItemsMoved(const QHash<QString, QPointF> &oldPositions,
                                    const QHash<QString, QPointF> &newPositions)
{
    if (!m_document || oldPositions.isEmpty() || newPositions.isEmpty())
        return;

    auto *stack = flatlas::core::UndoManager::instance().stack();
    stack->beginMacro(tr("Move Selection"));
    bool movedAnything = false;

    for (const auto &obj : m_document->objects()) {
        if (!oldPositions.contains(obj->nickname()) || !newPositions.contains(obj->nickname()))
            continue;
        const QPointF oldFl = MapScene::qtToFl(oldPositions.value(obj->nickname()).x(), oldPositions.value(obj->nickname()).y());
        const QPointF newFl = MapScene::qtToFl(newPositions.value(obj->nickname()).x(), newPositions.value(obj->nickname()).y());
        if (qFuzzyCompare(oldFl.x() + 1.0, newFl.x() + 1.0)
            && qFuzzyCompare(oldFl.y() + 1.0, newFl.y() + 1.0))
            continue;
        const QVector3D oldPos(oldFl.x(), obj->position().y(), oldFl.y());
        const QVector3D newPos(newFl.x(), obj->position().y(), newFl.y());
        stack->push(new MoveObjectCommand(obj.get(), oldPos, newPos));
        movedAnything = true;
    }

    for (const auto &zone : m_document->zones()) {
        if (!oldPositions.contains(zone->nickname()) || !newPositions.contains(zone->nickname()))
            continue;
        const QPointF oldFl = MapScene::qtToFl(oldPositions.value(zone->nickname()).x(), oldPositions.value(zone->nickname()).y());
        const QPointF newFl = MapScene::qtToFl(newPositions.value(zone->nickname()).x(), newPositions.value(zone->nickname()).y());
        if (qFuzzyCompare(oldFl.x() + 1.0, newFl.x() + 1.0)
            && qFuzzyCompare(oldFl.y() + 1.0, newFl.y() + 1.0))
            continue;
        const QVector3D oldPos(oldFl.x(), zone->position().y(), oldFl.y());
        const QVector3D newPos(newFl.x(), zone->position().y(), newFl.y());
        stack->push(new MoveZoneCommand(zone.get(), oldPos, newPos));
        movedAnything = true;
    }

    stack->endMacro();
    if (movedAnything)
        m_document->setDirty(true);
}

void SystemEditorPage::syncTreeSelectionFromNicknames(const QStringList &nicknames)
{
    if (!m_objectTree)
        return;

    const QSet<QString> selectedSet(nicknames.begin(), nicknames.end());
    QSignalBlocker blocker(m_objectTree);
    m_objectTree->clearSelection();

    QTreeWidgetItem *firstMatch = nullptr;
    const QList<QTreeWidgetItem *> matches = m_objectTree->findItems(QStringLiteral("*"),
                                                                     Qt::MatchWildcard | Qt::MatchRecursive,
                                                                     0);
    for (QTreeWidgetItem *item : matches) {
        if (!item->parent())
            continue;
        const bool selected = selectedSet.contains(item->text(0));
        item->setSelected(selected);
        if (selected && !firstMatch)
            firstMatch = item;
    }
    m_objectTree->setCurrentItem(firstMatch);
}

void SystemEditorPage::syncSceneSelectionFromNicknames(const QStringList &nicknames)
{
    if (m_mapScene)
        m_mapScene->selectNicknames(nicknames);
}

QString SystemEditorPage::primarySelectedNickname() const
{
    return m_selectedNicknames.isEmpty() ? QString() : m_selectedNicknames.first();
}

void SystemEditorPage::createQuickObject(SolarObject::Type type,
                                         const QString &suggestedNickname,
                                         const QString &defaultArchetype)
{
    if (!m_document)
        return;

    bool ok = false;
    const QString nickname = QInputDialog::getText(this,
                                                   tr("Objekt erstellen"),
                                                   tr("Nickname:"),
                                                   QLineEdit::Normal,
                                                   suggestedNickname,
                                                   &ok).trimmed();
    if (!ok || nickname.isEmpty())
        return;

    auto obj = std::make_shared<SolarObject>();
    obj->setNickname(nickname);
    obj->setType(type);
    obj->setArchetype(defaultArchetype);

    auto *cmd = new AddObjectCommand(m_document.get(), obj, tr("Create Object"));
    flatlas::core::UndoManager::instance().push(cmd);
    refreshObjectList();
}

void SystemEditorPage::showNotYetPorted(const QString &featureName, const QString &v1Hint) const
{
    QString text = tr("%1 ist in V2 noch nicht portiert.").arg(featureName);
    if (!v1Hint.trimmed().isEmpty())
        text += tr("\n\nV1-Referenz: %1").arg(v1Hint);
    QMessageBox::information(const_cast<SystemEditorPage *>(this), tr("Noch nicht portiert"), text);
}

void SystemEditorPage::onCreateSun()
{
    createQuickObject(SolarObject::Sun, QStringLiteral("new_sun"), QStringLiteral("sun_1000"));
}

void SystemEditorPage::onCreatePlanet()
{
    createQuickObject(SolarObject::Planet, QStringLiteral("new_planet"), QStringLiteral("planet_earthgrncld_3000"));
}

void SystemEditorPage::onCreateBuoy()
{
    createQuickObject(SolarObject::Waypoint, QStringLiteral("new_buoy"), QStringLiteral("space_arch_buoy"));
}

void SystemEditorPage::onCreateWeaponPlatform()
{
    createQuickObject(SolarObject::Weapons_Platform, QStringLiteral("new_platform"), QStringLiteral("weapons_platform"));
}

void SystemEditorPage::onCreateDepot()
{
    createQuickObject(SolarObject::Depot, QStringLiteral("new_depot"), QStringLiteral("depot"));
}

void SystemEditorPage::onCreateTradeLane()
{
    createQuickObject(SolarObject::TradeLane, QStringLiteral("new_tradelane"), QStringLiteral("trade_lane_ring"));
}

void SystemEditorPage::onCreateBase()
{
    createQuickObject(SolarObject::Station, QStringLiteral("new_base"), QStringLiteral("station"));
}

void SystemEditorPage::onCreateDockingRing()
{
    createQuickObject(SolarObject::DockingRing, QStringLiteral("new_docking_ring"), QStringLiteral("docking_ring"));
}

} // namespace flatlas::editors
