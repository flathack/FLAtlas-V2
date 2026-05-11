// rendering/view3d/ModelViewport3D.cpp - reusable Qt3D model viewport for Freelancer models

#include "ModelViewport3D.h"

#include "rendering/preview/ModelCache.h"
#include "ModelGeometryBuilder.h"
#include "infrastructure/freelancer/FreelancerMaterialResolver.h"
#include "infrastructure/io/CmpLoader.h"

#include <QEvent>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <QtConcurrent/QtConcurrent>
#include <QtMath>

#include <cmath>
#include <limits>

#ifdef FLATLAS_HAS_QT3D
#include "OrbitCamera.h"
#include "MaterialFactory.h"

#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QPointLight>
#endif

namespace flatlas::rendering {

namespace {

QString formatLoadError(const QString &filePath)
{
    return QObject::tr("Could not load a supported Freelancer model from %1.")
        .arg(QFileInfo(filePath).fileName());
}

// Base palette for parts without material names. Used when no texture is found.
// Each nodeIndex selects a distinct hue; brightness adapts to the background.
QColor colorForNodeIndex(int nodeIndex, bool brightBackground)
{
    // 12 evenly-spaced hues so adjacent parts stay distinguishable
    static const int hues[] = {210, 30, 120, 0, 270, 60, 190, 340, 90, 240, 155, 315};
    const int hue = hues[nodeIndex % 12];
    const int sat = brightBackground ? 48 + (nodeIndex % 3) * 8
                                     : 55 + (nodeIndex % 3) * 10;
    const int val = brightBackground ? 150 + (nodeIndex % 4) * 10
                                     : 205 + (nodeIndex % 4) * 8;
    return QColor::fromHsv(hue, sat, val);
}

QColor colorForMesh(const flatlas::infrastructure::MeshData &mesh, int nodeIndex, bool brightBackground)
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

    if (mesh.materialName.isEmpty())
        return colorForNodeIndex(nodeIndex, brightBackground);

    const uint hash = qHash(mesh.materialName.toLower());
    const int hue = static_cast<int>(hash % 360u);
    // On a bright background reduce value so faces don't wash out
    const int sat = brightBackground ? 45 + static_cast<int>((hash / 360u) % 35u)
                                     : 52 + static_cast<int>((hash / 360u) % 42u);
    const int val = brightBackground ? 145 + static_cast<int>((hash / (360u * 100u)) % 35u)
                                     : 198 + static_cast<int>((hash / (360u * 80u)) % 36u);
    return QColor::fromHsv(hue, qBound(0, sat, 255), qBound(0, val, 255));
}

struct TextureLoadResult {
    int targetIndex = -1;
    QImage image;
};

} // namespace

ModelViewport3D::ModelViewport3D(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

ModelViewport3D::~ModelViewport3D() = default;

void ModelViewport3D::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

#ifdef FLATLAS_HAS_QT3D
    try {
        m_window = new Qt3DExtras::Qt3DWindow();
        m_container = QWidget::createWindowContainer(m_window, this);
        if (m_container) {
            m_container->setFocusPolicy(Qt::StrongFocus);
            m_container->setMouseTracking(true);
            m_container->setToolTip(tr("Left drag rotates, right drag moves, mouse wheel zooms."));
            m_container->installEventFilter(this);
            m_window->installEventFilter(this);
            layout->addWidget(m_container, 1);
            setupScene();
        }
    } catch (...) {
        m_window = nullptr;
        m_container = nullptr;
    }

    if (!m_window || !m_container) {
        auto *placeholder = new QLabel(tr("The 3D viewport could not be initialized."), this);
        placeholder->setAlignment(Qt::AlignCenter);
        layout->addWidget(placeholder, 1);
    }
#else
    auto *placeholder = new QLabel(tr("Qt3D is not available in this build."), this);
    placeholder->setAlignment(Qt::AlignCenter);
    layout->addWidget(placeholder, 1);
#endif

    m_statusOverlay = new QWidget(this);
    m_statusOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_statusOverlay->setStyleSheet(QStringLiteral(
        "QWidget { background: rgba(0, 0, 0, 140); }"
        "QLabel { color: white; font-size: 13px; padding: 10px; }"));
    auto *overlayLayout = new QVBoxLayout(m_statusOverlay);
    overlayLayout->setContentsMargins(16, 16, 16, 16);
    overlayLayout->addStretch();
    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    overlayLayout->addWidget(m_statusLabel);
    overlayLayout->addStretch();
    setStatusMessage(tr("No model loaded."));
}

