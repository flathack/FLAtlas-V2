#include "RingEditService.h"

#include "core/PathUtils.h"
#include "domain/SolarObject.h"
#include "domain/SystemDocument.h"
#include "domain/ZoneItem.h"
#include "infrastructure/parser/IniParser.h"

#include <QDir>
#include <QDirIterator>
#include <QHash>
#include <QList>
#include <QObject>
#include <QRegularExpression>
#include <QSet>
#include <QVector3D>

#include <algorithm>
#include <memory>

namespace flatlas::editors {
namespace {

using flatlas::domain::SolarObject;
using flatlas::domain::SystemDocument;
using flatlas::domain::ZoneItem;
using flatlas::infrastructure::IniParser;

QString rawEntryValue(const QVector<QPair<QString, QString>> &entries, const QString &key)
{
    for (int index = entries.size() - 1; index >= 0; --index) {
        if (entries[index].first.compare(key, Qt::CaseInsensitive) == 0)
            return entries[index].second.trimmed();
    }
    return {};
}

QString rawEntryValue(const SolarObject &object, const QString &key)
{
    return rawEntryValue(object.rawEntries(), key);
}

QString rawEntryValue(const ZoneItem &zone, const QString &key)
{
    return rawEntryValue(zone.rawEntries(), key);
}

QString formatNumeric(double value)
{
    if (qAbs(value - qRound64(value)) < 1e-6)
        return QString::number(qRound64(value));
    QString text = QString::number(value, 'f', 2);
    text.remove(QRegularExpression(QStringLiteral("0+$")));
    text.remove(QRegularExpression(QStringLiteral("\\.$")));
    return text;
}

QVector3D parseVec3(const QString &text)
{
    const QStringList parts = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (parts.size() < 3)
        return {};
    bool okX = false;
    bool okY = false;
    bool okZ = false;
    const float x = parts[0].trimmed().toFloat(&okX);
    const float y = parts[1].trimmed().toFloat(&okY);
    const float z = parts[2].trimmed().toFloat(&okZ);
    return (okX && okY && okZ) ? QVector3D(x, y, z) : QVector3D();
}

int resolvedSolarRadius(const SolarObject &object, const QString &gameRoot)
{
    const QString archetype = object.archetype().trimmed();
    if (!gameRoot.trimmed().isEmpty()) {
        const QString solarArchPath = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA/SOLAR/solararch.ini"));
        if (!solarArchPath.isEmpty()) {
            const auto document = IniParser::parseFile(solarArchPath);
            for (const auto &section : document) {
                if (section.name.compare(QStringLiteral("Solar"), Qt::CaseInsensitive) != 0)
                    continue;
                if (section.value(QStringLiteral("nickname")).trimmed().compare(archetype, Qt::CaseInsensitive) != 0)
                    continue;
                bool ok = false;
                const int radius = section.value(QStringLiteral("solar_radius")).trimmed().toInt(&ok);
                if (ok && radius > 0)
                    return radius;
                break;
            }
        }
    }

    static const QRegularExpression trailingNumber(QStringLiteral("_(\\d+(?:\\.\\d+)?)$"));
    const QRegularExpressionMatch match = trailingNumber.match(archetype);
    if (match.hasMatch()) {
        bool ok = false;
        const int radius = qRound(match.captured(1).toDouble(&ok));
        if (ok && radius > 0)
            return radius;
    }

    return 1500;
}

std::shared_ptr<ZoneItem> findZoneShared(const SystemDocument *document, const QString &nickname)
{
    if (!document)
        return {};
    for (const auto &zone : document->zones()) {
        if (zone && zone->nickname().compare(nickname, Qt::CaseInsensitive) == 0)
            return zone;
    }
    return {};
}

QString suggestedZoneNickname(const SystemDocument *document, const SolarObject &object)
{
    QString base = object.nickname().trimmed();
    if (base.isEmpty())
        base = QStringLiteral("object");
    base += QStringLiteral("_ring");

    QString candidate = base;
    int suffix = 2;
    while (findZoneShared(document, candidate))
        candidate = QStringLiteral("%1_%2").arg(base).arg(suffix++);
    return candidate;
}

RingEditState defaultStateFor(const SystemDocument *document,
                             const SolarObject &object,
                             const QString &gameRoot)
{
    RingEditState state;
    state.enabled = false;
    state.zoneNickname = suggestedZoneNickname(document, object);
    const int radius = resolvedSolarRadius(object, gameRoot);
    state.outerRadius = qMax(3000.0, static_cast<double>(radius) * 2.4);
    state.innerRadius = qMax(1000.0, static_cast<double>(radius) * 1.25);
    state.thickness = qMax(250.0, static_cast<double>(radius) * 0.18);
    if (state.innerRadius >= state.outerRadius)
        state.innerRadius = qMax(1.0, state.outerRadius * 0.5);
    return state;
}

QVector<QPair<QString, QString>> updatedObjectEntries(const SolarObject &object,
                                                      const RingEditState &state)
{
    QVector<QPair<QString, QString>> rewritten;
    rewritten.reserve(object.rawEntries().size() + 1);
    bool wroteRing = false;
    const QString ringValue = QStringLiteral("%1, %2").arg(state.zoneNickname, state.ringIni).trimmed();
    for (const auto &entry : object.rawEntries()) {
        if (entry.first.compare(QStringLiteral("ring"), Qt::CaseInsensitive) == 0) {
            if (state.enabled) {
                rewritten.append({entry.first, ringValue});
                wroteRing = true;
            }
            continue;
        }
        rewritten.append(entry);
    }
    if (state.enabled && !wroteRing)
        rewritten.append({QStringLiteral("ring"), ringValue});
    return rewritten;
}

QVector<QPair<QString, QString>> updatedZoneEntries(const ZoneItem *existingZone,
                                                    const SolarObject &hostObject,
                                                    const RingEditState &state)
{
    QVector<QPair<QString, QString>> entries = existingZone ? existingZone->rawEntries() : QVector<QPair<QString, QString>>{};
    const QString posValue = QStringLiteral("%1, %2, %3")
                                 .arg(formatNumeric(hostObject.position().x()))
                                 .arg(formatNumeric(hostObject.position().y()))
                                 .arg(formatNumeric(hostObject.position().z()));
    const QString rotateValue = QStringLiteral("%1, %2, %3")
                                    .arg(formatNumeric(state.rotateX))
                                    .arg(formatNumeric(state.rotateY))
                                    .arg(formatNumeric(state.rotateZ));
    const QString sizeValue = QStringLiteral("%1, %2, %3")
                                  .arg(formatNumeric(state.outerRadius))
                                  .arg(formatNumeric(state.innerRadius))
                                  .arg(formatNumeric(state.thickness));

    const QHash<QString, QString> replacements{
        {QStringLiteral("nickname"), state.zoneNickname},
        {QStringLiteral("pos"), posValue},
        {QStringLiteral("rotate"), rotateValue},
        {QStringLiteral("shape"), QStringLiteral("RING")},
        {QStringLiteral("size"), sizeValue},
        {QStringLiteral("sort"), state.sortValue.trimmed().isEmpty() ? QStringLiteral("99.500000") : state.sortValue.trimmed()},
    };

    QVector<QPair<QString, QString>> rewritten;
    rewritten.reserve(entries.size() + replacements.size());
    QSet<QString> seen;
    for (const auto &entry : entries) {
        const QString lowered = entry.first.trimmed().toLower();
        const auto it = replacements.constFind(lowered);
        if (it != replacements.constEnd()) {
            rewritten.append({entry.first, it.value()});
            seen.insert(lowered);
        } else {
            rewritten.append(entry);
        }
    }
    for (auto it = replacements.constBegin(); it != replacements.constEnd(); ++it) {
        if (!seen.contains(it.key()))
            rewritten.append({it.key(), it.value()});
    }
    return rewritten;
}

} // namespace

RingEditOptions RingEditService::loadOptions(const QString &gameRoot)
{
    RingEditOptions options;
    const QString ringsDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA/SOLAR/RINGS"));
    if (ringsDir.isEmpty())
        return options;

    QDirIterator it(ringsDir, QStringList{QStringLiteral("*.ini")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString absolutePath = it.next();
        const QString relative = QDir(ringsDir).relativeFilePath(absolutePath);
        const QString normalized = QStringLiteral("solar\\rings\\%1")
                                       .arg(QDir::fromNativeSeparators(relative).replace(QLatin1Char('/'), QLatin1Char('\\')));
        options.ringPresets.append(normalized);
    }
    options.ringPresets.removeDuplicates();
    std::sort(options.ringPresets.begin(), options.ringPresets.end(), [](const QString &lhs, const QString &rhs) {
        return lhs.compare(rhs, Qt::CaseInsensitive) < 0;
    });
    return options;
}

bool RingEditService::canHostRing(const SolarObject &object)
{
    if (hasRing(object))
        return true;
    if (object.type() == SolarObject::Planet || object.type() == SolarObject::Sun)
        return true;
    const QString archetype = object.archetype().trimmed();
    return archetype.contains(QStringLiteral("planet"), Qt::CaseInsensitive)
        || archetype.contains(QStringLiteral("sun"), Qt::CaseInsensitive)
        || archetype.contains(QStringLiteral("star"), Qt::CaseInsensitive);
}

bool RingEditService::hasRing(const SolarObject &object)
{
    return !linkedRingIniPath(object).isEmpty();
}

QString RingEditService::linkedZoneNickname(const SolarObject &object)
{
    const QString raw = rawEntryValue(object, QStringLiteral("ring"));
    if (raw.isEmpty())
        return {};
    const QStringList parts = raw.split(QLatin1Char(','), Qt::SkipEmptyParts);
    return parts.isEmpty() ? QString() : parts.first().trimmed();
}

QString RingEditService::linkedRingIniPath(const SolarObject &object)
{
    const QString raw = rawEntryValue(object, QStringLiteral("ring"));
    if (raw.isEmpty())
        return {};
    const QStringList parts = raw.split(QLatin1Char(','), Qt::SkipEmptyParts);
    return parts.size() >= 2 ? parts.last().trimmed() : QString();
}

bool RingEditService::isValidZoneNickname(const QString &nickname)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_]+$"));
    return pattern.match(nickname.trimmed()).hasMatch();
}

