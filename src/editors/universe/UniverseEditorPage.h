#pragma once
// editors/universe/UniverseEditorPage.h – Universe-Editor (Phase 9)

#include <QWidget>
#include <memory>

namespace flatlas::domain { class UniverseData; struct SystemInfo; }
class QGraphicsScene;
class QGraphicsView;
class QToolBar;
class QSplitter;
class QTreeWidget;
class QTreeWidgetItem;

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
    void openSystemRequested(const QString &systemNickname, const QString &systemFilePath);

private:
    void setupUi();
    void setupToolBar();
    void refreshSystemList();
    void refreshMap();
    void drawConnections();
    void onSystemSelected(QTreeWidgetItem *item, int column);
    void onSystemDoubleClicked(QTreeWidgetItem *item, int column);
    void onMapItemDoubleClicked(const QString &nickname);
    void onAddSystem();
    void onDeleteSystem();

    std::unique_ptr<flatlas::domain::UniverseData> m_data;
    QString m_filePath;

    QSplitter *m_splitter = nullptr;
    QToolBar *m_toolBar = nullptr;
    QTreeWidget *m_systemTree = nullptr;
    QGraphicsScene *m_mapScene = nullptr;
    QGraphicsView *m_mapView = nullptr;
};

} // namespace flatlas::editors
