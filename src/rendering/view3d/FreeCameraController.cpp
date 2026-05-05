// rendering/view3d/FreeCameraController.cpp - free-flight camera controller

#ifdef FLATLAS_HAS_QT3D

#include "FreeCameraController.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QtMath>

namespace flatlas::rendering {

FreeCameraController::FreeCameraController(Qt3DRender::QCamera *camera, QObject *parent)
    : QObject(parent)
    , m_camera(camera)
{
    synchronizeFromCamera();
}

void FreeCameraController::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    m_pressedKeys.clear();
    m_looking = false;
    m_cruiseCharging = false;
    m_cruiseActive = false;
    m_cruiseChargeElapsed = 0.0f;
    if (m_enabled)
        synchronizeFromCamera();
    emit enabledChanged(m_enabled);
}

void FreeCameraController::setFreelancerFlightModeEnabled(bool enabled)
{
    if (m_freelancerFlightMode == enabled)
        return;
    m_freelancerFlightMode = enabled;
    m_cruiseCharging = false;
    m_cruiseActive = false;
    m_cruiseChargeElapsed = 0.0f;
    setSpeed(m_freelancerFlightMode ? m_normalFlightSpeed : m_speed);
}

void FreeCameraController::setFreelancerFlightProfile(float normalSpeed, float cruiseSpeed, float cruiseChargeTime)
{
    m_normalFlightSpeed = qMax(1.0f, normalSpeed);
    m_cruiseSpeed = qMax(m_normalFlightSpeed, cruiseSpeed);
    m_cruiseChargeTime = qMax(0.0f, cruiseChargeTime);
    if (m_freelancerFlightMode && !m_cruiseActive)
        setSpeed(m_normalFlightSpeed);
}

void FreeCameraController::setThirdPersonCamera(float fovX, float zNear)
{
    if (!m_camera)
        return;
    m_camera->lens()->setPerspectiveProjection(qBound(35.0f, fovX, 120.0f),
                                               16.0f / 9.0f,
                                               qMax(0.1f, zNear),
                                               6000000.0f);
}

void FreeCameraController::beginCruise()
{
    if (!m_enabled || !m_freelancerFlightMode || m_cruiseActive || m_cruiseCharging)
        return;
    m_cruiseCharging = true;
    m_cruiseChargeElapsed = 0.0f;
    setSpeed(m_normalFlightSpeed);
}

void FreeCameraController::cancelCruise()
{
    if (!m_freelancerFlightMode)
        return;
    m_cruiseActive = false;
    m_cruiseCharging = false;
    m_cruiseChargeElapsed = 0.0f;
    setSpeed(m_normalFlightSpeed);
}

void FreeCameraController::setSpeed(float speed)
{
    const float nextSpeed = qBound(m_freelancerFlightMode ? 1.0f : m_minSpeed, speed, m_maxSpeed);
    if (qFuzzyCompare(m_speed, nextSpeed))
        return;
    m_speed = nextSpeed;
    emit speedChanged(m_speed);
}

void FreeCameraController::synchronizeFromCamera()
{
    if (!m_camera)
        return;

    m_position = m_camera->position();
    const QVector3D forward = m_camera->viewVector().normalized();
    if (forward.lengthSquared() <= 0.0001f)
        return;

    m_pitch = qRadiansToDegrees(std::asin(qBound(-1.0f, forward.y(), 1.0f)));
    m_yaw = qRadiansToDegrees(std::atan2(forward.x(), forward.z()));
    applyCamera();
}

void FreeCameraController::setPose(const QVector3D &position, const QVector3D &forward)
{
    const QVector3D normalized = forward.lengthSquared() > 0.0001f
        ? forward.normalized()
        : QVector3D(0.0f, 0.0f, -1.0f);
    m_position = position;
    m_pitch = qRadiansToDegrees(std::asin(qBound(-1.0f, normalized.y(), 1.0f)));
    m_yaw = qRadiansToDegrees(std::atan2(normalized.x(), normalized.z()));
    applyCamera();
}

