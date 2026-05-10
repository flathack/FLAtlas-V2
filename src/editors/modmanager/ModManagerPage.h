#pragma once
// editors/modmanager/ModManagerPage.h – Mod-Manager UI (Phase 13 + Editing Context)

#include <QWidget>
#include <QVector>
#include "ConflictDetector.h"
#include "ModWorkflow.h"

class QListWidget;
class QCheckBox;
class QTabWidget;
class QToolBar;
class QLabel;
class QTableWidget;
class QPushButton;

namespace flatlas::editors {

class ModManagerPage : public QWidget {
    Q_OBJECT
public:
    explicit ModManagerPage(QWidget *parent = nullptr);

    /// Set the mods directory and scan.
    void setModsDir(const QString &modsDir);

    /// Set the game installation path.
    void setGamePath(const QString &gamePath);

    /// Get detected mods.
    const QVector<ModInfo> &mods() const { return m_mods; }

signals:
    void titleChanged(const QString &title);

private:
    void setupUi();
    void setupToolBar();
    void refreshProfileTable();
    void refreshConflicts();
    void onAddDirectClicked();
    void onRemoveClicked();
    void onEditContextClicked();
    void onClearContextClicked();
    void onActivateClicked();
    void onDeactivateClicked();
    void onScanClicked();
    void onExportClicked();
    void onLaunchFlClicked();
    void onInstallGitClicked();
    void onInitializeGitClicked();
    void showGitDiffForCurrentChange();
    void refreshGitPanel();
    bool shouldRefreshGitPanel() const;
    QString selectedProfileSourcePath() const;
    QString suggestedReferencePath(const QString &modRoot) const;

    QString m_modsDir;
    QVector<ModInfo> m_mods;
    QVector<FileConflict> m_conflicts;
    ConflictDetector m_detector;
    ModWorkflow m_workflow;

    QToolBar *m_toolBar = nullptr;
    QTableWidget *m_profileTable = nullptr;
    QTableWidget *m_conflictTable = nullptr;
    QLabel *m_statusLabel = nullptr;
    QCheckBox *m_gitSupportCheck = nullptr;
    QLabel *m_gitStatusLabel = nullptr;
    QTabWidget *m_gitTabs = nullptr;
    QListWidget *m_gitChangesList = nullptr;
    QListWidget *m_gitCommitList = nullptr;
    QPushButton *m_gitInstallBtn = nullptr;
    QPushButton *m_gitInitBtn = nullptr;
    QPushButton *m_editCtxBtn = nullptr;
    QPushButton *m_clearCtxBtn = nullptr;
    int m_gitRefreshGeneration = 0;
    qint64 m_lastGitRefreshMs = 0;
    QString m_lastGitRefreshPath;
};

} // namespace flatlas::editors
