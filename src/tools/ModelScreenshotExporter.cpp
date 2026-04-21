#include "ModelScreenshotExporter.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMatrix4x4>
#include <QSaveFile>

#include <limits>

namespace flatlas::tools {
namespace {

QJsonArray vectorToJson(const QVector3D &value)
{
    return QJsonArray{value.x(), value.y(), value.z()};
}

QVector<const flatlas::infrastructure::MeshData *> selectBestLodMeshes(
    const flatlas::infrastructure::ModelNode &node)
{
    QVector<const flatlas::infrastructure::MeshData *> meshes;
    if (node.meshes.isEmpty())
        return meshes;

    int bestLodIndex = std::numeric_limits<int>::max();
    for (const auto &mesh : node.meshes) {
        if (mesh.lodIndex >= 0)
            bestLodIndex = qMin(bestLodIndex, mesh.lodIndex);
    }

    if (bestLodIndex == std::numeric_limits<int>::max()) {
        meshes.append(&node.meshes.first());
        return meshes;
    }

    for (const auto &mesh : node.meshes) {
        if (mesh.lodIndex == bestLodIndex)
            meshes.append(&mesh);
    }
    if (meshes.isEmpty())
        meshes.append(&node.meshes.first());
    return meshes;
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

    for (const auto *mesh : selectBestLodMeshes(node)) {
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

} // namespace flatlas::tools