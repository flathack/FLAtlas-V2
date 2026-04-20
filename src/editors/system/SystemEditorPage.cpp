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
#include "rendering/view3d/SceneView3D.h"
#include "core/UndoManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QToolBar>
#include <QTreeWidget>
#include <QAction>
#include <QMessageBox>
#include <QInputDialog>
#include <QMetaEnum>
#include <QPixmap>
#include <QLabel>

using namespace flatlas::domain;
using namespace flatlas::rendering;

namespace flatlas::editors {

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

    // 3D view host (right). The heavy Qt3D view is created lazily for stability.
    m_sceneView3DHost = new QWidget(this);
    auto *sceneHostLayout = new QVBoxLayout(m_sceneView3DHost);
    sceneHostLayout->setContentsMargins(0, 0, 0, 0);
    sceneHostLayout->setSpacing(0);
    m_sceneView3DPlaceholder = new QLabel(
        tr("3D View disabled\n\nEnable \"3D View\" in the toolbar to load the scene."),
        m_sceneView3DHost);
    m_sceneView3DPlaceholder->setAlignment(Qt::AlignCenter);
    sceneHostLayout->addWidget(m_sceneView3DPlaceholder);
    m_splitter->addWidget(m_sceneView3DHost);
    m_sceneView3DHost->hide();

    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setStretchFactor(2, 1);
    m_splitter->setSizes({220, 1000, 0});

    mainLayout->addWidget(m_splitter);

    connectSignals();
}

void SystemEditorPage::setupToolBar()
{
    m_toolBar = new QToolBar(this);
    m_toolBar->setIconSize(QSize(16, 16));

    auto *addObjectAction = m_toolBar->addAction(tr("+ Object"));
    connect(addObjectAction, &QAction::triggered, this, &SystemEditorPage::onAddObject);

    auto *displayFiltersAction = m_toolBar->addAction(tr("Display Filters"));
    connect(displayFiltersAction, &QAction::triggered, this, &SystemEditorPage::openDisplayFilterDialog);

    auto *deleteAction = m_toolBar->addAction(tr("Delete"));
    connect(deleteAction, &QAction::triggered, this, &SystemEditorPage::onDeleteSelected);

    auto *duplicateAction = m_toolBar->addAction(tr("Duplicate"));
    connect(duplicateAction, &QAction::triggered, this, &SystemEditorPage::onDuplicateSelected);

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

    m_sceneView3D = new SceneView3D(m_sceneView3DHost);
    if (auto *layout = qobject_cast<QVBoxLayout *>(m_sceneView3DHost->layout()))
        layout->addWidget(m_sceneView3D);

    if (m_sceneView3DPlaceholder)
        m_sceneView3DPlaceholder->hide();

    connect(m_sceneView3D, &SceneView3D::objectSelected, this, &SystemEditorPage::onObjectSelected);
}

void SystemEditorPage::set3DViewEnabled(bool enabled)
{
    m_is3DViewEnabled = enabled;

    if (!enabled) {
        if (m_sceneView3D)
            m_sceneView3D->hide();
        if (m_sceneView3DPlaceholder)
            m_sceneView3DPlaceholder->show();
        return;
    }

    ensureSceneView3D();
    m_sceneView3D->show();
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
            showObjectProperties(obj.get());
            return;
        }
    }
    for (const auto &zone : m_document->zones()) {
        if (zone->nickname() == nickname) {
            showZoneProperties(zone.get());
            return;
        }
    }
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

} // namespace flatlas::editors
