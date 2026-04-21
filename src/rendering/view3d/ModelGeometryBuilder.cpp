// rendering/view3d/ModelGeometryBuilder.cpp - reusable Qt3D geometry builders for Freelancer models

#ifdef FLATLAS_HAS_QT3D

#include "ModelGeometryBuilder.h"

#include <Qt3DCore/QAttribute>
#include <Qt3DCore/QBuffer>
#include <Qt3DCore/QGeometry>
#include <Qt3DRender/QGeometryRenderer>

#include <QByteArray>
#include <QMatrix4x4>
#include <QtMath>

#include <algorithm>
#include <array>
#include <cstring>

namespace flatlas::rendering {

namespace {

template <typename T>
void appendPod(QByteArray &blob, const T &value)
{
    const auto *raw = reinterpret_cast<const char *>(&value);
    blob.append(raw, static_cast<qsizetype>(sizeof(T)));
}

struct VertexRecord {
    float px;
    float py;
    float pz;
    float nx;
    float ny;
    float nz;
    float u;
    float v;
};

bool isRenderableMesh(const flatlas::infrastructure::MeshData &mesh)
{
    if (mesh.vertices.size() < 3 || mesh.indices.size() < 3)
        return false;

    const auto maxIndexIt = std::max_element(mesh.indices.cbegin(), mesh.indices.cend());
    if (maxIndexIt == mesh.indices.cend())
        return false;
    if (*maxIndexIt >= static_cast<uint32_t>(mesh.vertices.size()))
        return false;
    return true;
}

Qt3DRender::QGeometryRenderer *buildRendererFromBuffers(
    const QByteArray &vertexBlob,
    int vertexCount,
    int vertexStride,
    bool hasNormals,
    bool hasTexcoords,
    const QByteArray &indexBlob,
    Qt3DCore::QAttribute::VertexBaseType indexType,
    int indexCount,
    Qt3DRender::QGeometryRenderer::PrimitiveType primitiveType,
    Qt3DCore::QNode *owner)
{
    if (vertexBlob.isEmpty() || indexBlob.isEmpty() || vertexCount <= 0 || indexCount <= 0)
        return nullptr;

    auto *geometry = new Qt3DCore::QGeometry(owner);
    auto *vertexBuffer = new Qt3DCore::QBuffer(geometry);
    vertexBuffer->setData(vertexBlob);

    auto *positionAttr = new Qt3DCore::QAttribute(geometry);
    positionAttr->setName(Qt3DCore::QAttribute::defaultPositionAttributeName());
    positionAttr->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
    positionAttr->setVertexBaseType(Qt3DCore::QAttribute::Float);
    positionAttr->setVertexSize(3);
    positionAttr->setByteStride(vertexStride);
    positionAttr->setCount(vertexCount);
    positionAttr->setBuffer(vertexBuffer);
    geometry->addAttribute(positionAttr);

    if (hasNormals) {
        auto *normalAttr = new Qt3DCore::QAttribute(geometry);
        normalAttr->setName(Qt3DCore::QAttribute::defaultNormalAttributeName());
        normalAttr->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
        normalAttr->setVertexBaseType(Qt3DCore::QAttribute::Float);
        normalAttr->setVertexSize(3);
        normalAttr->setByteStride(vertexStride);
        normalAttr->setByteOffset(3 * static_cast<int>(sizeof(float)));
        normalAttr->setCount(vertexCount);
        normalAttr->setBuffer(vertexBuffer);
        geometry->addAttribute(normalAttr);
    }

    if (hasTexcoords) {
        auto *uvAttr = new Qt3DCore::QAttribute(geometry);
        uvAttr->setName(Qt3DCore::QAttribute::defaultTextureCoordinateAttributeName());
        uvAttr->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
        uvAttr->setVertexBaseType(Qt3DCore::QAttribute::Float);
        uvAttr->setVertexSize(2);
        uvAttr->setByteStride(vertexStride);
        uvAttr->setByteOffset(6 * static_cast<int>(sizeof(float)));
        uvAttr->setCount(vertexCount);
        uvAttr->setBuffer(vertexBuffer);
        geometry->addAttribute(uvAttr);
    }

    auto *indexBuffer = new Qt3DCore::QBuffer(geometry);
    indexBuffer->setData(indexBlob);

    auto *indexAttr = new Qt3DCore::QAttribute(geometry);
    indexAttr->setAttributeType(Qt3DCore::QAttribute::IndexAttribute);
    indexAttr->setVertexBaseType(indexType);
    indexAttr->setCount(indexCount);
    indexAttr->setBuffer(indexBuffer);
    geometry->addAttribute(indexAttr);

    auto *renderer = new Qt3DRender::QGeometryRenderer(owner);
    renderer->setGeometry(geometry);
    renderer->setPrimitiveType(primitiveType);
    renderer->setVertexCount(indexCount);
    return renderer;
}

} // namespace

void ModelBounds::include(const QVector3D &point)
{
    if (!valid) {
        minCorner = point;
        maxCorner = point;
        valid = true;
        return;
    }

    minCorner.setX(qMin(minCorner.x(), point.x()));
    minCorner.setY(qMin(minCorner.y(), point.y()));
    minCorner.setZ(qMin(minCorner.z(), point.z()));
    maxCorner.setX(qMax(maxCorner.x(), point.x()));
    maxCorner.setY(qMax(maxCorner.y(), point.y()));
    maxCorner.setZ(qMax(maxCorner.z(), point.z()));
}

void ModelBounds::include(const ModelBounds &other)
{
    if (!other.valid)
        return;
    include(other.minCorner);
    include(other.maxCorner);
}

QVector3D ModelBounds::center() const
{
    if (!valid)
        return {};
    return (minCorner + maxCorner) * 0.5f;
}

float ModelBounds::radius() const
{
    if (!valid)
        return 0.0f;
    return (maxCorner - minCorner).length() * 0.5f;
}

Qt3DRender::QGeometryRenderer *ModelGeometryBuilder::buildTriangleRenderer(
    const flatlas::infrastructure::MeshData &mesh,
    Qt3DCore::QNode *owner)
{
    if (!isRenderableMesh(mesh))
        return nullptr;

    QByteArray vertexBlob;
    vertexBlob.reserve(mesh.vertices.size() * static_cast<int>(sizeof(VertexRecord)));
    for (const auto &vertex : mesh.vertices) {
        VertexRecord record{
            vertex.position.x(), vertex.position.y(), vertex.position.z(),
            vertex.normal.x(), vertex.normal.y(), vertex.normal.z(),
            vertex.u, vertex.v
        };
        appendPod(vertexBlob, record);
    }

    QByteArray indexBlob;
    Qt3DCore::QAttribute::VertexBaseType indexType = Qt3DCore::QAttribute::UnsignedInt;
    const bool canUseShort = std::all_of(mesh.indices.cbegin(), mesh.indices.cend(),
        [](uint32_t index) { return index <= 0xFFFFu; });
    if (canUseShort) {
        indexType = Qt3DCore::QAttribute::UnsignedShort;
        indexBlob.reserve(mesh.indices.size() * static_cast<int>(sizeof(uint16_t)));
        for (uint32_t index : mesh.indices) {
            const uint16_t packed = static_cast<uint16_t>(index);
            appendPod(indexBlob, packed);
        }
    } else {
        indexBlob.reserve(mesh.indices.size() * static_cast<int>(sizeof(uint32_t)));
        for (uint32_t index : mesh.indices)
            appendPod(indexBlob, index);
    }

    return buildRendererFromBuffers(vertexBlob,
                                    mesh.vertices.size(),
                                    static_cast<int>(sizeof(VertexRecord)),
                                    true,
                                    true,
                                    indexBlob,
                                    indexType,
                                    mesh.indices.size(),
                                    Qt3DRender::QGeometryRenderer::Triangles,
                                    owner);
}

Qt3DRender::QGeometryRenderer *ModelGeometryBuilder::buildWireframeRenderer(
    const flatlas::infrastructure::MeshData &mesh,
    Qt3DCore::QNode *owner)
{
    if (!isRenderableMesh(mesh))
        return nullptr;

    QByteArray vertexBlob;
    vertexBlob.reserve(mesh.vertices.size() * static_cast<int>(sizeof(VertexRecord)));
    for (const auto &vertex : mesh.vertices) {
        VertexRecord record{
            vertex.position.x(), vertex.position.y(), vertex.position.z(),
            vertex.normal.x(), vertex.normal.y(), vertex.normal.z(),
            vertex.u, vertex.v
        };
        appendPod(vertexBlob, record);
    }

    QVector<uint32_t> lineIndices;
    lineIndices.reserve((mesh.indices.size() / 3) * 6);
    for (qsizetype i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const uint32_t a = mesh.indices[i];
        const uint32_t b = mesh.indices[i + 1];
        const uint32_t c = mesh.indices[i + 2];
        lineIndices.append(a); lineIndices.append(b);
        lineIndices.append(b); lineIndices.append(c);
        lineIndices.append(c); lineIndices.append(a);
    }

    if (lineIndices.isEmpty())
        return nullptr;

    QByteArray indexBlob;
    Qt3DCore::QAttribute::VertexBaseType indexType = Qt3DCore::QAttribute::UnsignedInt;
    const bool canUseShort = std::all_of(lineIndices.cbegin(), lineIndices.cend(),
        [](uint32_t index) { return index <= 0xFFFFu; });
    if (canUseShort) {
        indexType = Qt3DCore::QAttribute::UnsignedShort;
        indexBlob.reserve(lineIndices.size() * static_cast<int>(sizeof(uint16_t)));
        for (uint32_t index : lineIndices) {
            const uint16_t packed = static_cast<uint16_t>(index);
            appendPod(indexBlob, packed);
        }
    } else {
        indexBlob.reserve(lineIndices.size() * static_cast<int>(sizeof(uint32_t)));
        for (uint32_t index : lineIndices)
            appendPod(indexBlob, index);
    }

    return buildRendererFromBuffers(vertexBlob,
                                    mesh.vertices.size(),
                                    static_cast<int>(sizeof(VertexRecord)),
                                    true,
                                    true,
                                    indexBlob,
                                    indexType,
                                    lineIndices.size(),
                                    Qt3DRender::QGeometryRenderer::Lines,
                                    owner);
}

Qt3DRender::QGeometryRenderer *ModelGeometryBuilder::buildBoundingBoxRenderer(
    const ModelBounds &bounds,
    Qt3DCore::QNode *owner)
{
    if (!bounds.valid)
        return nullptr;

    const QVector3D min = bounds.minCorner;
    const QVector3D max = bounds.maxCorner;
    const QVector<QVector3D> corners{
        {min.x(), min.y(), min.z()},
        {max.x(), min.y(), min.z()},
        {max.x(), max.y(), min.z()},
        {min.x(), max.y(), min.z()},
        {min.x(), min.y(), max.z()},
        {max.x(), min.y(), max.z()},
        {max.x(), max.y(), max.z()},
        {min.x(), max.y(), max.z()},
    };

    QByteArray vertexBlob;
    vertexBlob.reserve(corners.size() * static_cast<int>(sizeof(VertexRecord)));
    for (const QVector3D &corner : corners) {
        VertexRecord record{corner.x(), corner.y(), corner.z(), 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
        appendPod(vertexBlob, record);
    }

    const uint16_t edges[] = {
        0, 1, 1, 2, 2, 3, 3, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
        0, 4, 1, 5, 2, 6, 3, 7
    };

    QByteArray indexBlob;
    indexBlob.reserve(static_cast<int>(sizeof(edges)));
    indexBlob.append(reinterpret_cast<const char *>(edges), static_cast<int>(sizeof(edges)));

    return buildRendererFromBuffers(vertexBlob,
                                    corners.size(),
                                    static_cast<int>(sizeof(VertexRecord)),
                                    true,
                                    true,
                                    indexBlob,
                                    Qt3DCore::QAttribute::UnsignedShort,
                                    static_cast<int>(std::size(edges)),
                                    Qt3DRender::QGeometryRenderer::Lines,
                                    owner);
}

ModelBounds ModelGeometryBuilder::boundsForMesh(const flatlas::infrastructure::MeshData &mesh)
{
    ModelBounds bounds;
    for (const auto &vertex : mesh.vertices)
        bounds.include(vertex.position);
    return bounds;
}

ModelBounds ModelGeometryBuilder::boundsForNode(const flatlas::infrastructure::ModelNode &node,
                                                const QVector3D &parentOffset)
{
    QMatrix4x4 transform;
    transform.translate(parentOffset);
    transform.translate(node.origin);
    transform.rotate(node.rotation);
    ModelBounds bounds;
    for (const auto &mesh : node.meshes) {
        for (const auto &vertex : mesh.vertices)
            bounds.include(transform.map(vertex.position));
    }
    const QVector3D childOffset = transform.map(QVector3D(0.0f, 0.0f, 0.0f));
    for (const auto &child : node.children) {
        ModelBounds childBounds = boundsForNode(child, childOffset);
        bounds.include(childBounds);
    }
    return bounds;
}

} // namespace flatlas::rendering

#endif // FLATLAS_HAS_QT3D
