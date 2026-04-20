#pragma once
// rendering/flight/FlightCamera.h – Chase-Kamera (Phase 15)

#include <QObject>
#include <QVector3D>

#ifdef FLATLAS_HAS_QT3D
namespace Qt3DRender { class QCamera; }
#endif

namespace flatlas::rendering {

class FlightController;

/// Freelancer-style chase camera: follows behind the player with smooth lag.
class FlightCamera : public QObject {
    Q_OBJECT
public:
    explicit FlightCamera(QObject *parent = nullptr);

    void setController(FlightController *ctrl) { m_controller = ctrl; }

#ifdef FLATLAS_HAS_QT3D
    void setCamera(Qt3DRender::QCamera *cam) { m_camera = cam; }
#endif

    /// Per-frame update. dt in seconds.
    void update(float dt);

    // Tuning
    void setFollowDistance(float d) { m_followDistance = d; }
    void setFollowHeight(float h) { m_followHeight = h; }
    void setSmoothFactor(float f) { m_smoothFactor = f; }

    float followDistance() const { return m_followDistance; }
    float followHeight() const { return m_followHeight; }

    QVector3D cameraPosition() const { return m_cameraPos; }
    QVector3D lookAtTarget() const { return m_lookAt; }

private:
    FlightController *m_controller = nullptr;
#ifdef FLATLAS_HAS_QT3D
    Qt3DRender::QCamera *m_camera = nullptr;
#endif

    QVector3D m_cameraPos{0, 500, 2000};
    QVector3D m_lookAt{0, 0, 0};

    float m_followDistance = 2000.0f;
    float m_followHeight = 500.0f;
    float m_smoothFactor = 5.0f; // higher = snappier
};

} // namespace flatlas::rendering
