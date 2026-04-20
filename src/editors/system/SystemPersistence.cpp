// editors/system/SystemPersistence.cpp – Laden/Speichern von System-INI-Dateien

#include "SystemPersistence.h"
#include "../../domain/SystemDocument.h"
#include "../../domain/SolarObject.h"
#include "../../domain/ZoneItem.h"
#include "../../core/PathUtils.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QVector3D>

using namespace flatlas::infrastructure;
using namespace flatlas::domain;

namespace flatlas::editors {

// ─── static storage ───────────────────────────────────────────────────────────
QHash<const SystemDocument *, IniDocument> SystemPersistence::s_extras;

// ─── helpers ──────────────────────────────────────────────────────────────────

static QVector3D parseVec3(const QString &text)
{
    const QStringList parts = text.split(QLatin1Char(','));
    if (parts.size() < 3)
        return {};
    bool okX, okY, okZ;
    float x = parts[0].trimmed().toFloat(&okX);
    float y = parts[1].trimmed().toFloat(&okY);
    float z = parts[2].trimmed().toFloat(&okZ);
    return (okX && okY && okZ) ? QVector3D(x, y, z) : QVector3D();
}

static QString vec3ToString(const QVector3D &v)
{
    return QStringLiteral("%1, %2, %3")
        .arg(static_cast<double>(v.x()), 0, 'f', -1)
        .arg(static_cast<double>(v.y()), 0, 'f', -1)
        .arg(static_cast<double>(v.z()), 0, 'f', -1);
}

static SolarObject::Type detectObjectType(const IniSection &sec)
{
    const QString archetype = sec.value(QStringLiteral("archetype")).toLower();

    // explicit "star" key → Sun
    if (!sec.value(QStringLiteral("star")).isEmpty())
        return SolarObject::Sun;

    const bool hasGoto = !sec.value(QStringLiteral("goto")).isEmpty();
    if (hasGoto) {
        if (archetype.contains(QLatin1String("jump_gate")))
            return SolarObject::JumpGate;
        if (archetype.contains(QLatin1String("jump_hole")))
            return SolarObject::JumpHole;
        return SolarObject::JumpGate; // fallback for goto
    }

    if (archetype.contains(QLatin1String("trade"))
        || !sec.value(QStringLiteral("prev_ring")).isEmpty()
        || !sec.value(QStringLiteral("next_ring")).isEmpty())
        return SolarObject::TradeLane;

    if (!sec.value(QStringLiteral("base")).isEmpty())
        return SolarObject::Station;

    if (archetype.contains(QLatin1String("sun")))
        return SolarObject::Sun;
    if (archetype.contains(QLatin1String("planet")))
        return SolarObject::Planet;
    if (archetype.contains(QLatin1String("satellite")))
        return SolarObject::Satellite;
    if (archetype.contains(QLatin1String("waypoint")))
        return SolarObject::Waypoint;
    if (archetype.contains(QLatin1String("weapons_platform")))
        return SolarObject::Weapons_Platform;
    if (archetype.contains(QLatin1String("depot")))
        return SolarObject::Depot;
    if (archetype.contains(QLatin1String("dock")))
        return SolarObject::DockingRing;
    if (archetype.contains(QLatin1String("wreck")))
        return SolarObject::Wreck;
    if (archetype.contains(QLatin1String("asteroid")))
        return SolarObject::Asteroid;

    return SolarObject::Other;
}

static ZoneItem::Shape parseShape(const QString &text)
{
    const QString s = text.trimmed().toUpper();
    if (s == QLatin1String("ELLIPSOID"))  return ZoneItem::Ellipsoid;
    if (s == QLatin1String("CYLINDER"))   return ZoneItem::Cylinder;
    if (s == QLatin1String("BOX"))        return ZoneItem::Box;
    if (s == QLatin1String("RING"))       return ZoneItem::Ring;
    return ZoneItem::Sphere; // default
}

static QString shapeToString(ZoneItem::Shape s)
{
    switch (s) {
    case ZoneItem::Sphere:    return QStringLiteral("SPHERE");
    case ZoneItem::Ellipsoid: return QStringLiteral("ELLIPSOID");
    case ZoneItem::Cylinder:  return QStringLiteral("CYLINDER");
    case ZoneItem::Box:       return QStringLiteral("BOX");
    case ZoneItem::Ring:      return QStringLiteral("RING");
    }
    return QStringLiteral("SPHERE");
}

static void applyUniverseNavMapScale(SystemDocument *doc, const QString &filePath)
{
    if (!doc || filePath.isEmpty())
        return;

    const QFileInfo systemInfo(filePath);
    const QString universeDir = QDir(systemInfo.absolutePath()).absoluteFilePath(QStringLiteral("../.."));
    const QString universePath =
        flatlas::core::PathUtils::ciResolvePath(universeDir, QStringLiteral("universe.ini"));
    if (universePath.isEmpty())
        return;

    const IniDocument universeDoc = IniParser::parseFile(universePath);
    if (universeDoc.isEmpty())
        return;

    const QString normalizedSystemPath =
        flatlas::core::PathUtils::normalizePath(systemInfo.absoluteFilePath()).toLower();
    const QString systemNickname = doc->name().trimmed();

    for (const IniSection &section : universeDoc) {
        if (section.name.compare(QStringLiteral("system"), Qt::CaseInsensitive) != 0)
            continue;

        bool matches = false;
        const QString relativeFile = section.value(QStringLiteral("file")).trimmed();
        if (!relativeFile.isEmpty()) {
            QString resolvedPath = flatlas::core::PathUtils::ciResolvePath(universeDir, relativeFile);
            if (resolvedPath.isEmpty())
                resolvedPath = QDir(universeDir).absoluteFilePath(relativeFile);

            const QString normalizedResolved =
                flatlas::core::PathUtils::normalizePath(QFileInfo(resolvedPath).absoluteFilePath()).toLower();
            matches = !normalizedResolved.isEmpty() && normalizedResolved == normalizedSystemPath;
        }

        if (!matches && !systemNickname.isEmpty()) {
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            matches = nickname.compare(systemNickname, Qt::CaseInsensitive) == 0;
        }

        if (!matches)
            continue;

        bool ok = false;
        const double navMapScale = section.value(QStringLiteral("NavMapScale")).toDouble(&ok);
        if (ok && navMapScale > 0.0)
            doc->setNavMapScale(navMapScale);
        return;
    }
}

// ─── load ─────────────────────────────────────────────────────────────────────

std::unique_ptr<SystemDocument> SystemPersistence::load(const QString &filePath)
{
    const IniDocument ini = IniParser::parseFile(filePath);
    if (ini.isEmpty())
        return nullptr;

    auto doc = std::make_unique<SystemDocument>();
    doc->setFilePath(filePath);

    IniDocument extras;

    for (const IniSection &sec : ini) {
        const QString sectionName = sec.name.toLower();

        // ── SystemInfo ──────────────────────────────────────────────
        if (sectionName == QLatin1String("systeminfo")) {
            doc->setName(sec.value(QStringLiteral("nickname")));
            bool ok = false;
            const double navMapScale = sec.value(QStringLiteral("NavMapScale")).toDouble(&ok);
            if (ok && navMapScale > 0.0)
                doc->setNavMapScale(navMapScale);
            continue;
        }

        // ── Object ──────────────────────────────────────────────────
        if (sectionName == QLatin1String("object")) {
            auto obj = std::make_shared<SolarObject>();
            obj->setNickname(sec.value(QStringLiteral("nickname")));
            obj->setArchetype(sec.value(QStringLiteral("archetype")));

            const QString posStr = sec.value(QStringLiteral("pos"));
            if (!posStr.isEmpty())
                obj->setPosition(parseVec3(posStr));

            const QString rotStr = sec.value(QStringLiteral("rotate"));
            if (!rotStr.isEmpty())
                obj->setRotation(parseVec3(rotStr));

            bool ok = false;
            int idsName = sec.value(QStringLiteral("ids_name")).toInt(&ok);
            if (ok) obj->setIdsName(idsName);

            int idsInfo = sec.value(QStringLiteral("ids_info")).toInt(&ok);
            if (ok) obj->setIdsInfo(idsInfo);

            obj->setBase(sec.value(QStringLiteral("base")));
            obj->setDockWith(sec.value(QStringLiteral("dock_with")));
            obj->setGotoTarget(sec.value(QStringLiteral("goto")));
            obj->setLoadout(sec.value(QStringLiteral("loadout")));
            obj->setComment(sec.value(QStringLiteral("comment")));
            obj->setType(detectObjectType(sec));

            doc->addObject(std::move(obj));
            continue;
        }

        // ── Zone ────────────────────────────────────────────────────
        if (sectionName == QLatin1String("zone")) {
            auto zone = std::make_shared<ZoneItem>();
            zone->setNickname(sec.value(QStringLiteral("nickname")));

            const QString posStr = sec.value(QStringLiteral("pos"));
            if (!posStr.isEmpty())
                zone->setPosition(parseVec3(posStr));

            const QString sizeStr = sec.value(QStringLiteral("size"));
            if (!sizeStr.isEmpty())
                zone->setSize(parseVec3(sizeStr));

            const QString rotStr = sec.value(QStringLiteral("rotate"));
            if (!rotStr.isEmpty())
                zone->setRotation(parseVec3(rotStr));

            const QString shapeStr = sec.value(QStringLiteral("shape"));
            if (!shapeStr.isEmpty())
                zone->setShape(parseShape(shapeStr));

            zone->setZoneType(sec.value(QStringLiteral("zone_type")));
            zone->setComment(sec.value(QStringLiteral("comment")));

            const QString tightStr = sec.value(QStringLiteral("tightness"));
            if (!tightStr.isEmpty())
                zone->setTightnessXyz(parseVec3(tightStr));

            bool ok = false;
            int dmg = sec.value(QStringLiteral("damage")).toInt(&ok);
            if (ok) zone->setDamage(dmg);

            float interf = sec.value(QStringLiteral("interference")).toFloat(&ok);
            if (ok) zone->setInterference(interf);

            float drag = sec.value(QStringLiteral("drag_modifier")).toFloat(&ok);
            if (ok) zone->setDragScale(drag);

            int sort = sec.value(QStringLiteral("sort")).toInt(&ok);
            if (ok) zone->setSortKey(sort);

            doc->addZone(std::move(zone));
            continue;
        }

        // ── everything else → extras ────────────────────────────────
        extras.append(sec);
    }

    s_extras[doc.get()] = extras;
    applyUniverseNavMapScale(doc.get(), filePath);
    doc->setDirty(false);
    return doc;
}

// ─── save ─────────────────────────────────────────────────────────────────────

static IniSection buildSystemInfo(const SystemDocument &doc)
{
    IniSection sec;
    sec.name = QStringLiteral("SystemInfo");
    sec.entries.append({QStringLiteral("nickname"), doc.name()});
    sec.entries.append({QStringLiteral("NavMapScale"), QString::number(doc.navMapScale(), 'f', 6)});
    return sec;
}

static IniSection buildObjectSection(const SolarObject &obj)
{
    IniSection sec;
    sec.name = QStringLiteral("Object");

    sec.entries.append({QStringLiteral("nickname"), obj.nickname()});

    if (obj.idsName() != 0)
        sec.entries.append({QStringLiteral("ids_name"), QString::number(obj.idsName())});
    if (obj.idsInfo() != 0)
        sec.entries.append({QStringLiteral("ids_info"), QString::number(obj.idsInfo())});

    if (!obj.position().isNull())
        sec.entries.append({QStringLiteral("pos"), vec3ToString(obj.position())});
    if (!obj.rotation().isNull())
        sec.entries.append({QStringLiteral("rotate"), vec3ToString(obj.rotation())});

    if (!obj.archetype().isEmpty())
        sec.entries.append({QStringLiteral("archetype"), obj.archetype()});
    if (!obj.base().isEmpty())
        sec.entries.append({QStringLiteral("base"), obj.base()});
    if (!obj.dockWith().isEmpty())
        sec.entries.append({QStringLiteral("dock_with"), obj.dockWith()});
    if (!obj.gotoTarget().isEmpty())
        sec.entries.append({QStringLiteral("goto"), obj.gotoTarget()});
    if (!obj.loadout().isEmpty())
        sec.entries.append({QStringLiteral("loadout"), obj.loadout()});
    if (!obj.comment().isEmpty())
        sec.entries.append({QStringLiteral("comment"), obj.comment()});

    return sec;
}

static IniSection buildZoneSection(const ZoneItem &zone)
{
    IniSection sec;
    sec.name = QStringLiteral("Zone");

    sec.entries.append({QStringLiteral("nickname"), zone.nickname()});

    if (!zone.position().isNull())
        sec.entries.append({QStringLiteral("pos"), vec3ToString(zone.position())});
    if (!zone.size().isNull())
        sec.entries.append({QStringLiteral("size"), vec3ToString(zone.size())});
    if (!zone.rotation().isNull())
        sec.entries.append({QStringLiteral("rotate"), vec3ToString(zone.rotation())});

    sec.entries.append({QStringLiteral("shape"), shapeToString(zone.shape())});

    if (!zone.zoneType().isEmpty())
        sec.entries.append({QStringLiteral("zone_type"), zone.zoneType()});
    if (!zone.comment().isEmpty())
        sec.entries.append({QStringLiteral("comment"), zone.comment()});
    if (!zone.tightnessXyz().isNull())
        sec.entries.append({QStringLiteral("tightness"), vec3ToString(zone.tightnessXyz())});
    if (zone.damage() != 0)
        sec.entries.append({QStringLiteral("damage"), QString::number(zone.damage())});
    if (zone.interference() != 0.0f)
        sec.entries.append({QStringLiteral("interference"), QString::number(static_cast<double>(zone.interference()))});
    if (zone.dragScale() != 0.0f)
        sec.entries.append({QStringLiteral("drag_modifier"), QString::number(static_cast<double>(zone.dragScale()))});
    if (zone.sortKey() != 0)
        sec.entries.append({QStringLiteral("sort"), QString::number(zone.sortKey())});

    return sec;
}

bool SystemPersistence::save(const SystemDocument &doc, const QString &filePath)
{
    IniDocument ini;

    // [SystemInfo]
    ini.append(buildSystemInfo(doc));

    // [Object] sections
    for (const auto &obj : doc.objects())
        ini.append(buildObjectSection(*obj));

    // [Zone] sections
    for (const auto &zone : doc.zones())
        ini.append(buildZoneSection(*zone));

    // extra sections (LightSource, Ambient, etc.)
    const auto it = s_extras.constFind(&doc);
    if (it != s_extras.constEnd())
        ini.append(*it);

    const QString text = IniParser::serialize(ini);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << text;
    return true;
}

bool SystemPersistence::save(const SystemDocument &doc)
{
    return save(doc, doc.filePath());
}

// ─── extras management ────────────────────────────────────────────────────────

IniDocument SystemPersistence::extraSections(const SystemDocument *doc)
{
    return s_extras.value(doc);
}

void SystemPersistence::clearExtras(const SystemDocument *doc)
{
    s_extras.remove(doc);
}

} // namespace flatlas::editors
