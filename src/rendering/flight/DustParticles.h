#pragma once
// rendering/flight/DustParticles.h – Geschwindigkeits-Partikel (Phase 15)

#include <QObject>
#include <QVector3D>
#include <QVector>

#ifdef FLATLAS_HAS_QT3D
namespace Qt3DCore { class QEntity; class QTransform; }
namespace Qt3DExtras { class QSphereMesh; class QPhongAlphaMaterial; }
#endif

namespace flatlas::rendering {

class FlightController;

/// Dust particles that visualize speed — more and faster streaks at higher velocity.
class DustParticles : public QObject {
    Q_OBJECT
public:
    explicit DustParticles(QObject *parent = nullptr);

    void setController(FlightController *ctrl) { m_controller = ctrl; }

#ifdef FLATLAS_HAS_QT3D
    void setParentEntity(Qt3DCore::QEntity *parent);
#endif

    /// Per-frame update.
    void update(float dt);

    void setParticleCount(int count) { m_particleCount = count; }
    void setSpawnRadius(float r) { m_spawnRadius = r; }
    int particleCount() const { return m_particleCount; }
    int activeParticles() const;

private:
    struct Particle {
        QVector3D position;
        float lifetime = 0.0f;
#ifdef FLATLAS_HAS_QT3D
        Qt3DCore::QEntity *entity = nullptr;
        Qt3DCore::QTransform *transform = nullptr;
#endif
    };

    void spawnParticle(Particle &p);

    FlightController *m_controller = nullptr;
    QVector<Particle> m_particles;
    int m_particleCount = 60;
    float m_spawnRadius = 3000.0f;
    float m_maxLifetime = 2.0f;

#ifdef FLATLAS_HAS_QT3D
    Qt3DCore::QEntity *m_parentEntity = nullptr;
    Qt3DExtras::QSphereMesh *m_sharedMesh = nullptr;
    Qt3DExtras::QPhongAlphaMaterial *m_sharedMaterial = nullptr;
#endif
};

} // namespace flatlas::rendering
