// rendering/view3d/ModelViewport3D.cpp - reusable Qt3D model viewport for Freelancer models

#include "ModelViewport3D.h"

#include "rendering/preview/ModelCache.h"
#include "ModelGeometryBuilder.h"
#include "infrastructure/freelancer/FreelancerMaterialResolver.h"

#include <QEvent>
#include <QFileInfo>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QWheelEvent>

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

QColor colorForMeshIndex(int index)
{
    static const QList<QColor> palette{
        QColor(212, 188, 128),
        QColor(178, 196, 210),
        QColor(153, 188, 166),
        QColor(204, 164, 164),
        QColor(166, 166, 202),
        QColor(224, 214, 156),
    };
    return palette.at(index % palette.size());
}

QColor colorForMesh(const flatlas::infrastructure::MeshData &mesh, int fallbackIndex)
{
    if (mesh.materialName.isEmpty())
        return colorForMeshIndex(fallbackIndex);

    const uint hash = qHash(mesh.materialName.toLower());
    const int hue = static_cast<int>(hash % 360u);
    const int sat = 90 + static_cast<int>((hash / 360u) % 80u);
    const int val = 170 + static_cast<int>((hash / (360u * 80u)) % 60u);
    return QColor::fromHsv(hue, qBound(0, sat, 255), qBound(0, val, 255));
}

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
            m_container->installEventFilter(this);
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
    m_orbitCamera->setPanButton(Qt::MiddleButton);
    m_orbitCamera->setResetState(QVector3D(0.0f, 0.0f, 0.0f), 500.0f, 45.0f, 22.0f);
    m_orbitCamera->resetView();

    auto *renderer = m_window->defaultFrameGraph();
    renderer->setClearColor(Qt::black);
    renderer->setFrustumCullingEnabled(true);

    m_lightEntity = new Qt3DCore::QEntity(m_rootEntity);
    m_light = new Qt3DRender::QPointLight(m_lightEntity);
    m_light->setColor(Qt::white);
    m_light->setIntensity(1.5f);
    m_lightTransform = new Qt3DCore::QTransform(m_lightEntity);
    m_lightTransform->setTranslation(QVector3D(900.0f, 1200.0f, 900.0f));
    m_lightEntity->addComponent(m_light);
    m_lightEntity->addComponent(m_lightTransform);

    m_window->setRootEntity(m_rootEntity);
}

void ModelViewport3D::clearSceneEntities()
{
    if (!m_sceneRoot || !m_rootEntity)
        return;

    delete m_sceneRoot;
    m_sceneRoot = new Qt3DCore::QEntity(m_rootEntity);
    m_modelRoot = new Qt3DCore::QEntity(m_sceneRoot);
    m_wireframeRoot = new Qt3DCore::QEntity(m_sceneRoot);
    m_overlayRoot = new Qt3DCore::QEntity(m_sceneRoot);
    m_boundingBoxEntity = nullptr;
}

void ModelViewport3D::addNodeRecursive(const flatlas::infrastructure::ModelNode &node,
                                       Qt3DCore::QEntity *meshParentEntity,
                                       Qt3DCore::QEntity *wireParentEntity)
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
    if (wireParentEntity && m_wireframeVisible) {
        wireNodeEntity = new Qt3DCore::QEntity(wireParentEntity);
        auto *transform = new Qt3DCore::QTransform(wireNodeEntity);
        transform->setTranslation(node.origin);
        transform->setRotation(node.rotation);
        wireNodeEntity->addComponent(transform);
    }

    for (int meshIndex = 0; meshIndex < node.meshes.size(); ++meshIndex) {
        const auto &mesh = node.meshes.at(meshIndex);

        if (meshNodeEntity) {
            auto *meshEntity = new Qt3DCore::QEntity(meshNodeEntity);
            if (auto *renderer = ModelGeometryBuilder::buildTriangleRenderer(mesh, meshEntity)) {
                Qt3DRender::QMaterial *material = nullptr;
                const QImage texture = flatlas::infrastructure::FreelancerMaterialResolver::loadTextureForMesh(m_filePath, mesh);
                if (!texture.isNull())
                    material = MaterialFactory::createFromImage(texture, meshEntity);
                if (!material)
                    material = MaterialFactory::createDefault(colorForMesh(mesh, meshIndex), meshEntity);
                meshEntity->addComponent(renderer);
                meshEntity->addComponent(material);
            } else {
                meshEntity->deleteLater();
            }
        }

        if (wireNodeEntity) {
            auto *wireEntity = new Qt3DCore::QEntity(wireNodeEntity);
            if (auto *renderer = ModelGeometryBuilder::buildWireframeRenderer(mesh, wireEntity)) {
                auto *material = MaterialFactory::createDefault(QColor(235, 235, 235), wireEntity);
                wireEntity->addComponent(renderer);
                wireEntity->addComponent(material);
            } else {
                wireEntity->deleteLater();
            }
        }
    }

    for (const auto &child : node.children)
        addNodeRecursive(child, meshNodeEntity, wireNodeEntity);
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

    m_orbitCamera->setResetState(center, distance, 45.0f, 22.0f);
    m_orbitCamera->resetView();
}
#endif

bool ModelViewport3D::loadModelFile(const QString &filePath, QString *errorMessage)
{
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

    flatlas::infrastructure::DecodedModel decoded;
    try {
        decoded = flatlas::rendering::ModelCache::instance().load(filePath);
    } catch (...) {
        const QString message = tr("An unexpected error occurred while loading %1.")
                                    .arg(QFileInfo(filePath).fileName());
        setStatusMessage(message);
        if (errorMessage)
            *errorMessage = message;
        emit modelLoaded(filePath, false, message);
        return false;
    }

    if (!decoded.isValid()) {
        const QString message = formatLoadError(filePath);
        setStatusMessage(message);
        if (errorMessage)
            *errorMessage = message;
        emit modelLoaded(filePath, false, message);
        return false;
    }

    m_filePath = filePath;
    m_hasModel = true;
    try {
        rebuildScene(decoded.rootNode);
    } catch (...) {
        clearModel();
        const QString message = tr("The model could not be rendered safely: %1.")
                                    .arg(QFileInfo(filePath).fileName());
        setStatusMessage(message);
        if (errorMessage)
            *errorMessage = message;
        emit modelLoaded(filePath, false, message);
        return false;
    }
    setStatusMessage(QString());
    emit modelLoaded(filePath, true, QString());
    return true;
}

void ModelViewport3D::rebuildScene(const flatlas::infrastructure::ModelNode &model)
{
#ifdef FLATLAS_HAS_QT3D
    clearSceneEntities();
    addNodeRecursive(model, m_modelRoot, m_wireframeRoot);

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
    updateVisibilityState();
#else
    Q_UNUSED(model);
#endif
}

void ModelViewport3D::clearModel()
{
    m_filePath.clear();
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

void ModelViewport3D::setWireframeVisible(bool visible)
{
    if (m_wireframeVisible == visible)
        return;
    m_wireframeVisible = visible;
    if (m_hasModel && !m_filePath.isEmpty()) {
        QString ignored;
        loadModelFile(m_filePath, &ignored);
        return;
    }
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
        m_window->defaultFrameGraph()->setClearColor(enabled ? QColor(Qt::white) : QColor(Qt::black));
#endif
    if (!m_hasModel)
        setStatusMessage(m_whiteBackground ? tr("No model loaded.") : tr("No model loaded."));
}

bool ModelViewport3D::eventFilter(QObject *watched, QEvent *event)
{
#ifdef FLATLAS_HAS_QT3D
    if (watched == m_container && m_orbitCamera) {
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
