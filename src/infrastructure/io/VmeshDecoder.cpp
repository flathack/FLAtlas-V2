// infrastructure/io/VmeshDecoder.cpp – VMESH-Geometrie-Dekodierung (Phase 8)

#include "VmeshDecoder.h"
#include <QDataStream>
#include <QIODevice>
#include <QtEndian>
#include <QtMath>

#include <limits>

namespace flatlas::infrastructure {

namespace {

bool isValidDecodedMesh(const DecodedMesh &mesh)
{
    if (mesh.positions.size() < 3 || mesh.indices.size() < 3)
        return false;
    uint32_t maxIndex = 0;
    bool hasIndex = false;
    for (uint32_t index : mesh.indices) {
        maxIndex = qMax(maxIndex, index);
        hasIndex = true;
    }
    return hasIndex && maxIndex < static_cast<uint32_t>(mesh.positions.size());
}

float decodedMeshScore(const DecodedMesh &mesh)
{
    if (!isValidDecodedMesh(mesh))
        return std::numeric_limits<float>::infinity();

    float score = 0.0f;

    int degenerateTriangles = 0;
    for (qsizetype i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const uint32_t a = mesh.indices.at(i);
        const uint32_t b = mesh.indices.at(i + 1);
        const uint32_t c = mesh.indices.at(i + 2);
        if (a == b || b == c || a == c)
            ++degenerateTriangles;
    }
    score += static_cast<float>(degenerateTriangles) * 100.0f;

    int suspiciousSmallVertices = 0;
    for (const auto &position : mesh.positions) {
        if (qMax(qMax(qAbs(position.x()), qAbs(position.y())), qAbs(position.z())) > 2'000'000.0f)
            return std::numeric_limits<float>::infinity();
        int smallAxes = 0;
        smallAxes += qAbs(position.x()) <= 2.0f ? 1 : 0;
        smallAxes += qAbs(position.y()) <= 2.0f ? 1 : 0;
        smallAxes += qAbs(position.z()) <= 2.0f ? 1 : 0;
        if (smallAxes >= 2)
            ++suspiciousSmallVertices;
    }
    score += static_cast<float>(suspiciousSmallVertices) * 5.0f;

    return score;
}

bool decodeVertices(QDataStream &ds, uint32_t fvfRaw, int numVertices, DecodedMesh &result)
{
    const int stride = VmeshDecoder::vertexStride(fvfRaw);
    if (stride == 0 || numVertices <= 0)
        return false;

    result.positions.resize(numVertices);
    if (fvfRaw & FVF_NORMAL)
        result.normals.resize(numVertices);
    if (VmeshDecoder::uvSetCount(fvfRaw) > 0)
        result.uvs.resize(numVertices);

    for (int i = 0; i < numVertices; ++i) {
        if (fvfRaw & FVF_XYZ) {
            float x = 0.0f, y = 0.0f, z = 0.0f;
            ds >> x >> y >> z;
            if (!qIsFinite(x) || !qIsFinite(y) || !qIsFinite(z))
                return false;
            result.positions[i] = QVector3D(x, y, z);
        }
        if (fvfRaw & FVF_NORMAL) {
            float nx = 0.0f, ny = 0.0f, nz = 0.0f;
            ds >> nx >> ny >> nz;
            if (!qIsFinite(nx) || !qIsFinite(ny) || !qIsFinite(nz))
                return false;
            result.normals[i] = QVector3D(nx, ny, nz);
        }
        if (fvfRaw & FVF_DIFFUSE) {
            uint32_t diffuse = 0;
            ds >> diffuse;
        }
        const int numUV = VmeshDecoder::uvSetCount(fvfRaw);
        for (int uv = 0; uv < numUV; ++uv) {
            float u = 0.0f, v = 0.0f;
            ds >> u >> v;
            if (!qIsFinite(u) || !qIsFinite(v))
                return false;
            if (uv == 0)
                result.uvs[i] = QVector2D(u, v);
        }
    }

    return true;
}

DecodedMesh decodeIndicesThenVertices(const QByteArray &data,
                                      int offset,
                                      uint16_t numMeshGroups,
                                      uint16_t numRefVertices,
                                      uint16_t fvfRaw,
                                      uint16_t numVertices)
{
    DecodedMesh result;
    result.fvf = fvfRaw;
    result.numVertices = numVertices;
    result.numIndices = numRefVertices;

    QDataStream ds(data);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
    ds.skipRawData(offset + 16);

    ds.skipRawData(numMeshGroups * 16);

    result.indices.resize(numRefVertices);
    for (int i = 0; i < numRefVertices; ++i) {
        uint16_t idx = 0;
        ds >> idx;
        result.indices[i] = idx;
    }

    if (!decodeVertices(ds, fvfRaw, numVertices, result))
        return {};

    return result;
}

DecodedMesh decodeVerticesThenIndices(const QByteArray &data,
                                      int offset,
                                      uint16_t numMeshGroups,
                                      uint16_t numRefVertices,
                                      uint16_t fvfRaw,
                                      uint16_t numVertices)
{
    DecodedMesh result;
    result.fvf = fvfRaw;
    result.numVertices = numVertices;
    result.numIndices = numRefVertices;

    const int vertexStride = VmeshDecoder::vertexStride(fvfRaw);
    if (vertexStride <= 0)
        return {};

    QDataStream ds(data);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
    ds.skipRawData(offset + 16);

    const int headerBytes = numMeshGroups * 16;
    ds.skipRawData(headerBytes);

    if (!decodeVertices(ds, fvfRaw, numVertices, result))
        return {};

    result.indices.resize(numRefVertices);
    for (int i = 0; i < numRefVertices; ++i) {
        uint16_t idx = 0;
        ds >> idx;
        result.indices[i] = idx;
    }

    const int expectedBytes = 16 + headerBytes + (numVertices * vertexStride) + (numRefVertices * 2);
    if (data.size() - offset < expectedBytes)
        return {};

    return result;
}

} // namespace

int VmeshDecoder::uvSetCount(uint32_t fvf)
{
    // The texture-coordinate count is a *numeric* value stored in bits 8–11 of the
    // D3D Flexible Vertex Format, NOT a set of independent flag bits.
    // 0x100 → 1 set, 0x200 → 2 sets, 0x300 → 3 sets, 0x400 → 4 sets, …
    return static_cast<int>((fvf & 0x0F00u) >> 8);
}

bool VmeshDecoder::isSupportedStructuredSingleBlockFvf(uint32_t fvf)
{
    const uint32_t baseBits = fvf & ~0x700u;
    if (baseBits != 0x2u && baseBits != 0x12u && baseBits != 0x42u && baseBits != 0x52u)
        return false;
    return uvSetCount(fvf) <= 5;
}

int VmeshDecoder::vertexStride(uint32_t fvf)
{
    int stride = 0;
    if (fvf & FVF_XYZ)     stride += 3 * sizeof(float);  // Position
    if (fvf & FVF_NORMAL)  stride += 3 * sizeof(float);  // Normal
    if (fvf & FVF_DIFFUSE) stride += sizeof(uint32_t);    // Diffuse color
    stride += uvSetCount(fvf) * 2 * sizeof(float);        // UV sets
    return stride;
}

DecodedMesh VmeshDecoder::decode(const QByteArray &data, int offset)
{
    if (data.size() - offset < 16)
        return {};

    QDataStream ds(data);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
    ds.skipRawData(offset);

    // VMeshData header:
    //   uint32 meshType     (0x01 = standard)
    //   uint32 surfaceType  (0x04 = D3DPT_TRIANGLELIST)
    //   uint16 numMeshGroups
    //   uint16 numRefVertices  (total index count)
    //   uint16 fvf (flexible vertex format)
    //   uint16 numVertices

    uint32_t meshType, surfaceType;
    uint16_t numMeshGroups, numRefVertices, fvfRaw, numVertices;

    ds >> meshType >> surfaceType;
    ds >> numMeshGroups >> numRefVertices >> fvfRaw >> numVertices;

    const DecodedMesh indicesThenVertices = decodeIndicesThenVertices(
        data, offset, numMeshGroups, numRefVertices, fvfRaw, numVertices);
    const DecodedMesh verticesThenIndices = decodeVerticesThenIndices(
        data, offset, numMeshGroups, numRefVertices, fvfRaw, numVertices);

    const float legacyScore = decodedMeshScore(indicesThenVertices);
    const float v1Score = decodedMeshScore(verticesThenIndices);

    if (legacyScore == std::numeric_limits<float>::infinity()
        && v1Score == std::numeric_limits<float>::infinity()) {
        return {};
    }

    return v1Score < legacyScore ? verticesThenIndices : indicesThenVertices;
}

DecodedMesh VmeshDecoder::decodeStructuredSingleBlock(const QByteArray &data, int offset)
{
    DecodedMesh result;
    if (data.size() - offset < 16)
        return result;

    const char *raw = data.constData() + offset;
    const int remaining = data.size() - offset;

    quint32 unknown0 = 0;
    quint32 unknown1 = 0;
    quint16 meshCount = 0;
    quint16 numRefVertices = 0;
    quint16 fvfRaw = 0;
    quint16 numVertices = 0;

    QDataStream ds(QByteArray::fromRawData(raw, remaining));
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
    ds >> unknown0 >> unknown1 >> meshCount >> numRefVertices >> fvfRaw >> numVertices;

    if (meshCount == 0 || numRefVertices < 3 || numVertices < 3)
        return {};
    if (!isSupportedStructuredSingleBlockFvf(fvfRaw))
        return {};

    struct MeshHeader {
        quint16 startVertex = 0;
        quint16 endVertex = 0;
        quint16 numRefIndices = 0;
    };
    QVector<MeshHeader> meshHeaders;
    meshHeaders.reserve(meshCount);
    int totalIndices = 0;
    for (int i = 0; i < meshCount; ++i) {
        if (ds.device()->bytesAvailable() < 12)
            return {};
        quint32 materialId = 0;
        quint16 startVertex = 0, endVertex = 0, numRefIndices = 0, padding = 0;
        ds >> materialId >> startVertex >> endVertex >> numRefIndices >> padding;
        if (endVertex < startVertex)
            return {};
        meshHeaders.append({startVertex, endVertex, numRefIndices});
        totalIndices += numRefIndices;
    }

    if (totalIndices != numRefVertices)
        return {};
    if (ds.device()->bytesAvailable() < totalIndices * 2)
        return {};

    result.indices.reserve(totalIndices);
    for (int i = 0; i < totalIndices / 3; ++i) {
        quint16 v1 = 0, v3 = 0, v2 = 0;
        ds >> v1 >> v3 >> v2;
        result.indices.append(v1);
        result.indices.append(v2);
        result.indices.append(v3);
    }

    result.fvf = fvfRaw;
    result.numVertices = numVertices;
    result.numIndices = totalIndices;
    if (!decodeVertices(ds, fvfRaw, numVertices, result))
        return {};
    return result;
}

QVector<int> VmeshDecoder::findStructuredSingleBlockOffsets(
    const QByteArray &data,
    int minMeshCount,
    int minIndexCount,
    int minVertexCount)
{
    QVector<int> offsets;
    if (data.size() < 16)
        return offsets;

    for (int offset = 0; offset <= data.size() - 16; offset += 2) {
        const uchar *base = reinterpret_cast<const uchar *>(data.constData() + offset);
        const quint16 meshCount = qFromLittleEndian<quint16>(base + 8);
        const quint16 indexCount = qFromLittleEndian<quint16>(base + 10);
        const quint16 fvfRaw = qFromLittleEndian<quint16>(base + 12);
        const quint16 vertexCount = qFromLittleEndian<quint16>(base + 14);

        if (meshCount < minMeshCount || indexCount < minIndexCount || vertexCount < minVertexCount)
            continue;
        if (!isSupportedStructuredSingleBlockFvf(fvfRaw))
            continue;
        const auto decoded = decodeStructuredSingleBlock(data, offset);
        if (!decoded.positions.isEmpty() && !decoded.indices.isEmpty())
            offsets.append(offset);
    }

    return offsets;
}

QVector<MeshGroup> VmeshDecoder::decodeRef(const QByteArray &data, int offset)
{
    QVector<MeshGroup> groups;

    if (data.size() - offset < 16)
        return groups;

    QDataStream ds(data);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.skipRawData(offset);

    // VMeshRef header:
    //   uint32 headerSize (in bytes, always 60)
    //   uint32 vmeshLibId (CRC of the VMeshData library)
    //   uint16 startVertex
    //   uint16 numVertices
    //   uint16 startIndex
    //   uint16 numIndices
    //   uint16 startMesh
    //   uint16 numMeshes

    uint32_t headerSize, vmeshLibId;
    uint16_t startVertex, numVertices, startIndex, numIndices, startMesh, numMeshes;

    ds >> headerSize >> vmeshLibId;
    ds >> startVertex >> numVertices >> startIndex >> numIndices >> startMesh >> numMeshes;

    // Read bounding box (6 floats: min xyz, max xyz) — skip
    ds.skipRawData(6 * sizeof(float));

    // The mesh ref just gives us the range; each meshgroup within is
    // defined by the VMeshData itself. We return a single group with the ref range.
    MeshGroup group;
    group.startVertex = startVertex;
    group.endVertex = startVertex + numVertices;
    group.numRefVertices = numVertices;
    group.startIndex = startIndex;
    group.numIndices = numIndices;
    groups.append(group);

    return groups;
}

} // namespace flatlas::infrastructure
