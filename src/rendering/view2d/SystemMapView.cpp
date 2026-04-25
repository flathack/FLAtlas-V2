#include "SystemMapView.h"
#include "MapScene.h"
#include "core/Theme.h"
#include "items/LightSourceItem.h"
#include "items/SolarObjectItem.h"
#include "items/ZoneItem2D.h"
#include "domain/SolarObject.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QLineF>
#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QApplication>
#include <QPainterPath>
#include <QRubberBand>
#include <QScrollBar>
#include <QTimer>
#include <QPalette>
#include <QKeyEvent>

#include <functional>
#include <limits>

namespace flatlas::rendering {

namespace {

// Matches the reference Universe-Viewer (flathack.github.io/docs/universe-viewer.html):
// an 880 px 8x8 grid is rendered inside a 1000 px canvas, i.e. 60 px (= 6 %)
// breathing space on every side. Without this padding, objects that legitimately
// sit at the edge of the NavMap (e.g. Jump Gates around ±80 000 FL units in Li01)
// end up glued to the viewport edge. The NavMapScale math itself is already
// correct — only the fitted target rect has to be expanded by the same ratio.
constexpr double kNavMapFitPaddingFactor = 1000.0 / 880.0; // ≈ 1.1364

QRectF withNavMapPadding(const QRectF &gridRect)
{
    if (gridRect.isNull() || gridRect.width() <= 0.0 || gridRect.height() <= 0.0)
        return gridRect;
    const QPointF c = gridRect.center();
    const qreal w = gridRect.width()  * kNavMapFitPaddingFactor;
    const qreal h = gridRect.height() * kNavMapFitPaddingFactor;
    return QRectF(c.x() - w / 2.0, c.y() - h / 2.0, w, h);
}

double fitScaleForView(const QGraphicsView *view, const QRectF &targetRect)
{
    if (!view)
        return 0.0;

    const QRect viewportRect = view->viewport()->rect().adjusted(2, 2, -2, -2);
    if (viewportRect.width() <= 0 || viewportRect.height() <= 0)
        return 0.0;
    if (targetRect.width() <= 0.0 || targetRect.height() <= 0.0)
        return 0.0;

    const double scaleX = static_cast<double>(viewportRect.width()) / targetRect.width();
    const double scaleY = static_cast<double>(viewportRect.height()) / targetRect.height();
    return std::max(0.0001, std::min(scaleX, scaleY));
}

QString itemNickname(QGraphicsItem *item)
{
    if (auto *solarItem = dynamic_cast<SolarObjectItem *>(item))
        return solarItem->nickname();
    if (auto *zoneItem = dynamic_cast<ZoneItem2D *>(item))
        return zoneItem->nickname();
    if (auto *lightItem = dynamic_cast<LightSourceItem *>(item))
        return lightItem->nickname();
    return {};
}

QString formatWorldDistance(double distance)
{
    const double safeDistance = std::max(0.0, distance);
    if (safeDistance >= 1000.0)
        return QStringLiteral("%1 km").arg(safeDistance / 1000.0, 0, 'f', 2);
    return QStringLiteral("%1 m").arg(safeDistance, 0, 'f', 0);
}

double worldDistanceForScenePoints(const QPointF &startScenePos, const QPointF &endScenePos)
{
    const QPointF startWorld = MapScene::qtToFl(startScenePos.x(), startScenePos.y());
    const QPointF endWorld = MapScene::qtToFl(endScenePos.x(), endScenePos.y());
    return QLineF(startWorld, endWorld).length();
}

}

SystemMapView::SystemMapView(QWidget *parent)
    : QGraphicsView(parent)
{
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform, false);
    setDragMode(QGraphicsView::NoDrag);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // NOTE: Do NOT enable QGraphicsView::CacheBackground here. drawBackground()
    // resets the painter transform so the wallpaper is drawn in viewport
    // coordinates; a cached background is stored in *scene* coordinates and
    // therefore gets translated together with the scroll position while the
    // user pans the view with the middle mouse button. The cache is then
    // invalidated on release, which is what produced the "background follows
    // the map during drag, snaps back on release" bug. The UniverseMapView
    // also intentionally omits this flag for the same reason.
    setOptimizationFlag(QGraphicsView::DontSavePainterState, true);
    setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
    // We need mouse-move events without a pressed button so the overlap-cluster
    // popup can appear on pure hover.
    setMouseTracking(true);
    if (viewport())
        viewport()->setMouseTracking(true);
    m_clusterHoverHideTimer = new QTimer(this);
    m_clusterHoverHideTimer->setSingleShot(true);
    m_clusterHoverHideTimer->setInterval(140);
    connect(m_clusterHoverHideTimer, &QTimer::timeout, this, [this]() {
        if (m_hoveredClusterIndex < 0)
            return;
        m_hoveredClusterIndex = -1;
        viewport()->update();
    });
    applyTheme();
}

MapScene *SystemMapView::mapScene() const
{
    return m_mapScene;
}

void SystemMapView::setMapScene(MapScene *scene)
{
    m_mapScene = scene;
    QGraphicsView::setScene(scene);
    m_pendingInitialFit = true;
    m_pendingInitialFitPasses = 3;
}

void SystemMapView::scheduleInitialFit()
{
    m_pendingInitialFit = true;
    m_pendingInitialFitPasses = 3;
    QTimer::singleShot(0, this, [this]() { applyInitialFitIfNeeded(); });
    QTimer::singleShot(25, this, [this]() { applyInitialFitIfNeeded(); });
    QTimer::singleShot(75, this, [this]() { applyInitialFitIfNeeded(); });
}

void SystemMapView::setBackgroundPixmap(const QPixmap &pixmap, const QColor &fallbackColor)
{
    m_backgroundPixmap = pixmap;
    m_backgroundColor = fallbackColor;
    m_backgroundDarkenAlpha = m_backgroundColor.lightness() >= 130 ? 0 : 180;
    viewport()->update();
}

void SystemMapView::applyTheme()
{
    const QPalette pal = palette();
    const QColor base = pal.color(QPalette::Base);
    const QColor text = pal.color(QPalette::Text);
    m_overlayTextColor = text;
    m_backgroundPixmap = QPixmap(flatlas::core::Theme::instance().wallpaperResourcePath());
    // A slightly stronger darkening overlay makes the wallpaper recede
    // ("durchsichtiger" in perceived contrast) so zones and solar objects
    // pop a bit more against the background.
    if (base.lightness() >= 170) {
        m_backgroundColor = QColor(236, 241, 247);
        m_backgroundDarkenAlpha = !m_backgroundPixmap.isNull() ? 150 : 0;
    } else {
        m_backgroundColor = QColor(15, 18, 24);
        m_backgroundDarkenAlpha = !m_backgroundPixmap.isNull() ? 215 : 0;
    }
    viewport()->update();
}

void SystemMapView::setSystemName(const QString &name)
{
    m_systemName = name;
    viewport()->update();
}

void SystemMapView::setDisplayFilterSettings(const SystemDisplayFilterSettings &settings)
{
    m_displayFilterSettings = settings;
    updateItemDetailForScale();
    viewport()->update();
}

