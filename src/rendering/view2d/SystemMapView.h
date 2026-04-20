#pragma once
#include <QGraphicsView>
#include <QColor>
#include <QHash>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include "SystemDisplayFilter.h"

class QRubberBand;

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
    void scheduleInitialFit();
    void zoomToFit();
    void setDisplayFilterSettings(const SystemDisplayFilterSettings &settings);
    SystemDisplayFilterSettings displayFilterSettings() const { return m_displayFilterSettings; }

signals:
    void objectSelected(const QString &nickname);
    void itemsMoved(const QHash<QString, QPointF> &oldPositions,
                    const QHash<QString, QPointF> &newPositions);

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

private:
    void applyInitialFitIfNeeded();
    void updateItemDetailForScale();
    void beginTrackedSelectionMove(QMouseEvent *event);
    void finishTrackedSelectionMove();
    void updateRubberBandSelection(const QRect &viewportRect, Qt::KeyboardModifiers modifiers);
    QString itemNicknameAtViewportPos(const QPoint &pos) const;

    MapScene *m_mapScene = nullptr;
    bool m_panning = false;
    QPoint m_lastPanPosition;
    QPixmap m_backgroundPixmap;
    QColor m_backgroundColor = QColor(15, 18, 24);
    int m_backgroundDarkenAlpha = 180;
    QString m_systemName;
    SystemDisplayFilterSettings m_displayFilterSettings;
    double m_minZoomScale = 0.01;
    bool m_pendingInitialFit = false;
    int m_pendingInitialFitPasses = 0;
    bool m_trackingSelectionMove = false;
    QHash<QString, QPointF> m_moveStartPositions;
    QRubberBand *m_rubberBand = nullptr;
    QPoint m_rubberBandOrigin;
    bool m_rubberBandSelecting = false;
    bool m_rubberBandDragged = false;
};

} // namespace flatlas::rendering
