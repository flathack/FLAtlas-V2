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
#include <Qt3DExtras/QCylinderMesh>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/QPhongAlphaMaterial>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DExtras/QTorusMesh>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QObjectPicker>
#include <Qt3DRender/QPointLight>
#include <Qt3DRender/QPickEvent>
#include <Qt3DRender/QRenderSettings>

#include <QByteArray>
#include <QEvent>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QQuaternion>
#include <QResizeEvent>
#include <QRegularExpression>
#include <QShowEvent>
#include <QTimer>
#include <QWheelEvent>
#include <QtConcurrent/QtConcurrent>

#include <cmath>
#include <functional>
#include <limits>
#endif

namespace flatlas::rendering {

#ifdef FLATLAS_HAS_QT3D
namespace {

constexpr double kFreelancerNavCellWorld = 30000.0;
constexpr int kFreelancerNavCellsPerAxis = 8;
constexpr double kFreelancerReferenceNavMapScale = 1.36;

enum GizmoHandle {
    GizmoNone = 0,
    GizmoMoveX,
    GizmoMoveY,
    GizmoMoveZ,
    GizmoRotateYaw,
    GizmoRotatePitch,
};

class TransformGizmoOverlay final : public QWidget {
public:
    explicit TransformGizmoOverlay(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(136, 136);
        setMouseTracking(true);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setCursor(Qt::ArrowCursor);
        setToolTip(QObject::tr("Transform gizmo: drag colored axes to move the selected object, drag rings to rotate it."));
    }

    std::function<void(int, const QPoint &)> beginDrag;
    std::function<void(const QPoint &)> drag;
    std::function<void()> finishDrag;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QPointF center(width() * 0.5, height() * 0.5);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(8, 12, 20, 180));
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 6, 6);

        auto drawAxis = [&painter, &center](const QPointF &end, const QColor &color) {
            QPen pen(color, 3.0);
            pen.setCapStyle(Qt::RoundCap);
            painter.setPen(pen);
            painter.drawLine(center, end);
            painter.setBrush(color);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(end, 6, 6);
        };

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 210, 75), 3.0));
        painter.drawEllipse(center, 44, 23);
        painter.setPen(QPen(QColor(255, 135, 220), 3.0));
        painter.drawEllipse(center, 23, 44);

        drawAxis(center + QPointF(43, 0), QColor(245, 80, 80));
        drawAxis(center + QPointF(0, -43), QColor(80, 220, 120));
        drawAxis(center + QPointF(-32, 32), QColor(95, 145, 255));

        painter.setPen(QPen(QColor(230, 235, 245), 1.0));
        painter.drawText(QRectF(center.x() + 49, center.y() - 10, 16, 20), Qt::AlignCenter, QStringLiteral("X"));
        painter.drawText(QRectF(center.x() - 8, center.y() - 65, 16, 20), Qt::AlignCenter, QStringLiteral("Y"));
        painter.drawText(QRectF(center.x() - 54, center.y() + 31, 16, 20), Qt::AlignCenter, QStringLiteral("Z"));
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton) {
            event->ignore();
            return;
        }
        m_activeHandle = hitHandle(event->pos());
        if (m_activeHandle == GizmoNone) {
            event->ignore();
            return;
        }
        if (beginDrag)
            beginDrag(m_activeHandle, event->pos());
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_activeHandle == GizmoNone) {
            setCursor(hitHandle(event->pos()) == GizmoNone ? Qt::ArrowCursor : Qt::SizeAllCursor);
            return;
        }
        if (drag)
            drag(event->pos());
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (m_activeHandle != GizmoNone && event->button() == Qt::LeftButton) {
            m_activeHandle = GizmoNone;
            if (finishDrag)
                finishDrag();
            event->accept();
            return;
        }
        event->ignore();
    }

private:
    int hitHandle(const QPoint &pos) const
    {
        const QPointF center(width() * 0.5, height() * 0.5);
        const QPointF point(pos);
        const QPointF local = point - center;
        const double distance = std::hypot(local.x(), local.y());
        if (QLineF(point, center + QPointF(43, 0)).length() <= 12.0)
            return GizmoMoveX;
        if (QLineF(point, center + QPointF(0, -43)).length() <= 12.0)
            return GizmoMoveY;
        if (QLineF(point, center + QPointF(-32, 32)).length() <= 12.0)
            return GizmoMoveZ;

        const double yaw = std::abs((local.x() * local.x()) / (44.0 * 44.0)
                                    + (local.y() * local.y()) / (23.0 * 23.0) - 1.0);
        if (yaw < 0.22)
            return GizmoRotateYaw;
        const double pitch = std::abs((local.x() * local.x()) / (23.0 * 23.0)
                                      + (local.y() * local.y()) / (44.0 * 44.0) - 1.0);
        if (pitch < 0.22)
            return GizmoRotatePitch;
        if (distance <= 10.0)
            return GizmoMoveY;
        return GizmoNone;
    }

    int m_activeHandle = GizmoNone;
};

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

