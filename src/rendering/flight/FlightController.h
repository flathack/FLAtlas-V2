#pragma once
// rendering/flight/FlightController.h – Flugsteuerung (Phase 15)

#include <QObject>
#include <QVector3D>
#include <QElapsedTimer>

namespace flatlas::rendering {

/// Freelancer-style flight controller with state machine and physics.
class FlightController : public QObject {
    Q_OBJECT
public:
    enum State { Docked, Normal, Cruise, Formation };
    Q_ENUM(State)

    explicit FlightController(QObject *parent = nullptr);

    // --- State ---
    State state() const { return m_state; }
    void setState(State s);
    void toggleCruise();
    void dock();
    void undock();

    // --- Input ---
    void setThrottle(float t);           // 0..1
    void setSteering(float yaw, float pitch); // -1..1 each
    void setAfterburner(bool on);

    // --- Physics tick ---
    void update(float dt);               // seconds

    // --- Getters ---
    QVector3D position() const { return m_position; }
    QVector3D velocity() const { return m_velocity; }
    QVector3D forward() const { return m_forward; }
    QVector3D up() const { return m_up; }
    float speed() const { return m_velocity.length(); }
    float throttle() const { return m_throttle; }
    bool afterburnerActive() const { return m_afterburner; }

    void setPosition(const QVector3D &pos);
    void setForward(const QVector3D &fwd);

    // --- Tuning ---
    float maxSpeed() const { return m_maxSpeed; }
    float cruiseSpeed() const { return m_cruiseSpeed; }
    void setMaxSpeed(float s) { m_maxSpeed = s; }
    void setCruiseSpeed(float s) { m_cruiseSpeed = s; }

signals:
    void stateChanged(State newState);
    void positionChanged(const QVector3D &pos);
    void speedChanged(float speed);

private:
    State m_state = Docked;
    QVector3D m_position{0, 0, 0};
    QVector3D m_velocity{0, 0, 0};
    QVector3D m_forward{0, 0, -1};
    QVector3D m_up{0, 1, 0};

    float m_throttle = 0.0f;
    float m_steerYaw = 0.0f;
    float m_steerPitch = 0.0f;
    bool m_afterburner = false;

    // Tuning constants
    float m_maxSpeed = 300.0f;
    float m_cruiseSpeed = 1500.0f;
    float m_afterburnerSpeed = 600.0f;
    float m_acceleration = 400.0f;
    float m_deceleration = 200.0f;
    float m_turnRate = 90.0f; // degrees/sec
};

} // namespace flatlas::rendering
