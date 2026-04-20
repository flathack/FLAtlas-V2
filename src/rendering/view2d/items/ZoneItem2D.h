#pragma once
#include <QGraphicsEllipseItem>
#include <QQuaternion>
#include <QString>
#include "domain/ZoneItem.h"

namespace flatlas::rendering {

class ZoneItem2D : public QGraphicsEllipseItem {
public:
    enum { Type = UserType + 2 };
    int type() const override { return Type; }

    explicit ZoneItem2D(const QString &nickname,
                        flatlas::domain::ZoneItem::Shape shape,
                        QGraphicsItem *parent = nullptr);

    QString nickname() const { return m_nickname; }
    void updateFromZone(const flatlas::domain::ZoneItem &zone);
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

private:
    static QQuaternion rotationQuaternionFromFl(float rx, float ry, float rz);
    static bool usesLegacyCylinderYaw(const QString &nickname);
    static qreal boxScreenRotation(const flatlas::domain::ZoneItem &zone, qreal sx, qreal sz);

    QString m_nickname;
    flatlas::domain::ZoneItem::Shape m_shape;
    bool m_drawAsRect = false;
    bool m_drawCylinderAsEllipse = false;
};

} // namespace flatlas::rendering