void SystemMapView::zoomToFit()
{
    if (!scene())
        return;
    QRectF targetRect = scene()->sceneRect();
    if (targetRect.isNull() || targetRect.width() <= 0.0 || targetRect.height() <= 0.0)
        targetRect = scene()->itemsBoundingRect();
    if (targetRect.isNull() || targetRect.width() <= 0.0 || targetRect.height() <= 0.0)
        return;

    // Fit the NavMap grid with the same breathing space as the reference
    // universe-viewer so objects at the NavMap edge do not cling to the
    // viewport border.
    const QRectF paddedRect = withNavMapPadding(targetRect);

    const auto previousAnchor = transformationAnchor();
    const auto previousResizeAnchor = resizeAnchor();
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);

    resetTransform();
    fitInView(paddedRect, Qt::KeepAspectRatio);
    centerOn(paddedRect.center());

    setTransformationAnchor(previousAnchor);
    setResizeAnchor(previousResizeAnchor);
    m_minZoomScale = fitScaleForView(this, paddedRect);
    updateItemDetailForScale();
    if (--m_pendingInitialFitPasses <= 0) {
        m_pendingInitialFit = false;
        m_pendingInitialFitPasses = 0;
    }
}

void SystemMapView::zoomToSceneRect(const QRectF &sceneRect)
{
    if (!scene())
        return;

    QRectF targetRect = sceneRect;
    if (targetRect.isNull() || targetRect.width() <= 0.0 || targetRect.height() <= 0.0)
        targetRect = scene()->itemsBoundingRect();
    if (targetRect.isNull() || targetRect.width() <= 0.0 || targetRect.height() <= 0.0)
        return;

    const QRectF paddedRect = withNavMapPadding(targetRect);
    const auto previousAnchor = transformationAnchor();
    const auto previousResizeAnchor = resizeAnchor();
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);

    resetTransform();
    fitInView(paddedRect, Qt::KeepAspectRatio);
    centerOn(paddedRect.center());

    setTransformationAnchor(previousAnchor);
    setResizeAnchor(previousResizeAnchor);
    m_minZoomScale = fitScaleForView(this, paddedRect);
    updateItemDetailForScale();
}

void SystemMapView::setMoveGroupResolver(std::function<QStringList(const QString &)> resolver)
{
    m_moveGroupResolver = std::move(resolver);
}

void SystemMapView::focusSelection()
{
    if (!m_mapScene) {
        zoomToFit();
        return;
    }

    const auto sceneItems = m_mapScene->selectedItems();
    if (sceneItems.isEmpty()) {
        zoomToFit();
        return;
    }

    QRectF selectionRect;
    bool hasRect = false;
    for (QGraphicsItem *item : sceneItems) {
        if (!item)
            continue;
        const QRectF itemRect = item->sceneBoundingRect();
        if (!hasRect) {
            selectionRect = itemRect;
            hasRect = true;
        } else {
            selectionRect = selectionRect.united(itemRect);
        }
    }

    if (!hasRect) {
        zoomToFit();
        return;
    }
    zoomToSceneRect(selectionRect);
}

bool SystemMapView::hasActiveMeasurement() const
{
    return m_measurementStage != MeasurementStage::Inactive || m_measurementHasFinal;
}

void SystemMapView::startMeasurement()
{
    startMeasurementMode();
}

void SystemMapView::clearMeasurementResults()
{
    clearMeasurement();
}

void SystemMapView::cancelActiveMeasurement()
{
    if (hasActiveMeasurement())
        cancelMeasurementMode();
}

void SystemMapView::wheelEvent(QWheelEvent *event)
{
    // Ctrl + wheel while a tracked selection move is active adjusts the
    // vertical (Y / height) offset in 10 m steps. The active X/Z drag
    // continues as normal; only the Y offset is accumulated and forwarded
    // via the itemsMoving / itemsMoved signals.
    if (m_trackingSelectionMove && (event->modifiers() & Qt::ControlModifier)) {
        const int notches = event->angleDelta().y() / 120;
        if (notches != 0) {
            constexpr double kStepMeters = 10.0;
            m_moveVerticalOffsetMeters += static_cast<double>(notches) * kStepMeters;

            if (m_mapScene) {
                QHash<QString, QPointF> currentScenePositions;
                const auto sceneItems = m_mapScene->selectedItems();
                for (QGraphicsItem *item : sceneItems) {
                    const QString nickname = itemNickname(item);
                    if (nickname.isEmpty() || !m_moveStartPositions.contains(nickname))
                        continue;
                    currentScenePositions.insert(nickname, item->scenePos());
                }
                if (!currentScenePositions.isEmpty()) {
                    if (!m_moveDidEmitStart) {
                        m_moveDidEmitStart = true;
                        emit itemsMoveStarted(m_moveStartPositions);
                    }
                    emit itemsMoving(currentScenePositions, m_moveVerticalOffsetMeters);
                }
            }
            viewport()->update();
        }
        event->accept();
        return;
    }

    constexpr double factor = 1.15;
    const double currentScale = transform().m11();

    if (event->angleDelta().y() > 0) {
        if (currentScale < 100.0)
            scale(factor, factor);
    } else {
        if (currentScale > m_minZoomScale)
            scale(1.0 / factor, 1.0 / factor);
        if (transform().m11() < m_minZoomScale) {
            resetTransform();
            scale(m_minZoomScale, m_minZoomScale);
        }
    }
    updateItemDetailForScale();
    event->accept();
}

void SystemMapView::mousePressEvent(QMouseEvent *event)
{
    if (m_measurementStage != MeasurementStage::Inactive) {
        if (event->button() == Qt::LeftButton) {
            const QPointF scenePos = mapToScene(event->pos());
            if (m_measurementStage == MeasurementStage::AwaitingStart) {
                m_measurementStartScenePos = scenePos;
                m_measurementEndScenePos = scenePos;
                m_measurementHasFinal = false;
                m_measurementStage = MeasurementStage::AwaitingEnd;
            } else {
                m_measurementEndScenePos = scenePos;
                m_measurementHasFinal = true;
                m_measurementStage = MeasurementStage::Inactive;
            }
            updateMeasurementCursor();
            viewport()->update();
            event->accept();
            return;
        }
        if (event->button() == Qt::MiddleButton) {
            cancelMeasurementMode();
            event->accept();
            return;
        }
    }

    if (m_placementMode) {
        if (event->button() == Qt::LeftButton) {
            const QPointF scenePos = mapToScene(event->pos());
            // Emit first, then clear the mode. This way the callback can
            // re-enter placement mode (e.g. chained placements) without
            // fighting the cleanup below.
            const QString /*unused*/ previousHelp = m_placementHelpText;
            m_placementMode = false;
            m_placementHelpText.clear();
            setCursor(Qt::ArrowCursor);
            viewport()->update();
            emit placementClicked(scenePos);
            event->accept();
            return;
        }
        if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
            m_placementMode = false;
            m_placementHelpText.clear();
            setCursor(Qt::ArrowCursor);
            viewport()->update();
            emit placementCanceled();
            event->accept();
            return;
        }
    }
    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_lastPanPosition = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && m_mapScene && !m_mapScene->isMoveEnabled()) {
        if (!m_rubberBand)
            m_rubberBand = new QRubberBand(QRubberBand::Rectangle, viewport());
        m_rubberBandOrigin = event->pos();
        m_rubberBandSelecting = true;
        m_rubberBandDragged = false;
        m_rubberBand->setGeometry(QRect(m_rubberBandOrigin, QSize()));
        m_rubberBand->hide();
        event->accept();
        return;
    }
    beginTrackedSelectionMove(event);
    QGraphicsView::mousePressEvent(event);
    // A left click on a currently unselected item only becomes "selected"
    // during the base class mousePressEvent above. Our first
    // beginTrackedSelectionMove therefore saw no selection and skipped
    // tracking. Retry so the ghost overlay and distance hint appear from
    // the very first drag pixel instead of only on the second click.
    if (!m_trackingSelectionMove
        && event->button() == Qt::LeftButton
        && m_mapScene
        && m_mapScene->isMoveEnabled()) {
        beginTrackedSelectionMove(event);
    }
}

