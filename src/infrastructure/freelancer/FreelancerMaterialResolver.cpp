#include "FreelancerMaterialResolver.h"

#include "infrastructure/io/CmpLoader.h"
#include "infrastructure/io/TextureLoader.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <algorithm>
#include <cmath>
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

bool isMaterialTextureNameNode(const QString &nodeName)
{
    return nodeName.compare(QStringLiteral("Dt_name"), Qt::CaseInsensitive) == 0
           || nodeName.compare(QStringLiteral("Et_name"), Qt::CaseInsensitive) == 0
           || nodeName.compare(QStringLiteral("Bt_name"), Qt::CaseInsensitive) == 0
           || nodeName.compare(QStringLiteral("Dm0_name"), Qt::CaseInsensitive) == 0
           || nodeName.compare(QStringLiteral("Dm1_name"), Qt::CaseInsensitive) == 0
           || nodeName.compare(QStringLiteral("Dm2_name"), Qt::CaseInsensitive) == 0
           || nodeName.compare(QStringLiteral("Dm3_name"), Qt::CaseInsensitive) == 0;
}

int materialTextureNodePriority(const QString &nodeName, const QString &value)
{
    if (nodeName.compare(QStringLiteral("Dt_name"), Qt::CaseInsensitive) == 0)
        return 0;
    if (nodeName.compare(QStringLiteral("Bt_name"), Qt::CaseInsensitive) == 0)
        return 2;
    if (nodeName.startsWith(QStringLiteral("Dm"), Qt::CaseInsensitive)
        && nodeName.endsWith(QStringLiteral("_name"), Qt::CaseInsensitive)) {
        const QString loweredValue = value.toLower();
        if (loweredValue.contains(QStringLiteral("detailmap")))
            return 4;
        return 1;
    }
    return 3;
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
        && isMaterialTextureNameNode(node->name)) {
        const QStringList parts = currentPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        if (parts.size() >= 3 && !node->data.isEmpty()) {
            const QString materialName = normalizeMaterialKey(parts.at(parts.size() - 2));
            const QString value = QString::fromLatin1(node->data.split('\0').value(0)).trimmed();
            if (!materialName.isEmpty() && !value.isEmpty())
                (*materialMap)[materialName].append(QString::number(materialTextureNodePriority(node->name, value))
                                                    + QLatin1Char('\t') + value);
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
    return normalized.contains(QStringLiteral("cap")) && normalized.size() > 3;
}

bool isPlanetLayerTextureName(const QString &value)
{
    const QString normalized = normalizePlanetTextureKey(value);
    return isPlanetCapTextureName(value)
           || normalized.contains(QStringLiteral("atmos"))
           || normalized.contains(QStringLiteral("cloud"))
           || normalized.contains(QStringLiteral("ring"));
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

QString siblingPlanetMaterialPath(const QString &spherePath)
{
    QFileInfo sphereInfo(spherePath);
    if (!sphereInfo.exists() || sphereInfo.suffix().compare(QStringLiteral("sph"), Qt::CaseInsensitive) != 0)
        return {};

    QString stem = sphereInfo.completeBaseName();
    const int underscore = stem.lastIndexOf(QLatin1Char('_'));
    if (underscore > 0 && underscore + 1 < stem.size()) {
        bool numericSuffix = true;
        for (int i = underscore + 1; i < stem.size(); ++i) {
            if (!stem.at(i).isDigit()) {
                numericSuffix = false;
                break;
            }
        }
        if (numericSuffix)
            stem = stem.left(underscore);
    }

    const QString matPath = sphereInfo.dir().filePath(stem + QStringLiteral(".mat"));
    if (QFileInfo::exists(matPath))
        return QFileInfo(matPath).absoluteFilePath();
    return {};
}

void appendUniqueCaseInsensitive(QStringList *values, const QString &value)
{
    if (!values || value.trimmed().isEmpty())
        return;
    if (!values->contains(value, Qt::CaseInsensitive))
        values->append(value);
}

void walkSphereMaterialNames(const std::shared_ptr<UtfNode> &node,
                             const QString &path,
                             QStringList *materialNames)
{
    if (!node || !materialNames)
        return;

    const QString currentPath = path.isEmpty() ? node->name : path + QLatin1Char('/') + node->name;
    const QString loweredPath = currentPath.toLower();
    const bool underSphere = loweredPath.contains(QStringLiteral("/sphere/"))
                             || loweredPath.startsWith(QStringLiteral("sphere/"));
    if (underSphere) {
        const QString loweredName = node->name.toLower();
        if (loweredName != QStringLiteral("radius") && loweredName != QStringLiteral("sides"))
            appendUniqueCaseInsensitive(materialNames, node->name);
    }

    for (const auto &child : node->children)
        walkSphereMaterialNames(child, currentPath, materialNames);
}

QStringList sphereMaterialNames(const QString &spherePath)
{
    QFile file(spherePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QStringList names;
    const auto root = CmpLoader::parseUtf(file.readAll());
    if (root) {
        for (const auto &child : root->children)
            walkSphereMaterialNames(child, QString(), &names);
    }

    std::stable_sort(names.begin(), names.end(), [](const QString &left, const QString &right) {
        const bool leftLayer = isPlanetLayerTextureName(left);
        const bool rightLayer = isPlanetLayerTextureName(right);
        if (leftLayer != rightLayer)
            return !leftLayer;
        const bool leftSide = left.contains(QStringLiteral("side"), Qt::CaseInsensitive);
        const bool rightSide = right.contains(QStringLiteral("side"), Qt::CaseInsensitive);
        if (leftSide != rightSide)
            return leftSide;
        return left < right;
    });
    return names;
}

int planetSideIndex(const QString &materialName)
{
    const QString lowered = materialName.toLower();
    if (lowered.contains(QStringLiteral("side1")))
        return 1;
    if (lowered.contains(QStringLiteral("side2")))
        return 2;
    return 0;
}

QRgb sampleImageBilinear(const QImage &image, double u, double v)
{
    if (image.isNull())
        return qRgba(0, 0, 0, 0);

    const double x = qBound(0.0, u, 1.0) * static_cast<double>(image.width() - 1);
    const double y = qBound(0.0, v, 1.0) * static_cast<double>(image.height() - 1);
    const int x0 = qBound(0, static_cast<int>(std::floor(x)), image.width() - 1);
    const int y0 = qBound(0, static_cast<int>(std::floor(y)), image.height() - 1);
    const int x1 = qBound(0, x0 + 1, image.width() - 1);
    const int y1 = qBound(0, y0 + 1, image.height() - 1);
    const double tx = x - static_cast<double>(x0);
    const double ty = y - static_cast<double>(y0);

    const QRgb c00 = image.pixel(x0, y0);
    const QRgb c10 = image.pixel(x1, y0);
    const QRgb c01 = image.pixel(x0, y1);
    const QRgb c11 = image.pixel(x1, y1);
    auto lerp = [](double a, double b, double t) {
        return a + (b - a) * t;
    };
    auto channel = [&](int c00Value, int c10Value, int c01Value, int c11Value) {
        const double top = lerp(c00Value, c10Value, tx);
        const double bottom = lerp(c01Value, c11Value, tx);
        return qBound(0, static_cast<int>(std::round(lerp(top, bottom, ty))), 255);
    };

    return qRgba(channel(qRed(c00), qRed(c10), qRed(c01), qRed(c11)),
                 channel(qGreen(c00), qGreen(c10), qGreen(c01), qGreen(c11)),
                 channel(qBlue(c00), qBlue(c10), qBlue(c01), qBlue(c11)),
                 channel(qAlpha(c00), qAlpha(c10), qAlpha(c01), qAlpha(c11)));
}

QImage projectPlanetSideTextures(const QImage &side1, const QImage &side2)
{
    if (side1.isNull() || side2.isNull())
        return {};

    const QImage source1 = side1.convertToFormat(QImage::Format_ARGB32);
    const QImage source2 = side2.convertToFormat(QImage::Format_ARGB32);
    const int height = qMin(1024, qMax(qMax(source1.width(), source1.height()), qMax(source2.width(), source2.height())));
    if (height <= 0)
        return {};

    QImage projected(height * 2, height, QImage::Format_ARGB32);
    for (int y = 0; y < projected.height(); ++y) {
        auto *row = reinterpret_cast<QRgb *>(projected.scanLine(y));
        const double latitude = M_PI_2 - ((static_cast<double>(y) + 0.5) / projected.height()) * M_PI;
        const double cosLatitude = std::cos(latitude);
        const double vertical = 0.5 - std::sin(latitude) * 0.5;
        for (int x = 0; x < projected.width(); ++x) {
            double longitude = ((static_cast<double>(x) + 0.5) / projected.width()) * M_PI * 2.0 - M_PI;

            const bool useSide1 = std::cos(longitude) >= 0.0;
            const double sideLongitude = useSide1
                                             ? longitude
                                             : (longitude >= 0.0 ? longitude - M_PI : longitude + M_PI);
            const double horizontal = 0.5 + std::sin(sideLongitude) * cosLatitude * 0.5;
            row[x] = sampleImageBilinear(useSide1 ? source1 : source2, horizontal, vertical);
        }
    }
    return projected;
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
                const int mipLevel = mipName == QStringLiteral("mips") ? 0 : mipName.mid(3).toInt(&ok);
                if (mipName == QStringLiteral("mips"))
                    ok = true;
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
        QStringList ordered = it.value();
        std::stable_sort(ordered.begin(), ordered.end(), [](const QString &left, const QString &right) {
            return left.section(QLatin1Char('\t'), 0, 0).toInt() < right.section(QLatin1Char('\t'), 0, 0).toInt();
        });

        QStringList unique;
        for (const QString &encodedValue : std::as_const(ordered)) {
            const QString value = encodedValue.section(QLatin1Char('\t'), 1).trimmed();
            if (!value.isEmpty() && !unique.contains(value, Qt::CaseInsensitive))
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

PlanetSurfaceTextureSet FreelancerMaterialResolver::loadPlanetSurfaceTextures(const QStringList &sourcePaths)
{
    QString spherePath;
    for (const QString &sourcePath : sourcePaths) {
        QFileInfo info(sourcePath);
        if (info.suffix().compare(QStringLiteral("sph"), Qt::CaseInsensitive) == 0 && info.exists()) {
            spherePath = info.absoluteFilePath();
            break;
        }
    }
    if (spherePath.isEmpty())
        return {};

    const QString matPath = siblingPlanetMaterialPath(spherePath);
    if (matPath.isEmpty())
        return {};

    QStringList materialNames = sphereMaterialNames(spherePath);
    if (materialNames.isEmpty())
        return {};

    const auto materialMap = extractUtfMaterialTextureMap(matPath);
    for (auto it = materialMap.constBegin(); it != materialMap.constEnd(); ++it) {
        if (planetSideIndex(it.key()) > 0)
            appendUniqueCaseInsensitive(&materialNames, it.key());
    }

    QStringList resolverSources{matPath};
    for (const QString &sourcePath : sourcePaths)
        appendUniqueCaseInsensitive(&resolverSources, sourcePath);

    auto resolveEmbedded = [](const QStringList &paths, const QStringList &textureValues) -> QImage {
        for (const QString &path : paths) {
            const auto embedded = extractUtfEmbeddedTextures(path);
            for (const QString &value : textureValues) {
                for (const QString &key : textureKeysForName(value)) {
                    const auto it = embedded.constFind(key);
                    if (it != embedded.cend() && !it.value().isNull())
                        return it.value();
                }
            }
        }
        return {};
    };

    auto resolveTextureImage = [&](const QStringList &textureValues) -> QImage {
        QImage image = resolveEmbedded(resolverSources, textureValues);
        if (!image.isNull())
            return image;

        for (const QString &sourcePath : std::as_const(resolverSources)) {
            for (const QString &value : textureValues) {
                const QString texturePath = resolveTextureValue(sourcePath, value);
                if (texturePath.isEmpty())
                    continue;

                image = TextureLoader::load(texturePath);
                if (!image.isNull())
                    return image;

                image = resolveEmbedded(QStringList{texturePath}, textureValues);
                if (!image.isNull())
                    return image;
            }
        }
        return {};
    };

    QHash<int, QImage> sideImages;
    QImage capImage;
    QImage firstSurfaceImage;
    for (const QString &materialName : materialNames) {
        if (!isPlanetCapTextureName(materialName) && isPlanetLayerTextureName(materialName))
            continue;

        QStringList textureValues = materialMap.value(normalizeMaterialKey(materialName));
        textureValues.erase(std::remove_if(textureValues.begin(), textureValues.end(), [](const QString &value) {
                                return !isPlanetCapTextureName(value) && isPlanetLayerTextureName(value);
                            }),
                            textureValues.end());
        if (textureValues.isEmpty())
            continue;

        const QImage image = resolveTextureImage(textureValues);
        if (image.isNull())
            continue;

        const int sideIndex = planetSideIndex(materialName);
        if (sideIndex > 0 && !sideImages.contains(sideIndex))
            sideImages.insert(sideIndex, image);
        else if (isPlanetCapTextureName(materialName) && capImage.isNull())
            capImage = image;
        if (sideIndex > 0 && firstSurfaceImage.isNull())
            firstSurfaceImage = image;
    }

    if (!sideImages.contains(1) || !sideImages.contains(2)) {
        for (const QString &alias : planetTextureAliases(QFileInfo(spherePath).completeBaseName())) {
            if (alias.isEmpty())
                continue;
            if (!sideImages.contains(1)) {
                const QImage image = resolveTextureImage(QStringList{alias + QStringLiteral("01"),
                                                                     alias + QStringLiteral("_01")});
                if (!image.isNull())
                    sideImages.insert(1, image);
            }
            if (!sideImages.contains(2)) {
                const QImage image = resolveTextureImage(QStringList{alias + QStringLiteral("02"),
                                                                     alias + QStringLiteral("_02")});
                if (!image.isNull())
                    sideImages.insert(2, image);
            }
            if (sideImages.contains(1) && sideImages.contains(2))
                break;
        }
    }

    if (capImage.isNull()) {
        for (const QString &alias : planetTextureAliases(QFileInfo(spherePath).completeBaseName())) {
            if (alias.isEmpty())
                continue;

            const QImage image = resolveTextureImage(QStringList{alias + QStringLiteral("cap"),
                                                                 alias + QStringLiteral("_cap")});
            if (!image.isNull()) {
                capImage = image;
                break;
            }
        }
    }

    const QImage sideAtlas = projectPlanetSideTextures(sideImages.value(1), sideImages.value(2));
    PlanetSurfaceTextureSet textures;
    textures.side1 = sideImages.value(1);
    textures.side2 = sideImages.value(2);
    textures.cap = capImage;
    textures.fallbackAtlas = !sideAtlas.isNull() ? sideAtlas : firstSurfaceImage;
    return textures;
}

QImage FreelancerMaterialResolver::loadBestPlanetTexture(const QString &archetype, const QStringList &sourcePaths)
{
    const PlanetSurfaceTextureSet sphereMaterialTextures = loadPlanetSurfaceTextures(sourcePaths);
    if (!sphereMaterialTextures.fallbackAtlas.isNull())
        return sphereMaterialTextures.fallbackAtlas;

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
