// infrastructure/io/CmpLoader.cpp - Decoder-first Freelancer CMP/3DB loader

#include "CmpLoader.h"

#include "VmeshDecoder.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QMatrix3x3>
#include <QtMath>

#include <algorithm>
#include <memory>
#include <optional>

namespace flatlas::infrastructure {

namespace {

constexpr char kUtfMagic[] = {'U', 'T', 'F', ' '};
constexpr int kUtfHeaderSize = 56;
constexpr int kUtfNodeEntrySize = 44;
constexpr int kCmpFixRecordSize = 176;

struct FlatUtfNode {
    QString name;
    quint32 flags = 0;
    quint32 peerOffset = 0;
    quint32 childOffset = 0;
    int dataOffset = -1;
    int allocatedSize = 0;
    int usedSize = 0;
    QString parentName;
    QString path;
};

bool sanitizeMesh(MeshData &mesh);

bool isUtfMagic(const QByteArray &magic)
{
    return magic.size() == 4 && std::equal(magic.begin(), magic.end(), std::begin(kUtfMagic));
}

int resolveUtfDataOffset(int storedOffset, const UtfFileHeader &header, int rawSize)
{
    if (storedOffset < 0)
        return storedOffset;
    if (header.dataOffset > 0) {
        const int relative = static_cast<int>(header.dataOffset) + storedOffset;
        if (relative >= 0 && relative < rawSize)
            return relative;
    }
    return storedOffset;
}

QHash<int, QString> buildStringOffsetLookup(const QByteArray &raw, const UtfFileHeader &header)
{
    QHash<int, QString> lookup;
    const int start = static_cast<int>(header.namesOffset);
    const int namesSize = qMax(static_cast<int>(header.namesAllocatedSize),
                               static_cast<int>(header.namesUsedSize));
    if (start < 0 || start >= raw.size() || namesSize <= 0)
        return lookup;

    const QByteArray chunk = raw.mid(start, qMin(namesSize, raw.size() - start));
    int offset = 0;
    for (const QByteArray &piece : chunk.split('\0')) {
        if (!piece.isEmpty()) {
            const QString text = QString::fromLatin1(piece).trimmed();
            if (!text.isEmpty())
                lookup.insert(offset, text);
        }
        offset += piece.size() + 1;
    }
    return lookup;
}

QVector<FlatUtfNode> parseFlatUtfNodes(const QByteArray &raw, const UtfFileHeader &header)
{
    QVector<FlatUtfNode> parsed;
    if (!isUtfMagic(header.magic) || header.nodeEntrySize < kUtfNodeEntrySize)
        return parsed;

    const auto nameLookup = buildStringOffsetLookup(raw, header);
    const int nodeCount = header.nodeCount();
    parsed.reserve(nodeCount);

    for (int index = 0; index < nodeCount; ++index) {
        const int base = static_cast<int>(header.nodeBlockOffset) + index * static_cast<int>(header.nodeEntrySize);
        const QByteArray chunk = raw.mid(base, kUtfNodeEntrySize);
        if (chunk.size() < kUtfNodeEntrySize)
            break;

        QDataStream ds(chunk);
        ds.setByteOrder(QDataStream::LittleEndian);

        quint32 peerOffset = 0;
        quint32 nameOffset = 0;
        quint32 flags = 0;
        quint32 reserved = 0;
        quint32 childOrDataOffset = 0;
        quint32 allocatedSize = 0;
        quint32 usedSize = 0;
        quint32 timestamp = 0;
        quint32 unknown0 = 0;
        quint32 unknown1 = 0;
        quint32 unknown2 = 0;
        ds >> peerOffset >> nameOffset >> flags >> reserved >> childOrDataOffset
           >> allocatedSize >> usedSize >> timestamp >> unknown0 >> unknown1 >> unknown2;

        FlatUtfNode node;
        node.name = nameLookup.value(static_cast<int>(nameOffset),
                                     QStringLiteral("<name@0x%1>").arg(nameOffset, 0, 16));
        node.flags = flags;
        node.peerOffset = peerOffset;
        node.childOffset = (flags & 0x80u) ? 0u : childOrDataOffset;
        node.dataOffset = (flags & 0x80u) ? resolveUtfDataOffset(static_cast<int>(childOrDataOffset), header, raw.size())
                                          : -1;
        node.allocatedSize = (flags & 0x80u) ? static_cast<int>(allocatedSize) : 0;
        node.usedSize = (flags & 0x80u) ? static_cast<int>(usedSize) : 0;
        parsed.append(node);
    }

    QHash<int, int> offsetToIndex;
    for (int index = 0; index < parsed.size(); ++index)
        offsetToIndex.insert(index * static_cast<int>(header.nodeEntrySize), index);

    std::function<void(int, int, const QString&, QSet<int>)> walk =
        [&](int offset, int parentIndex, const QString &parentPath, QSet<int> ancestors) {
            QSet<int> localSeen;
            int currentOffset = offset;
            while (offsetToIndex.contains(currentOffset)) {
                const int index = offsetToIndex.value(currentOffset);
                if (localSeen.contains(index) || ancestors.contains(index))
                    break;
                localSeen.insert(index);

                FlatUtfNode &node = parsed[index];
                if (parentIndex >= 0 && node.parentName.isEmpty())
                    node.parentName = parsed[parentIndex].name;
                if (node.path.isEmpty())
                    node.path = parentPath.isEmpty() ? node.name : parentPath + QLatin1Char('/') + node.name;

                if (node.childOffset && offsetToIndex.contains(static_cast<int>(node.childOffset))) {
                    QSet<int> nextAncestors = ancestors;
                    nextAncestors.insert(index);
                    walk(static_cast<int>(node.childOffset), index, node.path, nextAncestors);
                }

                if (!node.peerOffset)
                    break;
                currentOffset = static_cast<int>(node.peerOffset);
            }
        };

    walk(0, -1, QString(), {});
    return parsed;
}

QByteArray readNodeData(const FlatUtfNode &node, const QByteArray &raw)
{
    if (!(node.flags & 0x80u) || node.dataOffset < 0 || node.usedSize <= 0)
        return {};
    if (node.dataOffset >= raw.size())
        return {};
    return raw.mid(node.dataOffset, qMin(node.usedSize, raw.size() - node.dataOffset));
}

QString readNativeTextNode(const FlatUtfNode &node, const QByteArray &raw)
{
    const QByteArray chunk = readNodeData(node, raw);
    if (chunk.isEmpty())
        return {};
    const QByteArray head = chunk.split('\0').value(0);
    const QString text = QString::fromLatin1(head).trimmed();
    if (text.isEmpty())
        return {};
    return text;
}

int readNativeU32Node(const FlatUtfNode &node, const QByteArray &raw)
{
    const QByteArray chunk = readNodeData(node, raw);
    if (chunk.size() < 4)
        return -1;
    QDataStream ds(chunk);
    ds.setByteOrder(QDataStream::LittleEndian);
    quint32 value = 0;
    ds >> value;
    return static_cast<int>(value);
}

QHash<QString, QVector<int>> buildChildrenByParentPath(const QVector<FlatUtfNode> &nodes)
{
    QHash<QString, QVector<int>> children;
    for (int i = 0; i < nodes.size(); ++i) {
        const QString &path = nodes.at(i).path;
        if (path.isEmpty() || !path.contains(QLatin1Char('/')))
            continue;
        const QString parentPath = path.section(QLatin1Char('/'), 0, -2);
        children[parentPath].append(i);
    }
    return children;
}

QString vmeshFamilyKeyFromSourceName(QString sourceName)
{
    sourceName.replace(QLatin1Char('\\'), QLatin1Char('/'));
    sourceName = QFileInfo(sourceName).fileName().toLower();
    if (sourceName.endsWith(QStringLiteral(".vms")))
        sourceName.chop(4);
    const int marker = sourceName.lastIndexOf(QLatin1Char('-'));
    if (marker >= 0) {
        bool ok = false;
        sourceName.mid(marker + 1).toInt(&ok);
        if (ok)
            sourceName = sourceName.left(marker);
    }
    return sourceName;
}

int strideHintFromSourceName(QString sourceName)
{
    sourceName = QFileInfo(sourceName).fileName().toLower();
    const int dash = sourceName.lastIndexOf(QLatin1Char('-'));
    const int dot = sourceName.lastIndexOf(QStringLiteral(".vms"));
    if (dash < 0 || dot < 0 || dash >= dot)
        return -1;
    bool ok = false;
    const int value = sourceName.mid(dash + 1, dot - dash - 1).toInt(&ok);
    return ok && value > 0 ? value : -1;
}

bool looksLikeStructuredVmeshHeader(int meshCountHint,
                                    int referencedVertexCountHint,
                                    quint32 flexibleVertexFormatHint,
                                    int vertexCountHint,
                                    int triangleCountHint,
                                    int usedSize)
{
    if (meshCountHint <= 0 || meshCountHint > 4096)
        return false;
    if (referencedVertexCountHint < 0 || referencedVertexCountHint > 1000000)
        return false;
    if (flexibleVertexFormatHint == 0)
        return false;
    if (vertexCountHint <= 0 || vertexCountHint > 65535)
        return false;
    if (triangleCountHint <= 0 || triangleCountHint > 65535)
        return false;
    return usedSize >= 16;
}

VMeshDataHeaderHint buildVMeshDataHeaderHint(const QByteArray &blockBytes,
                                             const QString &sourceName,
                                             int usedSize)
{
    VMeshDataHeaderHint hint;
    if (blockBytes.isEmpty())
        return hint;

    QDataStream ds(blockBytes);
    ds.setByteOrder(QDataStream::LittleEndian);
    quint32 headerU32[4] = {};
    quint16 headerU16[8] = {};
    for (quint32 &value : headerU32) {
        if (ds.atEnd())
            break;
        ds >> value;
    }

    QDataStream ds16(blockBytes);
    ds16.setByteOrder(QDataStream::LittleEndian);
    for (quint16 &value : headerU16) {
        if (ds16.atEnd())
            break;
        ds16 >> value;
    }

    const int meshCountHint = static_cast<int>(headerU32[0]);
    const int referencedVertexCountHint = static_cast<int>(headerU32[1]);
    const quint32 flexibleVertexFormatHint = headerU32[2];
    const int vertexCountHint = static_cast<int>(headerU16[6]);
    const int triangleCountHint = static_cast<int>(headerU16[7]);
    const int meshHeaderCountHint = static_cast<int>(headerU16[4]);
    const int meshHeaderIndexEndHint = static_cast<int>(headerU16[5]);
    const int meshHeaderNumRefVerticesHint = static_cast<int>(headerU16[6]);
    const int meshHeaderEndVertexHint = static_cast<int>(headerU16[7]);

    if (looksLikeStructuredVmeshHeader(meshCountHint,
                                       referencedVertexCountHint,
                                       flexibleVertexFormatHint,
                                       vertexCountHint,
                                       triangleCountHint,
                                       usedSize)) {
        hint.structureKind = QStringLiteral("structured-header");
        hint.meshCountHint = meshCountHint;
        hint.referencedVertexCountHint = referencedVertexCountHint;
        hint.flexibleVertexFormatHint = flexibleVertexFormatHint;
        hint.vertexCountHint = vertexCountHint;
        hint.triangleCountHint = triangleCountHint;
        hint.meshHeaderCountHint = meshHeaderCountHint;
        hint.meshHeaderIndexEndHint = meshHeaderIndexEndHint;
        hint.meshHeaderNumRefVerticesHint = meshHeaderNumRefVerticesHint;
        hint.meshHeaderEndVertexHint = meshHeaderEndVertexHint;
        return hint;
    }

    if (strideHintFromSourceName(sourceName) > 0) {
        int finiteCount = 0;
        QDataStream floatStream(blockBytes);
        floatStream.setByteOrder(QDataStream::LittleEndian);
        floatStream.setFloatingPointPrecision(QDataStream::SinglePrecision);
        for (int i = 0; i < 4 && !floatStream.atEnd(); ++i) {
            float value = 0.0f;
            floatStream >> value;
            if (qIsFinite(value) && qAbs(value) <= 1000000.0f)
                ++finiteCount;
        }
        if (finiteCount >= 3)
            hint.structureKind = QStringLiteral("vertex-stream");
    }

    return hint;
}

quint32 freelancerCrc32(const QString &value)
{
    quint32 crc = 0xFFFFFFFFu;
    for (char byte : value.toLower().toLatin1()) {
        crc ^= static_cast<quint8>(byte);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 1u) ? (crc >> 1u) ^ 0xEDB88320u : (crc >> 1u);
    }
    return ~crc;
}

ModelBounds boundsFromExtrema(float maxX, float minX, float maxY, float minY, float maxZ, float minZ, float radius)
{
    ModelBounds bounds;
    bounds.minCorner = QVector3D(minX, minY, minZ);
    bounds.maxCorner = QVector3D(maxX, maxY, maxZ);
    bounds.radius = radius;
    bounds.valid = qIsFinite(radius)
        && qIsFinite(minX) && qIsFinite(minY) && qIsFinite(minZ)
        && qIsFinite(maxX) && qIsFinite(maxY) && qIsFinite(maxZ);
    return bounds;
}

QQuaternion quaternionFromRows(const QMatrix3x3 &matrix)
{
    return QQuaternion::fromRotationMatrix(matrix).normalized();
}

constexpr float kMaxPreviewAbsCoord = 1000000.0f;

bool decodeIndices16(const QByteArray &raw, int vertexCountHint, QVector<uint32_t> *indices)
{
    if (!indices || vertexCountHint <= 0 || raw.isEmpty() || (raw.size() % 2) != 0)
        return false;

    QVector<uint32_t> decoded;
    decoded.reserve(raw.size() / 2);
    QDataStream ds(raw);
    ds.setByteOrder(QDataStream::LittleEndian);
    while (!ds.atEnd()) {
        quint16 index = 0;
        ds >> index;
        if (index >= static_cast<quint16>(vertexCountHint))
            return false;
        decoded.append(index);
    }
    if (decoded.isEmpty())
        return false;
    *indices = decoded;
    return true;
}

QString triangleSoupVertexKey(const MeshVertex &vertex)
{
    return QStringLiteral("%1|%2|%3|%4|%5")
        .arg(std::round(vertex.position.x() * 100.0f) / 100.0f, 0, 'f', 2)
        .arg(std::round(vertex.position.y() * 100.0f) / 100.0f, 0, 'f', 2)
        .arg(std::round(vertex.position.z() * 100.0f) / 100.0f, 0, 'f', 2)
        .arg(std::round(vertex.u * 100.0f) / 100.0f, 0, 'f', 2)
        .arg(std::round(vertex.v * 100.0f) / 100.0f, 0, 'f', 2);
}

bool looksLikeExpandedTriangleSoup(const MeshData &mesh)
{
    const int vertexCount = mesh.vertices.size();
    const int indexCount = mesh.indices.size();
    if (vertexCount < 6 || indexCount < 6)
        return false;
    if (indexCount != vertexCount)
        return false;
    if ((indexCount % 3) != 0)
        return false;
    if (*std::max_element(mesh.indices.cbegin(), mesh.indices.cend()) != static_cast<uint32_t>(vertexCount - 1))
        return false;

    int sequentialTriangles = 0;
    const int triangleCount = indexCount / 3;
    for (int tri = 0; tri < triangleCount; ++tri) {
        const uint32_t a = mesh.indices.at(tri * 3);
        const uint32_t b = mesh.indices.at(tri * 3 + 1);
        const uint32_t c = mesh.indices.at(tri * 3 + 2);
        QVector<uint32_t> sorted{a, b, c};
        std::sort(sorted.begin(), sorted.end());
        const uint32_t base = static_cast<uint32_t>(tri * 3);
        if (sorted.at(0) == base && sorted.at(1) == base + 1 && sorted.at(2) == base + 2)
            ++sequentialTriangles;
    }
    if (sequentialTriangles * 10 < triangleCount * 9)
        return false;

    QSet<QString> uniqueKeys;
    for (const MeshVertex &vertex : mesh.vertices)
        uniqueKeys.insert(triangleSoupVertexKey(vertex));
    return uniqueKeys.size() < vertexCount;
}

bool compactTriangleSoupVertices(MeshData &mesh)
{
    if (!looksLikeExpandedTriangleSoup(mesh))
        return false;

    QHash<QString, int> seen;
    QVector<MeshVertex> compactVertices;
    compactVertices.reserve(mesh.vertices.size());
    QVector<uint32_t> remap;
    remap.reserve(mesh.vertices.size());

    for (const MeshVertex &vertex : mesh.vertices) {
        const QString key = triangleSoupVertexKey(vertex);
        auto it = seen.constFind(key);
        if (it == seen.constEnd()) {
            const int mapped = compactVertices.size();
            seen.insert(key, mapped);
            compactVertices.append(vertex);
            remap.append(static_cast<uint32_t>(mapped));
        } else {
            remap.append(static_cast<uint32_t>(*it));
        }
    }

    if (compactVertices.size() >= mesh.vertices.size())
        return false;

    QVector<uint32_t> remappedIndices;
    remappedIndices.reserve(mesh.indices.size());
    for (uint32_t index : std::as_const(mesh.indices)) {
        if (index >= static_cast<uint32_t>(remap.size()))
            return false;
        remappedIndices.append(remap.at(static_cast<qsizetype>(index)));
    }

    mesh.vertices = std::move(compactVertices);
    mesh.indices = std::move(remappedIndices);
    return true;
}

bool decodePositionsWithOffset(const QByteArray &raw, int stride, int positionOffset, QVector<QVector3D> *positions)
{
    if (!positions || stride < 12 || positionOffset < 0 || positionOffset + 12 > stride || raw.isEmpty()
        || (raw.size() % stride) != 0) {
        return false;
    }

    QVector<QVector3D> decoded;
    decoded.reserve(raw.size() / stride);
    QDataStream ds(raw);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
    while (!ds.atEnd()) {
        if (positionOffset > 0 && ds.skipRawData(positionOffset) != positionOffset)
            return false;

        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        ds >> x >> y >> z;
        if (!qIsFinite(x) || !qIsFinite(y) || !qIsFinite(z))
            return false;
        if (qMax(qMax(qAbs(x), qAbs(y)), qAbs(z)) > kMaxPreviewAbsCoord)
            return false;
        decoded.append(QVector3D(x, y, z));

        const int trailing = stride - positionOffset - 12;
        if (trailing > 0 && ds.skipRawData(trailing) != trailing)
            return false;
    }

    if (decoded.isEmpty())
        return false;
    *positions = decoded;
    return true;
}

ModelBounds boundsFromPositions(const QVector<QVector3D> &positions)
{
    ModelBounds bounds;
    if (positions.isEmpty())
        return bounds;

    QVector3D minCorner = positions.first();
    QVector3D maxCorner = positions.first();
    for (const QVector3D &position : positions) {
        minCorner.setX(qMin(minCorner.x(), position.x()));
        minCorner.setY(qMin(minCorner.y(), position.y()));
        minCorner.setZ(qMin(minCorner.z(), position.z()));
        maxCorner.setX(qMax(maxCorner.x(), position.x()));
        maxCorner.setY(qMax(maxCorner.y(), position.y()));
        maxCorner.setZ(qMax(maxCorner.z(), position.z()));
    }

    bounds.minCorner = minCorner;
    bounds.maxCorner = maxCorner;
    bounds.radius = (maxCorner - minCorner).length() * 0.5f;
    bounds.valid = true;
    return bounds;
}

float structuredGeometryScore(const QVector<QVector3D> &positions,
                              const ModelBounds &expectedBounds,
                              int zeroPrefix,
                              int preferredStride,
                              int actualStride)
{
    float score = 0.0f;
    score += static_cast<float>(zeroPrefix) * 50.0f;
    if (preferredStride > 0 && preferredStride != actualStride)
        score += static_cast<float>(qAbs(preferredStride - actualStride)) * 10.0f;

    if (!expectedBounds.valid || positions.isEmpty())
        return score;

    const ModelBounds actualBounds = boundsFromPositions(positions);
    const QVector3D actualSpan = actualBounds.maxCorner - actualBounds.minCorner;
    const QVector3D expectedSpan = expectedBounds.maxCorner - expectedBounds.minCorner;
    const QVector3D actualCenter = (actualBounds.maxCorner + actualBounds.minCorner) * 0.5f;
    const QVector3D expectedCenter = (expectedBounds.maxCorner + expectedBounds.minCorner) * 0.5f;

    score += (qAbs(actualSpan.x() - expectedSpan.x())
              + qAbs(actualSpan.y() - expectedSpan.y())
              + qAbs(actualSpan.z() - expectedSpan.z())) * 0.25f;
    score += (qAbs(actualCenter.x() - expectedCenter.x())
              + qAbs(actualCenter.y() - expectedCenter.y())
              + qAbs(actualCenter.z() - expectedCenter.z())) * 0.1f;

    if (expectedSpan.x() > 50.0f && actualSpan.x() < expectedSpan.x() * 0.1f)
        score += 250.0f;
    if (expectedSpan.y() > 50.0f && actualSpan.y() < expectedSpan.y() * 0.1f)
        score += 250.0f;
    if (expectedSpan.z() > 50.0f && actualSpan.z() < expectedSpan.z() * 0.1f)
        score += 250.0f;

    return score;
}

std::optional<QPair<int, QVector<uint32_t>>> findStructuredFamilyIndices(const QByteArray &headerBytes,
                                                                         int expectedIndexCount,
                                                                         int expectedIndexOffset,
                                                                         int maxVertexIndexHint)
{
    if (expectedIndexCount <= 0 || headerBytes.size() < expectedIndexCount * 2)
        return std::nullopt;

    const int minUniqueIndices = qMax(3, qMin(32, expectedIndexCount / 6));
    const int maxDegenerate = qMax(0, expectedIndexCount / 6);

    const int indexByteCount = expectedIndexCount * 2;
    std::optional<QPair<int, QVector<uint32_t>>> bestIndices;
    for (int offset = 0; offset <= headerBytes.size() - indexByteCount; offset += 2) {
        QVector<uint32_t> indices;
        if (!decodeIndices16(headerBytes.mid(offset, indexByteCount), qMax(1, maxVertexIndexHint), &indices))
            continue;

        const int uniqueCount = QSet<uint32_t>(indices.cbegin(), indices.cend()).size();
        if (uniqueCount < minUniqueIndices)
            continue;

        int degenerateCount = 0;
        for (int i = 0; i + 2 < indices.size(); i += 3) {
            if (indices.at(i) == indices.at(i + 1)
                || indices.at(i) == indices.at(i + 2)
                || indices.at(i + 1) == indices.at(i + 2)) {
                ++degenerateCount;
            }
        }
        if (degenerateCount > maxDegenerate)
            continue;

        const int score = degenerateCount * 1000 - uniqueCount + qAbs(offset - expectedIndexOffset);
        const auto candidate = qMakePair(score, indices);
        if (!bestIndices.has_value() || candidate.first < bestIndices->first)
            bestIndices = candidate;
    }

    return bestIndices;
}

std::optional<QPair<float, MeshData>> buildStructuredFamilyMesh(const QVector<uint32_t> &indices,
                                                                const QByteArray &positionBytes,
                                                                const ModelBounds &expectedBounds,
                                                                int preferredStride)
{
    if (indices.isEmpty() || positionBytes.isEmpty())
        return std::nullopt;

    const auto [minIndexIt, maxIndexIt] = std::minmax_element(indices.cbegin(), indices.cend());
    const int minIndex = static_cast<int>(*minIndexIt);
    const int maxIndex = static_cast<int>(*maxIndexIt);
    const int vertexCount = maxIndex + 1;
    if (vertexCount <= 0)
        return std::nullopt;

    static const QVector<int> strideCandidates = {
        16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, 68, 72, 76,
        80, 84, 88, 92, 96, 100, 104, 108, 112, 116, 120, 124, 128
    };

    const int minUniquePositions = qMax(3, qMin(64, vertexCount / 2));
    std::optional<QPair<float, MeshData>> bestMesh;

    for (int offset = 0; offset < qMin(320, positionBytes.size()); offset += 4) {
        for (int stride : strideCandidates) {
            if (offset + vertexCount * stride > positionBytes.size())
                continue;

            QVector<QVector3D> positions;
            for (int positionOffset = 0; positionOffset <= qMin(stride - 12, 32); positionOffset += 4) {
                if (!decodePositionsWithOffset(positionBytes.mid(offset, vertexCount * stride), stride, positionOffset, &positions))
                    continue;

                QSet<QString> uniqueRounded;
                for (const QVector3D &position : positions) {
                    uniqueRounded.insert(QStringLiteral("%1|%2|%3")
                                             .arg(std::round(position.x() * 100.0f) / 100.0f, 0, 'f', 2)
                                             .arg(std::round(position.y() * 100.0f) / 100.0f, 0, 'f', 2)
                                             .arg(std::round(position.z() * 100.0f) / 100.0f, 0, 'f', 2));
                }
                if (uniqueRounded.size() < minUniquePositions)
                    continue;

                int zeroPrefix = 0;
                for (const QVector3D &position : positions) {
                    if (qMax(qMax(qAbs(position.x()), qAbs(position.y())), qAbs(position.z())) < 1e-6f)
                        ++zeroPrefix;
                    else
                        break;
                }

                MeshData mesh;
                mesh.vertices.reserve(maxIndex - minIndex + 1);
                for (int i = minIndex; i <= maxIndex && i < positions.size(); ++i) {
                    MeshVertex vertex;
                    vertex.position = positions.at(i);
                    mesh.vertices.append(vertex);
                }
                for (uint32_t index : indices)
                    mesh.indices.append(index - static_cast<uint32_t>(minIndex));

                if (!sanitizeMesh(mesh))
                    continue;

                const float score = structuredGeometryScore(
                    positions.mid(minIndex, maxIndex - minIndex + 1),
                    expectedBounds,
                    zeroPrefix,
                    preferredStride,
                    stride);
                if (!bestMesh.has_value() || score < bestMesh->first)
                    bestMesh = qMakePair(score, mesh);
            }
        }
    }

    return bestMesh;
}

std::optional<QPair<float, MeshData>> decodeStructuredFamilyVariant(const QByteArray &headerBytes,
                                                                    int expectedIndexCount,
                                                                    int expectedIndexOffset,
                                                                    int maxVertexIndexHint,
                                                                    const ModelBounds &expectedBounds,
                                                                    int preferredStride)
{
    const auto indices = findStructuredFamilyIndices(
        headerBytes,
        expectedIndexCount,
        expectedIndexOffset,
        maxVertexIndexHint);
    if (!indices.has_value())
        return std::nullopt;
    return buildStructuredFamilyMesh(indices->second, headerBytes, expectedBounds, preferredStride);
}

std::optional<QPair<float, MeshData>> decodeStructuredFamilySplitPair(const QByteArray &headerBytes,
                                                                      const QByteArray &streamBytes,
                                                                      int expectedIndexCount,
                                                                      int expectedIndexOffset,
                                                                      int maxVertexIndexHint,
                                                                      const ModelBounds &expectedBounds,
                                                                      int preferredStride,
                                                                      int streamCapacityVertices)
{
    const auto indices = findStructuredFamilyIndices(
        headerBytes,
        expectedIndexCount,
        expectedIndexOffset,
        maxVertexIndexHint);
    if (!indices.has_value())
        return std::nullopt;

    if (!indices->second.isEmpty()) {
        const auto maxIndexIt = std::max_element(indices->second.cbegin(), indices->second.cend());
        if (maxIndexIt != indices->second.cend() && streamCapacityVertices > 0
            && static_cast<int>(*maxIndexIt) + 1 > streamCapacityVertices) {
            return std::nullopt;
        }
    }

    return buildStructuredFamilyMesh(indices->second, streamBytes, expectedBounds, preferredStride);
}

std::optional<MeshData> decodeStructuredFamilyForRef(const QByteArray &raw,
                                                     const QVector<VMeshDataBlock> &vmeshBlocks,
                                                     const VMeshDataBlock &resolvedBlock,
                                                     const VMeshRefRecord &ref)
{
    QVector<VMeshDataBlock> familyBlocks;
    if (!resolvedBlock.familyKey.isEmpty()) {
        for (const auto &block : vmeshBlocks) {
            if (block.familyKey.compare(resolvedBlock.familyKey, Qt::CaseInsensitive) == 0)
                familyBlocks.append(block);
        }
    }
    if (familyBlocks.isEmpty())
        familyBlocks.append(resolvedBlock);

    QVector<VMeshDataBlock> headerBlocks;
    QVector<VMeshDataBlock> streamBlocks;
    for (const auto &block : std::as_const(familyBlocks)) {
        if (block.headerHint.structureKind == QStringLiteral("structured-header"))
            headerBlocks.append(block);
        if (block.headerHint.structureKind == QStringLiteral("vertex-stream"))
            streamBlocks.append(block);
    }

    std::optional<QPair<float, MeshData>> best;
    const int maxVertexIndexHint = qMax(ref.vertexStart + ref.vertexCount, ref.vertexCount);

    if (!headerBlocks.isEmpty() && !streamBlocks.isEmpty()) {
        for (const auto &headerBlock : std::as_const(headerBlocks)) {
            const QByteArray headerBytes = raw.mid(headerBlock.dataOffset, headerBlock.usedSize);
            if (headerBytes.isEmpty())
                continue;
            for (const auto &streamBlock : std::as_const(streamBlocks)) {
                const QByteArray streamBytes = raw.mid(streamBlock.dataOffset, streamBlock.usedSize);
                if (streamBytes.isEmpty())
                    continue;
                const int preferredStride = streamBlock.strideHint > 0 ? streamBlock.strideHint : resolvedBlock.strideHint;
                const int streamCapacityVertices = preferredStride > 0 ? streamBlock.usedSize / preferredStride : -1;
                const auto candidate = decodeStructuredFamilySplitPair(
                    headerBytes,
                    streamBytes,
                    ref.indexCount,
                    ref.indexStart * 2,
                    qMax(1, maxVertexIndexHint),
                    ref.bounds,
                    preferredStride,
                    streamCapacityVertices);
                if (!candidate.has_value())
                    continue;
                if (!best.has_value() || candidate->first < best->first)
                    best = candidate;
            }
        }
    }

    for (const auto &block : std::as_const(familyBlocks)) {
        const QByteArray blockBytes = raw.mid(block.dataOffset, block.usedSize);
        if (blockBytes.isEmpty())
            continue;

        const auto candidate = decodeStructuredFamilyVariant(
            blockBytes,
            ref.indexCount,
            ref.indexStart * 2,
            qMax(1, maxVertexIndexHint),
            ref.bounds,
            block.strideHint > 0 ? block.strideHint : resolvedBlock.strideHint);
        if (!candidate.has_value())
            continue;
        if (!best.has_value() || candidate->first < best->first)
            best = candidate;
    }

    if (!best.has_value())
        return std::nullopt;
    return best->second;
}

QVector<NativeModelPart> buildPartsFromNodes(const QVector<FlatUtfNode> &nodes, const QByteArray &raw)
{
    QVector<NativeModelPart> parts;
    const auto childrenByParent = buildChildrenByParentPath(nodes);

    for (int i = 0; i < nodes.size(); ++i) {
        const auto &node = nodes.at(i);
        const bool isCmpPart = node.name.startsWith(QStringLiteral("Part_"))
            || (node.name == QStringLiteral("Root") && node.path.endsWith(QStringLiteral("/Cmpnd/Root")));
        if (!isCmpPart)
            continue;

        NativeModelPart part;
        part.name = node.name;
        part.parentPartName = node.parentName.startsWith(QStringLiteral("Part_")) ? node.parentName : QString();

        const auto followers = childrenByParent.value(node.path);
        for (int childIndex : followers) {
            const auto &child = nodes.at(childIndex);
            if (child.name.endsWith(QStringLiteral(".vms"), Qt::CaseInsensitive) && part.sourceName.isEmpty())
                part.sourceName = child.name;
            else if (child.name == QStringLiteral("File name") && part.fileName.isEmpty())
                part.fileName = readNativeTextNode(child, raw);
            else if (child.name == QStringLiteral("Object name") && part.objectName.isEmpty())
                part.objectName = readNativeTextNode(child, raw);
            else if (child.name == QStringLiteral("Index") && part.cmpIndex < 0)
                part.cmpIndex = readNativeU32Node(child, raw);
        }
        parts.append(part);
    }

    return parts;
}

QVector<VMeshDataBlock> parseVMeshDataBlocks(const QVector<FlatUtfNode> &nodes, const QByteArray &raw)
{
    QVector<VMeshDataBlock> blocks;
    for (const auto &node : nodes) {
        if (node.name != QStringLiteral("VMeshData") || node.dataOffset < 0 || node.usedSize <= 0)
            continue;

        const QByteArray blockBytes = readNodeData(node, raw);
        if (blockBytes.isEmpty())
            continue;

        VMeshDataBlock block;
        block.sourceName = node.parentName;
        block.nodePath = node.path;
        block.dataOffset = node.dataOffset;
        block.usedSize = node.usedSize;
        block.familyKey = vmeshFamilyKeyFromSourceName(block.sourceName);
        block.strideHint = strideHintFromSourceName(block.sourceName);

        block.headerHint = buildVMeshDataHeaderHint(blockBytes, block.sourceName, block.usedSize);
        if (block.headerHint.structureKind == QStringLiteral("unknown")) {
            const auto decoded = VmeshDecoder::decode(blockBytes);
            if (decoded.numVertices > 0 || decoded.numIndices > 0) {
                block.headerHint.structureKind = QStringLiteral("structured-single-block");
                block.headerHint.vertexCountHint = decoded.numVertices;
                block.headerHint.referencedVertexCountHint = decoded.numIndices;
                block.headerHint.flexibleVertexFormatHint = decoded.fvf;
            } else if (block.strideHint > 0) {
                block.headerHint.structureKind = QStringLiteral("vertex-stream");
            }
        }

        blocks.append(block);
    }
    return blocks;
}

std::optional<QPair<int, VMeshDataBlock>> resolveVMeshDataBlock(
    quint32 meshDataReference,
    const QVector<VMeshDataBlock> &blocks)
{
    if (blocks.isEmpty())
        return std::nullopt;
    if (blocks.size() == 1)
        return qMakePair(0, blocks.first());
    if (meshDataReference < static_cast<quint32>(blocks.size()))
        return qMakePair(static_cast<int>(meshDataReference), blocks.at(static_cast<int>(meshDataReference)));

    QVector<int> crcMatches;
    for (int i = 0; i < blocks.size(); ++i) {
        if (!blocks.at(i).sourceName.isEmpty()
            && freelancerCrc32(blocks.at(i).sourceName) == meshDataReference) {
            crcMatches.append(i);
        }
    }
    if (crcMatches.size() == 1)
        return qMakePair(crcMatches.first(), blocks.at(crcMatches.first()));
    return std::nullopt;
}

QVector<VMeshRefRecord> parseVMeshRefs(const QVector<FlatUtfNode> &nodes,
                                       const QByteArray &raw,
                                       const QVector<VMeshDataBlock> &blocks)
{
    QVector<VMeshRefRecord> refs;
    constexpr int vmeshRefSize = 60;

    for (const auto &node : nodes) {
        if (node.name != QStringLiteral("VMeshRef"))
            continue;
        const QByteArray data = readNodeData(node, raw);
        if (data.size() < vmeshRefSize)
            continue;

        QDataStream ds(data);
        ds.setByteOrder(QDataStream::LittleEndian);

        quint32 headerSize = 0;
        quint32 meshDataReference = 0;
        quint16 vertexStart = 0;
        quint16 vertexCount = 0;
        quint16 indexStart = 0;
        quint16 indexCount = 0;
        quint16 groupStart = 0;
        quint16 groupCount = 0;
        float maxX = 0.0f, minX = 0.0f, maxY = 0.0f, minY = 0.0f, maxZ = 0.0f, minZ = 0.0f;
        float centerX = 0.0f, centerY = 0.0f, centerZ = 0.0f, radius = 0.0f;
        ds >> headerSize >> meshDataReference >> vertexStart >> vertexCount >> indexStart >> indexCount
           >> groupStart >> groupCount >> maxX >> minX >> maxY >> minY >> maxZ >> minZ
           >> centerX >> centerY >> centerZ >> radius;

        VMeshRefRecord ref;
        ref.meshDataReference = meshDataReference;
        ref.vertexStart = static_cast<int>(vertexStart);
        ref.vertexCount = static_cast<int>(vertexCount);
        ref.indexStart = static_cast<int>(indexStart);
        ref.indexCount = static_cast<int>(indexCount);
        ref.groupStart = static_cast<int>(groupStart);
        ref.groupCount = static_cast<int>(groupCount);
        ref.parentName = node.parentName;
        ref.nodePath = node.path;
        ref.bounds = boundsFromExtrema(maxX, minX, maxY, minY, maxZ, minZ, radius);

        const QStringList pathParts = node.path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        if (!pathParts.isEmpty())
            ref.modelName = pathParts.first();
        for (const QString &segment : pathParts) {
            if (segment.startsWith(QStringLiteral("Level"), Qt::CaseInsensitive)) {
                ref.levelName = segment;
                break;
            }
        }

        const auto resolved = resolveVMeshDataBlock(meshDataReference, blocks);
        if (resolved.has_value()) {
            ref.matchedSourceName = resolved->second.sourceName;
            ref.resolutionHint = resolved->first == static_cast<int>(meshDataReference)
                ? QStringLiteral("direct-index")
                : QStringLiteral("crc-or-fallback");
        } else {
            ref.resolutionHint = QStringLiteral("unresolved");
        }

        refs.append(ref);
    }

    return refs;
}

QVector<CmpFixRecord> parseCmpFixRecords(const QVector<FlatUtfNode> &nodes, const QByteArray &raw)
{
    QVector<CmpFixRecord> records;
    const auto it = std::find_if(nodes.cbegin(), nodes.cend(), [](const FlatUtfNode &node) {
        return node.name == QStringLiteral("Fix") && node.parentName == QStringLiteral("Cons");
    });
    if (it == nodes.cend())
        return records;

    const QByteArray chunk = readNodeData(*it, raw);
    if (chunk.isEmpty() || chunk.size() < kCmpFixRecordSize || chunk.size() % kCmpFixRecordSize != 0)
        return records;

    for (int offset = 0; offset + kCmpFixRecordSize <= chunk.size(); offset += kCmpFixRecordSize) {
        const QByteArray recordBytes = chunk.mid(offset, kCmpFixRecordSize);
        const QByteArray parentBytes = recordBytes.left(64).split('\0').value(0);
        const QByteArray childBytes = recordBytes.mid(64, 64).split('\0').value(0);
        const QString parentName = QString::fromLatin1(parentBytes).trimmed();
        const QString childName = QString::fromLatin1(childBytes).trimmed();
        if (parentName.isEmpty() || childName.isEmpty())
            continue;

        QDataStream ds(recordBytes.mid(128));
        ds.setByteOrder(QDataStream::LittleEndian);
        ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
        float tx = 0.0f, ty = 0.0f, tz = 0.0f;
        ds >> tx >> ty >> tz;

        QMatrix3x3 matrix;
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                float value = 0.0f;
                ds >> value;
                matrix(row, col) = value;
            }
        }

