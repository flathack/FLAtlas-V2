// rendering/view3d/SkyRenderer.cpp – Hintergrund-Rendering (Phase 7)

#ifdef FLATLAS_HAS_QT3D

#include "SkyRenderer.h"

#include "MaterialFactory.h"

#include <Qt3DCore/QAttribute>
#include <Qt3DCore/QBuffer>
#include <Qt3DCore/QGeometry>
#include <Qt3DExtras/QTextureMaterial>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QTextureWrapMode>

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QVector3D>
#include <QtMath>

namespace flatlas::rendering {

namespace {

template <typename T>
void appendPod(QByteArray &blob, const T &value)
{
    blob.append(reinterpret_cast<const char *>(&value), static_cast<int>(sizeof(T)));
}

QImage loadDarkenedSkyTexture()
{
    QImage image(QStringLiteral(":/images/star-background.png"));
    if (image.isNull())
        return image;

    QImage darkened = image.convertToFormat(QImage::Format_ARGB32);
    constexpr int overlayAlpha = 150;
    for (int y = 0; y < darkened.height(); ++y) {
        auto *row = reinterpret_cast<QRgb *>(darkened.scanLine(y));
        for (int x = 0; x < darkened.width(); ++x) {
            const QColor source = QColor::fromRgba(row[x]);
            row[x] = qRgba(source.red() * (255 - overlayAlpha) / 255,
                           source.green() * (255 - overlayAlpha) / 255,
                           source.blue() * (255 - overlayAlpha) / 255,
                           255);
        }
    }
    return darkened;
}

Qt3DRender::QGeometryRenderer *createInsideSkySphere(Qt3DCore::QNode *owner)
{
    constexpr int rings = 48;
    constexpr int slices = 96;
    constexpr int vertexStride = 5 * static_cast<int>(sizeof(float));

    QByteArray vertexBlob;
    vertexBlob.reserve((rings + 1) * (slices + 1) * vertexStride);

    for (int ring = 0; ring <= rings; ++ring) {
        const float v = static_cast<float>(ring) / static_cast<float>(rings);
        const float theta = v * static_cast<float>(M_PI);
        const float sinTheta = std::sin(theta);
        const float cosTheta = std::cos(theta);

        for (int slice = 0; slice <= slices; ++slice) {
            const float u = static_cast<float>(slice) / static_cast<float>(slices);
            const float phi = u * 2.0f * static_cast<float>(M_PI);
            const QVector3D position(sinTheta * std::cos(phi), cosTheta, sinTheta * std::sin(phi));
            appendPod(vertexBlob, position.x());
            appendPod(vertexBlob, position.y());
            appendPod(vertexBlob, position.z());
            appendPod(vertexBlob, 1.0f - u);
            appendPod(vertexBlob, v);
        }
    }

    QByteArray indexBlob;
    indexBlob.reserve(rings * slices * 12 * static_cast<int>(sizeof(quint32)));
    for (int ring = 0; ring < rings; ++ring) {
        for (int slice = 0; slice < slices; ++slice) {
            const quint32 topLeft = static_cast<quint32>(ring * (slices + 1) + slice);
            const quint32 bottomLeft = static_cast<quint32>((ring + 1) * (slices + 1) + slice);
            const quint32 topRight = topLeft + 1;
            const quint32 bottomRight = bottomLeft + 1;

            // Keep the sphere visible from inside and outside. Some Qt3D
            // materials/drivers cull one side, which otherwise leaves a black
            // clear color instead of the panorama.
            appendPod(indexBlob, topLeft);
            appendPod(indexBlob, bottomRight);
            appendPod(indexBlob, bottomLeft);
            appendPod(indexBlob, topLeft);
            appendPod(indexBlob, topRight);
            appendPod(indexBlob, bottomRight);
            appendPod(indexBlob, topLeft);
            appendPod(indexBlob, bottomLeft);
            appendPod(indexBlob, bottomRight);
            appendPod(indexBlob, topLeft);
            appendPod(indexBlob, bottomRight);
            appendPod(indexBlob, topRight);
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
    positionAttr->setCount((rings + 1) * (slices + 1));
    positionAttr->setBuffer(vertexBuffer);
    geometry->addAttribute(positionAttr);

    auto *uvAttr = new Qt3DCore::QAttribute(geometry);
    uvAttr->setName(Qt3DCore::QAttribute::defaultTextureCoordinateAttributeName());
    uvAttr->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
    uvAttr->setVertexBaseType(Qt3DCore::QAttribute::Float);
    uvAttr->setVertexSize(2);
    uvAttr->setByteStride(vertexStride);
    uvAttr->setByteOffset(3 * static_cast<int>(sizeof(float)));
    uvAttr->setCount((rings + 1) * (slices + 1));
    uvAttr->setBuffer(vertexBuffer);
    geometry->addAttribute(uvAttr);

    auto *indexBuffer = new Qt3DCore::QBuffer(geometry);
    indexBuffer->setData(indexBlob);

    auto *indexAttr = new Qt3DCore::QAttribute(geometry);
    indexAttr->setAttributeType(Qt3DCore::QAttribute::IndexAttribute);
    indexAttr->setVertexBaseType(Qt3DCore::QAttribute::UnsignedInt);
    indexAttr->setCount(rings * slices * 12);
    indexAttr->setBuffer(indexBuffer);
    geometry->addAttribute(indexAttr);

    auto *renderer = new Qt3DRender::QGeometryRenderer(owner);
    renderer->setGeometry(geometry);
    renderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);
    renderer->setVertexCount(rings * slices * 12);
    return renderer;
}

} // namespace

SkyRenderer::SkyRenderer(Qt3DCore::QNode *parent)
    : Qt3DCore::QEntity(parent)
{
    m_mesh = createInsideSkySphere(this);

    auto *textureMaterial = new Qt3DExtras::QTextureMaterial(this);
    auto *texture = MaterialFactory::createTexture(loadDarkenedSkyTexture(), textureMaterial);
    texture->wrapMode()->setX(Qt3DRender::QTextureWrapMode::Repeat);
    texture->wrapMode()->setY(Qt3DRender::QTextureWrapMode::ClampToEdge);
    textureMaterial->setTexture(texture);
    m_material = textureMaterial;

    m_transform = new Qt3DCore::QTransform(this);
    m_transform->setScale(5000000.0f);

    addComponent(m_mesh);
    addComponent(m_material);
    addComponent(m_transform);
}

void SkyRenderer::setCenter(const QVector3D &center)
{
    if (m_transform)
        m_transform->setTranslation(center);
}

void SkyRenderer::setRadius(float radius)
{
    if (m_transform)
        m_transform->setScale(radius);
}

} // namespace flatlas::rendering

#endif // FLATLAS_HAS_QT3D
