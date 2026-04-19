// rendering/view3d/SkyRenderer.cpp – Hintergrund-Rendering (Phase 7)

#ifdef FLATLAS_HAS_QT3D

#include "SkyRenderer.h"

namespace flatlas::rendering {

SkyRenderer::SkyRenderer(Qt3DCore::QNode *parent)
    : Qt3DCore::QEntity(parent)
{
    m_mesh = new Qt3DExtras::QSphereMesh(this);
    m_mesh->setRadius(400000.0f);
    m_mesh->setRings(32);
    m_mesh->setSlices(32);
    m_mesh->setGenerateTangents(false);

    m_material = new Qt3DExtras::QPhongMaterial(this);
    m_material->setAmbient(QColor(5, 5, 15));
    m_material->setDiffuse(QColor(2, 2, 8));
    m_material->setSpecular(QColor(0, 0, 0));
    m_material->setShininess(0);

    m_transform = new Qt3DCore::QTransform(this);

    addComponent(m_mesh);
    addComponent(m_material);
    addComponent(m_transform);
}

void SkyRenderer::setRadius(float radius)
{
    m_mesh->setRadius(radius);
}

} // namespace flatlas::rendering

#endif // FLATLAS_HAS_QT3D