RingEditState RingEditService::loadState(const SystemDocument *document,
                                        const SolarObject &object,
                                        const QString &gameRoot)
{
    RingEditState state = defaultStateFor(document, object, gameRoot);
    state.ringIni = linkedRingIniPath(object);
    const QString zoneNickname = linkedZoneNickname(object);
    if (!zoneNickname.isEmpty())
        state.zoneNickname = zoneNickname;
    state.enabled = !state.ringIni.isEmpty();

    const auto zone = findZoneShared(document, state.zoneNickname);
    if (!zone)
        return state;

    const QVector3D size = zone->size();
    if (size.x() > 0.0f)
        state.outerRadius = size.x();
    if (size.y() > 0.0f)
        state.innerRadius = size.y();
    if (size.z() > 0.0f)
        state.thickness = size.z();

    const QVector3D rotation = zone->rotation().isNull() ? parseVec3(rawEntryValue(*zone, QStringLiteral("rotate"))) : zone->rotation();
    state.rotateX = rotation.x();
    state.rotateY = rotation.y();
    state.rotateZ = rotation.z();

    const QString sortValue = rawEntryValue(*zone, QStringLiteral("sort"));
    if (!sortValue.trimmed().isEmpty())
        state.sortValue = sortValue.trimmed();

    return state;
}