#ifdef FLATLAS_HAS_QT3D
void ModelViewport3D::setupScene()
{
    if (!m_window)
        return;

    m_rootEntity = new Qt3DCore::QEntity();
    m_sceneRoot = new Qt3DCore::QEntity(m_rootEntity);
    m_modelRoot = new Qt3DCore::QEntity(m_sceneRoot);
    m_wireframeRoot = new Qt3DCore::QEntity(m_sceneRoot);
    m_overlayRoot = new Qt3DCore::QEntity(m_sceneRoot);

    m_camera = m_window->camera();
    m_camera->lens()->setPerspectiveProjection(45.0f, 16.0f / 9.0f, 0.1f, 5000000.0f);

    m_orbitCamera = new OrbitCamera(m_camera, this);
    m_orbitCamera->setRotateButton(Qt::LeftButton);
    m_orbitCamera->setPanButton(Qt::RightButton);
    m_orbitCamera->setResetState(QVector3D(0.0f, 0.0f, 0.0f), 500.0f, 45.0f, 22.0f);
    m_orbitCamera->resetView();
    connect(m_orbitCamera, &OrbitCamera::cameraChanged, this, &ModelViewport3D::updateCameraDependentScene);

    auto *renderer = m_window->defaultFrameGraph();
    renderer->setClearColor(QColor(6, 10, 18));
    renderer->setFrustumCullingEnabled(false);

    m_lightEntity = new Qt3DCore::QEntity(m_rootEntity);
    m_light = new Qt3DRender::QPointLight(m_lightEntity);
    m_light->setColor(m_lightColor);
    m_light->setIntensity(m_lightIntensity);
    m_lightTransform = new Qt3DCore::QTransform(m_lightEntity);
    m_lightEntity->addComponent(m_light);
    m_lightEntity->addComponent(m_lightTransform);
    updateCameraDependentScene();

    m_fillLightEntity = new Qt3DCore::QEntity(m_rootEntity);
    m_fillLight = new Qt3DRender::QPointLight(m_fillLightEntity);
    m_fillLight->setColor(m_lightColor);
    m_fillLight->setIntensity(m_lightIntensity * 0.25f);
    m_fillLightTransform = new Qt3DCore::QTransform(m_fillLightEntity);
    m_fillLightEntity->addComponent(m_fillLight);
    m_fillLightEntity->addComponent(m_fillLightTransform);

    m_window->setRootEntity(m_rootEntity);
}

void ModelViewport3D::clearSceneEntities()
{
    if (!m_sceneRoot || !m_rootEntity)
        return;

    ++m_textureGeneration;
    m_textureTargets.clear();

    delete m_sceneRoot;
    m_sceneRoot = new Qt3DCore::QEntity(m_rootEntity);
    m_modelRoot = new Qt3DCore::QEntity(m_sceneRoot);
    m_wireframeRoot = new Qt3DCore::QEntity(m_sceneRoot);
    m_overlayRoot = new Qt3DCore::QEntity(m_sceneRoot);
    m_boundingBoxEntity = nullptr;
}

