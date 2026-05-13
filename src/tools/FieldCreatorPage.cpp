#include "FieldCreatorPage.h"

#include "core/EditingContext.h"
#include "core/PathUtils.h"
#include "infrastructure/freelancer/FreelancerMaterialResolver.h"
#include "infrastructure/parser/IniParser.h"
#include "rendering/preview/ModelCache.h"
#include "rendering/view3d/FreeCameraController.h"
#include "rendering/view3d/MaterialFactory.h"
#include "rendering/view3d/ModelGeometryBuilder.h"
#include "rendering/view3d/SkyRenderer.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QPushButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#ifdef FLATLAS_HAS_QT3D
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QCuboidMesh>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QPointLight>
#endif

#include <algorithm>
#include <array>
#include <cmath>

namespace flatlas::tools {

namespace {

constexpr double kPi = 3.14159265358979323846;

using flatlas::infrastructure::IniDocument;
using flatlas::infrastructure::IniParser;
using flatlas::infrastructure::IniSection;

class RulerWidget : public QWidget
{
public:
    explicit RulerWidget(Qt::Orientation orientation, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_orientation(orientation)
    {
        if (m_orientation == Qt::Vertical)
            setFixedWidth(42);
        else
            setFixedHeight(28);
    }

    void setLengthMeters(double meters)
    {
        m_lengthMeters = qMax(1.0, meters);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor(10, 14, 20));
        painter.setPen(QColor(130, 160, 190));

        const int majorTicks = 4;
        for (int i = 0; i <= majorTicks; ++i) {
            const double ratio = static_cast<double>(i) / majorTicks;
            const int labelValue = qRound(m_lengthMeters * ratio);
            if (m_orientation == Qt::Vertical) {
                const int y = height() - qRound(ratio * (height() - 8)) - 4;
                painter.drawLine(width() - 12, y, width() - 2, y);
                painter.drawText(QRect(2, y - 8, width() - 16, 16), Qt::AlignRight | Qt::AlignVCenter,
                                 QString::number(labelValue));
            } else {
                const int x = qRound(ratio * (width() - 8)) + 4;
                painter.drawLine(x, 0, x, 10);
                painter.drawText(QRect(x - 28, 11, 56, 14), Qt::AlignCenter, QString::number(labelValue));
            }
        }
        painter.setPen(QColor(190, 210, 230));
        painter.drawText(rect().adjusted(2, 2, -2, -2), m_orientation == Qt::Vertical ? Qt::AlignTop : Qt::AlignRight,
                         QStringLiteral("m"));
    }

private:
    Qt::Orientation m_orientation = Qt::Horizontal;
    double m_lengthMeters = 1.0;
};

QColor parseColorText(const QString &value, const QColor &fallback)
{
    const QStringList parts = value.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (parts.size() != 3)
        return fallback;
    bool okR = false;
    bool okG = false;
    bool okB = false;
    const int r = parts.at(0).trimmed().toInt(&okR);
    const int g = parts.at(1).trimmed().toInt(&okG);
    const int b = parts.at(2).trimmed().toInt(&okB);
    if (!okR || !okG || !okB)
        return fallback;
    return QColor(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255));
}

QString colorText(const QColor &color)
{
    return QStringLiteral("%1, %2, %3").arg(color.red()).arg(color.green()).arg(color.blue());
}

QDoubleSpinBox *makeUnitSpin(QWidget *parent)
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setRange(-1.0, 1.0);
    spin->setDecimals(2);
    spin->setSingleStep(0.05);
    return spin;
}

void addUniqueAsset(QVector<FieldAsset> &assets, const FieldAsset &asset)
{
    if (asset.nickname.trimmed().isEmpty())
        return;
    for (const FieldAsset &existing : std::as_const(assets)) {
        if (existing.nickname.compare(asset.nickname, Qt::CaseInsensitive) == 0)
            return;
    }
    assets.append(asset);
}

void includePoint(flatlas::infrastructure::ModelBounds &bounds, const QVector3D &point)
{
    if (!bounds.valid) {
        bounds.minCorner = point;
        bounds.maxCorner = point;
        bounds.valid = true;
        return;
    }
    bounds.minCorner.setX(qMin(bounds.minCorner.x(), point.x()));
    bounds.minCorner.setY(qMin(bounds.minCorner.y(), point.y()));
    bounds.minCorner.setZ(qMin(bounds.minCorner.z(), point.z()));
    bounds.maxCorner.setX(qMax(bounds.maxCorner.x(), point.x()));
    bounds.maxCorner.setY(qMax(bounds.maxCorner.y(), point.y()));
    bounds.maxCorner.setZ(qMax(bounds.maxCorner.z(), point.z()));
}

void collectModelBounds(const flatlas::infrastructure::ModelNode &node,
                        const QVector3D &parentOffset,
                        flatlas::infrastructure::ModelBounds &bounds)
{
    const QVector3D nodeOffset = parentOffset + node.origin;
    for (const auto &mesh : node.meshes) {
        for (const auto &vertex : mesh.vertices)
            includePoint(bounds, nodeOffset + node.rotation.rotatedVector(vertex.position));
    }
    for (const auto &child : node.children)
        collectModelBounds(child, nodeOffset, bounds);
}

flatlas::infrastructure::ModelBounds modelBounds(const flatlas::infrastructure::ModelNode &node)
{
    flatlas::infrastructure::ModelBounds bounds;
    collectModelBounds(node, QVector3D(), bounds);
    if (bounds.valid)
        bounds.radius = (bounds.maxCorner - bounds.minCorner).length() * 0.5f;
    return bounds;
}

QVector<FieldAsset> fallbackAssets(FieldTemplateKind kind)
{
    QVector<FieldAsset> assets;
    const FieldTemplate preset = FieldTemplateGenerator::preset(kind);
    for (const QString &shape : preset.cubeShapeFallbacks)
        addUniqueAsset(assets, {shape, {}, FieldTemplateGenerator::kindLabel(kind)});
    if (kind == FieldTemplateKind::Nebula)
        addUniqueAsset(assets, {preset.fillShape, {}, QStringLiteral("Nebula fill")});
    else
        addUniqueAsset(assets, {QStringLiteral("DAsteroid_mineable_small1"), {}, QStringLiteral("Dynamic asteroid")});
    return assets;
}

QVector<FieldAsset> scanAssets(const QString &gameRoot, FieldTemplateKind kind)
{
    QVector<FieldAsset> assets = fallbackAssets(kind);
    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    if (dataDir.isEmpty())
        return assets;

    if (kind == FieldTemplateKind::Nebula) {
        const QString nebulaDir = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("SOLAR/NEBULA"));
        if (nebulaDir.isEmpty())
            return assets;
        QDir dir(nebulaDir);
        const QStringList shapeFiles = dir.entryList({QStringLiteral("*_shapes.ini")}, QDir::Files, QDir::Name | QDir::IgnoreCase);
        for (const QString &fileName : shapeFiles) {
            const IniDocument doc = IniParser::parseFile(dir.absoluteFilePath(fileName));
            for (const IniSection &section : doc) {
                if (section.name.compare(QStringLiteral("Texture"), Qt::CaseInsensitive) != 0)
                    continue;
                for (const auto &entry : section.entries) {
                    if (entry.first.compare(QStringLiteral("shape_name"), Qt::CaseInsensitive) == 0
                        || entry.first.compare(QStringLiteral("tex_shape"), Qt::CaseInsensitive) == 0) {
                        addUniqueAsset(assets, {entry.second.trimmed(), {}, fileName});
                    }
                }
            }
        }
        return assets;
    }

    const QString asteroidArch = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("SOLAR/asteroidarch.ini"));
    if (asteroidArch.isEmpty())
        return assets;
    const IniDocument doc = IniParser::parseFile(asteroidArch);
    for (const IniSection &section : doc) {
        const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
        if (nickname.isEmpty())
            continue;
        const QString lower = nickname.toLower();
        bool matches = lower.contains(QStringLiteral("asteroid")) || lower.contains(QStringLiteral("mine"))
            || lower.contains(QStringLiteral("debris")) || lower.contains(QStringLiteral("ice"));
        if (kind == FieldTemplateKind::Mine)
            matches = lower.contains(QStringLiteral("mine"));
        else if (kind == FieldTemplateKind::Debris)
            matches = lower.contains(QStringLiteral("debris"));
        else if (kind == FieldTemplateKind::Ice)
            matches = lower.contains(QStringLiteral("ice"));
        else if (kind == FieldTemplateKind::Gas)
            matches = lower.contains(QStringLiteral("oxygen")) || lower.contains(QStringLiteral("gas"))
                || lower.contains(QStringLiteral("mine"));
        if (!matches)
            continue;
        addUniqueAsset(assets, {nickname, section.value(QStringLiteral("DA_archetype")).trimmed(), QStringLiteral("asteroidarch")});
    }
    return assets;
}

} // namespace

