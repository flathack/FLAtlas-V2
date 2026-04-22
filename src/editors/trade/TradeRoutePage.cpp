// editors/trade/TradeRoutePage.cpp – Trade-Route-Workspace

#include "TradeRoutePage.h"
#include "TradeScoring.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QGroupBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QTableWidget>
#include <QSpinBox>
#include <QElapsedTimer>
#include <QSplitter>

#include <QtConcurrent/QtConcurrent>

using namespace flatlas::domain;

namespace flatlas::editors {

namespace {

QString baseDisplayLabel(const TradeBaseRecord &base)
{
    return QStringLiteral("%1 (%2)").arg(base.displayName, base.systemDisplayName);
}

QString formatSeconds(int seconds)
{
    const int minutes = seconds / 60;
    const int remainingSeconds = seconds % 60;
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(remainingSeconds, 2, 10, QLatin1Char('0'));
}

} // namespace

TradeRoutePage::TradeRoutePage(QWidget *parent)
    : QWidget(parent)
    , m_loadWatcher(new QFutureWatcher<TradeRouteWorkspaceData>(this))
    , m_calcWatcher(new QFutureWatcher<QVector<TradeRouteCandidate>>(this))
    , m_recalcTimer(new QTimer(this))
{
    setupUi();

    m_recalcTimer->setInterval(250);
    m_recalcTimer->setSingleShot(true);
    connect(m_recalcTimer, &QTimer::timeout, this, &TradeRoutePage::startRecalculation);
    connect(m_loadWatcher, &QFutureWatcher<TradeRouteWorkspaceData>::finished, this, [this]() {
        m_workspace = m_loadWatcher->result();
        if (m_universe)
            m_workspace.universe = std::shared_ptr<UniverseData>(const_cast<UniverseData *>(m_universe), [](UniverseData *) {});
        populateCommodityFilter();
        populateCommodityTable();
        populatePriceTable();
        scheduleRecalculation();
    });
    connect(m_calcWatcher, &QFutureWatcher<QVector<TradeRouteCandidate>>::finished, this, [this]() {
        m_routes = m_calcWatcher->result();
        populateRouteTable();
        if (!m_routes.isEmpty())
            updateRouteDetails(0);
        else
            updateRouteDetails(-1);
        m_statusLabel->setText(tr("%1 routes ready. %2 commodities, %3 editable price points.")
                                   .arg(m_routes.size())
                                   .arg(m_workspace.commodities.size())
                                   .arg(std::count_if(m_workspace.prices.begin(), m_workspace.prices.end(), [](const TradePriceRecord &price) {
                                       return !price.implicit;
                                   })));
        emit titleChanged(QStringLiteral("Trade Routes (%1)").arg(m_routes.size()));
        emit scanComplete(m_routes.size());
    });
}

void TradeRoutePage::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setupToolBar();
    layout->addWidget(m_toolBar);

    auto *mainSplitter = new QSplitter(Qt::Horizontal, this);
    auto *editorSplitter = new QSplitter(Qt::Vertical, mainSplitter);

    auto *commodityBox = new QGroupBox(tr("Commodities"), editorSplitter);
    auto *commodityLayout = new QVBoxLayout(commodityBox);
    m_commodityTable = new QTableWidget(commodityBox);
    m_commodityTable->setColumnCount(6);
    m_commodityTable->setHorizontalHeaderLabels({
        tr("Nickname"), tr("Display"), tr("Base Price"), tr("Volume"), tr("ids_name"), tr("ids_info")
    });
    m_commodityTable->horizontalHeader()->setStretchLastSection(true);
    m_commodityTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_commodityTable->setAlternatingRowColors(true);
    m_commodityTable->verticalHeader()->setVisible(false);
    commodityLayout->addWidget(m_commodityTable);

