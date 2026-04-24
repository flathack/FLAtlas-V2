#include "RingPreviewSceneBuilder.h"

#include "core/PathUtils.h"
#include "infrastructure/parser/IniParser.h"
#include "rendering/preview/ModelCache.h"

#include <QFileInfo>
#include <QQuaternion>

#include <cmath>

namespace flatlas::rendering {
namespace {

using flatlas::infrastructure::IniDocument;
using flatlas::infrastructure::IniParser;
using flatlas::infrastructure::IniSection;
using flatlas::infrastructure::MeshData;
using flatlas::infrastructure::MeshVertex;
using flatlas::infrastructure::ModelNode;

QString normalizedPath(const QString &value)
{
    QString normalized = value.trimmed();
    normalized.replace(QLatin1Char('/'), QLatin1Char('\\'));
    return normalized;
}

QColor parseRgbColor(const QString &text, const QColor &fallback)
{
    const QStringList parts = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (parts.size() < 3)
        return fallback;

    bool okRed = false;
    bool okGreen = false;
    bool okBlue = false;
    const int red = parts[0].trimmed().toInt(&okRed);
    const int green = parts[1].trimmed().toInt(&okGreen);
    const int blue = parts[2].trimmed().toInt(&okBlue);
    if (!okRed || !okGreen || !okBlue)
        return fallback;
    return QColor(qBound(0, red, 255), qBound(0, green, 255), qBound(0, blue, 255));
}

QString previewColorTag(const QColor &color)
{
    return QStringLiteral("preview_color:%1,%2,%3")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue());
}

void appendVertex(MeshData &mesh,
                  const QVector3D &position,
                  const QVector3D &normal,
                  float u,
                  float v)
{
    MeshVertex vertex;
    vertex.position = position;
    vertex.normal = normal;
    vertex.u = u;
    vertex.v = v;
    mesh.vertices.append(vertex);
}

void appendQuad(MeshData &mesh,
                const QVector3D &a,
                const QVector3D &b,
                const QVector3D &c,
                const QVector3D &d,
                const QVector3D &normal,
                float u0,
                float u1)
{
    const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
    appendVertex(mesh, a, normal, u0, 1.0f);
    appendVertex(mesh, b, normal, u0, 0.0f);
    appendVertex(mesh, c, normal, u1, 1.0f);
    appendVertex(mesh, d, normal, u1, 0.0f);
    mesh.indices.append(base + 0);
    mesh.indices.append(base + 1);
    mesh.indices.append(base + 2);
    mesh.indices.append(base + 2);
    mesh.indices.append(base + 1);
    mesh.indices.append(base + 3);
}

MeshData buildSolidSphereMesh(double radius,
                              int latitudeSegments,
                              int longitudeSegments,
                              const QColor &color)
{
    MeshData mesh;
    mesh.materialName = QStringLiteral("host-radius-preview");
    mesh.materialValue = previewColorTag(color);

    const float sphereRadius = qMax(0.01f, static_cast<float>(radius));
    const int latSegments = qMax(6, latitudeSegments);
    const int lonSegments = qMax(12, longitudeSegments);
    constexpr float pi = 3.14159265358979323846f;
    constexpr float tau = 6.28318530717958647692f;

    for (int lat = 0; lat <= latSegments; ++lat) {
        const float v = static_cast<float>(lat) / static_cast<float>(latSegments);
        const float phi = v * pi;
        const float sinPhi = std::sin(phi);
        const float cosPhi = std::cos(phi);

        for (int lon = 0; lon <= lonSegments; ++lon) {
            const float u = static_cast<float>(lon) / static_cast<float>(lonSegments);
            const float theta = u * tau;
            const float sinTheta = std::sin(theta);
            const float cosTheta = std::cos(theta);
            const QVector3D normal(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
            appendVertex(mesh, normal * sphereRadius, normal, u, v);
        }
    }

    const int stride = lonSegments + 1;
    for (int lat = 0; lat < latSegments; ++lat) {
        for (int lon = 0; lon < lonSegments; ++lon) {
            const uint32_t topLeft = static_cast<uint32_t>(lat * stride + lon);
            const uint32_t topRight = topLeft + 1;
            const uint32_t bottomLeft = static_cast<uint32_t>((lat + 1) * stride + lon);
            const uint32_t bottomRight = bottomLeft + 1;

            if (lat > 0) {
                mesh.indices.append(topLeft);
                mesh.indices.append(bottomLeft);
                mesh.indices.append(topRight);
            }
            if (lat + 1 < latSegments) {
                mesh.indices.append(topRight);
                mesh.indices.append(bottomLeft);
                mesh.indices.append(bottomRight);
            }
        }
    }

    return mesh;
}

MeshData buildSolidAnnulusMesh(double innerRadius,
                               double outerRadius,
                               double thickness,
                               int segments,
                               const QColor &color)
{
    MeshData mesh;
    mesh.materialName = QStringLiteral("ring-preview");
    mesh.materialValue = previewColorTag(color);

    const float inner = qMax(0.01f, static_cast<float>(innerRadius));
    const float outer = qMax(inner + 0.01f, static_cast<float>(outerRadius));
    const float halfHeight = qMax(0.005f, static_cast<float>(thickness) * 0.5f);
    const int segmentCount = qMax(12, segments);
    constexpr float tau = 6.28318530717958647692f;

    for (int index = 0; index < segmentCount; ++index) {
        const float t0 = static_cast<float>(index) / static_cast<float>(segmentCount);
        const float t1 = static_cast<float>(index + 1) / static_cast<float>(segmentCount);
        const float angle0 = t0 * tau;
        const float angle1 = t1 * tau;

        const float cos0 = std::cos(angle0);
        const float sin0 = std::sin(angle0);
        const float cos1 = std::cos(angle1);
        const float sin1 = std::sin(angle1);

        const QVector3D topOuter0(outer * cos0, halfHeight, outer * sin0);
        const QVector3D topInner0(inner * cos0, halfHeight, inner * sin0);
        const QVector3D topOuter1(outer * cos1, halfHeight, outer * sin1);
        const QVector3D topInner1(inner * cos1, halfHeight, inner * sin1);
        const QVector3D bottomOuter0(outer * cos0, -halfHeight, outer * sin0);
        const QVector3D bottomInner0(inner * cos0, -halfHeight, inner * sin0);
        const QVector3D bottomOuter1(outer * cos1, -halfHeight, outer * sin1);
        const QVector3D bottomInner1(inner * cos1, -halfHeight, inner * sin1);

        appendQuad(mesh, topOuter0, topInner0, topOuter1, topInner1, QVector3D(0.0f, 1.0f, 0.0f), t0, t1);
        appendQuad(mesh, bottomOuter0, bottomOuter1, bottomInner0, bottomInner1, QVector3D(0.0f, -1.0f, 0.0f), t0, t1);
        appendQuad(mesh, topOuter0, bottomOuter0, topOuter1, bottomOuter1,
                   QVector3D(cos0 + cos1, 0.0f, sin0 + sin1).normalized(), t0, t1);
        appendQuad(mesh, topInner0, topInner1, bottomInner0, bottomInner1,
                   QVector3D(-(cos0 + cos1), 0.0f, -(sin0 + sin1)).normalized(), t0, t1);
    }

    return mesh;
}

QString ringPresetAbsolutePath(const QString &gameRoot, const QString &ringPreset)
{
    const QString normalizedPreset = normalizedPath(ringPreset);
    if (normalizedPreset.isEmpty())
        return {};

    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    if (dataDir.isEmpty())
        return {};

    return flatlas::core::PathUtils::ciResolvePath(dataDir, normalizedPreset);
}

} // namespace

RingPreviewVisualStyle RingPreviewSceneBuilder::loadVisualStyle(const QString &gameRoot, const QString &ringPreset)
{
    RingPreviewVisualStyle style;
    const QString presetPath = ringPresetAbsolutePath(gameRoot, ringPreset);
    if (presetPath.isEmpty() || !QFileInfo::exists(presetPath))
        return style;

    const IniDocument document = IniParser::parseFile(presetPath);
    for (const IniSection &section : document) {
        if (section.name.compare(QStringLiteral("Exterior"), Qt::CaseInsensitive) != 0)
            continue;
        style.color = parseRgbColor(section.value(QStringLiteral("color")).trimmed(), style.color);
        bool okOpacity = false;
        const float opacity = section.value(QStringLiteral("opacity")).trimmed().toFloat(&okOpacity);
        if (okOpacity)
            style.opacity = qBound(0.15f, opacity, 1.0f);
        bool okSegments = false;
        const int segments = section.value(QStringLiteral("num_segments")).trimmed().toInt(&okSegments);
        if (okSegments)
            style.segments = qBound(12, segments, 256);
        break;
    }

    return style;
}

RingPreviewSceneResult RingPreviewSceneBuilder::build(const RingPreviewSceneRequest &request)
{
    RingPreviewSceneResult result;
    result.sceneRoot.name = QStringLiteral("ring-preview-root");
    result.visualStyle = loadVisualStyle(request.gameRoot, request.ringPreset);

    if (!request.hostModelPath.trimmed().isEmpty() && QFileInfo::exists(request.hostModelPath)) {
        const auto decoded = ModelCache::instance().load(request.hostModelPath);
        if (decoded.isValid()) {
            ModelNode hostNode;
            hostNode.name = QStringLiteral("ring-preview-host");
            hostNode.children.append(decoded.rootNode);
            result.sceneRoot.children.append(hostNode);
            result.hasHostModel = true;
        } else {
            result.statusMessage = QObject::tr("Host-Modell konnte nicht geladen werden. Die Ring-Vorschau wird ohne Objekt angezeigt.");
        }
    } else if (!request.hostModelPath.trimmed().isEmpty()) {
        result.statusMessage = QObject::tr("Host-Modell konnte nicht aufgeloest werden. Die Ring-Vorschau wird ohne Objekt angezeigt.");
    }

    if (request.showHostRadiusSphere && request.hostRadius > 0.0) {
        ModelNode sphereNode;
        sphereNode.name = QStringLiteral("ring-preview-host-radius");
        sphereNode.meshes.append(buildSolidSphereMesh(request.hostRadius,
                                                      24,
                                                      48,
                                                      request.hostRadiusSphereIsSun
                                                          ? QColor(245, 208, 96)
                                                          : QColor(108, 134, 178)));
        result.sceneRoot.children.append(sphereNode);
    }

    if (request.innerRadius > 0.0 && request.outerRadius > request.innerRadius && request.thickness > 0.0) {
        result.geometryInputsValid = true;
        ModelNode ringNode;
        ringNode.name = QStringLiteral("ring-preview-geometry");
        ringNode.rotation = QQuaternion::fromEulerAngles(request.rotationEuler.x(),
                                                         request.rotationEuler.y(),
                                                         request.rotationEuler.z());
        ringNode.meshes.append(buildSolidAnnulusMesh(request.innerRadius,
                                                     request.outerRadius,
                                                     request.thickness,
                                                     result.visualStyle.segments,
                                                     result.visualStyle.color));
        result.sceneRoot.children.append(ringNode);
        result.hasRingGeometry = true;
    } else if (result.hasHostModel) {
        result.statusMessage = QObject::tr("Ungueltige Ringabmessungen: Die Vorschau zeigt aktuell nur das Host-Objekt.");
    } else {
        result.statusMessage = QObject::tr("Ungueltige Ringabmessungen und kein Host-Modell verfuegbar.");
    }

    result.hasRenderableScene = !result.sceneRoot.children.isEmpty() || !result.sceneRoot.meshes.isEmpty();
    return result;
}

} // namespace flatlas::rendering