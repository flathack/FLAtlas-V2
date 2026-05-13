#include "ModelScreenshotExporter.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMatrix4x4>
#include <QPainter>
#include <QPainterPath>
#include <QSaveFile>

#include "infrastructure/freelancer/FreelancerMaterialResolver.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace flatlas::tools {
namespace {

struct TexturedIconTriangle {
    QVector3D a;
    QVector3D b;
    QVector3D c;
    QVector3D normal;
    QColor color;
    QImage texture;
    QPointF uvA;
    QPointF uvB;
    QPointF uvC;
    float depth = 0.0f;
};

QJsonArray vectorToJson(const QVector3D &value)
{
    return QJsonArray{value.x(), value.y(), value.z()};
}

QVector<const flatlas::infrastructure::MeshData *> selectPreferredLodMeshes(
    const flatlas::infrastructure::ModelNode &node)
{
    QVector<const flatlas::infrastructure::MeshData *> selected;
    if (node.meshes.isEmpty())
        return selected;

    int bestLodIndex = std::numeric_limits<int>::max();
    for (const auto &mesh : node.meshes) {
        if (mesh.lodIndex >= 0 && mesh.lodIndex < bestLodIndex)
            bestLodIndex = mesh.lodIndex;
    }

    if (bestLodIndex < std::numeric_limits<int>::max()) {
        for (const auto &mesh : node.meshes) {
            if (mesh.lodIndex == bestLodIndex)
                selected.append(&mesh);
        }
    } else {
        for (const auto &mesh : node.meshes)
            selected.append(&mesh);
    }
    return selected;
}

QMatrix4x4 buildTransformMatrix(const QVector3D &translation, const QQuaternion &rotation)
{
    QMatrix4x4 transform;
    transform.translate(translation);
    transform.rotate(rotation);
    return transform;
}

QHash<QString, flatlas::infrastructure::CmpTransformHint> buildTransformHintLookup(
    const QVector<flatlas::infrastructure::CmpTransformHint> &hints)
{
    QHash<QString, flatlas::infrastructure::CmpTransformHint> lookup;
    lookup.reserve(hints.size());
    for (const auto &hint : hints)
        lookup.insert(hint.partName.trimmed().toLower(), hint);
    return lookup;
}

QMatrix4x4 resolveNodeTransform(
    const flatlas::infrastructure::ModelNode &node,
    const QMatrix4x4 &parentTransform,
    const QHash<QString, flatlas::infrastructure::CmpTransformHint> &transformHints)
{
    const auto it = transformHints.constFind(node.name.trimmed().toLower());
    if (it != transformHints.cend()) {
        const auto &hint = it.value();
        if (hint.hasCombinedTranslation || hint.hasCombinedRotation) {
            return buildTransformMatrix(
                hint.hasCombinedTranslation ? hint.combinedTranslation : QVector3D(),
                hint.hasCombinedRotation ? hint.combinedRotation : QQuaternion(1.0f, 0.0f, 0.0f, 0.0f));
        }
    }

    QMatrix4x4 nodeTransform = parentTransform;
    nodeTransform.translate(node.origin);
    nodeTransform.rotate(node.rotation);
    return nodeTransform;
}

void appendNodeTriangles(const flatlas::infrastructure::ModelNode &node,
                         const QMatrix4x4 &parentTransform,
                         const QHash<QString, flatlas::infrastructure::CmpTransformHint> &transformHints,
                         QVector<ScreenshotTriangle> *triangles)
{
    if (!triangles)
        return;

    const QMatrix4x4 nodeTransform = resolveNodeTransform(node, parentTransform, transformHints);

    for (const auto *mesh : selectPreferredLodMeshes(node)) {
        const auto &vertices = mesh->vertices;
        const auto &indices = mesh->indices;
        for (int index = 0; index + 2 < indices.size(); index += 3) {
            const uint32_t i0 = indices.at(index);
            const uint32_t i1 = indices.at(index + 1);
            const uint32_t i2 = indices.at(index + 2);
            if (i0 >= static_cast<uint32_t>(vertices.size())
                || i1 >= static_cast<uint32_t>(vertices.size())
                || i2 >= static_cast<uint32_t>(vertices.size())) {
                continue;
            }

            triangles->append(ScreenshotTriangle{
                nodeTransform.map(vertices.at(static_cast<int>(i0)).position),
                nodeTransform.map(vertices.at(static_cast<int>(i1)).position),
                nodeTransform.map(vertices.at(static_cast<int>(i2)).position),
            });
        }
    }

    for (const auto &child : node.children)
        appendNodeTriangles(child, nodeTransform, transformHints, triangles);
}

QColor fallbackMaterialColor(const QString &materialName, int nodeIndex)
{
    const uint hash = qHash(materialName.toLower().isEmpty()
                               ? QStringLiteral("node_%1").arg(nodeIndex)
                               : materialName.toLower());
    return QColor::fromHsv(static_cast<int>(hash % 360u),
                           48 + static_cast<int>((hash / 360u) % 38u),
                           165 + static_cast<int>((hash / (360u * 80u)) % 58u),
                           255);
}

QColor sampleTexture(const QImage &texture, float u, float v, const QColor &fallback)
{
    if (texture.isNull())
        return fallback;

    const auto wrap = [](float value) {
        float wrapped = std::fmod(value, 1.0f);
        if (wrapped < 0.0f)
            wrapped += 1.0f;
        return wrapped;
    };
    const int x = qBound(0, static_cast<int>(wrap(u) * static_cast<float>(texture.width())), texture.width() - 1);
    const int y = qBound(0, static_cast<int>((1.0f - wrap(v)) * static_cast<float>(texture.height())), texture.height() - 1);
    QColor sampled = QColor::fromRgba(texture.pixel(x, y));
    return sampled.alpha() < 8 ? fallback : sampled;
}

QColor sampleTexture(const QImage &texture, const QPointF &uv, const QColor &fallback)
{
    return sampleTexture(texture, static_cast<float>(uv.x()), static_cast<float>(uv.y()), fallback);
}

QColor shadeColor(const QColor &base, const QVector3D &normal)
{
    const QVector3D light = QVector3D(-0.28f, 0.82f, -0.5f).normalized();
    const QVector3D n = normal.lengthSquared() > 0.0f ? normal.normalized() : QVector3D(0.0f, 1.0f, 0.0f);
    const float lambert = std::abs(QVector3D::dotProduct(n, light));
    const float shade = qBound(0.42f, 0.58f + lambert * 0.58f, 1.18f);
    return QColor(qBound(0, static_cast<int>(base.red() * shade), 232),
                  qBound(0, static_cast<int>(base.green() * shade), 232),
                  qBound(0, static_cast<int>(base.blue() * shade), 232),
                  qBound(32, base.alpha(), 255));
}

void appendTexturedNodeTriangles(const QString &modelPath,
                                 const flatlas::infrastructure::ModelNode &node,
                                 const QMatrix4x4 &parentTransform,
                                 const QHash<QString, flatlas::infrastructure::CmpTransformHint> &transformHints,
                                 QVector<TexturedIconTriangle> *triangles,
                                 int *nodeIndex)
{
    if (!triangles || !nodeIndex)
        return;

    const int currentNodeIndex = (*nodeIndex)++;
    const QMatrix4x4 nodeTransform = resolveNodeTransform(node, parentTransform, transformHints);
    for (const auto *mesh : selectPreferredLodMeshes(node)) {
        const QColor fallback = fallbackMaterialColor(mesh->materialName, currentNodeIndex);
        const QImage texture = flatlas::infrastructure::FreelancerMaterialResolver::loadTextureForMesh(modelPath, *mesh)
                                   .convertToFormat(QImage::Format_ARGB32);
        const auto &vertices = mesh->vertices;
        const auto &indices = mesh->indices;
        for (int index = 0; index + 2 < indices.size(); index += 3) {
            const uint32_t i0 = indices.at(index);
            const uint32_t i1 = indices.at(index + 1);
            const uint32_t i2 = indices.at(index + 2);
            if (i0 >= static_cast<uint32_t>(vertices.size())
                || i1 >= static_cast<uint32_t>(vertices.size())
                || i2 >= static_cast<uint32_t>(vertices.size())) {
                continue;
            }

            const auto &v0 = vertices.at(static_cast<int>(i0));
            const auto &v1 = vertices.at(static_cast<int>(i1));
            const auto &v2 = vertices.at(static_cast<int>(i2));
            const QVector3D p0 = nodeTransform.map(v0.position);
            const QVector3D p1 = nodeTransform.map(v1.position);
            const QVector3D p2 = nodeTransform.map(v2.position);
            QVector3D normal = QVector3D::crossProduct(p1 - p0, p2 - p0);
            if (normal.lengthSquared() <= 0.0001f)
                normal = nodeTransform.mapVector((v0.normal + v1.normal + v2.normal) / 3.0f);

            const QColor base = sampleTexture(texture,
                                              (v0.u + v1.u + v2.u) / 3.0f,
                                              (v0.v + v1.v + v2.v) / 3.0f,
                                              fallback);
            triangles->append(TexturedIconTriangle{
                p0,
                p1,
                p2,
                normal,
                shadeColor(base, normal),
                texture,
                QPointF(v0.u, v0.v),
                QPointF(v1.u, v1.v),
                QPointF(v2.u, v2.v),
                (p0.y() + p1.y() + p2.y()) / 3.0f,
            });
        }
    }

    for (const auto &child : node.children)
        appendTexturedNodeTriangles(modelPath, child, nodeTransform, transformHints, triangles, nodeIndex);
}

float edgeFunction(const QPointF &a, const QPointF &b, const QPointF &c)
{
    return static_cast<float>((c.x() - a.x()) * (b.y() - a.y()) - (c.y() - a.y()) * (b.x() - a.x()));
}

void alphaBlendPixel(QImage *image, int x, int y, const QColor &source)
{
    if (!image || source.alpha() <= 0)
        return;
    QColor dest = QColor::fromRgba(image->pixel(x, y));
    const float srcA = source.alphaF();
    const float dstA = dest.alphaF();
    const float outA = srcA + dstA * (1.0f - srcA);
    if (outA <= 0.0f) {
        image->setPixelColor(x, y, Qt::transparent);
        return;
    }
    const auto blendChannel = [&](int src, int dst) {
        return qBound(0, static_cast<int>(((src * srcA) + (dst * dstA * (1.0f - srcA))) / outA), 255);
    };
    image->setPixelColor(x, y, QColor(blendChannel(source.red(), dest.red()),
                                      blendChannel(source.green(), dest.green()),
                                      blendChannel(source.blue(), dest.blue()),
                                      qBound(0, static_cast<int>(outA * 255.0f), 255)));
}

void rasterizeTexturedTriangle(QImage *image,
                               const QPolygonF &poly,
                               const QImage &texture,
                               const QPointF &uvA,
                               const QPointF &uvB,
                               const QPointF &uvC,
                               const QColor &fallback,
                               float shade)
{
    if (!image || poly.size() < 3)
        return;

    const QPointF a = poly.at(0);
    const QPointF b = poly.at(1);
    const QPointF c = poly.at(2);
    const float area = edgeFunction(a, b, c);
    if (std::abs(area) < 0.0001f)
        return;

    const QRect bounds = poly.boundingRect()
                             .adjusted(-1.0, -1.0, 1.0, 1.0)
                             .toAlignedRect()
                             .intersected(image->rect());
    for (int y = bounds.top(); y <= bounds.bottom(); ++y) {
        for (int x = bounds.left(); x <= bounds.right(); ++x) {
            const QPointF p(x + 0.5, y + 0.5);
            const float w0 = edgeFunction(b, c, p) / area;
            const float w1 = edgeFunction(c, a, p) / area;
            const float w2 = edgeFunction(a, b, p) / area;
            if (w0 < -0.001f || w1 < -0.001f || w2 < -0.001f)
                continue;

            const QPointF uv = uvA * w0 + uvB * w1 + uvC * w2;
            QColor color = sampleTexture(texture, uv, fallback);
            color.setRed(qBound(0, static_cast<int>(color.red() * shade), 232));
            color.setGreen(qBound(0, static_cast<int>(color.green() * shade), 232));
            color.setBlue(qBound(0, static_cast<int>(color.blue() * shade), 232));
            alphaBlendPixel(image, x, y, color);
        }
    }
}

} // namespace

