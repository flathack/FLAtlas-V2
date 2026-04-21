#pragma once
// infrastructure/freelancer/ModelAssetScanner.h - context-aware Freelancer 3D model discovery

#include <QString>
#include <QVector>

namespace flatlas::infrastructure {

struct ModelAssetEntry {
    QString categoryKey;
    QString categoryLabel;
    QString nickname;
    QString displayName;
    QString archetype;
    QString modelPath;
    QString relativeModelPath;
    QString sourceIniPath;
    int idsName = 0;
    QString typeValue;
    QString renderKind;

    QString titleText() const;
    QString searchBlob() const;
};

class ModelAssetScanner {
public:
    static QVector<ModelAssetEntry> scanCurrentContext();
    static QVector<ModelAssetEntry> scanInstallation(const QString &gamePath);
};

} // namespace flatlas::infrastructure

