// rendering/flight/FlightNavigation.cpp – Wegpunkte & Docking (Phase 15)

#include "FlightNavigation.h"
#include <limits>

namespace flatlas::rendering {

FlightNavigation::FlightNavigation(QObject *parent)
    : QObject(parent)
{
}

void FlightNavigation::setNavPoints(const QVector<NavPoint> &points)
{
    m_navPoints = points;
    m_activeTarget = m_navPoints.isEmpty() ? -1 : 0;
}

int FlightNavigation::nearestNavPoint(const QVector3D &pos) const
{
    int best = -1;
    float bestDist = std::numeric_limits<float>::max();
    for (int i = 0; i < m_navPoints.size(); ++i) {
        float d = (m_navPoints[i].position - pos).lengthSquared();
        if (d < bestDist) {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

int FlightNavigation::findNavPoint(const QString &nickname) const
{
    for (int i = 0; i < m_navPoints.size(); ++i) {
        if (m_navPoints[i].nickname.compare(nickname, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

void FlightNavigation::setActiveTarget(int index)
{
    if (index >= -1 && index < m_navPoints.size()) {
        m_activeTarget = index;
        emit activeTargetChanged(index);
    }
}

const NavPoint *FlightNavigation::activeNavPoint() const
{
    if (m_activeTarget >= 0 && m_activeTarget < m_navPoints.size())
        return &m_navPoints[m_activeTarget];
    return nullptr;
}

void FlightNavigation::cycleTarget()
{
    if (m_navPoints.isEmpty()) return;
    m_activeTarget = (m_activeTarget + 1) % m_navPoints.size();
    emit activeTargetChanged(m_activeTarget);
}

bool FlightNavigation::isInDockRange(const QVector3D &pos, float range) const
{
    return dockableNavPoint(pos, range) != nullptr;
}

const NavPoint *FlightNavigation::dockableNavPoint(const QVector3D &pos, float range) const
{
    float rangeSq = range * range;
    for (const auto &np : m_navPoints) {
        if (np.type == NavPoint::Station || np.type == NavPoint::JumpGate) {
            if ((np.position - pos).lengthSquared() < rangeSq)
                return &np;
        }
    }
    return nullptr;
}

} // namespace flatlas::rendering