void SystemMapView::mouseMoveEvent(QMouseEvent *event)
{
    m_lastMouseViewportPos = event->pos();
    m_hasMouseInViewport = true;
    if (m_measurementStage != MeasurementStage::Inactive) {
        if (m_measurementStage == MeasurementStage::AwaitingEnd)
            m_measurementEndScenePos = mapToScene(event->pos());
        viewport()->update();
        event->accept();
        return;
    }
    if (m_panning) {
        const QPoint delta = event->pos() - m_lastPanPosition;
        m_lastPanPosition = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    if (m_rubberBandSelecting) {
        const QRect selectionRect = QRect(m_rubberBandOrigin, event->pos()).normalized();
        const bool dragged = (event->pos() - m_rubberBandOrigin).manhattanLength() >= QApplication::startDragDistance();
        m_rubberBandDragged = dragged;
        if (dragged) {
            m_rubberBand->setGeometry(selectionRect);
            m_rubberBand->show();
        }
        event->accept();
        return;
    }
    updateHoveredClusterForMouse(event->pos());
    QGraphicsView::mouseMoveEvent(event);

    if (m_trackingSelectionMove && m_mapScene) {
        // Emit the live "moving" signal once per mouse move. The consumer
        // uses this to live-update the object editor and we redraw the
        // ghost overlay drawn in drawForeground().
        QHash<QString, QPointF> currentScenePositions;
        const auto sceneItems = m_mapScene->selectedItems();
        for (QGraphicsItem *item : sceneItems) {
            const QString nickname = itemNickname(item);
            if (nickname.isEmpty() || !m_moveStartPositions.contains(nickname))
                continue;
            currentScenePositions.insert(nickname, item->scenePos());
        }
        if (!currentScenePositions.isEmpty()) {
            if (!m_moveDidEmitStart) {
                m_moveDidEmitStart = true;
                emit itemsMoveStarted(m_moveStartPositions);
            }
            emit itemsMoving(currentScenePositions, m_moveVerticalOffsetMeters);
            viewport()->update();
        }
    }
}

void SystemMapView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_measurementStage != MeasurementStage::Inactive) {
        if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) {
            event->accept();
            return;
        }
    }
    if (m_panning && event->button() == Qt::MiddleButton) {
        m_panning = false;
        unsetCursor();
        event->accept();
        return;
    }
    if (m_rubberBandSelecting && event->button() == Qt::LeftButton) {
        const QRect selectionRect = QRect(m_rubberBandOrigin, event->pos()).normalized();
        if (m_rubberBand)
            m_rubberBand->hide();

        if (m_rubberBandDragged) {
            updateRubberBandSelection(selectionRect, event->modifiers());
        } else if (m_mapScene) {
            const QString nickname = itemNicknameAtViewportPos(event->pos());
            QStringList selection;
            if (!nickname.isEmpty()) {
                if (event->modifiers() & Qt::ControlModifier) {
                    selection = m_mapScene->selectedNicknames();
                    if (selection.contains(nickname))
                        selection.removeAll(nickname);
                    else
                        selection.append(nickname);
                } else {
                    selection = {nickname};
                }
            }
            m_mapScene->selectNicknames(selection);
        }

        m_rubberBandSelecting = false;
        m_rubberBandDragged = false;
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
    if (event->button() == Qt::LeftButton)
        finishTrackedSelectionMove();
}

void SystemMapView::contextMenuEvent(QContextMenuEvent *event)
{
    emit contextMenuRequested(event->globalPos(),
                              mapToScene(event->pos()),
                              zoneNicknamesAtViewportPos(event->pos()));
    event->accept();
}

void SystemMapView::drawBackground(QPainter *painter, const QRectF &rect)
{
    painter->save();
    painter->resetTransform();

    const QRect viewportRect = viewport()->rect();
    painter->fillRect(viewportRect, m_backgroundColor);
    if (!m_backgroundPixmap.isNull()) {
        const QPixmap scaled = m_backgroundPixmap.scaled(viewportRect.size(),
                                                         Qt::KeepAspectRatioByExpanding,
                                                         Qt::SmoothTransformation);
        const QPoint topLeft((viewportRect.width() - scaled.width()) / 2,
                             (viewportRect.height() - scaled.height()) / 2);
        painter->drawPixmap(topLeft, scaled);
        if (m_backgroundDarkenAlpha > 0)
            painter->fillRect(viewportRect, QColor(0, 0, 0, m_backgroundDarkenAlpha));
    }

    painter->restore();
    QGraphicsView::drawBackground(painter, rect);
}

