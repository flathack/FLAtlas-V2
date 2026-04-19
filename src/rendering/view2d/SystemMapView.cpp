#include "SystemMapView.h"
#include "MapScene.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
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

void SystemMapView::zoomToFit()
{
    if (!scene() || scene()->items().isEmpty())
        return;
    fitInView(scene()->itemsBoundingRect().adjusted(-20, -20, 20, 20),
              Qt::KeepAspectRatio);
}

void SystemMapView::wheelEvent(QWheelEvent *event)
{
    constexpr double factor = 1.15;
    const double currentScale = transform().m11();

    if (event->angleDelta().y() > 0) {
        if (currentScale < 100.0)
            scale(factor, factor);
    } else {
        if (currentScale > 0.01)
            scale(1.0 / factor, 1.0 / factor);
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

} // namespace flatlas::rendering
