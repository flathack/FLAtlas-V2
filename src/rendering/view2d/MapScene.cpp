#include "MapScene.h"
#include "items/SolarObjectItem.h"
#include "items/ZoneItem2D.h"
#include "items/TradelaneItem.h"
#include "domain/SystemDocument.h"
#include "domain/SolarObject.h"
#include "domain/ZoneItem.h"
#include <QPainter>
#include <QPen>
#include <cmath>

namespace {

constexpr double kFreelancerNavCellWorld = 30000.0;
constexpr int kFreelancerNavCellsPerAxis = 8;

double referenceHalfExtentWorld(const flatlas::domain::SystemDocument *doc)
{
    const double navMapScale = (doc && doc->navMapScale() > 0.0) ? doc->navMapScale() : 1.0;
    return (kFreelancerNavCellWorld * (kFreelancerNavCellsPerAxis / 2.0)) / navMapScale;
}

}

namespace flatlas::rendering {

MapScene::MapScene(QObject *parent)
    : QGraphicsScene(parent)
{
    // Default scene rect covering a typical Freelancer system
    // ±33000 FL units → ±330 scene units
    const double halfExtentScene = referenceHalfExtentWorld(nullptr) * kScale;
    setSceneRect(-halfExtentScene, -halfExtentScene, halfExtentScene * 2.0, halfExtentScene * 2.0);
}

void MapScene::loadDocument(flatlas::domain::SystemDocument *doc)
{
    clear();
    m_document = doc;
    if (!doc)
        return;

    const double halfExtentScene = referenceHalfExtentWorld(doc) * kScale;
    setSceneRect(-halfExtentScene, -halfExtentScene, halfExtentScene * 2.0, halfExtentScene * 2.0);

    for (const auto &obj : doc->objects())
        addSolarObject(obj);

    for (const auto &zone : doc->zones())
        addZone(zone);

    // React to future additions/removals
    connect(doc, &flatlas::domain::SystemDocument::objectAdded,
            this, &MapScene::addSolarObject);
    connect(doc, &flatlas::domain::SystemDocument::objectRemoved,
            this, [this](const std::shared_ptr<flatlas::domain::SolarObject> &obj) {
        const auto items = this->items();
        for (auto *item : items) {
            if (auto *soi = dynamic_cast<SolarObjectItem *>(item)) {
                if (soi->nickname() == obj->nickname()) {
                    removeItem(soi);
                    delete soi;
                    break;
                }
            }
        }
    });

    connect(doc, &flatlas::domain::SystemDocument::zoneAdded,
            this, &MapScene::addZone);
    connect(doc, &flatlas::domain::SystemDocument::zoneRemoved,
            this, [this](const std::shared_ptr<flatlas::domain::ZoneItem> &zone) {
        const auto items = this->items();
        for (auto *item : items) {
            if (auto *zi = dynamic_cast<ZoneItem2D *>(item)) {
                if (zi->nickname() == zone->nickname()) {
                    removeItem(zi);
                    delete zi;
                    break;
                }
            }
        }
    });

    // Forward selection changes
    connect(this, &QGraphicsScene::selectionChanged, this, [this]() {
        const auto sel = selectedItems();
        if (sel.isEmpty()) {
            emit selectionCleared();
            return;
        }
        if (auto *soi = dynamic_cast<SolarObjectItem *>(sel.first()))
            emit objectSelected(soi->nickname());
        else if (auto *zi = dynamic_cast<ZoneItem2D *>(sel.first()))
            emit objectSelected(zi->nickname());
    });
}

void MapScene::clear()
{
    if (m_document) {
        disconnect(m_document, nullptr, this, nullptr);
        m_document = nullptr;
    }
    QGraphicsScene::clear();
}

void MapScene::setGridVisible(bool visible)
{
    if (m_gridVisible != visible) {
        m_gridVisible = visible;
        invalidate(sceneRect(), QGraphicsScene::BackgroundLayer);
    }
}

bool MapScene::isGridVisible() const
{
    return m_gridVisible;
}

QPointF MapScene::flToQt(float x, float z)
{
    return QPointF(x * kScale, z * kScale);
}

QPointF MapScene::qtToFl(qreal x, qreal y)
{
    return QPointF(x / kScale, y / kScale);
}

void MapScene::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsScene::drawBackground(painter, rect);