void SystemMapView::drawForeground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawForeground(painter, rect);

    if (!scene())
        return;

    const QRectF sceneRect = scene()->sceneRect();
    if (sceneRect.isNull() || sceneRect.width() <= 0.0 || sceneRect.height() <= 0.0)
        return;

    painter->save();
    painter->resetTransform();

    QFont font(QStringLiteral("Segoe UI"), 11, QFont::Bold);
    painter->setFont(font);
    painter->setPen(m_overlayTextColor);
    const QFontMetrics fm(font);

    const double cellWidth = sceneRect.width() / 8.0;
    const double cellHeight = sceneRect.height() / 8.0;

    // Anchor the A-H / 1-8 labels to the actual grid edges (converted through
    // the current view transform) instead of the viewport edges. This matches
    // the reference universe-viewer where labels sit INSIDE the padding zone
    // around the grid; anchoring to the viewport edge visually compresses the
    // padding and makes objects at the grid edge appear closer to the border
    // than in the reference.
    const QPoint gridBottomLeftView = mapFromScene(QPointF(sceneRect.left(), sceneRect.bottom()));
    const QPoint gridTopLeftView    = mapFromScene(QPointF(sceneRect.left(), sceneRect.top()));
    const int columnLabelY = gridBottomLeftView.y() + fm.ascent() + 6; // just below grid
    const int rowLabelX    = gridTopLeftView.x() - 10;                 // just left of grid

    for (int i = 0; i < 8; ++i) {
        const double sceneX = sceneRect.left() + (cellWidth * (static_cast<double>(i) + 0.5));
        const QPoint viewPoint = mapFromScene(QPointF(sceneX, sceneRect.bottom()));
        const QString label(QChar(static_cast<char>('A' + i)));
        painter->drawText(viewPoint.x() - (fm.horizontalAdvance(label) / 2),
                          columnLabelY,
                          label);
    }

    for (int i = 0; i < 8; ++i) {
        const double sceneY = sceneRect.top() + (cellHeight * (static_cast<double>(i) + 0.5));
        const QPoint viewPoint = mapFromScene(QPointF(sceneRect.left(), sceneY));
        const QString label = QString::number(i + 1);
        painter->drawText(rowLabelX - fm.horizontalAdvance(label),
                          viewPoint.y() + (fm.height() / 3),
                          label);
    }

    if (!m_systemName.trimmed().isEmpty()) {
        QFont titleFont(QStringLiteral("Segoe UI"), 16, QFont::Bold);
        painter->setFont(titleFont);
        const QFontMetrics titleMetrics(titleFont);
        const QString title = m_systemName.trimmed().toUpper();
        painter->drawText((viewport()->width() - titleMetrics.horizontalAdvance(title)) / 2,
                          viewport()->height() - 54,
                          title);
        painter->setFont(font);
    }

    constexpr double kSceneScale = 0.01;
    const double sx = std::abs(transform().m11());
    const double sy = std::abs(transform().m22());
    if (sx > 1e-9 && sy > 1e-9) {
        const int margin = 16;
        const int barWidthPx = 140;
        const int barHeightPx = 100;
        const double worldW = barWidthPx / (sx * kSceneScale);
        const double worldH = barHeightPx / (sy * kSceneScale);

        const int x0 = margin + 16;
        const int y0 = viewport()->height() - 16;
        painter->drawLine(x0, y0, x0 + barWidthPx, y0);
        painter->drawLine(x0, y0 - 4, x0, y0 + 4);
        painter->drawLine(x0 + barWidthPx, y0 - 4, x0 + barWidthPx, y0 + 4);
        painter->drawText(x0, y0 - 8,
                          QStringLiteral("%1 km").arg(worldW / 1000.0, 0, 'f', 2));

        const int vx = margin;
        const int vy0 = margin;
        painter->drawLine(vx, vy0, vx, vy0 + barHeightPx);
        painter->drawLine(vx - 4, vy0, vx + 4, vy0);
        painter->drawLine(vx - 4, vy0 + barHeightPx, vx + 4, vy0 + barHeightPx);
        painter->drawText(vx + 8, vy0 + barHeightPx,
                          QStringLiteral("%1 km").arg(worldH / 1000.0, 0, 'f', 2));
    }

    if (m_hoveredClusterIndex >= 0 && m_hoveredClusterIndex < m_labelClusters.size()) {
        const LabelCluster &cluster = m_labelClusters[m_hoveredClusterIndex];
        const QFont popupFont(QStringLiteral("Segoe UI"), 8);
        const QFontMetrics popupMetrics(popupFont);
        painter->setFont(popupFont);

        // Cap the popup at a readable number of entries. If the cluster is
        // bigger, show the first N and a trailing "... (+N more)" line so the
        // popup stays compact and the viewport is not flooded.
        constexpr int kMaxPopupEntries = 8;
        const int totalEntries = static_cast<int>(cluster.nicknames.size());
        const bool truncated = totalEntries > kMaxPopupEntries;
        const int visibleEntries = truncated ? kMaxPopupEntries : totalEntries;
        QStringList shownLines;
        shownLines.reserve(visibleEntries + (truncated ? 1 : 0));
        for (int i = 0; i < visibleEntries; ++i)
            shownLines.append(cluster.nicknames.at(i));
        if (truncated)
            shownLines.append(QStringLiteral("... (+%1 more)").arg(totalEntries - visibleEntries));

        const int lineHeight = popupMetrics.height();
        const int padding = 6;
        const int spacing = 2;
        int widestLine = 0;
        for (const QString &name : shownLines)
            widestLine = std::max(widestLine, popupMetrics.horizontalAdvance(name));

        const int popupWidth = widestLine + padding * 2;
        const int popupHeight = padding * 2
            + lineHeight * static_cast<int>(shownLines.size())
            + spacing * std::max<int>(0, static_cast<int>(shownLines.size()) - 1);

        // Prefer placing the stack just above/right of the cluster so it does
        // not hide the objects themselves; clamp into the viewport.
        QPoint origin(cluster.viewportAnchor.x() + 12,
                      cluster.viewportAnchor.y() - popupHeight - 8);
        const QRect vp = viewport()->rect();
        if (origin.x() + popupWidth + 4 > vp.right())
            origin.setX(cluster.viewportAnchor.x() - popupWidth - 12);
        if (origin.x() < vp.left() + 4)
            origin.setX(vp.left() + 4);
        if (origin.y() < vp.top() + 4)
            origin.setY(cluster.viewportAnchor.y() + 12);
        if (origin.y() + popupHeight + 4 > vp.bottom())
            origin.setY(std::max(vp.top() + 4, vp.bottom() - popupHeight - 4));

        const QRect popupRect(origin, QSize(popupWidth, popupHeight));
        QColor bg(18, 22, 30, 225);
        QColor border(m_overlayTextColor);
        border.setAlpha(200);
        painter->setPen(Qt::NoPen);
        painter->setBrush(bg);
        painter->drawRoundedRect(popupRect, 4, 4);
        painter->setPen(QPen(border, 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(popupRect, 4, 4);

        // Leader line from the popup to the cluster anchor so the reader can
        // still see which objects the stacked labels belong to.
        QColor leader(m_overlayTextColor);
        leader.setAlpha(160);
        painter->setPen(QPen(leader, 1, Qt::DotLine));
        painter->drawLine(cluster.viewportAnchor, popupRect.center());

        painter->setPen(QColor(235, 240, 250, 240));
        int y = popupRect.top() + padding + popupMetrics.ascent();
        for (const QString &name : shownLines) {
            painter->drawText(popupRect.left() + padding, y, name);
            y += lineHeight + spacing;
        }
    }

    if (m_trackingSelectionMove && m_mapScene && !m_moveStartPositions.isEmpty()) {
        // Live "Move Object" overlay: ghost of the original position, a
        // connector line from old -> current scenePos per item, and a
        // distance label at the drag anchor.
        constexpr double kSceneScale = 0.01; // 1 FL meter = 0.01 scene units

        QHash<QString, QPointF> currentScenePositions;
        const auto sceneItems = m_mapScene->selectedItems();
        for (QGraphicsItem *item : sceneItems) {
            const QString nickname = itemNickname(item);
            if (nickname.isEmpty() || !m_moveStartPositions.contains(nickname))
                continue;
            currentScenePositions.insert(nickname, item->scenePos());
        }

        const QColor ghostColor(255, 200, 0, 220);
        const QColor ghostFill(255, 200, 0, 60);
        QPen ghostPen(ghostColor);
        ghostPen.setWidth(2);

        QPen linePen(ghostColor);
        linePen.setWidth(2);
        linePen.setStyle(Qt::DashLine);

        QPointF anchorNew;
        double anchorDistanceMeters = 0.0;
        bool hasAnchor = false;

        for (auto it = m_moveStartPositions.constBegin(); it != m_moveStartPositions.constEnd(); ++it) {
            if (!currentScenePositions.contains(it.key()))
                continue;
            const QPointF oldScene = it.value();
            const QPointF newScene = currentScenePositions.value(it.key());
            const QPointF oldView = mapFromScene(oldScene);
            const QPointF newView = mapFromScene(newScene);

            painter->setPen(ghostPen);
            painter->setBrush(ghostFill);
            constexpr int kGhostRadius = 10;
            painter->drawEllipse(oldView, kGhostRadius, kGhostRadius);
            painter->setBrush(Qt::NoBrush);
            painter->drawLine(QPointF(oldView.x() - kGhostRadius - 3, oldView.y()),
                              QPointF(oldView.x() + kGhostRadius + 3, oldView.y()));
            painter->drawLine(QPointF(oldView.x(), oldView.y() - kGhostRadius - 3),
                              QPointF(oldView.x(), oldView.y() + kGhostRadius + 3));

            painter->setPen(linePen);
            painter->drawLine(oldView, newView);

            const double distanceMeters = QLineF(oldScene, newScene).length() / kSceneScale;
            if (!hasAnchor || distanceMeters > anchorDistanceMeters) {
                anchorNew = newView;
                anchorDistanceMeters = distanceMeters;
                hasAnchor = true;
            }
        }

        if (hasAnchor) {
            const bool hasYOffset = std::abs(m_moveVerticalOffsetMeters) > 1e-6;
            QStringList lines;
            const QString distText = anchorDistanceMeters >= 1000.0
                ? QStringLiteral("%1 km").arg(anchorDistanceMeters / 1000.0, 0, 'f', 2)
                : QStringLiteral("%1 m").arg(anchorDistanceMeters, 0, 'f', 0);
            lines << tr("Distance: %1").arg(distText);
            if (hasYOffset) {
                const double absY = std::abs(m_moveVerticalOffsetMeters);
                const QString yText = absY >= 1000.0
                    ? QStringLiteral("%1 km").arg(absY / 1000.0, 0, 'f', 2)
                    : QStringLiteral("%1 m").arg(absY, 0, 'f', 0);
                lines << tr("Δy: %1%2")
                                .arg(m_moveVerticalOffsetMeters >= 0 ? QStringLiteral("+") : QStringLiteral("-"))
                                .arg(yText);
            }
            lines << tr("Ctrl+Wheel: ±10 m Y");

            QFont labelFont(QStringLiteral("Segoe UI"), 10, QFont::Bold);
            painter->setFont(labelFont);
            const QFontMetrics lm(labelFont);
            int textWidth = 0;
            for (const QString &l : lines)
                textWidth = std::max(textWidth, lm.horizontalAdvance(l));
            const int pad = 8;
            const int lineH = lm.height();
            const int boxW = textWidth + pad * 2;
            const int boxH = lineH * lines.size() + pad * 2;

            QPoint pos(int(anchorNew.x()) + 16, int(anchorNew.y()) - boxH - 12);
            const QRect vp = viewport()->rect();
            if (pos.x() + boxW > vp.right() - 4)
                pos.setX(vp.right() - boxW - 4);
            if (pos.x() < vp.left() + 4)
                pos.setX(vp.left() + 4);
            if (pos.y() < vp.top() + 4)
                pos.setY(int(anchorNew.y()) + 16);

            const QRect boxRect(pos, QSize(boxW, boxH));
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(20, 22, 30, 215));
            painter->drawRoundedRect(boxRect, 6, 6);
            QPen borderPen(ghostColor);
            borderPen.setWidth(1);
            painter->setPen(borderPen);
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(boxRect, 6, 6);

            painter->setPen(QColor(235, 240, 250, 240));
            int textY = boxRect.top() + pad + lm.ascent();
            for (const QString &l : lines) {
                painter->drawText(boxRect.left() + pad, textY, l);
                textY += lineH;
            }
            painter->setFont(font);
        }
    }

    if (m_measurementStage != MeasurementStage::Inactive || m_measurementHasFinal) {
        const bool previewActive = m_measurementStage == MeasurementStage::AwaitingEnd;
        const bool showStartPoint = previewActive || m_measurementHasFinal;
        const bool showEndPoint = previewActive || m_measurementHasFinal;
        if (showStartPoint && showEndPoint) {
            const QPointF startView = mapFromScene(m_measurementStartScenePos);
            const QPointF endView = mapFromScene(m_measurementEndScenePos);
            const QColor accent(60, 210, 255, 240);
            const QColor accentFill(60, 210, 255, previewActive ? 72 : 110);
            QPen linePen(accent);
            linePen.setWidth(2);
            linePen.setStyle(previewActive ? Qt::DashLine : Qt::SolidLine);
            painter->setPen(linePen);
            painter->drawLine(startView, endView);

            painter->setPen(QPen(accent, 2));
            painter->setBrush(accentFill);
            painter->drawEllipse(startView, 6, 6);
            painter->setBrush(previewActive ? QColor(255, 255, 255, 30) : accentFill);
            painter->drawEllipse(endView, 6, 6);

            const double distanceWorld = measurementDistanceWorld();
            const QString distanceText = tr("Distance: %1").arg(formatWorldDistance(distanceWorld));
            const QPointF midpoint = (startView + endView) / 2.0;
            QFont labelFont(QStringLiteral("Segoe UI"), 10, QFont::Bold);
            painter->setFont(labelFont);
            const QFontMetrics metrics(labelFont);
            const int pad = 8;
            const int boxW = metrics.horizontalAdvance(distanceText) + pad * 2;
            const int boxH = metrics.height() + pad * 2;
            QPoint labelPos(int(midpoint.x()) + 14, int(midpoint.y()) - boxH - 10);
            const QRect vp = viewport()->rect();
            if (labelPos.x() + boxW > vp.right() - 4)
                labelPos.setX(vp.right() - boxW - 4);
            if (labelPos.x() < vp.left() + 4)
                labelPos.setX(vp.left() + 4);
            if (labelPos.y() < vp.top() + 4)
                labelPos.setY(int(midpoint.y()) + 14);

            const QRect boxRect(labelPos, QSize(boxW, boxH));
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(18, 24, 34, 225));
            painter->drawRoundedRect(boxRect, 6, 6);
            painter->setPen(QPen(accent, 1));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(boxRect, 6, 6);
            painter->setPen(QColor(235, 240, 250, 240));
            painter->drawText(boxRect.left() + pad,
                              boxRect.top() + pad + metrics.ascent(),
                              distanceText);
            painter->setFont(font);
        }
    }

    if (m_placementMode) {
        const QRect vp = viewport()->rect();
        // Yellow frame - thick enough to be unmistakable even on dark
        // backgrounds. Drawn inset by half the pen width so nothing gets
        // clipped at the viewport edges.
        constexpr int kFrameThickness = 4;
        QPen framePen(QColor(255, 200, 0, 235));
        framePen.setWidth(kFrameThickness);
        framePen.setJoinStyle(Qt::MiterJoin);
        painter->setPen(framePen);
        painter->setBrush(Qt::NoBrush);
        const int inset = kFrameThickness / 2;
        painter->drawRect(vp.adjusted(inset, inset, -inset, -inset));

        const QString helpText = m_placementHelpText.isEmpty()
            ? tr("Klicke auf die Map, um das Objekt zu platzieren. [Esc] bricht ab.")
            : m_placementHelpText;
        QFont bannerFont(QStringLiteral("Segoe UI"), 11, QFont::Bold);
        painter->setFont(bannerFont);
        const QFontMetrics bannerMetrics(bannerFont);
        const int bannerPadding = 10;
        const int bannerWidth = bannerMetrics.horizontalAdvance(helpText) + bannerPadding * 2;
        const int bannerHeight = bannerMetrics.height() + bannerPadding;
        const QRect bannerRect((vp.width() - bannerWidth) / 2,
                               vp.top() + 12,
                               bannerWidth,
                               bannerHeight);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 200, 0, 220));
        painter->drawRoundedRect(bannerRect, 6, 6);
        painter->setPen(QColor(30, 25, 0));
        painter->drawText(bannerRect.left() + bannerPadding,
                          bannerRect.top() + bannerPadding / 2 + bannerMetrics.ascent(),
                          helpText);
    }

    if (m_measurementStage != MeasurementStage::Inactive) {
        const QRect vp = viewport()->rect();
        constexpr int kFrameThickness = 4;
        QPen framePen(QColor(60, 210, 255, 235));
        framePen.setWidth(kFrameThickness);
        framePen.setJoinStyle(Qt::MiterJoin);
        painter->setPen(framePen);
        painter->setBrush(Qt::NoBrush);
        const int inset = kFrameThickness / 2;
        painter->drawRect(vp.adjusted(inset, inset, -inset, -inset));

        const QString bannerText = measurementBannerText();
        QFont bannerFont(QStringLiteral("Segoe UI"), 11, QFont::Bold);
        painter->setFont(bannerFont);
        const QFontMetrics bannerMetrics(bannerFont);
        const int bannerPadding = 10;
        const int bannerWidth = bannerMetrics.horizontalAdvance(bannerText) + bannerPadding * 2;
        const int bannerHeight = bannerMetrics.height() + bannerPadding;
        const QRect bannerRect((vp.width() - bannerWidth) / 2,
                               vp.top() + 12,
                               bannerWidth,
                               bannerHeight);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(60, 210, 255, 220));
        painter->drawRoundedRect(bannerRect, 6, 6);
        painter->setPen(QColor(8, 20, 30));
        painter->drawText(bannerRect.left() + bannerPadding,
                          bannerRect.top() + bannerPadding / 2 + bannerMetrics.ascent(),
                          bannerText);
    }

    painter->restore();
}

