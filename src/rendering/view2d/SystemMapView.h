#pragma once
#include <QGraphicsView>
#include <QColor>
#include <QPixmap>

namespace flatlas::rendering {

class MapScene;

class SystemMapView : public QGraphicsView {
    Q_OBJECT
public:
    explicit SystemMapView(QWidget *parent = nullptr);

    MapScene *mapScene() const;
    void setMapScene(MapScene *scene);
    void setBackgroundPixmap(const QPixmap &pixmap, const QColor &fallbackColor = QColor(15, 18, 24));
    void zoomToFit();

signals:
    void objectSelected(const QString &nickname);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void drawBackground(QPainter *painter, const QRectF &rect) override;

private:
    MapScene *m_mapScene = nullptr;
    bool m_panning = false;
    QPixmap m_backgroundPixmap;
    QColor m_backgroundColor = QColor(15, 18, 24);
    int m_backgroundDarkenAlpha = 180;
};

} // namespace flatlas::rendering
