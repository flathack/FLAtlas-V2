#pragma once
#include <QGraphicsView>

namespace flatlas::rendering {

class MapScene;

class SystemMapView : public QGraphicsView {
    Q_OBJECT
public:
    explicit SystemMapView(QWidget *parent = nullptr);

    MapScene *mapScene() const;
    void setMapScene(MapScene *scene);
    void zoomToFit();

signals:
    void objectSelected(const QString &nickname);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    MapScene *m_mapScene = nullptr;
    bool m_panning = false;
};

} // namespace flatlas::rendering
