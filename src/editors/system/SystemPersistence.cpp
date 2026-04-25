// editors/system/SystemPersistence.cpp – Laden/Speichern von System-INI-Dateien

#include "SystemPersistence.h"
#include "../../domain/SystemDocument.h"
#include "../../domain/SolarObject.h"
#include "../../domain/ZoneItem.h"
#include "../../core/PathUtils.h"
#include "../../core/EditingContext.h"
#include "../../infrastructure/freelancer/IdsStringTable.h"
#include "../../infrastructure/parser/IniAnalysisService.h"
#include "../../infrastructure/parser/BiniDecoder.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QStringDecoder>
#include <QTextStream>
#include <QVector3D>

using namespace flatlas::infrastructure;
using namespace flatlas::domain;

namespace flatlas::editors {

// ─── static storage ───────────────────────────────────────────────────────────
QHash<const SystemDocument *, IniDocument> SystemPersistence::s_extras;
QHash<const SystemDocument *, IniSection> SystemPersistence::s_systemInfoSections;
QHash<const SystemDocument *, IniDocument> SystemPersistence::s_layoutSections;
QHash<const SystemDocument *, bool> SystemPersistence::s_nonStandardOrder;

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

static QString decodeIniText(const QByteArray &raw)
{
    if (raw.isEmpty())
        return {};

    if (BiniDecoder::isBini(raw))
        return {};

    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    const QString utf8Text = utf8(raw);
    if (!utf8.hasError())
        return utf8Text;
    return QString::fromLatin1(raw);
}

static QVector<QString> extractLeadingZoneComments(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QString text = decodeIniText(file.readAll());
    if (text.isEmpty())
        return {};

    QVector<QString> comments;
    QStringList pendingCommentLines;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString trimmed = rawLine.trimmed();
        if (trimmed.isEmpty()) {
            pendingCommentLines.clear();
            continue;
        }

        if (trimmed.startsWith(QLatin1Char(';'))) {
            pendingCommentLines.append(trimmed.mid(1).trimmed());
            continue;
        }
        if (trimmed.startsWith(QLatin1String("//"))) {
            pendingCommentLines.append(trimmed.mid(2).trimmed());
            continue;
        }

        if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']'))) {
            const QString sectionName = trimmed.mid(1, trimmed.size() - 2).trimmed();
            if (sectionName.compare(QStringLiteral("Zone"), Qt::CaseInsensitive) == 0)
                comments.append(pendingCommentLines.join(QLatin1Char('\n')).trimmed());
            pendingCommentLines.clear();
            continue;
        }

        pendingCommentLines.clear();
    }

    return comments;
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
    if (archetype.contains(QLatin1String("waypoint"))
        || archetype.contains(QLatin1String("buoy")))
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