template <typename T>
void appendPod(QByteArray &blob, const T &value)
{
    blob.append(reinterpret_cast<const char *>(&value), static_cast<int>(sizeof(T)));
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

Qt3DRender::QGeometryRenderer *buildPlanetCubeFaceRenderer(float radius,
                                                           const QVector3D &center,
                                                           const QVector3D &uAxis,
                                                           const QVector3D &vAxis,
                                                           Qt3DCore::QNode *owner)
{
    constexpr int subdivisions = 40;
    constexpr int vertexStride = 8 * static_cast<int>(sizeof(float));

    QByteArray vertexBlob;
    vertexBlob.reserve((subdivisions + 1) * (subdivisions + 1) * vertexStride);
    for (int y = 0; y <= subdivisions; ++y) {
        const float v = static_cast<float>(y) / static_cast<float>(subdivisions);
        const float localV = v * 2.0f - 1.0f;
        for (int x = 0; x <= subdivisions; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(subdivisions);
            const float localU = u * 2.0f - 1.0f;
            const QVector3D normal = (center + uAxis * localU + vAxis * localV).normalized();

            appendPod(vertexBlob, normal.x() * radius);
            appendPod(vertexBlob, normal.y() * radius);
            appendPod(vertexBlob, normal.z() * radius);
            appendPod(vertexBlob, normal.x());
            appendPod(vertexBlob, normal.y());
            appendPod(vertexBlob, normal.z());
            appendPod(vertexBlob, u);
            appendPod(vertexBlob, 1.0f - v);
        }
    }

    QByteArray indexBlob;
    indexBlob.reserve(subdivisions * subdivisions * 6 * static_cast<int>(sizeof(quint32)));
    for (int y = 0; y < subdivisions; ++y) {
        for (int x = 0; x < subdivisions; ++x) {
            const quint32 topLeft = static_cast<quint32>(y * (subdivisions + 1) + x);
            const quint32 bottomLeft = static_cast<quint32>((y + 1) * (subdivisions + 1) + x);
            const quint32 topRight = topLeft + 1;
            const quint32 bottomRight = bottomLeft + 1;
            appendPod(indexBlob, topLeft);
            appendPod(indexBlob, topRight);
            appendPod(indexBlob, bottomRight);
            appendPod(indexBlob, topLeft);
            appendPod(indexBlob, bottomRight);
            appendPod(indexBlob, bottomLeft);
        }
    }

    auto *geometry = new Qt3DCore::QGeometry(owner);
    auto *vertexBuffer = new Qt3DCore::QBuffer(geometry);
    vertexBuffer->setData(vertexBlob);

    auto *positionAttr = new Qt3DCore::QAttribute(geometry);
    positionAttr->setName(Qt3DCore::QAttribute::defaultPositionAttributeName());
    positionAttr->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
    positionAttr->setVertexBaseType(Qt3DCore::QAttribute::Float);
    positionAttr->setVertexSize(3);
    positionAttr->setByteStride(vertexStride);
    positionAttr->setCount((subdivisions + 1) * (subdivisions + 1));
    positionAttr->setBuffer(vertexBuffer);
    geometry->addAttribute(positionAttr);

    auto *normalAttr = new Qt3DCore::QAttribute(geometry);
    normalAttr->setName(Qt3DCore::QAttribute::defaultNormalAttributeName());
    normalAttr->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
    normalAttr->setVertexBaseType(Qt3DCore::QAttribute::Float);
    normalAttr->setVertexSize(3);
    normalAttr->setByteStride(vertexStride);
    normalAttr->setByteOffset(3 * static_cast<int>(sizeof(float)));
    normalAttr->setCount((subdivisions + 1) * (subdivisions + 1));
    normalAttr->setBuffer(vertexBuffer);
    geometry->addAttribute(normalAttr);

    auto *uvAttr = new Qt3DCore::QAttribute(geometry);
    uvAttr->setName(Qt3DCore::QAttribute::defaultTextureCoordinateAttributeName());
    uvAttr->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
    uvAttr->setVertexBaseType(Qt3DCore::QAttribute::Float);
    uvAttr->setVertexSize(2);
    uvAttr->setByteStride(vertexStride);
    uvAttr->setByteOffset(6 * static_cast<int>(sizeof(float)));
    uvAttr->setCount((subdivisions + 1) * (subdivisions + 1));
    uvAttr->setBuffer(vertexBuffer);
    geometry->addAttribute(uvAttr);

    auto *indexBuffer = new Qt3DCore::QBuffer(geometry);
    indexBuffer->setData(indexBlob);

    auto *indexAttr = new Qt3DCore::QAttribute(geometry);
    indexAttr->setAttributeType(Qt3DCore::QAttribute::IndexAttribute);
    indexAttr->setVertexBaseType(Qt3DCore::QAttribute::UnsignedInt);
    indexAttr->setCount(subdivisions * subdivisions * 6);
    indexAttr->setBuffer(indexBuffer);
    geometry->addAttribute(indexAttr);

    auto *renderer = new Qt3DRender::QGeometryRenderer(owner);
    renderer->setGeometry(geometry);
    renderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);
    renderer->setVertexCount(subdivisions * subdivisions * 6);
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

Qt3DExtras::QPhongMaterial *gizmoMaterial(const QColor &color, Qt3DCore::QNode *owner)
{
    auto *material = new Qt3DExtras::QPhongMaterial(owner);
    material->setDiffuse(color);
    material->setAmbient(color.darker(135));
    return material;
}

void configureObjectSphereMesh(Qt3DExtras::QSphereMesh *mesh, bool radiusSphere)
{
    if (!mesh)
        return;
    if (radiusSphere) {
        mesh->setRings(64);
        mesh->setSlices(96);
        return;
    }
    mesh->setRings(8);
    mesh->setSlices(12);
}

Qt3DCore::QTransform *createSelectionCircleTransform(float objectRadius, Qt3DCore::QEntity *entity)
{
    auto *transform = new Qt3DCore::QTransform(entity);
    transform->setTranslation(QVector3D(0.0f, -qMax(objectRadius * 1.05f, objectRadius + 120.0f), 0.0f));
    transform->setRotation(QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, 90.0f));
    return transform;
}

bool isPlanetLikeObject(const flatlas::domain::SolarObject &obj)
{
    return obj.type() == flatlas::domain::SolarObject::Planet
        || obj.archetype().contains(QStringLiteral("planet"), Qt::CaseInsensitive);
}

