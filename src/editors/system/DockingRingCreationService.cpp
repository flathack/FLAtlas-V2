#include "editors/system/DockingRingCreationService.h"

#include "core/PathUtils.h"
#include "domain/SolarObject.h"
#include "domain/SystemDocument.h"
#include "editors/system/DockingRingDialog.h"
#include "infrastructure/freelancer/IdsStringTable.h"
#include "rendering/view2d/MapScene.h"

#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QtMath>

namespace {

QString rawEntryValue(const flatlas::domain::SolarObject &object, const QString &key)
{
    const QVector<QPair<QString, QString>> rawEntries = object.rawEntries();
    for (int index = rawEntries.size() - 1; index >= 0; --index) {
        if (rawEntries.at(index).first.compare(key, Qt::CaseInsensitive) == 0)
            return rawEntries.at(index).second.trimmed();
    }
    return {};
}

QString resolvedIdsDisplayName(const QString &gameRoot, int idsName)
{
    if (idsName <= 0)
        return {};
    const QString exeDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("EXE"));
    if (exeDir.isEmpty())
        return {};
    flatlas::infrastructure::IdsStringTable ids;
    ids.loadFromFreelancerDir(exeDir);
    return ids.getString(idsName).trimmed();
}

void setOrAppendRawEntry(QVector<QPair<QString, QString>> *entries,
                         const QString &key,
                         const QString &value)
{
    if (!entries)
        return;
    for (QPair<QString, QString> &entry : *entries) {
        if (entry.first.compare(key, Qt::CaseInsensitive) == 0) {
            entry.second = value;
            return;
        }
    }
    entries->append({key, value});
}

bool objectHasBaseNickname(const flatlas::domain::SolarObject &object, QString *outBaseNickname = nullptr)
{
    QString baseNickname = object.base().trimmed();
    if (baseNickname.isEmpty())
        baseNickname = rawEntryValue(object, QStringLiteral("base"));
    if (outBaseNickname)
        *outBaseNickname = baseNickname;
    return !baseNickname.isEmpty();
}

QString nextAvailableFixtureNickname(const flatlas::domain::SystemDocument &document)
{
    const QString systemNickname = QFileInfo(document.filePath()).completeBaseName().trimmed();
    const QString prefix = QStringLiteral("%1_docking_fixture_").arg(systemNickname);
    QSet<int> usedSuffixes;
    for (const auto &objectPtr : document.objects()) {
        if (!objectPtr)
            continue;
        const QString nickname = objectPtr->nickname().trimmed();
        if (!nickname.startsWith(prefix, Qt::CaseInsensitive))
            continue;
        bool ok = false;
        const int suffix = nickname.mid(prefix.size()).toInt(&ok);
        if (ok)
            usedSuffixes.insert(suffix);
    }

    int nextSuffix = 1;
    while (usedSuffixes.contains(nextSuffix))
        ++nextSuffix;
    return QStringLiteral("%1%2").arg(prefix).arg(nextSuffix);
}

QVector3D fixturePositionForRing(const flatlas::domain::SolarObject &ringObject)
{
    const QVector3D ringPosition = ringObject.position();
    return QVector3D(ringPosition.x(),
                     ringPosition.y() + flatlas::editors::DockingRingCreationService::FixtureVerticalOffset,
                     ringPosition.z());
}

