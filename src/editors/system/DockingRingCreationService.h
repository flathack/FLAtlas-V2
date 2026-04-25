#pragma once

#include "editors/system/BaseEditService.h"

#include <QHash>
#include <QPointF>
#include <QString>

#include <memory>

namespace flatlas::domain {
class SolarObject;
class SystemDocument;
}

namespace flatlas::editors {

struct DockingRingCreateRequest;

struct DockingRingCreationResult {
    std::shared_ptr<flatlas::domain::SolarObject> createdRing;
    QVector<BaseStagedWrite> stagedWrites;
    QString baseNickname;
};

class DockingRingCreationService
{
public:
    static bool canHostDockingRing(const flatlas::domain::SolarObject &object);
    static QString chooseStartRoom(const QStringList &roomNames,
                                   const QString &preferredStartRoom,
                                   const QString &currentStartRoom = QString());
    static QString defaultBaseNickname(const flatlas::domain::SystemDocument &document);

    static bool apply(flatlas::domain::SystemDocument *document,
                      flatlas::domain::SolarObject *planet,
                      const DockingRingCreateRequest &request,
                      const QPointF &ringScenePos,
                      float ringYawDegrees,
                      const QString &gameRoot,
                      const QHash<QString, QString> &textOverrides,
                      DockingRingCreationResult *outResult,
                      QString *errorMessage = nullptr);
};

} // namespace flatlas::editors