    auto *priceBox = new QGroupBox(tr("Base Prices"), editorSplitter);
    auto *priceLayout = new QVBoxLayout(priceBox);
    m_priceTable = new QTableWidget(priceBox);
    m_priceTable->setColumnCount(5);
    m_priceTable->setHorizontalHeaderLabels({
        tr("Base"), tr("System"), tr("Price"), tr("Role"), tr("State")
    });
    m_priceTable->horizontalHeader()->setStretchLastSection(true);
    m_priceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_priceTable->setAlternatingRowColors(true);
    m_priceTable->verticalHeader()->setVisible(false);
    priceLayout->addWidget(m_priceTable);

    auto *routeBox = new QGroupBox(tr("Routes"), mainSplitter);
    auto *routeLayout = new QVBoxLayout(routeBox);
    m_routeModel = new QStandardItemModel(this);
    m_routeModel->setHorizontalHeaderLabels({
        tr("Commodity"), tr("Buy"), tr("Sell"), tr("Unit Profit"), tr("Total Profit"),
        tr("Jumps"), tr("Time"), tr("Distance"), tr("Profit/Min"), tr("Score")
    });
    m_routeView = new QTableView(routeBox);
    m_routeView->setModel(m_routeModel);
    m_routeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_routeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_routeView->setAlternatingRowColors(true);
    m_routeView->setSortingEnabled(true);
    m_routeView->horizontalHeader()->setStretchLastSection(true);
    m_routeView->verticalHeader()->setVisible(false);
    routeLayout->addWidget(m_routeView);

    auto *detailBox = new QGroupBox(tr("Analysis"), mainSplitter);
    auto *detailLayout = new QVBoxLayout(detailBox);
    m_routeSummaryLabel = new QLabel(detailBox);
    m_routeSummaryLabel->setWordWrap(true);
    detailLayout->addWidget(m_routeSummaryLabel);

    auto *metricsForm = new QFormLayout;
    m_profitLabel = new QLabel(detailBox);
    m_timeLabel = new QLabel(detailBox);
    m_distanceLabel = new QLabel(detailBox);
    m_scoreLabel = new QLabel(detailBox);
    m_pathLabel = new QLabel(detailBox);
    m_pathLabel->setWordWrap(true);
    metricsForm->addRow(tr("Profit"), m_profitLabel);
    metricsForm->addRow(tr("Travel"), m_timeLabel);
    metricsForm->addRow(tr("Distance"), m_distanceLabel);
    metricsForm->addRow(tr("Score"), m_scoreLabel);
    metricsForm->addRow(tr("Path"), m_pathLabel);
    detailLayout->addLayout(metricsForm);

    m_warningList = new QListWidget(detailBox);
    detailLayout->addWidget(m_warningList);

    m_routeScene = new QGraphicsScene(detailBox);
    m_routeViewWidget = new QGraphicsView(m_routeScene, detailBox);
    m_routeViewWidget->setMinimumHeight(180);
    detailLayout->addWidget(m_routeViewWidget);

    m_segmentTable = new QTableWidget(detailBox);
    m_segmentTable->setColumnCount(5);
    m_segmentTable->setHorizontalHeaderLabels({
        tr("Type"), tr("System"), tr("From"), tr("To"), tr("Seconds")
    });
    m_segmentTable->horizontalHeader()->setStretchLastSection(true);
    m_segmentTable->verticalHeader()->setVisible(false);
    m_segmentTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    detailLayout->addWidget(m_segmentTable);

    mainSplitter->addWidget(editorSplitter);
    mainSplitter->addWidget(routeBox);
    mainSplitter->addWidget(detailBox);
    mainSplitter->setStretchFactor(0, 2);
    mainSplitter->setStretchFactor(1, 3);
    mainSplitter->setStretchFactor(2, 2);
    editorSplitter->setStretchFactor(0, 1);
    editorSplitter->setStretchFactor(1, 2);
    layout->addWidget(mainSplitter);

