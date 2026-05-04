// rendering/view3d/SceneView3D.cpp - practical 3D system editor viewport

#include "SceneView3D.h"

#include "domain/SolarObject.h"
#include "domain/SystemDocument.h"
#include "domain/ZoneItem.h"

#include <QLabel>
#include <QVBoxLayout>

#ifdef FLATLAS_HAS_QT3D
#include "FreeCameraController.h"
#include "MaterialFactory.h"
#include "ModelGeometryBuilder.h"
#include "OrbitCamera.h"
#include "RingPreviewSceneBuilder.h"
#include "SelectionManager.h"
#include "SkyRenderer.h"
#include "ZoneColorScheme.h"
#include "ZoneGeometryBuilder.h"
#include "infrastructure/freelancer/FreelancerMaterialResolver.h"
#include "rendering/preview/ModelCache.h"

#include <Qt3DCore/QAttribute>
#include <Qt3DCore/QBuffer>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QGeometry>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QCuboidMesh>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QPointLight>
#include <Qt3DRender/QRenderSettings>

#include <QByteArray>
#include <QEvent>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHideEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPalette>
#include <QRegularExpression>
#include <QShowEvent>
#include <QTimer>
#include <QWheelEvent>
#include <QtConcurrent/QtConcurrent>

#include <cmath>
#include <limits>
#endif