void SystemMapView::showEvent(QShowEvent *event)
{
    QGraphicsView::showEvent(event);
    applyInitialFitIfNeeded();
}

void SystemMapView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    if (scene()) {
        QRectF targetRect = scene()->sceneRect();
        if (targetRect.isNull() || targetRect.width() <= 0.0 || targetRect.height() <= 0.0)
            targetRect = scene()->itemsBoundingRect();
        if (!targetRect.isNull() && targetRect.width() > 0.0 && targetRect.height() > 0.0)
            m_minZoomScale = fitScaleForView(this, withNavMapPadding(targetRect));
    }
    applyInitialFitIfNeeded();
    rebuildLabelClusters();
}

void SystemMapView::applyInitialFitIfNeeded()
{
    if (!m_pendingInitialFit || !scene())
        return;
    if (viewport()->width() <= 1 || viewport()->height() <= 1)
        return;
    zoomToFit();
}

void SystemMapView::updateItemDetailForScale()
{
    if (!scene())
        return;

    const qreal scale = transform().m11();
    const auto sceneItems = scene()->items();
    for (QGraphicsItem *item : sceneItems) {
        if (auto *solarItem = dynamic_cast<SolarObjectItem *>(item)) {
            // Reset suppression before re-applying the filter; rebuildLabelClusters
            // will set it again for items that belong to an overlap cluster.
            solarItem->setLabelSuppressedByCluster(false);
            solarItem->applyDisplayFilter(m_displayFilterSettings, scale);
        } else if (auto *zoneItem = dynamic_cast<ZoneItem2D *>(item)) {
            zoneItem->applyDisplayFilter(m_displayFilterSettings);
        }
    }
    rebuildLabelClusters();
    if (m_hasMouseInViewport)
        updateHoveredClusterForMouse(m_lastMouseViewportPos);
}

