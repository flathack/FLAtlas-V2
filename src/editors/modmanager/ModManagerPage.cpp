// editors/modmanager/ModManagerPage.cpp – Mod-Manager UI (Phase 13 + Editing Context)

#include "ModManagerPage.h"
#include "ModExportDialog.h"
#include "core/Config.h"
#include "core/EditingContext.h"
#include "core/GitSupport.h"
#include "core/PathUtils.h"

#include <QDesktopServices>
#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFutureWatcher>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QToolBar>
#include <QGroupBox>
#include <QFrame>
#include <QListWidget>
#include <QTabWidget>
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
#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent>

namespace {

struct GitPanelData
{
    flatlas::core::GitStatus status;
    QVector<flatlas::core::GitCommitInfo> commits;
};

class GitDiffHighlighter final : public QSyntaxHighlighter
{
public:
    explicit GitDiffHighlighter(QTextDocument *document)
        : QSyntaxHighlighter(document)
    {
        m_addedFormat.setForeground(QColor(92, 214, 118));
        m_deletedFormat.setForeground(QColor(255, 118, 118));
        m_hunkFormat.setForeground(QColor(110, 170, 255));
        m_hunkFormat.setFontWeight(QFont::DemiBold);
        m_headerFormat.setForeground(QColor(180, 190, 205));
        m_headerFormat.setFontWeight(QFont::DemiBold);
    }

protected:
    void highlightBlock(const QString &text) override
    {
        if (text.startsWith(QStringLiteral("@@"))) {
            setFormat(0, text.length(), m_hunkFormat);
        } else if (text.startsWith(QStringLiteral("+++")) || text.startsWith(QStringLiteral("---"))) {
            setFormat(0, text.length(), m_headerFormat);
        } else if (text.startsWith(QLatin1Char('+'))) {
            setFormat(0, text.length(), m_addedFormat);
        } else if (text.startsWith(QLatin1Char('-'))) {
            setFormat(0, text.length(), m_deletedFormat);
        } else if (text.startsWith(QStringLiteral("diff --git")) || text.startsWith(QStringLiteral("index "))) {
            setFormat(0, text.length(), m_headerFormat);
        }
    }

private:
    QTextCharFormat m_addedFormat;
    QTextCharFormat m_deletedFormat;
    QTextCharFormat m_hunkFormat;
    QTextCharFormat m_headerFormat;
};

QWidget *createGitRow(const QString &title, const QString &subtitle, QWidget *parent)
{
    auto *row = new QFrame(parent);
    row->setObjectName(QStringLiteral("gitListRow"));
    auto *layout = new QVBoxLayout(row);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(2);

    auto *titleLabel = new QLabel(title, row);
    titleLabel->setObjectName(QStringLiteral("gitListRowTitle"));
    titleLabel->setWordWrap(false);
    layout->addWidget(titleLabel);

    auto *subtitleLabel = new QLabel(subtitle, row);
    subtitleLabel->setObjectName(QStringLiteral("gitListRowSubtitle"));
    subtitleLabel->setWordWrap(false);
    layout->addWidget(subtitleLabel);

    return row;
}

QListWidgetItem *addGitRow(QListWidget *list, QWidget *row)
{
    auto *item = new QListWidgetItem(list);
    item->setSizeHint(QSize(0, 54));
    list->addItem(item);
    list->setItemWidget(item, row);
    return item;
}

} // namespace

namespace flatlas::editors {

ModManagerPage::ModManagerPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    refreshProfileTable();

