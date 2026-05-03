#pragma once
// rendering/view3d/SceneView3D.h - practical 3D system editor viewport

#include <QHash>
#include <QStringList>
#include <QWidget>

#include "rendering/view2d/SystemDisplayFilter.h"

#include <memory>

#ifdef FLATLAS_HAS_QT3D
namespace Qt3DExtras { class Qt3DWindow; }
namespace Qt3DCore { class QEntity; }
namespace Qt3DRender { class QCamera; class QPointLight; }
#endif

namespace flatlas::domain { class SystemDocument; class SolarObject; class ZoneItem; }
namespace flatlas::infrastructure { struct DecodedModel; struct ModelNode; }

namespace flatlas::rendering {

#ifdef FLATLAS_HAS_QT3D
class OrbitCamera;
class SelectionManager;
class SkyRenderer;
struct ModelBounds;
#endif

class SceneView3D : public QWidget {
    Q_OBJECT
public:
    explicit SceneView3D(QWidget *parent = nullptr);
    ~SceneView3D() override;

    void setArchetypeModelPaths(const QHash<QString, QString> &modelPaths);
    void setDisplayFilterSettings(const SystemDisplayFilterSettings &settings);
    void loadDocument(flatlas::domain::SystemDocument *doc);
    void selectObject(const QString &nickname);

signals:
    void objectSelected(const QString &nickname);

protected:
#ifdef FLATLAS_HAS_QT3D
    bool eventFilter(QObject *watched, QEvent *event) override;
#endif

private:
    void setupScene();
    void clearScene();
    void addSolarObject(const std::shared_ptr<flatlas::domain::SolarObject> &obj);
    void addZone(const std::shared_ptr<flatlas::domain::ZoneItem> &zone);
    void updateSceneCamera();
    void applyDisplayFilter();

#ifdef FLATLAS_HAS_QT3D
    void scheduleModelLoading();
    void attachLoadedModels(const QHash<QString, flatlas::infrastructure::DecodedModel> &models, int generation);
    int addModelNodeRecursive(const flatlas::infrastructure::ModelNode &node,
                              Qt3DCore::QEntity *parent,
                              const QString &nickname,
                              int nodeIndex = 0,
                              int depth = 0);
    QString modelPathForObject(const flatlas::domain::SolarObject &obj) const;

    Qt3DExtras::Qt3DWindow *m_3dWindow = nullptr;
    QWidget *m_container = nullptr;
    Qt3DCore::QEntity *m_rootEntity = nullptr;
    Qt3DCore::QEntity *m_sceneRoot = nullptr;
    Qt3DCore::QEntity *m_objectsRoot = nullptr;
    Qt3DCore::QEntity *m_zonesRoot = nullptr;
    Qt3DRender::QCamera *m_camera = nullptr;
    Qt3DRender::QPointLight *m_light = nullptr;
    OrbitCamera *m_orbitCamera = nullptr;
    SelectionManager *m_selectionManager = nullptr;
    SkyRenderer *m_skyRenderer = nullptr;
    QHash<QString, Qt3DCore::QEntity *> m_modelHostsByNickname;
    QHash<QString, Qt3DCore::QEntity *> m_markerEntitiesByNickname;
    QHash<QString, Qt3DCore::QEntity *> m_sceneEntitiesByNickname;
    QHash<QString, QStringList> m_nicknamesByModelPath;
    ModelBounds *m_sceneBounds = nullptr;
    ModelBounds *m_objectBounds = nullptr;
    ModelBounds *m_zoneBounds = nullptr;
    int m_loadGeneration = 0;
#endif

    flatlas::domain::SystemDocument *m_document = nullptr;
    QHash<QString, QString> m_archetypeModelPaths;
    SystemDisplayFilterSettings m_displayFilterSettings;
};

} // namespace flatlas::rendering