QVector<QPair<QString, QString>> buildFixtureEntries(const flatlas::domain::SolarObject &ringObject,
                                                     const QString &fixtureNickname,
                                                     const QString &baseNickname)
{
    const QVector3D position = fixturePositionForRing(ringObject);
    const QVector3D rotation = ringObject.rotation();
    QVector<QPair<QString, QString>> entries;
    entries.append({QStringLiteral("nickname"), fixtureNickname});
    entries.append(QPair<QString, QString>(QStringLiteral("ids_name"), QString::number(flatlas::editors::DockingRingCreationService::FixtureIdsName)));
    entries.append({QStringLiteral("pos"), QStringLiteral("%1, %2, %3")
        .arg(position.x(), 0, 'f', 2)
        .arg(position.y(), 0, 'f', 2)
        .arg(position.z(), 0, 'f', 2)});
    entries.append({QStringLiteral("rotate"), QStringLiteral("%1, %2, %3")
        .arg(rotation.x(), 0, 'f', 2)
        .arg(rotation.y(), 0, 'f', 2)
        .arg(rotation.z(), 0, 'f', 2)});
    entries.append({QStringLiteral("Archetype"), QStringLiteral("docking_fixture")});
    entries.append(QPair<QString, QString>(QStringLiteral("ids_info"), QString::number(flatlas::editors::DockingRingCreationService::FixtureIdsInfo)));
    entries.append({QStringLiteral("behavior"), QStringLiteral("NOTHING")});
    entries.append({QStringLiteral("dock_with"), baseNickname});
    entries.append({QStringLiteral("base"), baseNickname});
    const QString reputation = rawEntryValue(ringObject, QStringLiteral("reputation"));
    if (!reputation.isEmpty())
        entries.append({QStringLiteral("reputation"), reputation});
    return entries;
}

flatlas::editors::BaseEditState buildBaseCreateState(const flatlas::domain::SystemDocument &document,
                                                     const flatlas::domain::SolarObject &planet,
                                                     const flatlas::editors::DockingRingCreateRequest &request,
                                                     const QString &gameRoot,
                                                     const QHash<QString, QString> &textOverrides,
                                                     QString *errorMessage)
{
    using flatlas::editors::BaseEditService;
    flatlas::editors::BaseEditState state = BaseEditService::makeCreateState(document, gameRoot, errorMessage);
    state.baseNickname = request.baseNickname.trimmed();
    state.startRoom = flatlas::editors::DockingRingCreationService::chooseStartRoom(request.roomNames,
                                                                                     request.startRoom,
                                                                                     state.startRoom);
    state.priceVariance = request.priceVariance;
    state.reputation = request.factionDisplay.trimmed();
    state.voice = request.voice.trimmed();
    state.pilot = request.pilot.trimmed();
    state.idsName = request.stridName;
    state.objectNickname = planet.nickname();
    state.displayName = resolvedIdsDisplayName(gameRoot, state.idsName);
    if (state.displayName.isEmpty())
        state.displayName = resolvedIdsDisplayName(gameRoot, planet.idsName());
    if (state.displayName.isEmpty())
        state.displayName = planet.nickname().trimmed();

    if (!request.templateBase.trimmed().isEmpty()) {
        flatlas::editors::BaseEditState templateState;
        QString templateError;
        if (BaseEditService::loadTemplateState(request.templateBase.trimmed(),
                                               gameRoot,
                                               textOverrides,
                                               &templateState,
                                               &templateError)) {
            state.rooms = BaseEditService::applyTemplateRoomsForCreate(state, templateState, false);
        }
    }

    QSet<QString> enabledRooms;
    for (const QString &roomName : request.roomNames)
        enabledRooms.insert(roomName);
    if (state.rooms.isEmpty())
        state.rooms = BaseEditService::defaultRoomsForArchetype(state.archetype);
    for (auto &room : state.rooms)
        room.enabled = enabledRooms.contains(room.roomName);
    return state;
}

} // namespace

