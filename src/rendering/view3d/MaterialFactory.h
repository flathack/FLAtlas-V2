#pragma once
// rendering/view3d/MaterialFactory.h – Qt3D-Material aus Freelancer-Daten (Phase 8)

#ifdef FLATLAS_HAS_QT3D

#include <QImage>
#include <QString>
#include <Qt3DExtras/QTextureMaterial>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QDiffuseSpecularMaterial>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QTexture>
#include <Qt3DRender/QTextureImage>
#include <Qt3DCore/QNode>

namespace flatlas::rendering {

/// Creates Qt3D materials from Freelancer texture data.
class MaterialFactory {
public:
    /// Create a textured Phong material from a QImage.
    static Qt3DRender::QMaterial *createFromImage(const QImage &image,
                                                  Qt3DCore::QNode *parent);

    /// Create a default material with a solid color.
    static Qt3DExtras::QPhongMaterial *createDefault(const QColor &color,
                                                      Qt3DCore::QNode *parent);

    /// Create a Qt3D texture from a QImage.
    static Qt3DRender::QTexture2D *createTexture(const QImage &image,
                                                   Qt3DCore::QNode *parent);
};

} // namespace flatlas::rendering

#endif // FLATLAS_HAS_QT3D
