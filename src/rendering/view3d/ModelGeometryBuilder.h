#pragma once
// rendering/view3d/ModelGeometryBuilder.h - reusable Qt3D geometry builders for Freelancer models

#ifdef FLATLAS_HAS_QT3D

#include "infrastructure/io/CmpLoader.h"

#include <QVector3D>

namespace Qt3DCore { class QNode; }
namespace Qt3DRender { class QGeometryRenderer; }

namespace flatlas::rendering {

struct ModelBounds {
    QVector3D minCorner{0.0f, 0.0f, 0.0f};
    QVector3D maxCorner{0.0f, 0.0f, 0.0f};
    bool valid = false;

    void include(const QVector3D &point);
    void include(const ModelBounds &other);
    QVector3D center() const;
    float radius() const;
};

class ModelGeometryBuilder {
public:
    static Qt3DRender::QGeometryRenderer *buildTriangleRenderer(
        const flatlas::infrastructure::MeshData &mesh,
        Qt3DCore::QNode *owner);

    static Qt3DRender::QGeometryRenderer *buildWireframeRenderer(
        const flatlas::infrastructure::MeshData &mesh,
        Qt3DCore::QNode *owner);

    static Qt3DRender::QGeometryRenderer *buildBoundingBoxRenderer(
        const ModelBounds &bounds,
        Qt3DCore::QNode *owner);

    static ModelBounds boundsForMesh(const flatlas::infrastructure::MeshData &mesh);
    static ModelBounds boundsForNode(const flatlas::infrastructure::ModelNode &node,
                                     const QVector3D &parentOffset = QVector3D());
};

} // namespace flatlas::rendering

#endif // FLATLAS_HAS_QT3D

