#pragma once
// rendering/view3d/SceneView3D.h – Qt3D-Hauptview
// TODO Phase 7
#include <QWidget>
namespace flatlas::rendering {
class SceneView3D : public QWidget {
    Q_OBJECT
public:
    explicit SceneView3D(QWidget *parent = nullptr);
signals:
    void objectSelected(const QString &nickname);
};
} // namespace flatlas::rendering