    m_statusLabel = new QLabel(tr("Ready. Load Freelancer data to build the trade workspace."), this);
    layout->addWidget(m_statusLabel);

    connect(m_commodityTable, &QTableWidget::itemSelectionChanged, this, &TradeRoutePage::populatePriceTable);
    connect(m_commodityTable, &QTableWidget::cellChanged, this, [this](int row, int column) {
        if (m_updatingTables || row < 0 || row >= m_workspace.commodities.size())
            return;
        auto &commodity = m_workspace.commodities[row];
        const QString text = m_commodityTable->item(row, column) ? m_commodityTable->item(row, column)->text().trimmed() : QString();
        switch (column) {
        case 0: commodity.nickname = text; break;
        case 1: commodity.displayName = text; break;
        case 2: commodity.basePrice = qMax(0, text.toInt()); break;
        case 3: commodity.volume = qMax(1, text.toInt()); break;
        case 4: commodity.idsName = qMax(0, text.toInt()); break;
        case 5: commodity.idsInfo = qMax(0, text.toInt()); break;
        default: break;
        }
        for (auto &price : m_workspace.prices) {
            if (price.commodityNickname.compare(commodity.nickname, Qt::CaseInsensitive) == 0 && price.implicit) {
                price.price = commodity.basePrice;
                price.multiplier = 1.0;
            }
        }
        markDirty();
        populateCommodityFilter();
        populatePriceTable();
        scheduleRecalculation();
    });
    connect(m_priceTable, &QTableWidget::cellChanged, this, [this](int row, int column) {
        if (m_updatingTables)
            return;
        const QString commodityNickname = selectedCommodityNickname();
        if (commodityNickname.isEmpty())
            return;

        int visibleIndex = -1;
        for (int i = 0; i < m_workspace.prices.size(); ++i) {
            const auto &price = m_workspace.prices.at(i);
            if (price.commodityNickname.compare(commodityNickname, Qt::CaseInsensitive) != 0)
                continue;
            if (visibleIndex == row) {
                if (price.implicit)
                    return;
                auto &editablePrice = m_workspace.prices[i];
                if (column == 2) {
                    editablePrice.price = qMax(0, m_priceTable->item(row, column)->text().toInt());
                    const auto commodityIt = std::find_if(m_workspace.commodities.begin(), m_workspace.commodities.end(), [commodityNickname](const TradeCommodityRecord &commodity) {
                        return commodity.nickname.compare(commodityNickname, Qt::CaseInsensitive) == 0;
                    });
                    if (commodityIt != m_workspace.commodities.end() && commodityIt->basePrice > 0)
                        editablePrice.multiplier = static_cast<double>(editablePrice.price) / static_cast<double>(commodityIt->basePrice);
                } else if (column == 3) {
                    editablePrice.isSource = m_priceTable->item(row, column)->text().trimmed().compare(QStringLiteral("Source"), Qt::CaseInsensitive) == 0;
                }
                markDirty();
                scheduleRecalculation();
                return;
            }
            ++visibleIndex;
        }
    });
    connect(m_routeView->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this](const QModelIndex &current) {
        updateRouteDetails(current.row());
    });
}

