// rendering/view3d/OrbitCamera.cpp – Orbit-Kamera (Phase 7)

#ifdef FLATLAS_HAS_QT3D

#include "OrbitCamera.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QtMath>

namespace flatlas::rendering {

OrbitCamera::OrbitCamera(Qt3DRender::QCamera *camera, QObject *parent)
    : QObject(parent), m_camera(camera)
{
    updateCamera();
}

void OrbitCamera::setTarget(const QVector3D &target)
{
    m_target = target;
    updateCamera();
}

void OrbitCamera::setDistance(float distance)
{
    m_distance = qBound(m_minDistance, distance, m_maxDistance);
    updateCamera();
}

void OrbitCamera::setAzimuth(float degrees)
{
    m_azimuth = degrees;
    updateCamera();
}

void OrbitCamera::setElevation(float degrees)
{
    m_elevation = qBound(-89.0f, degrees, 89.0f);
    updateCamera();
}

void OrbitCamera::handleMousePress(QMouseEvent *event)
{
    m_lastMousePos = event->pos();
    if (event->button() == m_rotateButton)
        m_rotating = true;
    else if (event->button() == m_panButton)
        m_panning = true;
}

void OrbitCamera::handleMouseMove(QMouseEvent *event)
{
    const QPoint delta = event->pos() - m_lastMousePos;
    m_lastMousePos = event->pos();

    if (m_rotating) {
        m_azimuth += delta.x() * 0.3f;
        m_elevation = qBound(-89.0f, m_elevation - delta.y() * 0.3f, 89.0f);
        updateCamera();
    } else if (m_panning) {
        // Pan in camera-local XY plane
        const float panSpeed = m_distance * 0.001f;
        const QVector3D right = QVector3D::crossProduct(
            m_camera->viewVector(), m_camera->upVector()).normalized();
        const QVector3D up = m_camera->upVector().normalized();
        m_target -= right * delta.x() * panSpeed;
        m_target += up * delta.y() * panSpeed;
        updateCamera();
    }
}

void OrbitCamera::handleMouseRelease(QMouseEvent *event)
{
    if (event->button() == m_rotateButton)
        m_rotating = false;
    else if (event->button() == m_panButton)
        m_panning = false;
}

void OrbitCamera::handleWheel(QWheelEvent *event)
{
    const float factor = event->angleDelta().y() > 0 ? 0.85f : 1.15f;
    m_distance = qBound(m_minDistance, m_distance * factor, m_maxDistance);
    updateCamera();
}

void OrbitCamera::resetView()
{
    m_target = m_defaultTarget;
    m_distance = m_defaultDistance;
    m_azimuth = m_defaultAzimuth;
    m_elevation = m_defaultElevation;
    updateCamera();
}

void OrbitCamera::setResetState(const QVector3D &target, float distance,
                                float azimuthDegrees, float elevationDegrees)
{
    m_defaultTarget = target;
    m_defaultDistance = qBound(m_minDistance, distance, m_maxDistance);
    m_defaultAzimuth = azimuthDegrees;
    m_defaultElevation = qBound(-89.0f, elevationDegrees, 89.0f);
}

void OrbitCamera::updateCamera()
{
    if (!m_camera)
        return;

    const float azRad = qDegreesToRadians(m_azimuth);
    const float elRad = qDegreesToRadians(m_elevation);

    const float x = m_distance * qCos(elRad) * qSin(azRad);
    const float y = m_distance * qSin(elRad);
    const float z = m_distance * qCos(elRad) * qCos(azRad);

    m_camera->setPosition(m_target + QVector3D(x, y, z));
    m_camera->setViewCenter(m_target);
    m_camera->setUpVector(QVector3D(0, 1, 0));

    emit cameraChanged();
}

} // namespace flatlas::rendering

#endif // FLATLAS_HAS_QT3D
