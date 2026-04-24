#include "TradeLaneEditService.h"

#include "domain/SolarObject.h"
#include "domain/SystemDocument.h"

#include <QRegularExpression>

#include <cmath>

namespace flatlas::editors {

namespace {

QString rawEntryValue(const flatlas::domain::SolarObject &obj, const QString &key)
{
    const auto entries = obj.rawEntries();
    for (int index = entries.size() - 1; index >= 0; --index) {
        if (entries[index].first.compare(key, Qt::CaseInsensitive) == 0)
            return entries[index].second.trimmed();
    }
    return {};
}

int rawEntryIntValue(const flatlas::domain::SolarObject &obj, const QString &key, int fallback = 0)
{
    bool ok = false;
    const int value = rawEntryValue(obj, key).toInt(&ok);
    return ok ? value : fallback;
}

bool isTradeLaneObject(const std::shared_ptr<flatlas::domain::SolarObject> &obj)
{
    if (!obj)
        return false;
    if (obj->type() == flatlas::domain::SolarObject::TradeLane)
        return true;

    const QString archetype = obj->archetype().trimmed().toLower();
    if (archetype.contains(QLatin1String("trade_lane_ring")))
        return true;

    return !rawEntryValue(*obj, QStringLiteral("prev_ring")).isEmpty()
        || !rawEntryValue(*obj, QStringLiteral("next_ring")).isEmpty();
}

QHash<QString, std::shared_ptr<flatlas::domain::SolarObject>> tradeLaneMap(
    const flatlas::domain::SystemDocument &document)
{
    QHash<QString, std::shared_ptr<flatlas::domain::SolarObject>> map;
    for (const auto &obj : document.objects()) {
        if (!isTradeLaneObject(obj))
            continue;
        map.insert(obj->nickname().trimmed().toLower(), obj);
    }
    return map;
}

bool parseTradeLaneNickname(const QString &nickname, QString *outSystemNickname, int *outNumber)
{
    static const QRegularExpression kPattern(
        QStringLiteral("^(.*)_Trade_Lane_Ring_(\\d+)$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = kPattern.match(nickname.trimmed());
    if (!match.hasMatch())
        return false;
    if (outSystemNickname)
        *outSystemNickname = match.captured(1).trimmed();
    if (outNumber)
        *outNumber = match.captured(2).toInt();
    return true;
}

double distanceMeters(const QVector3D &lhs, const QVector3D &rhs)
{
    const double dx = static_cast<double>(rhs.x() - lhs.x());
    const double dz = static_cast<double>(rhs.z() - lhs.z());
    return std::hypot(dx, dz);
}

double tradeLaneYawDegrees(const QVector3D &startPosition, const QVector3D &endPosition)
{
    const double dx = static_cast<double>(endPosition.x() - startPosition.x());
    const double dz = static_cast<double>(endPosition.z() - startPosition.z());
    double angleDeg = std::atan2(dx, dz) * (180.0 / M_PI) + 180.0;
    if (angleDeg > 180.0)
        angleDeg -= 360.0;
    return angleDeg;
}

QVector<QPair<QString, QString>> filteredPreservedEntries(const flatlas::domain::SolarObject &obj)
{
    static const QSet<QString> kControlledKeys = {
        QStringLiteral("nickname"),
        QStringLiteral("pos"),
        QStringLiteral("rotate"),
        QStringLiteral("archetype"),
        QStringLiteral("ids_name"),
        QStringLiteral("ids_info"),
        QStringLiteral("prev_ring"),
        QStringLiteral("next_ring"),
        QStringLiteral("tradelane_space_name"),
        QStringLiteral("behavior"),
        QStringLiteral("difficulty_level"),
        QStringLiteral("loadout"),
        QStringLiteral("pilot"),
        QStringLiteral("reputation"),
    };

    QVector<QPair<QString, QString>> preserved;
    preserved.reserve(obj.rawEntries().size());
    for (const auto &entry : obj.rawEntries()) {
        if (!kControlledKeys.contains(entry.first.trimmed().toLower()))
            preserved.append(entry);
    }
    return preserved;
}

QVector<QPair<QString, QString>> buildRawEntries(const QVector<QPair<QString, QString>> &preservedEntries,
                                                 const QString &nickname,
                                                 const QVector3D &position,
                                                 double yawDegrees,
                                                 const TradeLaneEditRequest &request,
                                                 int index)
{
    QVector<QPair<QString, QString>> entries = preservedEntries;
    entries.append({QStringLiteral("nickname"), nickname});
    entries.append({QStringLiteral("pos"),
                    QStringLiteral("%1, 0, %2")
                        .arg(static_cast<double>(position.x()), 0, 'f', 0)
                        .arg(static_cast<double>(position.z()), 0, 'f', 0)});
    entries.append({QStringLiteral("rotate"),
                    QStringLiteral("0, %1, 0").arg(yawDegrees, 0, 'f', 0)});
    entries.append({QStringLiteral("archetype"), request.archetype.trimmed()});
    if (request.routeIdsName > 0)
        entries.append({QStringLiteral("ids_name"), QString::number(request.routeIdsName)});
    entries.append({QStringLiteral("ids_info"), QString::number(request.idsInfo)});
    if (index > 0) {
        entries.append({QStringLiteral("prev_ring"),
                        QStringLiteral("%1_Trade_Lane_Ring_%2")
                            .arg(request.systemNickname)
                            .arg(request.startNumber + index - 1)});
    }
    if (index < request.ringCount - 1) {
        entries.append({QStringLiteral("next_ring"),
                        QStringLiteral("%1_Trade_Lane_Ring_%2")
                            .arg(request.systemNickname)
                            .arg(request.startNumber + index + 1)});
    }
    if (index == 0 && request.startSpaceNameId > 0)
        entries.append({QStringLiteral("tradelane_space_name"), QString::number(request.startSpaceNameId)});
    else if (index == request.ringCount - 1 && request.endSpaceNameId > 0)
        entries.append({QStringLiteral("tradelane_space_name"), QString::number(request.endSpaceNameId)});
    entries.append({QStringLiteral("behavior"), request.behavior.trimmed().isEmpty() ? QStringLiteral("NOTHING") : request.behavior.trimmed()});
    entries.append({QStringLiteral("difficulty_level"), QString::number(request.difficultyLevel)});
    if (!request.loadout.trimmed().isEmpty())
        entries.append({QStringLiteral("loadout"), request.loadout.trimmed()});
    if (!request.pilot.trimmed().isEmpty())
        entries.append({QStringLiteral("pilot"), request.pilot.trimmed()});
    if (!request.reputation.trimmed().isEmpty())
        entries.append({QStringLiteral("reputation"), request.reputation.trimmed()});
    return entries;
}

} // namespace

std::optional<TradeLaneChain> TradeLaneEditService::detectChain(const flatlas::domain::SystemDocument &document,
                                                                const QString &selectedNickname,
                                                                QString *errorMessage)
{
    const QString selectedKey = selectedNickname.trimmed().toLower();
    const auto lanes = tradeLaneMap(document);
    if (!lanes.contains(selectedKey)) {
        if (errorMessage)
            *errorMessage = QObject::tr("Das ausgewaehlte Objekt gehoert zu keiner Trade Lane.");
        return std::nullopt;
    }

    auto current = lanes.value(selectedKey);
    QSet<QString> walked;
    while (current) {
        const QString currentKey = current->nickname().trimmed().toLower();
        if (walked.contains(currentKey)) {
            if (errorMessage)
                *errorMessage = QObject::tr("Die Trade Lane enthaelt eine zyklische prev_ring-Verkettung.");
            return std::nullopt;
        }
        walked.insert(currentKey);
        const QString prevKey = rawEntryValue(*current, QStringLiteral("prev_ring")).toLower();
        if (prevKey.isEmpty() || !lanes.contains(prevKey))
            break;
        current = lanes.value(prevKey);
    }

    TradeLaneChain chain;
    QSet<QString> seen;
    while (current) {
        const QString currentKey = current->nickname().trimmed().toLower();
        if (seen.contains(currentKey)) {
            if (errorMessage)
                *errorMessage = QObject::tr("Die Trade Lane enthaelt eine zyklische next_ring-Verkettung.");
            return std::nullopt;
        }
        seen.insert(currentKey);
        chain.rings.append(current);

        const QString nextKey = rawEntryValue(*current, QStringLiteral("next_ring")).toLower();
        if (nextKey.isEmpty())
            break;
        if (!lanes.contains(nextKey)) {
            if (errorMessage)
                *errorMessage = QObject::tr("Die Trade Lane referenziert einen fehlenden Ring '%1'.").arg(nextKey);
            return std::nullopt;
        }
        current = lanes.value(nextKey);
    }

    if (chain.rings.size() < 2) {
        if (errorMessage)
            *errorMessage = QObject::tr("Eine Trade Lane muss aus mindestens zwei Ringen bestehen.");
        return std::nullopt;
    }

    QString parsedSystemNickname;
    int parsedStartNumber = 1;
    parseTradeLaneNickname(chain.rings.first()->nickname(), &parsedSystemNickname, &parsedStartNumber);
    chain.systemNickname = parsedSystemNickname;
    chain.startNumber = parsedStartNumber;
    chain.startPosition = chain.rings.first()->position();
    chain.endPosition = chain.rings.last()->position();

    const auto &firstRing = *chain.rings.first();
    chain.archetype = firstRing.archetype().trimmed().isEmpty() ? rawEntryValue(firstRing, QStringLiteral("archetype"))
                                                                : firstRing.archetype().trimmed();
    chain.loadout = firstRing.loadout().trimmed().isEmpty() ? rawEntryValue(firstRing, QStringLiteral("loadout"))
                                                            : firstRing.loadout().trimmed();
    chain.reputation = rawEntryValue(firstRing, QStringLiteral("reputation"));
    chain.behavior = rawEntryValue(firstRing, QStringLiteral("behavior"));
    if (chain.behavior.isEmpty())
        chain.behavior = QStringLiteral("NOTHING");
    chain.pilot = rawEntryValue(firstRing, QStringLiteral("pilot"));
    if (chain.pilot.isEmpty())
        chain.pilot = QStringLiteral("pilot_solar_easiest");
    chain.difficultyLevel = rawEntryIntValue(firstRing, QStringLiteral("difficulty_level"), 1);
    chain.idsInfo = firstRing.idsInfo() > 0 ? firstRing.idsInfo() : rawEntryIntValue(firstRing, QStringLiteral("ids_info"), 66170);

    for (const auto &ring : chain.rings) {
        if (!ring)
            continue;
        const int routeIds = ring->idsName() > 0 ? ring->idsName() : rawEntryIntValue(*ring, QStringLiteral("ids_name"), 0);
        if (routeIds > 0 && chain.routeIdsName == 0)
            chain.routeIdsName = routeIds;
        const int endpointIds = rawEntryIntValue(*ring, QStringLiteral("tradelane_space_name"), 0);
        if (endpointIds > 0 && chain.startSpaceNameId == 0)
            chain.startSpaceNameId = endpointIds;
    }
    for (auto it = chain.rings.crbegin(); it != chain.rings.crend(); ++it) {
        if (!*it)
            continue;
        const int endpointIds = rawEntryIntValue(*(*it), QStringLiteral("tradelane_space_name"), 0);
        if (endpointIds > 0) {
            chain.endSpaceNameId = endpointIds;
            break;
        }
    }

    return chain;
}

QStringList TradeLaneEditService::memberNicknames(const TradeLaneChain &chain)
{
    QStringList nicknames;
    nicknames.reserve(chain.rings.size());
    for (const auto &ring : chain.rings) {
        if (ring)
            nicknames.append(ring->nickname());
    }
    return nicknames;
}

double TradeLaneEditService::laneLengthMeters(const TradeLaneChain &chain)
{
    if (chain.rings.size() < 2)
        return 0.0;
    return distanceMeters(chain.startPosition, chain.endPosition);
}

double TradeLaneEditService::derivedSpacingMeters(const TradeLaneChain &chain)
{
    if (chain.rings.size() < 2)
        return 0.0;
    return laneLengthMeters(chain) / static_cast<double>(chain.rings.size() - 1);
}

bool TradeLaneEditService::buildReplacementObjects(const TradeLaneChain &existingChain,
                                                   const TradeLaneEditRequest &request,
                                                   const QSet<QString> &occupiedNicknames,
                                                   QVector<std::shared_ptr<flatlas::domain::SolarObject>> *outObjects,
                                                   QString *errorMessage)
{
    if (!outObjects)
        return false;
    if (request.systemNickname.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("Der System-Nickname fuer die Trade Lane fehlt.");
        return false;
    }
    if (request.ringCount < 2) {
        if (errorMessage)
            *errorMessage = QObject::tr("Eine Trade Lane muss aus mindestens zwei Ringen bestehen.");
        return false;
    }
    if (distanceMeters(request.startPosition, request.endPosition) <= 0.001) {
        if (errorMessage)
            *errorMessage = QObject::tr("Start- und Endpunkt der Trade Lane sind ungueltig.");
        return false;
    }
    if (request.archetype.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("Bitte waehle einen gueltigen Trade-Lane-Archetype.");
        return false;
    }
    if (request.loadout.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("Bitte waehle ein gueltiges Loadout fuer die Trade Lane.");
        return false;
    }

    QVector<std::shared_ptr<flatlas::domain::SolarObject>> objects;
    objects.reserve(request.ringCount);

    QSet<QString> reservedNicknames = occupiedNicknames;
    for (int index = 0; index < request.ringCount; ++index) {
        const QString nickname = QStringLiteral("%1_Trade_Lane_Ring_%2")
                                     .arg(request.systemNickname)
                                     .arg(request.startNumber + index);
        const QString key = nickname.trimmed().toLower();
        if (reservedNicknames.contains(key)) {
            if (errorMessage) {
                *errorMessage = QObject::tr("Der Ring-Nickname '%1' existiert bereits ausserhalb der bearbeiteten Trade Lane.")
                                    .arg(nickname);
            }
            return false;
        }
        reservedNicknames.insert(key);

        const double t = request.ringCount <= 1
            ? 0.0
            : static_cast<double>(index) / static_cast<double>(request.ringCount - 1);
        const QVector3D position(
            static_cast<float>(request.startPosition.x()
                               + (request.endPosition.x() - request.startPosition.x()) * t),
            0.0f,
            static_cast<float>(request.startPosition.z()
                               + (request.endPosition.z() - request.startPosition.z()) * t));
        const double yawDegrees = tradeLaneYawDegrees(request.startPosition, request.endPosition);

        auto ring = std::make_shared<flatlas::domain::SolarObject>();
        ring->setNickname(nickname);
        ring->setType(flatlas::domain::SolarObject::TradeLane);
        ring->setArchetype(request.archetype.trimmed());
        ring->setPosition(position);
        ring->setRotation(QVector3D(0.0f, static_cast<float>(yawDegrees), 0.0f));
        ring->setIdsName(request.routeIdsName);
        ring->setIdsInfo(request.idsInfo);
        ring->setLoadout(request.loadout.trimmed());

        QVector<QPair<QString, QString>> preservedEntries;
        if (index < existingChain.rings.size() && existingChain.rings[index])
            preservedEntries = filteredPreservedEntries(*existingChain.rings[index]);
        ring->setRawEntries(buildRawEntries(preservedEntries, nickname, position, yawDegrees, request, index));
        objects.append(ring);
    }

    *outObjects = objects;
    return true;
}

} // namespace flatlas::editors