        CmpFixRecord record;
        record.parentName = parentName;
        record.childName = childName;
        record.translation = QVector3D(tx, ty, tz);
        record.rotation = quaternionFromRows(matrix);
        record.valid = qIsFinite(tx) && qIsFinite(ty) && qIsFinite(tz);
        records.append(record);
    }

    return records;
}

const CmpFixRecord *findTransformHint(const QVector<CmpFixRecord> &records, const QString &partName)
{
    const auto it = std::find_if(records.cbegin(), records.cend(), [&](const CmpFixRecord &record) {
        return record.childName.compare(partName, Qt::CaseInsensitive) == 0;
    });
    return it == records.cend() ? nullptr : &(*it);
}

QString firstPathForName(const QVector<UtfNodeRecord> &nodes, const QString &name)
{
    const auto it = std::find_if(nodes.cbegin(), nodes.cend(), [&](const UtfNodeRecord &record) {
        return record.name.compare(name, Qt::CaseInsensitive) == 0;
    });
    return it == nodes.cend() ? QString() : it->path;
}

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

    QVector<uint32_t> compactIndices;
    compactIndices.reserve(mesh.indices.size());
    for (uint32_t index : std::as_const(mesh.indices)) {
        if (index >= static_cast<uint32_t>(remap.size()))
            continue;
        const int mapped = remap.at(static_cast<qsizetype>(index));
        if (mapped < 0)
            continue;
        compactIndices.append(static_cast<uint32_t>(mapped));
    }

    const qsizetype triangleCount = compactIndices.size() / 3;
    compactIndices.resize(triangleCount * 3);

    QVector<uint32_t> filtered;
    filtered.reserve(compactIndices.size());
    for (qsizetype i = 0; i + 2 < compactIndices.size(); i += 3) {
        const uint32_t a = compactIndices.at(i);
        const uint32_t b = compactIndices.at(i + 1);
        const uint32_t c = compactIndices.at(i + 2);
        if (a == b || b == c || a == c)
            continue;
        filtered.append(a);
        filtered.append(b);
        filtered.append(c);
    }

    if (filtered.isEmpty())
        return false;

    mesh.vertices = std::move(sanitizedVertices);
    mesh.indices = std::move(filtered);
    compactTriangleSoupVertices(mesh);
    return true;
}

