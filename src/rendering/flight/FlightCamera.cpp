// rendering/flight/FlightCamera.cpp – Chase-Kamera (Phase 15)

#include "FlightCamera.h"
#include "FlightController.h"

#ifdef FLATLAS_HAS_QT3D
#include <Qt3DRender/QCamera>
#endif

namespace flatlas::rendering {

FlightCamera::FlightCamera(QObject *parent)
    : QObject(parent)
{
}

void FlightCamera::update(float dt)
{
    if (!m_controller || dt <= 0.0f)
        return;

    QVector3D shipPos = m_controller->position();
    QVector3D shipFwd = m_controller->forward();
    QVector3D shipUp = m_controller->up();

    // Desired camera position: behind and above the ship
    QVector3D desiredPos = shipPos
        - shipFwd * m_followDistance
        + shipUp * m_followHeight;

    // Smooth interpolation (exponential)
    float t = 1.0f - qExp(-m_smoothFactor * dt);
    m_cameraPos = m_cameraPos + (desiredPos - m_cameraPos) * t;

    // Look slightly ahead of the ship
    m_lookAt = shipPos + shipFwd * 500.0f;

#ifdef FLATLAS_HAS_QT3D
    if (m_camera) {
        m_camera->setPosition(m_cameraPos);
        m_camera->setViewCenter(m_lookAt);
        m_camera->setUpVector(shipUp);
    }
#endif
}

} // namespace flatlas::rendering
