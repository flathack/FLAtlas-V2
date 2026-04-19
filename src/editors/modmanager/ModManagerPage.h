#pragma once
// editors/modmanager/ModManagerPage.h – Mod-Manager UI (Phase 13)

#include <QWidget>
#include <QVector>
#include "ConflictDetector.h"
#include "ModWorkflow.h"

class QListWidget;
class QToolBar;
class QLabel;
class QTableWidget;

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
    void refreshModList();
    void refreshConflicts();
    void onActivateClicked();
    void onDeactivateClicked();
    void onScanClicked();

    QString m_modsDir;
    QVector<ModInfo> m_mods;
    QVector<FileConflict> m_conflicts;
    ConflictDetector m_detector;
    ModWorkflow m_workflow;

    QToolBar *m_toolBar = nullptr;
    QListWidget *m_modList = nullptr;
    QTableWidget *m_conflictTable = nullptr;
    QLabel *m_statusLabel = nullptr;
};

} // namespace flatlas::editors
