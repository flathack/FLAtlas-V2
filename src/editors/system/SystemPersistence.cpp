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

static QVector3D parseZoneSize(const QString &text)
{
    const QStringList parts = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return {};

    bool ok0 = false;
    const float s0 = parts.value(0).trimmed().toFloat(&ok0);
    if (!ok0)
        return {};

    if (parts.size() == 1)
        return QVector3D(s0, s0, s0);

    bool ok1 = false;
    const float s1 = parts.value(1).trimmed().toFloat(&ok1);
    if (!ok1)
        return {};

    if (parts.size() == 2)
        return QVector3D(s0, s1, s0);

    bool ok2 = false;
    const float s2 = parts.value(2).trimmed().toFloat(&ok2);
    if (!ok2)
        return {};

    return QVector3D(s0, s1, s2);
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

static void upsertEntry(QVector<IniEntry> &entries, const QString &key, const QString &value)
{
    for (auto &entry : entries) {
        if (entry.first.compare(key, Qt::CaseInsensitive) == 0) {
            entry.second = value;
            return;
        }
    }
    entries.append({key, value});
}

static void removeEntry(QVector<IniEntry> &entries, const QString &key)
{
    for (int i = entries.size() - 1; i >= 0; --i) {
        if (entries[i].first.compare(key, Qt::CaseInsensitive) == 0)
            entries.removeAt(i);
    }
}

static void setOptionalEntry(QVector<IniEntry> &entries, const QString &key, const QString &value)
{
    if (value.trimmed().isEmpty()) {
        removeEntry(entries, key);
        return;
    }
    upsertEntry(entries, key, value);
}

static void setOptionalIntEntry(QVector<IniEntry> &entries, const QString &key, int value)
{
    if (value == 0) {
        removeEntry(entries, key);
        return;
    }
    upsertEntry(entries, key, QString::number(value));
}

static void setOptionalFloatEntry(QVector<IniEntry> &entries, const QString &key, float value)
{
    if (qFuzzyIsNull(value)) {
        removeEntry(entries, key);
        return;
    }
    upsertEntry(entries, key, QString::number(static_cast<double>(value)));
}

static void setOptionalVec3Entry(QVector<IniEntry> &entries, const QString &key, const QVector3D &value)
{
    if (value.isNull()) {
        removeEntry(entries, key);
        return;
    }
    upsertEntry(entries, key, vec3ToString(value));
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

    // Build a robust identity for the system being loaded:
    //  1) a canonical (collapsed-`..`, on-disk casing) absolute path,
    //  2) the filename stem (e.g. "li01") as a fallback when the explicit
    //     nickname cannot be established — vanilla system .ini files often
    //     lack a `nickname = ` entry in [SystemInfo], which previously caused
    //     universe.ini’s NavMapScale to silently fall back to the default
    //     value and mis-render the 8x8 NavMap grid (default 1.0 instead of
    //     the actual 1.36 vanilla scale).
    auto canonicalize = [](const QString &path) {
        if (path.isEmpty())
            return QString();
        const QString canonical = QFileInfo(path).canonicalFilePath();
        const QString effective = canonical.isEmpty() ? QFileInfo(path).absoluteFilePath() : canonical;
        return flatlas::core::PathUtils::normalizePath(effective).toLower();
    };

    const QString canonicalSystemPath = canonicalize(systemInfo.absoluteFilePath());
    const QString systemNickname = doc->name().trimmed();
    const QString systemFileStem = systemInfo.completeBaseName(); // e.g. "Li01"

    for (const IniSection &section : universeDoc) {
        if (section.name.compare(QStringLiteral("system"), Qt::CaseInsensitive) != 0)
            continue;

        bool matches = false;
        const QString relativeFile = section.value(QStringLiteral("file")).trimmed();
        if (!relativeFile.isEmpty()) {
            QString resolvedPath = flatlas::core::PathUtils::ciResolvePath(universeDir, relativeFile);
            if (resolvedPath.isEmpty())
                resolvedPath = QDir(universeDir).absoluteFilePath(relativeFile);

            const QString canonicalResolved = canonicalize(resolvedPath);
            matches = !canonicalResolved.isEmpty()
                      && !canonicalSystemPath.isEmpty()
                      && canonicalResolved == canonicalSystemPath;

            // Also try matching by the file's basename (stem). This is the
            // most forgiving identifier and robust against any residual path
            // normalisation issues across mods/installations.
            if (!matches) {
                const QString relStem = QFileInfo(relativeFile).completeBaseName();
                if (!relStem.isEmpty() && !systemFileStem.isEmpty())
                    matches = relStem.compare(systemFileStem, Qt::CaseInsensitive) == 0;
            }
        }

        if (!matches) {
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            if (!nickname.isEmpty()) {
                if (!systemNickname.isEmpty() && nickname.compare(systemNickname, Qt::CaseInsensitive) == 0)
                    matches = true;
                else if (!systemFileStem.isEmpty() && nickname.compare(systemFileStem, Qt::CaseInsensitive) == 0)
                    matches = true;
            }
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
            applyObjectSection(*obj, sec);
            doc->addObject(std::move(obj));
            continue;
        }

        // ── Zone ────────────────────────────────────────────────────
        if (sectionName == QLatin1String("zone")) {
            auto zone = std::make_shared<ZoneItem>();
            applyZoneSection(*zone, sec);
            doc->addZone(std::move(zone));
            continue;
        }

        // ── everything else → extras ────────────────────────────────
        extras.append(sec);
    }

    s_extras[doc.get()] = extras;
    // Ensure the document has a usable name BEFORE looking up NavMapScale in
    // universe.ini — vanilla system .ini files frequently omit `nickname` in
    // [SystemInfo], and without a name the universe lookup would otherwise
    // silently miss and leave NavMapScale at the default.
    if (doc->name().trimmed().isEmpty())
        doc->setName(QFileInfo(filePath).completeBaseName());
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

IniSection SystemPersistence::serializeObjectSection(const SolarObject &obj)
{
    IniSection sec;
    sec.name = QStringLiteral("Object");
    sec.entries = obj.rawEntries();

    upsertEntry(sec.entries, QStringLiteral("nickname"), obj.nickname());
    setOptionalIntEntry(sec.entries, QStringLiteral("ids_name"), obj.idsName());
    setOptionalIntEntry(sec.entries, QStringLiteral("ids_info"), obj.idsInfo());
    setOptionalVec3Entry(sec.entries, QStringLiteral("pos"), obj.position());
    setOptionalVec3Entry(sec.entries, QStringLiteral("rotate"), obj.rotation());
    setOptionalEntry(sec.entries, QStringLiteral("archetype"), obj.archetype());
    setOptionalEntry(sec.entries, QStringLiteral("base"), obj.base());
    setOptionalEntry(sec.entries, QStringLiteral("dock_with"), obj.dockWith());
    setOptionalEntry(sec.entries, QStringLiteral("goto"), obj.gotoTarget());
    setOptionalEntry(sec.entries, QStringLiteral("loadout"), obj.loadout());
    setOptionalEntry(sec.entries, QStringLiteral("comment"), obj.comment());

    return sec;
}

IniSection SystemPersistence::serializeZoneSection(const ZoneItem &zone)
{
    IniSection sec;
    sec.name = QStringLiteral("Zone");
    sec.entries = zone.rawEntries();

    upsertEntry(sec.entries, QStringLiteral("nickname"), zone.nickname());
    setOptionalVec3Entry(sec.entries, QStringLiteral("pos"), zone.position());
    setOptionalVec3Entry(sec.entries, QStringLiteral("size"), zone.size());
    setOptionalVec3Entry(sec.entries, QStringLiteral("rotate"), zone.rotation());
    upsertEntry(sec.entries, QStringLiteral("shape"), shapeToString(zone.shape()));
    setOptionalEntry(sec.entries, QStringLiteral("zone_type"), zone.zoneType());
    setOptionalEntry(sec.entries, QStringLiteral("usage"), zone.usage());
    setOptionalEntry(sec.entries, QStringLiteral("pop_type"), zone.popType());
    setOptionalEntry(sec.entries, QStringLiteral("path_label"), zone.pathLabel());
    setOptionalEntry(sec.entries, QStringLiteral("comment"), zone.comment());
    setOptionalVec3Entry(sec.entries, QStringLiteral("tightness"), zone.tightnessXyz());
    setOptionalIntEntry(sec.entries, QStringLiteral("damage"), zone.damage());
    setOptionalFloatEntry(sec.entries, QStringLiteral("interference"), zone.interference());
    setOptionalFloatEntry(sec.entries, QStringLiteral("drag_modifier"), zone.dragScale());
    setOptionalIntEntry(sec.entries, QStringLiteral("sort"), zone.sortKey());

    return sec;
}

void SystemPersistence::applyObjectSection(SolarObject &obj, const IniSection &sec)
{
    obj.setRawEntries(sec.entries);
    obj.setNickname(sec.value(QStringLiteral("nickname")));
    obj.setArchetype(sec.value(QStringLiteral("archetype")));

    const QString posStr = sec.value(QStringLiteral("pos"));
    obj.setPosition(posStr.isEmpty() ? QVector3D() : parseVec3(posStr));

    const QString rotStr = sec.value(QStringLiteral("rotate"));
    obj.setRotation(rotStr.isEmpty() ? QVector3D() : parseVec3(rotStr));

    bool ok = false;
    const int idsName = sec.value(QStringLiteral("ids_name")).toInt(&ok);
    obj.setIdsName(ok ? idsName : 0);

    const int idsInfo = sec.value(QStringLiteral("ids_info")).toInt(&ok);
    obj.setIdsInfo(ok ? idsInfo : 0);

    obj.setBase(sec.value(QStringLiteral("base")));
    obj.setDockWith(sec.value(QStringLiteral("dock_with")));
    obj.setGotoTarget(sec.value(QStringLiteral("goto")));
    obj.setLoadout(sec.value(QStringLiteral("loadout")));
    obj.setComment(sec.value(QStringLiteral("comment")));
    obj.setType(detectObjectType(sec));
}

void SystemPersistence::applyZoneSection(ZoneItem &zone, const IniSection &sec)
{
    zone.setRawEntries(sec.entries);
    zone.setNickname(sec.value(QStringLiteral("nickname")));

    const QString posStr = sec.value(QStringLiteral("pos"));
    zone.setPosition(posStr.isEmpty() ? QVector3D() : parseVec3(posStr));

    const QString sizeStr = sec.value(QStringLiteral("size"));
    zone.setSize(sizeStr.isEmpty() ? QVector3D() : parseZoneSize(sizeStr));

    const QString rotStr = sec.value(QStringLiteral("rotate"));
    zone.setRotation(rotStr.isEmpty() ? QVector3D() : parseVec3(rotStr));

    const QString shapeStr = sec.value(QStringLiteral("shape"));
    zone.setShape(shapeStr.isEmpty() ? ZoneItem::Sphere : parseShape(shapeStr));

    zone.setZoneType(sec.value(QStringLiteral("zone_type")));
    zone.setUsage(sec.value(QStringLiteral("usage")));
    zone.setPopType(sec.value(QStringLiteral("pop_type")));
    zone.setPathLabel(sec.value(QStringLiteral("path_label")));
    zone.setComment(sec.value(QStringLiteral("comment")));

    const QString tightStr = sec.value(QStringLiteral("tightness"));
    zone.setTightnessXyz(tightStr.isEmpty() ? QVector3D() : parseVec3(tightStr));

    bool ok = false;
    const int dmg = sec.value(QStringLiteral("damage")).toInt(&ok);
    zone.setDamage(ok ? dmg : 0);

    const float interf = sec.value(QStringLiteral("interference")).toFloat(&ok);
    zone.setInterference(ok ? interf : 0.0f);

    const float drag = sec.value(QStringLiteral("drag_modifier")).toFloat(&ok);
    zone.setDragScale(ok ? drag : 0.0f);

    const int sort = sec.value(QStringLiteral("sort")).toInt(&ok);
    zone.setSortKey(ok ? sort : 0);
}

bool SystemPersistence::save(const SystemDocument &doc, const QString &filePath)
{
    IniDocument ini;

    // [SystemInfo]
    ini.append(buildSystemInfo(doc));

    // [Object] sections
    for (const auto &obj : doc.objects())
        ini.append(serializeObjectSection(*obj));

    // [Zone] sections
    for (const auto &zone : doc.zones())
        ini.append(serializeZoneSection(*zone));

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