void SystemMapView::rebuildLabelClusters()
{
    m_labelClusters.clear();
    m_hoveredClusterIndex = -1;
    if (!scene())
        return;

    // Match the label font used by SolarObjectItem so our overlap estimate
    // reflects what the user actually sees on screen.
    const QFont labelFont(QStringLiteral("Segoe UI"), 7);
    const QFontMetrics fm(labelFont);
    const int labelHeight = fm.height();

    struct Candidate {
        SolarObjectItem *item;
        QRect labelRect; // viewport coords
    };
    QVector<Candidate> candidates;
    candidates.reserve(64);

    const qreal scale = transform().m11();
    const auto sceneItems = scene()->items();
    for (QGraphicsItem *item : sceneItems) {
        auto *solarItem = dynamic_cast<SolarObjectItem *>(item);
        if (!solarItem || !solarItem->isVisible())
            continue;
        if (solarItem->isSelected())
            continue; // selected items always keep their label; skip clustering
        // Respect existing label gates (filter + zoom threshold). If the label
        // would be hidden anyway, it cannot overlap anything.
        if (!m_displayFilterSettings.labelVisibleForType(solarItem->objectType()))
            continue;
        // Mirror the zoom threshold used in SolarObjectItem so we do not create
        // clusters for labels that are not rendered at the current zoom.
        if (scale < 0.25)
            continue;

        const QPoint itemView = mapFromScene(solarItem->scenePos());
        const int labelWidth = fm.horizontalAdvance(solarItem->nickname());
        // Label sits to the upper right of the object marker; approximate the
        // offset in viewport pixels. The exact offset is not critical - we only
        // need a stable proxy that detects overlapping labels.
        const int offsetX = 6;
        const int offsetY = -labelHeight - 2;
        const QRect rect(itemView.x() + offsetX,
                         itemView.y() + offsetY,
                         labelWidth,
                         labelHeight);
        candidates.append({solarItem, rect});
    }

    const int count = candidates.size();
    if (count < 2)
        return;

    // Inflate each rect slightly so labels that nearly touch still count as
    // overlapping - visually they are just as hard to read.
    constexpr int kOverlapPadding = 2;

    QVector<int> parent(count);
    for (int i = 0; i < count; ++i)
        parent[i] = i;
    std::function<int(int)> find = [&](int x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    };
    auto unite = [&](int a, int b) {
        const int ra = find(a);
        const int rb = find(b);
        if (ra != rb)
            parent[ra] = rb;
    };

    for (int i = 0; i < count; ++i) {
        const QRect a = candidates[i].labelRect.adjusted(-kOverlapPadding, -kOverlapPadding,
                                                          kOverlapPadding, kOverlapPadding);
        for (int j = i + 1; j < count; ++j) {
            if (a.intersects(candidates[j].labelRect))
                unite(i, j);
        }
    }

    QHash<int, int> rootToCluster;
    for (int i = 0; i < count; ++i) {
        const int root = find(i);
        int clusterIdx = rootToCluster.value(root, -1);
        if (clusterIdx < 0) {
            clusterIdx = m_labelClusters.size();
            rootToCluster.insert(root, clusterIdx);
            LabelCluster cluster;
            cluster.viewportBounds = candidates[i].labelRect;
            m_labelClusters.append(cluster);
        }
        LabelCluster &cluster = m_labelClusters[clusterIdx];
        cluster.members.append(candidates[i].item);
        cluster.nicknames.append(candidates[i].item->nickname());
        cluster.viewportBounds = cluster.viewportBounds.united(candidates[i].labelRect);
    }

    // Drop trivial clusters (size < 2) and suppress inline labels on real
    // clusters. At normal zoom levels the labels of the primary navigational
    // landmarks (suns, planets, stations, jumpgates, jumpholes) should remain
    // visible even inside a cluster so the user can always orient themselves
    // from far away. Only when the user zooms in deep enough the full stacked
    // hover-popup becomes available for the fine-grained details.
    constexpr qreal kPopupDeepZoomThreshold = 1.25;
    const bool deepZoom = scale >= kPopupDeepZoomThreshold;

    auto isPriorityType = [](flatlas::domain::SolarObject::Type t) {
        using T = flatlas::domain::SolarObject::Type;
        return t == T::Sun || t == T::Planet || t == T::Station
            || t == T::JumpGate || t == T::JumpHole;
    };

    for (int i = m_labelClusters.size() - 1; i >= 0; --i) {
        LabelCluster &cluster = m_labelClusters[i];
        if (cluster.members.size() < 2) {
            m_labelClusters.removeAt(i);
            continue;
        }

        if (deepZoom) {
            // Deep zoom: suppress every inline label in the cluster; the
            // hover popup takes over.
            for (SolarObjectItem *member : cluster.members) {
                member->setLabelSuppressedByCluster(true);
                member->setLabelVisibleForScale(scale);
            }
            cluster.popupEligible = true;
        } else {
            // Normal zoom: only suppress the low-priority labels so the
            // primary landmarks stay readable.
            bool anySuppressed = false;
            for (SolarObjectItem *member : cluster.members) {
                if (isPriorityType(member->objectType()))
                    continue;
                member->setLabelSuppressedByCluster(true);
                member->setLabelVisibleForScale(scale);
                anySuppressed = true;
            }
            if (!anySuppressed) {
                // Nothing was actually decluttered - no popup needed either.
                m_labelClusters.removeAt(i);
                continue;
            }
            cluster.popupEligible = false;
        }
        cluster.viewportAnchor = cluster.viewportBounds.center();
    }
}

