#pragma once
// infrastructure/io/VmeshDecoder.h – VMESH-Geometrie-Dekodierung (Phase 8)

#include <QByteArray>
#include <QVector>
#include <QVector3D>
#include <QVector2D>
#include <cstdint>

namespace flatlas::infrastructure {

/// Flexible Vertex Format flags used in Freelancer VMESH data.
enum FVF : uint32_t {
    FVF_XYZ      = 0x002,   // Position (3 floats)
    FVF_NORMAL   = 0x010,   // Normal (3 floats)
    FVF_DIFFUSE  = 0x040,   // Diffuse color (1 uint32)
    // The texture-coordinate count is a numeric value in bits 8–11 (D3DFVF layout).
    // These named constants equal the raw field value for common counts.
    // Use uvSetCount() = (fvf & 0x0F00) >> 8 to read the actual count.
    FVF_TEX1     = 0x100,   // tex-count field = 1  (1 UV set)
    FVF_TEX2     = 0x200,   // tex-count field = 2  (2 UV sets)
    FVF_TEX4     = 0x400,   // tex-count field = 4  (4 UV sets; 0x300 = 3 sets)
    FVF_TEX8     = 0x800,   // tex-count field = 8  (8 UV sets)
};

/// Decoded mesh result from VMESH data.
struct DecodedMesh {
    QVector<QVector3D> positions;
    QVector<QVector3D> normals;
    QVector<QVector2D> uvs;       // First UV set
    QVector<uint32_t> indices;
    uint32_t fvf = 0;
    int numVertices = 0;
    int numIndices = 0;
};

/// A mesh group within a VMESH ref block – references a subset of the VMESH.
struct MeshGroup {
    int materialId = 0;
    int startVertex = 0;
    int endVertex = 0;
    int numRefVertices = 0;
    int startIndex = 0;    // Calculated from startMesh * 3
    int numIndices = 0;    // triCount * 3
};

class VmeshDecoder {
public:
    /// Decode a VMeshData block (the raw bytes from the UTF node).
    static DecodedMesh decode(const QByteArray &data, int offset = 0);

    /// Decode a structured single-block VMesh layout at the given byte offset.
    static DecodedMesh decodeStructuredSingleBlock(const QByteArray &data, int offset = 0);

    /// Scan a raw VMeshData block for plausible structured single-block headers.
    static QVector<int> findStructuredSingleBlockOffsets(
        const QByteArray &data,
        int minMeshCount = 1,
        int minIndexCount = 3,
        int minVertexCount = 3);

    /// Decode a VMeshRef block – returns mesh group definitions referencing a VMeshData.
    static QVector<MeshGroup> decodeRef(const QByteArray &data, int offset = 0);

    /// Compute stride in bytes for a given FVF flag set.
    static int vertexStride(uint32_t fvf);

    /// Count number of UV sets for a given FVF.
    static int uvSetCount(uint32_t fvf);

    static bool isSupportedStructuredSingleBlockFvf(uint32_t fvf);
};

} // namespace flatlas::infrastructure