    connect(&flatlas::core::EditingContext::instance(),
            &flatlas::core::EditingContext::contextChanged,
            this, [this]() {
        refreshProfileTable();
        refreshGitPanel();
    });
    connect(&flatlas::core::EditingContext::instance(),
            &flatlas::core::EditingContext::profilesChanged,
            this, [this]() {
        refreshProfileTable();
        refreshGitPanel();
    });
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive && isVisible())
            refreshGitPanel();
    });
    auto *gitRefreshTimer = new QTimer(this);
    gitRefreshTimer->setInterval(30000);
    connect(gitRefreshTimer, &QTimer::timeout, this, [this]() {
        if (qApp->applicationState() == Qt::ApplicationActive && isVisible())
            refreshGitPanel();
    });
    gitRefreshTimer->start();
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

    auto *exportBtn = new QPushButton(tr("Mod exportieren"), this);
    connect(exportBtn, &QPushButton::clicked, this, &ModManagerPage::onExportClicked);
    sidebarLayout->addWidget(exportBtn);

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

    auto *profilePanel = new QWidget(this);
    auto *profileLayout = new QVBoxLayout(profilePanel);
    profileLayout->setContentsMargins(0, 0, 0, 0);
    profileLayout->setSpacing(6);

    m_profileTable = new QTableWidget(profilePanel);
    m_profileTable->setColumnCount(4);
    m_profileTable->setHorizontalHeaderLabels({tr("Name"), tr("Type"), tr("Source Path"), tr("Status")});
    m_profileTable->horizontalHeader()->setStretchLastSection(true);
    m_profileTable->setAlternatingRowColors(true);
    m_profileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_profileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_profileTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_profileTable->verticalHeader()->setVisible(false);
    connect(m_profileTable, &QTableWidget::itemSelectionChanged, this, &ModManagerPage::refreshGitPanel);
    m_profileTable->setMinimumHeight(170);
    profileLayout->addWidget(m_profileTable, 0);

    auto *gitGroup = new QGroupBox(tr("Git"), profilePanel);
    gitGroup->setStyleSheet(QStringLiteral(
        "QGroupBox { font-weight: 600; border: 1px solid palette(mid); border-radius: 6px; margin-top: 8px; padding-top: 10px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
        "QListWidget { border: none; outline: 0; }"
        "QListWidget::item { border-bottom: 1px solid palette(mid); }"
        "QFrame#gitListRow { background: palette(base); border: none; }"
        "QLabel#gitListRowTitle { font-weight: 700; }"
        "QLabel#gitListRowSubtitle { color: palette(midlight); }"));
    auto *gitLayout = new QVBoxLayout(gitGroup);
    gitLayout->setSpacing(8);
    m_gitSupportCheck = new QCheckBox(tr("Enable Git support"), gitGroup);
    m_gitSupportCheck->setChecked(flatlas::core::Config::instance().getBool(QStringLiteral("gitSupportEnabled"), true));
    connect(m_gitSupportCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        flatlas::core::Config::instance().setBool(QStringLiteral("gitSupportEnabled"), enabled);
        flatlas::core::Config::instance().save();
        m_lastGitRefreshMs = 0;
        refreshGitPanel();
    });
    gitLayout->addWidget(m_gitSupportCheck);

    m_gitStatusLabel = new QLabel(tr("Select an installation to view Git information."), gitGroup);
    m_gitStatusLabel->setWordWrap(true);
    m_gitStatusLabel->setStyleSheet(QStringLiteral("QLabel { color: palette(text); padding: 4px 0; }"));
    gitLayout->addWidget(m_gitStatusLabel);

    auto *gitButtonRow = new QHBoxLayout();
    m_gitInstallBtn = new QPushButton(tr("Install Git to better manage your mod"), gitGroup);
    connect(m_gitInstallBtn, &QPushButton::clicked, this, &ModManagerPage::onInstallGitClicked);
    gitButtonRow->addWidget(m_gitInstallBtn);
    m_gitInitBtn = new QPushButton(tr("Initialize Git Repository"), gitGroup);
    connect(m_gitInitBtn, &QPushButton::clicked, this, &ModManagerPage::onInitializeGitClicked);
    gitButtonRow->addWidget(m_gitInitBtn);
    gitButtonRow->addStretch();
    gitLayout->addLayout(gitButtonRow);

    m_gitTabs = new QTabWidget(gitGroup);
    m_gitTabs->setDocumentMode(true);
    m_gitChangesList = new QListWidget(m_gitTabs);
    m_gitChangesList->setAlternatingRowColors(true);
    m_gitChangesList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_gitChangesList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
        showGitDiffForCurrentChange();
    });
    m_gitTabs->addTab(m_gitChangesList, tr("Changes"));
    m_gitCommitList = new QListWidget(m_gitTabs);
    m_gitCommitList->setAlternatingRowColors(true);
    m_gitCommitList->setSelectionMode(QAbstractItemView::NoSelection);
    m_gitTabs->addTab(m_gitCommitList, tr("History"));
    gitLayout->addWidget(m_gitTabs, 1);

    profileLayout->addWidget(gitGroup, 1);
    rightSide->addWidget(profilePanel);

    m_conflictTable = new QTableWidget(this);
    m_conflictTable->setColumnCount(2);
    m_conflictTable->setHorizontalHeaderLabels({tr("File"), tr("Conflicting Mods")});
    m_conflictTable->horizontalHeader()->setStretchLastSection(true);
    m_conflictTable->setAlternatingRowColors(true);
    m_conflictTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_conflictTable->verticalHeader()->setVisible(false);
    m_conflictTable->setVisible(false);
    rightSide->addWidget(m_conflictTable);

    rightSide->setStretchFactor(0, 1);
    rightSide->setStretchFactor(1, 0);
    splitter->addWidget(rightSide);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter, 1);

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
    m_toolBar->addAction(tr("Mod exportieren"), this, &ModManagerPage::onExportClicked);
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
    refreshGitPanel();
}

