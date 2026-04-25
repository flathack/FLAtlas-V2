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
    std::shared_ptr<flatlas::domain::SolarObject> createdFixture;
    QVector<BaseStagedWrite> stagedWrites;
    QString baseNickname;
};

class DockingRingCreationService
{
public:
    static constexpr int FixtureIdsName = 261166;
    static constexpr int FixtureIdsInfo = 66489;
    static constexpr float FixtureVerticalOffset = 350.0f;

    static bool canHostDockingRing(const flatlas::domain::SolarObject &object);
    static bool isDockingRingObject(const flatlas::domain::SolarObject &object);
    static bool isDockingFixtureObject(const flatlas::domain::SolarObject &object);
    static QString chooseStartRoom(const QStringList &roomNames,
                                   const QString &preferredStartRoom,
                                   const QString &currentStartRoom = QString());
    static QString defaultBaseNickname(const flatlas::domain::SystemDocument &document);
    static QString resolvedDockWithBase(const flatlas::domain::SolarObject &object);
    static std::shared_ptr<flatlas::domain::SolarObject> findAssociatedFixture(const flatlas::domain::SystemDocument *document,
                                                                               const flatlas::domain::SolarObject &ringObject,
                                                                               int *outMatchCount = nullptr);
    static std::shared_ptr<flatlas::domain::SolarObject> buildFixtureObject(const flatlas::domain::SystemDocument &document,
                                                                            const flatlas::domain::SolarObject &ringObject,
                                                                            QString *errorMessage = nullptr);
    static void syncExistingFixture(flatlas::domain::SolarObject *fixtureObject,
                                    const flatlas::domain::SolarObject &ringObject);

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