namespace flatlas::rendering {

#ifdef FLATLAS_HAS_QT3D
namespace {

constexpr double kFreelancerNavCellWorld = 30000.0;
constexpr int kFreelancerNavCellsPerAxis = 8;
constexpr double kFreelancerReferenceNavMapScale = 1.36;

double navGridHalfExtentWorld(double navMapScale)
{
    const double scale = navMapScale > 0.0 ? navMapScale : kFreelancerReferenceNavMapScale;
    const double referenceHalfExtent = kFreelancerNavCellWorld * (kFreelancerNavCellsPerAxis / 2.0);
    return referenceHalfExtent * (kFreelancerReferenceNavMapScale / scale);
}

void appendGridPoint(QByteArray &blob, const QVector3D &point)
{
    const float values[] = {point.x(), point.y(), point.z()};
    blob.append(reinterpret_cast<const char *>(values), static_cast<int>(sizeof(values)));
}

Qt3DRender::QGeometryRenderer *buildGridLineRenderer(const QVector<QVector3D> &points, Qt3DCore::QNode *owner)
{
    if (points.size() < 2)
        return nullptr;

    QByteArray vertexBlob;
    vertexBlob.reserve(points.size() * 3 * static_cast<int>(sizeof(float)));
    for (const QVector3D &point : points)
        appendGridPoint(vertexBlob, point);

    auto *geometry = new Qt3DCore::QGeometry(owner);
    auto *vertexBuffer = new Qt3DCore::QBuffer(geometry);
    vertexBuffer->setData(vertexBlob);

    auto *positionAttr = new Qt3DCore::QAttribute(geometry);
    positionAttr->setName(Qt3DCore::QAttribute::defaultPositionAttributeName());
    positionAttr->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
    positionAttr->setVertexBaseType(Qt3DCore::QAttribute::Float);
    positionAttr->setVertexSize(3);
    positionAttr->setByteStride(3 * static_cast<int>(sizeof(float)));
    positionAttr->setCount(points.size());
    positionAttr->setBuffer(vertexBuffer);
    geometry->addAttribute(positionAttr);

    auto *renderer = new Qt3DRender::QGeometryRenderer(owner);
    renderer->setGeometry(geometry);
    renderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Lines);
    renderer->setVertexCount(points.size());
    return renderer;
}

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

float sphereFallbackRadius(const QString &archetype)
{
    static const QRegularExpression trailingNumber(QStringLiteral("_(\\d+(?:\\.\\d+)?)$"));
    const QRegularExpressionMatch match = trailingNumber.match(archetype.trimmed());
    if (match.hasMatch()) {
        bool ok = false;
        const float radius = match.captured(1).toFloat(&ok);
        if (ok && radius > 0.0f)
            return radius;
    }
    return 1500.0f;
}

QColor colorForMesh(const flatlas::infrastructure::MeshData &mesh, int nodeIndex)
{
    const QString materialValue = mesh.materialValue.trimmed();
    if (materialValue.startsWith(QStringLiteral("preview_color:"), Qt::CaseInsensitive)) {
        const QStringList parts = materialValue.mid(QStringLiteral("preview_color:").size())
                                      .split(QLatin1Char(','), Qt::SkipEmptyParts);
        if (parts.size() >= 3) {
            bool okRed = false;
            bool okGreen = false;
            bool okBlue = false;
            const int red = parts[0].trimmed().toInt(&okRed);
            const int green = parts[1].trimmed().toInt(&okGreen);
            const int blue = parts[2].trimmed().toInt(&okBlue);
            if (okRed && okGreen && okBlue)
                return QColor(qBound(0, red, 255), qBound(0, green, 255), qBound(0, blue, 255));
        }
    }

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
    context.archetype = QStringList{
        zone.zoneType(),
        zone.usage(),
        zone.popType(),
        zone.pathLabel(),
        zone.comment(),
    }.join(QLatin1Char(' '));
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

void setEntityTreeEnabled(Qt3DCore::QEntity *entity, bool enabled)
{
    if (!entity)
        return;

    entity->setEnabled(enabled);
    const auto children = entity->childNodes();
    for (Qt3DCore::QNode *child : children) {
        if (auto *childEntity = qobject_cast<Qt3DCore::QEntity *>(child))
            setEntityTreeEnabled(childEntity, enabled);
    }
}

QString rawEntryValue(const flatlas::domain::SolarObject &object, const QString &key)
{
    const auto entries = object.rawEntries();
    for (int index = entries.size() - 1; index >= 0; --index) {
        if (entries[index].first.compare(key, Qt::CaseInsensitive) == 0)
            return entries[index].second.trimmed();
    }
    return {};
}

QString ringZoneNickname(const flatlas::domain::SolarObject &object)
{
    const QString raw = rawEntryValue(object, QStringLiteral("ring"));
    if (raw.isEmpty())
        return {};
    const QStringList parts = raw.split(QLatin1Char(','), Qt::SkipEmptyParts);
    return parts.isEmpty() ? QString() : parts.first().trimmed();
}

QString ringPresetPath(const flatlas::domain::SolarObject &object)
{
    const QString raw = rawEntryValue(object, QStringLiteral("ring"));
    if (raw.isEmpty())
        return {};
    const QStringList parts = raw.split(QLatin1Char(','), Qt::SkipEmptyParts);
    return parts.size() >= 2 ? parts.last().trimmed() : QString();
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
    m_3dWindow->setOpacity(1.0);
    m_container = QWidget::createWindowContainer(m_3dWindow, this);
    m_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_container->setMinimumSize(320, 240);
    m_container->setFocusPolicy(Qt::StrongFocus);
    m_container->setMouseTracking(true);
    m_container->setAutoFillBackground(true);
    m_container->setAttribute(Qt::WA_OpaquePaintEvent, true);
    QPalette containerPalette = m_container->palette();
    containerPalette.setColor(QPalette::Window, QColor(6, 10, 18));
    m_container->setPalette(containerPalette);
    m_container->setToolTip(tr("Left drag rotates, right drag pans, mouse wheel zooms."));
    m_container->installEventFilter(this);
    m_3dWindow->installEventFilter(this);
    layout->addWidget(m_container, 1);

    m_sceneBounds = new ModelBounds();
    m_objectBounds = new ModelBounds();
    m_zoneBounds = new ModelBounds();
    setupScene();
    setViewportActive(isVisible());
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

void SceneView3D::setArchetypeDisplayRadii(const QHash<QString, float> &displayRadii)
{
    m_archetypeDisplayRadii = displayRadii;
}

void SceneView3D::setArchetypeTextureSourcePaths(const QHash<QString, QStringList> &textureSourcePaths)
{
    m_archetypeTextureSourcePaths = textureSourcePaths;
}

void SceneView3D::setGameRoot(const QString &gameRoot)
{
    m_gameRoot = gameRoot;
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
    addNavigationGrid();

    m_linkedRingZoneNicknames.clear();
    for (const auto &obj : doc->objects()) {
        if (!obj)
            continue;
        const QString zoneNickname = ringZoneNickname(*obj);
        if (!zoneNickname.isEmpty())
            m_linkedRingZoneNicknames.insert(zoneNickname);
    }

    for (const auto &zone : doc->zones())
        addZone(zone);
    for (const auto &obj : doc->objects())
        addSolarObject(obj);

    applyDisplayFilter();
    updateSceneCamera();
    scheduleModelLoading();
    schedulePlanetTextureLoading();
#endif
}

void SceneView3D::selectObject(const QString &nickname)
{
#ifdef FLATLAS_HAS_QT3D
    if (!m_selectionManager)
        return;
    if (nickname.isEmpty() || m_objectCentersByNickname.contains(nickname))
        m_selectionManager->select(nickname);
    else
        m_selectionManager->select(QString());
#else
    Q_UNUSED(nickname);
#endif
}

bool SceneView3D::centerOnSelectedObject()
{
#ifdef FLATLAS_HAS_QT3D
    if (!m_selectionManager || !m_orbitCamera)
        return false;

    const QString nickname = m_selectionManager->selectedNickname();
    if (nickname.isEmpty() || !m_objectCentersByNickname.contains(nickname))
        return false;

    const QVector3D center = m_objectCentersByNickname.value(nickname);
    const float radius = qMax(m_objectRadiiByNickname.value(nickname, 0.0f), 500.0f);
    m_orbitCamera->setTarget(center);
    m_orbitCamera->setDistance(qMax(radius * 4.0f, 6000.0f));
    requestViewportUpdate();
    return true;
#else
    return false;
#endif
}

void SceneView3D::setZoomLevel(int percent)
{
    m_zoomLevel = qBound(0, percent, 100);
#ifdef FLATLAS_HAS_QT3D
    applyCameraZoom();
#endif
}

void SceneView3D::setZoneWireframesVisible(bool visible)
{
    m_zoneWireframesVisible = visible;
#ifdef FLATLAS_HAS_QT3D
    applyZoneWireframeVisibility();
#endif
}

void SceneView3D::setFreeCameraModeEnabled(bool enabled)
{
#ifdef FLATLAS_HAS_QT3D
    if (!m_freeCamera)
        return;
    if (m_freeCamera->isEnabled() == enabled)
        return;
    m_freeCamera->setEnabled(enabled);
    if (enabled) {
        if (m_container)
            m_container->setFocus(Qt::OtherFocusReason);
        if (m_freeCameraTimer && !m_freeCameraTimer->isActive()) {
            m_freeCameraClock.restart();
            m_freeCameraTimer->start();
        }
    } else if (m_freeCameraTimer) {
        m_freeCameraTimer->stop();
    }
    emit freeCameraModeChanged(enabled);
    requestViewportUpdate();
#else
    Q_UNUSED(enabled);
#endif
}

bool SceneView3D::isFreeCameraModeEnabled() const
{
#ifdef FLATLAS_HAS_QT3D
    return m_freeCamera && m_freeCamera->isEnabled();
#else
    return false;
#endif
}

float SceneView3D::freeCameraSpeed() const
{
#ifdef FLATLAS_HAS_QT3D
    return m_freeCamera ? m_freeCamera->speed() : 0.0f;
#else
    return 0.0f;
#endif
}

void SceneView3D::setViewportActive(bool active)
{
#ifdef FLATLAS_HAS_QT3D
    if (m_container)
        m_container->setVisible(active);
    if (m_3dWindow)
        m_3dWindow->setVisible(active);
    if (active) {
        if (m_container) {
            m_container->raise();
            m_container->update();
        }
        requestViewportUpdate();
    }
#else
    Q_UNUSED(active);
#endif
}

void SceneView3D::requestViewportUpdate()
{
#ifdef FLATLAS_HAS_QT3D
    if (!m_3dWindow)
        return;
    QTimer::singleShot(0, m_3dWindow, [window = m_3dWindow]() {
        window->requestUpdate();
    });
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
    renderer->setClearColor(QColor(6, 10, 18, 255));
    renderer->setFrustumCullingEnabled(false);

    auto *lightEntity = new Qt3DCore::QEntity(m_rootEntity);
    m_light = new Qt3DRender::QPointLight(lightEntity);
    m_light->setColor(Qt::white);
    m_light->setIntensity(1.6f);
    auto *lightTransform = new Qt3DCore::QTransform(lightEntity);
    lightTransform->setTranslation(QVector3D(150000.0f, 200000.0f, 150000.0f));
    lightEntity->addComponent(m_light);
    lightEntity->addComponent(lightTransform);

    m_skyRenderer = new SkyRenderer(m_rootEntity);
    m_skyRenderer->setRadius(2500000.0f);
    m_skyRenderer->setCenter(m_camera->position());
    connect(m_orbitCamera, &OrbitCamera::cameraChanged, this, [this]() {
        updateCameraDependentScene();
        syncZoomLevelFromCamera();
    });

    m_freeCamera = new FreeCameraController(m_camera, this);
    connect(m_freeCamera, &FreeCameraController::cameraChanged, this, [this]() {
        updateCameraDependentScene();
        requestViewportUpdate();
    });
    connect(m_freeCamera, &FreeCameraController::speedChanged,
            this, &SceneView3D::freeCameraSpeedChanged);
    m_freeCameraTimer = new QTimer(this);
    m_freeCameraTimer->setInterval(16);
    connect(m_freeCameraTimer, &QTimer::timeout, this, &SceneView3D::tickFreeCamera);

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
    m_markerMaterialsByNickname.clear();
    m_ringEntitiesByHostNickname.clear();
    m_sceneEntitiesByNickname.clear();
    m_zoneWireEntitiesByNickname.clear();
    m_nicknamesByModelPath.clear();
    m_planetTextureSourcePathsByNickname.clear();
    m_nicknamesWithRenderedModel.clear();
    m_objectCentersByNickname.clear();
    m_objectRadiiByNickname.clear();
    if (m_sceneBounds)
        *m_sceneBounds = ModelBounds();
    if (m_objectBounds)
        *m_objectBounds = ModelBounds();
    if (m_zoneBounds)
        *m_zoneBounds = ModelBounds();

    if (m_sceneRoot) {
        delete m_sceneRoot;
        m_sceneRoot = new Qt3DCore::QEntity(m_rootEntity);
        m_gridEntity = nullptr;
        m_zonesRoot = new Qt3DCore::QEntity(m_sceneRoot);
        m_objectsRoot = new Qt3DCore::QEntity(m_sceneRoot);
    }
#endif
}

void SceneView3D::addNavigationGrid()
{
#ifdef FLATLAS_HAS_QT3D
    if (!m_document || !m_sceneRoot)
        return;

    m_gridEntity = new Qt3DCore::QEntity(m_sceneRoot);

    const float halfExtent = static_cast<float>(navGridHalfExtentWorld(m_document->navMapScale()));
    const float spacing = (halfExtent * 2.0f) / static_cast<float>(kFreelancerNavCellsPerAxis);
    constexpr float gridY = -1.0f;

    QVector<QVector3D> gridPoints;
    QVector<QVector3D> originPoints;
    gridPoints.reserve((kFreelancerNavCellsPerAxis + 1) * 4);
    originPoints.reserve(4);

    for (int index = 0; index <= kFreelancerNavCellsPerAxis; ++index) {
        const float value = -halfExtent + spacing * static_cast<float>(index);
        QVector<QVector3D> &target = qFuzzyIsNull(value) ? originPoints : gridPoints;
        target.append(QVector3D(value, gridY, -halfExtent));
        target.append(QVector3D(value, gridY, halfExtent));
        target.append(QVector3D(-halfExtent, gridY, value));
        target.append(QVector3D(halfExtent, gridY, value));
    }

    if (auto *renderer = buildGridLineRenderer(gridPoints, m_gridEntity)) {
        auto *material = MaterialFactory::createDefault(QColor(150, 175, 190, 128), m_gridEntity);
        material->setAmbient(QColor(85, 105, 120));
        material->setDiffuse(QColor(150, 175, 190, 128));
        m_gridEntity->addComponent(renderer);
        m_gridEntity->addComponent(material);
    }

    auto *originEntity = new Qt3DCore::QEntity(m_gridEntity);
    if (auto *renderer = buildGridLineRenderer(originPoints, originEntity)) {
        auto *material = MaterialFactory::createDefault(QColor(220, 235, 245, 128), originEntity);
        material->setAmbient(QColor(145, 165, 180));
        material->setDiffuse(QColor(220, 235, 245, 128));
        originEntity->addComponent(renderer);
        originEntity->addComponent(material);
    } else {
        originEntity->deleteLater();
    }
#endif
}

void SceneView3D::addZone(const std::shared_ptr<flatlas::domain::ZoneItem> &zone)
{
#ifdef FLATLAS_HAS_QT3D
    if (!zone || !m_zonesRoot)
        return;
    if (m_linkedRingZoneNicknames.contains(zone->nickname()))
        return;

    const ZoneVisualStyle style = ZoneColorScheme::styleForZone(*zone);
    const ZoneGeometryBuildResult result = ZoneGeometryBuilder::buildZone(*zone, style, m_zonesRoot);
    if (!result.valid)
        return;
    m_sceneEntitiesByNickname.insert(zone->nickname(), result.rootEntity);
    if (result.wireEntity) {
        m_zoneWireEntitiesByNickname.insert(zone->nickname(), result.wireEntity);
        result.wireEntity->setEnabled(m_zoneWireframesVisible);
    }

    if (m_sceneBounds)
        m_sceneBounds->include(result.bounds);
    if (m_zoneBounds)
        m_zoneBounds->include(result.bounds);

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
    const bool radiusSphere = shouldRenderAsRadiusSphere(*obj);
    const float radius = radiusSphere ? displayRadiusForObject(*obj) : markerRadius(obj->type());
    m_objectCentersByNickname.insert(obj->nickname(), obj->position());
    m_objectRadiiByNickname.insert(obj->nickname(), radius);
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
    m_markerMaterialsByNickname.insert(obj->nickname(), markerMaterial);

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
    if (radiusSphere && obj->type() == flatlas::domain::SolarObject::Planet) {
        QStringList textureSources = m_archetypeTextureSourcePaths.value(obj->archetype().trimmed().toLower());
        if (textureSources.isEmpty() && !modelPath.isEmpty())
            textureSources.append(modelPath);
        if (!textureSources.isEmpty())
            m_planetTextureSourcePathsByNickname.insert(obj->nickname(), textureSources);
    } else if (!radiusSphere && !modelPath.isEmpty()) {
        m_nicknamesByModelPath[modelPath].append(obj->nickname());
    }

    addPlanetaryRing(*obj);
#else
    Q_UNUSED(obj);
#endif
}

void SceneView3D::addPlanetaryRing(const flatlas::domain::SolarObject &obj)
{
#ifdef FLATLAS_HAS_QT3D
    if (!m_document || !m_objectsRoot)
        return;

    const QString zoneNickname = ringZoneNickname(obj);
    if (zoneNickname.isEmpty())
        return;

    flatlas::domain::ZoneItem *ringZone = nullptr;
    for (const auto &zone : m_document->zones()) {
        if (zone && zone->nickname().compare(zoneNickname, Qt::CaseInsensitive) == 0) {
            ringZone = zone.get();
            break;
        }
    }
    if (!ringZone || ringZone->shape() != flatlas::domain::ZoneItem::Ring)
        return;

    const QVector3D size = ringZone->size();
    const double outerRadius = static_cast<double>(size.x());
    const double innerRadius = static_cast<double>(size.y());
    const double thickness = static_cast<double>(size.z());
    if (innerRadius <= 0.0 || outerRadius <= innerRadius || thickness <= 0.0)
        return;

    RingPreviewSceneRequest request;
    request.gameRoot = m_gameRoot;
    request.ringPreset = ringPresetPath(obj);
    request.innerRadius = innerRadius;
    request.outerRadius = outerRadius;
    request.thickness = thickness;
    request.rotationEuler = ringZone->rotation();

    const RingPreviewSceneResult result = RingPreviewSceneBuilder::build(request);
    if (!result.hasRingGeometry)
        return;

    auto *ringEntity = new Qt3DCore::QEntity(m_objectsRoot);
    auto *transform = new Qt3DCore::QTransform(ringEntity);
    transform->setTranslation(ringZone->position());
    ringEntity->addComponent(transform);

    const int rendered = addModelNodeRecursive(result.sceneRoot, ringEntity, obj.nickname(), QString());
    if (rendered <= 0) {
        ringEntity->deleteLater();
        return;
    }

    m_ringEntitiesByHostNickname[obj.nickname()].append(ringEntity);

    if (m_sceneBounds) {
        const QVector3D extent(static_cast<float>(outerRadius),
                               static_cast<float>(qMax(thickness, outerRadius * 0.05)),
                               static_cast<float>(outerRadius));
        m_sceneBounds->include(ringZone->position() + extent);
        m_sceneBounds->include(ringZone->position() - extent);
    }
    if (m_objectBounds) {
        const QVector3D extent(static_cast<float>(outerRadius),
                               static_cast<float>(qMax(thickness, outerRadius * 0.05)),
                               static_cast<float>(outerRadius));
        m_objectBounds->include(ringZone->position() + extent);
        m_objectBounds->include(ringZone->position() - extent);
    }
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
    applyCameraZoom();
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
            setEntityTreeEnabled(entity, objectVisibleForFilter(m_displayFilterSettings, *obj));
        const bool visible = objectVisibleForFilter(m_displayFilterSettings, *obj);
        for (Qt3DCore::QEntity *ringEntity : m_ringEntitiesByHostNickname.value(obj->nickname()))
            setEntityTreeEnabled(ringEntity, visible);
        if (m_nicknamesWithRenderedModel.contains(obj->nickname())) {
            if (Qt3DCore::QEntity *marker = m_markerEntitiesByNickname.value(obj->nickname(), nullptr))
                marker->setEnabled(false);
        }
    }
    for (const auto &zone : m_document->zones()) {
        if (!zone)
            continue;
        if (Qt3DCore::QEntity *entity = m_sceneEntitiesByNickname.value(zone->nickname(), nullptr))
            setEntityTreeEnabled(entity, zoneVisibleForFilter(m_displayFilterSettings, *zone));
    }
#endif
}

void SceneView3D::applyCameraZoom()
{
#ifdef FLATLAS_HAS_QT3D
    if (!m_orbitCamera)
        return;

    const float minDistance = m_orbitCamera->minDistance();
    const float maxDistance = m_orbitCamera->maxDistance();
    if (minDistance <= 0.0f || maxDistance <= minDistance)
        return;

    const double t = static_cast<double>(qBound(0, m_zoomLevel, 100)) / 100.0;
    const double logMin = std::log(static_cast<double>(minDistance));
    const double logMax = std::log(static_cast<double>(maxDistance));
    const float distance = static_cast<float>(std::exp(logMax + (logMin - logMax) * t));
    m_orbitCamera->setDistance(qBound(minDistance, distance, maxDistance));
#endif
}

void SceneView3D::syncZoomLevelFromCamera()
{
#ifdef FLATLAS_HAS_QT3D
    if (!m_orbitCamera)
        return;

    const float minDistance = m_orbitCamera->minDistance();
    const float maxDistance = m_orbitCamera->maxDistance();
    const float distance = qBound(minDistance, m_orbitCamera->distance(), maxDistance);
    if (minDistance <= 0.0f || maxDistance <= minDistance)
        return;

    const double logMin = std::log(static_cast<double>(minDistance));
    const double logMax = std::log(static_cast<double>(maxDistance));
    const double logDistance = std::log(static_cast<double>(distance));
    const double normalized = (logMax - logDistance) / (logMax - logMin);
    const int nextZoomLevel = qBound(0, static_cast<int>(std::lround(normalized * 100.0)), 100);
    if (nextZoomLevel == m_zoomLevel)
        return;

    m_zoomLevel = nextZoomLevel;
    emit zoomLevelChanged(m_zoomLevel);
#endif
}

void SceneView3D::applyZoneWireframeVisibility()
{
#ifdef FLATLAS_HAS_QT3D
    for (auto it = m_zoneWireEntitiesByNickname.begin(); it != m_zoneWireEntitiesByNickname.end(); ++it) {
        if (it.value())
            it.value()->setEnabled(m_zoneWireframesVisible);
    }
#endif
}

void SceneView3D::tickFreeCamera()
{
#ifdef FLATLAS_HAS_QT3D
    if (!m_freeCamera || !m_freeCamera->isEnabled())
        return;

    const qint64 elapsed = m_freeCameraClock.isValid() ? m_freeCameraClock.restart() : 16;
    const float deltaSeconds = qBound(0.001f, static_cast<float>(elapsed) / 1000.0f, 0.1f);
    m_freeCamera->update(deltaSeconds);
#endif
}

void SceneView3D::updateCameraDependentScene()
{
#ifdef FLATLAS_HAS_QT3D
    if (m_skyRenderer && m_camera)
        m_skyRenderer->setCenter(m_camera->position());
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

float SceneView3D::displayRadiusForObject(const flatlas::domain::SolarObject &obj) const
{
    const QString archetype = obj.archetype().trimmed().toLower();
    const float resolvedRadius = m_archetypeDisplayRadii.value(archetype, 0.0f);
    if (resolvedRadius > 0.0f)
        return resolvedRadius;
    if (shouldRenderAsRadiusSphere(obj))
        return sphereFallbackRadius(obj.archetype());
    return markerRadius(obj.type());
}

bool SceneView3D::shouldRenderAsRadiusSphere(const flatlas::domain::SolarObject &obj) const
{
    using Type = flatlas::domain::SolarObject::Type;
    const QString archetype = obj.archetype().trimmed().toLower();
    return obj.type() == Type::Planet
        || obj.type() == Type::Sun
        || archetype.contains(QStringLiteral("planet"))
        || archetype.contains(QStringLiteral("sun"))
        || archetype.contains(QStringLiteral("star"));
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
            const int renderedMeshCount = addModelNodeRecursive(it.value().rootNode, host, nickname, it.key());
            if (renderedMeshCount > 0) {
                m_nicknamesWithRenderedModel.insert(nickname);
                if (Qt3DCore::QEntity *marker = m_markerEntitiesByNickname.value(nickname, nullptr))
                    marker->setEnabled(false);
            } else {
                host->setEnabled(false);
            }
        }

    }
    applyDisplayFilter();
}

void SceneView3D::schedulePlanetTextureLoading()
{
    if (m_planetTextureSourcePathsByNickname.isEmpty())
        return;

    const int generation = m_loadGeneration;
    const QHash<QString, QStringList> sourcePathsByNickname = m_planetTextureSourcePathsByNickname;
    const QHash<QString, QString> archetypesByNickname = [&]() {
        QHash<QString, QString> values;
        if (!m_document)
            return values;
        for (const auto &obj : m_document->objects()) {
            if (obj)
                values.insert(obj->nickname(), obj->archetype());
        }
        return values;
    }();

    auto *watcher = new QFutureWatcher<QHash<QString, QImage>>(this);
    connect(watcher,
            &QFutureWatcher<QHash<QString, QImage>>::finished,
            this,
            [this, watcher, generation]() {
        watcher->deleteLater();
        QHash<QString, QImage> textures;
        try {
            textures = watcher->result();
        } catch (...) {
            return;
        }
        applyPlanetTextures(textures, generation);
    });

    watcher->setFuture(QtConcurrent::run([sourcePathsByNickname, archetypesByNickname]() {
        QHash<QString, QImage> textures;
        for (auto it = sourcePathsByNickname.constBegin(); it != sourcePathsByNickname.constEnd(); ++it) {
            const QImage texture = flatlas::infrastructure::FreelancerMaterialResolver::loadBestPlanetTexture(
                archetypesByNickname.value(it.key()), it.value());
            if (!texture.isNull())
                textures.insert(it.key(), texture);
        }
        return textures;
    }));
}

void SceneView3D::applyPlanetTextures(const QHash<QString, QImage> &textures, int generation)
{
    if (generation != m_loadGeneration)
        return;

    for (auto it = textures.constBegin(); it != textures.constEnd(); ++it) {
        Qt3DCore::QEntity *marker = m_markerEntitiesByNickname.value(it.key(), nullptr);
        if (!marker || it.value().isNull())
            continue;

        if (Qt3DRender::QMaterial *oldMaterial = m_markerMaterialsByNickname.value(it.key(), nullptr))
            marker->removeComponent(oldMaterial);

        Qt3DRender::QMaterial *textureMaterial = MaterialFactory::createFromImage(it.value(), marker);
        marker->addComponent(textureMaterial);
        m_markerMaterialsByNickname.insert(it.key(), textureMaterial);

        if (m_selectionManager)
            m_selectionManager->registerEntity(it.key(), marker, textureMaterial);
    }
}

int SceneView3D::addModelNodeRecursive(const flatlas::infrastructure::ModelNode &node,
                                       Qt3DCore::QEntity *parent,
                                       const QString &nickname,
                                       const QString &modelPath,
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
            Qt3DRender::QMaterial *material = nullptr;
            if (!modelPath.isEmpty()) {
                const QImage texture = flatlas::infrastructure::FreelancerMaterialResolver::loadTextureForMesh(modelPath, mesh);
                if (!texture.isNull())
                    material = MaterialFactory::createFromImage(texture, meshEntity);
            }
            if (!material)
                material = MaterialFactory::createDefault(colorForMesh(mesh, nodeIndex), meshEntity);
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
        renderedMeshCount += addModelNodeRecursive(child, nodeEntity, nickname, modelPath, childNodeIndex, depth + 1);
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
    case QEvent::Show:
    case QEvent::Expose:
    case QEvent::WindowActivate:
        setViewportActive(isVisible());
        break;
    case QEvent::MouseButtonPress:
        if (m_freeCamera && m_freeCamera->isEnabled()) {
            if (auto *mouseEvent = static_cast<QMouseEvent *>(event);
                mouseEvent->button() == Qt::LeftButton && m_container) {
                m_container->grabMouse();
            }
            m_freeCamera->handleMousePress(static_cast<QMouseEvent *>(event));
            return true;
        }
        m_orbitCamera->handleMousePress(static_cast<QMouseEvent *>(event));
        return true;
    case QEvent::MouseMove:
        if (m_freeCamera && m_freeCamera->isEnabled()) {
            m_freeCamera->handleMouseMove(static_cast<QMouseEvent *>(event));
            return true;
        }
        m_orbitCamera->handleMouseMove(static_cast<QMouseEvent *>(event));
        return true;
    case QEvent::MouseButtonRelease:
        if (m_freeCamera && m_freeCamera->isEnabled()) {
            m_freeCamera->handleMouseRelease(static_cast<QMouseEvent *>(event));
            if (auto *mouseEvent = static_cast<QMouseEvent *>(event);
                mouseEvent->button() == Qt::LeftButton && m_container) {
                m_container->releaseMouse();
            }
            return true;
        }
        m_orbitCamera->handleMouseRelease(static_cast<QMouseEvent *>(event));
        return true;
    case QEvent::Wheel:
        if (m_freeCamera && m_freeCamera->isEnabled()) {
            m_freeCamera->handleWheel(static_cast<QWheelEvent *>(event));
            return true;
        }
        m_orbitCamera->handleWheel(static_cast<QWheelEvent *>(event));
        return true;
    case QEvent::KeyPress:
        if (m_freeCamera && m_freeCamera->isEnabled()) {
            m_freeCamera->handleKeyPress(static_cast<QKeyEvent *>(event));
            return true;
        }
        break;
    case QEvent::KeyRelease:
        if (m_freeCamera && m_freeCamera->isEnabled()) {
            m_freeCamera->handleKeyRelease(static_cast<QKeyEvent *>(event));
            return true;
        }
        break;
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

void SceneView3D::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    setViewportActive(true);
    if (m_freeCamera && m_freeCamera->isEnabled() && m_freeCameraTimer && !m_freeCameraTimer->isActive()) {
        m_freeCameraClock.restart();
        m_freeCameraTimer->start();
    }
}

void SceneView3D::hideEvent(QHideEvent *event)
{
    if (m_freeCameraTimer)
        m_freeCameraTimer->stop();
    setViewportActive(false);
    QWidget::hideEvent(event);
}
#endif

} // namespace flatlas::rendering
