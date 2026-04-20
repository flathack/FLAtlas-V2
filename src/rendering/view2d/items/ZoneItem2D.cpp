#include "ZoneItem2D.h"
#include <QPen>
#include <QBrush>
#include <QPainter>

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
        fill = QColor(60, 100, 180, 40);
        break;
    case S::Box:
    case S::Cylinder:
        fill = QColor(60, 180, 100, 40);
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

    setPos(zone.position().x() * kScale, zone.position().z() * kScale);
    setRotation(0.0);
    m_drawAsRect = false;

    const qreal sx = zone.size().x() * kScale;
    const qreal sy = zone.size().y() * kScale;
    const qreal sz = zone.size().z() * kScale;
    qreal w = sx * 2.0;
    qreal h = sz * 2.0;

    switch (zone.shape()) {
    case flatlas::domain::ZoneItem::Sphere:
        w = sx * 2.0;
        h = sx * 2.0;
        break;
    case flatlas::domain::ZoneItem::Ellipsoid:
    case flatlas::domain::ZoneItem::Ring:
        w = sx * 2.0;
        h = sz * 2.0;
        break;
    case flatlas::domain::ZoneItem::Box:
        w = sx;
        h = sz;
        m_drawAsRect = true;
        setRotation(-zone.rotation().y());
        break;
    case flatlas::domain::ZoneItem::Cylinder:
        w = sx * 2.0;
        h = qMax(w, sy);
        m_drawAsRect = true;
        setRotation(-zone.rotation().y());
        break;
    }

    setRect(-w / 2.0, -h / 2.0, w, h);
}

void ZoneItem2D::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    painter->setPen(pen());
    painter->setBrush(brush());
    if (m_drawAsRect)
        painter->drawRect(rect());
    else
        painter->drawEllipse(rect());
}

} // namespace flatlas::rendering