void FreeCameraController::update(float deltaSeconds)
{
    if (!m_enabled || !m_camera || deltaSeconds <= 0.0f)
        return;

    if (m_freelancerFlightMode && m_cruiseCharging) {
        m_cruiseChargeElapsed += deltaSeconds;
        if (m_cruiseChargeTime <= 0.0f || m_cruiseChargeElapsed >= m_cruiseChargeTime) {
            m_cruiseCharging = false;
            m_cruiseActive = true;
            setSpeed(m_cruiseSpeed);
        }
    }

    QVector3D movement;
    const QVector3D forward = forwardVector();
    const QVector3D right = rightVector();
    const QVector3D up(0.0f, 1.0f, 0.0f);

    if (m_pressedKeys.contains(Qt::Key_W) || m_pressedKeys.contains(Qt::Key_Up))
        movement += forward;
    if (m_freelancerFlightMode && m_cruiseActive)
        movement += forward;
    if (m_pressedKeys.contains(Qt::Key_S) || m_pressedKeys.contains(Qt::Key_Down)) {
        if (!m_freelancerFlightMode)
            movement -= forward;
    }
    if (m_pressedKeys.contains(Qt::Key_D))
        movement += right;
    if (m_pressedKeys.contains(Qt::Key_A))
        movement -= right;
    if (m_pressedKeys.contains(Qt::Key_Space))
        movement += up;
    if (m_pressedKeys.contains(Qt::Key_Control) || m_pressedKeys.contains(Qt::Key_Meta))
        movement -= up;

    if (movement.lengthSquared() <= 0.0001f)
        return;

    m_position += movement.normalized() * m_speed * deltaSeconds;
    applyCamera();
}

void FreeCameraController::handleMousePress(QMouseEvent *event)
{
    if (!event || !m_enabled)
        return;
    m_lastMousePos = event->globalPosition();
    if (event->button() == Qt::LeftButton) {
        m_looking = true;
        event->accept();
    }
}

void FreeCameraController::handleMouseMove(QMouseEvent *event)
{
    if (!event || !m_enabled)
        return;

    const QPointF currentPos = event->globalPosition();
    const QPointF delta = currentPos - m_lastMousePos;
    m_lastMousePos = currentPos;
    if (!m_looking)
        return;

    m_yaw -= static_cast<float>(delta.x()) * m_mouseSensitivity;
    m_pitch = qBound(-89.0f, m_pitch - static_cast<float>(delta.y()) * m_mouseSensitivity, 89.0f);
    applyCamera();
    event->accept();
}

void FreeCameraController::handleMouseRelease(QMouseEvent *event)
{
    if (!event || !m_enabled)
        return;
    if (event->button() == Qt::LeftButton) {
        m_looking = false;
        event->accept();
    }
}

void FreeCameraController::handleWheel(QWheelEvent *event)
{
    if (!event || !m_enabled)
        return;
    if (m_freelancerFlightMode) {
        event->accept();
        return;
    }
    const float factor = event->angleDelta().y() > 0 ? 1.25f : 0.8f;
    setSpeed(m_speed * factor);
    event->accept();
}

void FreeCameraController::handleKeyPress(QKeyEvent *event)
{
    if (!event || !m_enabled || event->isAutoRepeat())
        return;
    if (m_freelancerFlightMode && event->key() == Qt::Key_C) {
        if (m_cruiseActive || m_cruiseCharging)
            cancelCruise();
        else
            beginCruise();
        event->accept();
        return;
    }
    if (m_freelancerFlightMode && event->key() == Qt::Key_S) {
        cancelCruise();
        event->accept();
        return;
    }
    m_pressedKeys.insert(event->key());
    event->accept();
}

void FreeCameraController::handleKeyRelease(QKeyEvent *event)
{
    if (!event || !m_enabled || event->isAutoRepeat())
        return;
    m_pressedKeys.remove(event->key());
    event->accept();
}

void FreeCameraController::applyCamera()
{
    if (!m_camera)
        return;
    const QVector3D forward = forwardVector();
    if (m_freelancerFlightMode) {
        const QVector3D cameraPosition =
            m_position - forward * m_thirdPersonDistance + QVector3D(0.0f, m_thirdPersonHeight, 0.0f);
        m_camera->setPosition(cameraPosition);
        m_camera->setViewCenter(m_position + forward * m_thirdPersonLookAhead);
    } else {
        m_camera->setPosition(m_position);
        m_camera->setViewCenter(m_position + forward);
    }
    m_camera->setUpVector(QVector3D(0.0f, 1.0f, 0.0f));
    emit cameraChanged();
}

QVector3D FreeCameraController::forwardVector() const
{
    const float yawRad = qDegreesToRadians(m_yaw);
    const float pitchRad = qDegreesToRadians(m_pitch);
    return QVector3D(qSin(yawRad) * qCos(pitchRad),
                     qSin(pitchRad),
                     qCos(yawRad) * qCos(pitchRad)).normalized();
}

QVector3D FreeCameraController::rightVector() const
{
    return QVector3D::crossProduct(forwardVector(), QVector3D(0.0f, 1.0f, 0.0f)).normalized();
}

} // namespace flatlas::rendering

#endif // FLATLAS_HAS_QT3D
