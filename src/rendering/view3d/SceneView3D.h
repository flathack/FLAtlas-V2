#pragma once
// rendering/view3d/SceneView3D.h – Qt3D-Hauptview (Phase 7)

#include <QWidget>
#include <memory>

#ifdef FLATLAS_HAS_QT3D
namespace Qt3DExtras { class Qt3DWindow; }
namespace Qt3DCore { class QEntity; }
namespace Qt3DRender { class QCamera; class QPointLight; }
#endif

namespace flatlas::domain { class SystemDocument; class SolarObject; }

namespace flatlas::rendering {

#ifdef FLATLAS_HAS_QT3D
class OrbitCamera;
class SelectionManager;
class SkyRenderer;
#endif

class SceneView3D : public QWidget {
    Q_OBJECT
public:
    explicit SceneView3D(QWidget *parent = nullptr);
    ~SceneView3D() override;

    /// Load objects from a SystemDocument into the 3D scene.
    void loadDocument(flatlas::domain::SystemDocument *doc);

    /// Select object by nickname (for 2D↔3D sync).
    void selectObject(const QString &nickname);

signals:
    void objectSelected(const QString &nickname);

protected:
#ifdef FLATLAS_HAS_QT3D
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
#endif

private:
    void setupScene();
    void clearScene();
    void addSolarObject(const std::shared_ptr<flatlas::domain::SolarObject> &obj);

#ifdef FLATLAS_HAS_QT3D
    Qt3DExtras::Qt3DWindow *m_3dWindow = nullptr;
    QWidget *m_container = nullptr;
    Qt3DCore::QEntity *m_rootEntity = nullptr;
    Qt3DCore::QEntity *m_objectsRoot = nullptr;
    Qt3DRender::QCamera *m_camera = nullptr;
    Qt3DRender::QPointLight *m_light = nullptr;
    OrbitCamera *m_orbitCamera = nullptr;
    SelectionManager *m_selectionManager = nullptr;
    SkyRenderer *m_skyRenderer = nullptr;
#endif
    flatlas::domain::SystemDocument *m_document = nullptr;
};

} // namespace flatlas::rendering
