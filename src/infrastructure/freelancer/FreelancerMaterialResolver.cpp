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

QImage FreelancerMaterialResolver::loadTextureForMesh(const QString &modelPath, const MeshData &mesh)
{
    const QString path = resolveTexturePathForMesh(modelPath, mesh);
    if (path.isEmpty())
        return {};
    return TextureLoader::load(path);
}

} // namespace flatlas::infrastructure