static int findEntryIndex(const QVector<IniEntry> &entries, const QString &key)
{
    for (int index = 0; index < entries.size(); ++index) {
        if (entries[index].first.compare(key, Qt::CaseInsensitive) == 0)
            return index;
    }
    return -1;
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

static bool vec3Equals(const QVector3D &lhs, const QVector3D &rhs)
{
    return qFuzzyCompare(lhs.x() + 1.0f, rhs.x() + 1.0f)
           && qFuzzyCompare(lhs.y() + 1.0f, rhs.y() + 1.0f)
           && qFuzzyCompare(lhs.z() + 1.0f, rhs.z() + 1.0f);
}

static void appendSerializedSection(QString &out,
                                    const IniSection &section,
                                    const QString &leadingComment = QString())
{
    if (!out.isEmpty() && !out.endsWith(QLatin1Char('\n')))
        out.append(QLatin1Char('\n'));
    if (!out.isEmpty())
        out.append(QLatin1Char('\n'));

    const QString trimmedComment = leadingComment.trimmed();
    if (!trimmedComment.isEmpty()) {
        const QStringList commentLines =
            trimmedComment.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
        for (const QString &commentLine : commentLines)
            out += QStringLiteral("; %1\n").arg(commentLine.trimmed());
    }

    out += QLatin1Char('[') + section.name + QLatin1String("]\n");
    for (const auto &entry : section.entries)
        out += entry.first + QLatin1String(" = ") + entry.second + QLatin1Char('\n');
}

enum class SystemSectionGroup {
    SystemInfo = 0,
    Archetype = 10,
    EncounterParameters = 20,
    TexturePanels = 30,
    Music = 40,
    Dust = 50,
    FieldRefs = 60,
    Ambient = 70,
    Background = 80,
    Content = 90,
    Other = 100,
};

static IniSection buildSystemInfo(const SystemDocument &doc, const IniSection &baseSection = IniSection{});

static QString normalizedSectionName(const QString &name)
{
    return name.trimmed().toLower();
}

static SystemSectionGroup classifySystemSection(const IniSection &section)
{
    const QString name = normalizedSectionName(section.name);
    if (name == QStringLiteral("systeminfo"))
        return SystemSectionGroup::SystemInfo;
    if (name == QStringLiteral("archetype"))
        return SystemSectionGroup::Archetype;
    if (name == QStringLiteral("encounterparameters"))
        return SystemSectionGroup::EncounterParameters;
    if (name == QStringLiteral("texturepanels"))
        return SystemSectionGroup::TexturePanels;
    if (name == QStringLiteral("music"))
        return SystemSectionGroup::Music;
    if (name == QStringLiteral("dust"))
        return SystemSectionGroup::Dust;
    if (name == QStringLiteral("nebula")
        || name == QStringLiteral("asteroids")
        || name == QStringLiteral("dynamicasteroids")) {
        return SystemSectionGroup::FieldRefs;
    }
    if (name == QStringLiteral("ambient"))
        return SystemSectionGroup::Ambient;
    if (name == QStringLiteral("background"))
        return SystemSectionGroup::Background;
    if (name == QStringLiteral("object")
        || name == QStringLiteral("zone")
        || name == QStringLiteral("lightsource")) {
        return SystemSectionGroup::Content;
    }
    return SystemSectionGroup::Other;
}

static QString normalizedIniValue(const QString &value)
{
    return value.trimmed().toLower();
}

static QString sectionIdentityKey(const IniSection &section)
{
    const QString sectionName = normalizedSectionName(section.name);
    if (sectionName == QStringLiteral("systeminfo"))
        return sectionName;

    const QVector<QString> identityKeys = {
        QStringLiteral("nickname"),
        QStringLiteral("zone"),
        QStringLiteral("file"),
        QStringLiteral("name"),
        QStringLiteral("base"),
    };
    for (const QString &key : identityKeys) {
        const QString value = section.value(key).trimmed();
        if (!value.isEmpty())
            return QStringLiteral("%1|%2|%3").arg(sectionName, key, normalizedIniValue(value));
    }
    return sectionName;
}

static void insertSectionByStandardOrder(IniDocument &sections, const IniSection &section)
{
    const auto sectionGroup = classifySystemSection(section);

    int insertIndex = sections.size();
    for (int index = sections.size() - 1; index >= 0; --index) {
        const auto existingGroup = classifySystemSection(sections[index]);
        if (existingGroup == sectionGroup) {
            insertIndex = index + 1;
            break;
        }
        if (static_cast<int>(existingGroup) < static_cast<int>(sectionGroup))
            break;
        insertIndex = index;
    }

    sections.insert(insertIndex, section);
}

static IniDocument standardizeSystemSectionOrder(const IniDocument &sections)
{
    IniDocument ordered;
    ordered.reserve(sections.size());
    for (const IniSection &section : sections)
        insertSectionByStandardOrder(ordered, section);
    return ordered;
}

static bool hasDifferentSectionSequence(const IniDocument &lhs, const IniDocument &rhs)
{
    if (lhs.size() != rhs.size())
        return true;
    for (int index = 0; index < lhs.size(); ++index) {
        if (sectionIdentityKey(lhs[index]) != sectionIdentityKey(rhs[index]))
            return true;
    }
    return false;
}

struct RawSectionBlock {
    IniSection section;
    QString text;
};

static QString normalizeLineEndings(const QString &text)
{
    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QChar('\r'), QChar('\n'));
    return normalized;
}

