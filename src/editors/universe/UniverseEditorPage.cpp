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
#include <functional>
#include <QGraphicsSceneMouseEvent>
#include <QSignalBlocker>
#include <QRectF>
#include <QPainter>
#include <QPixmap>
#include <QWidget>

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

static constexpr double NODE_RADIUS = 8.0;

class SystemNodeItem : public QGraphicsEllipseItem {
public:
    SystemNodeItem(const QString &nickname, double x, double y)
        : QGraphicsEllipseItem(-NODE_RADIUS, -NODE_RADIUS,
                                NODE_RADIUS * 2, NODE_RADIUS * 2)
        , m_nickname(nickname)
    {
        setPos(x, y);
        setBrush(QBrush(QColor(100, 180, 255)));
        setPen(QPen(QColor(40, 80, 140), 1.5));
        setToolTip(nickname);
        setData(0, nickname);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
        setZValue(10);

        // Label
        auto *label = new QGraphicsSimpleTextItem(nickname, this);
        label->setFont(QFont(QStringLiteral("Segoe UI"), 7));
        label->setBrush(QColor(220, 220, 220));
        label->setPos(NODE_RADIUS + 2, -6);
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

    setupToolBar();
    mainLayout->addWidget(m_toolBar);

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

void UniverseEditorPage::setupToolBar()
{
    m_toolBar = new QToolBar(this);
    m_toolBar->setIconSize(QSize(16, 16));
    m_toolBar->setMovable(false);

    m_toolBar->addAction(tr("Add System"), this, &UniverseEditorPage::onAddSystem);
    m_toolBar->addAction(tr("Delete System"), this, &UniverseEditorPage::onDeleteSystem);
    m_toolBar->addSeparator();
    m_toolBar->addAction(tr("Fit View"), this, &UniverseEditorPage::fitMapInView);
    m_toolBar->addSeparator();
    m_toolBar->addAction(tr("Shortest Path..."), this, &UniverseEditorPage::onFindShortestPath);
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
        auto *item = new QTreeWidgetItem(m_systemTree);
        item->setText(0, sys.nickname);
        item->setText(1, QStringLiteral("%1, %2")
                        .arg(sys.position.x(), 0, 'f', 0)
                        .arg(sys.position.z(), 0, 'f', 0));
        item->setData(0, Qt::UserRole, sys.nickname);
    }
}

// ─── Map rendering ───────────────────────────────────────

void UniverseEditorPage::refreshMap()
{
    m_mapScene->clear();
    m_connectionItems.clear();
    m_pathHighlightItems.clear();
    if (!m_data) return;

    // Compute adaptive scale: normalize so largest coordinate → 500 scene units
    double maxCoord = 1.0;
    for (const auto &sys : m_data->systems) {
        maxCoord = std::max(maxCoord, static_cast<double>(std::abs(sys.position.x())));
        maxCoord = std::max(maxCoord, static_cast<double>(std::abs(sys.position.z())));
    }
    m_mapScale = 500.0 / maxCoord;

    // Draw system nodes
    for (const auto &sys : m_data->systems) {
        double x = sys.position.x() * m_mapScale;
        double y = sys.position.z() * m_mapScale; // Y increases downward (FL row convention)
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
        double x = sys.position.x() * m_mapScale;
        double y = sys.position.z() * m_mapScale;
        posMap.insert(sys.nickname.toLower(), QPointF(x, y));
    }

    // Color-code connections by kind
    QPen gatePen(QColor(60, 180, 60, 200), 1.5);      // green = jump gate
    QPen holePen(QColor(200, 100, 40, 200), 1.2);      // orange = jump hole
    QPen otherPen(QColor(160, 60, 200, 160), 1.0);     // purple = alien gate / other
    gatePen.setStyle(Qt::SolidLine);
    holePen.setStyle(Qt::DashLine);
    otherPen.setStyle(Qt::DotLine);

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
        }
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
                node->setBrush(QBrush(QColor(255, 200, 60)));
                m_mapView->centerOn(node);
            } else {
                node->setBrush(QBrush(QColor(100, 180, 255)));
            }
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
}

void UniverseEditorPage::syncSystemPositionFromMap(const QString &nickname)
{
    if (!m_data || m_mapScale <= 0.0)
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
                item->setText(1, QStringLiteral("%1, %2")
                    .arg(sys->position.x(), 0, 'f', 0)
                    .arg(sys->position.z(), 0, 'f', 0));
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
        const double x = sys.position.x() * m_mapScale;
        const double y = sys.position.z() * m_mapScale;
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
