// rendering/view3d/MaterialFactory.cpp – Qt3D-Material aus Freelancer-Daten (Phase 8)

#ifdef FLATLAS_HAS_QT3D

#include "MaterialFactory.h"
#include <Qt3DRender/QColorMask>
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QNoDepthMask>
#include <Qt3DRender/QRenderPass>
#include <Qt3DRender/QTechnique>
#include <Qt3DRender/QTextureWrapMode>
#include <Qt3DRender/QPaintedTextureImage>
#include <QPainter>

namespace flatlas::rendering {

namespace {

QImage makePreviewTextureOpaque(const QImage &image)
{
    if (image.isNull() || !image.hasAlphaChannel())
        return image;

    QImage opaque = image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < opaque.height(); ++y) {
        auto *row = reinterpret_cast<QRgb *>(opaque.scanLine(y));
        for (int x = 0; x < opaque.width(); ++x)
            row[x] = qRgba(qRed(row[x]), qGreen(row[x]), qBlue(row[x]), 255);
    }
    return opaque;
}

} // namespace

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

    // Use QDiffuseSpecularMaterial (not QTextureMaterial): QTextureMaterial is an
    // UNLIT material that just samples the texel color, so normals/lighting are
    // ignored and every surface looks uniformly flat/smooth. QDiffuseSpecularMaterial
    // uses the texture as the diffuse input of a Phong-style lighting model so that
    // hard edges and face orientation become visible again.
    auto *material = new Qt3DExtras::QDiffuseSpecularMaterial(parent);
    auto *texture = createTexture(makePreviewTextureOpaque(image), material);
    texture->wrapMode()->setX(Qt3DRender::QTextureWrapMode::Repeat);
    texture->wrapMode()->setY(Qt3DRender::QTextureWrapMode::Repeat);
    material->setDiffuse(QVariant::fromValue(texture));
    material->setAmbient(QColor(22, 22, 22));
    material->setSpecular(QColor(12, 12, 12));
    material->setShininess(12.0f);
    return material;
}

Qt3DRender::QMaterial *MaterialFactory::createPlanetSurfaceFromImage(const QImage &image,
                                                                     Qt3DCore::QNode *parent)
{
    if (image.isNull())
        return createDefault(QColor(80, 135, 210), parent);

    auto *material = new Qt3DExtras::QDiffuseSpecularMaterial(parent);
    auto *texture = createTexture(makePreviewTextureOpaque(image), material);
    texture->wrapMode()->setX(Qt3DRender::QTextureWrapMode::ClampToEdge);
    texture->wrapMode()->setY(Qt3DRender::QTextureWrapMode::ClampToEdge);
    material->setDiffuse(QVariant::fromValue(texture));
    material->setAmbient(QColor(28, 28, 28));
    material->setSpecular(QColor(0, 0, 0));
    material->setShininess(1.0f);
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

void MaterialFactory::preventFramebufferAlphaWrites(Qt3DRender::QMaterial *material)
{
    if (!material || !material->effect())
        return;

    // Qt3D alpha materials blend correctly inside the scene, but the native
    // Qt3D window can expose framebuffer alpha to the desktop compositor. Keep
    // RGB blending and block writes to the native-window alpha channel so stale
    // widget pixels cannot leak through transparent geometry after tab switches.
    // Also keep translucent volumes out of the depth buffer so large system
    // zones do not hide other zones as soon as the renderer settles.
    for (Qt3DRender::QTechnique *technique : material->effect()->techniques()) {
        if (!technique)
            continue;
        for (Qt3DRender::QRenderPass *pass : technique->renderPasses()) {
            if (!pass)
                continue;
            auto *colorMask = new Qt3DRender::QColorMask(pass);
            colorMask->setRedMasked(true);
            colorMask->setGreenMasked(true);
            colorMask->setBlueMasked(true);
            colorMask->setAlphaMasked(false);
            pass->addRenderState(colorMask);

            auto *depthMask = new Qt3DRender::QNoDepthMask(pass);
            pass->addRenderState(depthMask);
        }
    }
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