QVector<ScreenshotTriangle> ModelScreenshotExporter::buildTriangles(
    const flatlas::infrastructure::DecodedModel &model)
{
    QVector<ScreenshotTriangle> triangles;
    QMatrix4x4 identity;
    const auto transformHints = buildTransformHintLookup(model.cmpTransformHints);
    appendNodeTriangles(model.rootNode, identity, transformHints, &triangles);
    return triangles;
}

bool ModelScreenshotExporter::exportTrianglesJson(const flatlas::infrastructure::DecodedModel &model,
                                                  const QString &outPath,
                                                  QString *errorMessage)
{
    const QVector<ScreenshotTriangle> triangles = buildTriangles(model);
    if (triangles.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Model produced no triangles.");
        return false;
    }

    const QFileInfo outInfo(outPath);
    QDir().mkpath(outInfo.absolutePath());

    QJsonArray triangleArray;
    for (const auto &triangle : triangles)
        triangleArray.append(QJsonArray{vectorToJson(triangle.a), vectorToJson(triangle.b), vectorToJson(triangle.c)});

    QJsonObject root;
    root.insert(QStringLiteral("sourcePath"), model.sourcePath);
    root.insert(QStringLiteral("triangleCount"), triangles.size());
    root.insert(QStringLiteral("triangles"), triangleArray);

    QSaveFile file(outPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not open output file: %1").arg(outPath);
        return false;
    }

    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if (file.write(payload) != payload.size()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not write JSON payload: %1").arg(outPath);
        return false;
    }
    if (!file.commit()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not finalize output file: %1").arg(outPath);
        return false;
    }

    return true;
}

