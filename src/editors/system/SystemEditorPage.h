#pragma once
// editors/system/SystemEditorPage.h – System-Editor (Phase 5)

#include <QWidget>
#include <QHash>
#include <QPointF>
#include <QStringList>
#include "rendering/view2d/SystemDisplayFilter.h"
#include <memory>

namespace flatlas::domain { class SystemDocument; class SolarObject; class ZoneItem; }
namespace flatlas::rendering { class MapScene; class SystemMapView; class SceneView3D; }
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
    void loadDocumentIntoUi();
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
    void onCreatePlanet();
    void onCreateBuoy();
    void onCreateWeaponPlatform();
    void onCreateDepot();
    void onCreateTradeLane();
    void onCreateBase();
    void onCreateDockingRing();
    void refreshObjectList();
    void onCanvasSelectionChanged(const QStringList &nicknames);
    void onTreeSelectionChanged();
    void onAddObject();
    void onDeleteSelected();
    void onDuplicateSelected();
    void onItemsMoved(const QHash<QString, QPointF> &oldPositions,
                      const QHash<QString, QPointF> &newPositions);
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
};

} // namespace flatlas::editors
