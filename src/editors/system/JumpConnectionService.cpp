#include "JumpConnectionService.h"

#include "SystemPersistence.h"
#include "core/EditingContext.h"
#include "domain/SolarObject.h"
#include "domain/SystemDocument.h"
#include "infrastructure/freelancer/IdsDataService.h"

#include <QFileInfo>
#include <memory>

namespace flatlas::editors {

namespace {

using flatlas::domain::SolarObject;
using flatlas::domain::SystemDocument;

QString vecToIni(const QVector3D &value)
{
    return QStringLiteral("%1, %2, %3")
        .arg(static_cast<double>(value.x()), 0, 'f', 2)
        .arg(static_cast<double>(value.y()), 0, 'f', 2)
        .arg(static_cast<double>(value.z()), 0, 'f', 2);
}

SolarObject *findObjectByNickname(SystemDocument *doc, const QString &nickname)
{
    if (!doc)
        return nullptr;
    for (const auto &obj : doc->objects()) {
        if (obj && obj->nickname().compare(nickname, Qt::CaseInsensitive) == 0)
            return obj.get();
    }
    return nullptr;
}

QString defaultGateConnectionName(const QString &originSystemDisplay,
                                  const QString &targetSystemDisplay)
{
    const QString origin = originSystemDisplay.trimmed().isEmpty()
        ? QStringLiteral("Unknown")
        : originSystemDisplay.trimmed();
    const QString target = targetSystemDisplay.trimmed().isEmpty()
        ? QStringLiteral("Unknown")
        : targetSystemDisplay.trimmed();
    return QStringLiteral("%1 -> %2").arg(origin, target);
}

QString defaultHoleConnectionName(const QString &targetSystemDisplay)
{
    const QString target = targetSystemDisplay.trimmed().isEmpty()
        ? QStringLiteral("Unknown")
        : targetSystemDisplay.trimmed();
    return QStringLiteral("%1 Jump Hole").arg(target);
}

bool saveWithRollback(const SystemDocument &primaryDoc,
                      const QString &primaryPath,
                      const SystemDocument *secondaryDoc,
                      const QString &secondaryPath,
                      const SystemDocument *primaryBackup,
                      const SystemDocument *secondaryBackup,
                      QString *errorMessage)
{
    if (!SystemPersistence::save(primaryDoc, primaryPath)) {
        if (errorMessage)
            *errorMessage = QObject::tr("Das Quellsystem konnte nicht gespeichert werden.");
        return false;
    }

    if (secondaryDoc && !secondaryPath.isEmpty() && !SystemPersistence::save(*secondaryDoc, secondaryPath)) {
        if (primaryBackup)
            SystemPersistence::save(*primaryBackup, primaryPath);
        if (secondaryBackup && !secondaryPath.isEmpty())
            SystemPersistence::save(*secondaryBackup, secondaryPath);
        if (errorMessage)
            *errorMessage = QObject::tr("Das Zielsystem konnte nicht gespeichert werden. Die Quellseite wurde zurueckgesetzt.");
        return false;
    }

    return true;
}

std::shared_ptr<SolarObject> buildJumpObject(const JumpConnectionCreateRequest &request,
                                             bool sourceSide,
                                             int idsNameValue)
{
    auto object = std::make_shared<SolarObject>();
    object->setNickname(sourceSide ? request.sourceObjectNickname : request.destinationObjectNickname);
    object->setType(request.kind.compare(QStringLiteral("gate"), Qt::CaseInsensitive) == 0
                        ? SolarObject::JumpGate
                        : SolarObject::JumpHole);
    object->setArchetype(request.archetype);
    object->setPosition(sourceSide ? request.sourcePosition : request.destinationPosition);
    object->setRotation(QVector3D(0.0f, 10.0f, 0.0f));
    object->setIdsName(idsNameValue);
    object->setIdsInfo(request.kind.compare(QStringLiteral("gate"), Qt::CaseInsensitive) == 0 ? 66145 : 66146);
    if (request.kind.compare(QStringLiteral("gate"), Qt::CaseInsensitive) == 0 && !request.loadout.trimmed().isEmpty())
        object->setLoadout(request.loadout.trimmed());

    const QString targetSystem = sourceSide ? request.destinationSystemNickname : request.sourceSystemNickname;
    const QString targetObject = sourceSide ? request.destinationObjectNickname : request.sourceObjectNickname;
    const QString jumpEffect = request.kind.compare(QStringLiteral("gate"), Qt::CaseInsensitive) == 0
        ? QStringLiteral("jump_effect_bretonia")
        : QStringLiteral("jump_effect_hole");

    QVector<QPair<QString, QString>> entries{
        {QStringLiteral("nickname"), object->nickname()},
        {QStringLiteral("pos"), vecToIni(object->position())},
        {QStringLiteral("rotate"), QStringLiteral("0, 10, 0")},
        {QStringLiteral("archetype"), request.archetype},
        {QStringLiteral("msg_id_prefix"), QStringLiteral("gcs_refer_system_%1").arg(targetSystem)},
        {QStringLiteral("jump_effect"), jumpEffect},
        {QStringLiteral("ids_info"), QString::number(object->idsInfo())},
        {QStringLiteral("visit"), QStringLiteral("0")},
        {QStringLiteral("goto"), QStringLiteral("%1, %2, gate_tunnel_bretonia").arg(targetSystem, targetObject)},
    };

    if (idsNameValue > 0)
        entries.insert(4, {QStringLiteral("ids_name"), QString::number(idsNameValue)});

    if (request.kind.compare(QStringLiteral("gate"), Qt::CaseInsensitive) == 0) {
        entries.append({QStringLiteral("reputation"), request.reputation.trimmed()});
        entries.append({QStringLiteral("behavior"), request.behavior.trimmed().isEmpty() ? QStringLiteral("NOTHING")
                                                                                          : request.behavior.trimmed()});
        entries.append({QStringLiteral("difficulty_level"), QString::number(request.difficultyLevel)});
        if (!request.loadout.trimmed().isEmpty())
            entries.append({QStringLiteral("loadout"), request.loadout.trimmed()});
        if (!request.pilot.trimmed().isEmpty())
            entries.append({QStringLiteral("pilot"), request.pilot.trimmed()});
    }

    object->setGotoTarget(QStringLiteral("%1, %2, gate_tunnel_bretonia").arg(targetSystem, targetObject));
    object->setRawEntries(entries);
    return object;
}

} // namespace

bool JumpConnectionService::createConnection(const JumpConnectionCreateRequest &request,
                                             QString *errorMessage)
{
    if (request.sourceSystemFilePath.trimmed().isEmpty() || request.destinationSystemFilePath.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("Quell- oder Zielsystemdatei fehlt.");
        return false;
    }
    if (request.sourceObjectNickname.trimmed().isEmpty() || request.destinationObjectNickname.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("Beide Objekt-Nicknames muessen gesetzt sein.");
        return false;
    }

    auto sourceDoc = SystemPersistence::load(request.sourceSystemFilePath);
    if (!sourceDoc) {
        if (errorMessage)
            *errorMessage = QObject::tr("Das Quellsystem konnte nicht geladen werden.");
        return false;
    }
    std::unique_ptr<SystemDocument> destDoc;
    const bool sameSystem = QFileInfo(request.sourceSystemFilePath) == QFileInfo(request.destinationSystemFilePath);
    if (sameSystem) {
        destDoc.reset();
    } else {
        destDoc = SystemPersistence::load(request.destinationSystemFilePath);
        if (!destDoc) {
            if (errorMessage)
                *errorMessage = QObject::tr("Das Zielsystem konnte nicht geladen werden.");
            return false;
        }
    }

    if (findObjectByNickname(sourceDoc.get(), request.sourceObjectNickname)) {
        if (errorMessage)
            *errorMessage = QObject::tr("Im Quellsystem existiert bereits ein Objekt mit diesem Nickname.");
        return false;
    }
    SystemDocument *destTargetDoc = sameSystem ? sourceDoc.get() : destDoc.get();
    if (findObjectByNickname(destTargetDoc, request.destinationObjectNickname)) {
        if (errorMessage)
            *errorMessage = QObject::tr("Im Zielsystem existiert bereits ein Objekt mit diesem Nickname.");
        return false;
    }

    const auto sourceBackup = SystemPersistence::load(request.sourceSystemFilePath);
    const auto destBackup = sameSystem ? nullptr : SystemPersistence::load(request.destinationSystemFilePath);

    int sourceIdsName = 0;
    int destIdsName = 0;
    const QString gameRoot = flatlas::core::EditingContext::instance().primaryGamePath();
    if (!gameRoot.trimmed().isEmpty()) {
        const auto dataset = flatlas::infrastructure::IdsDataService::loadFromGameRoot(gameRoot);
        const QString targetDll = flatlas::infrastructure::IdsDataService::defaultCreationDllName(dataset);
        const bool gate = request.kind.compare(QStringLiteral("gate"), Qt::CaseInsensitive) == 0;
        const QString sourceIdsText = gate
            ? defaultGateConnectionName(request.sourceSystemDisplayName, request.destinationSystemDisplayName)
            : defaultHoleConnectionName(request.destinationSystemDisplayName);
        const QString destIdsText = gate
            ? defaultGateConnectionName(request.destinationSystemDisplayName, request.sourceSystemDisplayName)
            : defaultHoleConnectionName(request.sourceSystemDisplayName);

        QString idsError;
        if (!flatlas::infrastructure::IdsDataService::writeStringEntry(
                dataset, targetDll, 0, sourceIdsText, &sourceIdsName, &idsError)) {
            if (errorMessage)
                *errorMessage = QObject::tr("Der IDS-Name fuer die Quellseite konnte nicht geschrieben werden.\n%1")
                                    .arg(idsError.isEmpty() ? QObject::tr("Unbekannter Fehler.") : idsError);
            return false;
        }
        if (!flatlas::infrastructure::IdsDataService::writeStringEntry(
                dataset, targetDll, 0, destIdsText, &destIdsName, &idsError)) {
            if (errorMessage)
                *errorMessage = QObject::tr("Der IDS-Name fuer die Zielseite konnte nicht geschrieben werden.\n%1")
                                    .arg(idsError.isEmpty() ? QObject::tr("Unbekannter Fehler.") : idsError);
            return false;
        }
    }

    sourceDoc->addObject(buildJumpObject(request, true, sourceIdsName));
    destTargetDoc->addObject(buildJumpObject(request, false, destIdsName));

    return saveWithRollback(*sourceDoc,
                            request.sourceSystemFilePath,
                            sameSystem ? nullptr : destDoc.get(),
                            sameSystem ? QString() : request.destinationSystemFilePath,
                            sourceBackup.get(),
                            destBackup.get(),
                            errorMessage);
}

bool JumpConnectionService::deleteConnection(const JumpConnectionDeleteRequest &request,
                                             QString *errorMessage)
{
    if (request.currentSystemFilePath.trimmed().isEmpty() || request.currentObjectNickname.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("Zu loeschende Jump-Verbindung ist unvollstaendig.");
        return false;
    }

    auto currentDoc = SystemPersistence::load(request.currentSystemFilePath);
    if (!currentDoc) {
        if (errorMessage)
            *errorMessage = QObject::tr("Das aktuelle System konnte nicht geladen werden.");
        return false;
    }

    const auto currentBackup = SystemPersistence::load(request.currentSystemFilePath);
    std::unique_ptr<SystemDocument> remoteDoc;
    std::unique_ptr<SystemDocument> remoteBackup;
    const bool sameSystem = !request.destinationSystemFilePath.trimmed().isEmpty()
        && QFileInfo(request.currentSystemFilePath) == QFileInfo(request.destinationSystemFilePath);

    SystemDocument *remoteTargetDoc = nullptr;
    if (request.removeRemoteSide && !request.destinationObjectNickname.trimmed().isEmpty()) {
        if (sameSystem) {
            remoteTargetDoc = currentDoc.get();
        } else if (!request.destinationSystemFilePath.trimmed().isEmpty()) {
            remoteDoc = SystemPersistence::load(request.destinationSystemFilePath);
            if (!remoteDoc) {
                if (errorMessage)
                    *errorMessage = QObject::tr("Das Zielsystem fuer die Gegenverbindung konnte nicht geladen werden.");
                return false;
            }
            remoteBackup = SystemPersistence::load(request.destinationSystemFilePath);
            remoteTargetDoc = remoteDoc.get();
        }
    }

    SolarObject *localObject = findObjectByNickname(currentDoc.get(), request.currentObjectNickname);
    if (!localObject) {
        if (errorMessage)
            *errorMessage = QObject::tr("Das lokale Jump-Objekt wurde nicht gefunden.");
        return false;
    }
    std::shared_ptr<SolarObject> localShared;
    for (const auto &obj : currentDoc->objects()) {
        if (obj && obj->nickname().compare(request.currentObjectNickname, Qt::CaseInsensitive) == 0) {
            localShared = obj;
            break;
        }
    }
    if (localShared)
        currentDoc->removeObject(localShared);

    if (remoteTargetDoc && !request.destinationObjectNickname.trimmed().isEmpty()) {
        for (const auto &obj : remoteTargetDoc->objects()) {
            if (obj && obj->nickname().compare(request.destinationObjectNickname, Qt::CaseInsensitive) == 0) {
                remoteTargetDoc->removeObject(obj);
                break;
            }
        }
    }

    return saveWithRollback(*currentDoc,
                            request.currentSystemFilePath,
                            sameSystem ? nullptr : remoteDoc.get(),
                            sameSystem ? QString() : request.destinationSystemFilePath,
                            currentBackup.get(),
                            remoteBackup.get(),
                            errorMessage);
}

} // namespace flatlas::editors
