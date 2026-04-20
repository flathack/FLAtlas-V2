#pragma once
// rendering/preview/ModelPreview.h – 3D-Modell-Vorschau (Phase 16)

#include <QDialog>
#include <QString>

#ifdef FLATLAS_HAS_QT3D
namespace Qt3DExtras { class Qt3DWindow; }
namespace Qt3DCore { class QEntity; }
namespace Qt3DRender { class QCamera; }
#endif

namespace flatlas::rendering {

class OrbitCamera;

/// Standalone dialog showing a rotatable 3D model (CMP/3DB).
class ModelPreview : public QDialog {
    Q_OBJECT
public:
    explicit ModelPreview(QWidget *parent = nullptr);
    ~ModelPreview() override;

    /// Load a CMP or 3DB model from file.
    void loadModel(const QString &filePath);

    /// Currently loaded file path.
    QString filePath() const { return m_filePath; }

    /// Whether a model is loaded.
    bool hasModel() const { return m_hasModel; }

protected:
#ifdef FLATLAS_HAS_QT3D
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
#endif

private:
    void setupUi();
    void setupScene();
    void clearModel();

    QString m_filePath;
    bool m_hasModel = false;

#ifdef FLATLAS_HAS_QT3D
    Qt3DExtras::Qt3DWindow *m_3dWindow = nullptr;
    QWidget *m_container = nullptr;
    Qt3DCore::QEntity *m_rootEntity = nullptr;
    Qt3DCore::QEntity *m_modelRoot = nullptr;
    Qt3DRender::QCamera *m_camera = nullptr;
    OrbitCamera *m_orbitCamera = nullptr;
#endif
};

} // namespace flatlas::rendering