void SystemMapView::updateHoveredClusterForMouse(const QPoint &viewportPos)
{
    if (m_labelClusters.isEmpty()) {
        if (m_hoveredClusterIndex != -1) {
            m_hoveredClusterIndex = -1;
            viewport()->update();
        }
        return;
    }

    // Hysteresis: a cluster appears once the cursor is within the show radius
    // around its bounding rect, and only disappears once the cursor leaves a
    // larger hide radius. Combined with a short debounce timer this prevents
    // the popup from flickering when the mouse passes between dense objects.
    constexpr int kShowMargin = 18;
    constexpr int kHideMargin = 42;

    int bestIndex = -1;
    int bestDistance = std::numeric_limits<int>::max();
    for (int i = 0; i < m_labelClusters.size(); ++i) {
        if (!m_labelClusters[i].popupEligible)
            continue;
        const QRect bounds = m_labelClusters[i].viewportBounds;
        const QRect showRect = bounds.adjusted(-kShowMargin, -kShowMargin, kShowMargin, kShowMargin);
        if (!showRect.contains(viewportPos))
            continue;
        const QPoint centre = bounds.center();
        const int dx = centre.x() - viewportPos.x();
        const int dy = centre.y() - viewportPos.y();
        const int distSq = dx * dx + dy * dy;
        if (distSq < bestDistance) {
            bestDistance = distSq;
            bestIndex = i;
        }
    }

    if (bestIndex < 0 && m_hoveredClusterIndex >= 0
        && m_hoveredClusterIndex < m_labelClusters.size()) {
        // Still inside the wider hide rect around the currently hovered cluster?
        const QRect bounds = m_labelClusters[m_hoveredClusterIndex].viewportBounds;
        const QRect hideRect = bounds.adjusted(-kHideMargin, -kHideMargin, kHideMargin, kHideMargin);
        if (hideRect.contains(viewportPos))
            bestIndex = m_hoveredClusterIndex;
    }

    if (bestIndex == m_hoveredClusterIndex) {
        if (bestIndex >= 0 && m_clusterHoverHideTimer)
            m_clusterHoverHideTimer->stop();
        return;
    }

    if (bestIndex >= 0) {
        if (m_clusterHoverHideTimer)
            m_clusterHoverHideTimer->stop();
        m_hoveredClusterIndex = bestIndex;
        viewport()->update();
    } else if (m_clusterHoverHideTimer) {
        // Defer the hide so a brief mouse twitch outside the bounds does not
        // collapse the popup mid-read.
        m_clusterHoverHideTimer->start();
    }
}

void SystemMapView::scrollContentsBy(int dx, int dy)
{
    QGraphicsView::scrollContentsBy(dx, dy);
    // Viewport positions of all labels shifted - rebuild so the suppression
    // and popup anchors stay accurate while panning.
    rebuildLabelClusters();
    if (m_hasMouseInViewport)
        updateHoveredClusterForMouse(m_lastMouseViewportPos);
}

void SystemMapView::leaveEvent(QEvent *event)
{
    m_hasMouseInViewport = false;
    if (m_hoveredClusterIndex >= 0 && m_clusterHoverHideTimer)
        m_clusterHoverHideTimer->start();
    QGraphicsView::leaveEvent(event);
}