float lightSourceMarkerRadius(const SystemLightSource &source)
{
    if (source.range > 0.0f)
        return qBound(220.0f, source.range * 0.012f, 1100.0f);
    return 520.0f;
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
    m_container->setToolTip(tr("Left click selects, right drag rotates, middle drag pans, mouse wheel zooms."));
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

void SceneView3D::setSystemLightSources(const QVector<SystemLightSource> &lightSources)
{
    m_systemLightSources = lightSources;

#ifdef FLATLAS_HAS_QT3D
    for (Qt3DCore::QEntity *entity : std::as_const(m_systemLightEntities))
        delete entity;
    m_systemLightEntities.clear();

    if (!m_rootEntity)
        return;

    const bool useFallbackLight = m_systemLightSources.isEmpty();
    if (m_defaultLightEntity)
        m_defaultLightEntity->setEnabled(useFallbackLight);
    if (useFallbackLight)
        return;

    for (const SystemLightSource &source : std::as_const(m_systemLightSources)) {
        const QString type = source.type.trimmed().toUpper();
        auto *lightEntity = new Qt3DCore::QEntity(m_rootEntity);
        const QColor color = source.color.isValid() ? source.color : QColor(Qt::white);

        auto *light = new Qt3DRender::QPointLight(lightEntity);
        light->setColor(color);
        light->setIntensity(type == QStringLiteral("DIRECTIONAL") ? 0.45f : 0.75f);
        if (type == QStringLiteral("DIRECTIONAL")) {
            light->setConstantAttenuation(1.0f);
            light->setLinearAttenuation(0.0f);
            light->setQuadraticAttenuation(0.0f);
        } else {
            if (source.attenuation.x() > 0.0f)
                light->setConstantAttenuation(source.attenuation.x());
            if (source.attenuation.y() > 0.0f)
                light->setLinearAttenuation(source.attenuation.y());
            if (source.attenuation.z() > 0.0f)
                light->setQuadraticAttenuation(source.attenuation.z());
            else if (source.range > 0.0f)
                light->setQuadraticAttenuation(1.0f / (source.range * source.range));
        }

        auto *transform = new Qt3DCore::QTransform(lightEntity);
        transform->setTranslation(source.position);
        lightEntity->addComponent(light);
        lightEntity->addComponent(transform);

        auto *markerMesh = new Qt3DExtras::QSphereMesh(lightEntity);
        markerMesh->setRadius(lightSourceMarkerRadius(source));
        markerMesh->setRings(16);
        markerMesh->setSlices(24);
        auto *markerMaterial = new Qt3DExtras::QPhongMaterial(lightEntity);
        markerMaterial->setDiffuse(color.lighter(145));
        markerMaterial->setAmbient(color.lighter(185));
        markerMaterial->setSpecular(QColor(255, 255, 220));
        markerMaterial->setShininess(60.0f);
        lightEntity->addComponent(markerMesh);
        lightEntity->addComponent(markerMaterial);

        m_systemLightEntities.append(lightEntity);
    }
    requestViewportUpdate();
#else
    Q_UNUSED(lightSources);
#endif
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
    if (m_objectCentersByNickname.contains(nickname))
        m_selectionManager->select(nickname);
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

void SceneView3D::setFreeCameraSpeed(float speed)
{
#ifdef FLATLAS_HAS_QT3D
    if (m_freeCamera)
        m_freeCamera->setSpeed(speed);
#else
    Q_UNUSED(speed);
#endif
}

void SceneView3D::setFreeCameraFlightProfile(float normalSpeed, float cruiseSpeed, float cruiseChargeTime)
{
#ifdef FLATLAS_HAS_QT3D
    if (m_freeCamera)
        m_freeCamera->setFreelancerFlightProfile(normalSpeed, cruiseSpeed, cruiseChargeTime);
#else
    Q_UNUSED(normalSpeed);
    Q_UNUSED(cruiseSpeed);
    Q_UNUSED(cruiseChargeTime);
#endif
}

void SceneView3D::setThirdPersonCamera(float fovX, float zNear)
{
#ifdef FLATLAS_HAS_QT3D
    if (m_freeCamera)
        m_freeCamera->setThirdPersonCamera(fovX, zNear);
#else
    Q_UNUSED(fovX);
    Q_UNUSED(zNear);
#endif
}

void SceneView3D::setFlightShipModel(const QString &modelPath)
{
#ifdef FLATLAS_HAS_QT3D
    if (!m_sceneRoot)
        return;

    if (m_flightShipEntity) {
        delete m_flightShipEntity;
        m_flightShipEntity = nullptr;
        m_flightShipTransform = nullptr;
    }

    m_flightShipEntity = new Qt3DCore::QEntity(m_sceneRoot);
    m_flightShipTransform = new Qt3DCore::QTransform(m_flightShipEntity);
    m_flightShipEntity->addComponent(m_flightShipTransform);

    int rendered = 0;
    if (!modelPath.trimmed().isEmpty()) {
        const flatlas::infrastructure::DecodedModel decoded = flatlas::rendering::ModelCache::instance().load(modelPath);
        if (decoded.isValid())
            rendered = addModelNodeRecursive(decoded.rootNode, m_flightShipEntity, QStringLiteral("__flight_ship"), modelPath);
    }

    if (rendered <= 0) {
        auto *mesh = new Qt3DExtras::QCuboidMesh(m_flightShipEntity);
        mesh->setXExtent(80.0f);
        mesh->setYExtent(28.0f);
        mesh->setZExtent(160.0f);
        auto *material = MaterialFactory::createDefault(QColor(70, 170, 230), m_flightShipEntity);
        m_flightShipEntity->addComponent(mesh);
        m_flightShipEntity->addComponent(material);
    }

    m_flightShipEntity->setEnabled(m_flightModeEnabled);
    updateFlightShipTransform();
    requestViewportUpdate();
#else
    Q_UNUSED(modelPath);
#endif
}

void SceneView3D::beginCruise()
{
#ifdef FLATLAS_HAS_QT3D
    if (m_freeCamera)
        m_freeCamera->beginCruise();
#endif
}

void SceneView3D::cancelCruise()
{
#ifdef FLATLAS_HAS_QT3D
    if (m_freeCamera)
        m_freeCamera->cancelCruise();
#endif
}

bool SceneView3D::setFreeCameraStartObject(const QString &nickname)
{
#ifdef FLATLAS_HAS_QT3D
    if (!m_freeCamera)
        return false;
    const QVector3D center = m_objectCentersByNickname.value(nickname, QVector3D());
    if (!m_objectCentersByNickname.contains(nickname) && !nickname.isEmpty())
        return false;
    const QVector3D spawn = center + QVector3D(0.0f, 350.0f, 1200.0f);
    m_freeCamera->setPose(spawn, QVector3D(0.0f, -0.12f, -1.0f));
    updateCameraDependentScene();
    requestViewportUpdate();
    return true;
#else
    Q_UNUSED(nickname);
    return false;
#endif
}

QWidget *SceneView3D::createTransformGizmoWidget(QWidget *parent)
{
#ifdef FLATLAS_HAS_QT3D
    auto *widget = new TransformGizmoOverlay(parent);
    widget->beginDrag = [this](int handle, const QPoint &pos) {
        beginGizmoDrag(handle, pos);
    };
    widget->drag = [this](const QPoint &pos) {
        updateGizmoDrag(pos);
    };
    widget->finishDrag = [this]() {
        finishGizmoDrag();
    };
    return widget;
#else
    Q_UNUSED(parent);
    return nullptr;
#endif
}

void SceneView3D::setTransformGizmoEnabled(bool enabled)
{
    m_transformGizmoEnabled = enabled;
#ifdef FLATLAS_HAS_QT3D
    if (!enabled)
        finishTransformGizmoEdit();
    else
        cancelCameraInteraction();
    updateTransformGizmo();
    requestViewportUpdate();
#endif
}

void SceneView3D::finishTransformGizmoEdit()
{
    m_transformGizmoEnabled = false;
#ifdef FLATLAS_HAS_QT3D
    if (m_activeGizmoHandle != GizmoNone)
        finishGizmoDrag();
    m_activeGizmoHandle = GizmoNone;
    m_gizmoDragNickname.clear();
    m_cameraMouseInteractionActive = false;
    if (m_selectionManager)
        m_selectionManager->setPickingSuppressed(false);
    if (m_orbitCamera)
        m_orbitCamera->cancelMouseInteraction();
    if (m_container && m_container->mouseGrabber() == m_container)
        m_container->releaseMouse();
    if (m_gizmoRoot)
        m_gizmoRoot->setEnabled(false);
    requestViewportUpdate();
#endif
}

void SceneView3D::cancelCameraInteraction()
{
#ifdef FLATLAS_HAS_QT3D
    m_activeGizmoHandle = GizmoNone;
    m_gizmoDragNickname.clear();
    m_cameraMouseInteractionActive = false;
    if (m_selectionManager)
        m_selectionManager->setPickingSuppressed(false);
    if (m_orbitCamera)
        m_orbitCamera->cancelMouseInteraction();
    if (m_container && m_container->mouseGrabber() == m_container)
        m_container->releaseMouse();
#endif
}

void SceneView3D::setFlightModeEnabled(bool enabled)
{
    if (m_flightModeEnabled == enabled)
        return;
    m_flightModeEnabled = enabled;
#ifdef FLATLAS_HAS_QT3D
    if (m_freeCamera)
        m_freeCamera->setFreelancerFlightModeEnabled(enabled);
    setFreeCameraModeEnabled(enabled);
    if (m_flightShipEntity)
        m_flightShipEntity->setEnabled(enabled);
    applyDisplayFilter();
    applyZoneWireframeVisibility();
    updateTransformGizmo();
    requestViewportUpdate();
#endif
}

void SceneView3D::refreshThemeColors()
{
#ifdef FLATLAS_HAS_QT3D
    loadDocument(m_document);
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
        if (m_orbitCamera)
            m_orbitCamera->updateCamera();
        updateCameraDependentScene();
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
    m_orbitCamera->setRotateButton(Qt::RightButton);
    m_orbitCamera->setPanButton(Qt::MiddleButton);
    m_orbitCamera->setResetState(QVector3D(0.0f, 0.0f, 0.0f), 80000.0f, 45.0f, 24.0f);
    m_orbitCamera->resetView();

    auto *renderer = m_3dWindow->defaultFrameGraph();
    renderer->setClearColor(QColor(6, 10, 18, 255));
    renderer->setFrustumCullingEnabled(false);

    m_defaultLightEntity = new Qt3DCore::QEntity(m_rootEntity);
    m_light = new Qt3DRender::QPointLight(m_defaultLightEntity);
    m_light->setColor(Qt::white);
    m_light->setIntensity(1.6f);
    auto *lightTransform = new Qt3DCore::QTransform(m_defaultLightEntity);
    lightTransform->setTranslation(QVector3D(150000.0f, 200000.0f, 150000.0f));
    m_defaultLightEntity->addComponent(m_light);
    m_defaultLightEntity->addComponent(lightTransform);
    setSystemLightSources(m_systemLightSources);

    m_skyRenderer = new SkyRenderer(m_rootEntity);
    m_skyRenderer->setRadius(2500000.0f);
    m_skyRenderer->setCenter(m_camera->position());
    connect(m_orbitCamera, &OrbitCamera::cameraChanged, this, [this]() {
        updateCameraDependentScene();
        syncZoomLevelFromCamera();
    });

    m_freeCamera = new FreeCameraController(m_camera, this);
    connect(m_freeCamera, &FreeCameraController::cameraChanged, this, [this]() {
        updateFlightShipTransform();
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
            this, [this](const QString &nickname) {
        updateSelectionMarker(nickname);
        updateHoverMarker(m_selectionManager ? m_selectionManager->hoveredNickname() : QString());
        updateTransformGizmo();
        emit objectSelected(nickname);
    });
    connect(m_selectionManager, &SelectionManager::objectHovered,
            this, [this](const QString &nickname) {
        updateHoverMarker(nickname);
        requestViewportUpdate();
    });

    m_3dWindow->setRootEntity(m_rootEntity);
#endif
}

void SceneView3D::clearScene()
{
#ifdef FLATLAS_HAS_QT3D
    ++m_loadGeneration;
    if (m_selectionManager)
        m_selectionManager->clear();
    if (m_gizmoRoot) {
        delete m_gizmoRoot;
        m_gizmoRoot = nullptr;
        m_gizmoTransform = nullptr;
    }
    m_modelHostsByNickname.clear();
    m_objectTransformsByNickname.clear();
    m_markerEntitiesByNickname.clear();
    m_markerMaterialsByNickname.clear();
    m_planetSegmentEntitiesByNickname.clear();
    m_selectionMarkerEntitiesByNickname.clear();
    m_hoverMarkerEntitiesByNickname.clear();
    m_ringEntitiesByHostNickname.clear();
    m_atmosphereZoneEntitiesByObjectNickname.clear();
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
        m_flightShipEntity = nullptr;
        m_flightShipTransform = nullptr;
        delete m_sceneRoot;
        m_sceneRoot = new Qt3DCore::QEntity(m_rootEntity);
        m_gridEntity = nullptr;
        m_zonesRoot = new Qt3DCore::QEntity(m_sceneRoot);
        m_objectsRoot = new Qt3DCore::QEntity(m_sceneRoot);
        updateTransformGizmo();
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

void SceneView3D::addAtmosphereZone(const flatlas::domain::SolarObject &obj)
{
#ifdef FLATLAS_HAS_QT3D
    if (!m_zonesRoot)
        return;

    using Type = flatlas::domain::SolarObject::Type;
    const QString archetype = obj.archetype().trimmed().toLower();
    const bool isAtmosphereHost = obj.type() == Type::Planet
        || obj.type() == Type::Sun
        || archetype.contains(QStringLiteral("planet"))
        || archetype.contains(QStringLiteral("sun"))
        || archetype.contains(QStringLiteral("star"));
    if (!isAtmosphereHost)
        return;

    bool ok = false;
    const float atmosphereRange = rawEntryValue(obj, QStringLiteral("atmosphere_range")).toFloat(&ok);
    if (!ok || atmosphereRange <= 0.0f)
        return;

    flatlas::domain::ZoneItem atmosphere;
    atmosphere.setNickname(obj.nickname() + QStringLiteral("::atmosphere"));
    atmosphere.setPosition(obj.position());
    atmosphere.setShape(flatlas::domain::ZoneItem::Sphere);
    atmosphere.setSize(QVector3D(atmosphereRange, atmosphereRange, atmosphereRange));
    atmosphere.setZoneType(QStringLiteral("atmosphere"));

    const ZoneVisualStyle style = ZoneColorScheme::styleForZone(atmosphere);
    const ZoneGeometryBuildResult result = ZoneGeometryBuilder::buildZone(atmosphere, style, m_zonesRoot);
    if (!result.valid)
        return;

    m_atmosphereZoneEntitiesByObjectNickname.insert(obj.nickname(), result.rootEntity);
    if (result.wireEntity) {
        m_zoneWireEntitiesByNickname.insert(atmosphere.nickname(), result.wireEntity);
        result.wireEntity->setEnabled(m_zoneWireframesVisible);
    }
    if (m_sceneBounds)
        m_sceneBounds->include(result.bounds);
    if (m_zoneBounds)
        m_zoneBounds->include(result.bounds);
#else
    Q_UNUSED(obj);
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
    m_objectTransformsByNickname.insert(obj->nickname(), objectTransform);

    auto *modelHost = new Qt3DCore::QEntity(objectEntity);
    m_modelHostsByNickname.insert(obj->nickname(), modelHost);

    const QColor baseColor = objectColor(obj->type());
    const bool radiusSphere = shouldRenderAsRadiusSphere(*obj);
    const bool planetLike = isPlanetLikeObject(*obj);
    const float radius = radiusSphere ? displayRadiusForObject(*obj) : markerRadius(obj->type());
    m_objectCentersByNickname.insert(obj->nickname(), obj->position());
    m_objectRadiiByNickname.insert(obj->nickname(), radius);
    auto *markerEntity = new Qt3DCore::QEntity(objectEntity);
    auto *markerMesh = new Qt3DExtras::QSphereMesh(markerEntity);
    markerMesh->setRadius(radius);
    configureObjectSphereMesh(markerMesh, radiusSphere);
    auto *markerMaterial = new Qt3DExtras::QPhongMaterial(markerEntity);
    markerMaterial->setDiffuse(baseColor);
    markerMaterial->setAmbient(baseColor.darker(175));
    markerEntity->addComponent(markerMesh);
    markerEntity->addComponent(markerMaterial);
    m_markerEntitiesByNickname.insert(obj->nickname(), markerEntity);
    m_markerMaterialsByNickname.insert(obj->nickname(), markerMaterial);

    if (m_selectionManager)
        m_selectionManager->registerEntity(obj->nickname(), markerEntity, markerMaterial);

    auto *selectionMarker = new Qt3DCore::QEntity(objectEntity);
    auto *selectionMesh = new Qt3DExtras::QTorusMesh(selectionMarker);
    selectionMesh->setRadius(qMax(radius * 1.05f, radius + 260.0f));
    selectionMesh->setMinorRadius(qMax(radius * 0.035f, 70.0f));
    selectionMesh->setRings(96);
    selectionMesh->setSlices(8);
    auto *selectionTransform = createSelectionCircleTransform(radius, selectionMarker);
    auto *selectionMaterial = new Qt3DExtras::QPhongAlphaMaterial(selectionMarker);
    selectionMaterial->setDiffuse(QColor(50, 155, 255, 185));
    selectionMaterial->setAmbient(QColor(50, 155, 255, 140));
    selectionMaterial->setAlpha(0.72f);
    MaterialFactory::preventFramebufferAlphaWrites(selectionMaterial);
    selectionMarker->addComponent(selectionMesh);
    selectionMarker->addComponent(selectionTransform);
    selectionMarker->addComponent(selectionMaterial);
    selectionMarker->setEnabled(false);
    m_selectionMarkerEntitiesByNickname.insert(obj->nickname(), selectionMarker);

    auto *hoverMarker = new Qt3DCore::QEntity(objectEntity);
    auto *hoverMesh = new Qt3DExtras::QSphereMesh(hoverMarker);
    hoverMesh->setRadius(qMax(radius * 1.10f, radius + 150.0f));
    hoverMesh->setRings(12);
    hoverMesh->setSlices(18);
    auto *hoverMaterial = new Qt3DExtras::QPhongAlphaMaterial(hoverMarker);
    hoverMaterial->setDiffuse(QColor(80, 220, 255, 72));
    hoverMaterial->setAmbient(QColor(80, 220, 255, 55));
    hoverMaterial->setAlpha(0.28f);
    MaterialFactory::preventFramebufferAlphaWrites(hoverMaterial);
    hoverMarker->addComponent(hoverMesh);
    hoverMarker->addComponent(hoverMaterial);
    hoverMarker->setEnabled(false);
    m_hoverMarkerEntitiesByNickname.insert(obj->nickname(), hoverMarker);

    if (m_sceneBounds) {
        m_sceneBounds->include(obj->position() + QVector3D(radius, radius, radius));
        m_sceneBounds->include(obj->position() - QVector3D(radius, radius, radius));
    }
    if (m_objectBounds) {
        m_objectBounds->include(obj->position() + QVector3D(radius, radius, radius));
        m_objectBounds->include(obj->position() - QVector3D(radius, radius, radius));
    }

    addAtmosphereZone(*obj);

    const QString modelPath = modelPathForObject(*obj);
    if (radiusSphere && planetLike) {
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
        if (Qt3DCore::QEntity *atmosphere = m_atmosphereZoneEntitiesByObjectNickname.value(obj->nickname(), nullptr))
            setEntityTreeEnabled(atmosphere, visible && !m_flightModeEnabled);
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
            setEntityTreeEnabled(entity, !m_flightModeEnabled && zoneVisibleForFilter(m_displayFilterSettings, *zone));
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
            it.value()->setEnabled(m_zoneWireframesVisible && !m_flightModeEnabled);
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
    updateFlightShipTransform();
#endif
}

void SceneView3D::updateCameraDependentScene()
{
#ifdef FLATLAS_HAS_QT3D
    if (m_skyRenderer && m_camera)
        m_skyRenderer->setCenter(m_camera->position());
#endif
}

void SceneView3D::updateFlightShipTransform()
{
#ifdef FLATLAS_HAS_QT3D
    if (!m_flightShipTransform || !m_freeCamera)
        return;

    const QVector3D forward = m_freeCamera->shipForward();
    const QVector3D safeForward = forward.lengthSquared() > 0.0001f ? forward.normalized() : QVector3D(0.0f, 0.0f, -1.0f);
    m_flightShipTransform->setTranslation(m_freeCamera->shipPosition());
    m_flightShipTransform->setRotation(QQuaternion::rotationTo(QVector3D(0.0f, 0.0f, -1.0f), safeForward));
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

flatlas::domain::SolarObject *SceneView3D::solarObjectByNickname(const QString &nickname) const
{
    if (!m_document)
        return nullptr;
    for (const auto &obj : m_document->objects()) {
        if (obj && obj->nickname() == nickname)
            return obj.get();
    }
    return nullptr;
}

void SceneView3D::updateSelectionMarker(const QString &nickname)
{
#ifdef FLATLAS_HAS_QT3D
    for (auto it = m_selectionMarkerEntitiesByNickname.begin();
         it != m_selectionMarkerEntitiesByNickname.end();
         ++it) {
        if (it.value())
            it.value()->setEnabled(it.key() == nickname);
    }
    requestViewportUpdate();
#else
    Q_UNUSED(nickname);
#endif
}

void SceneView3D::updateHoverMarker(const QString &nickname)
{
#ifdef FLATLAS_HAS_QT3D
    const QString selected = m_selectionManager ? m_selectionManager->selectedNickname() : QString();
    for (auto it = m_hoverMarkerEntitiesByNickname.begin();
         it != m_hoverMarkerEntitiesByNickname.end();
         ++it) {
        if (it.value())
            it.value()->setEnabled(it.key() == nickname && it.key() != selected);
    }
    requestViewportUpdate();
#else
    Q_UNUSED(nickname);
#endif
}

void SceneView3D::updateTransformGizmo()
{
#ifdef FLATLAS_HAS_QT3D
    const QString nickname = m_selectionManager ? m_selectionManager->selectedNickname() : QString();
    const bool visible = m_transformGizmoEnabled
        && !m_flightModeEnabled
        && !nickname.isEmpty()
        && m_objectCentersByNickname.contains(nickname)
        && m_sceneRoot;

    if (!visible) {
        if (m_gizmoRoot)
            m_gizmoRoot->setEnabled(false);
        return;
    }

    const bool creatingGizmo = !m_gizmoRoot;
    if (creatingGizmo && m_selectionManager)
        m_selectionManager->setPickingSuppressed(true);

    if (!m_gizmoRoot) {
        m_gizmoRoot = new Qt3DCore::QEntity(m_sceneRoot);
        m_gizmoTransform = new Qt3DCore::QTransform(m_gizmoRoot);
        m_gizmoRoot->addComponent(m_gizmoTransform);

        auto addAxis = [this](const QColor &color,
                              const QVector3D &translation,
                              const QQuaternion &rotation,
                              int handle) {
            auto *entity = new Qt3DCore::QEntity(m_gizmoRoot);
            auto *mesh = new Qt3DExtras::QCylinderMesh(entity);
            mesh->setRadius(260.0f);
            mesh->setLength(7200.0f);
            mesh->setRings(1);
            mesh->setSlices(16);
            auto *transform = new Qt3DCore::QTransform(entity);
            transform->setTranslation(translation);
            transform->setRotation(rotation);
            entity->addComponent(mesh);
            entity->addComponent(transform);
            entity->addComponent(gizmoMaterial(color, entity));
            auto *picker = new Qt3DRender::QObjectPicker(entity);
            picker->setHoverEnabled(false);
            entity->addComponent(picker);
            connect(picker, &Qt3DRender::QObjectPicker::pressed,
                    this, [this, handle](Qt3DRender::QPickEvent *event) {
                if (!m_transformGizmoEnabled)
                    return;
                if (m_selectionManager)
                    m_selectionManager->setPickingSuppressed(true);
                beginGizmoDrag(handle, event ? event->position().toPoint() : QPoint());
            });
        };

        addAxis(QColor(245, 80, 80),
                QVector3D(3600.0f, 0.0f, 0.0f),
                QQuaternion::fromAxisAndAngle(0.0f, 0.0f, 1.0f, 90.0f),
                GizmoMoveX);
        addAxis(QColor(80, 220, 120),
                QVector3D(0.0f, 3600.0f, 0.0f),
                QQuaternion(),
                GizmoMoveY);
        addAxis(QColor(95, 145, 255),
                QVector3D(0.0f, 0.0f, 3600.0f),
                QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, 90.0f),
                GizmoMoveZ);

        auto addRing = [this](const QColor &color, const QQuaternion &rotation, int handle) {
            auto *entity = new Qt3DCore::QEntity(m_gizmoRoot);
            auto *mesh = new Qt3DExtras::QTorusMesh(entity);
            mesh->setRadius(7800.0f);
            mesh->setMinorRadius(170.0f);
            mesh->setRings(64);
            mesh->setSlices(10);
            auto *transform = new Qt3DCore::QTransform(entity);
            transform->setRotation(rotation);
            entity->addComponent(mesh);
            entity->addComponent(transform);
            entity->addComponent(gizmoMaterial(color, entity));
            auto *picker = new Qt3DRender::QObjectPicker(entity);
            picker->setHoverEnabled(false);
            entity->addComponent(picker);
            connect(picker, &Qt3DRender::QObjectPicker::pressed,
                    this, [this, handle](Qt3DRender::QPickEvent *event) {
                if (!m_transformGizmoEnabled)
                    return;
                if (m_selectionManager)
                    m_selectionManager->setPickingSuppressed(true);
                beginGizmoDrag(handle, event ? event->position().toPoint() : QPoint());
            });
        };
        addRing(QColor(255, 210, 75), QQuaternion(), GizmoRotateYaw);
        addRing(QColor(255, 135, 220),
                QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, 90.0f),
                GizmoRotatePitch);
    }

    m_gizmoRoot->setEnabled(true);
    m_gizmoTransform->setTranslation(m_objectCentersByNickname.value(nickname));
    if (creatingGizmo && m_selectionManager) {
        QTimer::singleShot(0, this, [this]() {
            if (m_selectionManager && m_activeGizmoHandle == GizmoNone)
                m_selectionManager->setPickingSuppressed(false);
        });
    }
#endif
}

void SceneView3D::beginGizmoDrag(int handle, const QPoint &screenPos)
{
    if (!m_transformGizmoEnabled || !m_selectionManager || handle == GizmoNone)
        return;
    const QString nickname = m_selectionManager->selectedNickname();
    flatlas::domain::SolarObject *obj = solarObjectByNickname(nickname);
    if (!obj)
        return;

    m_activeGizmoHandle = handle;
    m_gizmoDragNickname = nickname;
    m_gizmoDragStartScreenPos = screenPos;
    m_gizmoDragStartPosition = obj->position();
    m_gizmoDragStartRotation = obj->rotation();
}

void SceneView3D::updateGizmoDrag(const QPoint &screenPos)
{
    if (!m_transformGizmoEnabled) {
        if (m_activeGizmoHandle != GizmoNone)
            finishGizmoDrag();
        return;
    }
    if (m_activeGizmoHandle == GizmoNone)
        return;

    flatlas::domain::SolarObject *obj = solarObjectByNickname(m_gizmoDragNickname);
    if (!obj)
        return;

    const QPoint delta = screenPos - m_gizmoDragStartScreenPos;
    const float distance = m_camera
        ? (m_camera->position() - m_gizmoDragStartPosition).length()
        : 80000.0f;
    const float moveScale = qBound(40.0f, distance / 450.0f, 900.0f);
    QVector3D position = m_gizmoDragStartPosition;
    QVector3D rotation = m_gizmoDragStartRotation;

    switch (m_activeGizmoHandle) {
    case GizmoMoveX:
        position.setX(position.x() + static_cast<float>(delta.x()) * moveScale);
        break;
    case GizmoMoveY:
        position.setY(position.y() - static_cast<float>(delta.y()) * moveScale);
        break;
    case GizmoMoveZ:
        position.setZ(position.z() + static_cast<float>(delta.x() - delta.y()) * moveScale * 0.5f);
        break;
    case GizmoRotateYaw:
        rotation.setY(rotation.y() + static_cast<float>(delta.x()) * 0.45f);
        break;
    case GizmoRotatePitch:
        rotation.setX(rotation.x() - static_cast<float>(delta.y()) * 0.45f);
        break;
    default:
        break;
    }

    applyGizmoTransform(position, rotation);
}

void SceneView3D::finishGizmoDrag()
{
    if (m_activeGizmoHandle == GizmoNone)
        return;
    m_activeGizmoHandle = GizmoNone;
    m_gizmoDragNickname.clear();
    if (m_selectionManager)
        m_selectionManager->setPickingSuppressed(false);
}

void SceneView3D::applyGizmoTransform(const QVector3D &position, const QVector3D &rotation)
{
    if (!m_transformGizmoEnabled)
        return;

    flatlas::domain::SolarObject *obj = solarObjectByNickname(m_gizmoDragNickname);
    if (!obj)
        return;

    obj->setPosition(position);
    obj->setRotation(rotation);
    m_objectCentersByNickname.insert(obj->nickname(), position);

    if (Qt3DCore::QTransform *transform = m_objectTransformsByNickname.value(obj->nickname(), nullptr)) {
        transform->setTranslation(position);
        transform->setRotation(ZoneGeometryBuilder::rotationFromFreelancer(rotation));
    }
    if (m_orbitCamera && m_selectionManager && obj->nickname() == m_selectionManager->selectedNickname())
        m_orbitCamera->setTarget(position);
    updateTransformGizmo();
    requestViewportUpdate();
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

    auto *watcher = new QFutureWatcher<QHash<QString, flatlas::infrastructure::PlanetSurfaceTextureSet>>(this);
    connect(watcher,
            &QFutureWatcher<QHash<QString, flatlas::infrastructure::PlanetSurfaceTextureSet>>::finished,
            this,
            [this, watcher, generation]() {
        watcher->deleteLater();
        QHash<QString, flatlas::infrastructure::PlanetSurfaceTextureSet> textures;
        try {
            textures = watcher->result();
        } catch (...) {
            return;
        }
        applyPlanetTextures(textures, generation);
    });

    watcher->setFuture(QtConcurrent::run([sourcePathsByNickname]() {
        QHash<QString, flatlas::infrastructure::PlanetSurfaceTextureSet> textures;
        for (auto it = sourcePathsByNickname.constBegin(); it != sourcePathsByNickname.constEnd(); ++it) {
            const flatlas::infrastructure::PlanetSurfaceTextureSet texture =
                flatlas::infrastructure::FreelancerMaterialResolver::loadPlanetSurfaceTextures(it.value());
            if (texture.hasSegmentedSurface() || !texture.fallbackAtlas.isNull())
                textures.insert(it.key(), texture);
        }
        return textures;
    }));
}

void SceneView3D::applyPlanetTextures(
    const QHash<QString, flatlas::infrastructure::PlanetSurfaceTextureSet> &textures, int generation)
{
    if (generation != m_loadGeneration)
        return;

    for (auto it = textures.constBegin(); it != textures.constEnd(); ++it) {
        Qt3DCore::QEntity *marker = m_markerEntitiesByNickname.value(it.key(), nullptr);
        if (!marker)
            continue;

        const auto oldSegments = m_planetSegmentEntitiesByNickname.take(it.key());
        for (Qt3DCore::QEntity *segment : oldSegments)
            delete segment;

        if (it.value().hasSegmentedSurface()) {
            const float radius = m_objectRadiiByNickname.value(it.key(), 1.0f);
            auto *segmentParent = qobject_cast<Qt3DCore::QEntity *>(marker->parent());
            if (!segmentParent)
                segmentParent = marker;
            QList<Qt3DCore::QEntity *> segments;
            auto addSegment = [&](const QImage &image,
                                  const QVector3D &center,
                                  const QVector3D &uAxis,
                                  const QVector3D &vAxis,
                                  float radiusScale) {
                if (image.isNull())
                    return;

                auto *segment = new Qt3DCore::QEntity(segmentParent);
                Qt3DRender::QGeometryRenderer *renderer =
                    buildPlanetCubeFaceRenderer(radius * radiusScale, center, uAxis, vAxis, segment);
                if (!renderer) {
                    delete segment;
                    return;
                }

                Qt3DRender::QMaterial *material = MaterialFactory::createPlanetSurfaceFromImage(image, segment);
                segment->addComponent(renderer);
                segment->addComponent(material);
                segments.append(segment);
                if (m_selectionManager)
                    m_selectionManager->registerEntity(it.key(), segment, material);
            };

            addSegment(it.value().side1,
                       QVector3D(0.0f, 0.0f, 1.0f),
                       QVector3D(1.0f, 0.0f, 0.0f),
                       QVector3D(0.0f, 1.0f, 0.0f),
                       1.0f);
            addSegment(it.value().side2,
                       QVector3D(1.0f, 0.0f, 0.0f),
                       QVector3D(0.0f, 0.0f, -1.0f),
                       QVector3D(0.0f, 1.0f, 0.0f),
                       1.0f);
            addSegment(it.value().side1,
                       QVector3D(0.0f, 0.0f, -1.0f),
                       QVector3D(-1.0f, 0.0f, 0.0f),
                       QVector3D(0.0f, 1.0f, 0.0f),
                       1.0f);
            addSegment(it.value().side2,
                       QVector3D(-1.0f, 0.0f, 0.0f),
                       QVector3D(0.0f, 0.0f, 1.0f),
                       QVector3D(0.0f, 1.0f, 0.0f),
                       1.0f);
            if (!it.value().cap.isNull()) {
                addSegment(it.value().cap,
                           QVector3D(0.0f, 1.0f, 0.0f),
                           QVector3D(1.0f, 0.0f, 0.0f),
                           QVector3D(0.0f, 0.0f, -1.0f),
                           1.0f);
                addSegment(it.value().cap,
                           QVector3D(0.0f, -1.0f, 0.0f),
                           QVector3D(1.0f, 0.0f, 0.0f),
                           QVector3D(0.0f, 0.0f, 1.0f),
                           1.0f);
            }
            if (!segments.isEmpty()) {
                marker->setEnabled(false);
                m_planetSegmentEntitiesByNickname.insert(it.key(), segments);
            }
            continue;
        }

        if (it.value().fallbackAtlas.isNull())
            continue;

        if (Qt3DRender::QMaterial *oldMaterial = m_markerMaterialsByNickname.value(it.key(), nullptr))
            marker->removeComponent(oldMaterial);

        Qt3DRender::QMaterial *textureMaterial =
            MaterialFactory::createPlanetSurfaceFromImage(it.value().fallbackAtlas, marker);
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
        if (m_activeGizmoHandle != GizmoNone) {
            if (!m_transformGizmoEnabled)
                finishGizmoDrag();
            return true;
        }
        if (m_freeCamera && m_freeCamera->isEnabled()) {
            if (auto *mouseEvent = static_cast<QMouseEvent *>(event);
                mouseEvent->button() == Qt::LeftButton && m_container) {
                m_container->grabMouse();
            }
            m_freeCamera->handleMousePress(static_cast<QMouseEvent *>(event));
            return true;
        }
        if (auto *mouseEvent = static_cast<QMouseEvent *>(event);
            mouseEvent->button() == Qt::LeftButton) {
            return QWidget::eventFilter(watched, event);
        }
        if (auto *mouseEvent = static_cast<QMouseEvent *>(event);
            mouseEvent->button() == Qt::RightButton || mouseEvent->button() == Qt::MiddleButton) {
            m_cameraMouseInteractionActive = true;
            if (m_selectionManager)
                m_selectionManager->setPickingSuppressed(true);
        }
        m_orbitCamera->handleMousePress(static_cast<QMouseEvent *>(event));
        return true;
    case QEvent::MouseMove:
        if (m_activeGizmoHandle != GizmoNone) {
            if (!m_transformGizmoEnabled) {
                finishGizmoDrag();
                return true;
            }
            updateGizmoDrag(static_cast<QMouseEvent *>(event)->pos());
            return true;
        }
        if (m_freeCamera && m_freeCamera->isEnabled()) {
            m_freeCamera->handleMouseMove(static_cast<QMouseEvent *>(event));
            return true;
        }
        if (auto *mouseEvent = static_cast<QMouseEvent *>(event);
            mouseEvent->buttons().testFlag(Qt::LeftButton)) {
            return QWidget::eventFilter(watched, event);
        }
        m_orbitCamera->handleMouseMove(static_cast<QMouseEvent *>(event));
        return true;
    case QEvent::MouseButtonRelease:
        if (m_activeGizmoHandle != GizmoNone) {
            finishGizmoDrag();
            return true;
        }
        if (m_freeCamera && m_freeCamera->isEnabled()) {
            m_freeCamera->handleMouseRelease(static_cast<QMouseEvent *>(event));
            if (auto *mouseEvent = static_cast<QMouseEvent *>(event);
                mouseEvent->button() == Qt::LeftButton && m_container) {
                m_container->releaseMouse();
            }
            return true;
        }
        if (auto *mouseEvent = static_cast<QMouseEvent *>(event);
            mouseEvent->button() == Qt::LeftButton) {
            return QWidget::eventFilter(watched, event);
        }
        m_orbitCamera->handleMouseRelease(static_cast<QMouseEvent *>(event));
        if (auto *mouseEvent = static_cast<QMouseEvent *>(event);
            m_cameraMouseInteractionActive
            && (mouseEvent->button() == Qt::RightButton || mouseEvent->button() == Qt::MiddleButton)) {
            m_cameraMouseInteractionActive = false;
            if (m_selectionManager) {
                QTimer::singleShot(0, this, [this]() {
                    if (m_selectionManager && !m_cameraMouseInteractionActive && m_activeGizmoHandle == GizmoNone)
                        m_selectionManager->setPickingSuppressed(false);
                });
            }
        }
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

void SceneView3D::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
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