void ModelViewport3D::addNodeRecursive(const flatlas::infrastructure::ModelNode &node,
                                       Qt3DCore::QEntity *meshParentEntity,
                                       Qt3DCore::QEntity *wireParentEntity,
                                       QVector<TextureLoadJob> *textureJobs,
                                       int nodeIndex,
                                       int depth)
{
    Qt3DCore::QEntity *meshNodeEntity = nullptr;
    Qt3DCore::QEntity *wireNodeEntity = nullptr;

    if (meshParentEntity) {
        meshNodeEntity = new Qt3DCore::QEntity(meshParentEntity);
        auto *transform = new Qt3DCore::QTransform(meshNodeEntity);
        transform->setTranslation(node.origin);
        transform->setRotation(node.rotation);
        meshNodeEntity->addComponent(transform);
    }
    if (wireParentEntity) {
        wireNodeEntity = new Qt3DCore::QEntity(wireParentEntity);
        auto *transform = new Qt3DCore::QTransform(wireNodeEntity);
        transform->setTranslation(node.origin);
        transform->setRotation(node.rotation);
        wireNodeEntity->addComponent(transform);
    }

    // Determine the best (lowest) LOD index available for this node, then only
    // render meshes at that level.  Rendering all LOD meshes simultaneously
    // causes the classic "stacked silhouette" artefact.
    int bestLodIdx = std::numeric_limits<int>::max();
    for (const auto &m : node.meshes) {
        if (m.lodIndex >= 0 && m.lodIndex < bestLodIdx)
            bestLodIdx = m.lodIndex;
    }
    // If no mesh has a non-negative lodIndex, render all meshes (fallback for
    // simple .3db files that have no LOD metadata).
    const bool lodFilterActive = bestLodIdx < std::numeric_limits<int>::max();

    for (int meshIndex = 0; meshIndex < node.meshes.size(); ++meshIndex) {
        const auto &mesh = node.meshes.at(meshIndex);

        // Skip meshes that belong to a worse LOD level.
        if (lodFilterActive && mesh.lodIndex >= 0 && mesh.lodIndex != bestLodIdx)
            continue;

        if (meshNodeEntity) {
            auto *meshEntity = new Qt3DCore::QEntity(meshNodeEntity);
            if (auto *renderer = ModelGeometryBuilder::buildTriangleRenderer(mesh, meshEntity)) {
                auto *material = MaterialFactory::createDefault(
                    colorForMesh(mesh, nodeIndex, m_whiteBackground), meshEntity);
                meshEntity->addComponent(renderer);
                meshEntity->addComponent(material);
                if (m_texturesVisible && textureJobs) {
                    const int targetIndex = m_textureTargets.size();
                    m_textureTargets.append(TextureTarget{meshEntity, material});
                    textureJobs->append(TextureLoadJob{targetIndex, m_filePath, mesh});
                }
            } else {
                meshEntity->deleteLater();
            }
        }

        if (wireNodeEntity) {
            auto *wireEntity = new Qt3DCore::QEntity(wireNodeEntity);
            if (auto *renderer = ModelGeometryBuilder::buildWireframeRenderer(mesh, wireEntity)) {
                const QColor wireColor = m_whiteBackground ? QColor(30, 30, 30) : QColor(20, 20, 20);
                auto *material = MaterialFactory::createDefault(wireColor, wireEntity);
                wireEntity->addComponent(renderer);
                wireEntity->addComponent(material);
            } else {
                wireEntity->deleteLater();
            }
        }
    }

    int childIndex = 0;
    for (const auto &child : node.children) {
        // Each child gets a unique index derived from the parent's index and its
        // own position so siblings have different colours across the whole tree.
        const int childNodeIndex = nodeIndex * 7 + depth * 3 + childIndex + 1;
        addNodeRecursive(child, meshNodeEntity, wireNodeEntity, textureJobs, childNodeIndex, depth + 1);
        ++childIndex;
    }
}

void ModelViewport3D::startTextureJobs(const QVector<TextureLoadJob> &jobs, int generation)
{
    if (jobs.isEmpty())
        return;

    auto *watcher = new QFutureWatcher<QVector<TextureLoadResult>>(this);
    connect(watcher, &QFutureWatcher<QVector<TextureLoadResult>>::finished,
            this, [this, watcher, generation]() {
        watcher->deleteLater();

        if (generation != m_textureGeneration)
            return;

        const QVector<TextureLoadResult> results = watcher->result();
        for (const TextureLoadResult &result : results) {
            if (result.targetIndex < 0 || result.targetIndex >= m_textureTargets.size() || result.image.isNull())
                continue;

            TextureTarget &target = m_textureTargets[result.targetIndex];
            if (!target.entity)
                continue;

            if (target.material) {
                target.entity->removeComponent(target.material);
                target.material->deleteLater();
            }
            auto *material = MaterialFactory::createFromImage(result.image, target.entity);
            target.entity->addComponent(material);
            target.material = material;
        }
    });

    watcher->setFuture(QtConcurrent::run([jobs]() {
        QVector<TextureLoadResult> results;
        results.reserve(jobs.size());
        for (const TextureLoadJob &job : jobs) {
            const QImage image =
                flatlas::infrastructure::FreelancerMaterialResolver::loadTextureForMesh(job.modelPath, job.mesh);
            if (!image.isNull())
                results.append(TextureLoadResult{job.targetIndex, image});
        }
        return results;
    }));
}