bool ModelScreenshotExporter::exportModelFileToJson(const QString &modelPath,
                                                    const QString &outPath,
                                                    QString *errorMessage)
{
    const auto model = flatlas::infrastructure::CmpLoader::loadModel(modelPath);
    if (!model.isValid()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Model could not be decoded: %1").arg(modelPath);
        return false;
    }

    return exportTrianglesJson(model, outPath, errorMessage);
}

bool ModelScreenshotExporter::exportTexturedTopViewPng(const QString &modelPath,
                                                       const QString &outPath,
                                                       int size,
                                                       QString *errorMessage)
{
    const int outputSize = qBound(64, size, 2048);
    const int imageSize = outputSize * 2;
    const auto model = flatlas::infrastructure::CmpLoader::loadModel(modelPath);
    if (!model.isValid()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Model could not be decoded: %1").arg(modelPath);
        return false;
    }

    QVector<TexturedIconTriangle> triangles;
    const QHash<QString, flatlas::infrastructure::CmpTransformHint> transformHints;
    int nodeIndex = 0;
    QMatrix4x4 identity;
    appendTexturedNodeTriangles(modelPath, model.rootNode, identity, transformHints, &triangles, &nodeIndex);
    if (triangles.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Model produced no renderable triangles: %1").arg(modelPath);
        return false;
    }

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();
    for (const auto &triangle : triangles) {
        const QVector3D points[] = {triangle.a, triangle.b, triangle.c};
        for (const QVector3D &p : points) {
            minX = std::min(minX, p.x());
            maxX = std::max(maxX, p.x());
            minZ = std::min(minZ, p.z());
            maxZ = std::max(maxZ, p.z());
        }
    }

    const float spanX = std::max(1.0f, maxX - minX);
    const float spanZ = std::max(1.0f, maxZ - minZ);
    const float scale = (static_cast<float>(imageSize) * 0.86f) / std::max(spanX, spanZ);
    const float centerX = (minX + maxX) * 0.5f;
    const float centerZ = (minZ + maxZ) * 0.5f;
    const QPointF imageCenter(imageSize * 0.5, imageSize * 0.5);

    std::sort(triangles.begin(), triangles.end(), [](const TexturedIconTriangle &left, const TexturedIconTriangle &right) {
        return left.depth < right.depth;
    });

    QImage image(imageSize, imageSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    auto project = [&](const QVector3D &point) {
        return QPointF(imageCenter.x() + (point.x() - centerX) * scale,
                       imageCenter.y() + (point.z() - centerZ) * scale);
    };

    for (const auto &triangle : triangles) {
        QPolygonF poly;
        poly << project(triangle.a) << project(triangle.b) << project(triangle.c);
        if (!triangle.texture.isNull()) {
            const float baseValue = std::max(0.01f, static_cast<float>(sampleTexture(triangle.texture, triangle.uvA, triangle.color).valueF()));
            const float shade = qBound(0.35f, static_cast<float>(triangle.color.valueF()) / baseValue, 1.25f);
            rasterizeTexturedTriangle(&image,
                                      poly,
                                      triangle.texture,
                                      triangle.uvA,
                                      triangle.uvB,
                                      triangle.uvC,
                                      triangle.color,
                                      shade);
            continue;
        }

        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(triangle.color);
        painter.drawPolygon(poly);
    }

    QDir().mkpath(QFileInfo(outPath).absolutePath());
    const QImage finalImage = image.scaled(outputSize,
                                           outputSize,
                                           Qt::IgnoreAspectRatio,
                                           Qt::SmoothTransformation);
    if (!finalImage.save(outPath, "PNG")) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not save PNG: %1").arg(outPath);
        return false;
    }

    return true;
}

