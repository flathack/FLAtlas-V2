// editors/modmanager/ModManagerPage.cpp – Mod-Manager UI (Phase 13)

#include "ModManagerPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QToolBar>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>

namespace flatlas::editors {

ModManagerPage::ModManagerPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void ModManagerPage::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setupToolBar();
    layout->addWidget(m_toolBar);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    // Left: Mod list
    m_modList = new QListWidget(this);
    m_modList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    splitter->addWidget(m_modList);

    // Right: Conflict table
    m_conflictTable = new QTableWidget(this);
    m_conflictTable->setColumnCount(2);
    m_conflictTable->setHorizontalHeaderLabels({tr("File"), tr("Conflicting Mods")});
    m_conflictTable->horizontalHeader()->setStretchLastSection(true);
    m_conflictTable->setAlternatingRowColors(true);
    m_conflictTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_conflictTable->verticalHeader()->setVisible(false);
    splitter->addWidget(m_conflictTable);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter);

    m_statusLabel = new QLabel(tr("Set mods directory to begin"), this);
    layout->addWidget(m_statusLabel);
}

void ModManagerPage::setupToolBar()
{
    m_toolBar = new QToolBar(this);
    m_toolBar->setMovable(false);

    m_toolBar->addAction(tr("Scan Mods..."), this, &ModManagerPage::onScanClicked);
    m_toolBar->addSeparator();
    m_toolBar->addAction(tr("Activate"), this, &ModManagerPage::onActivateClicked);
    m_toolBar->addAction(tr("Deactivate"), this, &ModManagerPage::onDeactivateClicked);
}

void ModManagerPage::setModsDir(const QString &modsDir)
{
    m_modsDir = modsDir;
    m_mods = m_detector.scanMods(modsDir);
    refreshModList();
    refreshConflicts();
}

void ModManagerPage::setGamePath(const QString &gamePath)
{
    m_workflow.setGamePath(gamePath);
    m_workflow.setBackupPath(gamePath + QStringLiteral("/_flatlas_backup"));
}

void ModManagerPage::refreshModList()
{
    m_modList->clear();
    for (const auto &mod : m_mods) {
        m_modList->addItem(QStringLiteral("%1 (%2 files)").arg(mod.name).arg(mod.files.size()));
    }

    m_statusLabel->setText(tr("Found %1 mods").arg(m_mods.size()));
    emit titleChanged(QStringLiteral("Mod Manager (%1)").arg(m_mods.size()));
}

void ModManagerPage::refreshConflicts()
{
    m_conflicts = m_detector.detectConflicts(m_mods);

    m_conflictTable->setRowCount(m_conflicts.size());
    for (int i = 0; i < m_conflicts.size(); ++i) {
        m_conflictTable->setItem(i, 0,
            new QTableWidgetItem(m_conflicts[i].relativePath));
        m_conflictTable->setItem(i, 1,
            new QTableWidgetItem(m_conflicts[i].modNames.join(QStringLiteral(", "))));
    }
    m_conflictTable->resizeColumnsToContents();
}

void ModManagerPage::onScanClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Mods Directory"));
    if (!dir.isEmpty())
        setModsDir(dir);
}

void ModManagerPage::onActivateClicked()
{
    auto selected = m_modList->selectionModel()->selectedRows();
    for (const auto &idx : selected) {
        int row = idx.row();
        if (row >= 0 && row < m_mods.size()) {
            auto copied = m_workflow.activateMod(m_mods[row]);
            if (copied.size() == m_mods[row].files.size()) {
                m_statusLabel->setText(tr("Activated: %1 (%2 files)")
                    .arg(m_mods[row].name).arg(copied.size()));
            } else {
                m_statusLabel->setText(tr("Error activating %1: %2")
                    .arg(m_mods[row].name, m_workflow.lastError()));
            }
        }
    }
}

void ModManagerPage::onDeactivateClicked()
{
    auto selected = m_modList->selectionModel()->selectedRows();
    for (const auto &idx : selected) {
        int row = idx.row();
        if (row >= 0 && row < m_mods.size()) {
            auto restored = m_workflow.deactivateMod(m_mods[row]);
            m_statusLabel->setText(tr("Deactivated: %1 (%2 files restored)")
                .arg(m_mods[row].name).arg(restored.size()));
        }
    }
}

} // namespace flatlas::editors