class FieldPreviewWidget : public QWidget
{
public:
    explicit FieldPreviewWidget(QWidget *parent = nullptr, bool compact = false)
        : QWidget(parent)
        , m_compact(compact)
    {
        setMinimumHeight(m_compact ? 180 : 520);
#ifdef FLATLAS_HAS_QT3D
        setupQt3D();
#else
#endif
        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]() {
#ifdef FLATLAS_HAS_QT3D
            if (m_freeCamera)
                m_freeCamera->update(0.033f);
            if (m_skyRenderer && m_camera)
                m_skyRenderer->setCenter(m_camera->position());
#else
            update();
#endif
        });
        timer->start(33);
    }

    void setField(const FieldTemplate &field)
    {
        const bool shouldFrameCamera = m_field.kind != field.kind
            || m_field.cubeSize != field.cubeSize
            || m_field.placedObjects.size() != field.placedObjects.size();
        m_field = field;
#ifdef FLATLAS_HAS_QT3D
        scheduleQt3DRebuild(0);
        if (shouldFrameCamera)
            frameCameraForField();
#endif
        update();
    }

    void setPreviewCubeCount(int count)
    {
        count = qBound(1, count, 5);
        if (m_previewCubeCount == count)
            return;
        m_previewCubeCount = count;
#ifdef FLATLAS_HAS_QT3D
        if (!m_compact)
            scheduleQt3DRebuild(140);
#endif
        update();
    }

    void setCubeWireframesVisible(bool visible)
    {
        if (m_cubeWireframesVisible == visible)
            return;
        m_cubeWireframesVisible = visible;
#ifdef FLATLAS_HAS_QT3D
        if (m_wireRoot)
            m_wireRoot->setEnabled(m_cubeWireframesVisible);
#endif
        update();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
#ifdef FLATLAS_HAS_QT3D
        if (watched == m_container || watched == m_window) {
            if (auto *mouseEvent = dynamic_cast<QMouseEvent *>(event)) {
                switch (event->type()) {
                case QEvent::MouseButtonPress:
                    if (m_container)
                        m_container->setFocus(Qt::MouseFocusReason);
                    if (m_freeCamera)
                        m_freeCamera->handleMousePress(mouseEvent);
                    break;
                case QEvent::MouseMove:
                    if (m_freeCamera)
                        m_freeCamera->handleMouseMove(mouseEvent);
                    break;
                case QEvent::MouseButtonRelease:
                    if (m_freeCamera)
                        m_freeCamera->handleMouseRelease(mouseEvent);
                    break;
                default:
                    break;
                }
                if (mouseEvent->isAccepted())
                    return true;
            }
            if (auto *wheelEvent = dynamic_cast<QWheelEvent *>(event)) {
                if (m_freeCamera)
                    m_freeCamera->handleWheel(wheelEvent);
                if (wheelEvent->isAccepted())
                    return true;
            }
            if (auto *keyEvent = dynamic_cast<QKeyEvent *>(event)) {
                if (event->type() == QEvent::KeyPress && m_freeCamera)
                    m_freeCamera->handleKeyPress(keyEvent);
                if (event->type() == QEvent::KeyRelease && m_freeCamera)
                    m_freeCamera->handleKeyRelease(keyEvent);
                if (keyEvent->isAccepted())
                    return true;
            }
        }
#endif
        return QWidget::eventFilter(watched, event);
    }

    void paintEvent(QPaintEvent *) override
    {
#ifdef FLATLAS_HAS_QT3D
        if (m_container)
            return;
#endif
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor(8, 11, 16));

        QLinearGradient background(rect().topLeft(), rect().bottomRight());
        background.setColorAt(0.0, m_field.kind == FieldTemplateKind::Nebula ? m_field.fogColor.darker(260) : QColor(12, 16, 22));
        background.setColorAt(1.0, QColor(4, 6, 10));
        painter.fillRect(rect(), background);

        const QPointF center(width() * 0.5, height() * 0.5);
        painter.setPen(QPen(QColor(130, 170, 205, 80), 1.0, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center, width() * 0.34, height() * 0.32);

        if (m_field.kind == FieldTemplateKind::Nebula) {
            painter.setPen(Qt::NoPen);
            QColor fog = m_field.fogColor;
            fog.setAlpha(42);
            painter.setBrush(fog);
            painter.drawEllipse(center, width() * 0.38, height() * 0.34);
        }

        QVector<FieldPlacedObject> objects = m_field.placedObjects;
        if (objects.isEmpty()) {
            const QVector<FieldAsset> assets = fallbackAssets(m_field.kind);
            objects = FieldTemplateGenerator::autoDistribute(assets, m_field.kind, 18, 77, QStringLiteral("Volume"));
        }

        std::sort(objects.begin(), objects.end(), [](const auto &left, const auto &right) {
            return left.z < right.z;
        });

        for (const FieldPlacedObject &object : std::as_const(objects)) {
            const double perspective = 1.0 / (1.8 + object.z);
            const QPointF pos(center.x() + object.x * width() * 0.36 * perspective,
                              center.y() + object.y * height() * 0.32 * perspective);
            const double radius = 10.0 * perspective;
            const bool mine = object.mineRole || object.assetNickname.contains(QStringLiteral("mine"), Qt::CaseInsensitive);

            painter.save();
            painter.translate(pos);
            painter.rotate(object.rotateZ);
            QColor fill = mine ? QColor(255, 128, 86, 230) : m_field.primaryColor;
            if (m_field.kind == FieldTemplateKind::Nebula) {
                fill = m_field.primaryColor;
                fill.setAlpha(70);
                painter.setPen(Qt::NoPen);
                painter.setBrush(fill);
                painter.drawEllipse(QPointF(), radius * 3.4, radius * 2.2);
            } else {
                painter.setPen(QPen(QColor(245, 248, 255, 180), 1.2));
                painter.setBrush(fill);
                QPolygonF poly;
                const int sides = mine ? 4 : 6;
                for (int i = 0; i < sides; ++i) {
                    const double a = i * 2.0 * kPi / sides;
                    const double r = radius * (mine && i % 2 ? 1.9 : 1.0);
                    poly << QPointF(std::cos(a) * r, std::sin(a) * r * 0.78);
                }
                painter.drawPolygon(poly);
            }
            painter.restore();

            painter.setPen(QColor(220, 230, 242, 210));
            painter.drawText(pos + QPointF(10, -8), object.assetNickname);
        }

        painter.setPen(QColor(210, 222, 236));
        painter.drawText(QRect(10, 10, width() - 20, 22),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         tr("Static layout fallback preview. Qt3D model rendering is not available."));
    }

