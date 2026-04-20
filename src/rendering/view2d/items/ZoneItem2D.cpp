#include "ZoneItem2D.h"
#include <QPen>
#include <QBrush>

namespace flatlas::rendering {

ZoneItem2D::ZoneItem2D(const QString &nickname,
                        flatlas::domain::ZoneItem::Shape shape,
                        QGraphicsItem *parent)
    : QGraphicsEllipseItem(parent)
    , m_nickname(nickname)
    , m_shape(shape)
{
    using S = flatlas::domain::ZoneItem::Shape;

    QColor fill;
    switch (shape) {
    case S::Sphere:
    case S::Ellipsoid:
    case S::Ring:
        fill = QColor(60, 100, 180, 40);  // blue-ish
        break;
    case S::Box:
    case S::Cylinder:
        fill = QColor(60, 180, 100, 40);  // green-ish
        break;
    default:
        fill = QColor(120, 120, 120, 40);
        break;
    }

    setBrush(QBrush(fill));

    QPen pen(fill.lighter(160));
    pen.setStyle(Qt::DotLine);
    pen.setWidthF(0.5);
    setPen(pen);

    setFlag(ItemIsSelectable);
    setZValue(-1);
    setToolTip(nickname);
}

void ZoneItem2D::updateFromZone(const flatlas::domain::ZoneItem &zone)
{
    constexpr double kScale = 0.01;

    setPos(zone.position().x() * kScale, -zone.position().z() * kScale);

    const qreal w = zone.size().x() * kScale;
    const qreal h = zone.size().z() * kScale;   // z → map-plane height
    setRect(-w / 2.0, -h / 2.0, w, h);
}

} // namespace flatlas::rendering
