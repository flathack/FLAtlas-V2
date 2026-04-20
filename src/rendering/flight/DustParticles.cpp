// rendering/flight/DustParticles.cpp – Geschwindigkeits-Partikel (Phase 15)

#include "DustParticles.h"
#include "FlightController.h"

#include <QRandomGenerator>

#ifdef FLATLAS_HAS_QT3D
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DExtras/QPhongAlphaMaterial>
#endif

namespace flatlas::rendering {

DustParticles::DustParticles(QObject *parent)
    : QObject(parent)
{
}

#ifdef FLATLAS_HAS_QT3D
void DustParticles::setParentEntity(Qt3DCore::QEntity *parent)
{
    m_parentEntity = parent;

    // Shared mesh & material for all particles
    m_sharedMesh = new Qt3DExtras::QSphereMesh(parent);
    m_sharedMesh->setRadius(3.0f);
    m_sharedMesh->setSlices(4);
    m_sharedMesh->setRings(4);

    m_sharedMaterial = new Qt3DExtras::QPhongAlphaMaterial(parent);
    m_sharedMaterial->setDiffuse(QColor(180, 200, 255));
    m_sharedMaterial->setAmbient(QColor(180, 200, 255));
    m_sharedMaterial->setAlpha(0.6f);

    // Pre-create particle entities
    m_particles.resize(m_particleCount);
    for (auto &p : m_particles) {
        p.entity = new Qt3DCore::QEntity(m_parentEntity);
        p.transform = new Qt3DCore::QTransform(p.entity);
        p.entity->addComponent(m_sharedMesh);
        p.entity->addComponent(m_sharedMaterial);
        p.entity->addComponent(p.transform);
        p.entity->setEnabled(false);
        p.lifetime = 0;
    }
}
#endif

int DustParticles::activeParticles() const
{
    int count = 0;
    for (const auto &p : m_particles)
        if (p.lifetime > 0) ++count;
    return count;
}

void DustParticles::spawnParticle(Particle &p)
{
    if (!m_controller) return;

    auto *rng = QRandomGenerator::global();
    QVector3D shipPos = m_controller->position();
    QVector3D shipFwd = m_controller->forward();

    // Spawn ahead and around the ship
    float x = (rng->generateDouble() - 0.5) * 2.0 * m_spawnRadius;
    float y = (rng->generateDouble() - 0.5) * 2.0 * m_spawnRadius;
    float z = (rng->generateDouble() - 0.5) * 2.0 * m_spawnRadius;

    p.position = shipPos + shipFwd * m_spawnRadius + QVector3D(x, y, z);
    p.lifetime = m_maxLifetime;
}

void DustParticles::update(float dt)
{
    if (!m_controller || dt <= 0.0f)
        return;

    float speed = m_controller->speed();
    float speedRatio = qBound(0.0f, speed / m_controller->maxSpeed(), 1.0f);

    // Only show particles when moving
    if (speed < 10.0f) {
#ifdef FLATLAS_HAS_QT3D
        for (auto &p : m_particles) {
            p.lifetime = 0;
            if (p.entity) p.entity->setEnabled(false);
        }
#endif
        return;
    }

    // Ensure particle array is sized
    if (m_particles.size() != m_particleCount)
        m_particles.resize(m_particleCount);

    int activeTarget = static_cast<int>(m_particleCount * speedRatio);

    for (int i = 0; i < m_particles.size(); ++i) {
        auto &p = m_particles[i];

        if (p.lifetime > 0) {
            p.lifetime -= dt;
            // Particles drift backward relative to ship (world-stationary)
            // The ship moves forward, so particles move to behind the ship
#ifdef FLATLAS_HAS_QT3D
            if (p.transform)
                p.transform->setTranslation(p.position);
            if (p.entity)
                p.entity->setEnabled(p.lifetime > 0);
#endif
        } else if (i < activeTarget) {
            // Respawn
            spawnParticle(p);
#ifdef FLATLAS_HAS_QT3D
            if (p.entity) p.entity->setEnabled(true);
#endif
        }
    }
}

} // namespace flatlas::rendering
