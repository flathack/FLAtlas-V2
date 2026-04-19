#pragma once
// domain/SolarObject.h – Freelancer-Objekt (Planet, Station, etc.)
// TODO Phase 2

#include <QObject>
#include <QString>
#include <QVector3D>

namespace flatlas::domain {

class SolarObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString nickname READ nickname WRITE setNickname NOTIFY changed)
    Q_PROPERTY(QVector3D position READ position WRITE setPosition NOTIFY changed)

public:
    explicit SolarObject(QObject *parent = nullptr) : QObject(parent) {}

    QString nickname() const { return m_nickname; }
    void setNickname(const QString &n) { m_nickname = n; emit changed(); }

    QVector3D position() const { return m_position; }
    void setPosition(const QVector3D &p) { m_position = p; emit changed(); }

    QString archetype() const { return m_archetype; }
    void setArchetype(const QString &a) { m_archetype = a; }

    int idsName() const { return m_idsName; }
    void setIdsName(int id) { m_idsName = id; }

    int idsInfo() const { return m_idsInfo; }
    void setIdsInfo(int id) { m_idsInfo = id; }

    QVector3D rotation() const { return m_rotation; }
    void setRotation(const QVector3D &r) { m_rotation = r; }

signals:
    void changed();

private:
    QString m_nickname;
    QString m_archetype;
    QVector3D m_position;
    QVector3D m_rotation;
    int m_idsName = 0;
    int m_idsInfo = 0;
};

} // namespace flatlas::domain
