#pragma once
// editors/system/SystemEditorPage.h – System-Editor (Phase 5)

#include <QWidget>
#include <QHash>
#include <QPointF>
#include <QStringList>
#include <QVector3D>
#include "rendering/view2d/SystemDisplayFilter.h"
#include <memory>

namespace flatlas::domain { class SystemDocument; class SolarObject; class ZoneItem; }
namespace flatlas::rendering { class MapScene; class SystemMapView; class SceneView3D; }
namespace flatlas::editors {
struct CreateFieldZoneResult;
struct CreateExclusionZoneResult;
struct CreateSimpleZoneRequest;
struct CreatePatrolZoneRequest;
struct CreateBuoyRequest;
struct CreateTradeLaneRequest;
struct ExclusionShellSettings;
}
class QToolBar;
class QSplitter;
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;
class QLabel;
class QAction;
class QPushButton;
class QComboBox;
class QLineEdit;
class QGroupBox;
class QFrame;
class QScrollArea;
class QVBoxLayout;
class QStackedWidget;
class QGraphicsEllipseItem;
class QGraphicsLineItem;
class QGraphicsPolygonItem;

namespace flatlas::editors { class IniCodeEditor; class IniSyntaxHighlighter; }

namespace flatlas::editors {

class SystemEditorPage : public QWidget {
    Q_OBJECT
public:
    explicit SystemEditorPage(QWidget *parent = nullptr);
    ~SystemEditorPage() override;

    /// Load system from INI file. Returns false on error.
    bool loadFile(const QString &filePath);

    /// Assign an already-created document (e.g. from wizard).
    void setDocument(std::unique_ptr<flatlas::domain::SystemDocument> doc);

    /// Save system to its current filePath.
    bool save();

    /// Save system to a new filePath.
    bool saveAs(const QString &filePath);

    flatlas::domain::SystemDocument *document() const;
    QString filePath() const;
    bool isDirty() const;

signals:
    void titleChanged(const QString &title);
    void selectionStatusChanged(const QString &message);
    void loadingProgressChanged(int percent, const QString &message);

private:
    struct PendingGeneratedZoneFile {
        QString absolutePath;
        QString content;
    };

    struct PendingTextFileWrite {
        QString absolutePath;
        QString content;
    };