QVector<MeshData> decodeMeshVariantsFromBlock(const QByteArray &blockBytes)
{
    QVector<MeshData> meshes;

    const auto appendDecoded = [&](const DecodedMesh &decoded) {
        if (decoded.positions.isEmpty() || decoded.indices.isEmpty())
            return;
        MeshData mesh;
        mesh.vertices.reserve(decoded.positions.size());
        mesh.indices = decoded.indices;
        for (int i = 0; i < decoded.positions.size(); ++i) {
            MeshVertex vertex;
            vertex.position = decoded.positions.at(i);
            if (i < decoded.normals.size())
                vertex.normal = decoded.normals.at(i);
            if (i < decoded.uvs.size()) {
                vertex.u = decoded.uvs.at(i).x();
                vertex.v = decoded.uvs.at(i).y();
            }
            mesh.vertices.append(vertex);
        }
        if (sanitizeMesh(mesh))
            meshes.append(mesh);
    };

    appendDecoded(VmeshDecoder::decode(blockBytes));
    const auto offsets = VmeshDecoder::findStructuredSingleBlockOffsets(blockBytes);
    for (int offset : offsets)
        appendDecoded(VmeshDecoder::decodeStructuredSingleBlock(blockBytes, offset));

    return meshes;
}

} // namespace

