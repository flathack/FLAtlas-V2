// infrastructure/io/CmpLoader.cpp - Decoder-first Freelancer CMP/3DB loader

#include "CmpLoader.h"

#include "VmeshDecoder.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QMatrix3x3>
#include <QRegularExpression>
#include <QtEndian>
#include <QtMath>

#include <algorithm>
#include <limits>
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
QVector<CmpTransformHint> buildCmpTransformHints(const QVector<CmpFixRecord> &records,
                                                 const QVector<NativeModelPart> &parts,
                                                 const QVector<FlatUtfNode> &nodes,
                                                 const QByteArray &raw);
QVector<MaterialReference> extractMaterialReferences(const QVector<FlatUtfNode> &nodes,
                                                     const QByteArray &raw);
QVector<PreviewMaterialBinding> buildPreviewMaterialBindings(const QVector<VMeshRefRecord> &vmeshRefs,
                                                             const QVector<NativeModelPart> &parts,
                                                             const QVector<UtfNodeRecord> &nodes,
                                                             const QVector<MaterialReference> &materialReferences);

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
    static constexpr quint32 kFreelancerCrcTable[] = {
        0u, 151466134u, 302932268u, 453595578u, 4285383705u, 4134204559u, 3982730549u, 3831797155u,
        4275800114u, 4158437540u, 3973441822u, 3855800712u, 28724267u, 145849533u, 330837255u, 448732561u,
        4256632932u, 4105183474u, 4021907784u, 3871228382u, 47895677u, 199091435u, 282375505u, 433292743u,
        57448534u, 174827712u, 291699066u, 409324012u, 4227947599u, 4110839001u, 3993976163u, 3876064757u,
        4218298568u, 4066971742u, 3915399652u, 3764875634u, 67364049u, 218420295u, 369985021u, 520795499u,
        95791354u, 213031020u, 398182870u, 515701056u, 4208487651u, 4091501685u, 3906342351u, 3788586329u,
        114897068u, 266207290u, 349655424u, 500195606u, 4189385909u, 4038312995u, 3954873753u, 3804079375u,
        4160927902u, 4043671560u, 3926710706u, 3809208612u, 124746887u, 241716241u, 358686123u, 476458301u,
        4141629840u, 4292571398u, 3838976188u, 3990163498u, 162629001u, 11973919u, 465560741u, 314102835u,
        134728098u, 16841012u, 436840590u, 319723544u, 4150922683u, 4268571949u, 3848563863u, 3965934593u,
        191582708u, 40657250u, 426062040u, 274858062u, 4094072301u, 4244743547u, 3859346625u, 4010787927u,
        4122008006u, 4239911248u, 3888036074u, 4005136508u, 182263263u, 64630089u, 416513267u, 299125861u,
        229794136u, 78991822u, 532414580u, 381366498u, 4074743105u, 4225275351u, 3771843693u, 3923178747u,
        4083804522u, 4201568764u, 3781658694u, 3898652880u, 201600371u, 84090341u, 503991391u, 386759881u,
        4026888508u, 4177674666u, 3792375824u, 3943440518u, 258520357u, 107972019u, 493278217u, 341959839u,
        249493774u, 131713432u, 483432482u, 366454964u, 4055055639u, 4172549505u, 3820837947u, 3938086061u,
        3988292384u, 3837768630u, 4290175500u, 4138848922u, 315967289u, 466778031u, 14362133u, 165418627u,
        325258002u, 442776452u, 23947838u, 141187752u, 3960393483u, 3842637725u, 4261457447u, 4144471729u,
        269456196u, 419996626u, 33682024u, 184992510u, 4016199517u, 3865405387u, 4251727473u, 4100654823u,
        4006878070u, 3889376224u, 4242176602u, 4124920524u, 297394031u, 415166457u, 62373443u, 179343061u,
        383165416u, 533828478u, 81314500u, 232780370u, 3921373169u, 3770439527u, 4222944989u, 4071765579u,
        3893177306u, 3775535948u, 4194519798u, 4077156960u, 392228803u, 510123861u, 91131631u, 208256633u,
        3949048716u, 3798369050u, 4184855200u, 4033405494u, 336361365u, 487278339u, 100800185u, 251995695u,
        364526526u, 482151208u, 129260178u, 246639108u, 3940024231u, 3822112561u, 4175011467u, 4057902621u,
        459588272u, 308539942u, 157983644u, 7181066u, 3825796777u, 3977131583u, 4127680389u, 4278212371u,
        3854518914u, 3971512852u, 4155583406u, 4273347384u, 450006683u, 332774925u, 148697015u, 31186721u,
        3872641748u, 4023706178u, 4108170232u, 4258956142u, 431888077u, 280569435u, 196114401u, 45565815u,
        403200742u, 286222960u, 168180682u, 50400092u, 3882196735u, 3999444585u, 4117495763u, 4234989381u,
        3758809720u, 3909997294u, 4060382036u, 4211323842u, 526853729u, 375396087u, 225003341u, 74348507u,
        517040714u, 399923932u, 215944038u, 98057200u, 3787238995u, 3904609989u, 4088582015u, 4206231529u,
        498987548u, 347783818u, 263426864u, 112501670u, 3805296133u, 3956737683u, 4041103145u, 4191774655u,
        3815143982u, 3932244664u, 4050131714u, 4168035220u, 470531639u, 353144481u, 235265819u, 117632909u,
    };

    quint32 crc = 0xFFFFFFFFu;
    for (char byte : value.toLower().toLatin1())
        crc = ((crc >> 8u) ^ kFreelancerCrcTable[(crc ^ static_cast<quint8>(byte)) & 0xFFu]) & 0xFFFFFFFFu;
    return (~crc) & 0xFFFFFFFFu;
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

QQuaternion quaternionFromRowsTuple(const QVector3D &row0, const QVector3D &row1, const QVector3D &row2)
{
    QMatrix3x3 matrix;
    matrix(0, 0) = row0.x(); matrix(0, 1) = row0.y(); matrix(0, 2) = row0.z();
    matrix(1, 0) = row1.x(); matrix(1, 1) = row1.y(); matrix(1, 2) = row1.z();
    matrix(2, 0) = row2.x(); matrix(2, 1) = row2.y(); matrix(2, 2) = row2.z();
    return quaternionFromRows(matrix);
}

constexpr float kMaxPreviewAbsCoord = 1000000.0f;
constexpr const char *kTextureExtensions[] = {".dds", ".tga", ".txm"};

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

QString normalizeModelKey(QString value)
{
    value = value.toLower().trimmed();
    value.replace(QLatin1Char('\\'), QLatin1Char('/'));
    value = QFileInfo(value).fileName();
    if (value.startsWith(QStringLiteral("part_")))
        value = value.mid(5);
    const int dot = value.lastIndexOf(QLatin1Char('.'));
    if (dot > 0)
        value = value.left(dot);

    QRegularExpression noisyLod(QStringLiteral("_lod(\\d)\\d+$"));
    value.replace(noisyLod, QStringLiteral("_lod\\1"));
    return value;
}

QString normalizeSourceKey(QString value)
{
    value = value.trimmed().toLower();
    value.replace(QLatin1Char('\\'), QLatin1Char('/'));
    value = QFileInfo(value).fileName();
    if (value.endsWith(QStringLiteral(".vms")))
        value.chop(4);
    return value;
}

bool sourceNameMatchesBlock(const QStringList &sourceNames, const VMeshDataBlock &block)
{
    if (sourceNames.isEmpty())
        return false;

    QSet<QString> wanted;
    for (const QString &sourceName : sourceNames) {
        const QString normalized = normalizeSourceKey(sourceName);
        if (!normalized.isEmpty()) {
            wanted.insert(normalized);
            wanted.insert(vmeshFamilyKeyFromSourceName(normalized));
        }
    }

    if (!block.sourceName.trimmed().isEmpty()) {
        const QString normalizedBlockSource = normalizeSourceKey(block.sourceName);
        if (wanted.contains(normalizedBlockSource))
            return true;
        if (wanted.contains(vmeshFamilyKeyFromSourceName(normalizedBlockSource)))
            return true;
    }

    if (!block.familyKey.trimmed().isEmpty() && wanted.contains(block.familyKey.toLower()))
        return true;

    return false;
}

QString topLevelUtfPathSegment(const QString &path)
{
    const QStringList parts = path.split(QRegularExpression(QStringLiteral(R"([/\\]+)")),
                                         Qt::SkipEmptyParts);
    return parts.isEmpty() ? QString() : parts.first();
}

QString levelSegmentFromUtfPath(const QString &path)
{
    const QStringList parts = path.split(QRegularExpression(QStringLiteral(R"([/\\]+)")),
                                         Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part.startsWith(QStringLiteral("Level"), Qt::CaseInsensitive))
            return part;
    }
    return {};
}

