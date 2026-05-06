#pragma once
// rendering/view3d/SceneView3D.h - practical 3D system editor viewport

#include <QHash>
#include <QImage>
#include <QSet>
#include <QStringList>
#include <QElapsedTimer>
#include <QWidget>

#include "rendering/view2d/SystemDisplayFilter.h"

#include <memory>

class QTimer;

#ifdef FLATLAS_HAS_QT3D
namespace Qt3DExtras { class Qt3DWindow; }
namespace Qt3DCore { class QEntity; }
namespace Qt3DCore { class QTransform; }
namespace Qt3DRender { class QCamera; class QMaterial; class QPointLight; }
#endif

namespace flatlas::domain { class SystemDocument; class SolarObject; class ZoneItem; }
namespace flatlas::infrastructure { struct DecodedModel; struct ModelNode; }

namespace flatlas::rendering {

#ifdef FLATLAS_HAS_QT3D
class OrbitCamera;
class FreeCameraController;
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
    void setArchetypeDisplayRadii(const QHash<QString, float> &displayRadii);
    void setArchetypeTextureSourcePaths(const QHash<QString, QStringList> &textureSourcePaths);
    void setGameRoot(const QString &gameRoot);
    void setDisplayFilterSettings(const SystemDisplayFilterSettings &settings);
    void loadDocument(flatlas::domain::SystemDocument *doc);
    void selectObject(const QString &nickname);
    bool centerOnSelectedObject();
    void setZoomLevel(int percent);
    void setZoneWireframesVisible(bool visible);
    void setFreeCameraModeEnabled(bool enabled);
    bool isFreeCameraModeEnabled() const;
    float freeCameraSpeed() const;
    void setFreeCameraSpeed(float speed);
    void setFreeCameraFlightProfile(float normalSpeed, float cruiseSpeed, float cruiseChargeTime);
    void setThirdPersonCamera(float fovX, float zNear);
    void setFlightShipModel(const QString &modelPath);
    void beginCruise();
    void cancelCruise();
    bool setFreeCameraStartObject(const QString &nickname);
    void setFlightModeEnabled(bool enabled);
    bool isFlightModeEnabled() const { return m_flightModeEnabled; }
    void refreshThemeColors();

#ifdef FLATLAS_HAS_QT3D
public slots:
    void setViewportActive(bool active);
#endif

signals:
    void objectSelected(const QString &nickname);
    void zoomLevelChanged(int percent);
    void freeCameraModeChanged(bool enabled);
    void freeCameraSpeedChanged(float speed);

protected:
#ifdef FLATLAS_HAS_QT3D
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
#endif

private:
    void setupScene();
    void clearScene();
    void addNavigationGrid();
    void addSolarObject(const std::shared_ptr<flatlas::domain::SolarObject> &obj);
    void addAtmosphereZone(const flatlas::domain::SolarObject &obj);
    void addPlanetaryRing(const flatlas::domain::SolarObject &obj);
    void addZone(const std::shared_ptr<flatlas::domain::ZoneItem> &zone);
    void updateSceneCamera();
    void applyDisplayFilter();

#ifdef FLATLAS_HAS_QT3D
    void requestViewportUpdate();
    void scheduleModelLoading();
    void schedulePlanetTextureLoading();
    void attachLoadedModels(const QHash<QString, flatlas::infrastructure::DecodedModel> &models, int generation);
    void applyPlanetTextures(const QHash<QString, QImage> &textures, int generation);
    int addModelNodeRecursive(const flatlas::infrastructure::ModelNode &node,
                              Qt3DCore::QEntity *parent,
                              const QString &nickname,
                              const QString &modelPath,
                              int nodeIndex = 0,
                              int depth = 0);
    QString modelPathForObject(const flatlas::domain::SolarObject &obj) const;
    float displayRadiusForObject(const flatlas::domain::SolarObject &obj) const;
    bool shouldRenderAsRadiusSphere(const flatlas::domain::SolarObject &obj) const;
    void applyCameraZoom();
    void syncZoomLevelFromCamera();
    void applyZoneWireframeVisibility();
    void tickFreeCamera();
    void updateCameraDependentScene();
    void updateFlightShipTransform();

    Qt3DExtras::Qt3DWindow *m_3dWindow = nullptr;
    QWidget *m_container = nullptr;
    Qt3DCore::QEntity *m_rootEntity = nullptr;
    Qt3DCore::QEntity *m_sceneRoot = nullptr;
    Qt3DCore::QEntity *m_gridEntity = nullptr;
    Qt3DCore::QEntity *m_objectsRoot = nullptr;
    Qt3DCore::QEntity *m_flightShipEntity = nullptr;
    Qt3DCore::QTransform *m_flightShipTransform = nullptr;
    Qt3DCore::QEntity *m_zonesRoot = nullptr;
    Qt3DRender::QCamera *m_camera = nullptr;
    Qt3DRender::QPointLight *m_light = nullptr;
    OrbitCamera *m_orbitCamera = nullptr;
    FreeCameraController *m_freeCamera = nullptr;
    SelectionManager *m_selectionManager = nullptr;
    SkyRenderer *m_skyRenderer = nullptr;
    QHash<QString, Qt3DCore::QEntity *> m_modelHostsByNickname;
    QHash<QString, Qt3DCore::QEntity *> m_markerEntitiesByNickname;
    QHash<QString, Qt3DRender::QMaterial *> m_markerMaterialsByNickname;
    QHash<QString, QList<Qt3DCore::QEntity *>> m_ringEntitiesByHostNickname;
    QHash<QString, Qt3DCore::QEntity *> m_atmosphereZoneEntitiesByObjectNickname;
    QHash<QString, Qt3DCore::QEntity *> m_sceneEntitiesByNickname;
    QHash<QString, Qt3DCore::QEntity *> m_zoneWireEntitiesByNickname;
    QHash<QString, QStringList> m_nicknamesByModelPath;
    QHash<QString, QStringList> m_planetTextureSourcePathsByNickname;
    QSet<QString> m_nicknamesWithRenderedModel;
    QSet<QString> m_linkedRingZoneNicknames;
    QHash<QString, QVector3D> m_objectCentersByNickname;
    QHash<QString, float> m_objectRadiiByNickname;
    ModelBounds *m_sceneBounds = nullptr;
    ModelBounds *m_objectBounds = nullptr;
    ModelBounds *m_zoneBounds = nullptr;
    QTimer *m_freeCameraTimer = nullptr;
    QElapsedTimer m_freeCameraClock;
    int m_loadGeneration = 0;
#endif

    flatlas::domain::SystemDocument *m_document = nullptr;
    QHash<QString, QString> m_archetypeModelPaths;
    QHash<QString, float> m_archetypeDisplayRadii;
    QHash<QString, QStringList> m_archetypeTextureSourcePaths;
    QString m_gameRoot;
    SystemDisplayFilterSettings m_displayFilterSettings;
    int m_zoomLevel = 50;
    bool m_zoneWireframesVisible = true;
    bool m_flightModeEnabled = false;
};

} // namespace flatlas::rendering