SolarObject *RingEditService::findHostForZone(const SystemDocument *document, const QString &zoneNickname)
{
    if (!document)
        return nullptr;
    const QString needle = zoneNickname.trimmed();
    if (needle.isEmpty())
        return nullptr;
    for (const auto &object : document->objects()) {
        if (object && linkedZoneNickname(*object).compare(needle, Qt::CaseInsensitive) == 0)
            return object.get();
    }
    return nullptr;
}

bool RingEditService::apply(SystemDocument *document,
                           SolarObject *object,
                           const RingEditState &state,
                           QString *errorMessage)
{
    if (!document || !object) {
        if (errorMessage)
            *errorMessage = QObject::tr("Kein System oder Host-Objekt fuer den Ring-Workflow verfuegbar.");
        return false;
    }
    if (!canHostRing(*object)) {
        if (errorMessage)
            *errorMessage = QObject::tr("Ringe koennen nur an Planeten oder Sonnen angehaengt werden.");
        return false;
    }

    RingEditState sanitized = state;
    sanitized.zoneNickname = sanitized.zoneNickname.trimmed();
    sanitized.ringIni = sanitized.ringIni.trimmed();
    sanitized.sortValue = sanitized.sortValue.trimmed();

    const QString currentZoneNickname = linkedZoneNickname(*object);
    if (sanitized.enabled) {
        if (sanitized.ringIni.isEmpty()) {
            if (errorMessage)
                *errorMessage = QObject::tr("Bitte eine Ring-INI angeben.");
            return false;
        }
        if (sanitized.zoneNickname.isEmpty() || !isValidZoneNickname(sanitized.zoneNickname)) {
            if (errorMessage)
                *errorMessage = QObject::tr("Bitte einen gueltigen Zone-Nickname angeben.");
            return false;
        }
        if (sanitized.innerRadius <= 0.0 || sanitized.outerRadius <= sanitized.innerRadius) {
            if (errorMessage)
                *errorMessage = QObject::tr("Inner Radius muss groesser als 0 und kleiner als Outer Radius sein.");
            return false;
        }
        if (sanitized.thickness <= 0.0) {
            if (errorMessage)
                *errorMessage = QObject::tr("Die Ring-Dicke muss groesser als 0 sein.");
            return false;
        }
        const auto conflictingZone = findZoneShared(document, sanitized.zoneNickname);
        if (conflictingZone
            && conflictingZone->nickname().compare(currentZoneNickname, Qt::CaseInsensitive) != 0) {
            if (errorMessage)
                *errorMessage = QObject::tr("Die Zone '%1' existiert bereits.").arg(sanitized.zoneNickname);
            return false;
        }
    }

    object->setRawEntries(updatedObjectEntries(*object, sanitized));

    if (!currentZoneNickname.isEmpty()
        && (!sanitized.enabled || currentZoneNickname.compare(sanitized.zoneNickname, Qt::CaseInsensitive) != 0)) {
        if (const auto currentZone = findZoneShared(document, currentZoneNickname))
            document->removeZone(currentZone);
    }

    if (sanitized.enabled) {
        auto zone = findZoneShared(document, sanitized.zoneNickname);
        if (!zone) {
            zone = std::make_shared<ZoneItem>();
            document->addZone(zone);
        }
        zone->setNickname(sanitized.zoneNickname);
        zone->setPosition(object->position());
        zone->setRotation(QVector3D(static_cast<float>(sanitized.rotateX),
                                    static_cast<float>(sanitized.rotateY),
                                    static_cast<float>(sanitized.rotateZ)));
        zone->setShape(ZoneItem::Ring);
        zone->setSize(QVector3D(static_cast<float>(sanitized.outerRadius),
                                static_cast<float>(sanitized.innerRadius),
                                static_cast<float>(sanitized.thickness)));
        zone->setRawEntries(updatedZoneEntries(zone.get(), *object, sanitized));
    }

    return true;
}

} // namespace flatlas::editors