private:
#ifdef FLATLAS_HAS_QT3D
    struct PreviewCube {
        QVector3D offset;
        int ix = 0;
        int iy = 0;
        int iz = 0;
    };

    struct PendingPlacedObject {
        FieldPlacedObject object;
        QVector3D cubeOffset;
        QQuaternion cubeRotation;
    };

    void setupQt3D()
    {
        auto *layout = m_compact
            ? static_cast<QLayout *>(new QGridLayout(this))
            : static_cast<QLayout *>(new QVBoxLayout(this));
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        m_window = new Qt3DExtras::Qt3DWindow();
        m_container = QWidget::createWindowContainer(m_window, this);
        m_container->setFocusPolicy(Qt::StrongFocus);
        m_container->setMouseTracking(true);
        m_container->setToolTip(m_compact
                                    ? tr("Selected asset preview")
                                    : tr("Click the preview, then use W/A/S/D, Space/Ctrl and mouse drag to fly through the field."));
        m_container->installEventFilter(this);
        m_window->installEventFilter(this);
        if (m_compact) {
            auto *grid = qobject_cast<QGridLayout *>(layout);
            m_verticalRuler = new RulerWidget(Qt::Vertical, this);
            m_horizontalRuler = new RulerWidget(Qt::Horizontal, this);
            grid->addWidget(m_verticalRuler, 0, 0);
            grid->addWidget(m_container, 0, 1);
            grid->addWidget(m_horizontalRuler, 1, 1);
            grid->setColumnStretch(1, 1);
            grid->setRowStretch(0, 1);
        } else {
            qobject_cast<QVBoxLayout *>(layout)->addWidget(m_container, 1);
        }

        m_rootEntity = new Qt3DCore::QEntity();
        m_sceneRoot = new Qt3DCore::QEntity(m_rootEntity);
        m_camera = m_window->camera();
        m_camera->lens()->setPerspectiveProjection(65.0f, 16.0f / 9.0f, 1.0f, 5000000.0f);
        m_camera->setPosition(QVector3D(0.0f, 220.0f, -1600.0f));
        m_camera->setViewCenter(QVector3D(0.0f, 0.0f, 0.0f));

        m_freeCamera = new flatlas::rendering::FreeCameraController(m_camera, this);
        m_freeCamera->setSpeed(900.0f);
        m_freeCamera->setEnabled(!m_compact);
        m_freeCamera->setPose(QVector3D(0.0f, 220.0f, -1600.0f), QVector3D(0.0f, -0.08f, 1.0f));
        connect(m_freeCamera, &flatlas::rendering::FreeCameraController::cameraChanged, this, [this]() {
            if (m_skyRenderer && m_camera)
                m_skyRenderer->setCenter(m_camera->position());
        });

        auto *renderer = m_window->defaultFrameGraph();
        renderer->setClearColor(QColor(6, 10, 18));
        renderer->setFrustumCullingEnabled(false);

        auto *lightEntity = new Qt3DCore::QEntity(m_rootEntity);
        auto *light = new Qt3DRender::QPointLight(lightEntity);
        light->setColor(Qt::white);
        light->setIntensity(1.6f);
        auto *lightTransform = new Qt3DCore::QTransform(lightEntity);
        lightTransform->setTranslation(QVector3D(0.0f, 14000.0f, -10000.0f));
        lightEntity->addComponent(light);
        lightEntity->addComponent(lightTransform);

        auto *fillLightEntity = new Qt3DCore::QEntity(m_rootEntity);
        auto *fillLight = new Qt3DRender::QPointLight(fillLightEntity);
        fillLight->setColor(QColor(160, 190, 255));
        fillLight->setIntensity(0.6f);
        auto *fillLightTransform = new Qt3DCore::QTransform(fillLightEntity);
        fillLightTransform->setTranslation(QVector3D(-9000.0f, -5000.0f, 9000.0f));
        fillLightEntity->addComponent(fillLight);
        fillLightEntity->addComponent(fillLightTransform);

        m_skyRenderer = new flatlas::rendering::SkyRenderer(m_rootEntity);
        m_skyRenderer->setRadius(m_compact ? 35000.0f : 250000.0f);
        m_skyRenderer->setCenter(m_camera->position());

        m_rebuildTimer = new QTimer(this);
        m_rebuildTimer->setSingleShot(true);
        connect(m_rebuildTimer, &QTimer::timeout, this, &FieldPreviewWidget::rebuildQt3DScene);

        m_batchTimer = new QTimer(this);
        m_batchTimer->setInterval(0);
        connect(m_batchTimer, &QTimer::timeout, this, &FieldPreviewWidget::loadNextObjectBatch);

        m_window->setRootEntity(m_rootEntity);
    }

    QString absoluteModelPathForAsset(const QString &assetNickname) const
    {
        const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
        if (gameRoot.trimmed().isEmpty())
            return {};
        const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
        if (dataDir.isEmpty())
            return {};

        const QString asteroidArch = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("SOLAR/asteroidarch.ini"));
        if (asteroidArch.isEmpty())
            return {};
        const IniDocument doc = IniParser::parseFile(asteroidArch);
        for (const IniSection &section : doc) {
            if (section.value(QStringLiteral("nickname")).trimmed().compare(assetNickname, Qt::CaseInsensitive) != 0)
                continue;
            const QString relativeModel = section.value(QStringLiteral("DA_archetype")).trimmed();
            if (relativeModel.isEmpty())
                return {};
            return flatlas::core::PathUtils::ciResolvePath(dataDir, relativeModel);
        }
        return {};
    }

    void rebuildQt3DScene()
    {
        if (!m_rootEntity)
            return;

        stopPendingBuild();
        if (m_sceneRoot) {
            m_sceneRoot->setEnabled(false);
            m_sceneRoot->deleteLater();
        }
        m_sceneRoot = new Qt3DCore::QEntity(m_rootEntity);
        m_wireRoot = new Qt3DCore::QEntity(m_sceneRoot);
        m_objectRoot = new Qt3DCore::QEntity(m_sceneRoot);
        m_wireRoot->setEnabled(m_cubeWireframesVisible);

        QVector<FieldPlacedObject> objects = m_field.placedObjects;
        if (objects.isEmpty()) {
            if (m_compact) {
                updateRulers(QVector3D());
                return;
            }
            QVector<FieldAsset> assets;
            for (const QString &shape : m_field.cubeShapeFallbacks)
                assets.append({shape, {}, QStringLiteral("preset")});
            objects = FieldTemplateGenerator::autoDistribute(assets, m_field.kind, 18, 77, QStringLiteral("Volume"));
        }

        if (m_compact) {
            for (const FieldPlacedObject &object : std::as_const(objects))
                addPlacedObject(object, QVector3D());
            return;
        }

        const QVector<PreviewCube> cubes = repeatedCubes();
        if (m_cubeWireframesVisible) {
            for (const PreviewCube &cube : cubes)
                addCubeWireBox(cube.offset);
        }

        const QVector<PreviewCube> objectCubes = objectCubesForPreview(cubes, objects.size());
        for (const PreviewCube &cube : objectCubes) {
            const QQuaternion cubeRotation = rotationForCube(cube);
            for (const FieldPlacedObject &object : std::as_const(objects))
                m_pendingObjects.append({object, cube.offset, cubeRotation});
        }
        loadNextObjectBatch();
    }

    void scheduleQt3DRebuild(int delayMs)
    {
        if (!m_rebuildTimer)
            return;
        stopPendingBuild();
        m_rebuildTimer->start(delayMs);
    }

    void stopPendingBuild()
    {
        m_pendingObjects.clear();
        if (m_batchTimer)
            m_batchTimer->stop();
    }

    void loadNextObjectBatch()
    {
        if (!m_objectRoot || m_pendingObjects.isEmpty()) {
            if (m_batchTimer)
                m_batchTimer->stop();
            return;
        }

        const int batchSize = qMax(1, m_previewCubeCount <= 2 ? 10 : 4);
        for (int i = 0; i < batchSize && !m_pendingObjects.isEmpty(); ++i) {
            const PendingPlacedObject next = m_pendingObjects.takeFirst();
            addPlacedObject(next.object, next.cubeOffset, next.cubeRotation);
        }
        if (!m_pendingObjects.isEmpty() && m_batchTimer && !m_batchTimer->isActive())
            m_batchTimer->start();
    }

    QVector<PreviewCube> repeatedCubes() const
    {
        QVector<PreviewCube> cubes;
        const double cube = previewCubeScale();
        const int count = qBound(1, m_previewCubeCount, 5);
        cubes.reserve(count * count * count);
        for (int ix = 0; ix < count; ++ix) {
            for (int iy = 0; iy < count; ++iy) {
                for (int iz = 0; iz < count; ++iz) {
                    cubes.append({QVector3D(repeatPosition(ix, count, cube),
                                            repeatPosition(iy, count, cube),
                                            repeatPosition(iz, count, cube)),
                                  ix, iy, iz});
                }
            }
        }
        if (cubes.isEmpty())
            return {{QVector3D(), 0, 0, 0}};

        std::sort(cubes.begin(), cubes.end(), [](const PreviewCube &left, const PreviewCube &right) {
            return left.offset.lengthSquared() < right.offset.lengthSquared();
        });
        return cubes;
    }

    QVector<PreviewCube> objectCubesForPreview(QVector<PreviewCube> cubes, int objectCount) const
    {
        if (m_previewCubeCount > 1) {
            QVector<PreviewCube> filledCubes;
            const double emptyFrequency = qBound(0.0, m_field.emptyCubeFrequency, 1.0);
            filledCubes.reserve(cubes.size());
            for (const PreviewCube &cube : std::as_const(cubes)) {
                if (cubeEmptyRoll(cube) >= emptyFrequency)
                    filledCubes.append(cube);
            }
            if (!filledCubes.isEmpty())
                cubes = filledCubes;
        }

        constexpr int maxPreviewObjects = 144;
        const int safeObjectCount = qMax(1, objectCount);
        const int maxOffsets = qMax(1, maxPreviewObjects / safeObjectCount);
        if (cubes.size() > maxOffsets)
            cubes.resize(maxOffsets);
        return cubes;
    }

    float repeatPosition(int index, int count, double cubeSize) const
    {
        if (count <= 1 || cubeSize <= 0.0)
            return 0.0f;
        const double centeredIndex = static_cast<double>(index) - (static_cast<double>(count - 1) * 0.5);
        return static_cast<float>(centeredIndex * cubeSize);
    }

    quint32 stableTextHash(const QString &text) const
    {
        quint32 hash = 2166136261u;
        const QByteArray bytes = text.toUtf8();
        for (char byte : bytes) {
            hash ^= static_cast<quint8>(byte);
            hash *= 16777619u;
        }
        return hash;
    }

    quint32 stableCubeHash(const PreviewCube &cube, quint32 salt) const
    {
        quint32 hash = stableTextHash(m_field.fileName.toLower()) ^ (salt * 16777619u);
        const auto mix = [&hash](int value) {
            hash ^= static_cast<quint32>(value + 32768);
            hash *= 16777619u;
        };
        mix(cube.ix);
        mix(cube.iy);
        mix(cube.iz);
        return hash;
    }

    double cubeEmptyRoll(const PreviewCube &cube) const
    {
        return static_cast<double>(stableCubeHash(cube, 11u) % 10000u) / 10000.0;
    }

    int cubeRotationVariant(const PreviewCube &cube) const
    {
        int count = 0;
        for (const QVector<int> &values : {m_field.cubeXAxisRotations, m_field.cubeYAxisRotations, m_field.cubeZAxisRotations}) {
            if (!values.isEmpty())
                count = count == 0 ? values.size() : qMin(count, values.size());
        }
        if (count <= 0)
            return -1;
        return static_cast<int>(stableCubeHash(cube, 23u) % static_cast<quint32>(count));
    }

    int rotationValueAt(const QVector<int> &values, int index) const
    {
        if (values.isEmpty() || index < 0)
            return 0;
        return values.at(qBound(0, index, values.size() - 1));
    }

    QQuaternion rotationForCube(const PreviewCube &cube) const
    {
        const int variant = cubeRotationVariant(cube);
        const int x = rotationValueAt(m_field.cubeXAxisRotations, variant);
        const int y = rotationValueAt(m_field.cubeYAxisRotations, variant);
        const int z = rotationValueAt(m_field.cubeZAxisRotations, variant);
        return QQuaternion::fromEulerAngles(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    }

    void addCubeWireBox(const QVector3D &cubeOffset)
    {
        const float half = static_cast<float>(previewCubeScale() * 0.5);
        flatlas::rendering::ModelBounds bounds;
        bounds.include(QVector3D(-half, -half, -half));
        bounds.include(QVector3D(half, half, half));

        auto *entity = new Qt3DCore::QEntity(m_wireRoot ? m_wireRoot : m_sceneRoot);
        auto *transform = new Qt3DCore::QTransform(entity);
        transform->setTranslation(cubeOffset);
        entity->addComponent(transform);

        auto *renderer = flatlas::rendering::ModelGeometryBuilder::buildBoundingBoxRenderer(bounds, entity);
        if (!renderer) {
            entity->deleteLater();
            return;
        }
        auto *material = flatlas::rendering::MaterialFactory::createDefault(QColor(110, 180, 235), entity);
        entity->addComponent(renderer);
        entity->addComponent(material);
    }

    void addPlacedObject(const FieldPlacedObject &object, const QVector3D &cubeOffset, const QQuaternion &cubeRotation = {})
    {
        const QString modelPath = absoluteModelPathForAsset(object.assetNickname);
        auto *host = new Qt3DCore::QEntity(m_objectRoot ? m_objectRoot : m_sceneRoot);
        auto *hostTransform = new Qt3DCore::QTransform(host);
        const double cubeScale = previewCubeScale();
        const QVector3D localPosition(static_cast<float>(object.x * cubeScale),
                                      static_cast<float>(object.y * cubeScale),
                                      static_cast<float>(object.z * cubeScale));
        const QQuaternion objectRotation = QQuaternion::fromEulerAngles(static_cast<float>(object.rotateX),
                                                                        static_cast<float>(object.rotateY),
                                                                        static_cast<float>(object.rotateZ));
        hostTransform->setTranslation(cubeOffset + cubeRotation.rotatedVector(localPosition));
        hostTransform->setRotation(cubeRotation * objectRotation);
        host->addComponent(hostTransform);

        if (!modelPath.isEmpty()) {
            try {
                const auto decoded = flatlas::rendering::ModelCache::instance().load(modelPath);
                Qt3DCore::QEntity *modelParent = host;
                if (m_compact) {
                    const flatlas::infrastructure::ModelBounds bounds = modelBounds(decoded.rootNode);
                    if (bounds.valid) {
                        modelParent = new Qt3DCore::QEntity(host);
                        auto *fitTransform = new Qt3DCore::QTransform(modelParent);
                        const QVector3D size = bounds.maxCorner - bounds.minCorner;
                        const float largestAxis = qMax(size.x(), qMax(size.y(), size.z()));
                        const float targetSize = static_cast<float>(previewCubeScale() * 1.35);
                        const float fitScale = largestAxis > 0.001f ? targetSize / largestAxis : 1.0f;
                        const QVector3D center = (bounds.minCorner + bounds.maxCorner) * 0.5f;
                        fitTransform->setScale(fitScale);
                        fitTransform->setTranslation(-center * fitScale);
                        modelParent->addComponent(fitTransform);
                        updateRulers(size * fitScale);
                    }
                }
                if (addModelNode(decoded.rootNode, modelParent, modelPath, 0) > 0)
                    return;
            } catch (...) {
            }
        }

        if (m_compact)
            updateRulers(QVector3D(previewCubeScale(), previewCubeScale(), previewCubeScale()));
        addFallbackMesh(object, host);
    }

    int addModelNode(const flatlas::infrastructure::ModelNode &node,
                     Qt3DCore::QEntity *parent,
                     const QString &modelPath,
                     int depth)
    {
        auto *nodeEntity = new Qt3DCore::QEntity(parent);
        auto *transform = new Qt3DCore::QTransform(nodeEntity);
        transform->setTranslation(node.origin);
        transform->setRotation(node.rotation);
        nodeEntity->addComponent(transform);

        int visibleMeshCount = 0;
        for (int meshIndex = 0; meshIndex < node.meshes.size(); ++meshIndex) {
            const auto &mesh = node.meshes.at(meshIndex);
            auto *meshEntity = new Qt3DCore::QEntity(nodeEntity);
            auto *renderer = flatlas::rendering::ModelGeometryBuilder::buildTriangleRenderer(mesh, meshEntity);
            if (!renderer) {
                meshEntity->deleteLater();
                continue;
            }
            const QColor color = m_field.primaryColor.isValid()
                ? m_field.primaryColor.lighter(105 + ((depth + meshIndex) % 3) * 8)
                : QColor::fromHsv((depth * 47 + meshIndex * 31) % 360, 90, 205);
            Qt3DRender::QMaterial *material = nullptr;
            if (!modelPath.isEmpty()) {
                const QImage texture = flatlas::infrastructure::FreelancerMaterialResolver::loadTextureForMesh(modelPath, mesh);
                if (!texture.isNull())
                    material = flatlas::rendering::MaterialFactory::createFromImage(texture, meshEntity);
            }
            if (!material)
                material = flatlas::rendering::MaterialFactory::createDefault(color, meshEntity);
            meshEntity->addComponent(renderer);
            meshEntity->addComponent(material);
            ++visibleMeshCount;
        }

        for (const auto &child : node.children)
            visibleMeshCount += addModelNode(child, nodeEntity, modelPath, depth + 1);
        return visibleMeshCount;
    }

    void addFallbackMesh(const FieldPlacedObject &object, Qt3DCore::QEntity *parent)
    {
        const float fallbackRadius = qBound(35.0f, static_cast<float>(previewCubeScale() * 0.18), 180.0f);
        auto *mesh = object.mineRole
            ? static_cast<Qt3DRender::QGeometryRenderer *>(new Qt3DExtras::QCuboidMesh(parent))
            : static_cast<Qt3DRender::QGeometryRenderer *>(new Qt3DExtras::QSphereMesh(parent));
        if (auto *sphere = qobject_cast<Qt3DExtras::QSphereMesh *>(mesh)) {
            sphere->setRadius(fallbackRadius);
            sphere->setRings(12);
            sphere->setSlices(18);
        }
        if (auto *cube = qobject_cast<Qt3DExtras::QCuboidMesh *>(mesh)) {
            cube->setXExtent(fallbackRadius * 0.85f);
            cube->setYExtent(fallbackRadius * 0.85f);
            cube->setZExtent(fallbackRadius * 2.0f);
        }
        auto *material = flatlas::rendering::MaterialFactory::createDefault(
            object.mineRole ? QColor(240, 100, 70) : m_field.primaryColor, parent);
        parent->addComponent(mesh);
        parent->addComponent(material);
    }

    double previewCubeScale() const
    {
        return qBound(180.0, static_cast<double>(m_field.cubeSize > 0 ? m_field.cubeSize : 400), 2200.0);
    }

    void frameCameraForField()
    {
        if (!m_freeCamera)
            return;
        const float scale = static_cast<float>(previewCubeScale());
        const float fieldDepth = m_compact ? scale : static_cast<float>(qMax(m_field.zoneSizeZ, static_cast<int>(scale)));
        const float fieldHeight = m_compact ? scale : static_cast<float>(qMax(m_field.zoneSizeY, static_cast<int>(scale)));
        const QVector3D position(0.0f, fieldHeight * 0.35f, -fieldDepth * 0.85f - scale * 3.0f);
        m_freeCamera->setPose(position, QVector3D(0.0f, -0.12f, 1.0f));
        m_freeCamera->setSpeed(qBound(180.0f, scale * 1.8f, 9000.0f));
        if (m_skyRenderer && m_camera)
            m_skyRenderer->setCenter(m_camera->position());
    }

    void updateRulers(const QVector3D &size)
    {
        if (!m_compact)
            return;
        const double widthMeters = qMax(1.0, static_cast<double>(qMax(size.x(), size.z())));
        const double heightMeters = qMax(1.0, static_cast<double>(size.y()));
        if (m_horizontalRuler)
            m_horizontalRuler->setLengthMeters(widthMeters);
        if (m_verticalRuler)
            m_verticalRuler->setLengthMeters(heightMeters);
    }

    Qt3DExtras::Qt3DWindow *m_window = nullptr;
    QWidget *m_container = nullptr;
    Qt3DCore::QEntity *m_rootEntity = nullptr;
    Qt3DCore::QEntity *m_sceneRoot = nullptr;
    Qt3DCore::QEntity *m_wireRoot = nullptr;
    Qt3DCore::QEntity *m_objectRoot = nullptr;
    Qt3DRender::QCamera *m_camera = nullptr;
    flatlas::rendering::FreeCameraController *m_freeCamera = nullptr;
    flatlas::rendering::SkyRenderer *m_skyRenderer = nullptr;
    QTimer *m_rebuildTimer = nullptr;
    QTimer *m_batchTimer = nullptr;
    QVector<PendingPlacedObject> m_pendingObjects;
#endif

    RulerWidget *m_horizontalRuler = nullptr;
    RulerWidget *m_verticalRuler = nullptr;
    bool m_compact = false;
    int m_previewCubeCount = 1;
    bool m_cubeWireframesVisible = true;
    FieldTemplate m_field;
};

