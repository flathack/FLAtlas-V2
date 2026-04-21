// infrastructure/io/CmpLoader.cpp – CMP/3DB-Modell-Loader (Phase 8)

#include "CmpLoader.h"
#include "VmeshDecoder.h"
#include <QFile>
#include <QDataStream>
#include <QIODevice>
#include <QtMath>

#include <algorithm>

namespace flatlas::infrastructure {

namespace {

bool isFiniteVector(const QVector3D &value)
{
    return qIsFinite(value.x()) && qIsFinite(value.y()) && qIsFinite(value.z());
}

bool sanitizeMesh(MeshData &mesh)
{
    if (mesh.vertices.isEmpty() || mesh.indices.isEmpty())
        return false;

    QVector<MeshVertex> sanitizedVertices;
    sanitizedVertices.reserve(mesh.vertices.size());
    QVector<int> remap(mesh.vertices.size(), -1);

    for (qsizetype i = 0; i < mesh.vertices.size(); ++i) {
        const auto &vertex = mesh.vertices.at(i);
        if (!isFiniteVector(vertex.position))
            continue;

        MeshVertex clean = vertex;
        if (!isFiniteVector(clean.normal))
            clean.normal = QVector3D(0.0f, 1.0f, 0.0f);
        if (!qIsFinite(clean.u))
            clean.u = 0.0f;
        if (!qIsFinite(clean.v))
            clean.v = 0.0f;

        remap[i] = sanitizedVertices.size();
        sanitizedVertices.append(clean);
    }

    if (sanitizedVertices.size() < 3)
        return false;

    QVector<uint32_t> sanitizedIndices;
    sanitizedIndices.reserve(mesh.indices.size());
    for (uint32_t index : std::as_const(mesh.indices)) {
        if (index >= static_cast<uint32_t>(remap.size()))
            continue;
        const int mapped = remap.at(static_cast<qsizetype>(index));
        if (mapped < 0)
            continue;
        sanitizedIndices.append(static_cast<uint32_t>(mapped));
    }

    const qsizetype triangleCount = sanitizedIndices.size() / 3;
    if (triangleCount <= 0)
        return false;
    sanitizedIndices.resize(triangleCount * 3);

    QVector<uint32_t> compactIndices;
    compactIndices.reserve(sanitizedIndices.size());
    for (qsizetype i = 0; i + 2 < sanitizedIndices.size(); i += 3) {
        const uint32_t a = sanitizedIndices.at(i);
        const uint32_t b = sanitizedIndices.at(i + 1);
        const uint32_t c = sanitizedIndices.at(i + 2);
        if (a == b || b == c || a == c)
            continue;
        compactIndices.append(a);
        compactIndices.append(b);
        compactIndices.append(c);
    }

    if (compactIndices.isEmpty())
        return false;

    mesh.vertices = std::move(sanitizedVertices);
    mesh.indices = std::move(compactIndices);
    return true;
}

} // namespace

// ─── UTF file header constants ────────────────────────────
static constexpr int UTF_SIGNATURE_SIZE = 4;
static constexpr char UTF_SIGNATURE[4] = {'U', 'T', 'F', ' '};
static constexpr int UTF_HEADER_SIZE = 56; // Full header up to string block offset
static constexpr int UTF_NODE_SIZE = 56;   // Each node entry in the node block

// ─── UTF parsing ──────────────────────────────────────────

std::shared_ptr<UtfNode> CmpLoader::parseUtf(const QByteArray &data)
{
    if (data.size() < UTF_HEADER_SIZE)
        return nullptr;

    // Verify signature
    if (data.left(4) != QByteArray(UTF_SIGNATURE, 4))
        return nullptr;

    QDataStream ds(data);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.skipRawData(4); // signature

    uint32_t version;
    ds >> version; // typically 0x101

    int32_t nodeBlockOffset, nodeBlockSize;
    ds >> nodeBlockOffset >> nodeBlockSize;

    int32_t zero1;
    ds >> zero1; // unused/entry size

    int32_t stringBlockOffset, stringBlockSize;
    ds >> stringBlockOffset >> stringBlockSize;

    int32_t zero2;
    ds >> zero2; // unused

    int32_t dataBlockOffset;
    ds >> dataBlockOffset;

    if (nodeBlockOffset + nodeBlockSize > data.size())
        return nullptr;
    if (stringBlockOffset + stringBlockSize > data.size())
        return nullptr;

    QByteArray stringBlock = data.mid(stringBlockOffset, stringBlockSize);

    auto root = std::make_shared<UtfNode>();
    root->name = "\\";

    parseUtfNode(data, nodeBlockOffset, stringBlock, dataBlockOffset, root);

    return root;
}

void CmpLoader::parseUtfNode(const QByteArray &data, int nodeOffset,
                               const QByteArray &stringBlock, int dataBlockOffset,
                               std::shared_ptr<UtfNode> &parent)
{
    if (nodeOffset < 0 || nodeOffset + UTF_NODE_SIZE > data.size())
        return;

    QDataStream ds(data);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.skipRawData(nodeOffset);

    int32_t siblingOffset, nameOffset;
    ds >> siblingOffset >> nameOffset;

    // Read node name from string block
    QString name;
    if (nameOffset >= 0 && nameOffset < stringBlock.size()) {
        int end = stringBlock.indexOf('\0', nameOffset);
        if (end < 0) end = stringBlock.size();
        name = QString::fromLatin1(stringBlock.mid(nameOffset, end - nameOffset));
    }

    int32_t flags;
    ds >> flags; // 0x10 = intermediate, 0x80 = leaf

    int32_t zero1;
    ds >> zero1;

    int32_t childOffset;
    ds >> childOffset;

    int32_t zero2;
    ds >> zero2;

    int32_t allocatedSize, dataSize, dataOff;
    int32_t zero3, zero4;
    ds >> allocatedSize >> dataSize >> dataOff;
    ds >> zero3 >> zero4;

    auto node = std::make_shared<UtfNode>();
    node->name = name;

    if (flags & 0x10) {
        // Directory/intermediate node — recurse into children
        if (childOffset > 0)
            parseUtfNode(data, childOffset, stringBlock, dataBlockOffset, node);
    } else if (flags & 0x80) {
        // Leaf node — extract data
        int absDataOffset = dataBlockOffset + dataOff;
        if (absDataOffset >= 0 && absDataOffset + dataSize <= data.size())
            node->data = data.mid(absDataOffset, dataSize);
    }

    parent->children.append(node);

    // Process sibling
    if (siblingOffset > 0)
        parseUtfNode(data, siblingOffset, stringBlock, dataBlockOffset, parent);
}

std::shared_ptr<UtfNode> CmpLoader::findNode(const std::shared_ptr<UtfNode> &root,
                                               const QString &path)
{
    if (!root)
        return nullptr;

    QStringList parts = path.split('\\', Qt::SkipEmptyParts);
    auto current = root;

    for (const QString &part : parts) {
        bool found = false;
        for (const auto &child : current->children) {
            if (child->name.compare(part, Qt::CaseInsensitive) == 0) {
                current = child;
                found = true;
                break;
            }
        }
        if (!found)
            return nullptr;
    }

    return current;
}

// ─── Mesh extraction ──────────────────────────────────────

MeshData CmpLoader::buildMeshFromVMesh(const QByteArray &vmeshData,
                                         int startVertex, int numVertices,
                                         int startIndex, int numIndices)
{
    MeshData mesh;
    auto decoded = VmeshDecoder::decode(vmeshData);

    if (decoded.positions.isEmpty())
        return mesh;

    if (startVertex < 0 || numVertices <= 0 || startIndex < 0 || numIndices <= 0)
        return mesh;
    if (startVertex >= decoded.positions.size() || startIndex >= decoded.indices.size())
        return mesh;

    // Extract the relevant vertex range
    int endVertex = qMin(startVertex + numVertices, decoded.positions.size());
    for (int i = startVertex; i < endVertex; ++i) {
        MeshVertex v;
        v.position = decoded.positions[i];
        if (i < decoded.normals.size())
            v.normal = decoded.normals[i];
        if (i < decoded.uvs.size()) {
            v.u = decoded.uvs[i].x();
            v.v = decoded.uvs[i].y();
        }
        mesh.vertices.append(v);
    }

    // Extract the relevant index range, rebased to vertex 0
    int endIndex = qMin(startIndex + numIndices, decoded.indices.size());
    for (int i = startIndex; i < endIndex; ++i) {
        uint32_t idx = decoded.indices[i];
        if (idx < static_cast<uint32_t>(startVertex))
            continue;
        const uint32_t rebased = idx - static_cast<uint32_t>(startVertex);
        if (rebased >= static_cast<uint32_t>(mesh.vertices.size()))
            continue;
        mesh.indices.append(rebased);
    }

    if (!sanitizeMesh(mesh))
        return {};

    return mesh;
}

ModelNode CmpLoader::extractPart(const std::shared_ptr<UtfNode> &partRoot,
                                   const std::shared_ptr<UtfNode> &vmeshLib)
{
    ModelNode node;
    node.name = partRoot->name;

    // Try to find VMeshRef (reference to VMeshData in the library)
    auto vmeshRef = findNode(partRoot, "VMeshRef");
    if (vmeshRef && !vmeshRef->data.isEmpty() && vmeshLib) {
        auto refs = VmeshDecoder::decodeRef(vmeshRef->data);

        // Find the matching VMeshData in the library
        for (const auto &libChild : vmeshLib->children) {
            auto vmeshDataNode = findNode(libChild, "VMeshData");
            if (vmeshDataNode && !vmeshDataNode->data.isEmpty()) {
                for (const auto &ref : refs) {
                    auto mesh = buildMeshFromVMesh(vmeshDataNode->data,
                                                    ref.startVertex,
                                                    ref.numRefVertices,
                                                    ref.startIndex,
                                                    ref.numIndices);
                    if (!mesh.vertices.isEmpty())
                        node.meshes.append(mesh);
                }
                if (!node.meshes.isEmpty())
                    break; // Found our mesh data
            }
        }
    }

    // Try to find VMeshPart (direct mesh data in some 3DB files)
    if (node.meshes.isEmpty()) {
        auto vmeshPart = findNode(partRoot, "VMeshPart");
        if (vmeshPart) {
            auto vmeshDataNode = findNode(vmeshPart, "VMeshData");
            if (vmeshDataNode && !vmeshDataNode->data.isEmpty()) {
                auto decoded = VmeshDecoder::decode(vmeshDataNode->data);
                MeshData mesh;
                for (int i = 0; i < decoded.positions.size(); ++i) {
                    MeshVertex v;
                    v.position = decoded.positions[i];
                    if (i < decoded.normals.size()) v.normal = decoded.normals[i];
                    if (i < decoded.uvs.size()) { v.u = decoded.uvs[i].x(); v.v = decoded.uvs[i].y(); }
                    mesh.vertices.append(v);
                }
                mesh.indices = decoded.indices;
                if (sanitizeMesh(mesh))
                    node.meshes.append(mesh);
            }
        }
    }

    return node;
}

// ─── Public API ───────────────────────────────────────────

ModelNode CmpLoader::loadCmp(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QByteArray data = file.readAll();
    auto root = parseUtf(data);
    if (!root)
        return {};

    ModelNode model;
    model.name = filePath.section('/', -1).section('\\', -1);

    // Find VMeshLibrary (shared mesh data for all parts)
    auto vmeshLib = findNode(root, "VMeshLibrary");

    // CMP files have a Cmpnd (compound) node listing all parts
    auto cmpnd = findNode(root, "Cmpnd");
    if (cmpnd) {
        // Each child of Cmpnd is a part definition
        for (const auto &partDef : cmpnd->children) {
            // Find the "File name" leaf which gives us the part path
            auto fileNameNode = findNode(partDef, "File name");
            if (!fileNameNode || fileNameNode->data.isEmpty())
                continue;

            QString partPath = QString::fromLatin1(fileNameNode->data).trimmed();
            partPath.remove(QChar('\0'));

            // Find the actual part node
            auto partNode = findNode(root, partPath);
            if (partNode) {
                model.children.append(extractPart(partNode, vmeshLib));
            }
        }
    }

    // If no Cmpnd found, treat as a single-part model
    if (model.children.isEmpty()) {
        model = extractPart(root, vmeshLib);
        model.name = filePath.section('/', -1).section('\\', -1);
    }

    return model;
}

ModelNode CmpLoader::load3db(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QByteArray data = file.readAll();
    auto root = parseUtf(data);
    if (!root)
        return {};

    // 3DB files have VMeshLibrary at root level with a single mesh
    auto vmeshLib = findNode(root, "VMeshLibrary");

    ModelNode model = extractPart(root, vmeshLib);
    model.name = filePath.section('/', -1).section('\\', -1);

    return model;
}

} // namespace flatlas::infrastructure
