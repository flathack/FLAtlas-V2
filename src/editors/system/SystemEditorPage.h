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
    QTreeWidget *m_objectTree = nullptr;
    QAction *m_toggle3DAction = nullptr;
    bool m_is3DViewEnabled = false;
    flatlas::rendering::SystemDisplayFilterSettings m_displayFilterSettings;
};

} // namespace flatlas::editors