FieldCreatorPage::FieldCreatorPage(QWidget *parent)
    : QWidget(parent)
    , m_presets(FieldTemplateGenerator::presets())
{
    m_template = FieldTemplateGenerator::preset(FieldTemplateKind::Asteroid);
    buildUi();
    loadAssetsForKind();
    applyCurrentPreset();
}

void FieldCreatorPage::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);

    auto *tabs = new QTabWidget(this);
    root->addWidget(tabs, 1);

    auto *editorPage = new QWidget(tabs);
    auto *editorLayout = new QVBoxLayout(editorPage);
    editorLayout->setContentsMargins(0, 0, 0, 0);

    auto *previewPage = new QWidget(tabs);
    auto *previewLayout = new QVBoxLayout(previewPage);
    previewLayout->setContentsMargins(0, 0, 0, 0);

    auto *splitter = new QSplitter(editorPage);
    editorLayout->addWidget(splitter, 1);

    auto *left = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 8, 0);

    auto *templateBox = new QGroupBox(tr("Template"), left);
    auto *templateForm = new QFormLayout(templateBox);
    m_kindCombo = new QComboBox(templateBox);
    const QVector<FieldTemplateKind> kinds = {
        FieldTemplateKind::Asteroid, FieldTemplateKind::Ice, FieldTemplateKind::Debris,
        FieldTemplateKind::Mine, FieldTemplateKind::Gas, FieldTemplateKind::Nebula};
    for (FieldTemplateKind kind : kinds)
        m_kindCombo->addItem(FieldTemplateGenerator::kindLabel(kind), static_cast<int>(kind));
    templateForm->addRow(tr("Field type:"), m_kindCombo);
    m_presetCombo = new QComboBox(templateBox);
    for (const FieldTemplate &preset : std::as_const(m_presets))
        m_presetCombo->addItem(preset.presetName, static_cast<int>(preset.kind));
    templateForm->addRow(tr("Preset:"), m_presetCombo);
    m_fileNameEdit = new QLineEdit(templateBox);
    templateForm->addRow(tr("File name:"), m_fileNameEdit);
    auto *resetPresetButton = new QPushButton(tr("Reset to preset"), templateBox);
    resetPresetButton->setToolTip(tr("Reloads the selected preset and clears the current placed assets."));
    templateForm->addRow(resetPresetButton);
    leftLayout->addWidget(templateBox);

    auto *zoneBox = new QGroupBox(tr("Zone defaults"), left);
    auto *zoneForm = new QGridLayout(zoneBox);
    m_propertyFlagsSpin = new QSpinBox(zoneBox);
    m_propertyFlagsSpin->setRange(0, 999999);
    m_visitSpin = new QSpinBox(zoneBox);
    m_visitSpin->setRange(0, 999999);
    m_damageSpin = new QSpinBox(zoneBox);
    m_damageSpin->setRange(0, 2000000);
    m_zoneSizeXSpin = new QSpinBox(zoneBox);
    m_zoneSizeYSpin = new QSpinBox(zoneBox);
    m_zoneSizeZSpin = new QSpinBox(zoneBox);
    for (QSpinBox *spin : {m_zoneSizeXSpin, m_zoneSizeYSpin, m_zoneSizeZSpin}) {
        spin->setRange(1, 1000000);
        spin->setSingleStep(1000);
    }
    zoneForm->addWidget(new QLabel(tr("Property flags:"), zoneBox), 0, 0);
    zoneForm->addWidget(m_propertyFlagsSpin, 0, 1);
    zoneForm->addWidget(new QLabel(tr("Visit:"), zoneBox), 1, 0);
    zoneForm->addWidget(m_visitSpin, 1, 1);
    zoneForm->addWidget(new QLabel(tr("Damage:"), zoneBox), 2, 0);
    zoneForm->addWidget(m_damageSpin, 2, 1);
    zoneForm->addWidget(new QLabel(tr("Field size X:"), zoneBox), 3, 0);
    zoneForm->addWidget(m_zoneSizeXSpin, 3, 1);
    zoneForm->addWidget(new QLabel(tr("Field size Y:"), zoneBox), 4, 0);
    zoneForm->addWidget(m_zoneSizeYSpin, 4, 1);
    zoneForm->addWidget(new QLabel(tr("Field size Z:"), zoneBox), 5, 0);
    zoneForm->addWidget(m_zoneSizeZSpin, 5, 1);
    leftLayout->addWidget(zoneBox);

    auto *assetBox = new QGroupBox(tr("Asset palette"), left);
    auto *assetLayout = new QVBoxLayout(assetBox);
    m_assetList = new QListWidget(assetBox);
    m_assetList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    assetLayout->addWidget(m_assetList);
    assetLayout->addWidget(new QLabel(tr("Selected asset"), assetBox));
    m_assetPreview = new FieldPreviewWidget(assetBox, true);
    assetLayout->addWidget(m_assetPreview);
    leftLayout->addWidget(assetBox, 1);

    auto *middle = new QWidget(splitter);
    auto *middleLayout = new QVBoxLayout(middle);
    middleLayout->setContentsMargins(8, 0, 8, 0);

    auto *fieldBox = new QGroupBox(tr("Field parameters"), middle);
    auto *fieldForm = new QGridLayout(fieldBox);
    m_texturePanelsEdit = new QLineEdit(fieldBox);
    m_billboardShapeEdit = new QLineEdit(fieldBox);
    m_spacedustEdit = new QLineEdit(fieldBox);
    m_musicEdit = new QLineEdit(fieldBox);
    m_cubeSizeSpin = new QSpinBox(fieldBox);
    m_cubeSizeSpin->setRange(0, 50000);
    m_fillDistanceSpin = new QSpinBox(fieldBox);
    m_fillDistanceSpin->setRange(0, 50000);
    m_emptyCubeSpin = new QDoubleSpinBox(fieldBox);
    m_emptyCubeSpin->setRange(0.0, 1.0);
    m_emptyCubeSpin->setDecimals(2);
    m_emptyCubeSpin->setSingleStep(0.05);
    m_billboardCountSpin = new QSpinBox(fieldBox);
    m_billboardCountSpin->setRange(0, 100000);
    m_dynamicCountSpin = new QSpinBox(fieldBox);
    m_dynamicCountSpin->setRange(0, 100000);
    m_fogDistanceSpin = new QSpinBox(fieldBox);
    m_fogDistanceSpin->setRange(0, 100000);
    m_puffCountSpin = new QSpinBox(fieldBox);
    m_puffCountSpin->setRange(0, 100000);
    m_primaryColorEdit = new QLineEdit(fieldBox);
    m_ambientColorEdit = new QLineEdit(fieldBox);
    m_fogColorEdit = new QLineEdit(fieldBox);

    int row = 0;
    fieldForm->addWidget(new QLabel(tr("Texture panels:"), fieldBox), row, 0);
    fieldForm->addWidget(m_texturePanelsEdit, row++, 1, 1, 3);
    fieldForm->addWidget(new QLabel(tr("Billboard/fill shape:"), fieldBox), row, 0);
    fieldForm->addWidget(m_billboardShapeEdit, row++, 1, 1, 3);
    fieldForm->addWidget(new QLabel(tr("Spacedust:"), fieldBox), row, 0);
    fieldForm->addWidget(m_spacedustEdit, row, 1);
    fieldForm->addWidget(new QLabel(tr("Music:"), fieldBox), row, 2);
    fieldForm->addWidget(m_musicEdit, row++, 3);
    fieldForm->addWidget(new QLabel(tr("Cube size:"), fieldBox), row, 0);
    fieldForm->addWidget(m_cubeSizeSpin, row, 1);
    fieldForm->addWidget(new QLabel(tr("Fill distance:"), fieldBox), row, 2);
    fieldForm->addWidget(m_fillDistanceSpin, row++, 3);
    fieldForm->addWidget(new QLabel(tr("Empty cubes:"), fieldBox), row, 0);
    fieldForm->addWidget(m_emptyCubeSpin, row, 1);
    fieldForm->addWidget(new QLabel(tr("Billboards:"), fieldBox), row, 2);
    fieldForm->addWidget(m_billboardCountSpin, row++, 3);
    fieldForm->addWidget(new QLabel(tr("Dynamic:"), fieldBox), row, 0);
    fieldForm->addWidget(m_dynamicCountSpin, row, 1);
    fieldForm->addWidget(new QLabel(tr("Fog distance:"), fieldBox), row, 2);
    fieldForm->addWidget(m_fogDistanceSpin, row++, 3);
    fieldForm->addWidget(new QLabel(tr("Puffs:"), fieldBox), row, 0);
    fieldForm->addWidget(m_puffCountSpin, row, 1);
    fieldForm->addWidget(new QLabel(tr("Primary color:"), fieldBox), row, 2);
    fieldForm->addWidget(m_primaryColorEdit, row++, 3);
    fieldForm->addWidget(new QLabel(tr("Ambient color:"), fieldBox), row, 0);
    fieldForm->addWidget(m_ambientColorEdit, row, 1);
    fieldForm->addWidget(new QLabel(tr("Fog color:"), fieldBox), row, 2);
    fieldForm->addWidget(m_fogColorEdit, row++, 3);
    middleLayout->addWidget(fieldBox);

    auto *placementBox = new QGroupBox(tr("Placement"), middle);
    auto *placementLayout = new QVBoxLayout(placementBox);
    auto *manualForm = new QGridLayout();
    m_manualAssetEdit = new QLineEdit(placementBox);
    m_xSpin = makeUnitSpin(placementBox);
    m_ySpin = makeUnitSpin(placementBox);
    m_zSpin = makeUnitSpin(placementBox);
    m_rotateXSpin = new QSpinBox(placementBox);
    m_rotateYSpin = new QSpinBox(placementBox);
    m_rotateZSpin = new QSpinBox(placementBox);
    for (QSpinBox *spin : {m_rotateXSpin, m_rotateYSpin, m_rotateZSpin})
        spin->setRange(-360, 360);
    m_mineRoleCheck = new QCheckBox(tr("Mine role"), placementBox);
    auto *addButton = new QPushButton(tr("Add Selected"), placementBox);
    auto *autoButton = new QPushButton(tr("Auto Fill"), placementBox);
    m_autoCountSpin = new QSpinBox(placementBox);
    m_autoCountSpin->setRange(1, 500);
    m_autoCountSpin->setValue(24);
    m_seedSpin = new QSpinBox(placementBox);
    m_seedSpin->setRange(1, 999999);
    m_seedSpin->setValue(1209);
    m_spreadCombo = new QComboBox(placementBox);
    m_spreadCombo->addItems({tr("Volume"), tr("Belt"), tr("Shell")});
    manualForm->addWidget(new QLabel(tr("Asset:"), placementBox), 0, 0);
    manualForm->addWidget(m_manualAssetEdit, 0, 1, 1, 5);
    manualForm->addWidget(new QLabel(tr("X:"), placementBox), 1, 0);
    manualForm->addWidget(m_xSpin, 1, 1);
    manualForm->addWidget(new QLabel(tr("Y:"), placementBox), 1, 2);
    manualForm->addWidget(m_ySpin, 1, 3);
    manualForm->addWidget(new QLabel(tr("Z:"), placementBox), 1, 4);
    manualForm->addWidget(m_zSpin, 1, 5);
    manualForm->addWidget(new QLabel(tr("Rot X:"), placementBox), 2, 0);
    manualForm->addWidget(m_rotateXSpin, 2, 1);
    manualForm->addWidget(new QLabel(tr("Rot Y:"), placementBox), 2, 2);
    manualForm->addWidget(m_rotateYSpin, 2, 3);
    manualForm->addWidget(new QLabel(tr("Rot Z:"), placementBox), 2, 4);
    manualForm->addWidget(m_rotateZSpin, 2, 5);
    manualForm->addWidget(m_mineRoleCheck, 3, 0, 1, 2);
    manualForm->addWidget(addButton, 3, 2);
    manualForm->addWidget(new QLabel(tr("Count:"), placementBox), 3, 3);
    manualForm->addWidget(m_autoCountSpin, 3, 4);
    manualForm->addWidget(autoButton, 3, 5);
    manualForm->addWidget(new QLabel(tr("Seed:"), placementBox), 4, 0);
    manualForm->addWidget(m_seedSpin, 4, 1);
    manualForm->addWidget(new QLabel(tr("Spread:"), placementBox), 4, 2);
    manualForm->addWidget(m_spreadCombo, 4, 3, 1, 3);
    placementLayout->addLayout(manualForm);
    m_placementTable = new QTableWidget(0, 5, placementBox);
    m_placementTable->setHorizontalHeaderLabels({tr("Asset"), tr("X"), tr("Y"), tr("Z"), tr("Rotation")});
    m_placementTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_placementTable->verticalHeader()->hide();
    placementLayout->addWidget(m_placementTable, 1);
    middleLayout->addWidget(placementBox, 1);

    auto *right = new QWidget(splitter);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(8, 0, 0, 0);
    m_linkPreview = new QTextEdit(right);
    m_linkPreview->setReadOnly(true);
    m_linkPreview->setMaximumHeight(150);
    m_iniPreview = new QTextEdit(right);
    m_iniPreview->setReadOnly(true);
    rightLayout->addWidget(new QLabel(tr("System link preview"), right));
    rightLayout->addWidget(m_linkPreview);
    rightLayout->addWidget(new QLabel(tr("Generated field INI"), right));
    rightLayout->addWidget(m_iniPreview, 2);

    splitter->addWidget(left);
    splitter->addWidget(middle);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 1);

    auto *previewControls = new QHBoxLayout();
    previewControls->setContentsMargins(0, 0, 0, 8);
    m_previewControlsToggle = new QPushButton(tr("Hide controls"), previewPage);
    m_previewControlsToggle->setCheckable(true);
    previewControls->addWidget(m_previewControlsToggle);
    previewControls->addWidget(new QLabel(tr("Preview cube grid:"), previewPage));
    auto *decreaseGridButton = new QPushButton(tr("-"), previewPage);
    decreaseGridButton->setFixedWidth(32);
    auto *increaseGridButton = new QPushButton(tr("+"), previewPage);
    increaseGridButton->setFixedWidth(32);
    m_previewCubeSlider = new QSlider(Qt::Horizontal, previewPage);
    m_previewCubeSlider->setRange(1, 5);
    m_previewCubeSlider->setValue(1);
    m_previewCubeSlider->setTickInterval(1);
    m_previewCubeSlider->setTickPosition(QSlider::TicksBelow);
    m_previewCubeSlider->setToolTip(tr("Controls how many adjacent cube boxes are shown per axis in the 3D preview."));
    m_previewCubeValueLabel = new QLabel(QString::number(m_previewCubeSlider->value()), previewPage);
    m_previewCubeValueLabel->setMinimumWidth(18);
    m_previewCubeValueLabel->setAlignment(Qt::AlignCenter);
    previewControls->addWidget(decreaseGridButton);
    previewControls->addWidget(m_previewCubeSlider, 1);
    previewControls->addWidget(increaseGridButton);
    previewControls->addWidget(m_previewCubeValueLabel);
    m_previewWireframesCheck = new QCheckBox(tr("Show cube wireframes"), previewPage);
    m_previewWireframesCheck->setChecked(true);
    m_previewWireframesCheck->setToolTip(tr("Shows or hides the cube boundary lines in the 3D preview."));
    previewControls->addWidget(m_previewWireframesCheck);
    previewLayout->addLayout(previewControls);

    auto *previewBody = new QHBoxLayout();
    previewBody->setContentsMargins(0, 0, 0, 0);
    previewBody->setSpacing(8);
    m_previewHelpPanel = new QWidget(previewPage);
    m_previewHelpPanel->setFixedWidth(220);
    auto *helpLayout = new QVBoxLayout(m_previewHelpPanel);
    helpLayout->setContentsMargins(8, 8, 8, 8);
    auto *helpTitle = new QLabel(tr("Camera controls"), m_previewHelpPanel);
    QFont helpTitleFont = helpTitle->font();
    helpTitleFont.setBold(true);
    helpTitle->setFont(helpTitleFont);
    helpLayout->addWidget(helpTitle);
    helpLayout->addWidget(new QLabel(tr("W/A/S/D: move"), m_previewHelpPanel));
    helpLayout->addWidget(new QLabel(tr("Space/Ctrl: up/down"), m_previewHelpPanel));
    helpLayout->addWidget(new QLabel(tr("Mouse drag: look around"), m_previewHelpPanel));
    helpLayout->addWidget(new QLabel(tr("Mouse wheel: adjust speed"), m_previewHelpPanel));
    helpLayout->addWidget(new QLabel(tr("Click preview first to capture controls."), m_previewHelpPanel));
    helpLayout->addStretch(1);
    previewBody->addWidget(m_previewHelpPanel);

    m_preview = new FieldPreviewWidget(previewPage);
    previewBody->addWidget(m_preview, 1);
    previewLayout->addLayout(previewBody, 1);

    tabs->addTab(editorPage, tr("Template"));
    tabs->addTab(previewPage, tr("3D Preview"));

    auto *bottom = new QHBoxLayout();
    m_statusLabel = new QLabel(tr("Ready"), this);
    auto *loadButton = new QPushButton(tr("Load Template"), this);
    auto *saveButton = new QPushButton(tr("Save Template"), this);
    bottom->addWidget(m_statusLabel, 1);
    bottom->addWidget(loadButton);
    bottom->addWidget(saveButton);
    root->addLayout(bottom);

    const auto changed = [this]() {
        updateTemplateFromUi();
        refreshPreviews();
    };
    for (QLineEdit *edit : {m_fileNameEdit, m_texturePanelsEdit, m_billboardShapeEdit,
                            m_spacedustEdit, m_musicEdit, m_primaryColorEdit, m_ambientColorEdit, m_fogColorEdit}) {
        connect(edit, &QLineEdit::textChanged, this, changed);
    }
    for (QSpinBox *spin : {m_propertyFlagsSpin, m_visitSpin, m_damageSpin, m_zoneSizeXSpin, m_zoneSizeYSpin,
                           m_zoneSizeZSpin, m_cubeSizeSpin, m_fillDistanceSpin,
                           m_billboardCountSpin, m_dynamicCountSpin, m_fogDistanceSpin, m_puffCountSpin}) {
        connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, changed);
    }
    connect(m_emptyCubeSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, changed);
    connect(m_previewControlsToggle, &QPushButton::toggled, this, [this](bool hidden) {
        if (m_previewHelpPanel)
            m_previewHelpPanel->setVisible(!hidden);
        if (m_previewControlsToggle)
            m_previewControlsToggle->setText(hidden ? tr("Show controls") : tr("Hide controls"));
    });
    connect(decreaseGridButton, &QPushButton::clicked, this, [this]() {
        if (m_previewCubeSlider)
            m_previewCubeSlider->setValue(m_previewCubeSlider->value() - 1);
    });
    connect(increaseGridButton, &QPushButton::clicked, this, [this]() {
        if (m_previewCubeSlider)
            m_previewCubeSlider->setValue(m_previewCubeSlider->value() + 1);
    });
    connect(m_previewCubeSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_previewCubeValueLabel)
            m_previewCubeValueLabel->setText(QString::number(value));
        if (m_preview)
            m_preview->setPreviewCubeCount(value);
    });
    connect(m_previewWireframesCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_preview)
            m_preview->setCubeWireframesVisible(checked);
    });
    connect(m_kindCombo, &QComboBox::currentIndexChanged, this, [this]() {
        const FieldTemplateKind kind = currentKind();
        for (int index = 0; index < m_presetCombo->count(); ++index) {
            if (static_cast<FieldTemplateKind>(m_presetCombo->itemData(index).toInt()) == kind) {
                QSignalBlocker blocker(m_presetCombo);
                m_presetCombo->setCurrentIndex(index);
                break;
            }
        }
        loadAssetsForKind();
        applyCurrentPreset(true);
    });
    connect(m_presetCombo, &QComboBox::currentIndexChanged, this, [this]() {
        const int index = m_presetCombo->currentIndex();
        if (index >= 0) {
            const auto presetKind = static_cast<FieldTemplateKind>(m_presetCombo->itemData(index).toInt());
            const int kindIndex = m_kindCombo->findData(static_cast<int>(presetKind));
            if (kindIndex >= 0 && kindIndex != m_kindCombo->currentIndex()) {
                QSignalBlocker blocker(m_kindCombo);
                m_kindCombo->setCurrentIndex(kindIndex);
                loadAssetsForKind();
            }
        }
        applyCurrentPreset(true);
    });
    connect(m_assetList, &QListWidget::itemSelectionChanged, this, [this]() {
        const auto items = m_assetList->selectedItems();
        if (!items.isEmpty()) {
            m_manualAssetEdit->setText(items.first()->text().section(QStringLiteral("  "), 0, 0).trimmed());
            m_mineRoleCheck->setChecked(m_manualAssetEdit->text().contains(QStringLiteral("mine"), Qt::CaseInsensitive)
                                        || currentKind() == FieldTemplateKind::Mine
                                        || currentKind() == FieldTemplateKind::Gas);
        }
        updateAssetPreview();
    });
    connect(addButton, &QPushButton::clicked, this, &FieldCreatorPage::addManualObject);
    connect(autoButton, &QPushButton::clicked, this, &FieldCreatorPage::autoDistributeObjects);
    connect(resetPresetButton, &QPushButton::clicked, this, &FieldCreatorPage::resetCurrentPreset);
    connect(loadButton, &QPushButton::clicked, this, &FieldCreatorPage::loadTemplate);
    connect(saveButton, &QPushButton::clicked, this, &FieldCreatorPage::saveTemplate);
}

