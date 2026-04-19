#pragma once
// editors/trade/TradeRoutePage.h – Trade-Route-Analyse (Phase 11)

#include <QWidget>
#include <QVector>
#include "domain/TradeRoute.h"

class QTableWidget;
class QToolBar;
class QSpinBox;
class QLabel;
class QSplitter;

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
    void populateTable();

    QString m_dataPath;
    const flatlas::domain::UniverseData *m_universe = nullptr;
    QHash<QString, QString> m_baseToSystem;
    QVector<flatlas::domain::TradeRouteCandidate> m_routes;

    QToolBar *m_toolBar = nullptr;
    QSpinBox *m_maxResultsSpin = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_statusLabel = nullptr;
};

} // namespace flatlas::editors
