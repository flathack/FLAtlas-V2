// editors/universe/UniverseEditorPage.cpp – Universe-Editor (Phase 9)

#include "UniverseEditorPage.h"
#include "UniverseSerializer.h"
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
#include <QAction>
#include "tools/PathFinderDialog.h"

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

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        const double factor = (event->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
        scale(factor, factor);
    }
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
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setZValue(10);

        // Label
        auto *label = new QGraphicsSimpleTextItem(nickname, this);
        label->setFont(QFont(QStringLiteral("Segoe UI"), 7));
        label->setBrush(QColor(220, 220, 220));
        label->setPos(NODE_RADIUS + 2, -6);
    }

    QString nickname() const { return m_nickname; }

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
    m_mapScene->setBackgroundBrush(QColor(20, 20, 30));
    m_mapView = new UniverseMapView(m_mapScene, this);

    m_splitter->addWidget(m_systemTree);
    m_splitter->addWidget(m_mapView);
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
    m_toolBar->addAction(tr("Fit View"), this, [this]() {
        if (m_mapView && m_mapScene)
            m_mapView->fitInView(m_mapScene->itemsBoundingRect().adjusted(-50, -50, 50, 50),
                                  Qt::KeepAspectRatio);
    });
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

    refreshSystemList();
    refreshMap();

    emit titleChanged(QStringLiteral("Universe (%1 systems)").arg(m_data->systemCount()));
    return true;
}

bool UniverseEditorPage::save()
{
    if (m_filePath.isEmpty() || !m_data)
        return false;
    return UniverseSerializer::save(*m_data, m_filePath);
}

bool UniverseEditorPage::saveAs(const QString &filePath)
{
    if (!m_data)
        return false;
    if (UniverseSerializer::save(*m_data, filePath)) {
        m_filePath = filePath;
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

static constexpr double MAP_SCALE = 0.01; // Universe coordinates → scene coordinates

void UniverseEditorPage::refreshMap()
{
    m_mapScene->clear();
    if (!m_data) return;

    // Draw system nodes
    QHash<QString, SystemNodeItem *> nodeMap;
    for (const auto &sys : m_data->systems) {
        double x = sys.position.x() * MAP_SCALE;
        double y = -sys.position.z() * MAP_SCALE; // Flip Z for top-down view
        auto *node = new SystemNodeItem(sys.nickname, x, y);
        m_mapScene->addItem(node);
        nodeMap.insert(sys.nickname.toLower(), node);
    }

    drawConnections();

    // Fit view
    m_mapView->fitInView(m_mapScene->itemsBoundingRect().adjusted(-50, -50, 50, 50),
                          Qt::KeepAspectRatio);
}

void UniverseEditorPage::drawConnections()
{
    if (!m_data) return;

    // Build node position map
    QHash<QString, QPointF> posMap;
    for (const auto &sys : m_data->systems) {
        double x = sys.position.x() * MAP_SCALE;
        double y = -sys.position.z() * MAP_SCALE;
        posMap.insert(sys.nickname.toLower(), QPointF(x, y));
    }

    // Draw connection lines
    QPen linePen(QColor(80, 120, 180, 160), 1.0);
    for (const auto &conn : m_data->connections) {
        auto fromIt = posMap.find(conn.fromSystem.toLower());
        auto toIt = posMap.find(conn.toSystem.toLower());
        if (fromIt != posMap.end() && toIt != posMap.end()) {
            auto *line = m_mapScene->addLine(
                QLineF(fromIt.value(), toIt.value()), linePen);
            line->setZValue(1); // Behind nodes
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

void UniverseEditorPage::onSystemDoubleClicked(QTreeWidgetItem *item, int)
{
    if (!item || !m_data) return;
    QString nickname = item->data(0, Qt::UserRole).toString();
    const SystemInfo *sys = m_data->findSystem(nickname);
    if (sys && !sys->filePath.isEmpty())
        emit openSystemRequested(sys->nickname, sys->filePath);
}

void UniverseEditorPage::onMapItemDoubleClicked(const QString &nickname)
{
    if (!m_data) return;
    const SystemInfo *sys = m_data->findSystem(nickname);
    if (sys && !sys->filePath.isEmpty())
        emit openSystemRequested(sys->nickname, sys->filePath);
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
    emit titleChanged(QStringLiteral("Universe (%1 systems)*").arg(m_data->systemCount()));
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
    emit titleChanged(QStringLiteral("Universe (%1 systems)*").arg(m_data->systemCount()));
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
    clearPathHighlight();
    if (!m_data || systemPath.size() < 2) return;

    // Positionen sammeln
    QHash<QString, QPointF> posMap;
    for (const auto &sys : m_data->systems) {
        double x = sys.position.x() * MAP_SCALE;
        double y = -sys.position.z() * MAP_SCALE;
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

} // namespace flatlas::editors