    void loadDocumentIntoUi();
    void openSystemSettingsDialog();
    void emitLoadingProgress(int percent, const QString &message);
    flatlas::domain::SolarObject *findObjectByNickname(const QString &nickname) const;
    flatlas::domain::ZoneItem *findZoneByNickname(const QString &nickname) const;
    QString normalizeObjectNicknameToGroupRoot(const QString &nickname) const;
    QStringList normalizeSelectionNicknames(const QStringList &nicknames) const;
    QStringList expandSelectionNicknamesForScene(const QStringList &nicknames) const;
    QStringList objectGroupNicknames(const QString &rootNickname) const;
    bool isChildObject(const flatlas::domain::SolarObject &obj) const;
    bool hasSingleObjectGroupSelection() const;
    void open3DPreviewForSelection();
    void setupUi();
    void setupToolBar();
    void setupObjectList();
    void connectSignals();
    void ensureSceneView3D();
    void bindDocumentSignals();
    void refreshTitle();
    void set3DViewEnabled(bool enabled);
    void openDisplayFilterDialog();
    void loadDisplayFilterSettings();
    void saveDisplayFilterSettings() const;
    QString displayFilterConfigKey() const;
    void setupRightSidebar();
    void applyThemeStyling();
    void updateSelectionSummary();
    void updateSidebarButtons();
    void refreshObjectJumpList();
    void jumpToSelectedFromSidebar();
    void updateIniEditorForSelection();
    QString serializeSelectionToIni() const;
    void applyIniEditorChanges();
    void openSystemIniExternally() const;
    void createQuickObject(flatlas::domain::SolarObject::Type type,
                           const QString &suggestedNickname,
                           const QString &defaultArchetype = QString());
    void showNotYetPorted(const QString &featureName, const QString &v1Hint = QString()) const;
    void onCreateSun();
    void onCreateLightSource();
    void onCreateSurprise();
    void onCreatePatrolZone();
    void onCreateJumpConnection();
    void onCreatePlanet();
    void onCreateBuoy();
    void onCreateWeaponPlatform();
    void onCreateDepot();
    void onCreateTradeLane();
    void onCreateBase();
    void onCreateDockingRing();
    void onCreateAsteroidNebulaZone();
    void onCreateExclusionZone();
    void refreshObjectList();
    void onCanvasSelectionChanged(const QStringList &nicknames);
    void onTreeSelectionChanged();
    void onAddObject();
    void onDeleteSelected();
    void onDuplicateSelected();
    void onItemsMoved(const QHash<QString, QPointF> &oldPositions,
                      const QHash<QString, QPointF> &newPositions,
                      double verticalOffsetMeters);
    void onItemsMoveStarted(const QHash<QString, QPointF> &startScenePositions);
    void onItemsMoving(const QHash<QString, QPointF> &currentScenePositions,
                       double verticalOffsetMeters);
    void showObjectProperties(flatlas::domain::SolarObject *obj);
    void showZoneProperties(flatlas::domain::ZoneItem *zone);
    void syncTreeSelectionFromNicknames(const QStringList &nicknames);
    void syncSceneSelectionFromNicknames(const QStringList &nicknames);
    QString primarySelectedNickname() const;
    void pruneSelectionByCurrentFilter();
    bool isNicknameVisibleUnderCurrentFilter(const QString &nickname) const;
    bool isObjectVisibleUnderCurrentFilter(const flatlas::domain::SolarObject &obj) const;
    bool isZoneVisibleUnderCurrentFilter(const flatlas::domain::ZoneItem &zone) const;
    void applyObjectListSearchFilter();
    void refreshSidebarVisibilityState();
    void applySidebarFilteredStyle(QTreeWidgetItem *item, bool visible);
    void updateEditorModeUi();
    void rebuildMultiSelectionEditorList();
    QWidget *buildMultiSelectionRow(const QString &nickname, const QString &kindText);
    void removeNicknameFromSelection(const QString &nickname);
    bool eventFilter(QObject *watched, QEvent *event) override;
    void beginFieldZonePlacement(const CreateFieldZoneResult &request);
    void updateFieldZonePlacementPreview(const QPointF &currentScenePos);
    void finalizeFieldZonePlacement(const QPointF &edgeScenePos);
    void cancelFieldZonePlacement();
    void clearFieldZonePlacementPreview();
    void beginSimpleZonePlacement(const CreateSimpleZoneRequest &request);
    void updateSimpleZonePlacementPreview(const QPointF &currentScenePos);
    void finalizeSimpleZonePlacement(const QPointF &edgeScenePos);
    void cancelSimpleZonePlacement();
    void beginPatrolZonePlacement(const CreatePatrolZoneRequest &request);
    void updatePatrolZonePlacementPreview(const QPointF &currentScenePos);
    void finalizePatrolZonePlacement(const QPointF &endScenePos);
    void cancelPatrolZonePlacement();
    void beginBuoyPlacement(const CreateBuoyRequest &request);
    void updateBuoyPlacementPreview(const QPointF &currentScenePos);
    void finalizeBuoyPlacement(const QPointF &scenePos);
    void cancelBuoyPlacement();
    void beginTradeLanePlacement();
    void updateTradeLanePlacementPreview(const QPointF &currentScenePos);
    void finalizeTradeLanePlacement(const QPointF &endScenePos);
    void cancelTradeLanePlacement();
    void beginExclusionZonePlacement(const CreateExclusionZoneResult &request);
    void updateExclusionZonePlacementPreview(const QPointF &currentScenePos);
    void finalizeExclusionZonePlacement(const QPointF &edgeScenePos);
    void cancelExclusionZonePlacement();
    bool isFieldZone(const flatlas::domain::ZoneItem &zone) const;
    bool resolveLinkedFieldSectionForZone(const QString &zoneNickname,
                                          QString *outSectionName,
                                          QString *outFieldZoneNickname,
                                          QString *outRelativeFilePath,
                                          QString *outAbsoluteFilePath = nullptr) const;
    bool resolveLinkedFieldInfoForExclusion(const QString &exclusionZoneNickname,
                                           QString *outSectionName,
                                           QString *outFieldZoneNickname,
                                           QString *outRelativeFilePath,
                                           QString *outAbsoluteFilePath,
                                           QString *outCurrentText = nullptr,
                                           ExclusionShellSettings *outShellSettings = nullptr) const;
    QString pendingFieldIniText(const QString &relativeFilePath, const QString &absoluteFilePath) const;
    void stageFieldIniText(const QString &relativeFilePath,
                           const QString &absoluteFilePath,
                           const QString &content);
    void addLinkedFieldSection(const QString &sectionName,
                               const QString &zoneNickname,
                               const QString &relativeFilePath);
    void removeLinkedFieldSectionsForNicknames(const QStringList &zoneNicknames);
    bool writePendingGeneratedZoneFiles(QString *errorMessage);
    bool writePendingTextFiles(QString *errorMessage);
    void syncLightSourcesInScene();
    int findLightSourceSectionIndexByNickname(const QString &nickname) const;
    QStringList lightSourceNicknames() const;
    bool ensureSavedForCrossSystemOperation(const QString &actionTitle, const QString &actionDescription);

