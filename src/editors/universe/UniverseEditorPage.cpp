// editors/universe/UniverseEditorPage.cpp – Universe-Editor (Phase 9)

#include "UniverseEditorPage.h"
#include "UniverseSerializer.h"
#include "core/PathUtils.h"
#include "domain/UniverseData.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QToolBar>
#include <QTreeWidget>
#include <QHeaderView>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsSimpleTextItem>
#include <QMessageBox>
#include <QInputDialog>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include "tools/PathFinderDialog.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <functional>
#include <QGraphicsSceneMouseEvent>
#include <QSignalBlocker>
#include <QRectF>
#include <QPainter>
#include <QPixmap>
#include <QWidget>
#include <QTabBar>
#include <QPen>
#include <QBrush>

namespace flatlas::editors {

using namespace flatlas::domain;

// ─── Custom graphics view with zoom ──────────────────────

class UniverseMapView : public QGraphicsView {
public:
    explicit UniverseMapView(QGraphicsScene *scene, QWidget *parent = nullptr)
        : QGraphicsView(scene, parent)
    {
        setDragMode(QGraphicsView::ScrollHandDrag);
        setRenderHint(QPainter::Antialiasing);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    }

    std::function<void(const QString &)> onNodeDoubleClicked;
    std::function<void()> onViewportReady;
    void setBackgroundPixmap(const QPixmap &pixmap, const QColor &fallbackColor = QColor(15, 18, 24))
    {
        m_backgroundPixmap = pixmap;
        m_backgroundColor = fallbackColor;
        m_backgroundDarkenAlpha = m_backgroundColor.lightness() >= 130 ? 0 : 180;
        viewport()->update();
    }

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        const double factor = (event->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
        scale(factor, factor);
    }

    void showEvent(QShowEvent *event) override
    {
        QGraphicsView::showEvent(event);
        if (onViewportReady)
            onViewportReady();
    }

    void resizeEvent(QResizeEvent *event) override
    {
        QGraphicsView::resizeEvent(event);
        if (onViewportReady)
            onViewportReady();
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (onNodeDoubleClicked) {
            auto *item = itemAt(event->pos());
            // Walk up to find an item tagged with the system nickname.
            while (item && item->data(0).toString().isEmpty())
                item = item->parentItem();
            if (item) {
                onNodeDoubleClicked(item->data(0).toString());
                return;
            }
        }
        QGraphicsView::mouseDoubleClickEvent(event);
    }

    void drawBackground(QPainter *painter, const QRectF &rect) override
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

private:
    QPixmap m_backgroundPixmap;
    QColor m_backgroundColor = QColor(15, 18, 24);
    int m_backgroundDarkenAlpha = 180;
};

// ─── Clickable system node ───────────────────────────────

static constexpr double NODE_RADIUS = 6.0;

class SystemNodeItem : public QGraphicsEllipseItem {
public:
    SystemNodeItem(const QString &nickname, double x, double y)
        : QGraphicsEllipseItem(-NODE_RADIUS, -NODE_RADIUS,
                                NODE_RADIUS * 2, NODE_RADIUS * 2)
        , m_nickname(nickname)
    {
        setPos(x, y);
        setBrush(Qt::NoBrush);
        setPen(Qt::NoPen);
        setToolTip(nickname);
        setData(0, nickname);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
        setZValue(10);

        // Label
        auto *label = new QGraphicsSimpleTextItem(nickname, this);
        label->setFont(QFont(QStringLiteral("Segoe UI"), 7));
        label->setBrush(QColor(220, 220, 220));
        label->setPos(NODE_RADIUS + 2, -5);
    }

