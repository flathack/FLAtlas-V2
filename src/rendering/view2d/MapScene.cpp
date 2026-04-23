#include "MapScene.h"
#include "items/LightSourceItem.h"
#include "items/SolarObjectItem.h"
#include "items/ZoneItem2D.h"
#include "items/TradelaneItem.h"
#include "domain/SystemDocument.h"
#include "domain/SolarObject.h"
#include "domain/ZoneItem.h"
#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QPalette>
#include <QPainter>
#include <QPen>
#include <QSet>
#include <QSignalBlocker>
#include <cmath>

namespace {

constexpr double kFreelancerNavCellWorld = 30000.0;   // NavMap cell size at NavMapScale = 1.36
constexpr int    kFreelancerNavCellsPerAxis = 8;
constexpr double kFreelancerReferenceNavMapScale = 1.36; // vanilla default
constexpr int kLoadProgressYieldInterval = 12;

// Grid half-extent in FL world units for a given NavMapScale.
//
// The 8x8 NavMap grid is an in-game navigation aid whose cell size depends
// on NavMapScale. At the vanilla reference value 1.36 a cell is 30'000 FL
// units, so the half-extent is 4 * 30'000 = 120'000.
// For any other NavMapScale the cell scales inversely:
//     cell       = 30'000 * (1.36 / navMapScale)
//     halfExtent = 4 * cell = 163'200 / navMapScale
// A NavMapScale of 2.0 therefore shrinks the grid to a cell size of 20'400
// and a half-extent of 81'600 FL units, which is what Freelancer itself
// draws on its in-game map.
double halfExtentWorldForScale(double navMapScale)
{
    const double scale = navMapScale > 0.0 ? navMapScale : kFreelancerReferenceNavMapScale;
    const double referenceHalfExtent = kFreelancerNavCellWorld * (kFreelancerNavCellsPerAxis / 2.0);
    return referenceHalfExtent * (kFreelancerReferenceNavMapScale / scale);
}

double referenceHalfExtentWorld(const flatlas::domain::SystemDocument *doc)
{
    return halfExtentWorldForScale(doc ? doc->navMapScale() : 0.0);
}

QString itemNickname(QGraphicsItem *item)
{
    if (auto *soi = dynamic_cast<flatlas::rendering::SolarObjectItem *>(item))
        return soi->nickname();
    if (auto *zi = dynamic_cast<flatlas::rendering::ZoneItem2D *>(item))
        return zi->nickname();
    if (auto *li = dynamic_cast<flatlas::rendering::LightSourceItem *>(item))
        return li->nickname();
    return {};
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

    // Wire selection forwarding exactly once for the lifetime of the scene.
    // Previously this was re-connected inside loadDocument(), which caused
    // duplicate emissions of selectionNicknamesChanged when a second system
    // was opened and amplified selection-sync recursion in the editor page.
    connect(this, &QGraphicsScene::selectionChanged, this, [this]() {
        const auto sel = selectedItems();
        if (sel.isEmpty()) {
            emit selectionCleared();
            emit selectionNicknamesChanged({});
            return;
        }
        QStringList nicknames;
        nicknames.reserve(sel.size());
        for (QGraphicsItem *item : sel) {
            const QString nickname = itemNickname(item);
            if (!nickname.isEmpty())
                nicknames.append(nickname);
        }
        nicknames.removeDuplicates();
        emit selectionNicknamesChanged(nicknames);
        if (!nicknames.isEmpty())
            emit objectSelected(nicknames.first());
    });
}

void MapScene::loadDocument(flatlas::domain::SystemDocument *doc,
                            const std::function<void(int current, int total)> &progressCallback)
{
    clear();
    m_document = doc;
    if (!doc)
        return;

    const double halfExtentScene = referenceHalfExtentWorld(doc) * kScale;
    setSceneRect(-halfExtentScene, -halfExtentScene, halfExtentScene * 2.0, halfExtentScene * 2.0);

    const int totalItems = doc->objects().size() + doc->zones().size();
    int processedItems = 0;
    const auto reportProgress = [&]() {
        if (progressCallback)
            progressCallback(processedItems, totalItems);
        if (processedItems > 0 && (processedItems % kLoadProgressYieldInterval) == 0)
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    };

    for (const auto &obj : doc->objects()) {
        addSolarObject(obj);
        ++processedItems;
        reportProgress();
    }

    for (const auto &zone : doc->zones()) {
        addZone(zone);
        ++processedItems;
        reportProgress();
    }

    if (progressCallback)
        progressCallback(totalItems, totalItems);

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
    // Note: selection forwarding is wired once in the constructor.
}

void MapScene::clear()
{
    if (m_document) {
        disconnect(m_document, nullptr, this, nullptr);
        m_document = nullptr;
    }
    QGraphicsScene::clear();
}

void MapScene::setLightSources(const QVector<LightSourceVisual> &lightSources)
{
    const auto sceneItems = items();
    for (QGraphicsItem *item : sceneItems) {
        if (auto *lightItem = dynamic_cast<LightSourceItem *>(item)) {
            removeItem(lightItem);
            delete lightItem;
        }
    }

    for (const LightSourceVisual &lightSource : lightSources)
        addLightSource(lightSource);
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

void MapScene::setMoveEnabled(bool enabled)
{
    if (m_moveEnabled == enabled)
        return;

    m_moveEnabled = enabled;
    const auto sceneItems = items();
    for (QGraphicsItem *item : sceneItems) {
        if (dynamic_cast<SolarObjectItem *>(item) || dynamic_cast<ZoneItem2D *>(item))
            item->setFlag(QGraphicsItem::ItemIsMovable, m_moveEnabled);
    }
}

bool MapScene::isMoveEnabled() const
{
    return m_moveEnabled;
}

QStringList MapScene::selectedNicknames() const
{
    QStringList nicknames;
    const auto sel = selectedItems();
    nicknames.reserve(sel.size());
    for (QGraphicsItem *item : sel) {
        const QString nickname = itemNickname(item);
        if (!nickname.isEmpty())
            nicknames.append(nickname);
    }
    nicknames.removeDuplicates();
    return nicknames;
}

void MapScene::selectNicknames(const QStringList &nicknames)
{
    const QSet<QString> selectedSet(nicknames.begin(), nicknames.end());
    QSignalBlocker blocker(this);
    clearSelection();
    for (QGraphicsItem *item : items()) {
        const QString nickname = itemNickname(item);
        if (!nickname.isEmpty() && selectedSet.contains(nickname))
            item->setSelected(true);
    }
    blocker.unblock();

    if (selectedSet.isEmpty()) {
        emit selectionCleared();
        emit selectionNicknamesChanged({});
        return;
    }

    const QStringList currentSelection = selectedNicknames();
    emit selectionNicknamesChanged(currentSelection);
    if (!currentSelection.isEmpty())
        emit objectSelected(currentSelection.first());
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

    const QPalette pal = qApp->palette();
    const QColor textColor = pal.color(QPalette::Text);
    QColor gridColor = textColor;
    gridColor.setAlpha(pal.color(QPalette::Base).lightness() >= 170 ? 55 : 70);
    QColor originColor = textColor;
    originColor.setAlpha(pal.color(QPalette::Base).lightness() >= 170 ? 95 : 110);

    QPen gridPen(gridColor, 0);
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
    QPen originPen(originColor, 0);
    painter->setPen(originPen);
    painter->drawLine(QPointF(visibleGrid.left(), 0), QPointF(visibleGrid.right(), 0));
    painter->drawLine(QPointF(0, visibleGrid.top()), QPointF(0, visibleGrid.bottom()));
}

void MapScene::addLightSource(const LightSourceVisual &lightSource)
{
    if (lightSource.nickname.trimmed().isEmpty())
        return;

    auto *item = new LightSourceItem(lightSource.nickname);
    item->setPos(flToQt(lightSource.position.x(), lightSource.position.z()));
    item->setData(0, lightSource.nickname);
    addItem(item);
}

void MapScene::addSolarObject(const std::shared_ptr<flatlas::domain::SolarObject> &obj)
{
    auto *item = new SolarObjectItem(obj->nickname(), obj->type());
    item->updateFromObject(*obj);

    const QPointF pos = flToQt(obj->position().x(), obj->position().z());
    item->setPos(pos);

    item->setFlag(QGraphicsItem::ItemIsSelectable, true);
    item->setFlag(QGraphicsItem::ItemIsMovable, m_moveEnabled);

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
    item->setFlag(QGraphicsItem::ItemIsMovable, m_moveEnabled);

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
