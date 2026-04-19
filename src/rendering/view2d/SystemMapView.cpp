#include "SystemMapView.h"
#include <QGraphicsScene>
namespace flatlas::rendering {
SystemMapView::SystemMapView(QWidget *parent) : QGraphicsView(parent)
{
    setScene(new QGraphicsScene(this));
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);
    // TODO Phase 4: Zoom, Selektion, Custom Items
}
} // namespace flatlas::rendering
