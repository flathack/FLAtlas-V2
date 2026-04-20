#pragma once
#include <QGraphicsEllipseItem>
#include <QQuaternion>
#include <QString>
#include "domain/ZoneItem.h"
#include "rendering/view2d/SystemDisplayFilter.h"

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
    void applyDisplayFilter(const SystemDisplayFilterSettings &settings);
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

private:
    static QQuaternion rotationQuaternionFromFl(float rx, float ry, float rz);
    static bool usesLegacyCylinderYaw(const QString &nickname);
    static qreal boxScreenRotation(const flatlas::domain::ZoneItem &zone, qreal sx, qreal sz);
    static QPen penForZone(const flatlas::domain::ZoneItem &zone);
    static QBrush brushForZone(const flatlas::domain::ZoneItem &zone);

    QString m_nickname;
    flatlas::domain::ZoneItem::Shape m_shape;
    bool m_drawAsRect = false;
    bool m_drawCylinderAsEllipse = false;
    bool m_visibleByFilter = true;
};

} // namespace flatlas::rendering