void SystemMapView::keyPressEvent(QKeyEvent *event)
{
    if (m_measurementStage != MeasurementStage::Inactive && event->key() == Qt::Key_Escape) {
        cancelMeasurementMode();
        event->accept();
        return;
    }
    if (m_placementMode && event->key() == Qt::Key_Escape) {
        m_placementMode = false;
        m_placementHelpText.clear();
        setCursor(Qt::ArrowCursor);
        viewport()->update();
        emit placementCanceled();
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void SystemMapView::setPlacementMode(bool enabled, const QString &helpText)
{
    if (m_placementMode == enabled && m_placementHelpText == helpText)
        return;
    m_placementMode = enabled;
    m_placementHelpText = enabled ? helpText : QString();
    if (enabled) {
        setFocus(Qt::OtherFocusReason);
        setCursor(Qt::CrossCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
    viewport()->update();
}

void SystemMapView::startMeasurementMode()
{
    clearMeasurement(true);
    m_measurementStage = MeasurementStage::AwaitingStart;
    updateMeasurementCursor();
    setFocus(Qt::OtherFocusReason);
    viewport()->update();
}

void SystemMapView::cancelMeasurementMode()
{
    clearMeasurement(false);
}

void SystemMapView::clearMeasurement(bool keepMode)
{
    if (!keepMode)
        m_measurementStage = MeasurementStage::Inactive;
    m_measurementStartScenePos = QPointF();
    m_measurementEndScenePos = QPointF();
    m_measurementHasFinal = false;
    updateMeasurementCursor();
    viewport()->update();
}

void SystemMapView::updateMeasurementCursor()
{
    if (m_measurementStage != MeasurementStage::Inactive)
        setCursor(Qt::CrossCursor);
    else if (!m_placementMode)
        setCursor(Qt::ArrowCursor);
}

QString SystemMapView::measurementBannerText() const
{
    if (m_measurementStage == MeasurementStage::AwaitingStart)
        return tr("Ruler: click the first point. [Esc] cancels.");
    if (m_measurementStage == MeasurementStage::AwaitingEnd)
        return tr("Ruler: click the second point. [Esc] cancels.");
    return tr("Ruler");
}

bool SystemMapView::hasMeasurementResult() const
{
    return m_measurementHasFinal;
}

double SystemMapView::measurementDistanceWorld() const
{
    if (m_measurementStage == MeasurementStage::Inactive && !m_measurementHasFinal)
        return 0.0;
    return worldDistanceForScenePoints(m_measurementStartScenePos, m_measurementEndScenePos);
}

void SystemMapView::beginTrackedSelectionMove(QMouseEvent *event)
{
    m_trackingSelectionMove = false;
    m_moveStartPositions.clear();
    m_moveVerticalOffsetMeters = 0.0;
    m_moveDidEmitStart = false;

    if (!m_mapScene || !m_mapScene->isMoveEnabled() || event->button() != Qt::LeftButton)
        return;

    QGraphicsItem *hitItem = itemAt(event->pos());
    const QString hitNickname = itemNickname(hitItem);
    const bool hitAlreadySelected = hitItem && hitItem->isSelected();
    const Qt::KeyboardModifiers modifiers = event->modifiers();
    const bool plainDragStart = !(modifiers & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier));
    if (plainDragStart && !hitNickname.isEmpty()) {
        const QStringList targetSelection = resolvedMoveSelectionForHit(hitNickname, hitAlreadySelected);
        if (!targetSelection.isEmpty())
            m_mapScene->selectNicknames(targetSelection);
        hitItem = itemAt(event->pos());
    }

    if (!hitItem || !hitItem->isSelected())
        return;

    const auto sceneItems = m_mapScene->selectedItems();
    for (QGraphicsItem *item : sceneItems) {
        const QString nickname = itemNickname(item);
        if (!nickname.isEmpty())
            m_moveStartPositions.insert(nickname, item->scenePos());
    }

    m_trackingSelectionMove = !m_moveStartPositions.isEmpty();
}

QStringList SystemMapView::resolvedMoveSelectionForHit(const QString &hitNickname, bool hitAlreadySelected) const
{
    if (!m_mapScene || hitNickname.trimmed().isEmpty())
        return {};

    auto resolveNickname = [this](const QString &nickname) {
        if (!m_moveGroupResolver)
            return QStringList{nickname};
        const QStringList resolved = m_moveGroupResolver(nickname);
        return resolved.isEmpty() ? QStringList{nickname} : resolved;
    };

    QStringList targetSelection;
    if (hitAlreadySelected) {
        const QStringList currentSelection = m_mapScene->selectedNicknames();
        for (const QString &nickname : currentSelection)
            targetSelection.append(resolveNickname(nickname));
    } else {
        targetSelection = resolveNickname(hitNickname);
    }

    targetSelection.removeDuplicates();
    return targetSelection;
}

void SystemMapView::finishTrackedSelectionMove()
{
    if (!m_trackingSelectionMove || !m_mapScene) {
        m_moveStartPositions.clear();
        m_moveVerticalOffsetMeters = 0.0;
        m_moveDidEmitStart = false;
        m_trackingSelectionMove = false;
        viewport()->update();
        return;
    }

    QHash<QString, QPointF> movedFrom;
    QHash<QString, QPointF> movedTo;
    const auto sceneItems = m_mapScene->selectedItems();
    const double yOffset = m_moveVerticalOffsetMeters;
    const bool hasYOffset = std::abs(yOffset) > 1e-6;
    for (QGraphicsItem *item : sceneItems) {
        const QString nickname = itemNickname(item);
        if (nickname.isEmpty() || !m_moveStartPositions.contains(nickname))
            continue;

        const QPointF oldPos = m_moveStartPositions.value(nickname);
        const QPointF newPos = item->scenePos();
        const bool xzChanged = QLineF(oldPos, newPos).length() > 0.0001;
        if (!xzChanged && !hasYOffset)
            continue;

        movedFrom.insert(nickname, oldPos);
        movedTo.insert(nickname, newPos);
    }

    const double emittedOffset = m_moveVerticalOffsetMeters;
    m_moveStartPositions.clear();
    m_moveVerticalOffsetMeters = 0.0;
    m_moveDidEmitStart = false;
    m_trackingSelectionMove = false;
    viewport()->update();

    if (!movedFrom.isEmpty())
        emit itemsMoved(movedFrom, movedTo, emittedOffset);
}

void SystemMapView::updateRubberBandSelection(const QRect &viewportRect, Qt::KeyboardModifiers modifiers)
{
    if (!m_mapScene || viewportRect.isNull())
        return;

    const QPolygonF scenePolygon = mapToScene(viewportRect);
    QPainterPath selectionPath;
    selectionPath.addPolygon(scenePolygon);
    selectionPath.closeSubpath();

    if (!(modifiers & Qt::ControlModifier))
        m_mapScene->clearSelection();

    m_mapScene->setSelectionArea(selectionPath,
                                 (modifiers & Qt::ControlModifier) ? Qt::AddToSelection : Qt::ReplaceSelection,
                                 Qt::IntersectsItemShape,
                                 viewportTransform());
}

QString SystemMapView::itemNicknameAtViewportPos(const QPoint &pos) const
{
    return itemNickname(itemAt(pos));
}

QStringList SystemMapView::zoneNicknamesAtViewportPos(const QPoint &pos) const
{
    QStringList nicknames;
    const auto hitItems = items(pos);
    nicknames.reserve(hitItems.size());
    for (QGraphicsItem *item : hitItems) {
        auto *zoneItem = dynamic_cast<ZoneItem2D *>(item);
        if (!zoneItem || !zoneItem->isVisible())
            continue;
        const QString nickname = zoneItem->nickname().trimmed();
        if (!nickname.isEmpty())
            nicknames.append(nickname);
    }
    nicknames.removeDuplicates();
    return nicknames;
}

} // namespace flatlas::rendering
