#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>

namespace flatlas::tools {

enum class FieldTemplateKind {
    Asteroid,
    Ice,
    Debris,
    Mine,
    Gas,
    Nebula
};

struct FieldAsset {
    QString nickname;
    QString modelPath;
    QString category;
};

struct FieldPlacedObject {
    QString assetNickname;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    int rotateX = 0;
    int rotateY = 0;
    int rotateZ = 0;
    bool mineRole = false;
};

struct FieldTemplate {
    FieldTemplateKind kind = FieldTemplateKind::Asteroid;
    QString presetName;
    QString fileName;
    QString zoneNickname;
    QString texturePanelsFile;
    QString billboardShape;
    QString fillShape;
    QStringList cubeShapeFallbacks;
    QString spacedust;
    QString music;
    QColor primaryColor = QColor(220, 220, 180);
    QColor ambientColor = QColor(120, 120, 120);
    QColor fogColor = QColor(40, 72, 120);
    int propertyFlags = 66;
    int visit = 32;
    int damage = 0;
    int sort = 25;
    int zoneSizeX = 10000;
    int zoneSizeY = 8000;
    int zoneSizeZ = 10000;
    int cubeSize = 400;
    int fillDistance = 3200;
    double emptyCubeFrequency = 0.6;
    int billboardCount = 400;
    int dynamicCount = 12;
    int fogDistance = 1600;
    int puffCount = 20;
    double lightningGap = 3.0;
    double lightningDuration = 0.75;
    QVector<FieldPlacedObject> placedObjects;
};

class FieldTemplateGenerator
{
public:
    static QString kindKey(FieldTemplateKind kind);
    static QString kindLabel(FieldTemplateKind kind);
    static QString linkedSectionName(FieldTemplateKind kind);
    static QString relativeDirectory(FieldTemplateKind kind);
    static QString defaultFileName(FieldTemplateKind kind);
    static QString defaultZoneNickname(FieldTemplateKind kind);
    static QVector<FieldTemplate> presets();
    static FieldTemplate preset(FieldTemplateKind kind);
    static FieldTemplate parseFieldIni(const QString &fileName, const QString &iniText, QString *errorMessage = nullptr);
    static QString generateFieldIni(const FieldTemplate &field);
    static QString generateSystemLinkPreview(const FieldTemplate &field);
    static QString normalizedFileName(QString fileName, FieldTemplateKind kind);
    static QVector<FieldPlacedObject> autoDistribute(const QVector<FieldAsset> &assets,
                                                     FieldTemplateKind kind,
                                                     int count,
                                                     quint32 seed,
                                                     const QString &spread);

private:
    static QString colorText(const QColor &color);
};

} // namespace flatlas::tools
