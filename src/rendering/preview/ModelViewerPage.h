#pragma once
// rendering/preview/ModelViewerPage.h - standalone 3D model viewer page built on the reusable 3D viewport

#include "infrastructure/freelancer/ModelAssetScanner.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QCheckBox;
class QVBoxLayout;

namespace flatlas::rendering {

class ModelPreview;
class ModelViewport3D;

class ModelViewerPage : public QWidget {
    Q_OBJECT
public:
    explicit ModelViewerPage(QWidget *parent = nullptr);

private:
    const flatlas::infrastructure::ModelAssetEntry *findEntryByModelPath(const QString &modelPath) const;
    bool ensureViewport();
    void rebuildEntries();
    void rebuildTree();
    void setCurrentEntry(const flatlas::infrastructure::ModelAssetEntry *entry);
    void loadEntryIntoViewport(const flatlas::infrastructure::ModelAssetEntry *entry);
    void loadArbitraryModel();
    void updateButtons();
    void updateDetails();

    QVector<flatlas::infrastructure::ModelAssetEntry> m_entries;
    const flatlas::infrastructure::ModelAssetEntry *m_currentEntry = nullptr;

    QLabel *m_titleLabel = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QTreeWidget *m_tree = nullptr;
    QWidget *m_viewportHost = nullptr;
    QVBoxLayout *m_viewportHostLayout = nullptr;
    QLabel *m_viewportPlaceholder = nullptr;
    ModelViewport3D *m_viewport = nullptr;
    QLabel *m_fileLabel = nullptr;
    QLabel *m_detailsLabel = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_loadFileButton = nullptr;
    QPushButton *m_resetCameraButton = nullptr;
    QPushButton *m_openSourceIniButton = nullptr;
    QPushButton *m_openModelFileButton = nullptr;
    QPushButton *m_openSeparatePreviewButton = nullptr;
    QCheckBox *m_meshCheck = nullptr;
    QCheckBox *m_wireframeCheck = nullptr;
    QCheckBox *m_boundsCheck = nullptr;
    QCheckBox *m_whiteBgCheck = nullptr;
};

} // namespace flatlas::rendering
