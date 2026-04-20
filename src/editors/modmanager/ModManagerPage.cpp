// editors/modmanager/ModManagerPage.cpp – Mod-Manager UI (Phase 13 + Editing Context)

#include "ModManagerPage.h"
#include "core/EditingContext.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QToolBar>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QProcess>
#include <QFileInfo>
#include <QFileIconProvider>

namespace flatlas::editors {

ModManagerPage::ModManagerPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    refreshProfileTable();

    connect(&flatlas::core::EditingContext::instance(),
            &flatlas::core::EditingContext::contextChanged,
            this, [this]() { refreshProfileTable(); });
    connect(&flatlas::core::EditingContext::instance(),
            &flatlas::core::EditingContext::profilesChanged,
            this, [this]() { refreshProfileTable(); });
}

void ModManagerPage::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setupToolBar();
    layout->addWidget(m_toolBar);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    // Left side: Sidebar with operations
    auto *sidebar = new QWidget(this);
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(8, 8, 8, 8);
    sidebarLayout->setSpacing(6);

    auto *addLabel = new QLabel(tr("<b>Add Installation</b>"), this);
    sidebarLayout->addWidget(addLabel);

    auto *addDirectBtn = new QPushButton(tr("Add Direct Installation..."), this);
    connect(addDirectBtn, &QPushButton::clicked, this, &ModManagerPage::onAddDirectClicked);
    sidebarLayout->addWidget(addDirectBtn);

    sidebarLayout->addSpacing(12);
    auto *editLabel = new QLabel(tr("<b>Editing Context</b>"), this);
    sidebarLayout->addWidget(editLabel);

    m_editCtxBtn = new QPushButton(tr("Open for Editing"), this);
    m_editCtxBtn->setStyleSheet(
        QStringLiteral("QPushButton { background: #1a6630; color: white; padding: 6px; border-radius: 3px; }"
                        "QPushButton:hover { background: #22883e; }"));
    connect(m_editCtxBtn, &QPushButton::clicked, this, &ModManagerPage::onEditContextClicked);
    sidebarLayout->addWidget(m_editCtxBtn);

    m_clearCtxBtn = new QPushButton(tr("Clear Editing Context"), this);
    connect(m_clearCtxBtn, &QPushButton::clicked, this, &ModManagerPage::onClearContextClicked);
    sidebarLayout->addWidget(m_clearCtxBtn);

    sidebarLayout->addSpacing(12);
    auto *actionsLabel = new QLabel(tr("<b>Actions</b>"), this);
    sidebarLayout->addWidget(actionsLabel);

    auto *removeBtn = new QPushButton(tr("Remove Installation"), this);
    connect(removeBtn, &QPushButton::clicked, this, &ModManagerPage::onRemoveClicked);
    sidebarLayout->addWidget(removeBtn);

    auto *scanBtn = new QPushButton(tr("Scan Mods..."), this);
    connect(scanBtn, &QPushButton::clicked, this, &ModManagerPage::onScanClicked);
    sidebarLayout->addWidget(scanBtn);

    sidebarLayout->addSpacing(12);
    auto *runLabel = new QLabel(tr("<b>Run</b>"), this);
    sidebarLayout->addWidget(runLabel);

    auto *launchBtn = new QPushButton(tr("Launch Freelancer"), this);
    launchBtn->setStyleSheet(
        QStringLiteral("QPushButton { background: #28a745; color: white; padding: 6px; border-radius: 3px; }"
                        "QPushButton:hover { background: #2fc553; }"));
    connect(launchBtn, &QPushButton::clicked, this, &ModManagerPage::onLaunchFlClicked);
    sidebarLayout->addWidget(launchBtn);

    sidebarLayout->addStretch();
    sidebar->setFixedWidth(220);
    splitter->addWidget(sidebar);

    // Right side: Profile table + Conflict table (stacked)
    auto *rightSide = new QSplitter(Qt::Vertical, this);

    m_profileTable = new QTableWidget(this);
    m_profileTable->setColumnCount(4);
    m_profileTable->setHorizontalHeaderLabels({tr("Name"), tr("Type"), tr("Source Path"), tr("Status")});
    m_profileTable->horizontalHeader()->setStretchLastSection(true);
    m_profileTable->setAlternatingRowColors(true);
    m_profileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_profileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_profileTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_profileTable->verticalHeader()->setVisible(false);
    rightSide->addWidget(m_profileTable);

    m_conflictTable = new QTableWidget(this);
    m_conflictTable->setColumnCount(2);
    m_conflictTable->setHorizontalHeaderLabels({tr("File"), tr("Conflicting Mods")});
    m_conflictTable->horizontalHeader()->setStretchLastSection(true);
    m_conflictTable->setAlternatingRowColors(true);
    m_conflictTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_conflictTable->verticalHeader()->setVisible(false);
    rightSide->addWidget(m_conflictTable);

    rightSide->setStretchFactor(0, 3);
    rightSide->setStretchFactor(1, 1);
    splitter->addWidget(rightSide);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter);

    m_statusLabel = new QLabel(tr("Add a Freelancer installation to begin"), this);
    m_statusLabel->setContentsMargins(8, 4, 8, 4);
    layout->addWidget(m_statusLabel);
}

void ModManagerPage::setupToolBar()
{
    m_toolBar = new QToolBar(this);
    m_toolBar->setMovable(false);

    m_toolBar->addAction(tr("Add Direct..."), this, &ModManagerPage::onAddDirectClicked);
    m_toolBar->addSeparator();
    m_toolBar->addAction(tr("Activate"), this, &ModManagerPage::onActivateClicked);
    m_toolBar->addAction(tr("Deactivate"), this, &ModManagerPage::onDeactivateClicked);
}