    std::unique_ptr<flatlas::domain::SystemDocument> m_document;
    flatlas::rendering::MapScene *m_mapScene = nullptr;
    flatlas::rendering::SystemMapView *m_mapView = nullptr;
    flatlas::rendering::SceneView3D *m_sceneView3D = nullptr;
    QWidget *m_sceneView3DHost = nullptr;
    QLabel *m_sceneView3DPlaceholder = nullptr;
    QToolBar *m_toolBar = nullptr;
    QSplitter *m_splitter = nullptr;
    QSplitter *m_leftSidebarSplitter = nullptr;
    QLineEdit *m_objectSearchEdit = nullptr;
    QLabel *m_objectSearchHintLabel = nullptr;
    QTreeWidget *m_objectTree = nullptr;
    QStackedWidget *m_editorStack = nullptr;
    QWidget *m_emptyEditorPage = nullptr;
    QLabel *m_emptyEditorLabel = nullptr;
    QWidget *m_singleEditorPage = nullptr;
    IniCodeEditor *m_iniEditor = nullptr;
    IniSyntaxHighlighter *m_iniEditorHighlighter = nullptr;
    QPushButton *m_applyIniButton = nullptr;
    QPushButton *m_openSystemIniButton = nullptr;
    QPushButton *m_preview3DButton = nullptr;
    QWidget *m_multiSelectionPage = nullptr;
    QLabel *m_multiSelectionLabel = nullptr;
    QScrollArea *m_multiSelectionScrollArea = nullptr;
    QWidget *m_multiSelectionListHost = nullptr;
    QVBoxLayout *m_multiSelectionListLayout = nullptr;
    QAction *m_toggle3DAction = nullptr;
    bool m_is3DViewEnabled = false;
    QStringList m_selectedNicknames;
    flatlas::rendering::SystemDisplayFilterSettings m_displayFilterSettings;
    QWidget *m_rightSidebar = nullptr;
    QLabel *m_selectionTitleLabel = nullptr;
    QLabel *m_selectionSubtitleLabel = nullptr;
    QPushButton *m_createObjectButton = nullptr;
    QPushButton *m_createAsteroidNebulaButton = nullptr;
    QPushButton *m_createZoneButton = nullptr;
    QPushButton *m_createPatrolZoneButton = nullptr;
    QPushButton *m_createJumpButton = nullptr;
    QPushButton *m_createSunButton = nullptr;
    QPushButton *m_createPlanetButton = nullptr;
    QPushButton *m_createLightButton = nullptr;
    QPushButton *m_createWreckButton = nullptr;
    QPushButton *m_createBuoyButton = nullptr;
    QPushButton *m_createWeaponPlatformButton = nullptr;
    QPushButton *m_createDepotButton = nullptr;
    QPushButton *m_createTradelaneButton = nullptr;
    QPushButton *m_createBaseButton = nullptr;
    QPushButton *m_createDockingRingButton = nullptr;
    QPushButton *m_createRingButton = nullptr;
    QPushButton *m_editTradelaneButton = nullptr;
    QPushButton *m_deleteSidebarButton = nullptr;
    QPushButton *m_editZonePopulationButton = nullptr;
    QPushButton *m_editRingButton = nullptr;
    QPushButton *m_addExclusionZoneButton = nullptr;
    QPushButton *m_editBaseButton = nullptr;
    QPushButton *m_baseBuilderButton = nullptr;
    QPushButton *m_systemSettingsButton = nullptr;
    QComboBox *m_objectJumpCombo = nullptr;
    QPushButton *m_objectJumpButton = nullptr;
    QLabel *m_systemFileInfoLabel = nullptr;
    QLabel *m_systemStatsLabel = nullptr;
    QPushButton *m_saveFileButton = nullptr;
    bool m_isShuttingDown = false;
    bool m_syncingSelection = false;

