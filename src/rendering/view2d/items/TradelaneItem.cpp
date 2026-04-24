#include "TradelaneItem.h"
#include <QPen>

namespace flatlas::rendering {

namespace {

QPen tradeLanePen(bool highlighted)
{
    if (highlighted) {
        QPen pen(QColor(0xFF, 0x44, 0x44));
        pen.setWidthF(2.5);
        pen.setStyle(Qt::SolidLine);
        return pen;
    }

    QPen pen(QColor(0x00, 0xAA, 0xAA));
    pen.setWidthF(1.0);
    pen.setStyle(Qt::DashLine);
    return pen;
}

}

TradelaneItem::TradelaneItem(const QString &nickname,
                              const QPointF &start, const QPointF &end,
                              QGraphicsItem *parent)
    : QGraphicsLineItem(parent)
    , m_nickname(nickname)
{
    setLine(QLineF(start, end));

    setPen(tradeLanePen(false));
    setAcceptedMouseButtons(Qt::NoButton);
    setFlag(ItemIsSelectable, false);
    setZValue(50);
    setToolTip(nickname);
}

void TradelaneItem::setHighlighted(bool highlighted)
{
    setPen(tradeLanePen(highlighted));
}

} // namespace flatlas::rendering