bool ModelScreenshotExporter::exportTexturedPerspectivePng(const QString &modelPath,
                                                           const QString &outPath,
                                                           int size,
                                                           float yawDegrees,
                                                           float pitchDegrees,
                                                           QString *errorMessage)
{
    const int outputSize = qBound(64, size, 2048);
    const int imageSize = outputSize * 2;
    const auto model = flatlas::infrastructure::CmpLoader::loadModel(modelPath);
    if (!model.isValid()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Model could not be decoded: %1").arg(modelPath);
        return false;
    }

    QVector<TexturedIconTriangle> triangles;
    const QHash<QString, flatlas::infrastructure::CmpTransformHint> transformHints;
    int nodeIndex = 0;
    QMatrix4x4 identity;
    appendTexturedNodeTriangles(modelPath, model.rootNode, identity, transformHints, &triangles, &nodeIndex);
    if (triangles.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Model produced no renderable triangles: %1").arg(modelPath);
        return false;
    }

    QVector3D minPoint(std::numeric_limits<float>::max(),
                       std::numeric_limits<float>::max(),
                       std::numeric_limits<float>::max());
    QVector3D maxPoint(std::numeric_limits<float>::lowest(),
                       std::numeric_limits<float>::lowest(),
                       std::numeric_limits<float>::lowest());
    for (const auto &triangle : triangles) {
        const QVector3D points[] = {triangle.a, triangle.b, triangle.c};
        for (const QVector3D &p : points) {
            minPoint.setX(std::min(minPoint.x(), p.x()));
            minPoint.setY(std::min(minPoint.y(), p.y()));
            minPoint.setZ(std::min(minPoint.z(), p.z()));
            maxPoint.setX(std::max(maxPoint.x(), p.x()));
            maxPoint.setY(std::max(maxPoint.y(), p.y()));
            maxPoint.setZ(std::max(maxPoint.z(), p.z()));
        }
    }

    const QVector3D center = (minPoint + maxPoint) * 0.5f;
    constexpr float pi = 3.14159265358979323846f;
    const float yaw = yawDegrees * pi / 180.0f;
    const float pitch = pitchDegrees * pi / 180.0f;
    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);
    const float cp = std::cos(pitch);
    const float sp = std::sin(pitch);

    auto cameraProject = [&](const QVector3D &point) {
        const QVector3D local = point - center;
        const float x1 = cy * local.x() + sy * local.z();
        const float z1 = -sy * local.x() + cy * local.z();
        const float y1 = local.y();
        const float y2 = cp * y1 - sp * z1;
        const float z2 = sp * y1 + cp * z1;
        return QVector3D(x1, -y2, z2);
    };

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    for (const auto &triangle : triangles) {
        const QVector3D points[] = {cameraProject(triangle.a), cameraProject(triangle.b), cameraProject(triangle.c)};
        for (const QVector3D &p : points) {
            minX = std::min(minX, p.x());
            maxX = std::max(maxX, p.x());
            minY = std::min(minY, p.y());
            maxY = std::max(maxY, p.y());
        }
    }

    const float spanX = std::max(1.0f, maxX - minX);
    const float spanY = std::max(1.0f, maxY - minY);
    const float scale = (static_cast<float>(imageSize) * 0.86f) / std::max(spanX, spanY);
    const QPointF projectedCenter((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);
    const QPointF imageCenter(imageSize * 0.5, imageSize * 0.5);

    struct ProjectedTriangle {
        TexturedIconTriangle triangle;
        QPolygonF poly;
        float depth = 0.0f;
    };

    QVector<ProjectedTriangle> projected;
    projected.reserve(triangles.size());
    auto toImagePoint = [&](const QVector3D &cameraPoint) {
        return QPointF(imageCenter.x() + (cameraPoint.x() - projectedCenter.x()) * scale,
                       imageCenter.y() + (cameraPoint.y() - projectedCenter.y()) * scale);
    };

    for (const auto &triangle : triangles) {
        const QVector3D a = cameraProject(triangle.a);
        const QVector3D b = cameraProject(triangle.b);
        const QVector3D c = cameraProject(triangle.c);
        QPolygonF poly;
        poly << toImagePoint(a) << toImagePoint(b) << toImagePoint(c);
        projected.append(ProjectedTriangle{triangle, poly, (a.z() + b.z() + c.z()) / 3.0f});
    }

    std::sort(projected.begin(), projected.end(), [](const ProjectedTriangle &left, const ProjectedTriangle &right) {
        return left.depth < right.depth;
    });

    QImage image(imageSize, imageSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    for (const auto &entry : projected) {
        const auto &triangle = entry.triangle;
        if (!triangle.texture.isNull()) {
            const float baseValue = std::max(0.01f, static_cast<float>(sampleTexture(triangle.texture, triangle.uvA, triangle.color).valueF()));
            const float shade = qBound(0.35f, static_cast<float>(triangle.color.valueF()) / baseValue, 1.25f);
            rasterizeTexturedTriangle(&image,
                                      entry.poly,
                                      triangle.texture,
                                      triangle.uvA,
                                      triangle.uvB,
                                      triangle.uvC,
                                      triangle.color,
                                      shade);
            continue;
        }

        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(triangle.color);
        painter.drawPolygon(entry.poly);
    }

    QDir().mkpath(QFileInfo(outPath).absolutePath());
    const QImage finalImage = image.scaled(outputSize,
                                           outputSize,
                                           Qt::IgnoreAspectRatio,
                                           Qt::SmoothTransformation);
    if (!finalImage.save(outPath, "PNG")) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not save PNG: %1").arg(outPath);
        return false;
    }

    return true;
}

} // namespace flatlas::tools
