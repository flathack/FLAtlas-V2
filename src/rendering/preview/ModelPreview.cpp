// rendering/preview/ModelPreview.cpp – 3D-Modell-Vorschau (Phase 16)

#include "ModelPreview.h"
#include "ModelCache.h"
#include "infrastructure/io/CmpLoader.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QFileInfo>

#ifdef FLATLAS_HAS_QT3D
#include "rendering/view3d/OrbitCamera.h"
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QPointLight>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DExtras/QPhongMaterial>
#include <QMouseEvent>
#include <QWheelEvent>
#endif

namespace flatlas::rendering {

ModelPreview::ModelPreview(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
}

ModelPreview::~ModelPreview() = default;

void ModelPreview::setupUi()
{
    setWindowTitle(tr("Model Preview"));
    resize(800, 600);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

#ifdef FLATLAS_HAS_QT3D
    m_3dWindow = new Qt3DExtras::Qt3DWindow();
    m_container = QWidget::createWindowContainer(m_3dWindow, this);
    layout->addWidget(m_container);
    setupScene();
#else
    auto *label = new QLabel(tr("3D Preview – Qt3D not available"), this);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);
#endif
}

void ModelPreview::setupScene()
{
#ifdef FLATLAS_HAS_QT3D
    m_rootEntity = new Qt3DCore::QEntity();

    m_camera = m_3dWindow->camera();
    m_camera->lens()->setPerspectiveProjection(45.0f, 4.0f / 3.0f, 0.1f, 100000.0f);

    m_orbitCamera = new OrbitCamera(m_camera, this);
    m_orbitCamera->setDistance(500.0f);

    auto *renderer = m_3dWindow->defaultFrameGraph();
    renderer->setClearColor(QColor(30, 30, 40));

    // Light
    auto *lightEntity = new Qt3DCore::QEntity(m_rootEntity);
    auto *light = new Qt3DRender::QPointLight(lightEntity);
    light->setColor(Qt::white);
    light->setIntensity(1.5f);
    auto *lightTransform = new Qt3DCore::QTransform(lightEntity);
    lightTransform->setTranslation(QVector3D(500, 500, 500));
    lightEntity->addComponent(light);
    lightEntity->addComponent(lightTransform);

    m_modelRoot = new Qt3DCore::QEntity(m_rootEntity);

    m_3dWindow->setRootEntity(m_rootEntity);
#endif
}

void ModelPreview::loadModel(const QString &filePath)
{
    m_filePath = filePath;
    clearModel();

    auto model = ModelCache::instance().load(filePath);
    m_hasModel = !model.meshes.isEmpty() || !model.children.isEmpty();

    setWindowTitle(tr("Model Preview – %1").arg(QFileInfo(filePath).fileName()));

#ifdef FLATLAS_HAS_QT3D
    if (!m_hasModel || !m_modelRoot)
        return;

    // Build Qt3D entities from ModelNode tree
    std::function<void(const flatlas::infrastructure::ModelNode &, Qt3DCore::QEntity *)> buildNode;
    buildNode = [&](const flatlas::infrastructure::ModelNode &node, Qt3DCore::QEntity *parent) {
        auto *entity = new Qt3DCore::QEntity(parent);

        auto *transform = new Qt3DCore::QTransform(entity);
        transform->setTranslation(node.origin);
        entity->addComponent(transform);

        // Each mesh gets a sphere placeholder (real mesh rendering requires GPU buffers)
        for (const auto &mesh : node.meshes) {
            Q_UNUSED(mesh);
            auto *sphere = new Qt3DExtras::QSphereMesh(entity);
            sphere->setRadius(10.0f);
            auto *mat = new Qt3DExtras::QPhongMaterial(entity);
            mat->setDiffuse(QColor(180, 180, 200));
            entity->addComponent(sphere);
            entity->addComponent(mat);
        }

        for (const auto &child : node.children)
            buildNode(child, entity);
    };

    buildNode(model, m_modelRoot);

    // Fit camera to model bounds
    m_orbitCamera->setDistance(200.0f);
    m_orbitCamera->resetView();
#endif
}

void ModelPreview::clearModel()
{
#ifdef FLATLAS_HAS_QT3D
    if (m_modelRoot) {
        delete m_modelRoot;
        m_modelRoot = new Qt3DCore::QEntity(m_rootEntity);
    }
#endif
    m_hasModel = false;
}

#ifdef FLATLAS_HAS_QT3D
void ModelPreview::mousePressEvent(QMouseEvent *event)
{
    if (m_orbitCamera) m_orbitCamera->handleMousePress(event);
    QDialog::mousePressEvent(event);
}

void ModelPreview::mouseMoveEvent(QMouseEvent *event)
{
    if (m_orbitCamera) m_orbitCamera->handleMouseMove(event);
    QDialog::mouseMoveEvent(event);
}

void ModelPreview::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_orbitCamera) m_orbitCamera->handleMouseRelease(event);
    QDialog::mouseReleaseEvent(event);
}

void ModelPreview::wheelEvent(QWheelEvent *event)
{
    if (m_orbitCamera) m_orbitCamera->handleWheel(event);
    QDialog::wheelEvent(event);
}
#endif

} // namespace flatlas::rendering
