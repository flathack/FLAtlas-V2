#pragma once
// editors/modmanager/ModManagerPage.h – Mod-Manager UI (Phase 13 + Editing Context)

#include <QWidget>
#include <QVector>
#include "ConflictDetector.h"
#include "ModWorkflow.h"

class QListWidget;
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
    void onLaunchFlClicked();

    QString m_modsDir;
    QVector<ModInfo> m_mods;
    QVector<FileConflict> m_conflicts;
    ConflictDetector m_detector;
    ModWorkflow m_workflow;

    QToolBar *m_toolBar = nullptr;
    QTableWidget *m_profileTable = nullptr;
    QTableWidget *m_conflictTable = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_editCtxBtn = nullptr;
    QPushButton *m_clearCtxBtn = nullptr;
};

} // namespace flatlas::editors
