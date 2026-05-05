#pragma once
// rendering/view3d/FreeCameraController.h - free-flight camera controller for the 3D system view

#ifdef FLATLAS_HAS_QT3D

#include <QElapsedTimer>
#include <QObject>
#include <QPointF>
#include <QSet>
#include <QVector3D>

#include <Qt3DRender/QCamera>

class QKeyEvent;
class QMouseEvent;
class QWheelEvent;

namespace flatlas::rendering {

class FreeCameraController : public QObject {
    Q_OBJECT
public:
    explicit FreeCameraController(Qt3DRender::QCamera *camera, QObject *parent = nullptr);

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }
    void setFreelancerFlightModeEnabled(bool enabled);
    void setFreelancerFlightProfile(float normalSpeed, float cruiseSpeed, float cruiseChargeTime);
    void setThirdPersonCamera(float fovX, float zNear);
    void beginCruise();
    void cancelCruise();

    void setSpeed(float speed);
    float speed() const { return m_speed; }
    QVector3D shipPosition() const { return m_position; }
    QVector3D shipForward() const { return forwardVector(); }
    bool cruiseActive() const { return m_cruiseActive; }
    bool cruiseCharging() const { return m_cruiseCharging; }
    float minSpeed() const { return m_minSpeed; }
    float maxSpeed() const { return m_maxSpeed; }

    void synchronizeFromCamera();
    void setPose(const QVector3D &position, const QVector3D &forward);
    void update(float deltaSeconds);

    void handleMousePress(QMouseEvent *event);
    void handleMouseMove(QMouseEvent *event);
    void handleMouseRelease(QMouseEvent *event);
    void handleWheel(QWheelEvent *event);
    void handleKeyPress(QKeyEvent *event);
    void handleKeyRelease(QKeyEvent *event);

signals:
    void cameraChanged();
    void speedChanged(float speed);
    void enabledChanged(bool enabled);

private:
    void applyCamera();
    QVector3D forwardVector() const;
    QVector3D rightVector() const;

    Qt3DRender::QCamera *m_camera = nullptr;
    bool m_enabled = false;
    bool m_looking = false;
    QPointF m_lastMousePos;
    QSet<int> m_pressedKeys;
    QVector3D m_position{0.0f, 0.0f, 0.0f};
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    float m_speed = 12000.0f;
    float m_minSpeed = 250.0f;
    float m_maxSpeed = 500000.0f;
    float m_mouseSensitivity = 0.18f;
    bool m_freelancerFlightMode = false;
    bool m_cruiseCharging = false;
    bool m_cruiseActive = false;
    float m_normalFlightSpeed = 80.0f;
    float m_cruiseSpeed = 300.0f;
    float m_cruiseChargeTime = 5.0f;
    float m_cruiseChargeElapsed = 0.0f;
    float m_thirdPersonDistance = 620.0f;
    float m_thirdPersonHeight = 155.0f;
    float m_thirdPersonLookAhead = 850.0f;
};

} // namespace flatlas::rendering

#endif // FLATLAS_HAS_QT3D
