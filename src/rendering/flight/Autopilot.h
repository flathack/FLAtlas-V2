#pragma once
// rendering/flight/Autopilot.h – Automatische Navigation (Phase 15)

#include <QObject>
#include <QVector3D>

namespace flatlas::rendering {

class FlightController;

/// Autopilot: flies towards a target, activates cruise, handles docking approach.
class Autopilot : public QObject {
    Q_OBJECT
public:
    enum Mode { Off, Goto, DockApproach };
    Q_ENUM(Mode)

    explicit Autopilot(QObject *parent = nullptr);

    void setController(FlightController *ctrl) { m_controller = ctrl; }

    /// Start autopilot to a target position.
    void gotoTarget(const QVector3D &target, const QString &name = {});

    /// Start docking approach (slower, precise).
    void dockAt(const QVector3D &dockPos, const QString &baseName = {});

    /// Cancel autopilot.
    void cancel();

    /// Per-frame update.
    void update(float dt);

    Mode mode() const { return m_mode; }
    QString targetName() const { return m_targetName; }
    QVector3D targetPosition() const { return m_targetPos; }
    float distanceToTarget() const;

signals:
    void arrivedAtTarget(const QString &name);
    void modeChanged(Mode mode);

private:
    void steerToward(const QVector3D &target, float dt);

    FlightController *m_controller = nullptr;
    Mode m_mode = Off;
    QVector3D m_targetPos;
    QString m_targetName;
    float m_arrivalRadius = 500.0f;
    float m_dockRadius = 100.0f;
    float m_cruiseEngageDistance = 5000.0f;
};

} // namespace flatlas::rendering
