#include "FreelancerMaterialResolver.h"

#include "infrastructure/io/CmpLoader.h"
#include "infrastructure/io/TextureLoader.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>

namespace flatlas::infrastructure {

QMutex FreelancerMaterialResolver::s_cacheMutex;
QHash<QString, QHash<QString, QStringList>> FreelancerMaterialResolver::s_materialTextureMapCache;
QHash<QString, QHash<QString, QImage>> FreelancerMaterialResolver::s_embeddedTextureCache;

namespace {

QString normalizeMaterialKey(QString value)
{
    value = value.trimmed().toLower();
    value.replace(QLatin1Char('\\'), QLatin1Char('/'));
    value = QFileInfo(value).fileName();
    const int dot = value.lastIndexOf(QLatin1Char('.'));
    if (dot > 0)
        value = value.left(dot);
    return value;
}

void walkUtfNode(const std::shared_ptr<UtfNode> &node,
                 const QString &path,
                 QHash<QString, QStringList> *materialMap)
{
    if (!node || !materialMap)
        return;

    const QString currentPath = path.isEmpty() ? node->name : path + QLatin1Char('/') + node->name;
    const QString loweredPath = currentPath.toLower();
    if (loweredPath.contains(QStringLiteral("material library/"))
        && (loweredPath.endsWith(QStringLiteral("/dt_name")) || loweredPath.endsWith(QStringLiteral("/et_name")))) {
        const QStringList parts = currentPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        if (parts.size() >= 3 && !node->data.isEmpty()) {
            const QString materialName = normalizeMaterialKey(parts.at(parts.size() - 2));
            const QString value = QString::fromLatin1(node->data.split('\0').value(0)).trimmed();
            if (!materialName.isEmpty() && !value.isEmpty())
                (*materialMap)[materialName].append(value);
        }
    }

    for (const auto &child : node->children)
        walkUtfNode(child, currentPath, materialMap);
}

QStringList textureKeysForName(QString value)
{
    QStringList keys;
    value = value.trimmed().replace(QLatin1Char('\\'), QLatin1Char('/'));
    value = QFileInfo(value).fileName();
    if (value.isEmpty())
        return keys;
    const QString lowered = value.toLower();
    keys.append(lowered);
    const QString stem = QFileInfo(value).completeBaseName().toLower();
    if (!stem.isEmpty() && !keys.contains(stem))
        keys.append(stem);
    if (!lowered.contains(QLatin1Char('.'))) {
        const QString dds = lowered + QStringLiteral(".dds");
        const QString tga = lowered + QStringLiteral(".tga");
        if (!keys.contains(dds))
            keys.append(dds);
        if (!keys.contains(tga))
            keys.append(tga);
    }
    return keys;
}

QImage decodeEmbeddedTextureBlob(const QByteArray &blob)
{
    if (blob.size() >= 4 && blob.left(4) == QByteArrayLiteral("DDS "))
        return TextureLoader::loadDDSFromData(blob);
    const QImage tga = TextureLoader::loadTGAFromData(blob);
    if (!tga.isNull())
        return tga;
    return TextureLoader::loadDDSFromData(blob);
}

void walkUtfNodeForEmbeddedTextures(const std::shared_ptr<UtfNode> &node,
                                    const QString &path,
                                    QHash<QString, QPair<int, QByteArray>> *bestEntries)
{
    if (!node || !bestEntries)
        return;

    const QString currentPath = path.isEmpty() ? node->name : path + QLatin1Char('/') + node->name;
    const QString loweredPath = currentPath.toLower();
    if (loweredPath.contains(QStringLiteral("texture library/")) && node->children.isEmpty() && !node->data.isEmpty()) {
        const QStringList parts = currentPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        if (parts.size() >= 3) {
            const QString textureName = parts.at(parts.size() - 2).trimmed();
            const QString mipName = parts.last().trimmed().toLower();
            if (!textureName.isEmpty() && mipName.startsWith(QStringLiteral("mip"))) {
                bool ok = false;
                const int mipLevel = mipName.mid(3).toInt(&ok);
                if (ok) {
                    const QString key = textureName.toLower();
                    const auto existing = bestEntries->value(key);
                    if (!bestEntries->contains(key) || mipLevel < existing.first)
                        bestEntries->insert(key, qMakePair(mipLevel, node->data));
                }
            }
        }
    }

    for (const auto &child : node->children)
        walkUtfNodeForEmbeddedTextures(child, currentPath, bestEntries);
}

} // namespace

QString FreelancerMaterialResolver::findDataRoot(const QString &path)
{
    QDir dir(path);
    while (dir.exists()) {
        if (dir.dirName().compare(QStringLiteral("DATA"), Qt::CaseInsensitive) == 0)
            return dir.absolutePath();
        if (!dir.cdUp())
            break;
    }
    return {};
}

QHash<QString, QStringList> FreelancerMaterialResolver::extractUtfMaterialTextureMap(const QString &utfPath)
{
    const QString cacheKey = QFileInfo(utfPath).absoluteFilePath().toLower();
    {
        QMutexLocker locker(&s_cacheMutex);
        const auto it = s_materialTextureMapCache.constFind(cacheKey);
        if (it != s_materialTextureMapCache.cend())
            return *it;
    }

    QHash<QString, QStringList> materialMap;
    QFile file(utfPath);
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray raw = file.readAll();
        const auto root = CmpLoader::parseUtf(raw);
        if (root) {
            for (const auto &child : root->children)
                walkUtfNode(child, QString(), &materialMap);
        }
    }

