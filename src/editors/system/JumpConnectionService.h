#pragma once

#include <QVector3D>
#include <QString>

namespace flatlas::domain { class SystemDocument; }

namespace flatlas::editors {

struct JumpConnectionCreateRequest {
    QString sourceSystemNickname;
    QString sourceSystemDisplayName;
    QString sourceSystemFilePath;
    QString destinationSystemNickname;
    QString destinationSystemDisplayName;
    QString destinationSystemFilePath;
    QString sourceObjectNickname;
    QString destinationObjectNickname;
    QString archetype;
    QString kind;
    QVector3D sourcePosition;
    QVector3D destinationPosition;
    QString loadout;
    QString reputation;
    QString pilot;
    QString behavior;
    int difficultyLevel = 1;
};

struct JumpConnectionDeleteRequest {
    QString currentSystemFilePath;
    QString currentObjectNickname;
    QString destinationSystemFilePath;
    QString destinationObjectNickname;
    bool removeRemoteSide = true;
};

class JumpConnectionService
{
public:
    static bool createConnection(const JumpConnectionCreateRequest &request,
                                 QString *errorMessage = nullptr);

    static bool deleteConnection(const JumpConnectionDeleteRequest &request,
                                 QString *errorMessage = nullptr);
};

} // namespace flatlas::editors