void FieldCreatorPage::loadAssetsForKind()
{
    m_assets.clear();
    m_assetList->clear();

    const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
    const QVector<FieldTemplateKind> themes = {
        FieldTemplateKind::Mine,
        FieldTemplateKind::Asteroid,
        FieldTemplateKind::Ice,
        FieldTemplateKind::Debris,
        FieldTemplateKind::Gas,
        FieldTemplateKind::Nebula,
    };

    for (FieldTemplateKind theme : themes) {
        QVector<FieldAsset> assets = scanAssets(gameRoot, theme);
        std::sort(assets.begin(), assets.end(), [](const FieldAsset &left, const FieldAsset &right) {
            return QString::compare(left.nickname, right.nickname, Qt::CaseInsensitive) < 0;
        });

        auto *header = new QListWidgetItem(FieldTemplateGenerator::kindLabel(theme), m_assetList);
        header->setFlags(Qt::NoItemFlags);
        QFont headerFont = header->font();
        headerFont.setBold(true);
        header->setFont(headerFont);
        header->setForeground(QColor(140, 170, 205));

        for (const FieldAsset &asset : std::as_const(assets)) {
            m_assets.append(asset);
            const QString label = asset.modelPath.isEmpty()
                ? QStringLiteral("%1  [%2]").arg(asset.nickname, asset.category)
                : QStringLiteral("%1  [%2]").arg(asset.nickname, asset.modelPath);
            auto *item = new QListWidgetItem(label, m_assetList);
            item->setData(Qt::UserRole, asset.nickname);
            item->setToolTip(asset.modelPath);
        }
    }
    m_assetList->clearSelection();
    updateAssetPreview();
}

