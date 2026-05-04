#pragma once
// rendering/view3d/ZoneGeometryBuilder.h - procedural 3D zone geometry

#include "domain/ZoneItem.h"
#include "ModelGeometryBuilder.h"
#include "ZoneColorScheme.h"

#ifdef FLATLAS_HAS_QT3D

#include <QQuaternion>
#include <QVector3D>

namespace Qt3DCore { class QEntity; }
namespace Qt3DRender { class QMaterial; }

namespace flatlas::rendering {

struct ZoneGeometryBuildResult {
    Qt3DCore::QEntity *rootEntity = nullptr;
    Qt3DCore::QEntity *pickEntity = nullptr;
    Qt3DCore::QEntity *wireEntity = nullptr;
    Qt3DRender::QMaterial *selectionMaterial = nullptr;
    ModelBounds bounds;
    bool valid = false;
};

class ZoneGeometryBuilder {
public:
    static ZoneGeometryBuildResult buildZone(const flatlas::domain::ZoneItem &zone,
                                             const ZoneVisualStyle &style,
                                             Qt3DCore::QEntity *parent);

    static QQuaternion rotationFromFreelancer(const QVector3D &rotation);
};

} // namespace flatlas::rendering

#endif // FLATLAS_HAS_QT3D
