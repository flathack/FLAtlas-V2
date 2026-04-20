#include "SystemMapView.h"
#include "MapScene.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QScrollBar>

namespace flatlas::rendering {

SystemMapView::SystemMapView(QWidget *parent)
    : QGraphicsView(parent)
{
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::RubberBandDrag);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

MapScene *SystemMapView::mapScene() const
{
    return m_mapScene;
}

void SystemMapView::setMapScene(MapScene *scene)
{
    m_mapScene = scene;
    QGraphicsView::setScene(scene);
}

void SystemMapView::setBackgroundPixmap(const QPixmap &pixmap, const QColor &fallbackColor)
{
    m_backgroundPixmap = pixmap;
    m_backgroundColor = fallbackColor;
    m_backgroundDarkenAlpha = m_backgroundColor.lightness() >= 130 ? 0 : 180;
    viewport()->update();
}

void SystemMapView::setSystemName(const QString &name)
{
    m_systemName = name;
    viewport()->update();
}

void SystemMapView::zoomToFit()
{
    if (!scene())
        return;
    QRectF targetRect = scene()->sceneRect();
    if (targetRect.isNull() || targetRect.width() <= 0.0 || targetRect.height() <= 0.0)
        targetRect = scene()->itemsBoundingRect();
    if (targetRect.isNull() || targetRect.width() <= 0.0 || targetRect.height() <= 0.0)
        return;

    resetTransform();
    fitInView(targetRect, Qt::KeepAspectRatio);
    m_minZoomScale = qMax(0.0001, transform().m11());
}

void SystemMapView::wheelEvent(QWheelEvent *event)
{
    constexpr double factor = 1.15;
    const double currentScale = transform().m11();

    if (event->angleDelta().y() > 0) {
        if (currentScale < 100.0)
            scale(factor, factor);
    } else {
        if (currentScale > m_minZoomScale)
            scale(1.0 / factor, 1.0 / factor);
        if (transform().m11() < m_minZoomScale) {
            resetTransform();
            scale(m_minZoomScale, m_minZoomScale);
        }
    }
    event->accept();
}

void SystemMapView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        setDragMode(QGraphicsView::ScrollHandDrag);
        // Send a synthetic left-button press so ScrollHandDrag activates
        QMouseEvent fake(event->type(), event->position(), event->globalPosition(),
                         Qt::LeftButton, Qt::LeftButton, event->modifiers());
        QGraphicsView::mousePressEvent(&fake);
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void SystemMapView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_panning) {
        m_panning = false;
        setDragMode(QGraphicsView::RubberBandDrag);
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void SystemMapView::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    menu.addAction(tr("Add Object..."));
    menu.addSeparator();
    menu.addAction(tr("Delete"));
    menu.addAction(tr("Properties..."));
    menu.exec(event->globalPos());
}

void SystemMapView::drawBackground(QPainter *painter, const QRectF &rect)
{
    painter->save();
    painter->resetTransform();

    const QRect viewportRect = viewport()->rect();
    painter->fillRect(viewportRect, m_backgroundColor);
    if (!m_backgroundPixmap.isNull()) {
        painter->drawTiledPixmap(viewportRect, m_backgroundPixmap);
        if (m_backgroundDarkenAlpha > 0)
            painter->fillRect(viewportRect, QColor(0, 0, 0, m_backgroundDarkenAlpha));
    }

    painter->restore();
    QGraphicsView::drawBackground(painter, rect);
}

void SystemMapView::drawForeground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawForeground(painter, rect);

    if (!scene())
        return;

    const QRectF sceneRect = scene()->sceneRect();
    if (sceneRect.isNull() || sceneRect.width() <= 0.0 || sceneRect.height() <= 0.0)
        return;

    painter->save();
    painter->resetTransform();

    QFont font(QStringLiteral("Segoe UI"), 11, QFont::Bold);
    painter->setFont(font);
    painter->setPen(QColor(230, 235, 245, 235));
    const QFontMetrics fm(font);

    const double cellWidth = sceneRect.width() / 8.0;
    const double cellHeight = sceneRect.height() / 8.0;
    const int bottomMargin = 22;
    const int leftMargin = 20;

    for (int i = 0; i < 8; ++i) {
        const double sceneX = sceneRect.left() + (cellWidth * (static_cast<double>(i) + 0.5));
        const QPoint viewPoint = mapFromScene(QPointF(sceneX, sceneRect.bottom()));
        const QString label(QChar(static_cast<char>('A' + i)));
        painter->drawText(viewPoint.x() - (fm.horizontalAdvance(label) / 2),
                          viewport()->height() - bottomMargin,
                          label);
    }

    for (int i = 0; i < 8; ++i) {
        const double sceneY = sceneRect.top() + (cellHeight * (static_cast<double>(i) + 0.5));
        const QPoint viewPoint = mapFromScene(QPointF(sceneRect.left(), sceneY));
        const QString label = QString::number(i + 1);
        painter->drawText(leftMargin - fm.horizontalAdvance(label),
                          viewPoint.y() + (fm.height() / 3),
                          label);
    }

    if (!m_systemName.trimmed().isEmpty()) {
        QFont titleFont(QStringLiteral("Segoe UI"), 16, QFont::Bold);
        painter->setFont(titleFont);
        const QFontMetrics titleMetrics(titleFont);
        const QString title = m_systemName.trimmed().toUpper();
        painter->drawText((viewport()->width() - titleMetrics.horizontalAdvance(title)) / 2,
                          viewport()->height() - 54,
                          title);
        painter->setFont(font);
    }

    constexpr double kSceneScale = 0.01;
    const double sx = std::abs(transform().m11());
    const double sy = std::abs(transform().m22());
    if (sx > 1e-9 && sy > 1e-9) {
        const int margin = 16;
        const int barWidthPx = 140;
        const int barHeightPx = 100;
        const double worldW = barWidthPx / (sx * kSceneScale);
        const double worldH = barHeightPx / (sy * kSceneScale);

        const int x0 = margin + 16;
        const int y0 = viewport()->height() - 16;
        painter->drawLine(x0, y0, x0 + barWidthPx, y0);
        painter->drawLine(x0, y0 - 4, x0, y0 + 4);
        painter->drawLine(x0 + barWidthPx, y0 - 4, x0 + barWidthPx, y0 + 4);
        painter->drawText(x0, y0 - 8,
                          QStringLiteral("%1 km").arg(worldW / 1000.0, 0, 'f', 2));

        const int vx = margin;
        const int vy0 = margin;
        painter->drawLine(vx, vy0, vx, vy0 + barHeightPx);
        painter->drawLine(vx - 4, vy0, vx + 4, vy0);
        painter->drawLine(vx - 4, vy0 + barHeightPx, vx + 4, vy0 + barHeightPx);
        painter->drawText(vx + 8, vy0 + barHeightPx,
                          QStringLiteral("%1 km").arg(worldH / 1000.0, 0, 'f', 2));
    }

    painter->restore();
}

} // namespace flatlas::rendering
