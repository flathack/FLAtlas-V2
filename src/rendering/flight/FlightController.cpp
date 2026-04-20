// rendering/flight/FlightController.cpp – Flugsteuerung (Phase 15)

#include "FlightController.h"
#include <QtMath>
#include <QQuaternion>

namespace flatlas::rendering {

FlightController::FlightController(QObject *parent)
    : QObject(parent)
{
}

void FlightController::setState(State s)
{
    if (m_state == s) return;
    m_state = s;
    emit stateChanged(m_state);
}

void FlightController::toggleCruise()
{
    if (m_state == Docked) return;
    setState(m_state == Cruise ? Normal : Cruise);
}

void FlightController::dock()
{
    m_velocity = QVector3D(0, 0, 0);
    m_throttle = 0;
    setState(Docked);
}

void FlightController::undock()
{
    setState(Normal);
}

void FlightController::setThrottle(float t)
{
    m_throttle = qBound(0.0f, t, 1.0f);
}

void FlightController::setSteering(float yaw, float pitch)
{
    m_steerYaw = qBound(-1.0f, yaw, 1.0f);
    m_steerPitch = qBound(-1.0f, pitch, 1.0f);
}

void FlightController::setAfterburner(bool on)
{
    m_afterburner = on && (m_state == Normal);
}

void FlightController::setPosition(const QVector3D &pos)
{
    m_position = pos;
}

void FlightController::setForward(const QVector3D &fwd)
{
    m_forward = fwd.normalized();
}

void FlightController::update(float dt)
{
    if (m_state == Docked || dt <= 0.0f)
        return;

    // --- Steering: rotate forward vector ---
    float yawAngle = m_steerYaw * m_turnRate * dt;
    float pitchAngle = m_steerPitch * m_turnRate * dt;

    // Yaw around up
    QQuaternion yawQ = QQuaternion::fromAxisAndAngle(m_up, -yawAngle);
    m_forward = yawQ.rotatedVector(m_forward).normalized();

    // Pitch around right
    QVector3D right = QVector3D::crossProduct(m_forward, m_up).normalized();
    QQuaternion pitchQ = QQuaternion::fromAxisAndAngle(right, pitchAngle);
    m_forward = pitchQ.rotatedVector(m_forward).normalized();
    m_up = pitchQ.rotatedVector(m_up).normalized();

    // --- Target speed ---
    float targetSpeed = 0.0f;
    if (m_state == Cruise) {
        targetSpeed = m_cruiseSpeed;
    } else if (m_afterburner) {
        targetSpeed = m_afterburnerSpeed;
    } else {
        targetSpeed = m_throttle * m_maxSpeed;
    }

    // --- Accelerate/decelerate ---
    float currentSpeed = m_velocity.length();
    float desiredSpeed;
    if (currentSpeed < targetSpeed)
        desiredSpeed = qMin(currentSpeed + m_acceleration * dt, targetSpeed);
    else
        desiredSpeed = qMax(currentSpeed - m_deceleration * dt, targetSpeed);

    m_velocity = m_forward * desiredSpeed;

    // --- Move ---
    QVector3D oldPos = m_position;
    m_position += m_velocity * dt;

    if (m_position != oldPos)
        emit positionChanged(m_position);
    if (qAbs(desiredSpeed - currentSpeed) > 0.01f)
        emit speedChanged(desiredSpeed);
}

} // namespace flatlas::rendering
