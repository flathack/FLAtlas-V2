// rendering/preview/CharacterPreview.cpp – Charakter-Vorschau (Phase 16)

#include "CharacterPreview.h"
#include "ModelCache.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QDir>
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

namespace {

int meshCountForNode(const flatlas::infrastructure::ModelNode &node)
{
    int count = node.meshes.size();
    for (const auto &child : node.children)
        count += meshCountForNode(child);
    return count;
}

} // namespace

CharacterPreview::CharacterPreview(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
}

CharacterPreview::~CharacterPreview() = default;

void CharacterPreview::setupUi()
{
    setWindowTitle(tr("Character Preview"));
    resize(700, 600);

    auto *mainLayout = new QVBoxLayout(this);

    // Controls bar
    auto *controlsLayout = new QHBoxLayout();
    controlsLayout->addWidget(new QLabel(tr("Body:"), this));
    m_bodyCombo = new QComboBox(this);
    connect(m_bodyCombo, &QComboBox::currentTextChanged, this, [this](const QString &body) {
        m_currentBody = body;
        rebuildModel();
    });
    controlsLayout->addWidget(m_bodyCombo);

    controlsLayout->addWidget(new QLabel(tr("Head:"), this));
    m_headCombo = new QComboBox(this);
    connect(m_headCombo, &QComboBox::currentTextChanged, this, [this](const QString &head) {
        m_currentHead = head;
        rebuildModel();
    });
    controlsLayout->addWidget(m_headCombo);
    controlsLayout->addStretch();
    mainLayout->addLayout(controlsLayout);

    // 3D view
#ifdef FLATLAS_HAS_QT3D
    m_3dWindow = new Qt3DExtras::Qt3DWindow();
    m_container = QWidget::createWindowContainer(m_3dWindow, this);
    mainLayout->addWidget(m_container, 1);
    setupScene();
#else
    auto *label = new QLabel(tr("Character Preview – Qt3D not available"), this);
    label->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(label, 1);
#endif

    m_statusLabel = new QLabel(this);
    mainLayout->addWidget(m_statusLabel);
}

void CharacterPreview::setupScene()
{
#ifdef FLATLAS_HAS_QT3D
    m_rootEntity = new Qt3DCore::QEntity();

    m_camera = m_3dWindow->camera();
    m_camera->lens()->setPerspectiveProjection(45.0f, 4.0f / 3.0f, 0.1f, 10000.0f);

    m_orbitCamera = new OrbitCamera(m_camera, this);
    m_orbitCamera->setDistance(50.0f);
    m_orbitCamera->setElevation(10.0f);

    auto *renderer = m_3dWindow->defaultFrameGraph();
    renderer->setClearColor(QColor(40, 40, 50));

    auto *lightEntity = new Qt3DCore::QEntity(m_rootEntity);
    auto *light = new Qt3DRender::QPointLight(lightEntity);
    light->setColor(Qt::white);
    light->setIntensity(1.5f);
    auto *lightTransform = new Qt3DCore::QTransform(lightEntity);
    lightTransform->setTranslation(QVector3D(100, 200, 100));
    lightEntity->addComponent(light);
    lightEntity->addComponent(lightTransform);

    m_charRoot = new Qt3DCore::QEntity(m_rootEntity);
    m_3dWindow->setRootEntity(m_rootEntity);
#endif
}

void CharacterPreview::setCharacterDir(const QString &dir)
{
    m_characterDir = dir;
    scanCharacterParts();
}