    QString nickname() const { return m_nickname; }
    std::function<void(const QString &)> onSelected;
    std::function<void(const QString &)> onMoved;
    std::function<void(const QString &)> onMoveFinished;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
    {
        if (change == QGraphicsItem::ItemSelectedHasChanged && value.toBool() && onSelected)
            onSelected(m_nickname);
        if (change == QGraphicsItem::ItemPositionHasChanged && onMoved)
            onMoved(m_nickname);
        return QGraphicsEllipseItem::itemChange(change, value);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
    {
        QGraphicsEllipseItem::mouseReleaseEvent(event);
        if (onMoveFinished)
            onMoveFinished(m_nickname);
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
    {
        Q_UNUSED(option)
        Q_UNUSED(widget)

        painter->setRenderHint(QPainter::Antialiasing, true);

        const bool selected = isSelected();
        const QColor coreColor = selected ? QColor(255, 220, 120) : QColor(185, 225, 255);
        const QColor glowColor = selected ? QColor(255, 200, 80, 110) : QColor(120, 190, 255, 80);
        const QColor haloColor = selected ? QColor(255, 170, 60, 55) : QColor(110, 180, 255, 35);

        painter->setPen(Qt::NoPen);
        painter->setBrush(haloColor);
        painter->drawEllipse(QPointF(0.0, 0.0), NODE_RADIUS + 4.0, NODE_RADIUS + 4.0);

        painter->setBrush(glowColor);
        painter->drawEllipse(QPointF(0.0, 0.0), NODE_RADIUS + 2.2, NODE_RADIUS + 2.2);

        QRadialGradient coreGradient(QPointF(-1.4, -1.4), NODE_RADIUS + 1.8);
        coreGradient.setColorAt(0.0, QColor(255, 255, 255, 255));
        coreGradient.setColorAt(0.35, coreColor.lighter(125));
        coreGradient.setColorAt(0.75, coreColor);
        coreGradient.setColorAt(1.0, QColor(coreColor.red(), coreColor.green(), coreColor.blue(), 150));
        painter->setBrush(coreGradient);
        painter->drawEllipse(QPointF(0.0, 0.0), NODE_RADIUS, NODE_RADIUS);

        painter->setBrush(QColor(255, 255, 255, selected ? 210 : 180));
        painter->drawEllipse(QPointF(-1.4, -1.4), 1.8, 1.8);
    }

private:
    QString m_nickname;
};

// ─── Constructor / Destructor ────────────────────────────

UniverseEditorPage::UniverseEditorPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

UniverseEditorPage::~UniverseEditorPage() = default;

// ─── UI Setup ────────────────────────────────────────────

void UniverseEditorPage::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_sectorTabs = new QTabBar(this);
    m_sectorTabs->setDocumentMode(true);
    m_sectorTabs->setDrawBase(false);
    m_sectorTabs->setExpanding(false);
    m_sectorTabs->setVisible(false);
    connect(m_sectorTabs, &QTabBar::currentChanged, this, [this](int index) {
        if (!m_sectorTabs || index < 0 || index >= m_sectorTabs->count())
            return;
        applySector(m_sectorTabs->tabData(index).toString());
    });
    mainLayout->addWidget(m_sectorTabs);

    m_splitter = new QSplitter(Qt::Horizontal, this);

    // System tree (left panel)
    m_systemTree = new QTreeWidget(this);
    m_systemTree->setHeaderLabels({tr("System"), tr("Position")});
    m_systemTree->setColumnWidth(0, 180);
    m_systemTree->header()->setStretchLastSection(true);
    m_systemTree->setAlternatingRowColors(true);
    m_systemTree->setRootIsDecorated(false);
    m_systemTree->setSelectionMode(QAbstractItemView::SingleSelection);

    connect(m_systemTree, &QTreeWidget::itemClicked,
            this, &UniverseEditorPage::onSystemSelected);
    connect(m_systemTree, &QTreeWidget::itemDoubleClicked,
            this, &UniverseEditorPage::onSystemDoubleClicked);

    // Map (right panel)
    m_mapScene = new QGraphicsScene(this);
    auto *mapView = new UniverseMapView(m_mapScene, this);
    mapView->setBackgroundPixmap(QPixmap(QStringLiteral(":/images/star-background.png")),
                                 QColor(15, 18, 24));
    mapView->onNodeDoubleClicked = [this](const QString &nickname) {
        onMapItemDoubleClicked(nickname);
    };
    mapView->onViewportReady = [this]() {
        if (m_pendingInitialFitPasses > 0)
            QMetaObject::invokeMethod(this, &UniverseEditorPage::fitMapInView, Qt::QueuedConnection);
    };
    m_mapView = mapView;

    auto *mapPanel = new QWidget(this);
    auto *mapLayout = new QVBoxLayout(mapPanel);
    mapLayout->setContentsMargins(0, 0, 0, 0);
    mapLayout->setSpacing(0);

    m_mapToolBar = new QToolBar(mapPanel);
    m_mapToolBar->setIconSize(QSize(16, 16));
    m_mapToolBar->setMovable(false);
    m_mapToolBar->addAction(tr("Add System"), this, &UniverseEditorPage::onAddSystem);
    m_mapToolBar->addAction(tr("Delete System"), this, &UniverseEditorPage::onDeleteSystem);
    m_mapToolBar->addSeparator();
    m_mapToolBar->addAction(tr("Fit View"), this, &UniverseEditorPage::fitMapInView);
    m_mapToolBar->addAction(tr("Shortest Path..."), this, &UniverseEditorPage::onFindShortestPath);
    m_mapToolBar->addSeparator();
    m_moveAction = m_mapToolBar->addAction(tr("Move"));
    m_moveAction->setCheckable(true);
    m_moveAction->setChecked(false);
    connect(m_moveAction, &QAction::toggled, this, &UniverseEditorPage::setMoveEnabled);

    mapLayout->addWidget(m_mapToolBar);
    mapLayout->addWidget(m_mapView);

    m_splitter->addWidget(m_systemTree);
    m_splitter->addWidget(mapPanel);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({240, 800});

    mainLayout->addWidget(m_splitter);
}
// ─── File I/O ────────────────────────────────────────────

bool UniverseEditorPage::loadFile(const QString &filePath)
{
    auto data = UniverseSerializer::load(filePath);
    if (!data)
        return false;

    m_data = std::move(data);
    m_filePath = filePath;
    m_dirty = false;
    m_highlightedPath.clear();
    m_activeSector = QStringLiteral("universe");

    rebuildSectorTabs();
    refreshSystemList();
    refreshMap();
    refreshTitle();
    return true;
}

bool UniverseEditorPage::save()
{
    if (m_filePath.isEmpty() || !m_data)
        return false;
    if (!UniverseSerializer::save(*m_data, m_filePath))
        return false;
    setDirty(false);
    return true;
}

bool UniverseEditorPage::saveAs(const QString &filePath)
{
    if (!m_data)
        return false;
    if (!m_filePath.isEmpty() && m_filePath != filePath && QFileInfo::exists(m_filePath)) {
        QFile::remove(filePath);
        QFile::copy(m_filePath, filePath);
    }
    if (UniverseSerializer::save(*m_data, filePath)) {
        m_filePath = filePath;
        setDirty(false);
        return true;
    }
    return false;
}

UniverseData *UniverseEditorPage::data() const
{
    return m_data.get();
}

// ─── System list ─────────────────────────────────────────

void UniverseEditorPage::refreshSystemList()
{
    m_systemTree->clear();
    if (!m_data) return;

    for (const auto &sys : m_data->systems) {
        if (!systemVisibleInActiveSector(sys))
            continue;
        auto *item = new QTreeWidgetItem(m_systemTree);
        item->setText(0, sys.nickname);
        const QPointF pos = scenePositionForSystem(sys);
        item->setText(1, QStringLiteral("%1, %2")
                        .arg(pos.x(), 0, 'f', 0)
                        .arg(pos.y(), 0, 'f', 0));
        item->setData(0, Qt::UserRole, sys.nickname);
    }
}

// ─── Map rendering ───────────────────────────────────────

void UniverseEditorPage::refreshMap()
{
    m_mapScene->clear();
    m_connectionItems.clear();
    m_externalConnectionItems.clear();
    m_pathHighlightItems.clear();
    if (!m_data) return;

    // Compute adaptive scale: normalize so largest coordinate → 500 scene units
    double maxCoord = 1.0;
    for (const auto &sys : m_data->systems) {
        if (!systemVisibleInActiveSector(sys))
            continue;
        const QPointF pos = scenePositionForSystem(sys);
        maxCoord = std::max(maxCoord, std::abs(pos.x()));
        maxCoord = std::max(maxCoord, std::abs(pos.y()));
    }
    m_mapScale = 500.0 / maxCoord;

    // Draw system nodes
    for (const auto &sys : m_data->systems) {
        if (!systemVisibleInActiveSector(sys))
            continue;
        const QPointF pos = scenePositionForSystem(sys);
        double x = pos.x() * m_mapScale;
        double y = pos.y() * m_mapScale; // Y increases downward (FL row convention)
        auto *node = new SystemNodeItem(sys.nickname, x, y);
        node->setFlag(QGraphicsItem::ItemIsMovable, m_moveEnabled);
        node->onSelected = [this](const QString &nickname) { onNodeSelected(nickname); };
        node->onMoved = [this](const QString &nickname) { onNodeMoved(nickname); };
        node->onMoveFinished = [this](const QString &nickname) { onNodeMoveFinished(nickname); };
        m_mapScene->addItem(node);
    }

    drawConnections();
    if (!m_highlightedPath.isEmpty())
        highlightPath(m_highlightedPath);

    const QRectF contentRect = mapContentRect();
    if (contentRect.isValid())
        m_mapScene->setSceneRect(contentRect);

    // Fit the map once after loading/rebuilding the scene.
    m_pendingInitialFitPasses = 3;
    QMetaObject::invokeMethod(this, &UniverseEditorPage::fitMapInView, Qt::QueuedConnection);
}

void UniverseEditorPage::drawConnections()
{
    if (!m_data) return;
    clearConnectionLines();

    // Build node position map
    QHash<QString, QPointF> posMap;
    for (const auto &sys : m_data->systems) {
        if (!systemVisibleInActiveSector(sys))
            continue;
        const QPointF pos = scenePositionForSystem(sys);
        double x = pos.x() * m_mapScale;
        double y = pos.y() * m_mapScale;
        posMap.insert(sys.nickname.toLower(), QPointF(x, y));
    }

    // Color-code connections by kind
    QPen gatePen(QColor(90, 170, 255, 210), 1.0);       // blue = jump gate
    QPen holePen(QColor(255, 150, 70, 220), 1.0);      // orange = jump hole
    QPen otherPen(QColor(90, 210, 120, 190), 1.0);     // green = other
    QPen sectorPen(QColor(190, 110, 255, 220), 1.2);   // purple = external sector link
    gatePen.setStyle(Qt::SolidLine);
    holePen.setStyle(Qt::SolidLine);
    otherPen.setStyle(Qt::SolidLine);
    sectorPen.setStyle(Qt::SolidLine);

    const QRectF bounds = mapContentRect();

    for (const auto &conn : m_data->connections) {
        auto fromIt = posMap.find(conn.fromSystem.toLower());
        auto toIt = posMap.find(conn.toSystem.toLower());
        if (fromIt != posMap.end() && toIt != posMap.end()) {
            QPen pen = otherPen;
            if (conn.kind == QStringLiteral("gate"))
                pen = gatePen;
            else if (conn.kind == QStringLiteral("hole"))
                pen = holePen;
            auto *line = m_mapScene->addLine(
                QLineF(fromIt.value(), toIt.value()), pen);
            line->setZValue(1); // Behind nodes
            m_connectionItems.append(line);
            continue;
        }

        if (m_activeSector.compare(QStringLiteral("universe"), Qt::CaseInsensitive) == 0)
            continue;

        const bool fromVisible = fromIt != posMap.end();
        const bool toVisible = toIt != posMap.end();
        if (fromVisible == toVisible)
            continue;

        const QString visibleNickname = fromVisible ? conn.fromSystem : conn.toSystem;
        const QString hiddenNickname = fromVisible ? conn.toSystem : conn.fromSystem;
        const SystemInfo *visibleSys = m_data->findSystem(visibleNickname);
        const SystemInfo *hiddenSys = m_data->findSystem(hiddenNickname);
        if (!visibleSys || !hiddenSys)
            continue;

        const QString targetSector = externalSectorForSystem(*hiddenSys);
        if (targetSector.isEmpty())
            continue;

        const QPointF fromPoint = fromVisible ? fromIt.value() : toIt.value();
        const QPointF hiddenUniversePos(hiddenSys->position.x() * m_mapScale,
                                        hiddenSys->position.z() * m_mapScale);
        QPointF edgePoint = connectionEdgePoint(fromPoint, hiddenUniversePos, bounds);
        if (edgePoint == fromPoint) {
            QPointF fallback = hiddenUniversePos;
            if (fallback == fromPoint)
                fallback += QPointF(1.0, 0.0);
            edgePoint = connectionEdgePoint(fromPoint, fallback, bounds.adjusted(-40, -40, 40, 40));
        }

        auto *line = m_mapScene->addLine(QLineF(fromPoint, edgePoint), sectorPen);
        line->setZValue(1);
        m_externalConnectionItems.append(line);

        const QString labelText = sectorDisplayName(targetSector);
        auto *label = m_mapScene->addSimpleText(labelText, QFont(QStringLiteral("Segoe UI"), 7));
        label->setBrush(QColor(220, 180, 255));
        label->setZValue(2);
        QPointF labelPos = edgePoint;
        const QRectF labelRect = label->boundingRect();
        const qreal margin = 6.0;
        if (std::abs(edgePoint.x() - bounds.left()) < 2.0)
            labelPos += QPointF(margin, -labelRect.height() * 0.5);
        else if (std::abs(edgePoint.x() - bounds.right()) < 2.0)
            labelPos += QPointF(-labelRect.width() - margin, -labelRect.height() * 0.5);
        else if (std::abs(edgePoint.y() - bounds.top()) < 2.0)
            labelPos += QPointF(-labelRect.width() * 0.5, margin);
        else
            labelPos += QPointF(-labelRect.width() * 0.5, -labelRect.height() - margin);
        label->setPos(labelPos);
        m_externalConnectionItems.append(label);
    }
}

// ─── Selection / Navigation ──────────────────────────────

void UniverseEditorPage::onSystemSelected(QTreeWidgetItem *item, int)
{
    if (!item) return;
    QString nickname = item->data(0, Qt::UserRole).toString();

    // Highlight on map
    for (auto *gItem : m_mapScene->items()) {
        auto *node = dynamic_cast<SystemNodeItem *>(gItem);
        if (node) {
            if (node->nickname().compare(nickname, Qt::CaseInsensitive) == 0) {
                node->setSelected(true);
                m_mapView->centerOn(node);
            } else {
                node->setSelected(false);
            }
            node->update();
        }
    }
}

void UniverseEditorPage::onNodeSelected(const QString &nickname)
{
    if (!m_systemTree)
        return;

    const QSignalBlocker blocker(m_systemTree);
    for (int i = 0; i < m_systemTree->topLevelItemCount(); ++i) {
        auto *item = m_systemTree->topLevelItem(i);
        if (item && item->data(0, Qt::UserRole).toString().compare(nickname, Qt::CaseInsensitive) == 0) {
            m_systemTree->setCurrentItem(item);
            break;
        }
    }

    onSystemSelected(m_systemTree->currentItem(), 0);
}

void UniverseEditorPage::onSystemDoubleClicked(QTreeWidgetItem *item, int)
{
    if (!item || !m_data) return;
    QString nickname = item->data(0, Qt::UserRole).toString();
    const SystemInfo *sys = m_data->findSystem(nickname);
    if (sys && !sys->filePath.isEmpty()) {
        QString resolved = resolveSystemPath(sys->filePath);
        emit openSystemRequested(sys->nickname, resolved);
    }
}

void UniverseEditorPage::onMapItemDoubleClicked(const QString &nickname)
{
    if (!m_data) return;
    const SystemInfo *sys = m_data->findSystem(nickname);
    if (sys && !sys->filePath.isEmpty()) {
        QString resolved = resolveSystemPath(sys->filePath);
        emit openSystemRequested(sys->nickname, resolved);
    }
}

// ─── Add / Delete ────────────────────────────────────────

void UniverseEditorPage::onAddSystem()
{
    if (!m_data) return;

    bool ok;
    QString nickname = QInputDialog::getText(this, tr("New System"),
                                              tr("System nickname:"),
                                              QLineEdit::Normal, QString(), &ok);
    if (!ok || nickname.trimmed().isEmpty()) return;
    nickname = nickname.trimmed();

    // Check duplicate
    if (m_data->findSystem(nickname)) {
        QMessageBox::warning(this, tr("Duplicate"),
                             tr("A system with nickname '%1' already exists.").arg(nickname));
        return;
    }

    SystemInfo info;
    info.nickname = nickname;
    info.displayName = nickname;
    info.position = QVector3D(0, 0, 0);

    m_data->addSystem(info);
    refreshSystemList();
    refreshMap();
    setDirty(true);
}

void UniverseEditorPage::onDeleteSystem()
{
    if (!m_data) return;

    auto *item = m_systemTree->currentItem();
    if (!item) return;

    QString nickname = item->data(0, Qt::UserRole).toString();
    auto ret = QMessageBox::question(this, tr("Delete System"),
                                      tr("Delete system '%1'?").arg(nickname));
    if (ret != QMessageBox::Yes) return;

    // Remove from data
    m_data->systems.erase(
        std::remove_if(m_data->systems.begin(), m_data->systems.end(),
                        [&](const SystemInfo &s) {
                            return s.nickname.compare(nickname, Qt::CaseInsensitive) == 0;
                        }),
        m_data->systems.end());

    // Remove connections involving this system
    m_data->connections.erase(
        std::remove_if(m_data->connections.begin(), m_data->connections.end(),
                        [&](const JumpConnection &c) {
                            return c.fromSystem.compare(nickname, Qt::CaseInsensitive) == 0 ||
                                   c.toSystem.compare(nickname, Qt::CaseInsensitive) == 0;
                        }),
        m_data->connections.end());

    refreshSystemList();
    refreshMap();
    setDirty(true);
}

// ─── Path Finder ─────────────────────────────────────────

void UniverseEditorPage::clearPathHighlight()
{
    for (auto *item : m_pathHighlightItems) {
        m_mapScene->removeItem(item);
        delete item;
    }
    m_pathHighlightItems.clear();
}

void UniverseEditorPage::highlightPath(const QStringList &systemPath)
{
    m_highlightedPath = systemPath;
    clearPathHighlight();
    if (!m_data || systemPath.size() < 2) return;

    // Positionen sammeln
    QHash<QString, QPointF> posMap;
    for (const auto &sys : m_data->systems) {
        double x = sys.position.x() * m_mapScale;
        double y = sys.position.z() * m_mapScale;
        posMap.insert(sys.nickname.toLower(), QPointF(x, y));
    }

    // Pfadlinien zeichnen (gelb, dick)
    QPen pathPen(QColor(255, 200, 0), 3.0);
    for (int i = 0; i + 1 < systemPath.size(); ++i) {
        auto fromIt = posMap.find(systemPath[i].toLower());
        auto toIt = posMap.find(systemPath[i + 1].toLower());
        if (fromIt != posMap.end() && toIt != posMap.end()) {
            auto *line = m_mapScene->addLine(
                QLineF(fromIt.value(), toIt.value()), pathPen);
            line->setZValue(10); // Über normalen Verbindungen
            m_pathHighlightItems.append(line);
        }
    }

    // Pfad-Knoten hervorheben (gelbe Kreise)
    for (const auto &sysName : systemPath) {
        auto posIt = posMap.find(sysName.toLower());
        if (posIt != posMap.end()) {
            auto *circle = m_mapScene->addEllipse(
                posIt.value().x() - 6, posIt.value().y() - 6, 12, 12,
                QPen(QColor(255, 200, 0), 2.0),
                QBrush(QColor(255, 200, 0, 80)));
            circle->setZValue(11);
            m_pathHighlightItems.append(circle);
        }
    }
}

void UniverseEditorPage::onFindShortestPath()
{
    if (!m_data) return;

    auto *dialog = new flatlas::tools::PathFinderDialog(m_data.get(), this);
    connect(dialog, &flatlas::tools::PathFinderDialog::pathFound,
            this, &UniverseEditorPage::highlightPath);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

QString UniverseEditorPage::resolveSystemPath(const QString &relativePath) const
{
    // universe.ini lives in DATA/UNIVERSE; system file paths are relative to DATA
    QString dataDir = QFileInfo(m_filePath).absolutePath(); // .../DATA/UNIVERSE
    dataDir = QFileInfo(dataDir).absolutePath();            // .../DATA
    QString resolved = flatlas::core::PathUtils::ciResolvePath(dataDir, relativePath);
    if (!resolved.isEmpty())
        return resolved;
    return QDir(dataDir).filePath(relativePath);
}

void UniverseEditorPage::clearConnectionLines()
{
    for (auto *item : m_connectionItems) {
        if (item)
            m_mapScene->removeItem(item);
        delete item;
    }
    m_connectionItems.clear();

    for (auto *item : m_externalConnectionItems) {
        if (item)
            m_mapScene->removeItem(item);
        delete item;
    }
    m_externalConnectionItems.clear();
}

QString UniverseEditorPage::externalSectorForSystem(const SystemInfo &sys) const
{
    QString fallback;
    for (auto it = sys.sectorPositions.constBegin(); it != sys.sectorPositions.constEnd(); ++it) {
        if (it.key().compare(QStringLiteral("universe"), Qt::CaseInsensitive) == 0)
            continue;
        if (it.key().compare(m_activeSector, Qt::CaseInsensitive) != 0)
            return it.key();
        fallback = it.key();
    }
    return fallback;
}

QPointF UniverseEditorPage::connectionEdgePoint(const QPointF &from, const QPointF &towards, const QRectF &bounds) const
{
    QPointF delta = towards - from;
    if (std::abs(delta.x()) < 1e-6 && std::abs(delta.y()) < 1e-6)
        return from;

    double bestT = std::numeric_limits<double>::infinity();
    QPointF bestPoint = from;

    auto consider = [&](double t, const QPointF &point) {
        if (t > 0.0 && t < bestT &&
            point.x() >= bounds.left() - 1.0 && point.x() <= bounds.right() + 1.0 &&
            point.y() >= bounds.top() - 1.0 && point.y() <= bounds.bottom() + 1.0) {
            bestT = t;
            bestPoint = point;
        }
    };

    if (std::abs(delta.x()) > 1e-6) {
        double tLeft = (bounds.left() - from.x()) / delta.x();
        consider(tLeft, QPointF(bounds.left(), from.y() + delta.y() * tLeft));
        double tRight = (bounds.right() - from.x()) / delta.x();
        consider(tRight, QPointF(bounds.right(), from.y() + delta.y() * tRight));
    }
    if (std::abs(delta.y()) > 1e-6) {
        double tTop = (bounds.top() - from.y()) / delta.y();
        consider(tTop, QPointF(from.x() + delta.x() * tTop, bounds.top()));
        double tBottom = (bounds.bottom() - from.y()) / delta.y();
        consider(tBottom, QPointF(from.x() + delta.x() * tBottom, bounds.bottom()));
    }

    return bestPoint;
}

bool UniverseEditorPage::systemVisibleInActiveSector(const SystemInfo &sys) const
{
    if (m_activeSector.compare(QStringLiteral("universe"), Qt::CaseInsensitive) == 0)
        return true;
    return sys.sectorPositions.contains(m_activeSector);
}

QPointF UniverseEditorPage::scenePositionForSystem(const SystemInfo &sys) const
{
    if (m_activeSector.compare(QStringLiteral("universe"), Qt::CaseInsensitive) != 0) {
        const auto it = sys.sectorPositions.constFind(m_activeSector);
        if (it != sys.sectorPositions.constEnd())
            return it.value();
    }
    return QPointF(sys.position.x(), sys.position.z());
}

QString UniverseEditorPage::sectorDisplayName(const QString &sectorKey) const
{
    if (!m_data)
        return sectorKey;

    for (const auto &sector : m_data->sectors) {
        if (sector.key.compare(sectorKey, Qt::CaseInsensitive) == 0)
            return sector.displayName;
    }
    return sectorKey;
}

void UniverseEditorPage::rebuildSectorTabs()
{
    if (!m_sectorTabs)
        return;

    m_sectorTabs->blockSignals(true);
    while (m_sectorTabs->count() > 0)
        m_sectorTabs->removeTab(0);

    if (!m_data || m_data->sectors.size() <= 1) {
        m_sectorTabs->setVisible(false);
        m_sectorTabs->blockSignals(false);
        return;
    }

    int activeIndex = 0;
    for (int i = 0; i < m_data->sectors.size(); ++i) {
        const auto &sector = m_data->sectors[i];
        const int tabIndex = m_sectorTabs->addTab(sector.displayName);
        m_sectorTabs->setTabData(tabIndex, sector.key);
        if (sector.key.compare(m_activeSector, Qt::CaseInsensitive) == 0)
            activeIndex = tabIndex;
    }

    m_sectorTabs->setCurrentIndex(activeIndex);
    m_sectorTabs->setVisible(m_sectorTabs->count() > 1);
    m_sectorTabs->blockSignals(false);
}

void UniverseEditorPage::applySector(const QString &sectorKey)
{
    m_activeSector = sectorKey.trimmed().isEmpty() ? QStringLiteral("universe") : sectorKey.trimmed();

    if (m_moveAction) {
        const bool editable = m_activeSector.compare(QStringLiteral("universe"), Qt::CaseInsensitive) == 0;
        m_moveAction->setEnabled(editable);
        if (!editable && m_moveAction->isChecked())
            m_moveAction->setChecked(false);
    }

    refreshSystemList();
    refreshMap();
    refreshTitle();
}

void UniverseEditorPage::syncSystemPositionFromMap(const QString &nickname)
{
    if (!m_data || m_mapScale <= 0.0 ||
        m_activeSector.compare(QStringLiteral("universe"), Qt::CaseInsensitive) != 0)
        return;

    QPointF scenePos;
    bool foundNode = false;
    for (auto *gItem : m_mapScene->items()) {
        auto *node = dynamic_cast<SystemNodeItem *>(gItem);
        if (node && node->nickname().compare(nickname, Qt::CaseInsensitive) == 0) {
            scenePos = node->pos();
            foundNode = true;
            break;
        }
    }
    if (!foundNode)
        return;

    for (auto &sys : m_data->systems) {
        if (sys.nickname.compare(nickname, Qt::CaseInsensitive) == 0) {
            sys.position.setX(static_cast<float>(scenePos.x() / m_mapScale));
            sys.position.setZ(static_cast<float>(scenePos.y() / m_mapScale));
            break;
        }
    }

    for (int i = 0; i < m_systemTree->topLevelItemCount(); ++i) {
        auto *item = m_systemTree->topLevelItem(i);
        if (item && item->data(0, Qt::UserRole).toString().compare(nickname, Qt::CaseInsensitive) == 0) {
            if (const auto *sys = m_data->findSystem(nickname)) {
                const QPointF pos = scenePositionForSystem(*sys);
                item->setText(1, QStringLiteral("%1, %2")
                    .arg(pos.x(), 0, 'f', 0)
                    .arg(pos.y(), 0, 'f', 0));
            }
            break;
        }
    }
}

QRectF UniverseEditorPage::mapContentRect() const
{
    if (!m_data || m_data->systems.isEmpty())
        return {};

    double minX = 0.0;
    double maxX = 0.0;
    double minY = 0.0;
    double maxY = 0.0;
    bool first = true;

    for (const auto &sys : m_data->systems) {
        if (!systemVisibleInActiveSector(sys))
            continue;
        const QPointF pos = scenePositionForSystem(sys);
        const double x = pos.x() * m_mapScale;
        const double y = pos.y() * m_mapScale;
        if (first) {
            minX = maxX = x;
            minY = maxY = y;
            first = false;
        } else {
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }
    }

    if (first)
        return {};

    double width = maxX - minX;
    double height = maxY - minY;

    // Keep single-system and tiny maps comfortably visible.
    const double minimumSpan = 120.0;
    if (width < minimumSpan) {
        const double expand = (minimumSpan - width) / 2.0;
        minX -= expand;
        maxX += expand;
        width = minimumSpan;
    }
    if (height < minimumSpan) {
        const double expand = (minimumSpan - height) / 2.0;
        minY -= expand;
        maxY += expand;
        height = minimumSpan;
    }

    const double padX = std::max(24.0, width * 0.08);
    const double padY = std::max(24.0, height * 0.08);
    return QRectF(
        QPointF(minX - padX, minY - padY),
        QPointF(maxX + padX, maxY + padY));
}

void UniverseEditorPage::fitMapInView()
{
    if (!m_mapView || !m_mapScene || !m_data || m_data->systems.isEmpty())
        return;
    if (m_mapView->viewport()->width() <= 1 || m_mapView->viewport()->height() <= 1)
        return;

    const QRectF rect = mapContentRect();
    if (!rect.isValid() || rect.isEmpty())
        return;

    m_mapScene->setSceneRect(rect);
    m_mapView->resetTransform();
    m_mapView->fitInView(rect, Qt::KeepAspectRatio);
    if (m_pendingInitialFitPasses > 0)
        --m_pendingInitialFitPasses;
}

void UniverseEditorPage::setMoveEnabled(bool enabled)
{
    m_moveEnabled = enabled;

    if (!m_mapScene)
        return;

    for (auto *gItem : m_mapScene->items()) {
        auto *node = dynamic_cast<SystemNodeItem *>(gItem);
        if (!node)
            continue;
        node->setFlag(QGraphicsItem::ItemIsMovable, enabled);
    }
}

void UniverseEditorPage::onNodeMoved(const QString &nickname)
{
    syncSystemPositionFromMap(nickname);
    drawConnections();
    if (!m_highlightedPath.isEmpty())
        highlightPath(m_highlightedPath);
}

void UniverseEditorPage::onNodeMoveFinished(const QString &nickname)
{
    syncSystemPositionFromMap(nickname);
    setDirty(true);
    onNodeSelected(nickname);
}

void UniverseEditorPage::refreshTitle()
{
    if (!m_data)
        return;

    QString title = QStringLiteral("Universe (%1 systems)").arg(m_data->systemCount());
    if (m_activeSector.compare(QStringLiteral("universe"), Qt::CaseInsensitive) != 0)
        title += QStringLiteral(" - %1").arg(sectorDisplayName(m_activeSector));
    if (m_dirty)
        title += QLatin1Char('*');
    emit titleChanged(title);
}

void UniverseEditorPage::setDirty(bool dirty)
{
    m_dirty = dirty;
    refreshTitle();
}

} // namespace flatlas::editors