void TradeRoutePage::setupToolBar()
{
    m_toolBar = new QToolBar(this);
    m_toolBar->setMovable(false);
    m_toolBar->setIconSize(QSize(16, 16));

    m_toolBar->addAction(tr("Reload"), this, &TradeRoutePage::reloadWorkspace);
    m_toolBar->addAction(tr("Save All"), this, &TradeRoutePage::saveWorkspace);
    m_toolBar->addAction(tr("Recalculate"), this, &TradeRoutePage::scanAndCalculate);
    m_toolBar->addSeparator();
    m_toolBar->addAction(tr("Add Commodity"), this, &TradeRoutePage::addCommodity);
    m_toolBar->addAction(tr("Remove Commodity"), this, &TradeRoutePage::removeCommodity);
    m_toolBar->addAction(tr("Add Price"), this, &TradeRoutePage::addPriceEntry);
    m_toolBar->addAction(tr("Remove Price"), this, &TradeRoutePage::removePriceEntry);

    m_toolBar->addSeparator();
    m_toolBar->addWidget(new QLabel(tr(" Search: "), m_toolBar));
    m_searchEdit = new QLineEdit(m_toolBar);
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setPlaceholderText(tr("commodity, base, system"));
    m_toolBar->addWidget(m_searchEdit);

    m_toolBar->addWidget(new QLabel(tr(" Commodity: "), m_toolBar));
    m_commodityFilter = new QComboBox(m_toolBar);
    m_toolBar->addWidget(m_commodityFilter);

    m_toolBar->addWidget(new QLabel(tr(" Cargo: "), m_toolBar));
    m_cargoSpin = new QSpinBox(m_toolBar);
    m_cargoSpin->setRange(1, 10000);
    m_cargoSpin->setValue(275);
    m_toolBar->addWidget(m_cargoSpin);

    m_toolBar->addWidget(new QLabel(tr(" Max jumps: "), m_toolBar));
    m_maxJumpsSpin = new QSpinBox(m_toolBar);
    m_maxJumpsSpin->setRange(0, 64);
    m_maxJumpsSpin->setValue(12);
    m_toolBar->addWidget(m_maxJumpsSpin);

    m_maxResultsSpin = new QSpinBox(m_toolBar);
    m_maxResultsSpin->setRange(10, 1000);
    m_maxResultsSpin->setValue(250);
    m_toolBar->addWidget(new QLabel(tr(" Max routes: "), m_toolBar));
    m_toolBar->addWidget(m_maxResultsSpin);

    m_localOnlyCheck = new QCheckBox(tr("Local only"), m_toolBar);
    m_toolBar->addWidget(m_localOnlyCheck);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &TradeRoutePage::scheduleRecalculation);
    connect(m_commodityFilter, &QComboBox::currentTextChanged, this, &TradeRoutePage::scheduleRecalculation);
    connect(m_cargoSpin, qOverload<int>(&QSpinBox::valueChanged), this, &TradeRoutePage::scheduleRecalculation);
    connect(m_maxJumpsSpin, qOverload<int>(&QSpinBox::valueChanged), this, &TradeRoutePage::scheduleRecalculation);
    connect(m_maxResultsSpin, qOverload<int>(&QSpinBox::valueChanged), this, &TradeRoutePage::scheduleRecalculation);
    connect(m_localOnlyCheck, &QCheckBox::checkStateChanged, this, &TradeRoutePage::scheduleRecalculation);
}

void TradeRoutePage::setDataPath(const QString &dataPath)
{
    m_dataPath = dataPath;
}

void TradeRoutePage::setUniverseData(const UniverseData *universe)
{
    m_universe = universe;
}

void TradeRoutePage::setBaseSystemMap(const QHash<QString, QString> &map)
{
    m_baseToSystem = map;
}

void TradeRoutePage::scanAndCalculate()
{
    if (m_dataPath.isEmpty()) {
        m_statusLabel->setText(tr("No data path set."));
        return;
    }

    if (m_workspace.dataPath.compare(m_dataPath, Qt::CaseInsensitive) != 0 && !m_loadWatcher->isRunning()) {
        reloadWorkspace();
        return;
    }

    scheduleRecalculation();
}

void TradeRoutePage::reloadWorkspace()
{
    if (m_dataPath.isEmpty()) {
        m_statusLabel->setText(tr("No data path set."));
        return;
    }

    m_statusLabel->setText(tr("Loading commodities, markets and universe graph..."));
    const QString dataPath = m_dataPath;
    m_loadWatcher->setFuture(QtConcurrent::run([dataPath]() {
        return TradeRouteDataService::loadFromDataPath(dataPath);
    }));
}

