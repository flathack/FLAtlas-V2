#pragma once

#include <QHash>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <memory>

namespace flatlas::domain {
class SolarObject;
class SystemDocument;
}

namespace flatlas::editors {

struct BaseRoomNpcState {
    QString nickname;
    QString nameText;
    QString reputation;
    QString affiliation;
    QString role;
    QString body;
    QString head;
    QString leftHand;
    QString rightHand;
    QVector<QPair<QString, QString>> templateEntries;
};

struct BaseRoomState {
    QString roomName;
    QString scenePath;
    QString templateContent;
    bool enabled = true;
    QVector<BaseRoomNpcState> npcs;
};

struct BaseEditState {
    bool editMode = false;
    QString objectNickname;
    QString baseNickname;
    QString systemNickname;
    QString archetype;
    QString loadout;
    QString reputation;
    QString pilot;
    QString voice;
    QString head;
    QString body;
    QString bgcsBaseRunBy;
    QString displayName;
    QString infocardXml;
    int idsName = 0;
    int idsInfo = 0;
    double priceVariance = 0.15;
    QString startRoom;
    QString rotate;
    QVector<BaseRoomState> rooms;
    QString universeBaseFileRelativePath;
    QString baseIniAbsolutePath;
    QString roomsDirectoryAbsolutePath;
    QString universeIniAbsolutePath;
    QString mbaseAbsolutePath;
};

struct BaseStagedWrite {
    QString absolutePath;
    QString content;
};

struct BaseApplyResult {
    std::shared_ptr<flatlas::domain::SolarObject> createdObject;
    QVector<BaseStagedWrite> stagedWrites;
};

struct BaseArchetypeDefaults {
    QString archetype;
    QString sourceBaseNickname;
    QString sourceObjectNickname;
    QString loadout;
    QString infocardXml;
};

class BaseEditService {
public:
    static bool objectHasBase(const flatlas::domain::SolarObject &object);
    static QVector<BaseRoomState> defaultRoomsForArchetype(const QString &archetype);
    static BaseArchetypeDefaults archetypeDefaults(const QString &archetype,
                                                   const QString &gameRoot,
                                                   const QHash<QString, QString> &textOverrides = {});
    static QVector<BaseRoomState> applyTemplateRoomsForCreate(const BaseEditState &targetState,
                                                              const BaseEditState &templateState,
                                                              bool copyNpcs);

    static BaseEditState makeCreateState(const flatlas::domain::SystemDocument &document,
                                         const QString &gameRoot,
                                         QString *errorMessage = nullptr);

    static bool loadTemplateState(const QString &baseNickname,
                                  const QString &gameRoot,
                                  const QHash<QString, QString> &textOverrides,
                                  BaseEditState *outState,
                                  QString *errorMessage = nullptr);

    static bool loadState(const flatlas::domain::SystemDocument &document,
                          const flatlas::domain::SolarObject &object,
                          const QString &gameRoot,
                          const QHash<QString, QString> &textOverrides,
                          BaseEditState *outState,
                          QString *errorMessage = nullptr);

    static bool applyCreate(const BaseEditState &state,
                            const QPointF &scenePos,
                            const QString &gameRoot,
                            const QHash<QString, QString> &textOverrides,
                            BaseApplyResult *outResult,
                            QString *errorMessage = nullptr);

    static bool applyEdit(flatlas::domain::SolarObject &object,
                          const BaseEditState &state,
                          const QString &gameRoot,
                          const QHash<QString, QString> &textOverrides,
                          BaseApplyResult *outResult,
                          QString *errorMessage = nullptr);
};

} // namespace flatlas::editors