void FieldCreatorPage::applyCurrentPreset(bool preservePlacements)
{
    FieldTemplateKind kind = currentKind();
    const QVector<FieldPlacedObject> existingObjects = m_template.placedObjects;
    const int presetIndex = m_presetCombo->currentIndex();
    if (presetIndex >= 0) {
        const auto presetKind = static_cast<FieldTemplateKind>(m_presetCombo->itemData(presetIndex).toInt());
        if (presetKind == kind) {
            for (const FieldTemplate &preset : std::as_const(m_presets)) {
                if (preset.kind == kind) {
                    m_template = preset;
                    break;
                }
            }
        } else {
            m_template = FieldTemplateGenerator::preset(kind);
        }
    } else {
        m_template = FieldTemplateGenerator::preset(kind);
    }

    if (preservePlacements)
        m_template.placedObjects = existingObjects;
    applyTemplateToUi(m_template);
}

void FieldCreatorPage::resetCurrentPreset()
{
    applyCurrentPreset(false);
}

void FieldCreatorPage::applyTemplateToUi(const FieldTemplate &field)
{
    m_template = field;
    const int kindIndex = m_kindCombo->findData(static_cast<int>(m_template.kind));
    const int presetIndex = m_presetCombo->findData(static_cast<int>(m_template.kind));

    const std::array<QSignalBlocker, 23> blockers = {
        QSignalBlocker(m_kindCombo),
        QSignalBlocker(m_presetCombo),
        QSignalBlocker(m_fileNameEdit),
        QSignalBlocker(m_texturePanelsEdit),
        QSignalBlocker(m_billboardShapeEdit),
        QSignalBlocker(m_spacedustEdit),
        QSignalBlocker(m_musicEdit),
        QSignalBlocker(m_propertyFlagsSpin),
        QSignalBlocker(m_visitSpin),
        QSignalBlocker(m_damageSpin),
        QSignalBlocker(m_zoneSizeXSpin),
        QSignalBlocker(m_zoneSizeYSpin),
        QSignalBlocker(m_zoneSizeZSpin),
        QSignalBlocker(m_cubeSizeSpin),
        QSignalBlocker(m_fillDistanceSpin),
        QSignalBlocker(m_emptyCubeSpin),
        QSignalBlocker(m_billboardCountSpin),
        QSignalBlocker(m_dynamicCountSpin),
        QSignalBlocker(m_fogDistanceSpin),
        QSignalBlocker(m_puffCountSpin),
        QSignalBlocker(m_primaryColorEdit),
        QSignalBlocker(m_ambientColorEdit),
        QSignalBlocker(m_fogColorEdit),
    };
    Q_UNUSED(blockers);

    if (kindIndex >= 0)
        m_kindCombo->setCurrentIndex(kindIndex);
    if (presetIndex >= 0)
        m_presetCombo->setCurrentIndex(presetIndex);

    m_fileNameEdit->setText(m_template.fileName);
    m_texturePanelsEdit->setText(m_template.texturePanelsFile);
    m_billboardShapeEdit->setText(m_template.billboardShape.isEmpty() ? m_template.fillShape : m_template.billboardShape);
    m_spacedustEdit->setText(m_template.spacedust);
    m_musicEdit->setText(m_template.music);
    m_propertyFlagsSpin->setValue(m_template.propertyFlags);
    m_visitSpin->setValue(m_template.visit);
    m_damageSpin->setValue(m_template.damage);
    m_zoneSizeXSpin->setValue(m_template.zoneSizeX);
    m_zoneSizeYSpin->setValue(m_template.zoneSizeY);
    m_zoneSizeZSpin->setValue(m_template.zoneSizeZ);
    m_cubeSizeSpin->setValue(m_template.cubeSize);
    m_fillDistanceSpin->setValue(m_template.fillDistance);
    m_emptyCubeSpin->setValue(m_template.emptyCubeFrequency);
    m_billboardCountSpin->setValue(m_template.billboardCount);
    m_dynamicCountSpin->setValue(m_template.dynamicCount);
    m_fogDistanceSpin->setValue(m_template.fogDistance);
    m_puffCountSpin->setValue(m_template.puffCount);
    m_primaryColorEdit->setText(colorText(m_template.primaryColor));
    m_ambientColorEdit->setText(colorText(m_template.ambientColor));
    m_fogColorEdit->setText(colorText(m_template.fogColor));
    loadAssetsForKind();
    refreshPreviews();
}

