#pragma once
// editors/universe/UniverseEditorPage.h – Universe-Editor (Phase 9)

#include <QWidget>
#include <memory>

namespace flatlas::domain { class UniverseData; struct SystemInfo; }
namespace flatlas::infrastructure { class IdsStringTable; }
class QGraphicsScene;
class QGraphicsView;
class QGraphicsItem;
class QGraphicsLineItem;
class QToolBar;
class QSplitter;
class QTreeWidget;
class QTreeWidgetItem;
class QAction;
class QTabBar;

namespace flatlas::editors {

class UniverseEditorPage : public QWidget {
    Q_OBJECT
public:
    explicit UniverseEditorPage(QWidget *parent = nullptr);
    ~UniverseEditorPage() override;

    /// Load Universe.ini.
    bool loadFile(const QString &filePath);

    /// Save to current file path.
    bool save();

    /// Save to a new file path.
    bool saveAs(const QString &filePath);

    flatlas::domain::UniverseData *data() const;
    QString filePath() const { return m_filePath; }

signals:
    void titleChanged(const QString &title);
    void openSystemRequested(const QString &systemNickname,
                             const QString &systemFilePath,
                             const QString &ingameName);

private:
    void setupUi();
    void refreshSystemList();
    void refreshMap();
    void drawConnections();
    void onSystemSelected(QTreeWidgetItem *item, int column);
    void onSystemDoubleClicked(QTreeWidgetItem *item, int column);
    void onMapItemDoubleClicked(const QString &nickname);
    void onAddSystem();
    void onDeleteSystem();
    void onNodeSelected(const QString &nickname);
    void onNodeMoved(const QString &nickname);
    void onNodeMoveFinished(const QString &nickname);

    /// Hervorhebung eines Pfades auf der Karte.
    void highlightPath(const QStringList &systemPath);
    void clearPathHighlight();
    void onFindShortestPath();
    QString resolveSystemPath(const QString &relativePath) const;
    void clearConnectionLines();
    QString externalSectorForSystem(const flatlas::domain::SystemInfo &sys) const;
    QPointF connectionEdgePoint(const QPointF &from, const QPointF &towards, const QRectF &bounds) const;
    void syncSystemPositionFromMap(const QString &nickname);
    bool systemVisibleInActiveSector(const flatlas::domain::SystemInfo &sys) const;
    QPointF scenePositionForSystem(const flatlas::domain::SystemInfo &sys) const;
    QString sectorDisplayName(const QString &sectorKey) const;
    void rebuildSectorTabs();
    void applySector(const QString &sectorKey);
    void applyThemeStyling();
    QRectF mapContentRect() const;
    void fitMapInView();
    void setMoveEnabled(bool enabled);
    void reloadIdsStrings();
    QString resolvedSystemName(const flatlas::domain::SystemInfo &sys) const;
    QString mapLabelForSystem(const flatlas::domain::SystemInfo &sys) const;
    QString listLabelForSystem(const flatlas::domain::SystemInfo &sys) const;
    void refreshTitle();
    void setDirty(bool dirty);

    std::unique_ptr<flatlas::domain::UniverseData> m_data;
    QString m_filePath;
    bool m_dirty = false;
    bool m_moveEnabled = false;
    int m_pendingInitialFitPasses = 0;
    double m_mapScale = 1.0;

    QSplitter *m_splitter = nullptr;
    QToolBar *m_toolBar = nullptr;
    QToolBar *m_mapToolBar = nullptr;
    QAction *m_moveAction = nullptr;
    QTabBar *m_sectorTabs = nullptr;
    QTreeWidget *m_systemTree = nullptr;
    QGraphicsScene *m_mapScene = nullptr;
    QGraphicsView *m_mapView = nullptr;
    QVector<QGraphicsLineItem *> m_connectionItems;
    QVector<QGraphicsItem *> m_externalConnectionItems;
    QVector<QGraphicsItem *> m_pathHighlightItems;
    QStringList m_highlightedPath;
    QString m_activeSector = QStringLiteral("universe");
    std::unique_ptr<flatlas::infrastructure::IdsStringTable> m_idsStrings;
};

} // namespace flatlas::editors
