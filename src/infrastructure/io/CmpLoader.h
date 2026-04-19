#pragma once
// infrastructure/io/CmpLoader.h – CMP/3DB-Modell-Loader (Phase 8)
// Parses Freelancer's UTF (Universal Tree Format) containers.

#include <QByteArray>
#include <QString>
#include <QVector>
#include <QVector3D>
#include <QHash>
#include <memory>

namespace flatlas::infrastructure {

/// Single vertex with position, normal, and UV.
struct MeshVertex {
    QVector3D position;
    QVector3D normal;
    float u = 0;
    float v = 0;
};

/// Mesh data (a triangle list with material reference).
struct MeshData {
    QVector<MeshVertex> vertices;
    QVector<uint32_t> indices;
    QString materialName;
};

/// A node in the model hierarchy (CMP can have multiple parts).
struct ModelNode {
    QString name;
    QVector3D origin{0, 0, 0};       // Part origin offset
    QVector<MeshData> meshes;
    QVector<ModelNode> children;
};

/// A node in the UTF tree (internal representation).
struct UtfNode {
    QString name;
    QByteArray data;                  // Leaf data (empty for directories)
    QVector<std::shared_ptr<UtfNode>> children;
    bool isDirectory() const { return data.isEmpty() && !children.isEmpty(); }
};

/// Loads Freelancer CMP (compound model) and 3DB (single part) files.
class CmpLoader {
public:
    /// Load a CMP file (compound model with multiple parts).
    static ModelNode loadCmp(const QString &filePath);

    /// Load a 3DB file (single-part model).
    static ModelNode load3db(const QString &filePath);

    /// Parse a raw UTF tree from binary data.
    static std::shared_ptr<UtfNode> parseUtf(const QByteArray &data);

    /// Find a node by path (e.g. "\\VMeshLibrary\\mesh_name\\VMeshData").
    static std::shared_ptr<UtfNode> findNode(const std::shared_ptr<UtfNode> &root,
                                              const QString &path);

private:
    static void parseUtfNode(const QByteArray &data, int nodeOffset,
                              const QByteArray &stringBlock, int dataBlockOffset,
                              std::shared_ptr<UtfNode> &parent);
    static ModelNode extractPart(const std::shared_ptr<UtfNode> &partRoot,
                                  const std::shared_ptr<UtfNode> &vmeshLib);
    static MeshData buildMeshFromVMesh(const QByteArray &vmeshData,
                                        int startVertex, int numVertices,
                                        int startIndex, int numIndices);
};

} // namespace flatlas::infrastructure
