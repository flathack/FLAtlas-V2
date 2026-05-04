#ifdef FLATLAS_HAS_QT3D

#include "ZoneGeometryBuilder.h"

#include <Qt3DCore/QAttribute>
#include <Qt3DCore/QBuffer>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QGeometry>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QCuboidMesh>
#include <Qt3DExtras/QCylinderMesh>
#include <Qt3DExtras/QPhongAlphaMaterial>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DRender/QGeometryRenderer>

#include <QByteArray>
#include <QtMath>

#include <cmath>

namespace flatlas::rendering {

namespace {

constexpr float kMinExtent = 1.0f;
constexpr int kCircleSegments = 48;

struct LocalZoneGeometry {
    QVector3D halfExtents;
    QVector3D sphereScale;
    float cylinderRadius = 0.0f;
    float cylinderLength = 0.0f;
    bool valid = false;
};

float positiveOr(float value, float fallback)
{
    return std::isfinite(value) && value > 0.0f ? value : fallback;
}

LocalZoneGeometry normalizedGeometry(const flatlas::domain::ZoneItem &zone)
{
    LocalZoneGeometry geometry;
    const QVector3D size = zone.size();

    using Shape = flatlas::domain::ZoneItem::Shape;
    switch (zone.shape()) {
    case Shape::Sphere:
    case Shape::Ring: {
        const float radius = qMax(positiveOr(size.x(), qMax(qMax(size.y(), size.z()), kMinExtent)), kMinExtent);
        geometry.sphereScale = QVector3D(radius, radius, radius);
        geometry.halfExtents = geometry.sphereScale;
        geometry.valid = true;
        break;
    }
    case Shape::Ellipsoid: {
        const float fallback = qMax(qMax(size.x(), size.y()), qMax(size.z(), kMinExtent));
        geometry.sphereScale = QVector3D(qMax(positiveOr(size.x(), fallback), kMinExtent),
                                         qMax(positiveOr(size.y(), fallback), kMinExtent),
                                         qMax(positiveOr(size.z(), fallback), kMinExtent));
        geometry.halfExtents = geometry.sphereScale;
        geometry.valid = true;
        break;
    }
    case Shape::Cylinder: {
        geometry.cylinderRadius = qMax(positiveOr(size.x(), qMax(size.z(), kMinExtent)), kMinExtent);
        geometry.cylinderLength = qMax(positiveOr(size.y(), qMax(size.z(), kMinExtent)), kMinExtent);
        geometry.halfExtents = QVector3D(geometry.cylinderRadius,
                                         geometry.cylinderLength * 0.5f,
                                         geometry.cylinderRadius);
        geometry.valid = true;
        break;
    }
    case Shape::Box: {
        geometry.halfExtents = QVector3D(qMax(positiveOr(size.x(), kMinExtent), kMinExtent) * 0.5f,
                                         qMax(positiveOr(size.y(), kMinExtent), kMinExtent) * 0.5f,
                                         qMax(positiveOr(size.z(), kMinExtent), kMinExtent) * 0.5f);
        geometry.valid = true;
        break;
    }
    }

    return geometry;
}

void appendPoint(QByteArray &blob, const QVector3D &point)
{
    const float values[] = { point.x(), point.y(), point.z() };
    blob.append(reinterpret_cast<const char *>(values), static_cast<int>(sizeof(values)));
}

Qt3DRender::QGeometryRenderer *buildLineRenderer(const QVector<QVector3D> &points, Qt3DCore::QNode *owner)
{
    if (points.size() < 2)
        return nullptr;

    QByteArray vertexBlob;
    vertexBlob.reserve(points.size() * 3 * static_cast<int>(sizeof(float)));
    for (const QVector3D &point : points)
        appendPoint(vertexBlob, point);

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

void appendLine(QVector<QVector3D> &points, const QVector3D &a, const QVector3D &b)
{
    points.append(a);
    points.append(b);
}

void appendEllipseRing(QVector<QVector3D> &points, const QVector3D &scale, int axis)
{
    QVector3D previous;
    for (int i = 0; i <= kCircleSegments; ++i) {
        const float angle = static_cast<float>(2.0 * M_PI * i / kCircleSegments);
        QVector3D current;
        if (axis == 0)
            current = QVector3D(0.0f, std::cos(angle) * scale.y(), std::sin(angle) * scale.z());
        else if (axis == 1)
            current = QVector3D(std::cos(angle) * scale.x(), 0.0f, std::sin(angle) * scale.z());
        else
            current = QVector3D(std::cos(angle) * scale.x(), std::sin(angle) * scale.y(), 0.0f);
        if (i > 0)
            appendLine(points, previous, current);
        previous = current;
    }
}

QVector<QVector3D> ellipsoidWirePoints(const QVector3D &scale)
{
    QVector<QVector3D> points;
    points.reserve(kCircleSegments * 6);
    appendEllipseRing(points, scale, 0);
    appendEllipseRing(points, scale, 1);
    appendEllipseRing(points, scale, 2);
    return points;
}

QVector<QVector3D> cylinderWirePoints(float radius, float length)
{
    QVector<QVector3D> points;
    points.reserve(kCircleSegments * 6 + 16);
    const float halfLength = length * 0.5f;
    QVector<QVector3D> top;
    QVector<QVector3D> bottom;
    top.reserve(kCircleSegments);
    bottom.reserve(kCircleSegments);
    for (int i = 0; i < kCircleSegments; ++i) {
        const float angle = static_cast<float>(2.0 * M_PI * i / kCircleSegments);
        const float x = std::cos(angle) * radius;
        const float z = std::sin(angle) * radius;
        top.append(QVector3D(x, halfLength, z));
        bottom.append(QVector3D(x, -halfLength, z));
    }
    for (int i = 0; i < kCircleSegments; ++i) {
        const int next = (i + 1) % kCircleSegments;
        appendLine(points, top.at(i), top.at(next));
        appendLine(points, bottom.at(i), bottom.at(next));
    }
    for (int i = 0; i < 8; i += 2)
        appendLine(points, top.at(i * kCircleSegments / 8), bottom.at(i * kCircleSegments / 8));
    return points;
}

QVector<QVector3D> boxWirePoints(const QVector3D &half)
{
    const QVector<QVector3D> c{
        {-half.x(), -half.y(), -half.z()},
        { half.x(), -half.y(), -half.z()},
        { half.x(),  half.y(), -half.z()},
        {-half.x(),  half.y(), -half.z()},
        {-half.x(), -half.y(),  half.z()},
        { half.x(), -half.y(),  half.z()},
        { half.x(),  half.y(),  half.z()},
        {-half.x(),  half.y(),  half.z()},
    };
    QVector<QVector3D> points;
    points.reserve(24);
    const int edges[] = {
        0, 1, 1, 2, 2, 3, 3, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
        0, 4, 1, 5, 2, 6, 3, 7
    };
    for (int i = 0; i < 24; i += 2)
        appendLine(points, c.at(edges[i]), c.at(edges[i + 1]));
    return points;
}

void includeRotatedBounds(ModelBounds &bounds,
                          const QVector3D &position,
                          const QQuaternion &rotation,
                          const QVector3D &half)
{
    for (int x = -1; x <= 1; x += 2) {
        for (int y = -1; y <= 1; y += 2) {
            for (int z = -1; z <= 1; z += 2) {
                const QVector3D local(half.x() * x, half.y() * y, half.z() * z);
                bounds.include(position + rotation.rotatedVector(local));
            }
        }
    }
}

Qt3DExtras::QPhongAlphaMaterial *makeFillMaterial(const QColor &color, float alpha, Qt3DCore::QNode *owner)
{
    auto *material = new Qt3DExtras::QPhongAlphaMaterial(owner);
    material->setDiffuse(color);
    material->setAmbient(color.darker(170));
    material->setAlpha(qBound(0.0f, alpha, 1.0f));
    return material;
}

Qt3DExtras::QPhongAlphaMaterial *makeWireMaterial(const QColor &color, Qt3DCore::QNode *owner)
{
    QColor wireColor = color;
    wireColor.setAlphaF(0.5f);
    auto *material = new Qt3DExtras::QPhongAlphaMaterial(owner);
    material->setDiffuse(wireColor);
    material->setAmbient(wireColor.darker(170));
    material->setAlpha(0.5f);
    return material;
}

} // namespace

QQuaternion ZoneGeometryBuilder::rotationFromFreelancer(const QVector3D &rotation)
{
    constexpr float tolerance = 0.25f;
    float rx = rotation.x();
    float ry = rotation.y();
    float rz = rotation.z();

    if (std::abs(std::abs(rx) - 180.0f) <= tolerance
        && std::abs(std::abs(rz) - 180.0f) <= tolerance) {
        rx = 0.0f;
        ry += 180.0f;
        rz = 0.0f;
        if (ry > 180.0f)
            ry -= 360.0f;
        else if (ry < -180.0f)
            ry += 360.0f;
    }

    return QQuaternion::fromEulerAngles(rx, ry, rz);
}

ZoneGeometryBuildResult ZoneGeometryBuilder::buildZone(const flatlas::domain::ZoneItem &zone,
                                                       const ZoneVisualStyle &style,
                                                       Qt3DCore::QEntity *parent)
{
    ZoneGeometryBuildResult result;
    if (!parent)
        return result;

    const LocalZoneGeometry geometry = normalizedGeometry(zone);
    if (!geometry.valid)
        return result;

    auto *root = new Qt3DCore::QEntity(parent);
    auto *rootTransform = new Qt3DCore::QTransform(root);
    const QQuaternion rotation = rotationFromFreelancer(zone.rotation());
    rootTransform->setTranslation(zone.position());
    rootTransform->setRotation(rotation);
    root->addComponent(rootTransform);

    QVector<QVector3D> wirePoints;
    using Shape = flatlas::domain::ZoneItem::Shape;
    if (zone.shape() == Shape::Box) {
        wirePoints = boxWirePoints(geometry.halfExtents);
        auto *volume = new Qt3DCore::QEntity(root);
        auto *mesh = new Qt3DExtras::QCuboidMesh(volume);
        mesh->setXExtent(geometry.halfExtents.x() * 2.0f);
        mesh->setYExtent(geometry.halfExtents.y() * 2.0f);
        mesh->setZExtent(geometry.halfExtents.z() * 2.0f);
        volume->addComponent(mesh);
        volume->addComponent(makeFillMaterial(style.fillColor, style.fillVisible ? style.opacity : 0.035f, volume));
    } else if (zone.shape() == Shape::Cylinder) {
        wirePoints = cylinderWirePoints(geometry.cylinderRadius, geometry.cylinderLength);
        auto *volume = new Qt3DCore::QEntity(root);
        auto *mesh = new Qt3DExtras::QCylinderMesh(volume);
        mesh->setRadius(geometry.cylinderRadius);
        mesh->setLength(geometry.cylinderLength);
        mesh->setRings(1);
        mesh->setSlices(32);
        volume->addComponent(mesh);
        volume->addComponent(makeFillMaterial(style.fillColor, style.fillVisible ? style.opacity : 0.035f, volume));
    } else {
        wirePoints = ellipsoidWirePoints(geometry.sphereScale);
        auto *volume = new Qt3DCore::QEntity(root);
        auto *mesh = new Qt3DExtras::QSphereMesh(volume);
        mesh->setRadius(1.0f);
        mesh->setRings(12);
        mesh->setSlices(24);
        auto *transform = new Qt3DCore::QTransform(volume);
        transform->setScale3D(geometry.sphereScale);
        volume->addComponent(mesh);
        volume->addComponent(transform);
        volume->addComponent(makeFillMaterial(style.fillColor, style.fillVisible ? style.opacity : 0.035f, volume));
    }

    auto *wire = new Qt3DCore::QEntity(root);
    if (auto *renderer = buildLineRenderer(wirePoints, wire)) {
        auto *wireMaterial = makeWireMaterial(style.wireColor, wire);
        wire->addComponent(renderer);
        wire->addComponent(wireMaterial);
        result.pickEntity = wire;
        result.selectionMaterial = wireMaterial;
    } else {
        wire->deleteLater();
    }

    ModelBounds bounds;
    includeRotatedBounds(bounds, zone.position(), rotation, geometry.halfExtents);
    result.rootEntity = root;
    result.bounds = bounds;
    result.valid = true;
    return result;
}

} // namespace flatlas::rendering

#endif // FLATLAS_HAS_QT3D
