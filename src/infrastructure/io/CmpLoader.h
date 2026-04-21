#pragma once
// infrastructure/io/CmpLoader.h – CMP/3DB-Modell-Loader (Phase 8)
// Parses Freelancer's UTF (Universal Tree Format) containers.

#include <QByteArray>
#include <QStringList>
#include <QQuaternion>
#include <QString>
#include <QVector>
#include <QVector3D>
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
    int materialId = -1;
    QString materialName;
    QString textureName;
    QString materialValue;
    QStringList textureCandidates;
    QString matchHint;
    QString debugHint;
};

/// A node in the model hierarchy (CMP can have multiple parts).
struct ModelNode {
    QString name;
    QVector3D origin{0, 0, 0};       // Part origin offset
    QQuaternion rotation{1.0f, 0.0f, 0.0f, 0.0f}; // Part rotation from CMP transforms
    QVector<MeshData> meshes;
    QVector<ModelNode> children;
};

enum class NativeModelFormat {
    Unknown,
    Cmp,
    Db3,
};

struct UtfFileHeader {
    QByteArray magic;
    quint32 version = 0;
    quint32 nodeBlockOffset = 0;
    quint32 nodeBlockSize = 0;
    quint32 reserved0 = 0;
    quint32 nodeEntrySize = 0;
    quint32 namesOffset = 0;
    quint32 namesAllocatedSize = 0;
    quint32 namesUsedSize = 0;
    quint32 dataOffset = 0;
    quint32 reserved1 = 0;
    quint32 reserved2 = 0;
    quint32 timestampLow = 0;
    quint32 timestampHigh = 0;

    int nodeCount() const
    {
        return nodeEntrySize > 0 ? static_cast<int>(nodeBlockSize / nodeEntrySize) : 0;
    }
};

struct UtfNodeRecord {
    QString name;
    QString parentName;
    QString path;
    quint32 flags = 0;
    quint32 peerOffset = 0;
    quint32 childOffset = 0;
    int dataOffset = -1;
    int allocatedSize = 0;
    int usedSize = 0;

    bool isDataNode() const { return (flags & 0x80u) != 0; }
};

struct NativeModelPart {
    QString name;
    int cmpIndex = -1;
    QString parentPartName;
    QString sourceName;
    QString fileName;
    QString objectName;
};

struct VMeshDataHeaderHint {
    QString structureKind = QStringLiteral("unknown");
    int meshCountHint = -1;
    int referencedVertexCountHint = -1;
    quint32 flexibleVertexFormatHint = 0;
    int vertexCountHint = -1;
    int triangleCountHint = -1;
    int meshHeaderCountHint = -1;
    int meshHeaderIndexEndHint = -1;
    int meshHeaderNumRefVerticesHint = -1;
    int meshHeaderEndVertexHint = -1;
};

struct VMeshDataBlock {
    QString sourceName;
    QString nodePath;
    int dataOffset = -1;
    int usedSize = 0;
    QString familyKey;
    int strideHint = -1;
    VMeshDataHeaderHint headerHint;
};

struct ModelBounds {
    QVector3D minCorner;
    QVector3D maxCorner;
    float radius = 0.0f;
    bool valid = false;
};

struct VMeshRefRecord {
    quint32 meshDataReference = 0;
    int vertexStart = 0;
    int vertexCount = 0;
    int indexStart = 0;
    int indexCount = 0;
    int groupStart = 0;
    int groupCount = 0;
    QString parentName;
    QString nodePath;
    QString modelName;
    QString levelName;
    QString matchedSourceName;
    QString resolutionHint;
    QString debugHint;
    bool usedStructuredFamilyFallback = false;
    ModelBounds bounds;
};

struct CmpFixRecord {
    QString parentName;
    QString childName;
    QVector3D translation;
    QQuaternion rotation;
    bool valid = false;
};

struct CmpTransformHint {
    QString partName;
    QString parentPartName;
    QVector3D localTranslation;
    QQuaternion localRotation;
    QVector3D combinedTranslation;
    QQuaternion combinedRotation;
    bool hasLocalTranslation = false;
    bool hasLocalRotation = false;
    bool hasCombinedTranslation = false;
    bool hasCombinedRotation = false;
};

struct MaterialReference {
    QString kind;
    QString value;
    QString nodeName;
    QString nodePath;
};

struct PreviewMaterialBinding {
    QString modelName;
    QString levelName;
    QString partName;
    int groupStart = 0;
    int groupCount = 0;
    QStringList sourceNames;
    QString textureValue;
    QStringList textureCandidates;
    QString materialValue;
    QString referenceNodePath;
    QString matchHint;
};

struct DecodedModel {
    QString sourcePath;
    NativeModelFormat format = NativeModelFormat::Unknown;
    UtfFileHeader header;
    QVector<UtfNodeRecord> utfNodes;
    QVector<NativeModelPart> parts;
    QVector<VMeshDataBlock> vmeshDataBlocks;
    QVector<VMeshRefRecord> vmeshRefs;
    QVector<CmpFixRecord> cmpFixRecords;
    QVector<CmpTransformHint> cmpTransformHints;
    QVector<MaterialReference> materialReferences;
    QVector<PreviewMaterialBinding> previewMaterialBindings;
    QStringList warnings;
    ModelNode rootNode;

    bool isValid() const
    {
        return !rootNode.meshes.isEmpty() || !rootNode.children.isEmpty();
    }
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
    /// Decode a Freelancer native model into a reusable intermediate structure.
    static DecodedModel loadModel(const QString &filePath);

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
    static UtfFileHeader parseUtfHeader(const QByteArray &data);
    static void parseUtfNode(const QByteArray &data, int nodeOffset,
                              const QByteArray &stringBlock, int dataBlockOffset,
                              std::shared_ptr<UtfNode> &parent);
    static ModelNode extractPart(const NativeModelPart &part,
                                 const QString &partPath,
                                 const QByteArray &raw,
                                 const QVector<UtfNodeRecord> &nodes,
                                 const QVector<VMeshDataBlock> &vmeshBlocks,
                                 QVector<VMeshRefRecord> &vmeshRefs,
                                 const QVector<CmpTransformHint> &cmpTransformHints,
                                 const QVector<PreviewMaterialBinding> &previewMaterialBindings,
                                 QStringList *warnings);
    static QVector<MeshData> buildMeshesFromVMesh(const QByteArray &vmeshData,
                                                  const VMeshRefRecord &ref,
                                                  const PreviewMaterialBinding *binding,
                                                  const QString &directPlanHint,
                                                  QString *resolvedPlanHint = nullptr);
};

} // namespace flatlas::infrastructure
