#include "ModelScreenshotExporter.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMatrix4x4>
#include <QSaveFile>

namespace flatlas::tools {
namespace {

QJsonArray vectorToJson(const QVector3D &value)
{
    return QJsonArray{value.x(), value.y(), value.z()};
}

const flatlas::infrastructure::MeshData *selectHighestDetailMesh(const flatlas::infrastructure::ModelNode &node)
{
    if (node.meshes.isEmpty())
        return nullptr;

    const flatlas::infrastructure::MeshData *bestMesh = &node.meshes.first();
    for (const auto &mesh : node.meshes) {
        if (mesh.indices.size() > bestMesh->indices.size()) {
            bestMesh = &mesh;
            continue;
        }
        if (mesh.indices.size() == bestMesh->indices.size()
            && mesh.vertices.size() > bestMesh->vertices.size()) {
            bestMesh = &mesh;
        }
    }

    return bestMesh;
}

void appendNodeTriangles(const flatlas::infrastructure::ModelNode &node,
                         const QMatrix4x4 &parentTransform,
                         QVector<ScreenshotTriangle> *triangles)
{
    if (!triangles)
        return;

    QMatrix4x4 nodeTransform = parentTransform;
    nodeTransform.translate(node.origin);
    nodeTransform.rotate(node.rotation);

    if (const auto *mesh = selectHighestDetailMesh(node)) {
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
        appendNodeTriangles(child, nodeTransform, triangles);
}

} // namespace

QVector<ScreenshotTriangle> ModelScreenshotExporter::buildTriangles(
    const flatlas::infrastructure::DecodedModel &model)
{
    QVector<ScreenshotTriangle> triangles;
    QMatrix4x4 identity;
    appendNodeTriangles(model.rootNode, identity, &triangles);
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