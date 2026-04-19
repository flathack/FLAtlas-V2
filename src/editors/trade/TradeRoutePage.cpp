// editors/trade/TradeRoutePage.cpp – Trade-Route-Analyse (Phase 11)

#include "TradeRoutePage.h"
#include "MarketScanner.h"
#include "TradeScoring.h"

#include <QVBoxLayout>
#include <QToolBar>
#include <QTableWidget>
#include <QHeaderView>
#include <QSpinBox>
#include <QLabel>
#include <QElapsedTimer>

using namespace flatlas::domain;

namespace flatlas::editors {

TradeRoutePage::TradeRoutePage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void TradeRoutePage::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setupToolBar();
    layout->addWidget(m_toolBar);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({
        tr("Commodity"), tr("Buy Base"), tr("Sell Base"),
        tr("Profit"), tr("Jumps")
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSortingEnabled(true);
    m_table->verticalHeader()->setVisible(false);
    layout->addWidget(m_table);

    m_statusLabel = new QLabel(tr("Ready — set data path and click Scan"), this);
    layout->addWidget(m_statusLabel);
}

void TradeRoutePage::setupToolBar()
{
    m_toolBar = new QToolBar(this);
    m_toolBar->setMovable(false);
    m_toolBar->setIconSize(QSize(16, 16));

    m_toolBar->addAction(tr("Scan && Calculate"), this,
                          &TradeRoutePage::scanAndCalculate);

    m_toolBar->addSeparator();
    m_toolBar->addWidget(new QLabel(tr(" Max routes: "), m_toolBar));

    m_maxResultsSpin = new QSpinBox(m_toolBar);
    m_maxResultsSpin->setRange(10, 1000);
    m_maxResultsSpin->setValue(50);
    m_toolBar->addWidget(m_maxResultsSpin);
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

    QElapsedTimer timer;
    timer.start();

    // Scan market files
    auto entries = MarketScanner::scanDirectory(m_dataPath);

    // Score routes
    TradeScoring scoring;
    scoring.setMarketData(entries);
    if (m_universe)
        scoring.setUniverseData(m_universe);
    if (!m_baseToSystem.isEmpty())
        scoring.setBaseSystemMap(m_baseToSystem);

    m_routes = scoring.findTopRoutes(m_maxResultsSpin->value());

    qint64 elapsed = timer.elapsed();

    populateTable();

    m_statusLabel->setText(tr("Found %1 routes from %2 market entries in %3 ms")
        .arg(m_routes.size())
        .arg(entries.size())
        .arg(elapsed));

    emit titleChanged(QStringLiteral("Trade Routes (%1)").arg(m_routes.size()));
    emit scanComplete(m_routes.size());
}

void TradeRoutePage::populateTable()
{
    m_table->setSortingEnabled(false);
    m_table->setRowCount(m_routes.size());

    for (int i = 0; i < m_routes.size(); ++i) {
        const auto &r = m_routes[i];
        m_table->setItem(i, 0, new QTableWidgetItem(r.commodity));
        m_table->setItem(i, 1, new QTableWidgetItem(r.fromBase));
        m_table->setItem(i, 2, new QTableWidgetItem(r.toBase));

        auto *profitItem = new QTableWidgetItem();
        profitItem->setData(Qt::DisplayRole, r.profit);
        m_table->setItem(i, 3, profitItem);

        auto *jumpsItem = new QTableWidgetItem();
        jumpsItem->setData(Qt::DisplayRole, r.jumps >= 0 ? r.jumps : QVariant());
        m_table->setItem(i, 4, jumpsItem);
    }

    m_table->setSortingEnabled(true);
    m_table->resizeColumnsToContents();
}

} // namespace flatlas::editors
