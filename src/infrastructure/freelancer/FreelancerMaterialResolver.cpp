#include "FreelancerMaterialResolver.h"

#include "infrastructure/io/CmpLoader.h"
#include "infrastructure/io/TextureLoader.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <algorithm>
#include <limits>

namespace flatlas::infrastructure {

QMutex FreelancerMaterialResolver::s_cacheMutex;
QHash<QString, QHash<QString, QStringList>> FreelancerMaterialResolver::s_materialTextureMapCache;
QHash<QString, QHash<QString, QImage>> FreelancerMaterialResolver::s_embeddedTextureCache;
QHash<QString, QHash<QString, QString>> FreelancerMaterialResolver::s_dataRootFileScanCache;
QHash<QString, bool> FreelancerMaterialResolver::s_dataRootScannedFlag;

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

QString normalizePlanetTextureKey(QString value)
{
    value = value.trimmed().toLower();
    QString normalized;
    normalized.reserve(value.size());
    for (const QChar ch : value) {
        if (ch.isLetterOrNumber())
            normalized.append(ch);
    }
    return normalized;
}

bool isPlanetCapTextureName(const QString &value)
{
    const QString normalized = normalizePlanetTextureKey(value);
    return normalized.endsWith(QStringLiteral("cap")) && normalized.size() > 3;
}

QStringList planetTextureAliases(QString archetype)
{
    archetype = QFileInfo(archetype.trimmed().toLower()).completeBaseName();
    if (archetype.startsWith(QStringLiteral("planet_")))
        archetype = archetype.mid(7);

    QStringList parts = archetype.split(QLatin1Char('_'), Qt::SkipEmptyParts);
    while (!parts.isEmpty()) {
        bool ok = false;
        parts.last().toDouble(&ok);
        if (!ok)
            break;
        parts.removeLast();
    }

    const QString core = normalizePlanetTextureKey(parts.join(QString()));
    if (core.isEmpty())
        return {};

    QStringList aliases{core};
    const QStringList layerSuffixes{
        QStringLiteral("clouds"),
        QStringLiteral("cloud"),
        QStringLiteral("cld"),
        QStringLiteral("rings"),
        QStringLiteral("ring"),
        QStringLiteral("atmosphere"),
        QStringLiteral("atmos"),
        QStringLiteral("atmo"),
        QStringLiteral("atm"),
    };
    const QStringList surfaceSuffixes{
        QStringLiteral("grck"),
        QStringLiteral("rock"),
        QStringLiteral("rck"),
        QStringLiteral("moon"),
        QStringLiteral("ice"),
        QStringLiteral("lava"),
        QStringLiteral("molten"),
    };

    for (const QString &suffix : layerSuffixes) {
        if (core.endsWith(suffix) && core.size() > suffix.size() + 2)
            aliases.append(core.left(core.size() - suffix.size()));
    }
    for (const QString &suffix : surfaceSuffixes) {
        if (core.endsWith(suffix) && core.size() > suffix.size() + 2)
            aliases.append(core.left(core.size() - suffix.size()));
    }
    if (core.endsWith(QStringLiteral("ed")) && core.size() > 5)
        aliases.append(core.left(core.size() - 2));

    aliases.removeDuplicates();
    std::sort(aliases.begin(), aliases.end(), [](const QString &left, const QString &right) {
        return left.size() > right.size();
    });
    return aliases;
}

int planetTextureScore(const QString &archetype, const QString &name, const QImage &image)
{
    const QString lowered = name.trimmed().toLower();
    const QString normalized = normalizePlanetTextureKey(lowered);
    int score = 0;

    for (const QString &alias : planetTextureAliases(archetype)) {
        if (alias.isEmpty())
            continue;
        if (normalized == alias)
            score = qMax(score, 300 + alias.size());
        else if (normalized.contains(alias))
            score = qMax(score, 200 + alias.size());
        else if (alias.contains(normalized) && normalized.size() >= 5)
            score = qMax(score, 150 + normalized.size());
    }

    const QStringList preferTerms{
        QStringLiteral("planet"),
        QStringLiteral("surface"),
        QStringLiteral("surf"),
        QStringLiteral("diffuse"),
        QStringLiteral("tex"),
    };
    const QStringList excludeTerms{
        QStringLiteral("cloud"),
        QStringLiteral("cld"),
        QStringLiteral("ring"),
        QStringLiteral("atmo"),
        QStringLiteral("atmos"),
        QStringLiteral("glow"),
        QStringLiteral("haze"),
        QStringLiteral("halo"),
        QStringLiteral("shine"),
        QStringLiteral("light"),
    };

    for (const QString &term : preferTerms) {
        if (lowered.contains(term)) {
            score += 45;
            break;
        }
    }
    for (const QString &term : excludeTerms) {
        if (lowered.contains(term)) {
            score -= 120;
            break;
        }
    }
    if (isPlanetCapTextureName(lowered))
        score -= 160;

    if (!image.isNull()) {
        const double ratio = image.height() > 0 ? static_cast<double>(image.width()) / image.height() : 0.0;
        score += qMax(0, 100 - static_cast<int>(qAbs(ratio - 2.0) * 100.0));
        score += qMin(image.width() * image.height() / 4096, 120);
    }

    return score;
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

    // Final fallback: locate a file by its bare name anywhere under dataRoot.
    // Doing this naively with QDirIterator rescans the entire DATA tree for
    // every unresolved candidate. Models such as li_manhattan_bar.cmp feed us
    // many bogus names (authoring timestamps / concatenated strings) which
    // made the app freeze for tens of seconds. We therefore scan each
    // dataRoot at most once and keep a basename->fullpath map in memory.
    const QString normalizedRoot = QDir(dataRoot).absolutePath();
    const QString baseKey = baseName.toLower();
    if (baseKey.isEmpty())
        return {};
    {
        QMutexLocker lock(&s_cacheMutex);
        const bool scanned = s_dataRootScannedFlag.value(normalizedRoot, false);
        if (!scanned) {
            auto &map = s_dataRootFileScanCache[normalizedRoot];
            QDirIterator it(normalizedRoot, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString path = it.next();
                const QString fileKey = QFileInfo(path).fileName().toLower();
                if (!fileKey.isEmpty() && !map.contains(fileKey))
                    map.insert(fileKey, path);
            }
            s_dataRootScannedFlag.insert(normalizedRoot, true);
        }
        const auto &map = s_dataRootFileScanCache.value(normalizedRoot);
        const auto hit = map.constFind(baseKey);
        if (hit != map.cend())
            return hit.value();
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

QImage FreelancerMaterialResolver::loadBestPlanetTexture(const QString &archetype, const QStringList &sourcePaths)
{
    QHash<QString, QImage> candidates;

    for (const QString &rawSourcePath : sourcePaths) {
        const QString sourcePath = rawSourcePath.trimmed();
        if (sourcePath.isEmpty())
            continue;

        const QFileInfo sourceInfo(sourcePath);
        const QString sourceKey = sourceInfo.completeBaseName().toLower();

        const QImage direct = TextureLoader::load(sourcePath);
        if (!direct.isNull())
            candidates.insert(sourceKey, direct);

        const auto embedded = extractUtfEmbeddedTextures(sourcePath);
        for (auto it = embedded.constBegin(); it != embedded.constEnd(); ++it) {
            if (!it.value().isNull())
                candidates.insert(it.key(), it.value());
        }

        const auto materialMap = extractUtfMaterialTextureMap(sourcePath);
        for (auto it = materialMap.constBegin(); it != materialMap.constEnd(); ++it) {
            for (const QString &value : it.value()) {
                const QString texturePath = resolveTextureValue(sourcePath, value);
                if (texturePath.isEmpty())
                    continue;

                QImage image = TextureLoader::load(texturePath);
                if (image.isNull()) {
                    const auto textureEmbedded = extractUtfEmbeddedTextures(texturePath);
                    if (!textureEmbedded.isEmpty())
                        image = textureEmbedded.constBegin().value();
                }
                if (!image.isNull())
                    candidates.insert(!it.key().isEmpty() ? it.key() : QFileInfo(value).completeBaseName().toLower(), image);
            }
        }
    }

    QString bestKey;
    QImage bestImage;
    int bestScore = std::numeric_limits<int>::min();
    QString bestNonCapKey;
    QImage bestNonCapImage;
    int bestNonCapScore = std::numeric_limits<int>::min();
    for (auto it = candidates.constBegin(); it != candidates.constEnd(); ++it) {
        const int score = planetTextureScore(archetype, it.key(), it.value());
        if (score > bestScore) {
            bestScore = score;
            bestKey = it.key();
            bestImage = it.value();
        }
        if (!isPlanetCapTextureName(it.key()) && score > bestNonCapScore) {
            bestNonCapScore = score;
            bestNonCapKey = it.key();
            bestNonCapImage = it.value();
        }
    }
    Q_UNUSED(bestKey);
    Q_UNUSED(bestNonCapKey);
    return !bestNonCapImage.isNull() ? bestNonCapImage : bestImage;
}

} // namespace flatlas::infrastructure