void ModManagerPage::refreshProfileTable()
{
    auto &ctx = flatlas::core::EditingContext::instance();
    const auto &profiles = ctx.profiles();
    const QString editingId = ctx.editingProfileId();

    m_profileTable->setRowCount(profiles.size());
    QFileIconProvider iconProvider;
    for (int i = 0; i < profiles.size(); ++i) {
        const auto &p = profiles[i];

        auto *nameItem = new QTableWidgetItem(p.name);
        nameItem->setData(Qt::UserRole, p.id);
        if (p.id == editingId) {
            nameItem->setText(QStringLiteral("\u2714 ") + p.name);
        }

        // Try to find Freelancer.exe and use its icon
        QDir dir(p.sourcePath());
        QString exePath;
        if (dir.exists(QStringLiteral("EXE/Freelancer.exe")))
            exePath = dir.filePath(QStringLiteral("EXE/Freelancer.exe"));
        else if (dir.exists(QStringLiteral("Freelancer.exe")))
            exePath = dir.filePath(QStringLiteral("Freelancer.exe"));
        if (!exePath.isEmpty())
            nameItem->setIcon(iconProvider.icon(QFileInfo(exePath)));

        m_profileTable->setItem(i, 0, nameItem);
        m_profileTable->setItem(i, 1, new QTableWidgetItem(p.mode));
        m_profileTable->setItem(i, 2, new QTableWidgetItem(p.sourcePath()));

        QString status;
        if (p.id == editingId)
            status = tr("Editing");
        else if (QDir(p.sourcePath()).exists())
            status = tr("Ready");
        else
            status = tr("Path Missing");
        m_profileTable->setItem(i, 3, new QTableWidgetItem(status));
    }
    m_profileTable->resizeColumnsToContents();

    m_statusLabel->setText(tr("%1 installations").arg(profiles.size()));
    emit titleChanged(QStringLiteral("Mod Manager (%1)").arg(profiles.size()));
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

void ModManagerPage::onAddDirectClicked()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Freelancer Installation Directory"));
    if (dir.isEmpty())
        return;

    // Derive name from directory name
    QString name = QDir(dir).dirName();
    if (name.isEmpty())
        name = dir;

    flatlas::core::ModProfile profile;
    profile.name = name;
    profile.mode = QStringLiteral("direct");
    profile.directPath = dir;

    flatlas::core::EditingContext::instance().addProfile(profile);
    m_statusLabel->setText(tr("Added: %1").arg(name));
}

void ModManagerPage::onRemoveClicked()
{
    int row = m_profileTable->currentRow();
    if (row < 0) return;
    auto *item = m_profileTable->item(row, 0);
    if (!item) return;

    QString profileId = item->data(Qt::UserRole).toString();
    QString name = item->text();

    auto answer = QMessageBox::question(
        this, tr("Remove Installation"),
        tr("Remove \"%1\" from the list?\n\n"
           "(This does not delete any files on disk.)")
            .arg(name));
    if (answer == QMessageBox::Yes)
        flatlas::core::EditingContext::instance().removeProfile(profileId);
}

void ModManagerPage::onEditContextClicked()
{
    int row = m_profileTable->currentRow();
    if (row < 0) {
        QMessageBox::information(this, tr("Mod Manager"),
            tr("Select an installation in the table first."));
        return;
    }
    auto *item = m_profileTable->item(row, 0);
    if (!item) return;

    QString profileId = item->data(Qt::UserRole).toString();
    if (!flatlas::core::EditingContext::instance().setEditingProfile(profileId)) {
        QMessageBox::warning(this, tr("Mod Manager"),
            tr("Could not set editing context. Path may be missing."));
    }
}

void ModManagerPage::onClearContextClicked()
{
    flatlas::core::EditingContext::instance().clearEditingProfile();
}

void ModManagerPage::setModsDir(const QString &modsDir)
{
    m_modsDir = modsDir;
    m_mods = m_detector.scanMods(modsDir);
    refreshConflicts();
}

void ModManagerPage::setGamePath(const QString &gamePath)
{
    m_workflow.setGamePath(gamePath);
    m_workflow.setBackupPath(gamePath + QStringLiteral("/_flatlas_backup"));
}

void ModManagerPage::onScanClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Mods Directory"));
    if (!dir.isEmpty())
        setModsDir(dir);
}

void ModManagerPage::onActivateClicked()
{
    // Use profile table for selection context
    m_statusLabel->setText(tr("Activate: select mods from scan results"));
}

void ModManagerPage::onDeactivateClicked()
{
    m_statusLabel->setText(tr("Deactivate: select mods from scan results"));
}

void ModManagerPage::onLaunchFlClicked()
{
    auto &ctx = flatlas::core::EditingContext::instance();
    QString gamePath = ctx.primaryGamePath();
    if (gamePath.isEmpty()) {
        QMessageBox::information(this, tr("Mod Manager"),
            tr("No editing context set. Select an installation first."));
        return;
    }

    // Look for Freelancer.exe in the game path
    QDir dir(gamePath);
    QString exe;
    if (dir.exists(QStringLiteral("EXE/Freelancer.exe")))
        exe = dir.filePath(QStringLiteral("EXE/Freelancer.exe"));
    else if (dir.exists(QStringLiteral("Freelancer.exe")))
        exe = dir.filePath(QStringLiteral("Freelancer.exe"));

    if (exe.isEmpty()) {
        exe = QFileDialog::getOpenFileName(
            this, tr("Select Freelancer.exe"), gamePath, tr("Executable (*.exe)"));
        if (exe.isEmpty()) return;
    }

    QProcess::startDetached(exe, {}, QFileInfo(exe).absolutePath());
    m_statusLabel->setText(tr("Freelancer launched"));
}

} // namespace flatlas::editors