namespace flatlas::editors {

bool DockingRingCreationService::canHostDockingRing(const flatlas::domain::SolarObject &object)
{
    if (object.type() == flatlas::domain::SolarObject::Type::Sun)
        return false;
    if (object.type() == flatlas::domain::SolarObject::Type::Planet)
        return true;
    const QString archetype = object.archetype().trimmed();
    return archetype.contains(QStringLiteral("planet"), Qt::CaseInsensitive)
        && !archetype.contains(QStringLiteral("sun"), Qt::CaseInsensitive);
}

bool DockingRingCreationService::isDockingRingObject(const flatlas::domain::SolarObject &object)
{
    return object.type() == flatlas::domain::SolarObject::Type::DockingRing
        || object.archetype().contains(QStringLiteral("dock_ring"), Qt::CaseInsensitive);
}

bool DockingRingCreationService::isDockingFixtureObject(const flatlas::domain::SolarObject &object)
{
    return object.archetype().contains(QStringLiteral("docking_fixture"), Qt::CaseInsensitive);
}

QString DockingRingCreationService::chooseStartRoom(const QStringList &roomNames,
                                                    const QString &preferredStartRoom,
                                                    const QString &currentStartRoom)
{
    QStringList rooms;
    for (const QString &roomName : roomNames) {
        const QString trimmed = roomName.trimmed();
        if (!trimmed.isEmpty() && !rooms.contains(trimmed, Qt::CaseInsensitive))
            rooms.append(trimmed);
    }

    if (!preferredStartRoom.trimmed().isEmpty() && rooms.contains(preferredStartRoom.trimmed(), Qt::CaseInsensitive))
        return preferredStartRoom.trimmed();
    if (!currentStartRoom.trimmed().isEmpty() && rooms.contains(currentStartRoom.trimmed(), Qt::CaseInsensitive))
        return currentStartRoom.trimmed();
    if (rooms.contains(QStringLiteral("Deck"), Qt::CaseInsensitive))
        return QStringLiteral("Deck");
    return rooms.isEmpty() ? QString() : rooms.first();
}

QString DockingRingCreationService::defaultBaseNickname(const flatlas::domain::SystemDocument &document)
{
    const QString systemNickname = QFileInfo(document.filePath()).completeBaseName().trimmed();
    QSet<int> suffixes;
    for (const auto &objectPtr : document.objects()) {
        if (!objectPtr)
            continue;
        QString baseNickname;
        if (!objectHasBaseNickname(*objectPtr, &baseNickname))
            continue;
        const QString upperNickname = baseNickname.toUpper();
        const QString prefix = systemNickname.toUpper() + QStringLiteral("_");
        if (!upperNickname.startsWith(prefix) || !upperNickname.endsWith(QStringLiteral("_BASE")))
            continue;
        const QString middle = upperNickname.mid(prefix.size(), upperNickname.size() - prefix.size() - 5);
        bool ok = false;
        const int suffix = middle.toInt(&ok);
        if (ok)
            suffixes.insert(suffix);
    }

    int nextSuffix = 1;
    while (suffixes.contains(nextSuffix))
        ++nextSuffix;
    return QStringLiteral("%1_%2_Base").arg(systemNickname, QStringLiteral("%1").arg(nextSuffix, 2, 10, QChar('0')));
}

QString DockingRingCreationService::resolvedDockWithBase(const flatlas::domain::SolarObject &object)
{
    QString baseNickname = rawEntryValue(object, QStringLiteral("dock_with"));
    if (baseNickname.isEmpty())
        baseNickname = rawEntryValue(object, QStringLiteral("base"));
    if (baseNickname.isEmpty())
        baseNickname = object.base().trimmed();
    if (baseNickname.isEmpty())
        baseNickname = object.dockWith().trimmed();
    return baseNickname.trimmed();
}

std::shared_ptr<flatlas::domain::SolarObject> DockingRingCreationService::findAssociatedFixture(const flatlas::domain::SystemDocument *document,
                                                                                                const flatlas::domain::SolarObject &ringObject,
                                                                                                int *outMatchCount)
{
    if (outMatchCount)
        *outMatchCount = 0;
    if (!document)
        return {};

    const QString baseNickname = resolvedDockWithBase(ringObject);
    if (baseNickname.isEmpty())
        return {};

    QVector<std::shared_ptr<flatlas::domain::SolarObject>> matches;
    for (const auto &objectPtr : document->objects()) {
        if (!objectPtr || !isDockingFixtureObject(*objectPtr))
            continue;
        if (resolvedDockWithBase(*objectPtr).compare(baseNickname, Qt::CaseInsensitive) != 0)
            continue;
        matches.append(objectPtr);
    }

    if (outMatchCount)
        *outMatchCount = matches.size();
    return matches.size() == 1 ? matches.first() : std::shared_ptr<flatlas::domain::SolarObject>{};
}

std::shared_ptr<flatlas::domain::SolarObject> DockingRingCreationService::buildFixtureObject(const flatlas::domain::SystemDocument &document,
                                                                                              const flatlas::domain::SolarObject &ringObject,
                                                                                              QString *errorMessage)
{
    const QString baseNickname = resolvedDockWithBase(ringObject);
    if (baseNickname.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("The docking ring does not contain a dock_with/base target for a docking_fixture.");
        return {};
    }

    auto fixture = std::make_shared<flatlas::domain::SolarObject>();
    fixture->setNickname(nextAvailableFixtureNickname(document));
    fixture->setArchetype(QStringLiteral("docking_fixture"));
    fixture->setIdsName(FixtureIdsName);
    fixture->setIdsInfo(FixtureIdsInfo);
    fixture->setType(flatlas::domain::SolarObject::Type::Other);
    fixture->setDockWith(baseNickname);
    fixture->setBase(baseNickname);
    fixture->setPosition(fixturePositionForRing(ringObject));
    fixture->setRotation(ringObject.rotation());
    fixture->setRawEntries(buildFixtureEntries(ringObject, fixture->nickname(), baseNickname));
    return fixture;
}

void DockingRingCreationService::syncExistingFixture(flatlas::domain::SolarObject *fixtureObject,
                                                     const flatlas::domain::SolarObject &ringObject)
{
    if (!fixtureObject)
        return;
    const QString baseNickname = resolvedDockWithBase(ringObject);
    fixtureObject->setArchetype(QStringLiteral("docking_fixture"));
    fixtureObject->setIdsName(FixtureIdsName);
    fixtureObject->setIdsInfo(FixtureIdsInfo);
    fixtureObject->setDockWith(baseNickname);
    fixtureObject->setBase(baseNickname);
    fixtureObject->setPosition(fixturePositionForRing(ringObject));
    fixtureObject->setRotation(ringObject.rotation());
    fixtureObject->setRawEntries(buildFixtureEntries(ringObject, fixtureObject->nickname(), baseNickname));
}

bool DockingRingCreationService::apply(flatlas::domain::SystemDocument *document,
                                       flatlas::domain::SolarObject *planet,
                                       const DockingRingCreateRequest &request,
                                       const QPointF &ringScenePos,
                                       float ringYawDegrees,
                                       const QString &gameRoot,
                                       const QHash<QString, QString> &textOverrides,
                                       DockingRingCreationResult *outResult,
                                       QString *errorMessage)
{
    if (!document || !planet || !outResult) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Docking ring creation received incomplete arguments.");
        return false;
    }