static QVector<RawSectionBlock> extractRawSectionBlocks(const QString &rawText,
                                                        const IniDocument &sections,
                                                        const IniAnalysisResult &analysis,
                                                        QString *outTrailingText = nullptr)
{
    QVector<RawSectionBlock> blocks;
    if (sections.isEmpty() || analysis.sections.size() != sections.size()) {
        if (outTrailingText)
            *outTrailingText = rawText;
        return blocks;
    }

    const QString normalizedText = normalizeLineEndings(rawText);
    const QStringList lines = normalizedText.split(QLatin1Char('\n'));
    blocks.reserve(sections.size());

    int nextBlockStartLine = 1;
    for (int index = 0; index < sections.size(); ++index) {
        const IniSectionInfo &info = analysis.sections.at(index);
        const int blockStartLine = qMax(1, nextBlockStartLine);
        const int blockEndLine = qMin(info.endLine, lines.size());
        QStringList blockLines;
        for (int line = blockStartLine; line <= blockEndLine; ++line)
            blockLines.append(lines.at(line - 1));

        RawSectionBlock block;
        block.section = sections.at(index);
        block.text = blockLines.join(QLatin1Char('\n'));
        blocks.append(block);
        nextBlockStartLine = blockEndLine + 1;
    }

    QStringList trailingLines;
    for (int line = qMax(1, nextBlockStartLine); line <= lines.size(); ++line)
        trailingLines.append(lines.at(line - 1));
    if (outTrailingText)
        *outTrailingText = trailingLines.join(QLatin1Char('\n'));

    return blocks;
}

static QString rebuildRawTextWithStandardOrder(const QVector<RawSectionBlock> &blocks,
                                               const QString &trailingText)
{
    if (blocks.isEmpty())
        return trailingText;

    IniDocument sections;
    sections.reserve(blocks.size());
    for (const RawSectionBlock &block : blocks)
        sections.append(block.section);
    const IniDocument orderedSections = standardizeSystemSectionOrder(sections);

    QHash<QString, QVector<int>> blockIndicesByKey;
    blockIndicesByKey.reserve(blocks.size());
    for (int index = 0; index < blocks.size(); ++index)
        blockIndicesByKey[sectionIdentityKey(blocks[index].section)].append(index);

    QString rebuilt;
    for (const IniSection &orderedSection : orderedSections) {
        const QString key = sectionIdentityKey(orderedSection);
        auto it = blockIndicesByKey.find(key);
        if (it == blockIndicesByKey.end() || it->isEmpty())
            continue;
        const QString chunk = blocks[it->takeFirst()].text;
        if (!rebuilt.isEmpty() && !rebuilt.endsWith(QLatin1Char('\n')) && !chunk.startsWith(QLatin1Char('\n')))
            rebuilt.append(QLatin1Char('\n'));
        rebuilt.append(chunk);
    }

    if (!trailingText.isEmpty()) {
        if (!rebuilt.isEmpty() && !rebuilt.endsWith(QLatin1Char('\n')) && !trailingText.startsWith(QLatin1Char('\n')))
            rebuilt.append(QLatin1Char('\n'));
        rebuilt.append(trailingText);
    }
    return rebuilt;
}

static IniDocument buildCurrentSections(const SystemDocument &doc,
                                        const IniDocument &extras,
                                        const IniSection &systemInfoSection)
{
    IniDocument sections;
    sections.reserve(1 + doc.objects().size() + doc.zones().size() + extras.size());
    sections.append(buildSystemInfo(doc, systemInfoSection));
    for (const IniSection &section : extras)
        sections.append(section);
    for (const auto &obj : doc.objects())
        sections.append(SystemPersistence::serializeObjectSection(*obj));
    for (const auto &zone : doc.zones())
        sections.append(SystemPersistence::serializeZoneSection(*zone));
    return sections;
}

