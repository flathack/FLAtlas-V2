#include "editors/system/DockingRingCreationService.h"

#include "domain/SolarObject.h"
#include "domain/SystemDocument.h"
#include "editors/system/DockingRingDialog.h"
#include "rendering/view2d/MapScene.h"

#include <QFileInfo>
#include <QSet>
#include <QtMath>

namespace {

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
    if (baseNickname.isEmpty()) {
        const QVector<QPair<QString, QString>> rawEntries = object.rawEntries();
        for (const auto &entry : rawEntries) {
            if (entry.first.compare(QStringLiteral("base"), Qt::CaseInsensitive) == 0) {
                baseNickname = entry.second.trimmed();
                break;
            }
        }
    }
    if (outBaseNickname)
        *outBaseNickname = baseNickname;
    return !baseNickname.isEmpty();
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
    state.displayName = request.idsNameText.trimmed();
    state.idsName = request.stridName;
    state.objectNickname = planet.nickname();

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
    return true;
}

} // namespace flatlas::editors