    QString baseNickname;
    objectHasBaseNickname(*planet, &baseNickname);
    if (request.needsBase)
        baseNickname = request.baseNickname.trimmed();
    if (baseNickname.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("The selected planet does not provide a base nickname for dock_with.");
        return false;
    }

    if (request.needsBase) {
        QString baseError;
        BaseApplyResult baseResult;
        BaseEditState baseState = buildBaseCreateState(*document, *planet, request, gameRoot, textOverrides, &baseError);
        if (baseState.baseNickname.isEmpty()) {
            if (errorMessage)
                *errorMessage = baseError.isEmpty() ? QStringLiteral("The docking ring base nickname is empty.") : baseError;
            return false;
        }
        if (!BaseEditService::applyCreate(baseState,
                                          rendering::MapScene::flToQt(planet->position().x(), planet->position().z()),
                                          gameRoot,
                                          textOverrides,
                                          &baseResult,
                                          &baseError)) {
            if (errorMessage)
                *errorMessage = baseError;
            return false;
        }
        outResult->stagedWrites = baseResult.stagedWrites;

        QVector<QPair<QString, QString>> rawEntries = planet->rawEntries();
        setOrAppendRawEntry(&rawEntries, QStringLiteral("base"), baseNickname);
        if (!request.factionDisplay.trimmed().isEmpty())
            setOrAppendRawEntry(&rawEntries, QStringLiteral("reputation"), request.factionDisplay.trimmed());
        setOrAppendRawEntry(&rawEntries, QStringLiteral("visit"), QStringLiteral("1"));
        planet->setBase(baseNickname);
        planet->setRawEntries(rawEntries);
    }