bool looksLikeMaterialReference(const QString &value, QString *kind)
{
    const QString lowered = value.trimmed().toLower();
    if (lowered.endsWith(QStringLiteral(".mat"))) {
        if (kind)
            *kind = QStringLiteral("material");
        return true;
    }
    for (const char *extension : kTextureExtensions) {
        if (lowered.endsWith(QLatin1String(extension))) {
            if (kind)
                *kind = QStringLiteral("texture");
            return true;
        }
    }
    return false;
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

struct FamilyMeshHeader {
    int startVertex = 0;
    int endVertex = 0;
    int numRefIndices = 0;
    int startIndex = 0;
};

int refSourceVertexEnd(const VMeshRefRecord &ref)
{
    return ref.vertexCount > 0 ? ref.vertexStart + ref.vertexCount : -1;
}

int refSourceIndexEnd(const VMeshRefRecord &ref)
{
    return ref.indexCount > 0 ? ref.indexStart + ref.indexCount : -1;
}

int refSourceGroupEnd(const VMeshRefRecord &ref)
{
    return ref.groupCount > 0 ? ref.groupStart + ref.groupCount : -1;
}

int scoreStructuredHeaderBlock(const VMeshDataBlock &block, const VMeshRefRecord &ref)
{
    if (block.headerHint.structureKind != QStringLiteral("structured-header"))
        return std::numeric_limits<int>::max();

    const int sourceGroupEnd = refSourceGroupEnd(ref);
    const int sourceVertexEnd = refSourceVertexEnd(ref);
    const int sourceIndexEnd = refSourceIndexEnd(ref);

    int score = 0;

    if (block.headerHint.meshHeaderCountHint > 0 && sourceGroupEnd > 0) {
        if (block.headerHint.meshHeaderCountHint == sourceGroupEnd) {
            score -= 5000;
        } else {
            score += qAbs(block.headerHint.meshHeaderCountHint - sourceGroupEnd) * 400;
        }
    }

    if (block.headerHint.meshHeaderEndVertexHint > 0 && sourceVertexEnd > 0) {
        if (block.headerHint.meshHeaderEndVertexHint == sourceVertexEnd) {
            score -= 3000;
        } else if (block.headerHint.meshHeaderEndVertexHint > sourceVertexEnd) {
            score += (block.headerHint.meshHeaderEndVertexHint - sourceVertexEnd);
        } else {
            score += (sourceVertexEnd - block.headerHint.meshHeaderEndVertexHint) * 800;
        }
    }

    if (block.headerHint.meshHeaderIndexEndHint > 0 && sourceIndexEnd > 0) {
        if (block.headerHint.meshHeaderIndexEndHint == sourceIndexEnd) {
            score -= 3000;
        } else if (block.headerHint.meshHeaderIndexEndHint > sourceIndexEnd) {
            score += (block.headerHint.meshHeaderIndexEndHint - sourceIndexEnd) * 2;
        } else {
            score += (sourceIndexEnd - block.headerHint.meshHeaderIndexEndHint) * 1200;
        }
    }

    return score;
}

int scoreStructuredStreamBlock(const VMeshDataBlock &block, const VMeshRefRecord &ref)
{
    if (block.headerHint.structureKind != QStringLiteral("vertex-stream"))
        return std::numeric_limits<int>::max();

    const int sourceVertexEnd = refSourceVertexEnd(ref);
    int score = 0;

    if (block.strideHint > 0) {
        const int capacityVertices = block.usedSize / block.strideHint;
        if (sourceVertexEnd > 0) {
            if (capacityVertices >= sourceVertexEnd) {
                score += capacityVertices - sourceVertexEnd;
            } else {
                score += (sourceVertexEnd - capacityVertices) * 2000;
            }
        }
    } else {
        score += 100000;
    }

    return score;
}

enum class StructuredHeaderSemantics {
    None,
    IndexEndMatches,
    VertexEndMatches,
    EndRangesMatch,
    EndRangesAndGroupMatch,
};

StructuredHeaderSemantics classifyStructuredHeaderSemantics(const VMeshDataBlock &block,
                                                            const VMeshRefRecord &ref)
{
    if (block.headerHint.structureKind != QStringLiteral("structured-header"))
        return StructuredHeaderSemantics::None;

    const int sourceGroupEnd = refSourceGroupEnd(ref);
    const int sourceVertexEnd = refSourceVertexEnd(ref);
    const int sourceIndexEnd = refSourceIndexEnd(ref);

    const bool groupMatch = block.headerHint.meshHeaderCountHint > 0
        && sourceGroupEnd > 0
        && block.headerHint.meshHeaderCountHint == sourceGroupEnd;
    const bool vertexMatch = block.headerHint.meshHeaderEndVertexHint > 0
        && sourceVertexEnd > 0
        && block.headerHint.meshHeaderEndVertexHint == sourceVertexEnd;
    const bool indexMatch = block.headerHint.meshHeaderIndexEndHint > 0
        && sourceIndexEnd > 0
        && block.headerHint.meshHeaderIndexEndHint == sourceIndexEnd;

    if (vertexMatch && indexMatch && groupMatch)
        return StructuredHeaderSemantics::EndRangesAndGroupMatch;
    if (vertexMatch && indexMatch)
        return StructuredHeaderSemantics::EndRangesMatch;
    if (vertexMatch)
        return StructuredHeaderSemantics::VertexEndMatches;
    if (indexMatch)
        return StructuredHeaderSemantics::IndexEndMatches;
    return StructuredHeaderSemantics::None;
}

int structuredHeaderSemanticsPenalty(StructuredHeaderSemantics semantics)
{
    switch (semantics) {
    case StructuredHeaderSemantics::EndRangesAndGroupMatch:
        return 0;
    case StructuredHeaderSemantics::EndRangesMatch:
        return 750;
    case StructuredHeaderSemantics::VertexEndMatches:
    case StructuredHeaderSemantics::IndexEndMatches:
        return 3000;
    case StructuredHeaderSemantics::None:
    default:
        return 12000;
    }
}

struct StructuredFamilyPlan {
    VMeshDataBlock headerBlock;
    std::optional<VMeshDataBlock> streamBlock;
    StructuredHeaderSemantics semantics = StructuredHeaderSemantics::None;
    int headerScore = std::numeric_limits<int>::max();
    int streamScore = std::numeric_limits<int>::max();
    int pairingPenalty = std::numeric_limits<int>::max();
    bool splitPlan = false;
    bool decodeReady = false;
};

QString describeStructuredFamilyPlan(const StructuredFamilyPlan &plan)
{
    const QString kind = plan.splitPlan
        ? QStringLiteral("family-split-header-stream")
        : QStringLiteral("single-block");

    QString semantics = QStringLiteral("none");
    switch (plan.semantics) {
    case StructuredHeaderSemantics::EndRangesAndGroupMatch:
        semantics = QStringLiteral("mesh-header-end-ranges-and-group-match-source");
        break;
    case StructuredHeaderSemantics::EndRangesMatch:
        semantics = QStringLiteral("mesh-header-end-ranges-match-source");
        break;
    case StructuredHeaderSemantics::VertexEndMatches:
        semantics = QStringLiteral("header-end-vertex-matches-source-range");
        break;
    case StructuredHeaderSemantics::IndexEndMatches:
        semantics = QStringLiteral("header-index-end-matches-source-range");
        break;
    case StructuredHeaderSemantics::None:
    default:
        break;
    }

    const QString streamName = plan.streamBlock.has_value()
        ? QFileInfo(plan.streamBlock->sourceName).fileName()
        : QStringLiteral("-");
    return QStringLiteral("%1 | header=%2 | stream=%3 | semantics=%4 | decode_ready=%5")
        .arg(kind,
             QFileInfo(plan.headerBlock.sourceName).fileName(),
             streamName,
             semantics,
             plan.decodeReady ? QStringLiteral("yes") : QStringLiteral("no"));
}

bool structuredHeaderSingleBlockDecodeReady(const VMeshDataBlock &block, const VMeshRefRecord &ref)
{
    const StructuredHeaderSemantics semantics = classifyStructuredHeaderSemantics(block, ref);
    if (semantics == StructuredHeaderSemantics::EndRangesAndGroupMatch)
        return true;

    const int sourceVertexEnd = refSourceVertexEnd(ref);
    const int sourceIndexEnd = refSourceIndexEnd(ref);
    return block.headerHint.meshHeaderEndVertexHint > 0
        && sourceVertexEnd > 0
        && block.headerHint.meshHeaderIndexEndHint > 0
        && sourceIndexEnd > 0
        && sourceVertexEnd <= block.headerHint.meshHeaderEndVertexHint
        && sourceIndexEnd <= block.headerHint.meshHeaderIndexEndHint;
}

int structuredStreamPairingPenalty(const VMeshDataBlock &headerBlock,
                                   const VMeshDataBlock &streamBlock,
                                   const VMeshRefRecord &ref)
{
    Q_UNUSED(ref);
    if (headerBlock.headerHint.structureKind != QStringLiteral("structured-header"))
        return 50000;
    if (streamBlock.headerHint.structureKind != QStringLiteral("vertex-stream"))
        return 50000;
    if (headerBlock.headerHint.vertexCountHint <= 0 || streamBlock.strideHint <= 0)
        return 16000;

    const int streamCapacityVertices = streamBlock.usedSize / streamBlock.strideHint;
    if (streamCapacityVertices < headerBlock.headerHint.vertexCountHint)
        return 32000 + (headerBlock.headerHint.vertexCountHint - streamCapacityVertices) * 10;

    return streamCapacityVertices - headerBlock.headerHint.vertexCountHint;
}

std::optional<StructuredFamilyPlan> chooseBestStructuredFamilySplitPlan(
    const QVector<VMeshDataBlock> &headerBlocks,
    const QVector<VMeshDataBlock> &streamBlocks,
    const VMeshRefRecord &ref)
{
    if (headerBlocks.isEmpty() || streamBlocks.isEmpty())
        return std::nullopt;

    std::optional<StructuredFamilyPlan> best;
    int bestScore = std::numeric_limits<int>::max();

    for (const auto &headerBlock : headerBlocks) {
        const StructuredHeaderSemantics semantics = classifyStructuredHeaderSemantics(headerBlock, ref);
        const int headerPenalty = structuredHeaderSemanticsPenalty(semantics);
        const int headerScore = scoreStructuredHeaderBlock(headerBlock, ref);
        for (const auto &streamBlock : streamBlocks) {
            const int streamScore = scoreStructuredStreamBlock(streamBlock, ref);
            const int pairingPenalty = structuredStreamPairingPenalty(headerBlock, streamBlock, ref);
            const bool decodeReady = semantics == StructuredHeaderSemantics::EndRangesAndGroupMatch
                && pairingPenalty < 32000;
            const int planScore = headerPenalty
                + headerScore
                + streamScore
                + pairingPenalty
                + (decodeReady ? 0 : 6000);
            if (!best.has_value() || planScore < bestScore) {
                bestScore = planScore;
                best = StructuredFamilyPlan{
                    headerBlock,
                    streamBlock,
                    semantics,
                    headerScore,
                    streamScore,
                    pairingPenalty,
                    true,
                    decodeReady,
                };
            }
        }
    }

    return best;
}

std::optional<StructuredFamilyPlan> chooseBestStructuredFamilySinglePlan(
    const QVector<VMeshDataBlock> &familyBlocks,
    const VMeshRefRecord &ref)
{
    std::optional<StructuredFamilyPlan> best;
    int bestScore = std::numeric_limits<int>::max();

    for (const auto &block : familyBlocks) {
        const StructuredHeaderSemantics semantics = classifyStructuredHeaderSemantics(block, ref);
        const int headerScore = scoreStructuredHeaderBlock(block, ref);
        const bool decodeReady = structuredHeaderSingleBlockDecodeReady(block, ref);
        const int planScore = headerScore
            + structuredHeaderSemanticsPenalty(semantics)
            + (decodeReady ? 0 : 6000);
        if (!best.has_value() || planScore < bestScore) {
            bestScore = planScore;
            best = StructuredFamilyPlan{
                block,
                std::nullopt,
                semantics,
                headerScore,
                std::numeric_limits<int>::max(),
                std::numeric_limits<int>::max(),
                false,
                decodeReady,
            };
        }
    }

    return best;
}

QVector<FamilyMeshHeader> parseStructuredFamilyMeshHeaders(const QByteArray &headerBytes, int expectedMeshCount)
{
    QVector<FamilyMeshHeader> headers;
    if (expectedMeshCount <= 0 || headerBytes.size() < 16 + (expectedMeshCount * 12))
        return headers;

    const quint16 meshCount = qFromLittleEndian<quint16>(
        reinterpret_cast<const uchar *>(headerBytes.constData() + 8));
    if (meshCount != expectedMeshCount)
        return headers;

    int pos = 16;
    int triangleStart = 0;
    headers.reserve(expectedMeshCount);
    for (int i = 0; i < expectedMeshCount; ++i) {
        if (pos + 12 > headerBytes.size())
            return {};
        pos += 4; // materialId
        const quint16 startVertex = qFromLittleEndian<quint16>(
            reinterpret_cast<const uchar *>(headerBytes.constData() + pos));
        const quint16 endVertex = qFromLittleEndian<quint16>(
            reinterpret_cast<const uchar *>(headerBytes.constData() + pos + 2));
        const quint16 numRefIndices = qFromLittleEndian<quint16>(
            reinterpret_cast<const uchar *>(headerBytes.constData() + pos + 4));
        if (endVertex < startVertex)
            return {};
        headers.append({static_cast<int>(startVertex),
                        static_cast<int>(endVertex),
                        static_cast<int>(numRefIndices),
                        triangleStart});
        triangleStart += static_cast<int>(numRefIndices);
        pos += 8;
    }

    return headers;
}

void refineStructuredFamilyWindowFromHeaders(const QVector<FamilyMeshHeader> &headers,
                                             int refGroupStart,
                                             int refGroupCount,
                                             int refVertexStart,
                                             int *subsetStart,
                                             int *subsetCount,
                                             int *expectedIndexCount,
                                             int *expectedIndexOffset)
{
    if (!subsetStart || !subsetCount || !expectedIndexCount || !expectedIndexOffset)
        return;
    if (headers.isEmpty() || refGroupCount <= 0 || refGroupStart < 0
        || refGroupStart + refGroupCount > headers.size()) {
        return;
    }

    int minStartVertex = std::numeric_limits<int>::max();
    int maxEndVertex = -1;
    int totalIndices = 0;
    int firstIndexOffset = headers.at(refGroupStart).startIndex;

    for (int i = refGroupStart; i < refGroupStart + refGroupCount; ++i) {
        const auto &header = headers.at(i);
        minStartVertex = qMin(minStartVertex, header.startVertex);
        maxEndVertex = qMax(maxEndVertex, header.endVertex);
        totalIndices += header.numRefIndices;
    }

    if (minStartVertex < 0 || maxEndVertex < minStartVertex || totalIndices <= 0)
        return;

    *subsetStart = refVertexStart + minStartVertex;
    *subsetCount = (maxEndVertex - minStartVertex) + 1;
    *expectedIndexCount = totalIndices;
    *expectedIndexOffset = firstIndexOffset * 2;
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
                                                                int subsetStart,
                                                                int subsetCount,
                                                                const ModelBounds &expectedBounds,
                                                                int preferredStride)
{
    if (indices.isEmpty() || positionBytes.isEmpty() || subsetStart < 0 || subsetCount <= 0)
        return std::nullopt;

    const auto [minIndexIt, maxIndexIt] = std::minmax_element(indices.cbegin(), indices.cend());
    const int maxIndex = static_cast<int>(*maxIndexIt);
    const int subsetEnd = subsetStart + subsetCount;
    const int vertexCount = qMax(maxIndex + 1, subsetEnd);
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
                if (subsetEnd > positions.size())
                    continue;
                if (minIndexIt == indices.cend())
                    continue;
                if (static_cast<int>(*minIndexIt) < subsetStart || maxIndex >= subsetEnd)
                    continue;

                int zeroPrefix = 0;
                for (const QVector3D &position : positions) {
                    if (qMax(qMax(qAbs(position.x()), qAbs(position.y())), qAbs(position.z())) < 1e-6f)
                        ++zeroPrefix;
                    else
                        break;
                }

                MeshData mesh;
                mesh.vertices.reserve(subsetCount);
                for (int i = subsetStart; i < subsetEnd && i < positions.size(); ++i) {
                    MeshVertex vertex;
                    vertex.position = positions.at(i);
                    mesh.vertices.append(vertex);
                }
                for (uint32_t index : indices)
                    mesh.indices.append(index - static_cast<uint32_t>(subsetStart));

                if (!sanitizeMesh(mesh))
                    continue;

                const float score = structuredGeometryScore(
                    positions.mid(subsetStart, subsetCount),
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
                                                                    int subsetStart,
                                                                    int subsetCount,
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
    return buildStructuredFamilyMesh(indices->second,
                                     headerBytes,
                                     subsetStart,
                                     subsetCount,
                                     expectedBounds,
                                     preferredStride);
}

std::optional<QPair<float, MeshData>> decodeStructuredFamilySplitPair(const QByteArray &headerBytes,
                                                                      const QByteArray &streamBytes,
                                                                      int expectedIndexCount,
                                                                      int expectedIndexOffset,
                                                                      int maxVertexIndexHint,
                                                                      int subsetStart,
                                                                      int subsetCount,
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

    std::optional<QPair<float, MeshData>> best;

    const auto headerCandidate = buildStructuredFamilyMesh(indices->second,
                                                           headerBytes,
                                                           subsetStart,
                                                           subsetCount,
                                                           expectedBounds,
                                                           preferredStride);
    if (headerCandidate.has_value())
        best = headerCandidate;

    bool canUseStream = true;
    if (!indices->second.isEmpty()) {
        const auto maxIndexIt = std::max_element(indices->second.cbegin(), indices->second.cend());
        if (maxIndexIt != indices->second.cend() && streamCapacityVertices > 0
            && static_cast<int>(*maxIndexIt) + 1 > streamCapacityVertices) {
            canUseStream = false;
        }
    }

    if (canUseStream) {
        const auto streamCandidate = buildStructuredFamilyMesh(indices->second,
                                                               streamBytes,
                                                               subsetStart,
                                                               subsetCount,
                                                               expectedBounds,
                                                               preferredStride);
        if (streamCandidate.has_value()
            && (!best.has_value() || streamCandidate->first < best->first)) {
            best = streamCandidate;
        }
    }

    return best;
}

std::optional<MeshData> decodeStructuredFamilyForRef(const QByteArray &raw,
                                                     const QVector<VMeshDataBlock> &vmeshBlocks,
                                                     const VMeshDataBlock &resolvedBlock,
                                                     const VMeshRefRecord &ref,
                                                     const QStringList &preferredSourceNames,
                                                     QString *selectedPlanDescription = nullptr)
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

    if (!preferredSourceNames.isEmpty()) {
        QVector<VMeshDataBlock> preferredBlocks;
        for (const auto &block : std::as_const(familyBlocks)) {
            if (sourceNameMatchesBlock(preferredSourceNames, block))
                preferredBlocks.append(block);
        }
        if (!preferredBlocks.isEmpty())
            familyBlocks = preferredBlocks;
    }

    QVector<VMeshDataBlock> headerBlocks;
    QVector<VMeshDataBlock> streamBlocks;
    for (const auto &block : std::as_const(familyBlocks)) {
        if (block.headerHint.structureKind == QStringLiteral("structured-header"))
            headerBlocks.append(block);
        if (block.headerHint.structureKind == QStringLiteral("vertex-stream"))
            streamBlocks.append(block);
    }

    std::sort(headerBlocks.begin(), headerBlocks.end(), [&](const VMeshDataBlock &a, const VMeshDataBlock &b) {
        return scoreStructuredHeaderBlock(a, ref) < scoreStructuredHeaderBlock(b, ref);
    });
    std::sort(streamBlocks.begin(), streamBlocks.end(), [&](const VMeshDataBlock &a, const VMeshDataBlock &b) {
        return scoreStructuredStreamBlock(a, ref) < scoreStructuredStreamBlock(b, ref);
    });

    std::optional<QPair<float, MeshData>> best;
    const int maxVertexIndexHint = qMax(ref.vertexStart + ref.vertexCount, ref.vertexCount);

    const auto trySplitPlan = [&](const StructuredFamilyPlan &plan) -> std::optional<QPair<float, MeshData>> {
        if (!plan.streamBlock.has_value())
            return std::nullopt;
        const auto &headerBlock = plan.headerBlock;
        const auto &streamBlock = plan.streamBlock.value();
        const QByteArray headerBytes = raw.mid(headerBlock.dataOffset, headerBlock.usedSize);
        const QByteArray streamBytes = raw.mid(streamBlock.dataOffset, streamBlock.usedSize);
        if (headerBytes.isEmpty() || streamBytes.isEmpty())
            return std::nullopt;
        int subsetStart = ref.vertexStart;
        int subsetCount = ref.vertexCount;
        int expectedIndexCount = ref.indexCount;
        int expectedIndexOffset = ref.indexStart * 2;
        const auto meshHeaders = parseStructuredFamilyMeshHeaders(headerBytes, headerBlock.headerHint.meshHeaderCountHint);
        refineStructuredFamilyWindowFromHeaders(meshHeaders,
                                                ref.groupStart,
                                                ref.groupCount,
                                                ref.vertexStart,
                                                &subsetStart,
                                                &subsetCount,
                                                &expectedIndexCount,
                                                &expectedIndexOffset);
        const int preferredStride = streamBlock.strideHint > 0 ? streamBlock.strideHint : resolvedBlock.strideHint;
        const int streamCapacityVertices = preferredStride > 0 ? streamBlock.usedSize / preferredStride : -1;
        const auto candidate = decodeStructuredFamilySplitPair(headerBytes,
                                                               streamBytes,
                                                               expectedIndexCount,
                                                               expectedIndexOffset,
                                                               qMax(1, maxVertexIndexHint),
                                                               subsetStart,
                                                               subsetCount,
                                                               ref.bounds,
                                                               preferredStride,
                                                               streamCapacityVertices);
        if (!candidate.has_value())
            return std::nullopt;
        const float rankedScore = candidate->first
            + static_cast<float>(structuredHeaderSemanticsPenalty(plan.semantics))
            + static_cast<float>(plan.pairingPenalty)
            + (plan.decodeReady ? 0.0f : 6000.0f);
        return qMakePair(rankedScore, candidate->second);
    };

    const auto trySinglePlan = [&](const StructuredFamilyPlan &plan) -> std::optional<QPair<float, MeshData>> {
        const auto &block = plan.headerBlock;
        const QByteArray blockBytes = raw.mid(block.dataOffset, block.usedSize);
        if (blockBytes.isEmpty())
            return std::nullopt;
        int subsetStart = ref.vertexStart;
        int subsetCount = ref.vertexCount;
        int expectedIndexCount = ref.indexCount;
        int expectedIndexOffset = ref.indexStart * 2;
        const auto meshHeaders = parseStructuredFamilyMeshHeaders(blockBytes, block.headerHint.meshHeaderCountHint);
        refineStructuredFamilyWindowFromHeaders(meshHeaders,
                                               ref.groupStart,
                                               ref.groupCount,
                                               ref.vertexStart,
                                               &subsetStart,
                                               &subsetCount,
                                               &expectedIndexCount,
                                               &expectedIndexOffset);

        const auto candidate = decodeStructuredFamilyVariant(
            blockBytes,
            expectedIndexCount,
            expectedIndexOffset,
            qMax(1, maxVertexIndexHint),
            subsetStart,
            subsetCount,
            ref.bounds,
            block.strideHint > 0 ? block.strideHint : resolvedBlock.strideHint);
        if (!candidate.has_value())
            return std::nullopt;
        const float rankedScore = candidate->first
            + static_cast<float>(structuredHeaderSemanticsPenalty(plan.semantics))
            + (plan.decodeReady ? 0.0f : 6000.0f);
        return qMakePair(rankedScore, candidate->second);
    };

    const auto splitPlan = chooseBestStructuredFamilySplitPlan(headerBlocks, streamBlocks, ref);
    const auto singlePlan = chooseBestStructuredFamilySinglePlan(familyBlocks, ref);

    if (splitPlan.has_value() && splitPlan->decodeReady) {
        const auto candidate = trySplitPlan(splitPlan.value());
        if (candidate.has_value()) {
            if (selectedPlanDescription)
                *selectedPlanDescription = describeStructuredFamilyPlan(splitPlan.value());
            return candidate->second;
        }
    }

    if (singlePlan.has_value() && singlePlan->decodeReady) {
        const auto candidate = trySinglePlan(singlePlan.value());
        if (candidate.has_value()) {
            if (selectedPlanDescription)
                *selectedPlanDescription = describeStructuredFamilyPlan(singlePlan.value());
            return candidate->second;
        }
    }

    if (splitPlan.has_value()) {
        const auto candidate = trySplitPlan(splitPlan.value());
        if (candidate.has_value()) {
            best = candidate;
            if (selectedPlanDescription)
                *selectedPlanDescription = describeStructuredFamilyPlan(splitPlan.value());
        }
    }

    if (singlePlan.has_value()) {
        const auto candidate = trySinglePlan(singlePlan.value());
        if (candidate.has_value() && (!best.has_value() || candidate->first < best->first)) {
            best = candidate;
            if (selectedPlanDescription)
                *selectedPlanDescription = describeStructuredFamilyPlan(singlePlan.value());
        }
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
        if (node.name != QStringLiteral("Root") || !node.path.endsWith(QStringLiteral("/Cmpnd/Root")))
            continue;

        NativeModelPart rootPart;
        rootPart.name = QStringLiteral("Root");
        const auto followers = childrenByParent.value(node.path);
        for (int childIndex : followers) {
            const auto &child = nodes.at(childIndex);
            if (child.name == QStringLiteral("File name") && rootPart.fileName.isEmpty())
                rootPart.fileName = readNativeTextNode(child, raw);
            else if (child.name == QStringLiteral("Object name") && rootPart.objectName.isEmpty())
                rootPart.objectName = readNativeTextNode(child, raw);
            else if (child.name == QStringLiteral("Index") && rootPart.cmpIndex < 0)
                rootPart.cmpIndex = readNativeU32Node(child, raw);
        }
        if (!rootPart.objectName.isEmpty() && rootPart.cmpIndex >= 0)
            parts.append(rootPart);
        break;
    }

    for (int i = 0; i < nodes.size(); ++i) {
        const auto &node = nodes.at(i);
        const bool isCmpPart = node.name.startsWith(QStringLiteral("Part_"))
            || (node.name == QStringLiteral("Root") && node.path.endsWith(QStringLiteral("/Cmpnd/Root")));
        if (!isCmpPart)
            continue;
        if (node.name == QStringLiteral("Root"))
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

        if (part.sourceName.isEmpty() || part.fileName.isEmpty() || part.objectName.isEmpty() || part.cmpIndex < 0) {
            for (int followerIndex = i + 1; followerIndex < nodes.size(); ++followerIndex) {
                const auto &follower = nodes.at(followerIndex);
                if (follower.name.startsWith(QStringLiteral("Part_")))
                    break;
                if (follower.name.endsWith(QStringLiteral(".vms"), Qt::CaseInsensitive) && part.sourceName.isEmpty())
                    part.sourceName = follower.name;
                else if (follower.name == QStringLiteral("File name") && part.fileName.isEmpty())
                    part.fileName = readNativeTextNode(follower, raw);
                else if (follower.name == QStringLiteral("Object name") && part.objectName.isEmpty())
                    part.objectName = readNativeTextNode(follower, raw);
                else if (follower.name == QStringLiteral("Index") && part.cmpIndex < 0)
                    part.cmpIndex = readNativeU32Node(follower, raw);
            }
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
        QSet<QString> crcCandidates;
        if (!blocks.at(i).sourceName.isEmpty()) {
            crcCandidates.insert(blocks.at(i).sourceName);
            crcCandidates.insert(blocks.at(i).sourceName.toLower());
            crcCandidates.insert(QFileInfo(blocks.at(i).sourceName).fileName());
            crcCandidates.insert(QFileInfo(blocks.at(i).sourceName).completeBaseName());
        }
        if (!blocks.at(i).familyKey.isEmpty()) {
            crcCandidates.insert(blocks.at(i).familyKey);
            crcCandidates.insert(blocks.at(i).familyKey.toLower());
        }
        for (const QString &candidate : std::as_const(crcCandidates)) {
            if (!candidate.isEmpty() && freelancerCrc32(candidate) == meshDataReference) {
                crcMatches.append(i);
                break;
            }
        }
    }
    if (crcMatches.size() == 1)
        return qMakePair(crcMatches.first(), blocks.at(crcMatches.first()));
    return std::nullopt;
}

std::optional<QPair<int, VMeshDataBlock>> resolveVMeshDataBlockForRef(
    quint32 meshDataReference,
    const QString &refNodePath,
    const QVector<VMeshDataBlock> &blocks)
{
    if (const auto resolved = resolveVMeshDataBlock(meshDataReference, blocks))
        return resolved;

    const QString refTopLevel = topLevelUtfPathSegment(refNodePath);
    if (refTopLevel.isEmpty())
        return std::nullopt;

    QVector<int> topLevelMatches;
    for (int i = 0; i < blocks.size(); ++i) {
        if (topLevelUtfPathSegment(blocks.at(i).nodePath).compare(refTopLevel, Qt::CaseInsensitive) == 0)
            topLevelMatches.append(i);
    }
    if (topLevelMatches.size() == 1)
        return qMakePair(topLevelMatches.first(), blocks.at(topLevelMatches.first()));

    const QString levelSegment = levelSegmentFromUtfPath(refNodePath).toLower();
    if (!levelSegment.isEmpty()) {
        QString lodToken = levelSegment;
        lodToken.replace(QStringLiteral("level"), QStringLiteral("lod"));
        QVector<int> lodMatches;
        for (int i = 0; i < blocks.size(); ++i) {
            const QString haystack = (blocks.at(i).sourceName + QLatin1Char('|') + blocks.at(i).familyKey).toLower();
            if (haystack.contains(lodToken))
                lodMatches.append(i);
        }
        if (lodMatches.size() == 1)
            return qMakePair(lodMatches.first(), blocks.at(lodMatches.first()));
    }
    return std::nullopt;
}

QStringList candidateSourceNamesForRef(const VMeshRefRecord &ref,
                                       const QVector<NativeModelPart> &parts)
{
    QStringList names;
    const QString normalizedModel = normalizeModelKey(ref.modelName);
    for (const auto &part : parts) {
        if (!part.sourceName.trimmed().isEmpty()) {
            if (!part.objectName.trimmed().isEmpty()
                && normalizeModelKey(part.objectName).compare(normalizedModel, Qt::CaseInsensitive) == 0) {
                names.append(part.sourceName);
            } else if (normalizeModelKey(part.name).compare(normalizedModel, Qt::CaseInsensitive) == 0) {
                names.append(part.sourceName);
            }
        }
    }
    names.removeAll(QString());
    names.removeDuplicates();
    return names;
}

std::optional<QPair<int, VMeshDataBlock>> resolveVMeshDataBlockForRefWithSources(
    const VMeshRefRecord &ref,
    const QVector<VMeshDataBlock> &blocks,
    const QVector<NativeModelPart> &parts)
{
    const QStringList sourceNames = candidateSourceNamesForRef(ref, parts);
    if (!sourceNames.isEmpty()) {
        QVector<int> matches;
        for (int i = 0; i < blocks.size(); ++i) {
            if (sourceNameMatchesBlock(sourceNames, blocks.at(i)))
                matches.append(i);
        }
        if (matches.size() == 1)
            return qMakePair(matches.first(), blocks.at(matches.first()));
    }
    return resolveVMeshDataBlockForRef(ref.meshDataReference, ref.nodePath, blocks);
}

QVector<VMeshRefRecord> parseVMeshRefs(const QVector<FlatUtfNode> &nodes,
                                       const QByteArray &raw,
                                       const QVector<VMeshDataBlock> &blocks,
                                       const QVector<NativeModelPart> &parts)
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

        ref.modelName = topLevelUtfPathSegment(node.path);
        const QStringList pathParts = node.path.split(QRegularExpression(QStringLiteral(R"([/\\]+)")),
                                                     Qt::SkipEmptyParts);
        for (const QString &segment : pathParts) {
            if (segment.startsWith(QStringLiteral("Level"), Qt::CaseInsensitive)) {
                ref.levelName = segment;
                break;
            }
        }

        const auto resolved = resolveVMeshDataBlockForRefWithSources(ref, blocks, parts);
        if (resolved.has_value()) {
            ref.matchedSourceName = resolved->second.sourceName;
            if (resolved->first == static_cast<int>(meshDataReference))
                ref.resolutionHint = QStringLiteral("direct-index");
            else if (!topLevelUtfPathSegment(node.path).isEmpty()
                     && topLevelUtfPathSegment(node.path).compare(topLevelUtfPathSegment(resolved->second.nodePath),
                                                                  Qt::CaseInsensitive) == 0) {
                ref.resolutionHint = QStringLiteral("top-level-path");
            } else {
                ref.resolutionHint = QStringLiteral("crc-or-fallback");
            }
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

const CmpTransformHint *findCombinedTransformHint(const QVector<CmpTransformHint> &hints, const QString &partName)
{
    const auto it = std::find_if(hints.cbegin(), hints.cend(), [&](const CmpTransformHint &hint) {
        return hint.partName.compare(partName, Qt::CaseInsensitive) == 0;
    });
    return it == hints.cend() ? nullptr : &(*it);
}

bool shouldKeepEmptyCmpPartNode(const QString &partName,
                                const QSet<QString> &parentPartNames,
                                const QVector<CmpTransformHint> &hints)
{
    const QString lowered = partName.trimmed().toLower();
    if (lowered.isEmpty())
        return false;
    if (parentPartNames.contains(lowered))
        return true;

    const auto it = std::find_if(hints.cbegin(), hints.cend(), [&](const CmpTransformHint &hint) {
        return hint.partName.compare(partName, Qt::CaseInsensitive) == 0
            && (hint.hasLocalTranslation || hint.hasLocalRotation
                || hint.hasCombinedTranslation || hint.hasCombinedRotation);
    });
    return it != hints.cend();
}

bool refBelongsToPart(const VMeshRefRecord &ref,
                      const NativeModelPart &part,
                      const QString &partPath)
{
    const QString prefix = partPath + QLatin1Char('/');
    if (!partPath.isEmpty() && ref.nodePath.startsWith(prefix, Qt::CaseInsensitive))
        return true;
    if (!part.sourceName.trimmed().isEmpty()
        && ref.matchedSourceName.compare(part.sourceName, Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (!part.objectName.trimmed().isEmpty()) {
        if (ref.parentName.compare(part.objectName, Qt::CaseInsensitive) == 0)
            return true;
        if (ref.nodePath.contains(part.objectName, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

QQuaternion combineRotations(const QQuaternion &parent, const QQuaternion &local)
{
    return (parent * local).normalized();
}

QVector3D rotateTranslation(const QQuaternion &rotation, const QVector3D &value)
{
    return rotation.rotatedVector(value);
}

QPair<QVector3D, QQuaternion> combinedCmpTransformForPart(
    const QString &partName,
    const QHash<QString, QString> &parentByPart,
    const QHash<QString, QVector3D> &localTranslations,
    const QHash<QString, QQuaternion> &localRotations,
    QHash<QString, QVector3D> &combinedTranslations,
    QHash<QString, QQuaternion> &combinedRotations,
    QSet<QString> &activeParts)
{
    if (combinedTranslations.contains(partName) || combinedRotations.contains(partName)) {
        return qMakePair(combinedTranslations.value(partName, QVector3D()),
                         combinedRotations.value(partName, QQuaternion(1.0f, 0.0f, 0.0f, 0.0f)));
    }

    if (activeParts.contains(partName)) {
        return qMakePair(localTranslations.value(partName, QVector3D()),
                         localRotations.value(partName, QQuaternion(1.0f, 0.0f, 0.0f, 0.0f)));
    }

    activeParts.insert(partName);
    const QVector3D localTranslation = localTranslations.value(partName, QVector3D());
    const QQuaternion localRotation = localRotations.value(partName, QQuaternion(1.0f, 0.0f, 0.0f, 0.0f));
    const QString parentName = parentByPart.value(partName);

    QVector3D parentTranslation;
    QQuaternion parentRotation(1.0f, 0.0f, 0.0f, 0.0f);
    if (!parentName.isEmpty()) {
        const auto parentCombined = combinedCmpTransformForPart(parentName,
                                                                parentByPart,
                                                                localTranslations,
                                                                localRotations,
                                                                combinedTranslations,
                                                                combinedRotations,
                                                                activeParts);
        parentTranslation = parentCombined.first;
        parentRotation = parentCombined.second;
    }

    const QQuaternion combinedRotation = combineRotations(parentRotation, localRotation);
    const QVector3D combinedTranslation = parentTranslation + rotateTranslation(parentRotation, localTranslation);
    combinedTranslations.insert(partName, combinedTranslation);
    combinedRotations.insert(partName, combinedRotation);
    activeParts.remove(partName);
    return qMakePair(combinedTranslation, combinedRotation);
}

struct JointLocalTransformMaps {
    QHash<QString, QVector3D> translations;
    QHash<QString, QQuaternion> rotations;
    QHash<QString, QString> parents;
};

QString parseCmpJointString(const QByteArray &blob, int *pos)
{
    if (!pos || *pos < 0 || *pos + 64 > blob.size())
        return {};
    const QByteArray value = blob.mid(*pos, 64).split('\0').value(0);
    *pos += 64;
    return QString::fromLatin1(value).trimmed();
}

QVector3D parseCmpJointVector(const QByteArray &blob, int *pos)
{
    if (!pos || *pos < 0 || *pos + 12 > blob.size())
        return {};
    QDataStream ds(blob.mid(*pos, 12));
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
    float x = 0.0f, y = 0.0f, z = 0.0f;
    ds >> x >> y >> z;
    *pos += 12;
    return QVector3D(x, y, z);
}

QQuaternion parseCmpJointRotation(const QByteArray &blob, int *pos)
{
    if (!pos || *pos < 0 || *pos + 36 > blob.size())
        return QQuaternion(1.0f, 0.0f, 0.0f, 0.0f);

    QDataStream ds(blob.mid(*pos, 36));
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
    float values[9]{};
    for (float &value : values)
        ds >> value;
    *pos += 36;
    return quaternionFromRowsTuple(QVector3D(values[0], values[1], values[2]),
                                   QVector3D(values[3], values[4], values[5]),
                                   QVector3D(values[6], values[7], values[8]));
}

JointLocalTransformMaps extract208ByteJointLocals(const QVector<FlatUtfNode> &nodes,
                                                  const QVector<NativeModelPart> &parts,
                                                  const QByteArray &raw,
                                                  const QString &nodeName)
{
    JointLocalTransformMaps result;
    const auto jointIt = std::find_if(nodes.cbegin(), nodes.cend(), [&](const FlatUtfNode &node) {
        return node.name.compare(nodeName, Qt::CaseInsensitive) == 0;
    });
    if (jointIt == nodes.cend())
        return result;

    const QByteArray blob = readNodeData(*jointIt, raw);
    constexpr int kRecordSize = 208;
    if (blob.isEmpty() || (blob.size() % kRecordSize) != 0)
        return result;

    QHash<QString, NativeModelPart> partByObjectName;
    for (const auto &part : parts) {
        if (!part.objectName.trimmed().isEmpty())
            partByObjectName.insert(part.objectName.toLower(), part);
    }

    for (int recordIndex = 0; recordIndex < blob.size() / kRecordSize; ++recordIndex) {
        int pos = recordIndex * kRecordSize;
        const QString parentObjectName = parseCmpJointString(blob, &pos);
        const QString objectName = parseCmpJointString(blob, &pos);
        const QVector3D offsetA = parseCmpJointVector(blob, &pos);
        const QVector3D offsetB = parseCmpJointVector(blob, &pos);
        const QQuaternion rotation = parseCmpJointRotation(blob, &pos);

        const auto partIt = partByObjectName.constFind(objectName.toLower());
        if (partIt == partByObjectName.cend())
            continue;

        result.translations.insert(partIt->name.toLower(), offsetA + offsetB);
        result.rotations.insert(partIt->name.toLower(), rotation);
        if (!parentObjectName.trimmed().isEmpty()) {
            const auto parentIt = partByObjectName.constFind(parentObjectName.toLower());
            result.parents.insert(partIt->name.toLower(),
                                  parentIt == partByObjectName.cend() ? QString() : parentIt->name);
        } else {
            result.parents.insert(partIt->name.toLower(), QString());
        }
    }

    return result;
}

QVector<CmpTransformHint> buildCmpTransformHints(const QVector<CmpFixRecord> &records,
                                                 const QVector<NativeModelPart> &parts,
                                                 const QVector<FlatUtfNode> &nodes,
                                                 const QByteArray &raw)
{
    QVector<CmpTransformHint> hints;
    if (records.isEmpty() && parts.isEmpty())
        return hints;

    QHash<QString, QString> parentByPart;
    for (const auto &part : parts)
        parentByPart.insert(part.name.toLower(), part.parentPartName);
    for (const auto &record : records) {
        if (!record.parentName.trimmed().isEmpty())
            parentByPart.insert(record.childName.toLower(), record.parentName);
    }

    QHash<QString, QVector3D> localTranslations;
    QHash<QString, QQuaternion> localRotations;
    for (const auto &record : records) {
        localTranslations.insert(record.childName.toLower(), record.translation);
        localRotations.insert(record.childName.toLower(), record.rotation);
    }

    const JointLocalTransformMaps revLocals = extract208ByteJointLocals(nodes, parts, raw, QStringLiteral("Rev"));
    const JointLocalTransformMaps prisLocals = extract208ByteJointLocals(nodes, parts, raw, QStringLiteral("Pris"));

    for (auto it = revLocals.translations.cbegin(); it != revLocals.translations.cend(); ++it)
        localTranslations.insert(it.key(), it.value());
    for (auto it = revLocals.rotations.cbegin(); it != revLocals.rotations.cend(); ++it)
        localRotations.insert(it.key(), it.value());
    for (auto it = revLocals.parents.cbegin(); it != revLocals.parents.cend(); ++it)
        parentByPart.insert(it.key(), it.value());

    for (auto it = prisLocals.translations.cbegin(); it != prisLocals.translations.cend(); ++it)
        localTranslations.insert(it.key(), it.value());
    for (auto it = prisLocals.rotations.cbegin(); it != prisLocals.rotations.cend(); ++it)
        localRotations.insert(it.key(), it.value());
    for (auto it = prisLocals.parents.cbegin(); it != prisLocals.parents.cend(); ++it)
        parentByPart.insert(it.key(), it.value());

    QSet<QString> partNames;
    for (const auto &part : parts)
        partNames.insert(part.name.toLower());
    for (const auto &record : records)
        partNames.insert(record.childName.toLower());
    for (auto it = revLocals.translations.cbegin(); it != revLocals.translations.cend(); ++it)
        partNames.insert(it.key());
    for (auto it = prisLocals.translations.cbegin(); it != prisLocals.translations.cend(); ++it)
        partNames.insert(it.key());

    QHash<QString, QVector3D> combinedTranslations;
    QHash<QString, QQuaternion> combinedRotations;
    for (const QString &partKey : std::as_const(partNames)) {
        QSet<QString> activeParts;
        combinedCmpTransformForPart(partKey,
                                    parentByPart,
                                    localTranslations,
                                    localRotations,
                                    combinedTranslations,
                                    combinedRotations,
                                    activeParts);
    }

    hints.reserve(partNames.size());
    for (const QString &partKey : std::as_const(partNames)) {
        CmpTransformHint hint;
        const auto partIt = std::find_if(parts.cbegin(), parts.cend(), [&](const NativeModelPart &part) {
            return part.name.compare(partKey, Qt::CaseInsensitive) == 0;
        });
        hint.partName = partIt != parts.cend() ? partIt->name : partKey;
        hint.parentPartName = parentByPart.value(partKey);
        hint.localTranslation = localTranslations.value(partKey, QVector3D());
        hint.localRotation = localRotations.value(partKey, QQuaternion(1.0f, 0.0f, 0.0f, 0.0f));
        hint.combinedTranslation = combinedTranslations.value(partKey, QVector3D());
        hint.combinedRotation = combinedRotations.value(partKey, QQuaternion(1.0f, 0.0f, 0.0f, 0.0f));
        hint.hasLocalTranslation = localTranslations.contains(partKey);
        hint.hasLocalRotation = localRotations.contains(partKey);
        hint.hasCombinedTranslation = combinedTranslations.contains(partKey);
        hint.hasCombinedRotation = combinedRotations.contains(partKey);
        hints.append(hint);
    }
    return hints;
}

QString firstPathForName(const QVector<UtfNodeRecord> &nodes, const QString &name)
{
    const auto it = std::find_if(nodes.cbegin(), nodes.cend(), [&](const UtfNodeRecord &record) {
        return record.name.compare(name, Qt::CaseInsensitive) == 0;
    });
    return it == nodes.cend() ? QString() : it->path;
}

QString matchedPartNameForRef(const VMeshRefRecord &ref,
                              const QVector<NativeModelPart> &parts,
                              const QVector<UtfNodeRecord> &nodes)
{
    for (const auto &part : parts) {
        const QString path = firstPathForName(nodes, part.name);
        if (!path.isEmpty() && ref.nodePath.startsWith(path + QLatin1Char('/'), Qt::CaseInsensitive))
            return part.name;
    }
    return {};
}

QStringList nodeReferenceCandidates(const FlatUtfNode &node, const QByteArray &raw)
{
    QStringList values{node.name};
    const QString textValue = readNativeTextNode(node, raw);
    if (!textValue.isEmpty())
        values.append(textValue);
    return values;
}

QVector<MaterialReference> extractMaterialReferences(const QVector<FlatUtfNode> &nodes,
                                                     const QByteArray &raw)
{
    QVector<MaterialReference> references;
    QSet<QString> seen;
    for (const auto &node : nodes) {
        const QStringList candidates = nodeReferenceCandidates(node, raw);
        for (const QString &candidate : candidates) {
            const QString normalized = candidate.trimmed();
            QString kind;
            if (!looksLikeMaterialReference(normalized, &kind))
                continue;
            const QString key = kind + QLatin1Char('|') + normalized.toLower() + QLatin1Char('|') + node.path.toLower();
            if (seen.contains(key))
                continue;
            seen.insert(key);
            references.append({kind, normalized, node.name, node.path});
        }
    }
    return references;
}

QStringList matchTextureCandidates(const QString &modelName,
                                   const QString &levelName,
                                   const QString &partName,
                                   const QStringList &sourceNames,
                                   int groupStart,
                                   int groupCount,
                                   const QVector<MaterialReference> &materialReferences,
                                   QString *matchHint,
                                   QString *referenceNodePath)
{
    QVector<MaterialReference> textures;
    QStringList materialValues;
    for (const auto &reference : materialReferences) {
        if (reference.kind == QStringLiteral("texture"))
            textures.append(reference);
        else if (reference.kind == QStringLiteral("material"))
            materialValues.append(reference.value);
    }

    QSet<QString> tokenSet;
    tokenSet.insert(normalizeModelKey(modelName));
    tokenSet.insert(normalizeModelKey(levelName));
    tokenSet.insert(normalizeModelKey(partName));
    for (const QString &sourceName : sourceNames)
        tokenSet.insert(normalizeModelKey(sourceName));
    tokenSet.remove(QString());

    struct ScoredReference {
        int score = 0;
        MaterialReference reference;
    };
    QVector<ScoredReference> scored;
    for (const auto &texture : textures) {
        const QStringList haystackParts{
            normalizeModelKey(texture.value),
            normalizeModelKey(texture.nodeName),
            normalizeModelKey(texture.nodePath),
            QString::number(groupStart),
            QString::number(groupCount),
        };
        int score = 0;
        for (const QString &token : tokenSet) {
            if (token.isEmpty())
                continue;
            for (const QString &hay : haystackParts) {
                if (!hay.isEmpty() && hay.contains(token)) {
                    ++score;
                    break;
                }
            }
        }
        if (score > 0)
            scored.append({score, texture});
    }

    QStringList candidates;
    if (!scored.isEmpty()) {
        std::sort(scored.begin(), scored.end(), [](const ScoredReference &a, const ScoredReference &b) {
            if (a.score != b.score)
                return a.score > b.score;
            return a.reference.value.toLower() < b.reference.value.toLower();
        });
        for (const auto &item : std::as_const(scored))
            candidates.append(item.reference.value);
        if (matchHint)
            *matchHint = QStringLiteral("token-match");
        if (referenceNodePath)
            *referenceNodePath = scored.first().reference.nodePath;
        return candidates;
    }

    if (textures.size() == 1) {
        if (matchHint)
            *matchHint = QStringLiteral("single-texture-fallback");
        if (referenceNodePath)
            *referenceNodePath = textures.first().nodePath;
        return {textures.first().value};
    }

    if (!textures.isEmpty()) {
        if (matchHint)
            *matchHint = QStringLiteral("first-texture-fallback");
        if (referenceNodePath)
            *referenceNodePath = textures.first().nodePath;
        for (const auto &item : std::as_const(textures))
            candidates.append(item.value);
        return candidates;
    }

    if (matchHint)
        *matchHint = QStringLiteral("no-texture-reference");
    return {};
}

QVector<PreviewMaterialBinding> buildPreviewMaterialBindings(const QVector<VMeshRefRecord> &vmeshRefs,
                                                             const QVector<NativeModelPart> &parts,
                                                             const QVector<UtfNodeRecord> &nodes,
                                                             const QVector<MaterialReference> &materialReferences)
{
    QVector<PreviewMaterialBinding> bindings;
    if (vmeshRefs.isEmpty() || materialReferences.isEmpty())
        return bindings;

    QString singleMaterialValue;
    int materialCount = 0;
    for (const auto &reference : materialReferences) {
        if (reference.kind == QStringLiteral("material")) {
            singleMaterialValue = reference.value;
            ++materialCount;
        }
    }

    for (const auto &ref : vmeshRefs) {
        PreviewMaterialBinding binding;
        binding.modelName = ref.modelName;
        binding.levelName = ref.levelName;
        binding.partName = matchedPartNameForRef(ref, parts, nodes);
        binding.groupStart = ref.groupStart;
        binding.groupCount = ref.groupCount;
        if (!ref.matchedSourceName.isEmpty())
            binding.sourceNames.append(ref.matchedSourceName);
        QString referenceNodePath;
        binding.textureCandidates = matchTextureCandidates(binding.modelName,
                                                           binding.levelName,
                                                           binding.partName,
                                                           binding.sourceNames,
                                                           binding.groupStart,
                                                           binding.groupCount,
                                                           materialReferences,
                                                           &binding.matchHint,
                                                           &referenceNodePath);
        if (!binding.textureCandidates.isEmpty())
            binding.textureValue = binding.textureCandidates.first();
        binding.referenceNodePath = referenceNodePath;
        if (materialCount == 1)
            binding.materialValue = singleMaterialValue;
        bindings.append(binding);
    }
    return bindings;
}

struct StructuredMeshHeader {
    int materialId = -1;
    int startVertex = 0;
    int endVertex = 0;
    int numRefIndices = 0;
    int startIndex = 0;
};

QVector<StructuredMeshHeader> parseStructuredMeshHeaders(const QByteArray &vmeshData)
{
    QVector<StructuredMeshHeader> headers;
    if (vmeshData.size() < 16)
        return headers;

    const quint16 meshCount = qFromLittleEndian<quint16>(
        reinterpret_cast<const uchar *>(vmeshData.constData() + 8));
    if (meshCount == 0)
        return headers;

    int pos = 16;
    int triangleStart = 0;
    headers.reserve(meshCount);
    for (int i = 0; i < meshCount; ++i) {
        if (pos + 12 > vmeshData.size())
            return {};
        const quint32 materialId = qFromLittleEndian<quint32>(
            reinterpret_cast<const uchar *>(vmeshData.constData() + pos));
        const quint16 startVertex = qFromLittleEndian<quint16>(
            reinterpret_cast<const uchar *>(vmeshData.constData() + pos + 4));
        const quint16 endVertex = qFromLittleEndian<quint16>(
            reinterpret_cast<const uchar *>(vmeshData.constData() + pos + 6));
        const quint16 numRefIndices = qFromLittleEndian<quint16>(
            reinterpret_cast<const uchar *>(vmeshData.constData() + pos + 8));
        if (endVertex < startVertex)
            return {};
        headers.append({static_cast<int>(materialId),
                        static_cast<int>(startVertex),
                        static_cast<int>(endVertex),
                        static_cast<int>(numRefIndices),
                        triangleStart});
        triangleStart += numRefIndices;
        pos += 12;
    }
    return headers;
}

const PreviewMaterialBinding *findPreviewMaterialBinding(const QVector<PreviewMaterialBinding> &bindings,
                                                         const VMeshRefRecord &ref,
                                                         const QString &partName)
{
    const auto it = std::find_if(bindings.cbegin(), bindings.cend(), [&](const PreviewMaterialBinding &binding) {
        if (binding.groupStart != ref.groupStart || binding.groupCount != ref.groupCount)
            return false;
        if (!binding.partName.isEmpty() && !partName.isEmpty()
            && binding.partName.compare(partName, Qt::CaseInsensitive) != 0) {
            return false;
        }
        if (!binding.modelName.isEmpty() && !ref.modelName.isEmpty()
            && binding.modelName.compare(ref.modelName, Qt::CaseInsensitive) != 0) {
            return false;
        }
        if (!binding.levelName.isEmpty() && !ref.levelName.isEmpty()
            && binding.levelName.compare(ref.levelName, Qt::CaseInsensitive) != 0) {
            return false;
        }
        return true;
    });
    return it == bindings.cend() ? nullptr : &(*it);
}

bool isFiniteVector(const QVector3D &value)
{
    return qIsFinite(value.x()) && qIsFinite(value.y()) && qIsFinite(value.z());
}

ModelBounds calculateMeshBounds(const MeshData &mesh)
{
    ModelBounds bounds;
    for (const auto &vertex : mesh.vertices) {
        if (!isFiniteVector(vertex.position))
            continue;
        if (!bounds.valid) {
            bounds.minCorner = vertex.position;
            bounds.maxCorner = vertex.position;
            bounds.valid = true;
            continue;
        }
        bounds.minCorner.setX(qMin(bounds.minCorner.x(), vertex.position.x()));
        bounds.minCorner.setY(qMin(bounds.minCorner.y(), vertex.position.y()));
        bounds.minCorner.setZ(qMin(bounds.minCorner.z(), vertex.position.z()));
        bounds.maxCorner.setX(qMax(bounds.maxCorner.x(), vertex.position.x()));
        bounds.maxCorner.setY(qMax(bounds.maxCorner.y(), vertex.position.y()));
        bounds.maxCorner.setZ(qMax(bounds.maxCorner.z(), vertex.position.z()));
    }
    if (bounds.valid)
        bounds.radius = (bounds.maxCorner - bounds.minCorner).length() * 0.5f;
    return bounds;
}

float meshBoundsScore(const MeshData &mesh, const ModelBounds &expectedBounds)
{
    if (!expectedBounds.valid)
        return 0.0f;
    const ModelBounds actualBounds = calculateMeshBounds(mesh);
    if (!actualBounds.valid)
        return std::numeric_limits<float>::infinity();

    const QVector3D minDelta = actualBounds.minCorner - expectedBounds.minCorner;
    const QVector3D maxDelta = actualBounds.maxCorner - expectedBounds.maxCorner;
    return minDelta.lengthSquared()
        + maxDelta.lengthSquared()
        + qAbs(actualBounds.radius - expectedBounds.radius);
}

QVector<int> candidateVertexStridesForRef(const VMeshRefRecord &ref)
{
    QVector<int> strides;
    const auto appendStride = [&](int stride) {
        if (stride >= 12 && !strides.contains(stride))
            strides.append(stride);
    };

    appendStride(strideHintFromSourceName(ref.matchedSourceName));
    for (int stride : {12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, 72, 80, 96, 112})
        appendStride(stride);
    return strides;
}

bool decodeRawWindowVertices(const QByteArray &blockBytes,
                             int offset,
                             int vertexCount,
                             int vertexStride,
                             QVector<MeshVertex> *outVertices)
{
    if (!outVertices || offset < 0 || vertexCount <= 0 || vertexStride < 12)
        return false;
    const qint64 requiredBytes = static_cast<qint64>(vertexCount) * vertexStride;
    if (offset + requiredBytes > blockBytes.size())
        return false;

    outVertices->clear();
    outVertices->reserve(vertexCount);
    for (int i = 0; i < vertexCount; ++i) {
        const int base = offset + (i * vertexStride);
        const float x = qFromLittleEndian<float>(reinterpret_cast<const uchar *>(blockBytes.constData() + base));
        const float y = qFromLittleEndian<float>(reinterpret_cast<const uchar *>(blockBytes.constData() + base + 4));
        const float z = qFromLittleEndian<float>(reinterpret_cast<const uchar *>(blockBytes.constData() + base + 8));
        if (!qIsFinite(x) || !qIsFinite(y) || !qIsFinite(z))
            return false;
        if (qMax(qMax(qAbs(x), qAbs(y)), qAbs(z)) > 2'000'000.0f)
            return false;

        MeshVertex vertex;
        vertex.position = QVector3D(x, y, z);

        if (vertexStride >= 24) {
            const float nx = qFromLittleEndian<float>(reinterpret_cast<const uchar *>(blockBytes.constData() + base + 12));
            const float ny = qFromLittleEndian<float>(reinterpret_cast<const uchar *>(blockBytes.constData() + base + 16));
            const float nz = qFromLittleEndian<float>(reinterpret_cast<const uchar *>(blockBytes.constData() + base + 20));
            if (qIsFinite(nx) && qIsFinite(ny) && qIsFinite(nz))
                vertex.normal = QVector3D(nx, ny, nz);
        }

        outVertices->append(vertex);
    }

    return true;
}

bool decodeRawWindowIndices(const QByteArray &blockBytes,
                            int offset,
                            int indexCount,
                            int indexSize,
                            int maxVertexIndexExclusive,
                            QVector<uint32_t> *outIndices)
{
    if (!outIndices || offset < 0 || indexCount <= 0 || maxVertexIndexExclusive <= 0)
        return false;
    if (indexSize != 2 && indexSize != 4)
        return false;
    const qint64 requiredBytes = static_cast<qint64>(indexCount) * indexSize;
    if (offset + requiredBytes > blockBytes.size())
        return false;

    outIndices->clear();
    outIndices->reserve(indexCount);
    for (int i = 0; i < indexCount; ++i) {
        const int base = offset + (i * indexSize);
        uint32_t index = 0;
        if (indexSize == 2) {
            index = qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(blockBytes.constData() + base));
        } else {
            index = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(blockBytes.constData() + base));
        }
        if (index >= static_cast<uint32_t>(maxVertexIndexExclusive))
            return false;
        outIndices->append(index);
    }

    return true;
}

std::optional<MeshData> decodeExactFitWindowedMesh(const QByteArray &blockBytes,
                                                   const VMeshRefRecord &ref,
                                                   int vertexStride,
                                                   int indexSize)
{
    constexpr int kHeaderSize = 16;

    if (ref.vertexStart < 0 || ref.vertexCount <= 0 || ref.indexStart < 0 || ref.indexCount <= 0)
        return std::nullopt;

    const int totalVertexCount = ref.vertexStart + ref.vertexCount;
    if (totalVertexCount <= 0)
        return std::nullopt;

    QVector<MeshVertex> decodedVertices;
    if (!decodeRawWindowVertices(blockBytes, kHeaderSize, totalVertexCount, vertexStride, &decodedVertices))
        return std::nullopt;

    QVector<uint32_t> decodedIndices;
    const int indexOffset = kHeaderSize + (totalVertexCount * vertexStride) + (ref.indexStart * indexSize);
    if (!decodeRawWindowIndices(blockBytes,
                                indexOffset,
                                ref.indexCount,
                                indexSize,
                                totalVertexCount,
                                &decodedIndices)) {
        return std::nullopt;
    }

    MeshData mesh;
    for (int i = ref.vertexStart; i < ref.vertexStart + ref.vertexCount; ++i)
        mesh.vertices.append(decodedVertices.at(i));

    for (uint32_t index : std::as_const(decodedIndices)) {
        if (index < static_cast<uint32_t>(ref.vertexStart))
            continue;
        const uint32_t rebased = index - static_cast<uint32_t>(ref.vertexStart);
        if (rebased >= static_cast<uint32_t>(mesh.vertices.size()))
            continue;
        mesh.indices.append(rebased);
    }

    if (!sanitizeMesh(mesh))
        return std::nullopt;
    return mesh;
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

QVector<MeshData> CmpLoader::buildMeshesFromVMesh(const QByteArray &vmeshData,
                                                  const VMeshRefRecord &ref,
                                                  const PreviewMaterialBinding *binding,
                                                  const QString &directPlanHint,
                                                  QString *resolvedPlanHint)
{
    QVector<MeshData> meshes;
    if (ref.vertexStart < 0 || ref.vertexCount <= 0 || ref.indexStart < 0 || ref.indexCount <= 0) {
        if (resolvedPlanHint)
            *resolvedPlanHint = directPlanHint + QStringLiteral("/invalid-ref-range");
        return meshes;
    }

    const auto applyPlanHint = [&](MeshData &candidate, const QString &planHint) {
        if (!planHint.isEmpty())
            candidate.debugHint = planHint;
    };

    const auto applyBinding = [&](MeshData &candidate, int materialId) {
        candidate.materialId = materialId;
        if (binding) {
            candidate.textureName = binding->textureValue;
            candidate.textureCandidates = binding->textureCandidates;
            candidate.materialValue = binding->materialValue;
            candidate.matchHint = binding->matchHint;
            if (!binding->textureValue.isEmpty())
                candidate.materialName = binding->textureValue;
            else if (!binding->materialValue.isEmpty())
                candidate.materialName = binding->materialValue;
        }
        if (candidate.materialName.isEmpty() && materialId >= 0)
            candidate.materialName = QStringLiteral("material_%1").arg(materialId);
    };

    const auto sliceDecodedRange = [&](const DecodedMesh &decoded,
                                       int startVertex,
                                       int numVertices,
                                       int startIndex,
                                       int numIndices,
                                       int materialId) -> MeshData {
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
        applyBinding(candidate, materialId);
        return candidate;
    };

    const auto appendFromDecoded = [&](const DecodedMesh &decoded) -> bool {
        const auto headers = parseStructuredMeshHeaders(vmeshData);
        if (!headers.isEmpty()
            && ref.groupStart >= 0
            && ref.groupCount > 0
            && ref.groupStart + ref.groupCount <= headers.size()) {
            for (int i = ref.groupStart; i < ref.groupStart + ref.groupCount; ++i) {
                const auto &header = headers.at(i);
                MeshData mesh = sliceDecodedRange(decoded,
                                                  header.startVertex,
                                                  (header.endVertex - header.startVertex) + 1,
                                                  header.startIndex,
                                                  header.numRefIndices,
                                                  header.materialId);
                if (!mesh.vertices.isEmpty())
                    meshes.append(mesh);
            }
            if (!meshes.isEmpty())
                return true;
        }

        MeshData mesh = sliceDecodedRange(decoded,
                                          ref.vertexStart,
                                          ref.vertexCount,
                                          ref.indexStart,
                                          ref.indexCount,
                                          -1);
        if (!mesh.vertices.isEmpty()) {
            meshes.append(mesh);
            return true;
        }
        return false;
    };

    {
        std::optional<MeshData> bestMesh;
        float bestScore = std::numeric_limits<float>::infinity();
        for (int vertexStride : candidateVertexStridesForRef(ref)) {
            for (int indexSize : {2, 4}) {
                auto candidate = decodeExactFitWindowedMesh(vmeshData, ref, vertexStride, indexSize);
                if (!candidate.has_value())
                    continue;
                const float score = meshBoundsScore(candidate.value(), ref.bounds);
                if (!bestMesh.has_value() || score < bestScore) {
                    bestScore = score;
                    bestMesh = std::move(candidate);
                }
            }
        }

        if (bestMesh.has_value()) {
            applyBinding(bestMesh.value(), -1);
            const QString planHint = directPlanHint + QStringLiteral("/exact-fit-windowed");
            applyPlanHint(bestMesh.value(), planHint);
            if (resolvedPlanHint)
                *resolvedPlanHint = planHint;
            meshes.append(bestMesh.value());
            return meshes;
        }
    }

    if (appendFromDecoded(VmeshDecoder::decode(vmeshData))) {
        const QString planHint = directPlanHint + QStringLiteral("/decoded");
        for (auto &mesh : meshes)
            applyPlanHint(mesh, planHint);
        if (resolvedPlanHint)
            *resolvedPlanHint = planHint;
        return meshes;
    }

    const auto offsets = VmeshDecoder::findStructuredSingleBlockOffsets(vmeshData);
    for (int offset : offsets) {
        if (appendFromDecoded(VmeshDecoder::decodeStructuredSingleBlock(vmeshData, offset))) {
            const QString planHint = directPlanHint + QStringLiteral("/structured-single-block@%1").arg(offset);
            for (auto &mesh : meshes)
                applyPlanHint(mesh, planHint);
            if (resolvedPlanHint)
                *resolvedPlanHint = planHint;
            return meshes;
        }
    }

    if (resolvedPlanHint && resolvedPlanHint->isEmpty())
        *resolvedPlanHint = directPlanHint + QStringLiteral("/no-mesh");

    return meshes;
}

ModelNode CmpLoader::extractPart(const NativeModelPart &part,
                                 const QString &partPath,
                                 const QByteArray &raw,
                                 const QVector<UtfNodeRecord> &nodes,
                                 const QVector<VMeshDataBlock> &vmeshBlocks,
                                 QVector<VMeshRefRecord> &vmeshRefs,
                                 const QVector<CmpTransformHint> &cmpTransformHints,
                                 const QVector<PreviewMaterialBinding> &previewMaterialBindings,
                                 QStringList *warnings)
{
    ModelNode node;
    const auto it = std::find_if(nodes.cbegin(), nodes.cend(), [&](const UtfNodeRecord &record) {
        return record.path.compare(partPath, Qt::CaseInsensitive) == 0;
    });
    if (it == nodes.cend())
        return node;

    node.name = it->name;
    if (const auto *transform = findCombinedTransformHint(cmpTransformHints, node.name)) {
        if (transform->hasLocalTranslation)
            node.origin = transform->localTranslation;
        if (transform->hasLocalRotation)
            node.rotation = transform->localRotation;
    }
    const QString prefix = partPath + QLatin1Char('/');
    for (auto &ref : vmeshRefs) {
        if (!refBelongsToPart(ref, part, partPath))
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
            resolved = resolveVMeshDataBlockForRef(ref.meshDataReference, ref.nodePath, vmeshBlocks);
        if (!resolved.has_value()) {
            if (warnings)
                warnings->append(QStringLiteral("Unresolved VMeshRef in %1").arg(ref.nodePath));
            continue;
        }

        const QByteArray blockBytes = raw.mid(resolved->second.dataOffset, resolved->second.usedSize);
        if (blockBytes.isEmpty())
            continue;
        const PreviewMaterialBinding *binding = findPreviewMaterialBinding(previewMaterialBindings, ref, node.name);
        const QString directPlanHint = QStringLiteral("direct:%1:%2")
                                           .arg(resolved->second.headerHint.structureKind,
                                                resolved->second.sourceName);
        QString resolvedPlanHint;
        const auto resolvedMeshes = buildMeshesFromVMesh(blockBytes, ref, binding, directPlanHint, &resolvedPlanHint);
        if (!resolvedPlanHint.isEmpty() && ref.debugHint.isEmpty())
            ref.debugHint = resolvedPlanHint;
        if (!resolvedMeshes.isEmpty()) {
            if (!resolvedPlanHint.isEmpty())
                ref.debugHint = resolvedPlanHint;
            for (const auto &mesh : resolvedMeshes)
                node.meshes.append(mesh);
            continue;
        }

        QString familyPlanDescription;
        const auto familyMesh = decodeStructuredFamilyForRef(raw,
                                                             vmeshBlocks,
                                                             resolved->second,
                                                             ref,
                                                             candidateSourceNamesForRef(ref, {part}),
                                                             &familyPlanDescription);
        if (familyMesh.has_value() && !familyMesh->vertices.isEmpty()) {
            MeshData mesh = *familyMesh;
            mesh.debugHint = familyPlanDescription;
            ref.debugHint = familyPlanDescription;
            ref.usedStructuredFamilyFallback = true;
            if (binding) {
                mesh.textureName = binding->textureValue;
                mesh.textureCandidates = binding->textureCandidates;
                mesh.materialValue = binding->materialValue;
                mesh.matchHint = binding->matchHint;
                if (!binding->textureValue.isEmpty())
                    mesh.materialName = binding->textureValue;
                else if (!binding->materialValue.isEmpty())
                    mesh.materialName = binding->materialValue;
            }
            node.meshes.append(mesh);
            if (warnings)
                warnings->append(QStringLiteral("Used structured-family fallback for %1 [%2]")
                                     .arg(ref.nodePath, familyPlanDescription));
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
    result.vmeshRefs = parseVMeshRefs(flatNodes, raw, result.vmeshDataBlocks, result.parts);
    result.cmpFixRecords = parseCmpFixRecords(flatNodes, raw);
    result.cmpTransformHints = buildCmpTransformHints(result.cmpFixRecords, result.parts, flatNodes, raw);
    result.materialReferences = extractMaterialReferences(flatNodes, raw);
    result.previewMaterialBindings = buildPreviewMaterialBindings(result.vmeshRefs,
                                                                  result.parts,
                                                                  result.utfNodes,
                                                                  result.materialReferences);

    ModelNode root;
    root.name = QFileInfo(filePath).fileName();

    if (result.format == NativeModelFormat::Cmp && !result.parts.isEmpty()) {
        QHash<QString, ModelNode> partNodesByName;
        QHash<QString, QString> parentByPart;
        for (const auto &part : std::as_const(result.parts))
            parentByPart.insert(part.name.toLower(), part.parentPartName);
        for (const auto &hint : std::as_const(result.cmpTransformHints)) {
            if (!hint.parentPartName.trimmed().isEmpty())
                parentByPart.insert(hint.partName.toLower(), hint.parentPartName);
        }
        QSet<QString> parentPartNames;
        for (auto it = parentByPart.cbegin(); it != parentByPart.cend(); ++it) {
            const QString lowered = it.value().trimmed().toLower();
            if (!lowered.isEmpty())
                parentPartNames.insert(lowered);
        }

        for (const auto &part : std::as_const(result.parts)) {
            const QString partPath = firstPathForName(result.utfNodes, part.name);
            if (partPath.isEmpty())
                continue;
            ModelNode partNode = extractPart(part, partPath, raw, result.utfNodes, result.vmeshDataBlocks,
                                             result.vmeshRefs, result.cmpTransformHints,
                                             result.previewMaterialBindings, &result.warnings);
            if (!partNode.meshes.isEmpty()
                || shouldKeepEmptyCmpPartNode(part.name, parentPartNames, result.cmpTransformHints)) {
                partNodesByName.insert(part.name.toLower(), partNode);
            }
        }

        QSet<QString> attached;
        std::function<ModelNode(QString)> buildHierarchy = [&](QString partKey) -> ModelNode {
            attached.insert(partKey);
            ModelNode node = partNodesByName.take(partKey);
            for (const QString &candidateKey : parentByPart.keys()) {
                if (attached.contains(candidateKey))
                    continue;
                if (parentByPart.value(candidateKey).compare(node.name, Qt::CaseInsensitive) == 0 &&
                    partNodesByName.contains(candidateKey)) {
                    node.children.append(buildHierarchy(candidateKey));
                }
            }
            return node;
        };

        QStringList orderedPartKeys;
        orderedPartKeys.reserve(result.parts.size());
        for (const auto &part : std::as_const(result.parts))
            orderedPartKeys.append(part.name.toLower());
        for (const QString &partKey : std::as_const(orderedPartKeys)) {
            if (!partNodesByName.contains(partKey) || attached.contains(partKey))
                continue;
            const QString parentName = parentByPart.value(partKey);
            if (!parentName.isEmpty() && partNodesByName.contains(parentName.toLower()))
                continue;
            root.children.append(buildHierarchy(partKey));
        }
    }

    if (root.children.isEmpty()) {
        QString partPath = firstPathForName(result.utfNodes, QFileInfo(filePath).baseName());
        if (partPath.isEmpty())
            partPath = firstPathForName(result.utfNodes, QStringLiteral("Root"));
        NativeModelPart directPart;
        directPart.name = QFileInfo(filePath).baseName();
        ModelNode direct = extractPart(directPart, partPath, raw, result.utfNodes, result.vmeshDataBlocks,
                                       result.vmeshRefs, result.cmpTransformHints,
                                       result.previewMaterialBindings, &result.warnings);
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
