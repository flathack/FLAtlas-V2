#pragma once
// editors/trade/TradeRoutePage.h – Trade-Route-Workspace

#include <QFutureWatcher>
#include <QHash>
#include <QTimer>
#include <QWidget>
#include <QVector>

#include "domain/TradeRoute.h"
#include "TradeRouteDataService.h"

class QGraphicsScene;
class QGraphicsView;
class QLineEdit;
class QListWidget;
class QStandardItemModel;
class QTableView;
class QTableWidget;
class QComboBox;
class QToolBar;
class QSpinBox;
class QLabel;
class QSplitter;
class QCheckBox;
class QPushButton;

namespace flatlas::domain { class UniverseData; }

namespace flatlas::editors {

class TradeRoutePage : public QWidget {
    Q_OBJECT
public:
    explicit TradeRoutePage(QWidget *parent = nullptr);

    /// Set the data directory to scan for market INIs.
    void setDataPath(const QString &dataPath);

    /// Set universe data for jump distance calculation.
    void setUniverseData(const flatlas::domain::UniverseData *universe);

    /// Set base-to-system mapping.
    void setBaseSystemMap(const QHash<QString, QString> &map);

    /// Trigger a full scan and route calculation.
    void scanAndCalculate();

    /// Get the computed routes.
    const QVector<flatlas::domain::TradeRouteCandidate> &routes() const { return m_routes; }

signals:
    void titleChanged(const QString &title);
    void scanComplete(int routeCount);

private:
    void setupUi();
    void setupToolBar();
    void reloadWorkspace();
    void saveWorkspace();
    void scheduleRecalculation();
    void startRecalculation();
    void populateCommodityTable();
    void populatePriceTable();
    void populateRouteTable();
    void populateCommodityFilter();
    void updateRouteDetails(int routeIndex);
    void refreshRouteScene(const flatlas::domain::TradeRouteCandidate &route);
    void markDirty();
    QString selectedCommodityNickname() const;
    int selectedRouteIndex() const;
    void addCommodity();
    void removeCommodity();
    void addPriceEntry();
    void removePriceEntry();

    QString m_dataPath;
    const flatlas::domain::UniverseData *m_universe = nullptr;
    QHash<QString, QString> m_baseToSystem;
    QVector<flatlas::domain::TradeRouteCandidate> m_routes;
    TradeRouteWorkspaceData m_workspace;
    bool m_dirty = false;
    bool m_updatingTables = false;

    QToolBar *m_toolBar = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_commodityFilter = nullptr;
    QSpinBox *m_cargoSpin = nullptr;
    QSpinBox *m_maxJumpsSpin = nullptr;
    QSpinBox *m_maxResultsSpin = nullptr;
    QCheckBox *m_localOnlyCheck = nullptr;
    QTableWidget *m_commodityTable = nullptr;
    QTableWidget *m_priceTable = nullptr;
    QTableWidget *m_segmentTable = nullptr;
    QTableView *m_routeView = nullptr;
    QStandardItemModel *m_routeModel = nullptr;
    QListWidget *m_warningList = nullptr;
    QLabel *m_routeSummaryLabel = nullptr;
    QLabel *m_profitLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_distanceLabel = nullptr;
    QLabel *m_scoreLabel = nullptr;
    QLabel *m_pathLabel = nullptr;
    QGraphicsView *m_routeViewWidget = nullptr;
    QGraphicsScene *m_routeScene = nullptr;
    QLabel *m_statusLabel = nullptr;
    QFutureWatcher<TradeRouteWorkspaceData> *m_loadWatcher = nullptr;
    QFutureWatcher<QVector<flatlas::domain::TradeRouteCandidate>> *m_calcWatcher = nullptr;
    QTimer *m_recalcTimer = nullptr;
};

} // namespace flatlas::editors