static IniDocument mergeSectionsWithLayout(const IniDocument &layoutSections,
                                           const IniDocument &currentSections)
{
    if (layoutSections.isEmpty())
        return standardizeSystemSectionOrder(currentSections);

    QHash<QString, QVector<int>> availableByKey;
    availableByKey.reserve(currentSections.size());
    for (int index = 0; index < currentSections.size(); ++index)
        availableByKey[sectionIdentityKey(currentSections[index])].append(index);

    QVector<bool> consumed(currentSections.size(), false);
    IniDocument merged;
    merged.reserve(currentSections.size());

    for (const IniSection &layoutSection : layoutSections) {
        const QString key = sectionIdentityKey(layoutSection);
        auto it = availableByKey.find(key);
        if (it == availableByKey.end() || it->isEmpty())
            continue;
        const int currentIndex = it->takeFirst();
        if (currentIndex < 0 || currentIndex >= currentSections.size() || consumed[currentIndex])
            continue;
        consumed[currentIndex] = true;
        merged.append(currentSections[currentIndex]);
    }

    for (int index = 0; index < currentSections.size(); ++index) {
        if (consumed[index])
            continue;
        insertSectionByStandardOrder(merged, currentSections[index]);
    }

    return merged;
}

static void setOptionalIntEntry(QVector<IniEntry> &entries, const QString &key, int value)
{
    if (value == 0) {
        removeEntry(entries, key);
        return;
    }
    const int existingIndex = findEntryIndex(entries, key);
    if (existingIndex >= 0) {
        bool ok = false;
        const int parsed = entries[existingIndex].second.trimmed().toInt(&ok);
        if (ok && parsed == value)
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
    const int existingIndex = findEntryIndex(entries, key);
    if (existingIndex >= 0) {
        bool ok = false;
        const float parsed = entries[existingIndex].second.trimmed().toFloat(&ok);
        if (ok && qFuzzyCompare(parsed + 1.0f, value + 1.0f))
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
    const int existingIndex = findEntryIndex(entries, key);
    if (existingIndex >= 0) {
        const QVector3D parsed = parseVec3(entries[existingIndex].second);
        if (vec3Equals(parsed, value))
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

        // Resolve the human-readable system name via `ids_name` / `strid_name`
        // in universe.ini and the Freelancer IDS string table (e.g. 196609
        // → "New York"). This is purely for UI display — the serialiser never
        // writes it back to the system .ini.
        int idsName = 0;
        int stridName = 0;
        bool idsOk = false;
        bool stridOk = false;
        const int parsedIds = section.value(QStringLiteral("ids_name")).toInt(&idsOk);
        const int parsedStrid = section.value(QStringLiteral("strid_name")).toInt(&stridOk);
        if (idsOk)
            idsName = parsedIds;
        if (stridOk)
            stridName = parsedStrid;

        if (idsName > 0 || stridName > 0) {
            const QString gamePath = flatlas::core::EditingContext::instance().primaryGamePath();
            if (!gamePath.trimmed().isEmpty()) {
                const QString exeDir = flatlas::core::PathUtils::ciResolvePath(gamePath, QStringLiteral("EXE"));
                const QString lookupDir = exeDir.isEmpty() ? gamePath : exeDir;
                flatlas::infrastructure::IdsStringTable ids;
                ids.loadFromFreelancerDir(lookupDir);
                QString resolved;
                if (idsName > 0)
                    resolved = ids.getString(idsName).trimmed();
                if (resolved.isEmpty() && stridName > 0)
                    resolved = ids.getString(stridName).trimmed();
                if (!resolved.isEmpty())
                    doc->setDisplayName(resolved);
            }
        }
        return;
    }
}

// ─── load ─────────────────────────────────────────────────────────────────────

std::unique_ptr<SystemDocument> SystemPersistence::load(const QString &filePath)
{
    const IniDocument ini = IniParser::parseFile(filePath);
    if (ini.isEmpty())
        return nullptr;
    const QVector<QString> zoneComments = extractLeadingZoneComments(filePath);

    auto doc = std::make_unique<SystemDocument>();
    doc->setFilePath(filePath);

    IniDocument extras;

    int zoneIndex = 0;
    for (const IniSection &sec : ini) {
        const QString sectionName = sec.name.toLower();

        // ── SystemInfo ──────────────────────────────────────────────
        if (sectionName == QLatin1String("systeminfo")) {
            s_systemInfoSections[doc.get()] = sec;
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
            if (zoneIndex < zoneComments.size() && !zoneComments[zoneIndex].trimmed().isEmpty())
                zone->setComment(zoneComments[zoneIndex].trimmed());
            doc->addZone(std::move(zone));
            ++zoneIndex;
            continue;
        }

        // ── everything else → extras ────────────────────────────────
        extras.append(sec);
    }

    s_extras[doc.get()] = extras;
    s_layoutSections[doc.get()] = ini;
    s_nonStandardOrder[doc.get()] = hasDifferentSectionSequence(ini, standardizeSystemSectionOrder(ini));
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

static IniSection buildSystemInfo(const SystemDocument &doc, const IniSection &baseSection)
{
    IniSection sec = baseSection;
    sec.name = QStringLiteral("SystemInfo");
    const QString nickname = doc.name().trimmed();
    if (!nickname.isEmpty()) {
        const int nicknameIndex = findEntryIndex(sec.entries, QStringLiteral("nickname"));
        if (nicknameIndex < 0 || sec.entries[nicknameIndex].second.trimmed() != nickname)
            upsertEntry(sec.entries, QStringLiteral("nickname"), nickname);
    }

    const int navIndex = findEntryIndex(sec.entries, QStringLiteral("NavMapScale"));
    if (navIndex >= 0) {
        bool ok = false;
        const double parsed = sec.entries[navIndex].second.trimmed().toDouble(&ok);
        if (!(ok && qFuzzyCompare(parsed + 1.0, doc.navMapScale() + 1.0))) {
            upsertEntry(sec.entries, QStringLiteral("NavMapScale"),
                        QString::number(doc.navMapScale(), 'f', 6));
        }
    } else {
        upsertEntry(sec.entries, QStringLiteral("NavMapScale"),
                    QString::number(doc.navMapScale(), 'f', 6));
    }
    return sec;
}

IniSection SystemPersistence::serializeObjectSection(const SolarObject &obj)
{
    IniSection sec;
    sec.name = QStringLiteral("Object");
    sec.entries = obj.rawEntries();

    const QString nickname = obj.nickname().trimmed();
    if (!nickname.isEmpty()) {
        const int nicknameIndex = findEntryIndex(sec.entries, QStringLiteral("nickname"));
        if (nicknameIndex < 0 || sec.entries[nicknameIndex].second.trimmed() != nickname)
            upsertEntry(sec.entries, QStringLiteral("nickname"), nickname);
    }
    setOptionalIntEntry(sec.entries, QStringLiteral("ids_name"), obj.idsName());
    setOptionalIntEntry(sec.entries, QStringLiteral("ids_info"), obj.idsInfo());
    setOptionalVec3Entry(sec.entries, QStringLiteral("pos"), obj.position());
    const int rotateIndex = findEntryIndex(sec.entries, QStringLiteral("rotate"));
    const bool preserveExplicitZeroRotate = rotateIndex >= 0
        && obj.rotation().isNull()
        && vec3Equals(parseVec3(sec.entries[rotateIndex].second), QVector3D());
    if (!preserveExplicitZeroRotate)
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

    const QString nickname = zone.nickname().trimmed();
    if (!nickname.isEmpty()) {
        const int nicknameIndex = findEntryIndex(sec.entries, QStringLiteral("nickname"));
        if (nicknameIndex < 0 || sec.entries[nicknameIndex].second.trimmed() != nickname)
            upsertEntry(sec.entries, QStringLiteral("nickname"), nickname);
    }
    setOptionalVec3Entry(sec.entries, QStringLiteral("pos"), zone.position());
    if (!zone.size().isNull()) {
        const int sizeIndex = findEntryIndex(sec.entries, QStringLiteral("size"));
        if (sizeIndex >= 0) {
            const QVector3D parsed = parseZoneSize(sec.entries[sizeIndex].second);
            if (!vec3Equals(parsed, zone.size()))
                upsertEntry(sec.entries, QStringLiteral("size"), vec3ToString(zone.size()));
        } else {
            upsertEntry(sec.entries, QStringLiteral("size"), vec3ToString(zone.size()));
        }
    } else {
        removeEntry(sec.entries, QStringLiteral("size"));
    }
    setOptionalVec3Entry(sec.entries, QStringLiteral("rotate"), zone.rotation());
    const int shapeIndex = findEntryIndex(sec.entries, QStringLiteral("shape"));
    if (shapeIndex >= 0) {
        if (parseShape(sec.entries[shapeIndex].second) != zone.shape())
            upsertEntry(sec.entries, QStringLiteral("shape"), shapeToString(zone.shape()));
    } else {
        upsertEntry(sec.entries, QStringLiteral("shape"), shapeToString(zone.shape()));
    }
    setOptionalEntry(sec.entries, QStringLiteral("zone_type"), zone.zoneType());
    setOptionalEntry(sec.entries, QStringLiteral("usage"), zone.usage());
    setOptionalEntry(sec.entries, QStringLiteral("pop_type"), zone.popType());
    setOptionalEntry(sec.entries, QStringLiteral("path_label"), zone.pathLabel());
    if (!zone.comment().trimmed().isEmpty() && findEntryIndex(sec.entries, QStringLiteral("comment")) < 0)
        removeEntry(sec.entries, QStringLiteral("comment"));
    setOptionalVec3Entry(sec.entries, QStringLiteral("tightness"), zone.tightnessXyz());
    setOptionalIntEntry(sec.entries, QStringLiteral("damage"), zone.damage());
    setOptionalFloatEntry(sec.entries, QStringLiteral("interference"), zone.interference());
    setOptionalFloatEntry(sec.entries, QStringLiteral("drag_modifier"), zone.dragScale());
    const int sortIndex = findEntryIndex(sec.entries, QStringLiteral("sort"));
    if (zone.sortKey() != 0) {
        setOptionalIntEntry(sec.entries, QStringLiteral("sort"), zone.sortKey());
    } else if (sortIndex >= 0) {
        bool okInt = false;
        sec.entries[sortIndex].second.trimmed().toInt(&okInt);
        if (okInt || sec.entries[sortIndex].second.trimmed().isEmpty())
            removeEntry(sec.entries, QStringLiteral("sort"));
    }

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
    const IniDocument extras = s_extras.value(&doc);
    const IniSection systemInfoSection = s_systemInfoSections.value(&doc);
    const IniDocument currentSections = buildCurrentSections(doc, extras, systemInfoSection);
    const IniDocument layoutSections = s_layoutSections.value(&doc);
    const IniDocument orderedSections = mergeSectionsWithLayout(layoutSections, currentSections);
    const QString text = serializeToText(doc);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << text;
    if (!orderedSections.isEmpty()
        && normalizedSectionName(orderedSections.first().name) == QStringLiteral("systeminfo")) {
        s_systemInfoSections[&doc] = orderedSections.first();
    }
    s_layoutSections[&doc] = orderedSections;
    s_nonStandardOrder[&doc] =
        hasDifferentSectionSequence(orderedSections, standardizeSystemSectionOrder(orderedSections));
    return true;
}

bool SystemPersistence::save(const SystemDocument &doc)
{
    return save(doc, doc.filePath());
}

QString SystemPersistence::serializeToText(const SystemDocument &doc)
{
    const IniDocument extras = s_extras.value(&doc);
    const IniSection systemInfoSection = s_systemInfoSections.value(&doc);
    const IniDocument currentSections = buildCurrentSections(doc, extras, systemInfoSection);
    const IniDocument layoutSections = s_layoutSections.value(&doc);
    const IniDocument orderedSections = mergeSectionsWithLayout(layoutSections, currentSections);

    QString text;
    for (const IniSection &section : orderedSections) {
        QString leadingComment;
        if (normalizedSectionName(section.name) == QStringLiteral("zone")) {
            if (findEntryIndex(section.entries, QStringLiteral("comment")) < 0) {
                const QString zoneNickname = section.value(QStringLiteral("nickname")).trimmed();
                for (const auto &zone : doc.zones()) {
                    if (zone && zone->nickname().compare(zoneNickname, Qt::CaseInsensitive) == 0) {
                        leadingComment = zone->comment();
                        break;
                    }
                }
            }
        }
        appendSerializedSection(text, section, leadingComment);
    }
    return text;
}

// ─── extras management ────────────────────────────────────────────────────────

IniDocument SystemPersistence::extraSections(const SystemDocument *doc)
{
    return s_extras.value(doc);
}

IniSection SystemPersistence::systemInfoSection(const SystemDocument *doc)
{
    return s_systemInfoSections.value(doc);
}

void SystemPersistence::setExtraSections(const SystemDocument *doc, const IniDocument &extras)
{
    if (!doc)
        return;
    s_extras.insert(doc, extras);
}

void SystemPersistence::setSystemInfoSection(const SystemDocument *doc, const IniSection &section)
{
    if (!doc)
        return;
    s_systemInfoSections.insert(doc, section);
}

void SystemPersistence::clearExtras(const SystemDocument *doc)
{
    s_extras.remove(doc);
    s_systemInfoSections.remove(doc);
    s_layoutSections.remove(doc);
    s_nonStandardOrder.remove(doc);
}

bool SystemPersistence::hasNonStandardSectionOrder(const SystemDocument *doc)
{
    return doc && s_nonStandardOrder.value(doc, false);
}

bool SystemPersistence::normalizeSectionOrder(const SystemDocument *doc)
{
    if (!doc)
        return false;

    const IniDocument currentSections = buildCurrentSections(*doc,
                                                             s_extras.value(doc),
                                                             s_systemInfoSections.value(doc));
    const IniDocument arrangedSections = mergeSectionsWithLayout(s_layoutSections.value(doc), currentSections);
    const IniDocument normalizedSections = standardizeSystemSectionOrder(arrangedSections);
    const bool changed = hasDifferentSectionSequence(arrangedSections, normalizedSections);
    s_layoutSections.insert(doc, normalizedSections);
    s_nonStandardOrder.insert(doc, false);
    return changed;
}

bool SystemPersistence::normalizeSectionOrderInFile(const QString &filePath,
                                                    bool *changed,
                                                    QString *errorMessage)
{
    if (changed)
        *changed = false;
    if (filePath.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Kein Dateipfad angegeben.");
        return false;
    }

    bool wasBini = false;
    const QString rawText = IniAnalysisService::loadIniLikeText(filePath, &wasBini);
    if (rawText.isEmpty()) {
        QFileInfo info(filePath);
        if (!info.exists() || info.size() == 0) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Die System-INI konnte nicht gelesen werden.");
            return false;
        }
    }
    if (wasBini) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Die Section-Reihenfolge kann fuer BINI-Dateien nicht direkt angepasst werden.");
        return false;
    }

    const IniDocument sections = IniParser::parseText(rawText);
    const IniAnalysisResult analysis = IniAnalysisService::analyzeText(rawText);
    if (sections.size() != analysis.sections.size()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Die System-INI konnte nicht sicher in rohe Section-Bloecke aufgeteilt werden.");
        return false;
    }

    const IniDocument orderedSections = standardizeSystemSectionOrder(sections);
    const bool needsChange = hasDifferentSectionSequence(sections, orderedSections);
    if (changed)
        *changed = needsChange;
    if (!needsChange)
        return true;

    QString trailingText;
    const QVector<RawSectionBlock> blocks = extractRawSectionBlocks(rawText, sections, analysis, &trailingText);
    if (blocks.size() != sections.size()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Die rohen Section-Bloecke konnten nicht vollstaendig extrahiert werden.");
        return false;
    }

    const QString reorderedText = rebuildRawTextWithStandardOrder(blocks, trailingText);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Die sortierte System-INI konnte nicht geschrieben werden.");
        return false;
    }
    file.write(reorderedText.toUtf8());
    return true;
}

} // namespace flatlas::editors