void CharacterPreview::scanCharacterParts()
{
    m_bodies.clear();
    m_heads.clear();

    QDir dir(m_characterDir);
    if (!dir.exists()) return;

    // Scan for body models (*.3db or *.cmp with "body" in name)
    const auto entries = dir.entryInfoList(
        QStringList{QStringLiteral("*.3db"), QStringLiteral("*.cmp")},
        QDir::Files);
    for (const auto &fi : entries) {
        QString name = fi.completeBaseName().toLower();
        if (name.contains(QStringLiteral("body")) || name.contains(QStringLiteral("torso")))
            m_bodies.append(fi.fileName());
        else if (name.contains(QStringLiteral("head")) || name.contains(QStringLiteral("face")))
            m_heads.append(fi.fileName());
        else
            m_bodies.append(fi.fileName()); // default to body category
    }

    m_bodyCombo->blockSignals(true);
    m_headCombo->blockSignals(true);
    m_bodyCombo->clear();
    m_headCombo->clear();
    m_bodyCombo->addItems(m_bodies);
    m_headCombo->addItem(tr("(none)"));
    m_headCombo->addItems(m_heads);
    m_bodyCombo->blockSignals(false);
    m_headCombo->blockSignals(false);

    m_statusLabel->setText(tr("Found %1 body, %2 head models")
        .arg(m_bodies.size()).arg(m_heads.size()));

    if (!m_bodies.isEmpty()) {
        m_currentBody = m_bodies.first();
        rebuildModel();
    }
}

void CharacterPreview::loadCharacter(const QString &body, const QString &head,
                                     const QString & /*accessory*/)
{
    m_currentBody = body;
    m_currentHead = head;

    // Update combos
    int bi = m_bodyCombo->findText(body);
    if (bi >= 0) m_bodyCombo->setCurrentIndex(bi);
    int hi = m_headCombo->findText(head);
    if (hi >= 0) m_headCombo->setCurrentIndex(hi);

    rebuildModel();
}

void CharacterPreview::rebuildModel()
{
#ifdef FLATLAS_HAS_QT3D
    // Clear existing
    if (m_charRoot) {
        delete m_charRoot;
        m_charRoot = new Qt3DCore::QEntity(m_rootEntity);
    }

    if (m_characterDir.isEmpty()) return;

    auto loadPart = [&](const QString &fileName, const QVector3D &offset) {
        if (fileName.isEmpty()) return;
        QString path = m_characterDir + QStringLiteral("/") + fileName;
        auto decoded = ModelCache::instance().load(path);

        auto *entity = new Qt3DCore::QEntity(m_charRoot);
        auto *transform = new Qt3DCore::QTransform(entity);
        transform->setTranslation(offset);
        entity->addComponent(transform);

        // Placeholder sphere per mesh
        const int meshCount = qMax(1, meshCountForNode(decoded.rootNode));
        for (int i = 0; i < meshCount; ++i) {
            auto *sphere = new Qt3DExtras::QSphereMesh(entity);
            sphere->setRadius(5.0f);
            auto *mat = new Qt3DExtras::QPhongMaterial(entity);
            mat->setDiffuse(QColor(200, 180, 160));
            entity->addComponent(sphere);
            entity->addComponent(mat);
        }
    };

    loadPart(m_currentBody, QVector3D(0, 0, 0));
    if (!m_currentHead.isEmpty() && m_currentHead != tr("(none)"))
        loadPart(m_currentHead, QVector3D(0, 15, 0)); // head above body
#endif

    setWindowTitle(tr("Character Preview – %1").arg(m_currentBody));
}

#ifdef FLATLAS_HAS_QT3D
void CharacterPreview::mousePressEvent(QMouseEvent *event)
{
    if (m_orbitCamera) m_orbitCamera->handleMousePress(event);
    QDialog::mousePressEvent(event);
}

void CharacterPreview::mouseMoveEvent(QMouseEvent *event)
{
    if (m_orbitCamera) m_orbitCamera->handleMouseMove(event);
    QDialog::mouseMoveEvent(event);
}

void CharacterPreview::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_orbitCamera) m_orbitCamera->handleMouseRelease(event);
    QDialog::mouseReleaseEvent(event);
}

void CharacterPreview::wheelEvent(QWheelEvent *event)
{
    if (m_orbitCamera) m_orbitCamera->handleWheel(event);
    QDialog::wheelEvent(event);
}
#endif

} // namespace flatlas::rendering
