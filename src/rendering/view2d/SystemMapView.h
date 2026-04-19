#pragma once
// rendering/view2d/SystemMapView.h – QGraphicsView für 2D-Systemkarte
// TODO Phase 4
#include <QGraphicsView>
namespace flatlas::rendering {
class SystemMapView : public QGraphicsView {
    Q_OBJECT
public:
    explicit SystemMapView(QWidget *parent = nullptr);
signals:
    void objectSelected(const QString &nickname);
};
} // namespace flatlas::rendering
