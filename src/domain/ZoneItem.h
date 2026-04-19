#pragma once
// domain/ZoneItem.h – Zone im System
// TODO Phase 2

#include <QObject>
#include <QString>
#include <QVector3D>

namespace flatlas::domain {

class ZoneItem : public QObject
{
    Q_OBJECT
public:
    enum Shape { Sphere, Ellipsoid, Cylinder, Box, Ring };
    Q_ENUM(Shape)

    explicit ZoneItem(QObject *parent = nullptr) : QObject(parent) {}

    QString nickname() const { return m_nickname; }
    void setNickname(const QString &n) { m_nickname = n; }

    QVector3D position() const { return m_position; }
    void setPosition(const QVector3D &p) { m_position = p; }

    QVector3D size() const { return m_size; }
    void setSize(const QVector3D &s) { m_size = s; }

    Shape shape() const { return m_shape; }
    void setShape(Shape s) { m_shape = s; }

private:
    QString m_nickname;
    QVector3D m_position;
    QVector3D m_size;
    Shape m_shape = Sphere;
};

} // namespace flatlas::domain
