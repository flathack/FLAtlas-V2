#pragma once
// rendering/flight/FlightNavigation.h – Wegpunkte & Docking (Phase 15)

#include <QObject>
#include <QVector>
#include <QVector3D>
#include <QString>

namespace flatlas::rendering {

/// A navigation waypoint (jump gate, trade lane ring, station, etc.)
struct NavPoint {
    enum Type { Station, JumpGate, JumpHole, TradeLane, Waypoint };

    QString nickname;
    QString displayName;
    Type type = Waypoint;
    QVector3D position;
    QString targetSystem; // for jump gates/holes
};

/// Manages navigation waypoints for the flight system.
class FlightNavigation : public QObject {
    Q_OBJECT
public:
    explicit FlightNavigation(QObject *parent = nullptr);

    /// Set waypoints (typically from SystemDocument objects).
    void setNavPoints(const QVector<NavPoint> &points);
    const QVector<NavPoint> &navPoints() const { return m_navPoints; }

    /// Find nearest nav point to a position.
    int nearestNavPoint(const QVector3D &pos) const;

    /// Find nav point by nickname.
    int findNavPoint(const QString &nickname) const;

    /// Select a nav point as active target.
    void setActiveTarget(int index);
    int activeTarget() const { return m_activeTarget; }
    const NavPoint *activeNavPoint() const;

    /// Cycle to the next nav point.
    void cycleTarget();

    /// Check if position is within docking range of a station.
    bool isInDockRange(const QVector3D &pos, float range = 500.0f) const;

    /// Get the dockable nav point if in range, or nullptr.
    const NavPoint *dockableNavPoint(const QVector3D &pos, float range = 500.0f) const;

    int navPointCount() const { return m_navPoints.size(); }

signals:
    void activeTargetChanged(int index);

private:
    QVector<NavPoint> m_navPoints;
    int m_activeTarget = -1;
};

} // namespace flatlas::rendering
