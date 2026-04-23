#include "LightSourceItem.h"

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QPen>

namespace flatlas::rendering {

namespace {

constexpr qreal kCrossHalfExtent = 7.0;
constexpr qreal kCrossStrokeWidth = 2.2;
constexpr qreal kGlowPadding = 6.0;

QColor symbolColor()
{
    const QColor base = QApplication::palette().color(QPalette::Base);
    return base.lightness() >= 170 ? QColor(18, 18, 18) : QColor(245, 247, 250);
}

}

LightSourceItem::LightSourceItem(const QString &nickname, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_nickname(nickname.trimmed())
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
    setToolTip(m_nickname);
    setZValue(26.0);
}

QRectF LightSourceItem::boundingRect() const
{
    const qreal diameter = (kCrossHalfExtent * 2.0) + (kGlowPadding * 2.0);
    return QRectF(-diameter * 0.5, -diameter * 0.5, diameter, diameter);
}

void LightSourceItem::paint(QPainter *painter,
                            const QStyleOptionGraphicsItem *option,
                            QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing, true);

    const QColor crossColor = symbolColor();
    if (isSelected()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(87, 205, 255, 78));
        painter->drawEllipse(QRectF(-11.0, -11.0, 22.0, 22.0));

        QPen selectionPen(QColor(145, 225, 255, 235));
        selectionPen.setWidthF(2.0);
        painter->setPen(selectionPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QRectF(-10.0, -10.0, 20.0, 20.0));
    }

    QPen crossPen(crossColor);
    crossPen.setWidthF(kCrossStrokeWidth);
    crossPen.setCapStyle(Qt::RoundCap);
    painter->setPen(crossPen);
    painter->drawLine(QPointF(-kCrossHalfExtent, 0.0), QPointF(kCrossHalfExtent, 0.0));
    painter->drawLine(QPointF(0.0, -kCrossHalfExtent), QPointF(0.0, kCrossHalfExtent));
}

} // namespace flatlas::rendering