void TradeRoutePage::saveWorkspace()
{
    QString errorMessage;
    if (!TradeRouteDataService::saveWorkspace(m_workspace, &errorMessage)) {
        QMessageBox::warning(this, tr("Trade Route Editor"), errorMessage);
        return;
    }

    m_dirty = false;
    m_statusLabel->setText(tr("Trade data saved to goods.ini and market_commodities.ini."));
    reloadWorkspace();
}

void TradeRoutePage::scheduleRecalculation()
{
    if (!m_workspace.dataPath.isEmpty())
        m_recalcTimer->start();
}

void TradeRoutePage::startRecalculation()
{
    if (m_workspace.dataPath.isEmpty())
        return;

    TradeRouteWorkspaceData workspace = m_workspace;
    if (m_universe)
        workspace.universe = std::shared_ptr<UniverseData>(const_cast<UniverseData *>(m_universe), [](UniverseData *) {});

    TradeRouteFilter filter;
    filter.searchText = m_searchEdit->text().trimmed();
    filter.commodityNickname = m_commodityFilter->currentData().toString();
    filter.cargoCapacity = m_cargoSpin->value();
    filter.maxJumps = m_maxJumpsSpin->value();
    filter.maxResults = m_maxResultsSpin->value();
    filter.localRoutesOnly = m_localOnlyCheck->isChecked();

    m_statusLabel->setText(tr("Recalculating trade routes..."));
    m_calcWatcher->setFuture(QtConcurrent::run([workspace, filter]() {
        TradeScoring scoring;
        scoring.setWorkspaceData(&workspace);
        return scoring.calculateRoutes(filter);
    }));
}

void TradeRoutePage::populateCommodityTable()
{
    m_updatingTables = true;
    m_commodityTable->setRowCount(m_workspace.commodities.size());
    for (int row = 0; row < m_workspace.commodities.size(); ++row) {
        const auto &commodity = m_workspace.commodities.at(row);
        m_commodityTable->setItem(row, 0, new QTableWidgetItem(commodity.nickname));
        m_commodityTable->setItem(row, 1, new QTableWidgetItem(commodity.displayName));
        m_commodityTable->setItem(row, 2, new QTableWidgetItem(QString::number(commodity.basePrice)));
        m_commodityTable->setItem(row, 3, new QTableWidgetItem(QString::number(commodity.volume)));
        m_commodityTable->setItem(row, 4, new QTableWidgetItem(QString::number(commodity.idsName)));
        m_commodityTable->setItem(row, 5, new QTableWidgetItem(QString::number(commodity.idsInfo)));
    }
    m_commodityTable->resizeColumnsToContents();
    m_updatingTables = false;
    if (m_commodityTable->rowCount() > 0 && m_commodityTable->currentRow() < 0)
        m_commodityTable->selectRow(0);
}

void TradeRoutePage::populatePriceTable()
{
    m_updatingTables = true;
    const QString commodityNickname = selectedCommodityNickname();
    QVector<TradePriceRecord> prices;
    for (const auto &price : m_workspace.prices) {
        if (commodityNickname.isEmpty() || price.commodityNickname.compare(commodityNickname, Qt::CaseInsensitive) == 0)
            prices.append(price);
    }

    m_priceTable->setRowCount(prices.size());
    for (int row = 0; row < prices.size(); ++row) {
        const auto &price = prices.at(row);
        auto *baseItem = new QTableWidgetItem(price.baseDisplayName.isEmpty() ? price.baseNickname : price.baseDisplayName);
        baseItem->setFlags(baseItem->flags() & ~Qt::ItemIsEditable);
        m_priceTable->setItem(row, 0, baseItem);

        auto *systemItem = new QTableWidgetItem(price.systemNickname);
        systemItem->setFlags(systemItem->flags() & ~Qt::ItemIsEditable);
        m_priceTable->setItem(row, 1, systemItem);

        auto *priceItem = new QTableWidgetItem(QString::number(price.price));
        if (price.implicit)
            priceItem->setFlags(priceItem->flags() & ~Qt::ItemIsEditable);
        m_priceTable->setItem(row, 2, priceItem);

        auto *roleItem = new QTableWidgetItem(price.isSource ? tr("Source") : tr("Sink"));
        if (price.implicit)
            roleItem->setFlags(roleItem->flags() & ~Qt::ItemIsEditable);
        m_priceTable->setItem(row, 3, roleItem);

        auto *stateItem = new QTableWidgetItem(price.implicit ? tr("Implicit") : tr("Explicit"));
        stateItem->setFlags(stateItem->flags() & ~Qt::ItemIsEditable);
        m_priceTable->setItem(row, 4, stateItem);
    }
    m_priceTable->resizeColumnsToContents();
    m_updatingTables = false;
}