void ModelViewport3D::updateCameraDependentScene()
{
    if (!m_camera)
        return;

    updateLightPosition();
}

void ModelViewport3D::updateLightPosition()
{
    if (!m_lightTransform)
        return;

    const float azimuthRad = qDegreesToRadians(m_lightAzimuth);
    const float elevationRad = qDegreesToRadians(m_lightElevation);
    const float horizontal = std::cos(elevationRad) * m_lightRadius;
    const QVector3D offset(horizontal * std::sin(azimuthRad),
                           std::sin(elevationRad) * m_lightRadius,
                           horizontal * std::cos(azimuthRad));
    m_lightTransform->setTranslation(m_lightTarget + offset);
    if (m_fillLightTransform)
        m_fillLightTransform->setTranslation(m_lightTarget - offset * 0.65f + QVector3D(0.0f, m_lightRadius * 0.2f, 0.0f));
}

void ModelViewport3D::updateVisibilityState()
{
    if (m_modelRoot)
        m_modelRoot->setEnabled(m_meshVisible);
    if (m_wireframeRoot)
        m_wireframeRoot->setEnabled(m_wireframeVisible);
    if (m_boundingBoxEntity)
        m_boundingBoxEntity->setEnabled(m_boundingBoxVisible);
}

void ModelViewport3D::fitCameraToBounds(const ModelBounds &bounds)
{
    if (!m_orbitCamera)
        return;

    const QVector3D center = bounds.valid ? bounds.center() : QVector3D();
    const float radius = qMax(bounds.radius(), 1.0f);
    const float distance = qMax(radius * 2.8f, 10.0f);
    const float minDistance = qMax(radius * 0.12f, 0.25f);
    const float maxDistance = qMax(distance * 200.0f, 500000.0f);
    const float farPlane = qMin(qMax(maxDistance * 2.0f, 5000000.0f), 100000000.0f);

    m_lightTarget = center;
    m_lightRadius = qMax(radius * 3.5f, 1000.0f);
    updateLightPosition();

    if (m_camera && m_camera->lens())
        m_camera->lens()->setFarPlane(farPlane);
    m_orbitCamera->setDistanceLimits(minDistance, maxDistance);
    m_orbitCamera->setResetState(center, distance, 45.0f, 22.0f);
    m_orbitCamera->resetView();
}
#endif

bool ModelViewport3D::loadModelFile(const QString &filePath, QString *errorMessage)
{
    // Increment generation so any previous async load can detect it is stale.
    const int myGeneration = ++m_loadGeneration;
    clearModel();

#ifdef FLATLAS_HAS_QT3D
    if (!m_window || !m_container || !m_rootEntity) {
        const QString message = tr("The 3D renderer is not available.");
        setStatusMessage(message);
        if (errorMessage)
            *errorMessage = message;
        emit modelLoaded(filePath, false, message);
        return false;
    }
#endif

    // Show loading indicator immediately so the UI never looks frozen.
    m_filePath = filePath;
    setStatusMessage(tr("Loading model..."));

    // Run the heavy file I/O and parsing on a background thread.
    // rebuildScene() is called back on the main thread via the finished signal.
    auto *watcher = new QFutureWatcher<flatlas::infrastructure::DecodedModel>(this);
    connect(watcher, &QFutureWatcher<flatlas::infrastructure::DecodedModel>::finished,
            this, [this, watcher, filePath, myGeneration]() {
        watcher->deleteLater();

        // Discard if a newer load was started while this one was running.
        if (myGeneration != m_loadGeneration)
            return;

        flatlas::infrastructure::DecodedModel decoded;
        try {
            decoded = watcher->result();
        } catch (...) {
            const QString message = tr("An unexpected error occurred while loading %1.")
                                        .arg(QFileInfo(filePath).fileName());
            m_filePath.clear();
            setStatusMessage(message);
            emit modelLoaded(filePath, false, message);
            return;
        }

        if (!decoded.isValid()) {
            m_filePath.clear();
            const QString message = formatLoadError(filePath);
            setStatusMessage(message);
            emit modelLoaded(filePath, false, message);
            return;
        }

        m_hasModel = true;
        m_currentModel = std::make_unique<flatlas::infrastructure::ModelNode>(decoded.rootNode);
        try {
            rebuildScene(decoded.rootNode);
        } catch (...) {
            clearModel();
            const QString message = tr("The model could not be rendered safely: %1.")
                                        .arg(QFileInfo(filePath).fileName());
            setStatusMessage(message);
            emit modelLoaded(filePath, false, message);
            return;
        }
        setStatusMessage(QString());
        emit modelLoaded(filePath, true, QString());
    });

    watcher->setFuture(QtConcurrent::run([filePath]() {
        return flatlas::rendering::ModelCache::instance().load(filePath);
    }));
    return true;
}

