// rendering/view3d/MaterialFactory.cpp – Qt3D-Material aus Freelancer-Daten (Phase 8)

#ifdef FLATLAS_HAS_QT3D

#include "MaterialFactory.h"
#include <Qt3DRender/QTextureWrapMode>
#include <Qt3DRender/QPaintedTextureImage>
#include <QPainter>

namespace flatlas::rendering {

/// Custom texture image source that paints a QImage.
class ImageTextureSource : public Qt3DRender::QPaintedTextureImage {
public:
    explicit ImageTextureSource(const QImage &image, Qt3DCore::QNode *parent = nullptr)
        : Qt3DRender::QPaintedTextureImage(parent), m_image(image)
    {
        setSize(image.size());
    }

    void paint(QPainter *painter) override
    {
        painter->drawImage(0, 0, m_image);
    }

private:
    QImage m_image;
};

Qt3DRender::QMaterial *MaterialFactory::createFromImage(const QImage &image,
                                                        Qt3DCore::QNode *parent)
{
    if (image.isNull())
        return createDefault(QColor(180, 180, 180), parent);

    auto *material = new Qt3DExtras::QTextureMaterial(parent);
    auto *texture = createTexture(image, material);
    texture->wrapMode()->setX(Qt3DRender::QTextureWrapMode::Repeat);
    texture->wrapMode()->setY(Qt3DRender::QTextureWrapMode::Repeat);
    material->setTexture(texture);
    return material;
}

Qt3DExtras::QPhongMaterial *MaterialFactory::createDefault(const QColor &color,
                                                            Qt3DCore::QNode *parent)
{
    auto *material = new Qt3DExtras::QPhongMaterial(parent);
    material->setDiffuse(color);
    material->setAmbient(color.darker(200));
    material->setSpecular(QColor(50, 50, 50));
    material->setShininess(25.0f);
    return material;
}

Qt3DRender::QTexture2D *MaterialFactory::createTexture(const QImage &image,
                                                         Qt3DCore::QNode *parent)
{
    auto *texture = new Qt3DRender::QTexture2D(parent);
    auto *texImage = new ImageTextureSource(image, texture);
    texture->addTextureImage(texImage);
    texture->setMinificationFilter(Qt3DRender::QAbstractTexture::LinearMipMapLinear);
    texture->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
    texture->setGenerateMipMaps(true);
    return texture;
}

} // namespace flatlas::rendering

#endif // FLATLAS_HAS_QT3D
