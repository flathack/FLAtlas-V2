#pragma once

#include <QString>
#include <QVector>

namespace flatlas::domain { class UniverseData; }

namespace flatlas::editors {

struct DeleteSystemSectionChange {
    QString filePath;
    QString sectionName;
    QString identifier;
    QString reason;
};

struct DeleteSystemFileDecision {
    QString path;
    QString reason;
};

struct DeleteSystemJumpCleanup {
    QString systemNickname;
    QString systemFilePath;
    QString objectNickname;
    QString zoneNickname;
};

struct DeleteSystemPrecheckReport {
    bool precheckCompleted = false;
    bool canDelete = false;
    QString systemNickname;
    QString systemDisplayName;
    QString universeFilePath;
    QString systemFilePath;
    QString systemFolderPath;
    QVector<DeleteSystemSectionChange> sectionChanges;
    QVector<DeleteSystemFileDecision> filesToDelete;
    QVector<DeleteSystemFileDecision> filesToKeep;
    QVector<DeleteSystemJumpCleanup> jumpCleanups;
    QStringList warnings;
    QStringList blockers;
};

class DeleteSystemService {
public:
    static DeleteSystemPrecheckReport precheck(const QString &universeFilePath,
                                               const flatlas::domain::UniverseData *data,
                                               const QString &systemNickname);

    static bool execute(const DeleteSystemPrecheckReport &report,
                        flatlas::domain::UniverseData *data,
                        QString *errorMessage = nullptr);
};

} // namespace flatlas::editors
