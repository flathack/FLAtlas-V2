#include "FreelancerFlightResolver.h"

#include "core/PathUtils.h"
#include "infrastructure/parser/IniParser.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

#include <algorithm>

namespace flatlas::infrastructure {

namespace {

QString entryFirstToken(const QString &value)
{
    return value.split(QLatin1Char(','), Qt::KeepEmptyParts).value(0).trimmed();
}

float floatValue(const IniSection &section, const QString &key, float fallback = 0.0f)
{
    bool ok = false;
    const float value = section.value(key).trimmed().toFloat(&ok);
    return ok ? value : fallback;
}

QString sectionNickname(const IniSection &section)
{
    return section.value(QStringLiteral("nickname")).trimmed();
}

} // namespace

QString FreelancerFlightResolver::dataPath(const QString &gameRoot, const QString &relativePath)
{
    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    if (dataDir.isEmpty())
        return {};
    QString resolved = flatlas::core::PathUtils::ciResolvePath(dataDir, relativePath);
    if (resolved.isEmpty()) {
        const QString nativeRelative = QString(relativePath).replace(QLatin1Char('/'), QDir::separator());
        const QString candidate = QDir(dataDir).absoluteFilePath(nativeRelative);
        if (QFileInfo::exists(candidate))
            resolved = candidate;
    }
    return resolved;
}

QVector<FreelancerShipPackage> FreelancerFlightResolver::loadShipPackages(const QString &gameRoot)
{
    QVector<FreelancerShipPackage> packages;
    const QString path = dataPath(gameRoot, QStringLiteral("EQUIPMENT/goods.ini"));
    if (path.isEmpty())
        return packages;

    const IniDocument doc = IniParser::parseFile(path);
    QSet<QString> seen;
    for (const IniSection &section : doc) {
        if (section.name.compare(QStringLiteral("Good"), Qt::CaseInsensitive) != 0)
            continue;

        const QString category = section.value(QStringLiteral("category")).trimmed();
        if (category.compare(QStringLiteral("ship"), Qt::CaseInsensitive) != 0
            && category.compare(QStringLiteral("ship_package"), Qt::CaseInsensitive) != 0) {
            continue;
        }

        FreelancerShipPackage package;
        package.nickname = sectionNickname(section);
        package.shipArchetype = section.value(QStringLiteral("ship")).trimmed();
        package.hullGood = section.value(QStringLiteral("hull")).trimmed();
        package.loadoutNickname = section.value(QStringLiteral("loadout")).trimmed();
        for (const QString &addon : section.values(QStringLiteral("addon"))) {
            const QString token = entryFirstToken(addon);
            if (token.contains(QStringLiteral("engine"), Qt::CaseInsensitive)) {
                package.engineNickname = token;
                break;
            }
        }
        package.modelPath = resolveShipModelPath(gameRoot, package.shipArchetype);

        if (package.nickname.isEmpty() || seen.contains(package.nickname.toLower()))
            continue;
        seen.insert(package.nickname.toLower());
        packages.append(package);
    }

    std::sort(packages.begin(), packages.end(), [](const FreelancerShipPackage &lhs,
                                                   const FreelancerShipPackage &rhs) {
        return lhs.nickname.compare(rhs.nickname, Qt::CaseInsensitive) < 0;
    });
    return packages;
}

FreelancerShipPackage FreelancerFlightResolver::resolveShipPackage(const QString &gameRoot, const QString &packageNickname)
{
    const QString requested = packageNickname.trimmed();
    const QVector<FreelancerShipPackage> packages = loadShipPackages(gameRoot);
    if (!requested.isEmpty()) {
        for (const FreelancerShipPackage &package : packages) {
            if (package.nickname.compare(requested, Qt::CaseInsensitive) == 0)
                return package;
        }
    }
    return packages.isEmpty() ? FreelancerShipPackage{} : packages.first();
}

FreelancerEngineStats FreelancerFlightResolver::resolveEngine(const QString &gameRoot, const QString &engineNickname)
{
    FreelancerEngineStats stats;
    const QString path = dataPath(gameRoot, QStringLiteral("EQUIPMENT/engine_equip.ini"));
    if (path.isEmpty())
        return stats;

    const QString requested = engineNickname.trimmed();
    const IniDocument doc = IniParser::parseFile(path);
    for (const IniSection &section : doc) {
        if (section.name.compare(QStringLiteral("Engine"), Qt::CaseInsensitive) != 0)
            continue;
        const QString nickname = sectionNickname(section);
        if (!requested.isEmpty() && nickname.compare(requested, Qt::CaseInsensitive) != 0)
            continue;

        stats.nickname = nickname;
        stats.maxForce = floatValue(section, QStringLiteral("max_force"));
        stats.linearDrag = floatValue(section, QStringLiteral("linear_drag"));
        stats.cruiseChargeTime = floatValue(section, QStringLiteral("cruise_charge_time"), 5.0f);
        if (stats.maxForce > 0.0f && stats.linearDrag > 0.0f)
            stats.maxSpeed = stats.maxForce / stats.linearDrag;
        return stats;
    }
    return stats;
}

float FreelancerFlightResolver::resolveCruiseSpeed(const QString &gameRoot)
{
    const QString path = dataPath(gameRoot, QStringLiteral("constants.ini"));
    if (path.isEmpty())
        return 300.0f;

    const IniDocument doc = IniParser::parseFile(path);
    const QStringList keys = {
        QStringLiteral("CRUISE_SPEED"),
        QStringLiteral("cruise_speed"),
        QStringLiteral("CruiseSpeed"),
    };
    for (const IniSection &section : doc) {
        for (const QString &key : keys) {
            bool ok = false;
            const float value = section.value(key).trimmed().toFloat(&ok);
            if (ok && value > 0.0f)
                return value;
        }
    }
    return 300.0f;
}

QString FreelancerFlightResolver::resolveShipModelPath(const QString &gameRoot, const QString &shipArchetype)
{
    const QString path = dataPath(gameRoot, QStringLiteral("SHIPS/shiparch.ini"));
    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    if (path.isEmpty() || dataDir.isEmpty())
        return {};

    const QString requested = shipArchetype.trimmed();
    if (requested.isEmpty())
        return {};

    const IniDocument doc = IniParser::parseFile(path);
    for (const IniSection &section : doc) {
        if (section.name.compare(QStringLiteral("Ship"), Qt::CaseInsensitive) != 0)
            continue;
        if (sectionNickname(section).compare(requested, Qt::CaseInsensitive) != 0)
            continue;

        const QString relativeModelPath = section.value(QStringLiteral("DA_archetype")).trimmed();
        if (relativeModelPath.isEmpty())
            return {};
        return flatlas::core::PathUtils::ciResolvePath(dataDir, relativeModelPath);
    }
    return {};
}

FreelancerThirdPersonCamera FreelancerFlightResolver::resolveThirdPersonCamera(const QString &gameRoot)
{
    FreelancerThirdPersonCamera camera;
    const QString path = dataPath(gameRoot, QStringLiteral("cameras.ini"));
    if (path.isEmpty())
        return camera;

    const IniDocument doc = IniParser::parseFile(path);
    for (const IniSection &section : doc) {
        if (section.name.compare(QStringLiteral("ThirdPersonCamera"), Qt::CaseInsensitive) != 0)
            continue;
        camera.fovX = floatValue(section, QStringLiteral("fovx"), camera.fovX);
        camera.zNear = floatValue(section, QStringLiteral("znear"), camera.zNear);
        return camera;
    }
    return camera;
}

FreelancerFlightStats FreelancerFlightResolver::resolveFlightStats(const QString &gameRoot,
                                                                   const QString &packageNickname)
{
    FreelancerFlightStats stats;
    stats.ship = resolveShipPackage(gameRoot, packageNickname);
    if (stats.ship.modelPath.isEmpty())
        stats.ship.modelPath = resolveShipModelPath(gameRoot, stats.ship.shipArchetype);
    stats.engine = resolveEngine(gameRoot, stats.ship.engineNickname);
    stats.cruiseSpeed = resolveCruiseSpeed(gameRoot);
    return stats;
}

} // namespace flatlas::infrastructure