void TradeRoutePage::populateRouteTable()
{
    m_routeModel->removeRows(0, m_routeModel->rowCount());
    for (int index = 0; index < m_routes.size(); ++index) {
        const auto &route = m_routes.at(index);
        QList<QStandardItem *> row;
        row.append(new QStandardItem(route.commodityDisplayName.isEmpty() ? route.commodity : route.commodityDisplayName));
        row.append(new QStandardItem(route.fromBase));
        row.append(new QStandardItem(route.toBase));
        row.append(new QStandardItem(QString::number(route.profit)));
        row.append(new QStandardItem(QString::number(route.totalProfit)));
        row.append(new QStandardItem(QString::number(route.jumps)));
        row.append(new QStandardItem(formatSeconds(route.travelTimeSeconds)));
        row.append(new QStandardItem(QString::number(route.totalDistance, 'f', 0)));
        row.append(new QStandardItem(QString::number(route.profitPerMinute, 'f', 1)));
        row.append(new QStandardItem(QString::number(route.score, 'f', 1)));
        for (auto *item : row)
            item->setData(index, Qt::UserRole);
        m_routeModel->appendRow(row);
    }
    m_routeView->resizeColumnsToContents();
    if (m_routeModel->rowCount() > 0)
        m_routeView->selectRow(0);
}

void TradeRoutePage::populateCommodityFilter()
{
    const QString currentValue = m_commodityFilter->currentData().toString();
    m_commodityFilter->blockSignals(true);
    m_commodityFilter->clear();
    m_commodityFilter->addItem(tr("All commodities"), QString());
    for (const auto &commodity : m_workspace.commodities)
        m_commodityFilter->addItem(commodity.displayName.isEmpty() ? commodity.nickname : commodity.displayName, commodity.nickname);
    const int currentIndex = m_commodityFilter->findData(currentValue);
    if (currentIndex >= 0)
        m_commodityFilter->setCurrentIndex(currentIndex);
    m_commodityFilter->blockSignals(false);
}

