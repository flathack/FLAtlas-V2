#pragma once
// editors/system/SystemEditorPage.h – System-Editor (Phase 5)

#include <QWidget>
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
class QGroupBox;

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

signals:
    void titleChanged(const QString &title);

private:
    void setupUi();
    void setupToolBar();
    void setupObjectList();
    void connectSignals();
    void ensureSceneView3D();
    void set3DViewEnabled(bool enabled);
    void openDisplayFilterDialog();
    void loadDisplayFilterSettings();
    void saveDisplayFilterSettings() const;
    QString displayFilterConfigKey() const;
    void setupRightSidebar();
    void updateSelectionSummary(const QString &nickname = QString());
    void updateSidebarButtons();
    void refreshObjectJumpList();
    void jumpToSelectedFromSidebar();
    void updateIniEditorForSelection(const QString &nickname = QString());
    QString serializeSelectionToIni(const QString &nickname) const;
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
    void onObjectSelected(const QString &nickname);
    void onAddObject();
    void onDeleteSelected();
    void onDuplicateSelected();
    void showObjectProperties(flatlas::domain::SolarObject *obj);
    void showZoneProperties(flatlas::domain::ZoneItem *zone);

    std::unique_ptr<flatlas::domain::SystemDocument> m_document;
    flatlas::rendering::MapScene *m_mapScene = nullptr;
    flatlas::rendering::SystemMapView *m_mapView = nullptr;
    flatlas::rendering::SceneView3D *m_sceneView3D = nullptr;
    QWidget *m_sceneView3DHost = nullptr;
    QLabel *m_sceneView3DPlaceholder = nullptr;
    QToolBar *m_toolBar = nullptr;
    QSplitter *m_splitter = nullptr;
    QSplitter *m_leftSidebarSplitter = nullptr;
    QTreeWidget *m_objectTree = nullptr;
    IniCodeEditor *m_iniEditor = nullptr;
    IniSyntaxHighlighter *m_iniEditorHighlighter = nullptr;
    QPushButton *m_applyIniButton = nullptr;
    QPushButton *m_openSystemIniButton = nullptr;
    QAction *m_toggle3DAction = nullptr;
    bool m_is3DViewEnabled = false;
    QString m_selectedNickname;
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
};

} // namespace flatlas::editors