void FieldCreatorPage::updateTemplateFromUi()
{
    m_template.kind = currentKind();
    m_template.fileName = FieldTemplateGenerator::normalizedFileName(m_fileNameEdit->text(), m_template.kind);
    m_template.zoneNickname = FieldTemplateGenerator::defaultZoneNickname(m_template.kind);
    m_template.texturePanelsFile = m_texturePanelsEdit->text().trimmed();
    m_template.billboardShape = m_billboardShapeEdit->text().trimmed();
    if (m_template.kind == FieldTemplateKind::Nebula) {
        m_template.fillShape = m_template.billboardShape;
        const QVector<FieldAsset> assets = selectedAssets();
        if (!assets.isEmpty())
            m_template.cubeShapeFallbacks.clear();
        for (const FieldAsset &asset : assets)
            m_template.cubeShapeFallbacks.append(asset.nickname);
    }
    m_template.spacedust = m_spacedustEdit->text().trimmed();
    m_template.music = m_musicEdit->text().trimmed();
    m_template.propertyFlags = m_propertyFlagsSpin->value();
    m_template.visit = m_visitSpin->value();
    m_template.damage = m_damageSpin->value();
    m_template.zoneSizeX = m_zoneSizeXSpin->value();
    m_template.zoneSizeY = m_zoneSizeYSpin->value();
    m_template.zoneSizeZ = m_zoneSizeZSpin->value();
    m_template.cubeSize = m_cubeSizeSpin->value();
    m_template.fillDistance = m_fillDistanceSpin->value();
    m_template.emptyCubeFrequency = m_emptyCubeSpin->value();
    m_template.billboardCount = m_billboardCountSpin->value();
    m_template.dynamicCount = m_dynamicCountSpin->value();
    m_template.fogDistance = m_fogDistanceSpin->value();
    m_template.puffCount = m_puffCountSpin->value();
    m_template.primaryColor = parseColorText(m_primaryColorEdit->text(), m_template.primaryColor);
    m_template.ambientColor = parseColorText(m_ambientColorEdit->text(), m_template.ambientColor);
    m_template.fogColor = parseColorText(m_fogColorEdit->text(), m_template.fogColor);
}