    if (!m_gridVisible)
        return;

    const double gridHalfExtent = referenceHalfExtentWorld(m_document) * kScale;
    const QRectF gridRect(-gridHalfExtent, -gridHalfExtent, gridHalfExtent * 2.0, gridHalfExtent * 2.0);
    const QRectF visibleGrid = rect.intersected(gridRect);
    if (visibleGrid.isNull() || visibleGrid.width() <= 0.0 || visibleGrid.height() <= 0.0)
        return;

    const double gridSpacing = gridRect.width() / static_cast<double>(kFreelancerNavCellsPerAxis);
    const double left = std::floor((visibleGrid.left() - gridRect.left()) / gridSpacing) * gridSpacing + gridRect.left();
    const double top  = std::floor((visibleGrid.top() - gridRect.top()) / gridSpacing) * gridSpacing + gridRect.top();

    QPen gridPen(QColor(60, 60, 60), 0);
    painter->setPen(gridPen);
    painter->setClipRect(visibleGrid);

    // Vertical lines
    for (double x = left; x <= visibleGrid.right(); x += gridSpacing)
        painter->drawLine(QPointF(x, visibleGrid.top()), QPointF(x, visibleGrid.bottom()));

    // Horizontal lines
    for (double y = top; y <= visibleGrid.bottom(); y += gridSpacing)
        painter->drawLine(QPointF(visibleGrid.left(), y), QPointF(visibleGrid.right(), y));

    painter->setClipping(false);

    // Origin cross (brighter)
    QPen originPen(QColor(100, 100, 100), 0);
    painter->setPen(originPen);
    painter->drawLine(QPointF(visibleGrid.left(), 0), QPointF(visibleGrid.right(), 0));
    painter->drawLine(QPointF(0, visibleGrid.top()), QPointF(0, visibleGrid.bottom()));
}

void MapScene::addSolarObject(const std::shared_ptr<flatlas::domain::SolarObject> &obj)
{
    auto *item = new SolarObjectItem(obj->nickname(), obj->type());
    item->updateFromObject(*obj);

    const QPointF pos = flToQt(obj->position().x(), obj->position().z());
    item->setPos(pos);

    item->setFlag(QGraphicsItem::ItemIsSelectable, true);
    item->setFlag(QGraphicsItem::ItemIsMovable, true);

    addItem(item);

    // Update item when domain object changes
    connect(obj.get(), &flatlas::domain::SolarObject::changed, this,
            [item, weak = std::weak_ptr(obj)]() {
        if (auto obj = weak.lock()) {
            item->updateFromObject(*obj);
            item->setPos(flToQt(obj->position().x(), obj->position().z()));
        }
    });
}

void MapScene::addZone(const std::shared_ptr<flatlas::domain::ZoneItem> &zone)
{
    auto *item = new ZoneItem2D(zone->nickname(), zone->shape());
    item->updateFromZone(*zone);

    const QPointF pos = flToQt(zone->position().x(), zone->position().z());
    item->setPos(pos);

    item->setFlag(QGraphicsItem::ItemIsSelectable, true);

    addItem(item);

    // Update item when domain zone changes
    connect(zone.get(), &flatlas::domain::ZoneItem::changed, this,
            [item, weak = std::weak_ptr(zone)]() {
        if (auto zone = weak.lock()) {
            item->updateFromZone(*zone);
            item->setPos(flToQt(zone->position().x(), zone->position().z()));
        }
    });
}

} // namespace flatlas::rendering
