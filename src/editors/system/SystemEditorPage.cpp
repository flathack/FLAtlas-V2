// editors/system/SystemEditorPage.cpp – System-Editor (Phase 5)

#include "SystemEditorPage.h"
#include "SystemDisplayFilterDialog.h"
#include "SystemPersistence.h"
#include "SystemUndoCommands.h"

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
#include <QSplitter>
#include <QToolBar>
#include <QTreeWidget>
#include <QAction>
#include <QMessageBox>
#include <QInputDialog>
#include <QMetaEnum>
#include <QPixmap>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QGroupBox>
#include <QFrame>
#include <QFileInfo>

using namespace flatlas::domain;
using namespace flatlas::rendering;

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

}

SystemEditorPage::SystemEditorPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

SystemEditorPage::~SystemEditorPage()
{
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

    // Object tree (left)
    m_objectTree = new QTreeWidget(this);
    m_objectTree->setHeaderLabels({tr("Nickname"), tr("Type")});
    m_objectTree->setMinimumWidth(200);
    m_splitter->addWidget(m_objectTree);

    // Map view (center)
    m_mapScene = new MapScene(this);
    m_mapView = new SystemMapView(this);
    m_mapView->setMapScene(m_mapScene);
    m_mapView->setBackgroundPixmap(QPixmap(QStringLiteral(":/images/star-background.png")),
                                   QColor(15, 18, 24));
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
    connect(m_mapScene, &MapScene::objectSelected, this, &SystemEditorPage::onObjectSelected);

    // 3D → 2D selection sync

    connect(m_objectTree, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem *current, QTreeWidgetItem *) {
        if (!current || !m_document)
            return;
        const QString nickname = current->text(0);
        onObjectSelected(nickname);
    });
}

bool SystemEditorPage::loadFile(const QString &filePath)
{
    auto doc = SystemPersistence::load(filePath);
    if (!doc)
        return false;

    m_document = std::move(doc);
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
    emit titleChanged(m_document->name());
    return true;
}