    auto createdRing = std::make_shared<flatlas::domain::SolarObject>();
    createdRing->setType(flatlas::domain::SolarObject::Type::DockingRing);
    createdRing->setNickname(request.nickname.trimmed());
    createdRing->setArchetype(request.archetype.trimmed().isEmpty() ? QStringLiteral("dock_ring") : request.archetype.trimmed());
    createdRing->setLoadout(request.loadout.trimmed());
    createdRing->setDockWith(baseNickname);
    createdRing->setIdsName(request.ringIdsName);
    createdRing->setIdsInfo(request.ringIdsInfo);
    const QPointF ringWorldPos = rendering::MapScene::qtToFl(ringScenePos.x(), ringScenePos.y());
    createdRing->setPosition(QVector3D(static_cast<float>(ringWorldPos.x()),
                                       planet->position().y(),
                                       static_cast<float>(ringWorldPos.y())));
    createdRing->setRotation(QVector3D(0.0f, ringYawDegrees, 0.0f));

    QVector<QPair<QString, QString>> ringEntries;
    ringEntries.append({QStringLiteral("nickname"), createdRing->nickname()});
    ringEntries.append({QStringLiteral("ids_name"), QString::number(createdRing->idsName())});
    ringEntries.append({QStringLiteral("ids_info"), QString::number(createdRing->idsInfo())});
    ringEntries.append({QStringLiteral("pos"), QStringLiteral("%1, %2, %3")
        .arg(createdRing->position().x(), 0, 'f', 2)
        .arg(createdRing->position().y(), 0, 'f', 2)
        .arg(createdRing->position().z(), 0, 'f', 2)});
    ringEntries.append({QStringLiteral("rotate"), QStringLiteral("0, %1, 0").arg(ringYawDegrees, 0, 'f', 2)});
    ringEntries.append({QStringLiteral("Archetype"), createdRing->archetype()});
    ringEntries.append({QStringLiteral("dock_with"), baseNickname});
    if (!createdRing->loadout().isEmpty())
        ringEntries.append({QStringLiteral("loadout"), createdRing->loadout()});
    ringEntries.append({QStringLiteral("behavior"), QStringLiteral("NOTHING")});
    if (!request.voice.trimmed().isEmpty())
        ringEntries.append({QStringLiteral("voice"), request.voice.trimmed()});
    if (!request.costume.trimmed().isEmpty())
        ringEntries.append({QStringLiteral("space_costume"), request.costume.trimmed().contains(',') ? request.costume.trimmed() : QStringLiteral(", %1").arg(request.costume.trimmed())});
    if (!request.pilot.trimmed().isEmpty())
        ringEntries.append({QStringLiteral("pilot"), request.pilot.trimmed()});
    ringEntries.append({QStringLiteral("difficulty_level"), QString::number(request.difficulty)});
    if (!request.factionDisplay.trimmed().isEmpty())
        ringEntries.append({QStringLiteral("reputation"), request.factionDisplay.trimmed()});
    createdRing->setRawEntries(ringEntries);

    outResult->createdRing = createdRing;
    outResult->baseNickname = baseNickname;
    if (request.createFixture) {
        outResult->createdFixture = buildFixtureObject(*document, *createdRing, errorMessage);
        if (!outResult->createdFixture)
            return false;
    }
    return true;
}

} // namespace flatlas::editors