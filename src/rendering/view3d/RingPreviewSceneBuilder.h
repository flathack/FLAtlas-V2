#pragma once

#include "infrastructure/io/CmpLoader.h"

#include <QColor>
#include <QString>
#include <QVector3D>

namespace flatlas::rendering {

struct RingPreviewVisualStyle {
    QColor color = QColor(190, 200, 220);
    float opacity = 0.85f;
    int segments = 64;
};

struct RingPreviewSceneRequest {
    QString gameRoot;
    QString hostModelPath;
    QString ringPreset;
    double hostRadius = 0.0;
    bool showHostRadiusSphere = false;
    bool hostRadiusSphereIsSun = false;
    double innerRadius = 0.0;
    double outerRadius = 0.0;
    double thickness = 0.0;
    QVector3D rotationEuler;
};

struct RingPreviewSceneResult {
    flatlas::infrastructure::ModelNode sceneRoot;
    QString statusMessage;
    RingPreviewVisualStyle visualStyle;
    bool hasHostModel = false;
    bool hasRingGeometry = false;
    bool hasRenderableScene = false;
    bool geometryInputsValid = false;
};

class RingPreviewSceneBuilder
{
public:
    static RingPreviewVisualStyle loadVisualStyle(const QString &gameRoot, const QString &ringPreset);
    static RingPreviewSceneResult build(const RingPreviewSceneRequest &request);
};

} // namespace flatlas::rendering