    for (auto it = materialMap.begin(); it != materialMap.end(); ++it) {
        QStringList unique;
        for (const QString &value : std::as_const(it.value())) {
            if (!unique.contains(value, Qt::CaseInsensitive))
                unique.append(value);
        }
        it.value() = unique;
    }

    QMutexLocker locker(&s_cacheMutex);
    s_materialTextureMapCache.insert(cacheKey, materialMap);
    return materialMap;
}

QHash<QString, QImage> FreelancerMaterialResolver::extractUtfEmbeddedTextures(const QString &utfPath)
{
    const QString cacheKey = QFileInfo(utfPath).absoluteFilePath().toLower();
    {
        QMutexLocker locker(&s_cacheMutex);
        const auto it = s_embeddedTextureCache.constFind(cacheKey);
        if (it != s_embeddedTextureCache.cend())
            return *it;
    }

    QHash<QString, QImage> textures;
    QFile file(utfPath);
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray raw = file.readAll();
        const auto root = CmpLoader::parseUtf(raw);
        if (root) {
            QHash<QString, QPair<int, QByteArray>> bestEntries;
            for (const auto &child : root->children)
                walkUtfNodeForEmbeddedTextures(child, QString(), &bestEntries);

            for (auto it = bestEntries.cbegin(); it != bestEntries.cend(); ++it) {
                const QImage image = decodeEmbeddedTextureBlob(it.value().second);
                if (image.isNull())
                    continue;
                for (const QString &key : textureKeysForName(it.key()))
                    textures.insert(key, image);
            }
        }
    }

    QMutexLocker locker(&s_cacheMutex);
    s_embeddedTextureCache.insert(cacheKey, textures);
    return textures;
}

QString FreelancerMaterialResolver::resolveTextureValue(const QString &sourcePath, const QString &value)
{
    const QString normalized = value.trimmed().replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (normalized.isEmpty())
        return {};

    QFileInfo candidateInfo(normalized);
    if (candidateInfo.isAbsolute() && candidateInfo.exists())
        return candidateInfo.absoluteFilePath();

    const QFileInfo sourceInfo(sourcePath);
    const QString sourceDir = sourceInfo.absolutePath();
    const QString baseName = QFileInfo(normalized).fileName();

    const QStringList directCandidates{
        QDir(sourceDir).filePath(normalized),
        QDir(sourceDir).filePath(baseName),
    };
    for (const QString &candidate : directCandidates) {
        if (QFileInfo::exists(candidate))
            return QFileInfo(candidate).absoluteFilePath();
    }

    const QString dataRoot = findDataRoot(sourceDir);
    if (dataRoot.isEmpty())
        return {};

    QStringList rootedCandidates;
    if (normalized.startsWith(QStringLiteral("DATA/"), Qt::CaseInsensitive))
        rootedCandidates.append(QDir(dataRoot).filePath(normalized.section(QLatin1Char('/'), 1)));
    rootedCandidates.append(QDir(dataRoot).filePath(normalized));
    rootedCandidates.append(QDir(dataRoot).filePath(baseName));
    for (const QString &candidate : std::as_const(rootedCandidates)) {
        if (QFileInfo::exists(candidate))
            return QFileInfo(candidate).absoluteFilePath();
    }

    QDirIterator it(dataRoot, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (QFileInfo(path).fileName().compare(baseName, Qt::CaseInsensitive) == 0)
            return path;
    }

    return {};
}