void FieldCreatorPage::refreshPreviews()
{
    updateTemplateFromUi();
    m_preview->setField(m_template);
    updateAssetPreview();
    m_linkPreview->setPlainText(FieldTemplateGenerator::generateSystemLinkPreview(m_template));
    m_iniPreview->setPlainText(FieldTemplateGenerator::generateFieldIni(m_template));
    refreshPlacementTable();
}

void FieldCreatorPage::refreshPlacementTable()
{
    m_placementTable->setRowCount(m_template.placedObjects.size());
    for (int row = 0; row < m_template.placedObjects.size(); ++row) {
        const FieldPlacedObject &object = m_template.placedObjects.at(row);
        m_placementTable->setItem(row, 0, new QTableWidgetItem(object.assetNickname));
        m_placementTable->setItem(row, 1, new QTableWidgetItem(QString::number(object.x, 'f', 2)));
        m_placementTable->setItem(row, 2, new QTableWidgetItem(QString::number(object.y, 'f', 2)));
        m_placementTable->setItem(row, 3, new QTableWidgetItem(QString::number(object.z, 'f', 2)));
        m_placementTable->setItem(row, 4, new QTableWidgetItem(QStringLiteral("%1, %2, %3")
                                                                   .arg(object.rotateX)
                                                                   .arg(object.rotateY)
                                                                   .arg(object.rotateZ)));
    }
}

void FieldCreatorPage::addManualObject()
{
    const QVector<FieldAsset> assets = selectedAssets();
    QStringList nicknames;
    for (const FieldAsset &asset : assets)
        nicknames.append(asset.nickname);
    if (nicknames.isEmpty())
        nicknames.append(m_manualAssetEdit->text().trimmed());
    nicknames.removeAll(QString());

    if (nicknames.isEmpty()) {
        QMessageBox::warning(this, tr("Field Creator"), tr("Please select or enter an asset first."));
        return;
    }

    const double spacing = nicknames.size() > 1 ? 1.6 / static_cast<double>(qMax(1, nicknames.size() - 1)) : 0.0;
    for (int index = 0; index < nicknames.size(); ++index) {
        FieldPlacedObject object;
        object.assetNickname = nicknames.at(index);
        object.x = m_xSpin->value() + (nicknames.size() > 1 ? -0.8 + spacing * index : 0.0);
        object.y = m_ySpin->value();
        object.z = m_zSpin->value();
        object.rotateX = m_rotateXSpin->value();
        object.rotateY = m_rotateYSpin->value();
        object.rotateZ = m_rotateZSpin->value();
        object.mineRole = m_mineRoleCheck->isChecked()
            || object.assetNickname.contains(QStringLiteral("mine"), Qt::CaseInsensitive)
            || currentKind() == FieldTemplateKind::Mine
            || currentKind() == FieldTemplateKind::Gas;
        m_template.placedObjects.append(object);
    }
    refreshPreviews();
}

void FieldCreatorPage::autoDistributeObjects()
{
    QVector<FieldAsset> assets;
    int count = m_autoCountSpin->value();
    if (!m_template.placedObjects.isEmpty()) {
        count = m_template.placedObjects.size();
        for (const FieldPlacedObject &object : std::as_const(m_template.placedObjects)) {
            if (!object.assetNickname.trimmed().isEmpty())
                assets.append({object.assetNickname, {}, QStringLiteral("placed")});
        }
    }
    if (assets.isEmpty())
        assets = selectedAssets().isEmpty() ? m_assets : selectedAssets();
    m_template.placedObjects = FieldTemplateGenerator::autoDistribute(
        assets, currentKind(), count, static_cast<quint32>(m_seedSpin->value()), m_spreadCombo->currentText());
    refreshPreviews();
}

void FieldCreatorPage::loadTemplate()
{
    QString startDir;
    const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    if (!dataDir.isEmpty()) {
        startDir = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("SOLAR/ASTEROIDS"));
        if (startDir.isEmpty())
            startDir = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("SOLAR/NEBULA"));
        if (startDir.isEmpty())
            startDir = dataDir;
    }

    const QString path = QFileDialog::getOpenFileName(this,
                                                      tr("Load Field Template"),
                                                      startDir,
                                                      tr("INI files (*.ini);;All files (*.*)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Field Creator"), tr("The template could not be loaded:\n%1").arg(path));
        return;
    }

    QString errorMessage;
    FieldTemplate loaded = FieldTemplateGenerator::parseFieldIni(QFileInfo(path).fileName(),
                                                                 QString::fromUtf8(file.readAll()),
                                                                 &errorMessage);
    if (!errorMessage.isEmpty()) {
        QMessageBox::warning(this, tr("Field Creator"), errorMessage);
        return;
    }

    applyTemplateToUi(loaded);
    m_statusLabel->setText(tr("Loaded: %1").arg(path));
}

void FieldCreatorPage::updateAssetPreview()
{
    if (!m_assetPreview)
        return;

    FieldTemplate preview = FieldTemplateGenerator::preset(currentKind());
    preview.cubeSize = qMax(420, m_template.cubeSize);
    preview.placedObjects.clear();

    QString nickname;
    const auto items = m_assetList ? m_assetList->selectedItems() : QList<QListWidgetItem *>();
    if (!items.isEmpty())
        nickname = items.first()->data(Qt::UserRole).toString().trimmed();
    if (nickname.isEmpty())
        nickname = m_manualAssetEdit ? m_manualAssetEdit->text().trimmed() : QString();
    if (!nickname.isEmpty()) {
        FieldPlacedObject object;
        object.assetNickname = nickname;
        object.mineRole = nickname.contains(QStringLiteral("mine"), Qt::CaseInsensitive)
            || currentKind() == FieldTemplateKind::Mine
            || currentKind() == FieldTemplateKind::Gas;
        preview.placedObjects.append(object);
    }

    m_assetPreview->setField(preview);
}

void FieldCreatorPage::saveTemplate()
{
    refreshPreviews();
    const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    if (dataDir.isEmpty()) {
        QMessageBox::warning(this, tr("Field Creator"), tr("No active Freelancer DATA directory could be resolved."));
        return;
    }

    const QString relativePath = FieldTemplateGenerator::relativeDirectory(m_template.kind)
        + QLatin1Char('\\') + FieldTemplateGenerator::normalizedFileName(m_template.fileName, m_template.kind);
    QString absolutePath = flatlas::core::PathUtils::ciResolvePath(dataDir, relativePath);
    if (absolutePath.isEmpty()) {
        QString normalizedRelative = relativePath;
        normalizedRelative.replace(QLatin1Char('\\'), QLatin1Char('/'));
        absolutePath = QDir(dataDir).absoluteFilePath(normalizedRelative);
    }

    if (QFileInfo::exists(absolutePath)) {
        const auto answer = QMessageBox::question(this, tr("Field Creator"),
                                                  tr("The template already exists:\n%1\n\nOverwrite it?").arg(absolutePath));
        if (answer != QMessageBox::Yes)
            return;
    }

    QDir().mkpath(QFileInfo(absolutePath).absolutePath());
    QSaveFile file(absolutePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Field Creator"), tr("The template could not be written:\n%1").arg(absolutePath));
        return;
    }
    file.write(FieldTemplateGenerator::generateFieldIni(m_template).toUtf8());
    if (!file.commit()) {
        QMessageBox::warning(this, tr("Field Creator"), tr("The template could not be saved:\n%1").arg(absolutePath));
        return;
    }
    m_statusLabel->setText(tr("Saved: %1").arg(absolutePath));
}

QVector<FieldAsset> FieldCreatorPage::selectedAssets() const
{
    QVector<FieldAsset> result;
    const auto items = m_assetList->selectedItems();
    for (QListWidgetItem *item : items) {
        const QString nickname = item->data(Qt::UserRole).toString();
        for (const FieldAsset &asset : m_assets) {
            if (asset.nickname == nickname) {
                result.append(asset);
                break;
            }
        }
    }
    return result;
}

FieldTemplateKind FieldCreatorPage::currentKind() const
{
    return static_cast<FieldTemplateKind>(m_kindCombo->currentData().toInt());
}

} // namespace flatlas::tools