void SystemEditorPage::setDocument(std::unique_ptr<SystemDocument> doc)
{
    if (m_document)
        SystemPersistence::clearExtras(m_document.get());
    m_document = std::move(doc);
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
    emit titleChanged(m_document->name());
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

void SystemEditorPage::ensureSceneView3D()
{
    if (m_sceneView3D)
        return;

    m_sceneView3D = new SceneView3D(m_rightSidebar);

    connect(m_sceneView3D, &SceneView3D::objectSelected, this, &SystemEditorPage::onObjectSelected);
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
}

void SystemEditorPage::loadDisplayFilterSettings()
{
    m_displayFilterSettings = flatlas::rendering::SystemDisplayFilterSettings::fromJson(
        flatlas::core::Config::instance().getJsonObject(displayFilterConfigKey()));
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
    m_objectTree->clear();
    if (!m_document)
        return;

    auto *objRoot = new QTreeWidgetItem(m_objectTree, {tr("Objects")});
    objRoot->setExpanded(true);
    for (const auto &obj : m_document->objects()) {
        auto *item = new QTreeWidgetItem(objRoot);
        item->setText(0, obj->nickname());
        item->setText(1, QMetaEnum::fromType<SolarObject::Type>().valueToKey(obj->type()));
    }

    auto *zoneRoot = new QTreeWidgetItem(m_objectTree, {tr("Zones")});
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
    updateSidebarButtons();
}

void SystemEditorPage::onObjectSelected(const QString &nickname)
{
    if (!m_document)
        return;

    // Sync 3D selection
    if (m_sceneView3D && m_is3DViewEnabled)
        m_sceneView3D->selectObject(nickname);

    for (const auto &obj : m_document->objects()) {
        if (obj->nickname() == nickname) {
            updateSelectionSummary(nickname);
            if (m_objectJumpCombo)
                m_objectJumpCombo->setCurrentText(nickname);
            updateSidebarButtons();
            showObjectProperties(obj.get());
            return;
        }
    }
    for (const auto &zone : m_document->zones()) {
        if (zone->nickname() == nickname) {
            updateSelectionSummary(nickname);
            if (m_objectJumpCombo)
                m_objectJumpCombo->setCurrentText(nickname);
            updateSidebarButtons();
            showZoneProperties(zone.get());
            return;
        }
    }

    updateSelectionSummary();
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
    if (!m_document || !m_objectTree->currentItem())
        return;

    auto *item = m_objectTree->currentItem();
    if (!item->parent())
        return; // root node

    const QString nickname = item->text(0);

    // Try objects first
    for (const auto &obj : m_document->objects()) {
        if (obj->nickname() == nickname) {
            auto *cmd = new RemoveObjectCommand(m_document.get(), obj);
            flatlas::core::UndoManager::instance().push(cmd);
            refreshObjectList();
            return;
        }
    }
    // Try zones
    for (const auto &zone : m_document->zones()) {
        if (zone->nickname() == nickname) {
            auto *cmd = new RemoveZoneCommand(m_document.get(), zone);
            flatlas::core::UndoManager::instance().push(cmd);
            refreshObjectList();
            return;
        }
    }
}

void SystemEditorPage::onDuplicateSelected()
{
    if (!m_document || !m_objectTree->currentItem())
        return;

    auto *item = m_objectTree->currentItem();
    if (!item->parent())
        return;

    const QString nickname = item->text(0);

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

void SystemEditorPage::updateSelectionSummary(const QString &nickname)
{
    if (!m_selectionTitleLabel || !m_selectionSubtitleLabel || !m_document) {
        if (m_selectionTitleLabel)
            m_selectionTitleLabel->setText(tr("Kein Objekt ausgewählt"));
        if (m_selectionSubtitleLabel)
            m_selectionSubtitleLabel->setText(tr("Wähle ein Objekt oder eine Zone in der Karte oder Liste aus."));
        return;
    }

    if (nickname.trimmed().isEmpty()) {
        m_selectionTitleLabel->setText(tr("Kein Objekt ausgewählt"));
        m_selectionSubtitleLabel->setText(tr("Wähle ein Objekt oder eine Zone in der Karte oder Liste aus."));
        return;
    }

    for (const auto &obj : m_document->objects()) {
        if (obj->nickname() == nickname) {
            m_selectionTitleLabel->setText(obj->nickname());
            m_selectionSubtitleLabel->setText(tr("Objekt · %1")
                                                  .arg(QString::fromLatin1(QMetaEnum::fromType<SolarObject::Type>().valueToKey(obj->type()))));
            return;
        }
    }

    for (const auto &zone : m_document->zones()) {
        if (zone->nickname() == nickname) {
            m_selectionTitleLabel->setText(zone->nickname());
            const QString zoneType = zone->zoneType().trimmed().isEmpty() ? tr("Zone") : zone->zoneType().trimmed();
            m_selectionSubtitleLabel->setText(tr("Zone · %1").arg(zoneType));
            return;
        }
    }

    m_selectionTitleLabel->setText(tr("Kein Objekt ausgewählt"));
    m_selectionSubtitleLabel->setText(tr("Wähle ein Objekt oder eine Zone in der Karte oder Liste aus."));
}

void SystemEditorPage::updateSidebarButtons()
{
    const QString selectedNickname = m_objectTree && m_objectTree->currentItem() ? m_objectTree->currentItem()->text(0) : QString();
    bool isObject = false;
    bool isZone = false;
    SolarObject::Type objectType = SolarObject::Other;

    if (m_document) {
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
        m_editTradelaneButton->setEnabled(isObject && objectType == SolarObject::TradeLane);
    if (m_deleteSidebarButton)
        m_deleteSidebarButton->setEnabled(isObject || isZone);
    if (m_editZonePopulationButton)
        m_editZonePopulationButton->setEnabled(isZone);
    if (m_editRingButton)
        m_editRingButton->setEnabled(isObject && (objectType == SolarObject::Planet || objectType == SolarObject::DockingRing));
    if (m_addExclusionZoneButton)
        m_addExclusionZoneButton->setEnabled(isZone);
    if (m_editBaseButton)
        m_editBaseButton->setEnabled(isObject && objectType == SolarObject::Station);
    if (m_baseBuilderButton)
        m_baseBuilderButton->setEnabled(isObject && objectType == SolarObject::Station);
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

    for (QGraphicsItem *item : m_mapScene->items()) {
        if (auto *solarItem = dynamic_cast<flatlas::rendering::SolarObjectItem *>(item)) {
            if (solarItem->nickname() == nickname) {
                m_mapView->centerOn(solarItem);
                solarItem->setSelected(true);
                onObjectSelected(nickname);
                return;
            }
        }
        if (auto *zoneItem = dynamic_cast<flatlas::rendering::ZoneItem2D *>(item)) {
            if (zoneItem->nickname() == nickname) {
                m_mapView->centerOn(zoneItem);
                zoneItem->setSelected(true);
                onObjectSelected(nickname);
                return;
            }
        }
    }
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
