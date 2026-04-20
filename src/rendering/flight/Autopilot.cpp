// rendering/flight/Autopilot.cpp – Automatische Navigation (Phase 15)

#include "Autopilot.h"
#include "FlightController.h"
#include <QtMath>

namespace flatlas::rendering {

Autopilot::Autopilot(QObject *parent)
    : QObject(parent)
{
}

void Autopilot::gotoTarget(const QVector3D &target, const QString &name)
{
    m_targetPos = target;
    m_targetName = name;
    m_mode = Goto;
    if (m_controller && m_controller->state() == FlightController::Docked)
        m_controller->undock();
    emit modeChanged(m_mode);
}

void Autopilot::dockAt(const QVector3D &dockPos, const QString &baseName)
{
    m_targetPos = dockPos;
    m_targetName = baseName;
    m_mode = DockApproach;
    if (m_controller && m_controller->state() == FlightController::Docked)
        m_controller->undock();
    emit modeChanged(m_mode);
}

void Autopilot::cancel()
{
    m_mode = Off;
    if (m_controller) {
        m_controller->setThrottle(0);
        if (m_controller->state() == FlightController::Cruise)
            m_controller->toggleCruise();
    }
    emit modeChanged(m_mode);
}

float Autopilot::distanceToTarget() const
{
    if (!m_controller) return 0.0f;
    return (m_targetPos - m_controller->position()).length();
}

void Autopilot::update(float dt)
{
    if (m_mode == Off || !m_controller || dt <= 0.0f)
        return;

    float dist = distanceToTarget();

    if (m_mode == Goto) {
        // Engage cruise if far enough
        if (dist > m_cruiseEngageDistance &&
            m_controller->state() == FlightController::Normal) {
            m_controller->toggleCruise();
        }
        // Disengage cruise when close
        if (dist < m_cruiseEngageDistance &&
            m_controller->state() == FlightController::Cruise) {
            m_controller->toggleCruise();
        }

        steerToward(m_targetPos, dt);
        m_controller->setThrottle(1.0f);

        // Arrival check
        if (dist < m_arrivalRadius) {
            m_controller->setThrottle(0);
            m_mode = Off;
            emit arrivedAtTarget(m_targetName);
            emit modeChanged(m_mode);
        }
    } else if (m_mode == DockApproach) {
        // Always normal speed for docking
        if (m_controller->state() == FlightController::Cruise)
            m_controller->toggleCruise();

        steerToward(m_targetPos, dt);

        // Slow down as we approach
        float throttle = qBound(0.1f, dist / 2000.0f, 0.5f);
        m_controller->setThrottle(throttle);

        if (dist < m_dockRadius) {
            m_controller->dock();
            m_mode = Off;
            emit arrivedAtTarget(m_targetName);
            emit modeChanged(m_mode);
        }
    }
}

void Autopilot::steerToward(const QVector3D &target, float /*dt*/)
{
    if (!m_controller) return;

    QVector3D toTarget = (target - m_controller->position()).normalized();
    QVector3D fwd = m_controller->forward();

    // Compute yaw and pitch steering
    QVector3D right = QVector3D::crossProduct(fwd, m_controller->up()).normalized();
    QVector3D up = m_controller->up();

    float yawDot = QVector3D::dotProduct(toTarget, right);
    float pitchDot = QVector3D::dotProduct(toTarget, up);

    // Proportional steering
    m_controller->setSteering(
        qBound(-1.0f, -yawDot * 3.0f, 1.0f),
        qBound(-1.0f, pitchDot * 3.0f, 1.0f)
    );
}

} // namespace flatlas::rendering