void ModManagerPage::refreshConflicts()
{
    m_conflicts = m_detector.detectConflicts(m_mods);

    m_conflictTable->setRowCount(m_conflicts.size());
    m_conflictTable->setVisible(!m_conflicts.isEmpty());
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

    const QString freelancerExe = flatlas::core::PathUtils::ciResolvePath(
        dir, QStringLiteral("EXE/Freelancer.exe"));
    if (freelancerExe.isEmpty()) {
        QMessageBox::warning(
            this,
            tr("Invalid Installation"),
            tr("The selected folder is not a valid Freelancer installation.\n\n"
               "Required file not found:\nEXE\\Freelancer.exe"));
        return;
    }

    // Derive name from directory name
    QString name = QDir(dir).dirName();
    if (name.isEmpty())
        name = dir;

    flatlas::core::ModProfile profile;
    profile.name = name;
    profile.mode = QStringLiteral("direct");
    profile.directPath = dir;

    if (!flatlas::core::EditingContext::instance().addProfile(profile)) {
        QMessageBox::information(
            this,
            tr("Installation Already Added"),
            tr("This Freelancer installation is already in the list."));
        return;
    }

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

QString ModManagerPage::selectedProfileSourcePath() const
{
    int row = m_profileTable ? m_profileTable->currentRow() : -1;
    if (row < 0)
        return flatlas::core::EditingContext::instance().primaryGamePath();
    auto *item = m_profileTable->item(row, 0);
    if (!item)
        return flatlas::core::EditingContext::instance().primaryGamePath();
    const auto profile = flatlas::core::EditingContext::instance().profileById(item->data(Qt::UserRole).toString());
    return profile.sourcePath();
}

QString ModManagerPage::suggestedReferencePath(const QString &modRoot) const
{
    const QString normalizedModRoot = QDir::cleanPath(modRoot).toLower();
    const auto &profiles = flatlas::core::EditingContext::instance().profiles();
    for (const auto &profile : profiles) {
        const QString path = profile.sourcePath();
        if (path.trimmed().isEmpty() || !QDir(path).exists())
            continue;
        if (QDir::cleanPath(path).toLower() != normalizedModRoot)
            return path;
    }
    return {};
}

void ModManagerPage::onExportClicked()
{
    const QString modRoot = selectedProfileSourcePath();
    if (modRoot.trimmed().isEmpty() || !QDir(modRoot).exists()) {
        QMessageBox::information(this,
                                 tr("Mod exportieren"),
                                 tr("Wähle zuerst eine gültige Installation oder einen aktiven Mod-Kontext aus."));
        return;
    }

    const auto &ctx = flatlas::core::EditingContext::instance();
    QString profileName = QFileInfo(modRoot).fileName();
    const int row = m_profileTable ? m_profileTable->currentRow() : -1;
    if (row >= 0) {
        if (auto *item = m_profileTable->item(row, 0))
            profileName = item->text().remove(QStringLiteral("\u2714 ")).trimmed();
    } else if (ctx.hasContext()) {
        profileName = ctx.editingProfile().name;
    }

    ModExportDialog dialog(profileName,
                           modRoot,
                           suggestedReferencePath(modRoot),
                           QFileInfo(modRoot).absolutePath(),
                           this);
    dialog.exec();
    if (dialog.exportedCount() > 0)
        m_statusLabel->setText(tr("Export erstellt: %1 Datei(en)").arg(dialog.exportedCount()));
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

void ModManagerPage::onInstallGitClicked()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://git-scm.com/download/win")));
}

void ModManagerPage::onInitializeGitClicked()
{
    const QString path = selectedProfileSourcePath();
    if (path.trimmed().isEmpty() || !QDir(path).exists()) {
        QMessageBox::information(this, tr("Git"), tr("Select an installation in the table first."));
        return;
    }

    const auto answer = QMessageBox::question(
        this,
        tr("Initialize Git Repository"),
        tr("Initialize a Git repository in this installation and create an initial commit?\n\n%1").arg(path));
    if (answer != QMessageBox::Yes)
        return;

    QString errorMessage;
    if (!flatlas::core::GitSupport::initializeRepository(path, &errorMessage)) {
        QMessageBox::warning(this,
                             tr("Git"),
                             tr("Git repository could not be initialized:\n%1").arg(errorMessage));
        m_lastGitRefreshMs = 0;
        refreshGitPanel();
        return;
    }

    m_statusLabel->setText(tr("Git repository initialized."));
    m_lastGitRefreshMs = 0;
    refreshGitPanel();
}

void ModManagerPage::showGitDiffForCurrentChange()
{
    if (!m_gitChangesList)
        return;

    auto *item = m_gitChangesList->currentItem();
    if (!item)
        return;

    const QString relativePath = item->data(Qt::UserRole).toString();
    if (relativePath.trimmed().isEmpty())
        return;

    QString errorMessage;
    const QString diff = flatlas::core::GitSupport::diffForFile(selectedProfileSourcePath(), relativePath, &errorMessage);
    if (!errorMessage.trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Git Diff"), errorMessage);
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Git Diff: %1").arg(relativePath));
    dialog.resize(980, 640);
    auto *layout = new QVBoxLayout(&dialog);

    auto *editor = new QPlainTextEdit(&dialog);
    editor->setReadOnly(true);
    editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    editor->setPlainText(diff.trimmed().isEmpty() ? tr("No diff available.") : diff);
    new GitDiffHighlighter(editor->document());
    layout->addWidget(editor, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}

void ModManagerPage::refreshGitPanel()
{
    if (!m_gitStatusLabel || !m_gitInstallBtn || !m_gitInitBtn || !m_gitSupportCheck
        || !m_gitTabs || !m_gitChangesList || !m_gitCommitList) {
        return;
    }

    const bool enabled = m_gitSupportCheck->isChecked();
    m_gitInstallBtn->setVisible(false);
    m_gitInitBtn->setVisible(false);
    m_gitTabs->setVisible(enabled);
    m_gitTabs->setTabText(0, tr("Changes"));
    m_gitTabs->setTabText(1, tr("History"));
    if (!enabled) {
        m_gitChangesList->clear();
        m_gitCommitList->clear();
        m_gitStatusLabel->setText(tr("Git support is disabled."));
        m_gitTabs->setVisible(false);
        return;
    }

    const QString path = selectedProfileSourcePath();
    const bool hasPath = !path.trimmed().isEmpty() && QDir(path).exists();
    if (!hasPath) {
        m_gitChangesList->clear();
        m_gitCommitList->clear();
        m_gitStatusLabel->setText(tr("Select an installation to view Git information."));
        m_gitInitBtn->setVisible(false);
        m_gitTabs->setVisible(false);
        return;
    }

    if (!shouldRefreshGitPanel())
        return;
    m_lastGitRefreshPath = QDir::cleanPath(path);
    m_lastGitRefreshMs = QDateTime::currentMSecsSinceEpoch();

    m_gitChangesList->clear();
    m_gitCommitList->clear();
    m_gitTabs->setVisible(true);
    m_gitStatusLabel->setText(tr("Loading Git information..."));
    addGitRow(m_gitChangesList, createGitRow(tr("Loading..."), tr("Reading working tree status."), m_gitChangesList));
    addGitRow(m_gitCommitList, createGitRow(tr("Loading..."), tr("Reading commit history."), m_gitCommitList));

    const int generation = ++m_gitRefreshGeneration;
    auto *watcher = new QFutureWatcher<GitPanelData>(this);
    connect(watcher, &QFutureWatcher<GitPanelData>::finished, this, [this, watcher, generation]() {
        const GitPanelData data = watcher->result();
        watcher->deleteLater();
        if (generation != m_gitRefreshGeneration)
            return;

        m_gitChangesList->clear();
        m_gitCommitList->clear();

        const auto &status = data.status;
        m_gitInstallBtn->setVisible(!status.gitAvailable);
        m_gitInitBtn->setVisible(status.gitAvailable && !status.repository);

        if (!status.gitAvailable) {
            m_gitStatusLabel->setText(tr("Git is not installed. Install Git to better manage your mod."));
            m_gitTabs->setVisible(false);
            return;
        }
        if (!status.repository) {
            m_gitStatusLabel->setText(tr("No Git repository found for this installation."));
            m_gitInitBtn->setEnabled(true);
            m_gitTabs->setVisible(false);
            return;
        }

        m_gitInitBtn->setVisible(false);
        m_gitStatusLabel->setText(tr("Repository: %1\nChanged files: %2, added lines: %3, deleted lines: %4")
            .arg(status.workTreeRoot)
            .arg(status.changedFileCount())
            .arg(status.addedLineCount())
            .arg(status.deletedLineCount()));
        m_gitTabs->setVisible(true);

        m_gitTabs->setTabText(0, tr("Changes (%1)").arg(status.changedFileCount()));
        if (status.files.isEmpty()) {
            addGitRow(m_gitChangesList, createGitRow(tr("No changes"), tr("Working tree is clean."), m_gitChangesList));
        } else {
            for (const auto &file : status.files) {
                const QString subtitle = file.untracked
                    ? tr("New file - +%1 / -%2").arg(file.addedLines).arg(file.deletedLines)
                    : tr("Modified - +%1 / -%2").arg(file.addedLines).arg(file.deletedLines);
                auto *item = addGitRow(m_gitChangesList, createGitRow(file.path, subtitle, m_gitChangesList));
                item->setData(Qt::UserRole, file.path);
            }
        }

        if (data.commits.isEmpty()) {
            addGitRow(m_gitCommitList, createGitRow(tr("No commits found."), tr("Repository history is empty."), m_gitCommitList));
            return;
        }
        for (const auto &commit : data.commits) {
            addGitRow(m_gitCommitList,
                      createGitRow(QStringLiteral("%1  %2").arg(commit.shortHash, commit.subject),
                                   QStringLiteral("%1 - %2").arg(commit.author, commit.relativeDate),
                                   m_gitCommitList));
        }
    });
    watcher->setFuture(QtConcurrent::run([path]() {
        GitPanelData data;
        data.status = flatlas::core::GitSupport::statusForPath(path);
        if (data.status.repository)
            data.commits = flatlas::core::GitSupport::recentCommitsForPath(path, 8);
        return data;
    }));
}

bool ModManagerPage::shouldRefreshGitPanel() const
{
    const QString path = QDir::cleanPath(selectedProfileSourcePath());
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (path != m_lastGitRefreshPath)
        return true;
    return now - m_lastGitRefreshMs >= 3000;
}

} // namespace flatlas::editors
