#pragma once
// domain/ZoneItem.h – Zone im System

#include <QObject>
#include <QString>
#include <QVector3D>

namespace flatlas::domain {

class ZoneItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString nickname READ nickname WRITE setNickname NOTIFY changed)
    Q_PROPERTY(QVector3D position READ position WRITE setPosition NOTIFY changed)
    Q_PROPERTY(Shape shape READ shape WRITE setShape NOTIFY changed)
    Q_PROPERTY(QString zoneType READ zoneType WRITE setZoneType NOTIFY changed)

public:
    enum Shape { Sphere, Ellipsoid, Cylinder, Box, Ring };
    Q_ENUM(Shape)

    explicit ZoneItem(QObject *parent = nullptr) : QObject(parent) {}

    QString nickname() const { return m_nickname; }
    void setNickname(const QString &n) { m_nickname = n; emit changed(); }

    QVector3D position() const { return m_position; }
    void setPosition(const QVector3D &p) { m_position = p; emit changed(); }

    QVector3D size() const { return m_size; }
    void setSize(const QVector3D &s) { m_size = s; emit changed(); }

    Shape shape() const { return m_shape; }
    void setShape(Shape s) { m_shape = s; emit changed(); }

    QVector3D rotation() const { return m_rotation; }
    void setRotation(const QVector3D &r) { m_rotation = r; emit changed(); }

    QString zoneType() const { return m_zoneType; }
    void setZoneType(const QString &t) { m_zoneType = t; emit changed(); }

    QString usage() const { return m_usage; }
    void setUsage(const QString &u) { m_usage = u; emit changed(); }

    QString popType() const { return m_popType; }
    void setPopType(const QString &p) { m_popType = p; emit changed(); }

    QString pathLabel() const { return m_pathLabel; }
    void setPathLabel(const QString &p) { m_pathLabel = p; emit changed(); }

    QString comment() const { return m_comment; }
    void setComment(const QString &c) { m_comment = c; emit changed(); }

    QVector3D tightnessXyz() const { return m_tightnessXyz; }
    void setTightnessXyz(const QVector3D &t) { m_tightnessXyz = t; emit changed(); }

    int damage() const { return m_damage; }
    void setDamage(int d) { m_damage = d; emit changed(); }

    float interference() const { return m_interference; }
    void setInterference(float i) { m_interference = i; emit changed(); }

    float dragScale() const { return m_dragScale; }
    void setDragScale(float d) { m_dragScale = d; emit changed(); }

    int sortKey() const { return m_sortKey; }
    void setSortKey(int k) { m_sortKey = k; emit changed(); }

signals:
    void changed();

private:
    QString m_nickname;
    QString m_zoneType;
    QString m_usage;
    QString m_popType;
    QString m_pathLabel;
    QString m_comment;
    QVector3D m_position;
    QVector3D m_size;
    QVector3D m_rotation;
    QVector3D m_tightnessXyz;
    Shape m_shape = Sphere;
    int m_damage = 0;
    float m_interference = 0.0f;
    float m_dragScale = 0.0f;
    int m_sortKey = 0;
};

} // namespace flatlas::domain
