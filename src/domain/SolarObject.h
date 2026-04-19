#pragma once
// domain/SolarObject.h – Freelancer-Objekt (Planet, Station, etc.)

#include <QObject>
#include <QString>
#include <QVector3D>

namespace flatlas::domain {

class SolarObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString nickname READ nickname WRITE setNickname NOTIFY changed)
    Q_PROPERTY(QVector3D position READ position WRITE setPosition NOTIFY changed)
    Q_PROPERTY(Type type READ type WRITE setType NOTIFY changed)
    Q_PROPERTY(QString base READ base WRITE setBase NOTIFY changed)
    Q_PROPERTY(QString dockWith READ dockWith WRITE setDockWith NOTIFY changed)
    Q_PROPERTY(QString gotoTarget READ gotoTarget WRITE setGotoTarget NOTIFY changed)
    Q_PROPERTY(QString loadout READ loadout WRITE setLoadout NOTIFY changed)
    Q_PROPERTY(QString comment READ comment WRITE setComment NOTIFY changed)

public:
    enum Type {
        Sun, Planet, Station, JumpGate, JumpHole, TradeLane,
        DockingRing, Satellite, Waypoint, Weapons_Platform,
        Depot, Wreck, Asteroid, Other
    };
    Q_ENUM(Type)

    explicit SolarObject(QObject *parent = nullptr) : QObject(parent) {}

    QString nickname() const { return m_nickname; }
    void setNickname(const QString &n) { m_nickname = n; emit changed(); }

    QVector3D position() const { return m_position; }
    void setPosition(const QVector3D &p) { m_position = p; emit changed(); }

    QString archetype() const { return m_archetype; }
    void setArchetype(const QString &a) { m_archetype = a; emit changed(); }

    int idsName() const { return m_idsName; }
    void setIdsName(int id) { m_idsName = id; emit changed(); }

    int idsInfo() const { return m_idsInfo; }
    void setIdsInfo(int id) { m_idsInfo = id; emit changed(); }

    QVector3D rotation() const { return m_rotation; }
    void setRotation(const QVector3D &r) { m_rotation = r; emit changed(); }

    Type type() const { return m_type; }
    void setType(Type t) { m_type = t; emit changed(); }

    QString base() const { return m_base; }
    void setBase(const QString &b) { m_base = b; emit changed(); }

    QString dockWith() const { return m_dockWith; }
    void setDockWith(const QString &d) { m_dockWith = d; emit changed(); }

    QString gotoTarget() const { return m_gotoTarget; }
    void setGotoTarget(const QString &g) { m_gotoTarget = g; emit changed(); }

    QString loadout() const { return m_loadout; }
    void setLoadout(const QString &l) { m_loadout = l; emit changed(); }

    QString comment() const { return m_comment; }
    void setComment(const QString &c) { m_comment = c; emit changed(); }

signals:
    void changed();

private:
    QString m_nickname;
    QString m_archetype;
    QString m_base;
    QString m_dockWith;
    QString m_gotoTarget;
    QString m_loadout;
    QString m_comment;
    QVector3D m_position;
    QVector3D m_rotation;
    int m_idsName = 0;
    int m_idsInfo = 0;
    Type m_type = Other;
};

} // namespace flatlas::domain
