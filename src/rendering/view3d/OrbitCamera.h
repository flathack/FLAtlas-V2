#pragma once
// rendering/view3d/OrbitCamera.h – Orbit-Kamera für 3D-View (Phase 7)

#ifdef FLATLAS_HAS_QT3D

#include <QObject>
#include <QVector3D>
#include <Qt>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DRender/QCamera>

class QMouseEvent;
class QWheelEvent;

namespace flatlas::rendering {

/// Orbit camera controller: rotate, pan, and zoom from widget mouse/wheel events.
class OrbitCamera : public QObject {
    Q_OBJECT
public:
    explicit OrbitCamera(Qt3DRender::QCamera *camera, QObject *parent = nullptr);

    void setTarget(const QVector3D &target);
    QVector3D target() const { return m_target; }

    void setDistance(float distance);
    float distance() const { return m_distance; }

    void setAzimuth(float degrees);
    float azimuth() const { return m_azimuth; }

    void setElevation(float degrees);
    float elevation() const { return m_elevation; }

    /// Call from the widget's mouse/wheel event handlers.
    void handleMousePress(QMouseEvent *event);
    void handleMouseMove(QMouseEvent *event);
    void handleMouseRelease(QMouseEvent *event);
    void handleWheel(QWheelEvent *event);

    /// Reset to default view.
    void resetView();
    void setResetState(const QVector3D &target, float distance,
                       float azimuthDegrees, float elevationDegrees);
    void setRotateButton(Qt::MouseButton button) { m_rotateButton = button; }
    void setPanButton(Qt::MouseButton button) { m_panButton = button; }

    /// Update camera transform from current parameters.
    void updateCamera();

signals:
    void cameraChanged();

private:
    Qt3DRender::QCamera *m_camera = nullptr;
    QVector3D m_target{0, 0, 0};
    float m_distance = 50000.0f;
    float m_azimuth = 45.0f;
    float m_elevation = 30.0f;
    QVector3D m_defaultTarget{0, 0, 0};
    float m_defaultDistance = 50000.0f;
    float m_defaultAzimuth = 45.0f;
    float m_defaultElevation = 30.0f;
    float m_minDistance = 100.0f;
    float m_maxDistance = 500000.0f;
    Qt::MouseButton m_rotateButton = Qt::RightButton;
    Qt::MouseButton m_panButton = Qt::MiddleButton;

    QPoint m_lastMousePos;
    bool m_rotating = false;
    bool m_panning = false;
};

} // namespace flatlas::rendering

#endif // FLATLAS_HAS_QT3D