    // Live "Move Object" state: populated while the user drags a selection
    // in the 2D map. Drives the live pos= update in the ini editor and is
    // folded into the final MoveObjectCommand on drop.
    QHash<QString, QVector3D> m_liveMoveStartWorld;
    QHash<QString, QVector3D> m_liveMoveCurrentWorld;
    bool m_liveMoveActive = false;
    QHash<QString, PendingGeneratedZoneFile> m_pendingGeneratedZoneFiles;
    QHash<QString, PendingTextFileWrite> m_pendingTextFileWrites;
    std::unique_ptr<CreateFieldZoneResult> m_pendingFieldZoneRequest;
    bool m_pendingFieldZoneHasCenter = false;
    QPointF m_pendingFieldZoneCenterScenePos;
    QGraphicsEllipseItem *m_fieldZonePlacementPreview = nullptr;
    std::unique_ptr<CreateSimpleZoneRequest> m_pendingSimpleZoneRequest;
    bool m_pendingSimpleZoneHasCenter = false;
    QPointF m_pendingSimpleZoneCenterScenePos;
    QGraphicsEllipseItem *m_simpleZonePlacementPreview = nullptr;
    std::unique_ptr<CreatePatrolZoneRequest> m_pendingPatrolZoneRequest;
    bool m_pendingPatrolZoneHasStart = false;
    QPointF m_pendingPatrolZoneStartScenePos;
    bool m_pendingPatrolZoneHasEnd = false;
    QPointF m_pendingPatrolZoneEndScenePos;
    qreal m_pendingPatrolZoneHalfWidthScene = 0.0;
    int m_pendingPatrolZoneStep = 0;
    QGraphicsLineItem *m_patrolZonePlacementPreview = nullptr;
    QGraphicsPolygonItem *m_patrolZoneWidthPreview = nullptr;
    std::unique_ptr<CreateBuoyRequest> m_pendingBuoyRequest;
    bool m_pendingBuoyHasAnchor = false;
    QPointF m_pendingBuoyAnchorScenePos;
    int m_pendingBuoyStep = 0;
    QGraphicsLineItem *m_buoyLinePreview = nullptr;
    QGraphicsEllipseItem *m_buoyCirclePreview = nullptr;
    QVector<QGraphicsEllipseItem *> m_buoyMarkerPreviews;
    bool m_pendingTradeLaneHasStart = false;
    QPointF m_pendingTradeLaneStartScenePos;
    QGraphicsLineItem *m_tradeLanePlacementPreview = nullptr;
    std::unique_ptr<CreateExclusionZoneResult> m_pendingExclusionZoneRequest;
    bool m_pendingExclusionZoneHasCenter = false;
    QPointF m_pendingExclusionZoneCenterScenePos;
    QGraphicsEllipseItem *m_exclusionZonePlacementPreview = nullptr;
};

} // namespace flatlas::editors