UtfFileHeader CmpLoader::parseUtfHeader(const QByteArray &data)
{
    UtfFileHeader header;
    if (data.size() < kUtfHeaderSize)
        return header;

    QDataStream ds(data);
    ds.setByteOrder(QDataStream::LittleEndian);
    char magic[4]{};
    if (ds.readRawData(magic, 4) != 4)
        return header;
    header.magic = QByteArray(magic, 4);
    ds >> header.version
       >> header.nodeBlockOffset
       >> header.nodeBlockSize
       >> header.reserved0
       >> header.nodeEntrySize
       >> header.namesOffset
       >> header.namesAllocatedSize
       >> header.namesUsedSize
       >> header.dataOffset
       >> header.reserved1
       >> header.reserved2
       >> header.timestampLow
       >> header.timestampHigh;
    return header;
}

std::shared_ptr<UtfNode> CmpLoader::parseUtf(const QByteArray &data)
{
    const UtfFileHeader header = parseUtfHeader(data);
    if (!isUtfMagic(header.magic))
        return nullptr;

    const QVector<FlatUtfNode> flatNodes = parseFlatUtfNodes(data, header);
    auto root = std::make_shared<UtfNode>();
    root->name = QStringLiteral("\\");

    QHash<QString, std::shared_ptr<UtfNode>> created;
    created.insert(QString(), root);

    QVector<FlatUtfNode> sorted = flatNodes;
    std::sort(sorted.begin(), sorted.end(), [](const FlatUtfNode &a, const FlatUtfNode &b) {
        return a.path.count(QLatin1Char('/')) < b.path.count(QLatin1Char('/'));
    });

    for (const auto &record : std::as_const(sorted)) {
        auto node = std::make_shared<UtfNode>();
        node->name = record.name;
        node->data = readNodeData(record, data);

        const QString parentPath = record.path.section(QLatin1Char('/'), 0, -2);
        const auto parent = created.value(parentPath, root);
        parent->children.append(node);
        created.insert(record.path, node);
    }

    return root;
}