bool ModelViewport3D::loadModelNode(const flatlas::infrastructure::ModelNode &model, QString *errorMessage)
{
    ++m_loadGeneration;
    clearModel();

#ifdef FLATLAS_HAS_QT3D
    if (!m_window || !m_container || !m_rootEntity) {
        const QString message = tr("The 3D renderer is not available.");
        setStatusMessage(message);
        if (errorMessage)
            *errorMessage = message;
        emit modelLoaded(QString(), false, message);
        return false;
    }
#endif

    try {
        m_currentModel = std::make_unique<flatlas::infrastructure::ModelNode>(model);
        rebuildScene(model);
    } catch (...) {
        clearModel();
        const QString message = tr("The grouped preview could not be rendered safely.");
        if (errorMessage)
            *errorMessage = message;
        setStatusMessage(message);
        emit modelLoaded(QString(), false, message);
        return false;
    }

    m_hasModel = true;
    setStatusMessage(QString());
    emit modelLoaded(QString(), true, QString());
    return true;
}

void ModelViewport3D::rebuildScene(const flatlas::infrastructure::ModelNode &model)
{
#ifdef FLATLAS_HAS_QT3D
    clearSceneEntities();
    const int textureGeneration = m_textureGeneration;
    QVector<TextureLoadJob> textureJobs;
    addNodeRecursive(model, m_modelRoot, m_wireframeRoot, &textureJobs);

    const ModelBounds bounds = ModelGeometryBuilder::boundsForNode(model);
    if (m_overlayRoot && bounds.valid) {
        m_boundingBoxEntity = new Qt3DCore::QEntity(m_overlayRoot);
        if (auto *renderer = ModelGeometryBuilder::buildBoundingBoxRenderer(bounds, m_boundingBoxEntity)) {
            auto *material = MaterialFactory::createDefault(QColor(80, 180, 255), m_boundingBoxEntity);
            m_boundingBoxEntity->addComponent(renderer);
            m_boundingBoxEntity->addComponent(material);
        }
    }

    fitCameraToBounds(bounds);
    updateCameraDependentScene();
    updateVisibilityState();
    startTextureJobs(textureJobs, textureGeneration);
#else
    Q_UNUSED(model);
#endif
}

void ModelViewport3D::clearModel()
{
    m_filePath.clear();
    m_currentModel.reset();
    m_hasModel = false;
#ifdef FLATLAS_HAS_QT3D
    clearSceneEntities();
#endif
    setStatusMessage(tr("No model loaded."));
}

void ModelViewport3D::resetView()
{
#ifdef FLATLAS_HAS_QT3D
    if (m_orbitCamera)
        m_orbitCamera->resetView();
#endif
}

OrbitCameraState ModelViewport3D::cameraState() const
{
    OrbitCameraState state;
#ifdef FLATLAS_HAS_QT3D
    if (m_orbitCamera) {
        state.target = m_orbitCamera->target();
        state.distance = m_orbitCamera->distance();
        state.azimuth = m_orbitCamera->azimuth();
        state.elevation = m_orbitCamera->elevation();
        state.valid = true;
    }
#endif
    return state;
}

void ModelViewport3D::setCameraState(const OrbitCameraState &state)
{
#ifdef FLATLAS_HAS_QT3D
    if (!m_orbitCamera || !state.valid)
        return;
    m_orbitCamera->setTarget(state.target);
    m_orbitCamera->setDistance(state.distance);
    m_orbitCamera->setAzimuth(state.azimuth);
    m_orbitCamera->setElevation(state.elevation);
#else
    Q_UNUSED(state);
#endif
}