void TradeRoutePage::updateRouteDetails(int routeIndex)
{
    if (routeIndex < 0 || routeIndex >= m_routes.size()) {
        m_routeSummaryLabel->setText(tr("No route selected."));
        m_profitLabel->clear();
        m_timeLabel->clear();
        m_distanceLabel->clear();
        m_scoreLabel->clear();
        m_pathLabel->clear();
        m_warningList->clear();
        m_segmentTable->setRowCount(0);
        m_routeScene->clear();
        return;
    }

    const auto &route = m_routes.at(routeIndex);
    m_routeSummaryLabel->setText(tr("%1 from %2 to %3")
                                     .arg(route.commodityDisplayName.isEmpty() ? route.commodity : route.commodityDisplayName,
                                          route.fromBase,
                                          route.toBase));
    m_profitLabel->setText(tr("%1 / unit, %2 total").arg(route.profit).arg(route.totalProfit));
    m_timeLabel->setText(tr("%1 (%2 per minute)").arg(formatSeconds(route.travelTimeSeconds), QString::number(route.profitPerMinute, 'f', 1)));
    m_distanceLabel->setText(tr("%1 units (%2 per distance)").arg(QString::number(route.totalDistance, 'f', 0), QString::number(route.profitPerDistance, 'f', 4)));
    m_scoreLabel->setText(tr("%1 score, risk %2, plausibility delta %3")
                              .arg(QString::number(route.score, 'f', 1),
                                   QString::number(route.riskScore, 'f', 1),
                                   QString::number(route.plausibilityDelta, 'f', 1)));
    m_pathLabel->setText(route.pathSystemNames.join(QStringLiteral(" -> ")));

    m_warningList->clear();
    if (route.warnings.isEmpty())
        m_warningList->addItem(tr("No plausibility warnings."));
    else
        m_warningList->addItems(route.warnings);

    m_segmentTable->setRowCount(route.segments.size());
    for (int row = 0; row < route.segments.size(); ++row) {
        const auto &segment = route.segments.at(row);
        m_segmentTable->setItem(row, 0, new QTableWidgetItem(segment.type));
        m_segmentTable->setItem(row, 1, new QTableWidgetItem(segment.systemDisplayName));
        m_segmentTable->setItem(row, 2, new QTableWidgetItem(segment.fromLabel));
        m_segmentTable->setItem(row, 3, new QTableWidgetItem(segment.toLabel));
        m_segmentTable->setItem(row, 4, new QTableWidgetItem(QString::number(segment.seconds)));
    }
    m_segmentTable->resizeColumnsToContents();
    refreshRouteScene(route);
}

void TradeRoutePage::refreshRouteScene(const TradeRouteCandidate &route)
{
    m_routeScene->clear();
    if (route.pathSystemNames.isEmpty())
        return;

    const int spacing = 150;
    const int y = 70;
    for (int i = 0; i < route.pathSystemNames.size(); ++i) {
        const int x = 30 + (i * spacing);
        m_routeScene->addEllipse(x, y, 22, 22, QPen(Qt::black), QBrush(QColor(QStringLiteral("#68a1ff"))));
        auto *label = m_routeScene->addText(route.pathSystemNames.at(i));
        label->setPos(x - 15, y + 28);
        if (i < route.pathSystemNames.size() - 1) {
            QPainterPath path;
            path.moveTo(x + 22, y + 11);
            path.lineTo(x + spacing - 8, y + 11);
            m_routeScene->addPath(path, QPen(QColor(QStringLiteral("#1f2937")), 2));
        }
    }
    m_routeScene->setSceneRect(m_routeScene->itemsBoundingRect().adjusted(-20, -20, 20, 20));
}

void TradeRoutePage::markDirty()
{
    m_dirty = true;
}

QString TradeRoutePage::selectedCommodityNickname() const
{
    const int row = m_commodityTable->currentRow();
    if (row < 0 || row >= m_workspace.commodities.size())
        return {};
    return m_workspace.commodities.at(row).nickname;
}

int TradeRoutePage::selectedRouteIndex() const
{
    const QModelIndex index = m_routeView->currentIndex();
    if (!index.isValid())
        return -1;
    return m_routeModel->item(index.row(), 0)->data(Qt::UserRole).toInt();
}

void TradeRoutePage::addCommodity()
{
    bool ok = false;
    const QString nickname = QInputDialog::getText(this,
                                                   tr("New Commodity"),
                                                   tr("Nickname"),
                                                   QLineEdit::Normal,
                                                   QStringLiteral("commodity_new_item"),
                                                   &ok).trimmed();
    if (!ok || nickname.isEmpty())
        return;

    TradeCommodityRecord commodity;
    commodity.nickname = nickname;
    commodity.displayName = TradeRouteDataService::fallbackCommodityDisplayName(nickname);
    commodity.basePrice = 100;
    commodity.volume = 1;
    commodity.sourceFilePath = m_workspace.goodsFilePath;
    m_workspace.commodities.append(commodity);
    populateCommodityFilter();
    populateCommodityTable();
    markDirty();
    scheduleRecalculation();
}

