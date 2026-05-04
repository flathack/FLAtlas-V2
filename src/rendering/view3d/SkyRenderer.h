#pragma once
// rendering/view3d/SkyRenderer.h – Hintergrund-Rendering (Phase 7)

#ifdef FLATLAS_HAS_QT3D

#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>

namespace Qt3DRender { class QMaterial; }
namespace Qt3DRender { class QGeometryRenderer; }

namespace flatlas::rendering {

/// Renders a dark sky sphere as background for the 3D scene.
class SkyRenderer : public Qt3DCore::QEntity {
    Q_OBJECT
public:
    explicit SkyRenderer(Qt3DCore::QNode *parent = nullptr);

    void setRadius(float radius);

private:
    Qt3DRender::QGeometryRenderer *m_mesh = nullptr;
    Qt3DRender::QMaterial *m_material = nullptr;
    Qt3DCore::QTransform *m_transform = nullptr;
};

} // namespace flatlas::rendering

#endif // FLATLAS_HAS_QT3D
