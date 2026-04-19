// infrastructure/io/VmeshDecoder.cpp – VMESH-Geometrie-Dekodierung (Phase 8)

#include "VmeshDecoder.h"
#include <QDataStream>
#include <QIODevice>

namespace flatlas::infrastructure {

int VmeshDecoder::uvSetCount(uint32_t fvf)
{
    // The tex-coord count is encoded in the upper nibble
    if (fvf & FVF_TEX8) return 8;
    if (fvf & FVF_TEX4) return 4;
    if (fvf & FVF_TEX2) return 2;
    if (fvf & FVF_TEX1) return 1;
    return 0;
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
    DecodedMesh result;

    if (data.size() - offset < 16)
        return result;

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

    result.fvf = fvfRaw;
    result.numVertices = numVertices;
    result.numIndices = numRefVertices;

    // Skip mesh group headers (each 16 bytes: materialId, startVertex, endVertex,
    // numRefVertices, padding[2])
    ds.skipRawData(numMeshGroups * 16);

    // Read indices (uint16)
    result.indices.resize(numRefVertices);
    for (int i = 0; i < numRefVertices; ++i) {
        uint16_t idx;
        ds >> idx;
        result.indices[i] = idx;
    }

    // Read vertices
    const int stride = vertexStride(fvfRaw);
    if (stride == 0)
        return result;

    result.positions.resize(numVertices);
    if (fvfRaw & FVF_NORMAL)
        result.normals.resize(numVertices);
    if (uvSetCount(fvfRaw) > 0)
        result.uvs.resize(numVertices);

    for (int i = 0; i < numVertices; ++i) {
        if (fvfRaw & FVF_XYZ) {
            float x, y, z;
            ds >> x >> y >> z;
            result.positions[i] = QVector3D(x, y, z);
        }
        if (fvfRaw & FVF_NORMAL) {
            float nx, ny, nz;
            ds >> nx >> ny >> nz;
            result.normals[i] = QVector3D(nx, ny, nz);
        }
        if (fvfRaw & FVF_DIFFUSE) {
            uint32_t diffuse;
            ds >> diffuse;
            // Diffuse color ignored for now
        }
        int numUV = uvSetCount(fvfRaw);
        for (int uv = 0; uv < numUV; ++uv) {
            float u, v;
            ds >> u >> v;
            if (uv == 0)
                result.uvs[i] = QVector2D(u, v);
        }
    }

    return result;
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
