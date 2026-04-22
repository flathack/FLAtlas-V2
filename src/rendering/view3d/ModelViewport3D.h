#pragma once
// rendering/view3d/ModelViewport3D.h - reusable Qt3D model viewport for Freelancer models

#include <QWidget>
#include <QString>

class QLabel;

#ifdef FLATLAS_HAS_QT3D
namespace Qt3DExtras { class Qt3DWindow; class QPhongMaterial; }
namespace Qt3DCore { class QEntity; class QTransform; }
namespace Qt3DRender { class QCamera; class QPointLight; }
#endif

namespace flatlas::infrastructure { struct ModelNode; }

namespace flatlas::rendering {

#ifdef FLATLAS_HAS_QT3D
class OrbitCamera;
struct ModelBounds;
#endif

class ModelViewport3D : public QWidget {
    Q_OBJECT
public:
    explicit ModelViewport3D(QWidget *parent = nullptr);
    ~ModelViewport3D() override;

    bool loadModelFile(const QString &filePath, QString *errorMessage = nullptr);
    void clearModel();
    void resetView();

    QString currentFilePath() const { return m_filePath; }
    bool hasModel() const { return m_hasModel; }

    void setWireframeVisible(bool visible);
    bool wireframeVisible() const { return m_wireframeVisible; }
    void setMeshVisible(bool visible);
    bool meshVisible() const { return m_meshVisible; }
    void setBoundingBoxVisible(bool visible);
    bool boundingBoxVisible() const { return m_boundingBoxVisible; }
    void setWhiteBackground(bool enabled);
    bool whiteBackground() const { return m_whiteBackground; }

signals:
    void modelLoaded(const QString &filePath, bool success, const QString &message);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupUi();
    void setupScene();
    void setStatusMessage(const QString &text);
    void rebuildScene(const flatlas::infrastructure::ModelNode &model);

#ifdef FLATLAS_HAS_QT3D
    void clearSceneEntities();
    void addNodeRecursive(const flatlas::infrastructure::ModelNode &node,
                          Qt3DCore::QEntity *meshParentEntity,
                          Qt3DCore::QEntity *wireParentEntity,
                          int nodeIndex = 0,
                          int depth = 0);
    void updateVisibilityState();
    void fitCameraToBounds(const ModelBounds &bounds);

    Qt3DExtras::Qt3DWindow *m_window = nullptr;
    QWidget *m_container = nullptr;
    Qt3DCore::QEntity *m_rootEntity = nullptr;
    Qt3DCore::QEntity *m_lightEntity = nullptr;
    Qt3DRender::QPointLight *m_light = nullptr;
    Qt3DCore::QTransform *m_lightTransform = nullptr;
    Qt3DCore::QEntity *m_sceneRoot = nullptr;
    Qt3DCore::QEntity *m_modelRoot = nullptr;
    Qt3DCore::QEntity *m_wireframeRoot = nullptr;
    Qt3DCore::QEntity *m_overlayRoot = nullptr;
    Qt3DCore::QEntity *m_boundingBoxEntity = nullptr;
    Qt3DRender::QCamera *m_camera = nullptr;
    OrbitCamera *m_orbitCamera = nullptr;
#endif

    QWidget *m_statusOverlay = nullptr;
    QLabel *m_statusLabel = nullptr;

    QString m_filePath;
    bool m_hasModel = false;
    bool m_wireframeVisible = false;
    bool m_meshVisible = true;
    bool m_boundingBoxVisible = false;
    bool m_whiteBackground = false;
};

} // namespace flatlas::rendering