void ModelViewport3D::setWireframeVisible(bool visible)
{
    if (m_wireframeVisible == visible)
        return;
    m_wireframeVisible = visible;
    updateVisibilityState();
}

void ModelViewport3D::setMeshVisible(bool visible)
{
    if (m_meshVisible == visible)
        return;
    m_meshVisible = visible;
    updateVisibilityState();
}

void ModelViewport3D::setBoundingBoxVisible(bool visible)
{
    if (m_boundingBoxVisible == visible)
        return;
    m_boundingBoxVisible = visible;
    updateVisibilityState();
}

void ModelViewport3D::setWhiteBackground(bool enabled)
{
    if (m_whiteBackground == enabled)
        return;
    m_whiteBackground = enabled;
#ifdef FLATLAS_HAS_QT3D
    if (m_window && m_window->defaultFrameGraph())
        m_window->defaultFrameGraph()->setClearColor(enabled ? QColor(230, 230, 230) : QColor(6, 10, 18));
    // Material colours are baked at scene build time, so rebuild when a model is loaded.
    if (m_hasModel && m_currentModel)
        rebuildScene(*m_currentModel);
#endif
    if (!m_hasModel)
        setStatusMessage(tr("No model loaded."));
}

void ModelViewport3D::setTexturesVisible(bool visible)
{
    if (m_texturesVisible == visible)
        return;
    m_texturesVisible = visible;
#ifdef FLATLAS_HAS_QT3D
    if (m_hasModel && m_currentModel)
        rebuildScene(*m_currentModel);
#endif
}

void ModelViewport3D::setLightAzimuth(float degrees)
{
#ifdef FLATLAS_HAS_QT3D
    m_lightAzimuth = degrees;
    updateLightPosition();
#else
    Q_UNUSED(degrees);
#endif
}

void ModelViewport3D::setLightIntensity(float intensity)
{
#ifdef FLATLAS_HAS_QT3D
    m_lightIntensity = qBound(0.0f, intensity, 5.0f);
    if (m_light)
        m_light->setIntensity(m_lightIntensity);
    if (m_fillLight)
        m_fillLight->setIntensity(m_lightIntensity * 0.25f);
#else
    Q_UNUSED(intensity);
#endif
}

void ModelViewport3D::setLightColor(const QColor &color)
{
#ifdef FLATLAS_HAS_QT3D
    if (!color.isValid())
        return;
    m_lightColor = color;
    if (m_light)
        m_light->setColor(m_lightColor);
    if (m_fillLight)
        m_fillLight->setColor(m_lightColor);
#else
    Q_UNUSED(color);
#endif
}

bool ModelViewport3D::eventFilter(QObject *watched, QEvent *event)
{
#ifdef FLATLAS_HAS_QT3D
    if ((watched == m_container || watched == m_window) && m_orbitCamera) {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
            m_orbitCamera->handleMousePress(static_cast<QMouseEvent *>(event));
            event->accept();
            return true;
        case QEvent::MouseMove:
            m_orbitCamera->handleMouseMove(static_cast<QMouseEvent *>(event));
            event->accept();
            return true;
        case QEvent::MouseButtonRelease:
            m_orbitCamera->handleMouseRelease(static_cast<QMouseEvent *>(event));
            event->accept();
            return true;
        case QEvent::Wheel:
            m_orbitCamera->handleWheel(static_cast<QWheelEvent *>(event));
            event->accept();
            return true;
        default:
            break;
        }
    }
#else
    Q_UNUSED(watched);
    Q_UNUSED(event);
#endif

    return QWidget::eventFilter(watched, event);
}

void ModelViewport3D::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_statusOverlay)
        m_statusOverlay->setGeometry(rect());
}

void ModelViewport3D::setStatusMessage(const QString &text)
{
    if (!m_statusOverlay || !m_statusLabel)
        return;
    m_statusLabel->setText(text);
    m_statusOverlay->setVisible(!text.trimmed().isEmpty());
    m_statusOverlay->raise();
    if (m_statusOverlay->parentWidget())
        m_statusOverlay->setGeometry(rect());
}

} // namespace flatlas::rendering