void TradeRoutePage::removeCommodity()
{
    const int row = m_commodityTable->currentRow();
    if (row < 0 || row >= m_workspace.commodities.size())
        return;
    const QString commodityNickname = m_workspace.commodities.at(row).nickname;
    m_workspace.commodities.removeAt(row);
    m_workspace.prices.erase(std::remove_if(m_workspace.prices.begin(), m_workspace.prices.end(), [&commodityNickname](const TradePriceRecord &price) {
        return price.commodityNickname.compare(commodityNickname, Qt::CaseInsensitive) == 0;
    }), m_workspace.prices.end());
    populateCommodityFilter();
    populateCommodityTable();
    populatePriceTable();
    markDirty();
    scheduleRecalculation();
}

void TradeRoutePage::addPriceEntry()
{
    const QString commodityNickname = selectedCommodityNickname();
    if (commodityNickname.isEmpty())
        return;

    QStringList choices;
    for (const auto &base : m_workspace.bases)
        choices.append(baseDisplayLabel(base));
    bool ok = false;
    const QString chosen = QInputDialog::getItem(this, tr("Add Price Entry"), tr("Base"), choices, 0, false, &ok);
    if (!ok || chosen.isEmpty())
        return;

    const int selectedIndex = choices.indexOf(chosen);
    if (selectedIndex < 0 || selectedIndex >= m_workspace.bases.size())
        return;

    const auto &base = m_workspace.bases.at(selectedIndex);
    const int price = QInputDialog::getInt(this, tr("Add Price Entry"), tr("Actual price"), 100, 1, 100000000, 1, &ok);
    if (!ok)
        return;

    const auto commodityIt = std::find_if(m_workspace.commodities.begin(), m_workspace.commodities.end(), [commodityNickname](const TradeCommodityRecord &commodity) {
        return commodity.nickname.compare(commodityNickname, Qt::CaseInsensitive) == 0;
    });
    if (commodityIt == m_workspace.commodities.end())
        return;

    TradePriceRecord record;
    record.baseNickname = base.nickname;
    record.baseDisplayName = base.displayName;
    record.systemNickname = base.systemNickname;
    record.commodityNickname = commodityNickname;
    record.price = price;
    record.multiplier = commodityIt->basePrice > 0 ? static_cast<double>(price) / static_cast<double>(commodityIt->basePrice) : 1.0;
    record.isSource = QMessageBox::question(this,
                                            tr("Price Role"),
                                            tr("Should this base be treated as a source (seller) for the commodity?"),
                                            QMessageBox::Yes | QMessageBox::No,
                                            QMessageBox::Yes) == QMessageBox::Yes;
    record.implicit = false;
    record.sourceFilePath = m_workspace.preferredMarketFilePath;
    m_workspace.prices.append(record);
    populatePriceTable();
    markDirty();
    scheduleRecalculation();
}

void TradeRoutePage::removePriceEntry()
{
    const QString commodityNickname = selectedCommodityNickname();
    const int row = m_priceTable->currentRow();
    if (commodityNickname.isEmpty() || row < 0)
        return;

    int visibleIndex = -1;
    for (int i = 0; i < m_workspace.prices.size(); ++i) {
        if (m_workspace.prices.at(i).commodityNickname.compare(commodityNickname, Qt::CaseInsensitive) != 0)
            continue;
        ++visibleIndex;
        if (visibleIndex == row) {
            if (m_workspace.prices.at(i).implicit)
                return;
            m_workspace.prices.removeAt(i);
            populatePriceTable();
            markDirty();
            scheduleRecalculation();
            return;
        }
    }
}

} // namespace flatlas::editors