std::shared_ptr<UtfNode> CmpLoader::findNode(const std::shared_ptr<UtfNode> &root, const QString &path)
{
    if (!root)
        return nullptr;
    QStringList parts = path.split(QLatin1Char('\\'), Qt::SkipEmptyParts);
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

MeshData CmpLoader::buildMeshFromVMesh(const QByteArray &vmeshData,
                                       int startVertex, int numVertices,
                                       int startIndex, int numIndices)
{
    MeshData mesh;
    if (startVertex < 0 || numVertices <= 0 || startIndex < 0 || numIndices <= 0)
        return mesh;
    const auto sliceDecoded = [&](const DecodedMesh &decoded) -> MeshData {
        MeshData candidate;
        if (decoded.positions.isEmpty())
            return candidate;
        if (startVertex >= decoded.positions.size() || startIndex >= decoded.indices.size())
            return candidate;

        const int endVertex = qMin(startVertex + numVertices, decoded.positions.size());
        for (int i = startVertex; i < endVertex; ++i) {
            MeshVertex v;
            v.position = decoded.positions.at(i);
            if (i < decoded.normals.size())
                v.normal = decoded.normals.at(i);
            if (i < decoded.uvs.size()) {
                v.u = decoded.uvs.at(i).x();
                v.v = decoded.uvs.at(i).y();
            }
            candidate.vertices.append(v);
        }

        const int endIndex = qMin(startIndex + numIndices, decoded.indices.size());
        for (int i = startIndex; i < endIndex; ++i) {
            const uint32_t idx = decoded.indices.at(i);
            if (idx < static_cast<uint32_t>(startVertex))
                continue;
            const uint32_t rebased = idx - static_cast<uint32_t>(startVertex);
            if (rebased >= static_cast<uint32_t>(candidate.vertices.size()))
                continue;
            candidate.indices.append(rebased);
        }

        if (!sanitizeMesh(candidate))
            return {};
        return candidate;
    };

    mesh = sliceDecoded(VmeshDecoder::decode(vmeshData));
    if (!mesh.vertices.isEmpty())
        return mesh;

    const auto offsets = VmeshDecoder::findStructuredSingleBlockOffsets(vmeshData);
    for (int offset : offsets) {
        mesh = sliceDecoded(VmeshDecoder::decodeStructuredSingleBlock(vmeshData, offset));
        if (!mesh.vertices.isEmpty())
            return mesh;
    }

    return {};
}

ModelNode CmpLoader::extractPart(const QString &partPath,
                                 const QByteArray &raw,
                                 const QVector<UtfNodeRecord> &nodes,
                                 const QVector<VMeshDataBlock> &vmeshBlocks,
                                 const QVector<VMeshRefRecord> &vmeshRefs,
                                 const QVector<CmpFixRecord> &cmpFixRecords,
                                 QStringList *warnings)
{
    ModelNode node;
    const auto it = std::find_if(nodes.cbegin(), nodes.cend(), [&](const UtfNodeRecord &record) {
        return record.path.compare(partPath, Qt::CaseInsensitive) == 0;
    });
    if (it == nodes.cend())
        return node;

    node.name = it->name;
    if (const auto *transform = findTransformHint(cmpFixRecords, node.name)) {
        node.origin = transform->translation;
        node.rotation = transform->rotation;
    }

    const QString prefix = partPath + QLatin1Char('/');
    for (const auto &ref : vmeshRefs) {
        if (!ref.nodePath.startsWith(prefix, Qt::CaseInsensitive))
            continue;

        std::optional<QPair<int, VMeshDataBlock>> resolved;
        if (!ref.matchedSourceName.isEmpty()) {
            const auto byName = std::find_if(vmeshBlocks.cbegin(), vmeshBlocks.cend(), [&](const VMeshDataBlock &block) {
                return block.sourceName.compare(ref.matchedSourceName, Qt::CaseInsensitive) == 0;
            });
            if (byName != vmeshBlocks.cend())
                resolved = qMakePair(static_cast<int>(std::distance(vmeshBlocks.cbegin(), byName)), *byName);
        }
        if (!resolved.has_value())
            resolved = resolveVMeshDataBlock(ref.meshDataReference, vmeshBlocks);
        if (!resolved.has_value()) {
            if (warnings)
                warnings->append(QStringLiteral("Unresolved VMeshRef in %1").arg(ref.nodePath));
            continue;
        }

        const QByteArray blockBytes = raw.mid(resolved->second.dataOffset, resolved->second.usedSize);
        if (blockBytes.isEmpty())
            continue;
        MeshData mesh = buildMeshFromVMesh(blockBytes, ref.vertexStart, ref.vertexCount, ref.indexStart, ref.indexCount);
        if (!mesh.vertices.isEmpty()) {
            node.meshes.append(mesh);
            continue;
        }

        const auto familyMesh = decodeStructuredFamilyForRef(raw, vmeshBlocks, resolved->second, ref);
        if (familyMesh.has_value() && !familyMesh->vertices.isEmpty()) {
            node.meshes.append(*familyMesh);
            if (warnings)
                warnings->append(QStringLiteral("Used structured-family fallback for %1").arg(ref.nodePath));
            continue;
        }

        const auto variants = decodeMeshVariantsFromBlock(blockBytes);
        for (const auto &variant : variants) {
            if (!variant.vertices.isEmpty()) {
                node.meshes.append(variant);
                break;
            }
        }
    }

    if (node.meshes.isEmpty()) {
        for (const auto &record : nodes) {
            if (record.name != QStringLiteral("VMeshData"))
                continue;
            if (!record.path.startsWith(prefix, Qt::CaseInsensitive))
                continue;
            const QByteArray blockBytes = raw.mid(record.dataOffset, record.usedSize);
            const auto variants = decodeMeshVariantsFromBlock(blockBytes);
            if (!variants.isEmpty()) {
                node.meshes.append(variants.first());
                break;
            }
        }
    }

    return node;
}

DecodedModel CmpLoader::loadModel(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QByteArray raw = file.readAll();
    DecodedModel result;
    result.sourcePath = filePath;
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    result.format = suffix == QStringLiteral("cmp") ? NativeModelFormat::Cmp
                    : suffix == QStringLiteral("3db") ? NativeModelFormat::Db3
                    : NativeModelFormat::Unknown;

    result.header = parseUtfHeader(raw);
    if (!isUtfMagic(result.header.magic)) {
        result.warnings.append(QStringLiteral("Unsupported UTF header"));
        return result;
    }

    const QVector<FlatUtfNode> flatNodes = parseFlatUtfNodes(raw, result.header);
    result.utfNodes.reserve(flatNodes.size());
    for (const auto &node : flatNodes) {
        UtfNodeRecord record;
        record.name = node.name;
        record.parentName = node.parentName;
        record.path = node.path;
        record.flags = node.flags;
        record.peerOffset = node.peerOffset;
        record.childOffset = node.childOffset;
        record.dataOffset = node.dataOffset;
        record.allocatedSize = node.allocatedSize;
        record.usedSize = node.usedSize;
        result.utfNodes.append(record);
    }

    result.parts = buildPartsFromNodes(flatNodes, raw);
    result.vmeshDataBlocks = parseVMeshDataBlocks(flatNodes, raw);
    result.vmeshRefs = parseVMeshRefs(flatNodes, raw, result.vmeshDataBlocks);
    result.cmpFixRecords = parseCmpFixRecords(flatNodes, raw);

    ModelNode root;
    root.name = QFileInfo(filePath).fileName();

    if (result.format == NativeModelFormat::Cmp && !result.parts.isEmpty()) {
        for (const auto &part : std::as_const(result.parts)) {
            const QString partPath = firstPathForName(result.utfNodes, part.name);
            if (partPath.isEmpty())
                continue;
            ModelNode partNode = extractPart(partPath, raw, result.utfNodes, result.vmeshDataBlocks,
                                             result.vmeshRefs, result.cmpFixRecords, &result.warnings);
            if (!partNode.meshes.isEmpty())
                root.children.append(partNode);
        }
    }

    if (root.children.isEmpty()) {
        QString partPath = firstPathForName(result.utfNodes, QFileInfo(filePath).baseName());
        if (partPath.isEmpty())
            partPath = firstPathForName(result.utfNodes, QStringLiteral("Root"));
        ModelNode direct = extractPart(partPath, raw, result.utfNodes, result.vmeshDataBlocks,
                                       result.vmeshRefs, result.cmpFixRecords, &result.warnings);
        if (!direct.meshes.isEmpty() || !direct.children.isEmpty())
            root = direct;
    }

    if (root.meshes.isEmpty() && root.children.isEmpty()) {
        for (const auto &block : std::as_const(result.vmeshDataBlocks)) {
            const QByteArray blockBytes = raw.mid(block.dataOffset, block.usedSize);
            const auto variants = decodeMeshVariantsFromBlock(blockBytes);
            if (!variants.isEmpty()) {
                root.meshes.append(variants.first());
                result.warnings.append(QStringLiteral("Used direct VMeshData fallback decode."));
                break;
            }
        }
    }

    result.rootNode = root;
    return result;
}

ModelNode CmpLoader::loadCmp(const QString &filePath)
{
    return loadModel(filePath).rootNode;
}

ModelNode CmpLoader::load3db(const QString &filePath)
{
    return loadModel(filePath).rootNode;
}

void CmpLoader::parseUtfNode(const QByteArray &, int, const QByteArray &, int, std::shared_ptr<UtfNode> &)
{
    // Legacy recursive helper kept only for compatibility with the header API.
}

} // namespace flatlas::infrastructure
