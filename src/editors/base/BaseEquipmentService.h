#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace flatlas::editors {

struct BaseEquipmentOption {
    QString nickname;
    QString ingameName;
    QString displayLabel;
    QString groupLabel;
};

struct BaseEquipmentState {
    QString dataPath;
    QString equipmentMarketFilePath;
    QString shipMarketFilePath;
    QVector<BaseEquipmentOption> equipmentOptions;
    QVector<BaseEquipmentOption> shipPackageOptions;
    QStringList equipment;
    QStringList shipPackages;
    QStringList shipPackageLevels;
    QString warningMessage;
};

struct BaseEquipmentStagedWrite {
    QString absolutePath;
    QString content;
};

class BaseEquipmentService {
public:
    static constexpr int MaxShipsPerBase = 3;

    static BaseEquipmentState load(const QString &systemFilePath, const QString &baseNickname);
    static QVector<BaseEquipmentStagedWrite> stagedWrites(const QString &systemFilePath,
                                                          const QString &baseNickname,
                                                          const QStringList &equipment,
                                                          const QStringList &shipPackages,
                                                          QString *errorMessage = nullptr);
    static QVector<BaseEquipmentStagedWrite> stagedWrites(const QString &systemFilePath,
                                                          const QString &baseNickname,
                                                          const QStringList &equipment,
                                                          const QStringList &shipPackages,
                                                          const QStringList &shipPackageLevels,
                                                          QString *errorMessage);
    static bool save(const QString &systemFilePath,
                     const QString &baseNickname,
                     const QStringList &equipment,
                     const QStringList &shipPackages,
                     QString *errorMessage = nullptr);
    static bool save(const QString &systemFilePath,
                     const QString &baseNickname,
                     const QStringList &equipment,
                     const QStringList &shipPackages,
                     const QStringList &shipPackageLevels,
                     QString *errorMessage);

    static QString displayLabel(const QString &nickname, const QString &ingameName);
};

} // namespace flatlas::editors
