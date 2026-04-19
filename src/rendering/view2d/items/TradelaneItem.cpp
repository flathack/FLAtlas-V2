#include "TradelaneItem.h"
#include <QPen>

namespace flatlas::rendering {

TradelaneItem::TradelaneItem(const QString &nickname,
                              const QPointF &start, const QPointF &end,
                              QGraphicsItem *parent)
    : QGraphicsLineItem(parent)
    , m_nickname(nickname)
{
    setLine(QLineF(start, end));

    QPen pen(QColor(0x00, 0xAA, 0xAA)); // #00AAAA
    pen.setWidthF(1.0);
    pen.setStyle(Qt::DashLine);
    setPen(pen);

    setFlag(ItemIsSelectable);
    setZValue(-2);
    setToolTip(nickname);
}

} // namespace flatlas::rendering
