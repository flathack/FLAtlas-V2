// rendering/view3d/SceneView3D.cpp - practical 3D system editor viewport

#include "SceneView3D.h"

#include "domain/SolarObject.h"
#include "domain/SystemDocument.h"
#include "domain/ZoneItem.h"

#include <QLabel>
#include <QVBoxLayout>

#ifdef FLATLAS_HAS_QT3D
#include "MaterialFactory.h"
#include "ModelGeometryBuilder.h"
#include "OrbitCamera.h"
#include "SelectionManager.h"
#include "SkyRenderer.h"
#include "ZoneColorScheme.h"
#include "ZoneGeometryBuilder.h"
#include "rendering/preview/ModelCache.h"

#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QCuboidMesh>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QPointLight>
#include <Qt3DRender/QRenderSettings>

#include <QEvent>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QtConcurrent/QtConcurrent>

#include <limits>
#endif

namespace flatlas::rendering {

#ifdef FLATLAS_HAS_QT3D
namespace {

QColor objectColor(flatlas::domain::SolarObject::Type type)
{
    using Type = flatlas::domain::SolarObject::Type;
    switch (type) {
    case Type::Sun:
        return QColor(255, 205, 70);
    case Type::Planet:
        return QColor(80, 135, 210);
    case Type::Station:
    case Type::DockingRing:
    case Type::Depot:
        return QColor(185, 190, 200);
    case Type::JumpGate:
    case Type::JumpHole:
        return QColor(80, 220, 170);
    case Type::TradeLane:
        return QColor(105, 135, 235);
    case Type::Weapons_Platform:
        return QColor(220, 145, 85);
    case Type::Wreck:
        return QColor(145, 125, 105);
    default:
        return QColor(165, 175, 185);
    }
}

float markerRadius(flatlas::domain::SolarObject::Type type)
{
    using Type = flatlas::domain::SolarObject::Type;
    switch (type) {
    case Type::Sun:
        return 1200.0f;
    case Type::Planet:
        return 650.0f;
    case Type::JumpGate:
    case Type::JumpHole:
        return 420.0f;
    case Type::TradeLane:
        return 260.0f;
    default:
        return 260.0f;
    }
}

QColor colorForMesh(const flatlas::infrastructure::MeshData &mesh, int nodeIndex)
{
    if (!mesh.materialValue.isEmpty()) {
        const uint hash = qHash(mesh.materialValue.toLower());
        return QColor::fromHsv(static_cast<int>(hash % 360u), 105, 190);
    }
    if (!mesh.materialName.isEmpty()) {
        const uint hash = qHash(mesh.materialName.toLower());
        return QColor::fromHsv(static_cast<int>(hash % 360u), 95, 205);
    }
    static const int hues[] = {210, 30, 120, 0, 270, 60, 190, 340, 90, 240, 155, 315};
    return QColor::fromHsv(hues[nodeIndex % 12], 95, 205);
}

bool objectVisibleForFilter(const SystemDisplayFilterSettings &settings,
                            const flatlas::domain::SolarObject &obj)
{
    SolarObjectDisplayContext context;
    context.nickname = obj.nickname();
    context.archetype = obj.archetype();
    context.type = obj.type();

    bool visible = settings.objectVisibleForType(obj.type());
    for (const SystemDisplayFilterRule &rule : settings.rules) {
        if (!matchesDisplayFilterRule(rule, context))
            continue;
        if (rule.target == DisplayFilterTarget::Label)
            continue;
        visible = (rule.action == DisplayFilterAction::Show);
    }
    return visible;
}

bool zoneVisibleForFilter(const SystemDisplayFilterSettings &settings,
                          const flatlas::domain::ZoneItem &zone)
{
    SolarObjectDisplayContext context;
    context.nickname = zone.nickname();
    context.typeNameOverride = QStringLiteral("Zone");

    bool visible = true;
    for (const SystemDisplayFilterRule &rule : settings.rules) {
        if (!matchesDisplayFilterRule(rule, context))
            continue;
        if (rule.target == DisplayFilterTarget::Label)
            continue;
        visible = (rule.action == DisplayFilterAction::Show);
    }
    return visible;
}

} // namespace
#endif

SceneView3D::SceneView3D(QWidget *parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(320, 240);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

#ifdef FLATLAS_HAS_QT3D
    m_3dWindow = new Qt3DExtras::Qt3DWindow();
    m_container = QWidget::createWindowContainer(m_3dWindow, this);
    m_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_container->setMinimumSize(320, 240);
    m_container->setFocusPolicy(Qt::StrongFocus);
    m_container->setMouseTracking(true);
    m_container->setToolTip(tr("Left drag rotates, right drag pans, mouse wheel zooms."));
    m_container->installEventFilter(this);
    m_3dWindow->installEventFilter(this);
    layout->addWidget(m_container, 1);

    m_sceneBounds = new ModelBounds();
    m_objectBounds = new ModelBounds();
    m_zoneBounds = new ModelBounds();
    setupScene();
#else
    auto *placeholder = new QLabel(tr("3D View - Qt3D not available"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    layout->addWidget(placeholder, 1);
#endif
}

SceneView3D::~SceneView3D()
{
#ifdef FLATLAS_HAS_QT3D
    delete m_sceneBounds;
    delete m_objectBounds;
    delete m_zoneBounds;
#endif
}

void SceneView3D::setArchetypeModelPaths(const QHash<QString, QString> &modelPaths)
{
    m_archetypeModelPaths = modelPaths;
}

void SceneView3D::setDisplayFilterSettings(const SystemDisplayFilterSettings &settings)
{
    m_displayFilterSettings = settings;
    applyDisplayFilter();
}

void SceneView3D::loadDocument(flatlas::domain::SystemDocument *doc)
{
    m_document = doc;
    clearScene();

    if (!doc)
        return;

#ifdef FLATLAS_HAS_QT3D
    for (const auto &zone : doc->zones())
        addZone(zone);
    for (const auto &obj : doc->objects())
        addSolarObject(obj);

    applyDisplayFilter();
    updateSceneCamera();
    scheduleModelLoading();
#endif
}

void SceneView3D::selectObject(const QString &nickname)
{
#ifdef FLATLAS_HAS_QT3D
    if (m_selectionManager)
        m_selectionManager->select(nickname);
#else
    Q_UNUSED(nickname);
#endif
}

void SceneView3D::setupScene()
{
#ifdef FLATLAS_HAS_QT3D
    m_rootEntity = new Qt3DCore::QEntity();
    m_sceneRoot = new Qt3DCore::QEntity(m_rootEntity);
    m_zonesRoot = new Qt3DCore::QEntity(m_sceneRoot);
    m_objectsRoot = new Qt3DCore::QEntity(m_sceneRoot);

    m_camera = m_3dWindow->camera();
    m_camera->lens()->setPerspectiveProjection(45.0f, 16.0f / 9.0f, 5.0f, 6000000.0f);

    m_orbitCamera = new OrbitCamera(m_camera, this);
    m_orbitCamera->setRotateButton(Qt::LeftButton);
    m_orbitCamera->setPanButton(Qt::RightButton);
    m_orbitCamera->setResetState(QVector3D(0.0f, 0.0f, 0.0f), 80000.0f, 45.0f, 24.0f);
    m_orbitCamera->resetView();

    auto *renderer = m_3dWindow->defaultFrameGraph();
    renderer->setClearColor(QColor(6, 10, 18));
    renderer->setFrustumCullingEnabled(true);

    auto *lightEntity = new Qt3DCore::QEntity(m_rootEntity);
    m_light = new Qt3DRender::QPointLight(lightEntity);
    m_light->setColor(Qt::white);
    m_light->setIntensity(1.6f);
    auto *lightTransform = new Qt3DCore::QTransform(lightEntity);
    lightTransform->setTranslation(QVector3D(150000.0f, 200000.0f, 150000.0f));
    lightEntity->addComponent(m_light);
    lightEntity->addComponent(lightTransform);

    m_skyRenderer = new SkyRenderer(m_rootEntity);

    m_selectionManager = new SelectionManager(this);
    connect(m_selectionManager, &SelectionManager::objectSelected,
            this, &SceneView3D::objectSelected);

    m_3dWindow->setRootEntity(m_rootEntity);
#endif
}

void SceneView3D::clearScene()
{
#ifdef FLATLAS_HAS_QT3D
    ++m_loadGeneration;
    if (m_selectionManager)
    m_selectionManager->clear();
    m_modelHostsByNickname.clear();
    m_markerEntitiesByNickname.clear();
    m_sceneEntitiesByNickname.clear();
    m_nicknamesByModelPath.clear();
    if (m_sceneBounds)
        *m_sceneBounds = ModelBounds();
    if (m_objectBounds)
        *m_objectBounds = ModelBounds();
    if (m_zoneBounds)
        *m_zoneBounds = ModelBounds();

    if (m_sceneRoot) {
        delete m_sceneRoot;
        m_sceneRoot = new Qt3DCore::QEntity(m_rootEntity);
        m_zonesRoot = new Qt3DCore::QEntity(m_sceneRoot);
        m_objectsRoot = new Qt3DCore::QEntity(m_sceneRoot);
    }
#endif
}

void SceneView3D::addZone(const std::shared_ptr<flatlas::domain::ZoneItem> &zone)
{
#ifdef FLATLAS_HAS_QT3D
    if (!zone || !m_zonesRoot)
        return;

    const ZoneVisualStyle style = ZoneColorScheme::styleForZone(*zone);
    const ZoneGeometryBuildResult result = ZoneGeometryBuilder::buildZone(*zone, style, m_zonesRoot);
    if (!result.valid)
        return;
    m_sceneEntitiesByNickname.insert(zone->nickname(), result.rootEntity);

    if (m_sceneBounds)
        m_sceneBounds->include(result.bounds);
    if (m_zoneBounds)
        m_zoneBounds->include(result.bounds);

    if (result.pickEntity && result.selectionMaterial && m_selectionManager)
        m_selectionManager->registerEntity(zone->nickname(), result.pickEntity, result.selectionMaterial);
#else
    Q_UNUSED(zone);
#endif
}

void SceneView3D::addSolarObject(const std::shared_ptr<flatlas::domain::SolarObject> &obj)
{
#ifdef FLATLAS_HAS_QT3D
    if (!obj || !m_objectsRoot)
        return;

    auto *objectEntity = new Qt3DCore::QEntity(m_objectsRoot);
    m_sceneEntitiesByNickname.insert(obj->nickname(), objectEntity);
    auto *objectTransform = new Qt3DCore::QTransform(objectEntity);
    objectTransform->setTranslation(obj->position());
    objectTransform->setRotation(ZoneGeometryBuilder::rotationFromFreelancer(obj->rotation()));
    objectEntity->addComponent(objectTransform);

    auto *modelHost = new Qt3DCore::QEntity(objectEntity);
    m_modelHostsByNickname.insert(obj->nickname(), modelHost);

    const QColor baseColor = objectColor(obj->type());
    const float radius = markerRadius(obj->type());
    auto *markerEntity = new Qt3DCore::QEntity(objectEntity);
    auto *markerMesh = new Qt3DExtras::QSphereMesh(markerEntity);
    markerMesh->setRadius(radius);
    markerMesh->setRings(8);
    markerMesh->setSlices(12);
    auto *markerMaterial = new Qt3DExtras::QPhongMaterial(markerEntity);
    markerMaterial->setDiffuse(baseColor);
    markerMaterial->setAmbient(baseColor.darker(175));
    markerEntity->addComponent(markerMesh);
    markerEntity->addComponent(markerMaterial);
    m_markerEntitiesByNickname.insert(obj->nickname(), markerEntity);

    if (m_selectionManager)
        m_selectionManager->registerEntity(obj->nickname(), markerEntity, markerMaterial);

    if (m_sceneBounds) {
        m_sceneBounds->include(obj->position() + QVector3D(radius, radius, radius));
        m_sceneBounds->include(obj->position() - QVector3D(radius, radius, radius));
    }
    if (m_objectBounds) {
        m_objectBounds->include(obj->position() + QVector3D(radius, radius, radius));
        m_objectBounds->include(obj->position() - QVector3D(radius, radius, radius));
    }

    const QString modelPath = modelPathForObject(*obj);
    if (!modelPath.isEmpty())
        m_nicknamesByModelPath[modelPath].append(obj->nickname());
#else
    Q_UNUSED(obj);
#endif
}

void SceneView3D::updateSceneCamera()
{
#ifdef FLATLAS_HAS_QT3D
    if (!m_orbitCamera || !m_sceneBounds)
        return;

    const ModelBounds *focusBounds = (m_objectBounds && m_objectBounds->valid) ? m_objectBounds : m_sceneBounds;
    const QVector3D center = focusBounds->valid ? focusBounds->center() : QVector3D();
    const float radius = qMax(focusBounds->radius(), 5000.0f);
    const float distance = qMax(radius * 2.4f, 25000.0f);
    m_orbitCamera->setDistanceLimits(qMax(radius * 0.015f, 50.0f), qMax(distance * 80.0f, 1000000.0f));
    m_orbitCamera->setResetState(center, distance, 45.0f, 24.0f);
    m_orbitCamera->resetView();
#endif
}

void SceneView3D::applyDisplayFilter()
{
#ifdef FLATLAS_HAS_QT3D
    if (!m_document)
        return;

    for (const auto &obj : m_document->objects()) {
        if (!obj)
            continue;
        if (Qt3DCore::QEntity *entity = m_sceneEntitiesByNickname.value(obj->nickname(), nullptr))
            entity->setEnabled(objectVisibleForFilter(m_displayFilterSettings, *obj));
    }
    for (const auto &zone : m_document->zones()) {
        if (!zone)
            continue;
        if (Qt3DCore::QEntity *entity = m_sceneEntitiesByNickname.value(zone->nickname(), nullptr))
            entity->setEnabled(zoneVisibleForFilter(m_displayFilterSettings, *zone));
    }
#endif
}

#ifdef FLATLAS_HAS_QT3D
QString SceneView3D::modelPathForObject(const flatlas::domain::SolarObject &obj) const
{
    const QString archetype = obj.archetype().trimmed().toLower();
    if (archetype.isEmpty())
        return {};
    return m_archetypeModelPaths.value(archetype);
}

void SceneView3D::scheduleModelLoading()
{
    if (m_nicknamesByModelPath.isEmpty())
        return;

    const int generation = ++m_loadGeneration;
    const QStringList paths = m_nicknamesByModelPath.keys();
    auto *watcher = new QFutureWatcher<QHash<QString, flatlas::infrastructure::DecodedModel>>(this);
    connect(watcher,
            &QFutureWatcher<QHash<QString, flatlas::infrastructure::DecodedModel>>::finished,
            this,
            [this, watcher, generation]() {
        watcher->deleteLater();
        QHash<QString, flatlas::infrastructure::DecodedModel> models;
        try {
            models = watcher->result();
        } catch (...) {
            return;
        }
        attachLoadedModels(models, generation);
    });

    watcher->setFuture(QtConcurrent::run([paths]() {
        QHash<QString, flatlas::infrastructure::DecodedModel> loaded;
        for (const QString &path : paths) {
            const auto decoded = flatlas::rendering::ModelCache::instance().load(path);
            if (decoded.isValid())
                loaded.insert(path, decoded);
        }
        return loaded;
    }));
}

void SceneView3D::attachLoadedModels(const QHash<QString, flatlas::infrastructure::DecodedModel> &models, int generation)
{
    if (generation != m_loadGeneration)
        return;

    for (auto it = models.constBegin(); it != models.constEnd(); ++it) {
        const QStringList nicknames = m_nicknamesByModelPath.value(it.key());
        for (const QString &nickname : nicknames) {
            Qt3DCore::QEntity *host = m_modelHostsByNickname.value(nickname, nullptr);
            if (!host)
                continue;
            const int renderedMeshCount = addModelNodeRecursive(it.value().rootNode, host, nickname);
            if (renderedMeshCount > 0) {
                if (Qt3DCore::QEntity *marker = m_markerEntitiesByNickname.value(nickname, nullptr))
                    marker->setEnabled(false);
            } else {
                host->setEnabled(false);
            }
        }
    }
}

int SceneView3D::addModelNodeRecursive(const flatlas::infrastructure::ModelNode &node,
                                       Qt3DCore::QEntity *parent,
                                       const QString &nickname,
                                       int nodeIndex,
                                       int depth)
{
    if (!parent)
        return 0;

    auto *nodeEntity = new Qt3DCore::QEntity(parent);
    auto *transform = new Qt3DCore::QTransform(nodeEntity);
    transform->setTranslation(node.origin);
    transform->setRotation(node.rotation);
    nodeEntity->addComponent(transform);

    int renderedMeshCount = 0;
    int bestLodIdx = std::numeric_limits<int>::max();
    for (const auto &mesh : node.meshes) {
        if (mesh.lodIndex >= 0 && mesh.lodIndex < bestLodIdx)
            bestLodIdx = mesh.lodIndex;
    }
    const bool lodFilterActive = bestLodIdx < std::numeric_limits<int>::max();

    for (const auto &mesh : node.meshes) {
        if (lodFilterActive && mesh.lodIndex >= 0 && mesh.lodIndex != bestLodIdx)
            continue;

        auto *meshEntity = new Qt3DCore::QEntity(nodeEntity);
        if (auto *renderer = ModelGeometryBuilder::buildTriangleRenderer(mesh, meshEntity)) {
            auto *material = MaterialFactory::createDefault(colorForMesh(mesh, nodeIndex), meshEntity);
            meshEntity->addComponent(renderer);
            meshEntity->addComponent(material);
            if (m_selectionManager)
                m_selectionManager->registerEntity(nickname, meshEntity, material);
            ++renderedMeshCount;
        } else {
            meshEntity->deleteLater();
        }
    }

    int childIndex = 0;
    for (const auto &child : node.children) {
        const int childNodeIndex = nodeIndex * 7 + depth * 3 + childIndex + 1;
        renderedMeshCount += addModelNodeRecursive(child, nodeEntity, nickname, childNodeIndex, depth + 1);
        ++childIndex;
    }

    if (renderedMeshCount <= 0)
        nodeEntity->setEnabled(false);
    return renderedMeshCount;
}

bool SceneView3D::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched);
    if (!m_orbitCamera || !event)
        return QWidget::eventFilter(watched, event);

    switch (event->type()) {
    case QEvent::MouseButtonPress:
        m_orbitCamera->handleMousePress(static_cast<QMouseEvent *>(event));
        return true;
    case QEvent::MouseMove:
        m_orbitCamera->handleMouseMove(static_cast<QMouseEvent *>(event));
        return true;
    case QEvent::MouseButtonRelease:
        m_orbitCamera->handleMouseRelease(static_cast<QMouseEvent *>(event));
        return true;
    case QEvent::Wheel:
        m_orbitCamera->handleWheel(static_cast<QWheelEvent *>(event));
        return true;
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}
#endif

} // namespace flatlas::rendering