QStringList FreelancerMaterialResolver::textureCandidatesForMesh(const QString &modelPath, const MeshData &mesh)
{
    QStringList candidates;
    if (!mesh.textureName.isEmpty())
        candidates.append(mesh.textureName);
    for (const QString &candidate : mesh.textureCandidates) {
        if (!candidates.contains(candidate, Qt::CaseInsensitive))
            candidates.append(candidate);
    }

    const auto modelMap = extractUtfMaterialTextureMap(modelPath);
    const QString materialKey = normalizeMaterialKey(!mesh.materialName.isEmpty() ? mesh.materialName : mesh.materialValue);
    if (!materialKey.isEmpty() && modelMap.contains(materialKey)) {
        for (const QString &candidate : modelMap.value(materialKey)) {
            if (!candidates.contains(candidate, Qt::CaseInsensitive))
                candidates.append(candidate);
        }
    }

    if (!mesh.materialValue.isEmpty() && mesh.materialValue.endsWith(QStringLiteral(".mat"), Qt::CaseInsensitive)) {
        const QString matPath = resolveTextureValue(modelPath, mesh.materialValue);
        if (!matPath.isEmpty()) {
            const auto matMap = extractUtfMaterialTextureMap(matPath);
            if (!materialKey.isEmpty() && matMap.contains(materialKey)) {
                for (const QString &candidate : matMap.value(materialKey)) {
                    if (!candidates.contains(candidate, Qt::CaseInsensitive))
                        candidates.append(candidate);
                }
            } else if (matMap.size() == 1) {
                const auto it = matMap.cbegin();
                for (const QString &candidate : it.value()) {
                    if (!candidates.contains(candidate, Qt::CaseInsensitive))
                        candidates.append(candidate);
                }
            }
        }
    }

    return candidates;
}

QString FreelancerMaterialResolver::resolveTexturePathForMesh(const QString &modelPath, const MeshData &mesh)
{
    const QStringList candidates = textureCandidatesForMesh(modelPath, mesh);
    for (const QString &candidate : candidates) {
        const QString path = resolveTextureValue(modelPath, candidate);
        if (!path.isEmpty())
            return path;
    }
    return {};
}

QImage FreelancerMaterialResolver::resolveEmbeddedTextureForMesh(const QString &modelPath, const MeshData &mesh)
{
    const QStringList candidates = textureCandidatesForMesh(modelPath, mesh);
    const auto modelTextures = extractUtfEmbeddedTextures(modelPath);
    for (const QString &candidate : candidates) {
        for (const QString &key : textureKeysForName(candidate)) {
            const auto it = modelTextures.constFind(key);
            if (it != modelTextures.cend() && !it.value().isNull())
                return it.value();
        }
    }

    if (!mesh.materialValue.isEmpty() && mesh.materialValue.endsWith(QStringLiteral(".mat"), Qt::CaseInsensitive)) {
        const QString matPath = resolveTextureValue(modelPath, mesh.materialValue);
        if (!matPath.isEmpty()) {
            const auto matTextures = extractUtfEmbeddedTextures(matPath);
            for (const QString &candidate : candidates) {
                for (const QString &key : textureKeysForName(candidate)) {
                    const auto it = matTextures.constFind(key);
                    if (it != matTextures.cend() && !it.value().isNull())
                        return it.value();
                }
            }
        }
    }

    return {};
}

QImage FreelancerMaterialResolver::loadTextureForMesh(const QString &modelPath, const MeshData &mesh)
{
    const QImage embedded = resolveEmbeddedTextureForMesh(modelPath, mesh);
    if (!embedded.isNull())
        return embedded;

    const QString path = resolveTexturePathForMesh(modelPath, mesh);
    if (path.isEmpty())
        return {};
    return TextureLoader::load(path);
}

} // namespace flatlas::infrastructure
