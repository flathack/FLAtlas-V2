// rendering/view3d/SceneView3D.cpp – Qt3D-Hauptview (Phase 7)

#include "SceneView3D.h"
#include "domain/SystemDocument.h"
#include "domain/SolarObject.h"

#include <QVBoxLayout>
#include <QLabel>

#ifdef FLATLAS_HAS_QT3D
#include "OrbitCamera.h"
#include "SelectionManager.h"
#include "SkyRenderer.h"

#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QPointLight>
#include <Qt3DRender/QRenderSettings>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DExtras/QCuboidMesh>
#include <Qt3DExtras/QPhongMaterial>
#include <QMouseEvent>
#include <QWheelEvent>
#endif

namespace flatlas::rendering {

SceneView3D::SceneView3D(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

#ifdef FLATLAS_HAS_QT3D
    m_3dWindow = new Qt3DExtras::Qt3DWindow();
    m_container = QWidget::createWindowContainer(m_3dWindow, this);
    layout->addWidget(m_container);

    setupScene();
#else
    auto *placeholder = new QLabel(tr("3D View – Qt3D not available"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    layout->addWidget(placeholder);
#endif
}

SceneView3D::~SceneView3D() = default;

void SceneView3D::loadDocument(flatlas::domain::SystemDocument *doc)
{
    m_document = doc;
    clearScene();

    if (!doc)
        return;

#ifdef FLATLAS_HAS_QT3D
    for (const auto &obj : doc->objects())
        addSolarObject(obj);
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

    // Camera
    m_camera = m_3dWindow->camera();
    m_camera->lens()->setPerspectiveProjection(45.0f, 16.0f / 9.0f, 10.0f, 1000000.0f);

    // Orbit camera controller
    m_orbitCamera = new OrbitCamera(m_camera, this);

    // Forward renderer
    auto *renderer = m_3dWindow->defaultFrameGraph();
    renderer->setClearColor(QColor(5, 5, 15));
    renderer->setFrustumCullingEnabled(true);

    // Light
    auto *lightEntity = new Qt3DCore::QEntity(m_rootEntity);
    m_light = new Qt3DRender::QPointLight(lightEntity);
    m_light->setColor(Qt::white);
    m_light->setIntensity(1.5f);
    auto *lightTransform = new Qt3DCore::QTransform(lightEntity);
    lightTransform->setTranslation(QVector3D(0, 100000, 100000));
    lightEntity->addComponent(m_light);
    lightEntity->addComponent(lightTransform);

    // Sky background
    m_skyRenderer = new SkyRenderer(m_rootEntity);

    // Selection manager
    m_selectionManager = new SelectionManager(this);
    connect(m_selectionManager, &SelectionManager::objectSelected,
            this, &SceneView3D::objectSelected);

    // Objects root
    m_objectsRoot = new Qt3DCore::QEntity(m_rootEntity);

    m_3dWindow->setRootEntity(m_rootEntity);
#endif
}

void SceneView3D::clearScene()
{
#ifdef FLATLAS_HAS_QT3D
    if (m_selectionManager)
        m_selectionManager->clear();

    if (m_objectsRoot) {
        delete m_objectsRoot;
        m_objectsRoot = new Qt3DCore::QEntity(m_rootEntity);
    }
#endif
}

void SceneView3D::addSolarObject(const std::shared_ptr<flatlas::domain::SolarObject> &obj)
{
#ifdef FLATLAS_HAS_QT3D
    if (!obj || !m_objectsRoot)
        return;

    auto *entity = new Qt3DCore::QEntity(m_objectsRoot);

    // Choose mesh based on type
    Qt3DRender::QGeometryRenderer *mesh = nullptr;
    QColor color;

    using Type = flatlas::domain::SolarObject::Type;
    switch (obj->type()) {
    case Type::Sun:
        { auto *s = new Qt3DExtras::QSphereMesh(entity); s->setRadius(2000); mesh = s; }
        color = QColor(255, 200, 50);
        break;
    case Type::Planet:
        { auto *s = new Qt3DExtras::QSphereMesh(entity); s->setRadius(1000); mesh = s; }
        color = QColor(80, 130, 200);
        break;
    case Type::Station:
    case Type::DockingRing:
    case Type::Depot:
        { auto *c = new Qt3DExtras::QCuboidMesh(entity); c->setXExtent(400); c->setYExtent(400); c->setZExtent(400); mesh = c; }
        color = QColor(200, 200, 200);
        break;
    case Type::JumpGate:
    case Type::JumpHole:
        { auto *s = new Qt3DExtras::QSphereMesh(entity); s->setRadius(600); mesh = s; }
        color = QColor(0, 255, 150);
        break;
    case Type::TradeLane:
        { auto *c = new Qt3DExtras::QCuboidMesh(entity); c->setXExtent(200); c->setYExtent(200); c->setZExtent(600); mesh = c; }
        color = QColor(150, 150, 255);
        break;
    default:
        { auto *s = new Qt3DExtras::QSphereMesh(entity); s->setRadius(300); mesh = s; }
        color = QColor(180, 180, 180);
        break;
    }

    auto *material = new Qt3DExtras::QPhongMaterial(entity);
    material->setDiffuse(color);
    material->setAmbient(color.darker(200));

    auto *transform = new Qt3DCore::QTransform(entity);
    // Freelancer uses X/Z horizontal, Y vertical — map directly
    transform->setTranslation(obj->position());

    entity->addComponent(mesh);
    entity->addComponent(material);
    entity->addComponent(transform);

    m_selectionManager->registerEntity(obj->nickname(), entity, material);
#else
    Q_UNUSED(obj);
#endif
}

#ifdef FLATLAS_HAS_QT3D
void SceneView3D::mousePressEvent(QMouseEvent *event)
{
    if (m_orbitCamera)
        m_orbitCamera->handleMousePress(event);
    QWidget::mousePressEvent(event);
}

void SceneView3D::mouseMoveEvent(QMouseEvent *event)
{
    if (m_orbitCamera)
        m_orbitCamera->handleMouseMove(event);
    QWidget::mouseMoveEvent(event);
}

void SceneView3D::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_orbitCamera)
        m_orbitCamera->handleMouseRelease(event);
    QWidget::mouseReleaseEvent(event);
}

void SceneView3D::wheelEvent(QWheelEvent *event)
{
    if (m_orbitCamera)
        m_orbitCamera->handleWheel(event);
    QWidget::wheelEvent(event);
}
#endif

} // namespace flatlas::rendering
