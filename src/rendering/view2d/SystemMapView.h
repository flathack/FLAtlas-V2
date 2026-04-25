#pragma once
#include <QGraphicsView>
#include <QColor>
#include <QHash>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QVector>
#include "SystemDisplayFilter.h"

class QRubberBand;
class QTimer;

namespace flatlas::rendering {

class MapScene;
class ZoneItem2D;
class SolarObjectItem;

class SystemMapView : public QGraphicsView {
    Q_OBJECT
public:
    explicit SystemMapView(QWidget *parent = nullptr);

    MapScene *mapScene() const;
    void setMapScene(MapScene *scene);
    void setBackgroundPixmap(const QPixmap &pixmap, const QColor &fallbackColor = QColor(15, 18, 24));
    void setSystemName(const QString &name);
    void applyTheme();
    void scheduleInitialFit();
    void zoomToFit();
    void zoomToSceneRect(const QRectF &sceneRect);
    void focusSelection();
    void setDisplayFilterSettings(const SystemDisplayFilterSettings &settings);
    SystemDisplayFilterSettings displayFilterSettings() const { return m_displayFilterSettings; }

    /// Enable/disable a one-shot placement mode. While active the view shows
    /// a yellow frame + help banner and the next left click is captured to
    /// emit placementClicked() with the chosen scene position. Escape or a
    /// right click cancels and emits placementCanceled().
    void setPlacementMode(bool enabled, const QString &helpText = QString());
    bool isPlacementModeActive() const { return m_placementMode; }
    bool hasActiveMeasurement() const;
    void cancelActiveMeasurement();

signals:
    void objectSelected(const QString &nickname);
    void itemsMoveStarted(const QHash<QString, QPointF> &startScenePositions);
    void itemsMoving(const QHash<QString, QPointF> &currentScenePositions,
                     double verticalOffsetMeters);
    void itemsMoved(const QHash<QString, QPointF> &oldPositions,
                    const QHash<QString, QPointF> &newPositions,
                    double verticalOffsetMeters);
    void placementClicked(const QPointF &scenePos);
    void placementCanceled();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    enum class MeasurementStage {
        Inactive,
        AwaitingStart,
        AwaitingEnd,
    };

    struct LabelCluster {
        QVector<SolarObjectItem *> members;
        QStringList nicknames;
        QRect viewportBounds;   // union of member label rects in viewport coords
        QPoint viewportAnchor;  // representative centre used to place the popup
        bool popupEligible = false; // only set when hover popup should appear
    };

    void applyInitialFitIfNeeded();
    void updateItemDetailForScale();
    void rebuildLabelClusters();
    void updateHoveredClusterForMouse(const QPoint &viewportPos);
    void beginTrackedSelectionMove(QMouseEvent *event);
    void finishTrackedSelectionMove();
    void updateRubberBandSelection(const QRect &viewportRect, Qt::KeyboardModifiers modifiers);
    QString itemNicknameAtViewportPos(const QPoint &pos) const;
    void startMeasurementMode();
    void cancelMeasurementMode();
    void clearMeasurement(bool keepMode = false);
    void updateMeasurementCursor();
    QString measurementBannerText() const;
    bool hasMeasurementResult() const;
    double measurementDistanceWorld() const;

    MapScene *m_mapScene = nullptr;
    bool m_panning = false;
    QPoint m_lastPanPosition;
    QPixmap m_backgroundPixmap;
    QColor m_backgroundColor = QColor(15, 18, 24);
    int m_backgroundDarkenAlpha = 180;
    QColor m_overlayTextColor = QColor(230, 235, 245, 235);
    QString m_systemName;
    SystemDisplayFilterSettings m_displayFilterSettings;
    double m_minZoomScale = 0.01;
    bool m_pendingInitialFit = false;
    int m_pendingInitialFitPasses = 0;
    bool m_trackingSelectionMove = false;
    QHash<QString, QPointF> m_moveStartPositions;
    double m_moveVerticalOffsetMeters = 0.0;
    bool m_moveDidEmitStart = false;
    QRubberBand *m_rubberBand = nullptr;
    QPoint m_rubberBandOrigin;
    bool m_rubberBandSelecting = false;
    bool m_rubberBandDragged = false;
    QVector<LabelCluster> m_labelClusters;
    int m_hoveredClusterIndex = -1;
    QPoint m_lastMouseViewportPos;
    bool m_hasMouseInViewport = false;
    QTimer *m_clusterHoverHideTimer = nullptr;
    bool m_placementMode = false;
    QString m_placementHelpText;
    MeasurementStage m_measurementStage = MeasurementStage::Inactive;
    QPointF m_measurementStartScenePos;
    QPointF m_measurementEndScenePos;
    bool m_measurementHasFinal = false;
};

} // namespace flatlas::rendering
