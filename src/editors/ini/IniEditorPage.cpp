#include "IniEditorPage.h"

#include "IniCodeEditor.h"
#include "IniSyntaxHighlighter.h"
#include "core/EditingContext.h"
#include "core/PathUtils.h"

#include <QAction>
#include <QCheckBox>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QSplitter>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

using namespace flatlas::infrastructure;

namespace flatlas::editors {

namespace {

enum ItemRoles {
    StartLineRole = Qt::UserRole + 1,
    EndLineRole,
    SnapshotIdRole,
    FilePathRole,
    SearchLineRole,
    SearchTextRole,
};

QString severityLabel(IniDiagnosticSeverity severity)
{
    switch (severity) {
    case IniDiagnosticSeverity::Info:
        return QObject::tr("Info");
    case IniDiagnosticSeverity::Warning:
        return QObject::tr("Warning");
    case IniDiagnosticSeverity::Error:
        return QObject::tr("Error");
    }
    return QObject::tr("Info");
}

QString diffCell(const QString &text)
{
    return text.toHtmlEscaped().replace(QLatin1Char(' '), QStringLiteral("&nbsp;"));
}

QString activeDataRoot()
{
    const auto &ctx = flatlas::core::EditingContext::instance();
    if (!ctx.hasContext())
        return {};
    return flatlas::core::PathUtils::ciResolvePath(ctx.primaryGamePath(), QStringLiteral("DATA"));
}

} // namespace

IniEditorPage::IniEditorPage(QWidget *parent)
    : QWidget(parent)
    , m_analysisTimer(new QTimer(this))
    , m_snapshotTimer(new QTimer(this))
    , m_autosaveTimer(new QTimer(this))
    , m_searchWatcher(new QFutureWatcher<QVector<IniSearchMatch>>(this))
{
    setupUi();

    m_analysisTimer->setSingleShot(true);
    m_analysisTimer->setInterval(250);
    connect(m_analysisTimer, &QTimer::timeout, this, &IniEditorPage::runAnalysis);

    m_snapshotTimer->setSingleShot(true);
    m_snapshotTimer->setInterval(2000);
    connect(m_snapshotTimer, &QTimer::timeout, this, [this]() {
        if (m_dirty)
            captureSnapshot(tr("Edit snapshot"));
    });

    m_autosaveTimer->setInterval(15000);
    connect(m_autosaveTimer, &QTimer::timeout, this, [this]() {
        if (m_dirty)
            writeRecoverySnapshot();
    });
    m_autosaveTimer->start();

    connect(m_searchWatcher, &QFutureWatcher<QVector<IniSearchMatch>>::finished,
            this, &IniEditorPage::handleGlobalSearchFinished);
}

void IniEditorPage::setupUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    setupToolbar();
    setupSearchStrip();
    rootLayout->addWidget(m_toolbar);
    rootLayout->addWidget(m_breadcrumbBar);
    rootLayout->addWidget(m_searchStrip);

    setupWorkspace();
    setupShortcuts();
}

void IniEditorPage::setupToolbar()
{
    m_toolbar = new QToolBar(this);
    m_toolbar->setMovable(false);

    auto *openAction = m_toolbar->addAction(tr("Open"));
    connect(openAction, &QAction::triggered, this, [this]() {
        const QString startDir = preferredProjectRoot().isEmpty() ? QFileInfo(m_filePath).absolutePath() : preferredProjectRoot();
        const QString selected = QFileDialog::getOpenFileName(this,
                                                              tr("Open File Editor File"),
                                                              startDir,
                                                              tr("INI-like Files (*.ini *.cfg *.txt *.bini);;All Files (*)"));
        if (!selected.isEmpty())
            emit openFileRequested(selected, QString(), 0);
    });

    auto *saveAction = m_toolbar->addAction(tr("Save"));
    connect(saveAction, &QAction::triggered, this, [this]() { save(); });

    auto *saveAsAction = m_toolbar->addAction(tr("Save As"));
    connect(saveAsAction, &QAction::triggered, this, [this]() {
        const QString selected = QFileDialog::getSaveFileName(this,
                                                              tr("Save INI File"),
                                                              m_filePath,
                                                              tr("INI Files (*.ini);;All Files (*)"));
        if (!selected.isEmpty())
            saveAs(selected);
    });

    m_toolbar->addSeparator();

    auto *undoAction = m_toolbar->addAction(tr("Undo"));
    connect(undoAction, &QAction::triggered, this, [this]() { m_editor->undo(); });
    auto *redoAction = m_toolbar->addAction(tr("Redo"));
    connect(redoAction, &QAction::triggered, this, [this]() { m_editor->redo(); });

    m_toolbar->addSeparator();

    auto *cutAction = m_toolbar->addAction(tr("Cut"));
    connect(cutAction, &QAction::triggered, this, &IniEditorPage::cutCurrentSelection);
    auto *copyAction = m_toolbar->addAction(tr("Copy"));
    connect(copyAction, &QAction::triggered, this, &IniEditorPage::copyCurrentSelection);
    auto *pasteAction = m_toolbar->addAction(tr("Paste"));
    connect(pasteAction, &QAction::triggered, this, [this]() { m_editor->paste(); });

    m_toolbar->addSeparator();

    auto *copySectionAction = m_toolbar->addAction(tr("Copy Section"));
    connect(copySectionAction, &QAction::triggered, this, &IniEditorPage::copyCurrentSection);
    auto *collapseAction = m_toolbar->addAction(tr("Collapse Section"));
    connect(collapseAction, &QAction::triggered, this, &IniEditorPage::collapseCurrentSection);
    auto *expandAction = m_toolbar->addAction(tr("Expand All"));
    connect(expandAction, &QAction::triggered, this, &IniEditorPage::expandAllSections);

    auto *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(spacer);

    m_summaryLabel = new QLabel(this);
    m_toolbar->addWidget(m_summaryLabel);

    m_breadcrumbBar = new QWidget(this);
    auto *breadcrumbLayout = new QHBoxLayout(m_breadcrumbBar);
    breadcrumbLayout->setContentsMargins(8, 6, 8, 6);
    breadcrumbLayout->setSpacing(4);
}

void IniEditorPage::setupSearchStrip()
{
    m_searchStrip = new QWidget(this);
    auto *layout = new QHBoxLayout(m_searchStrip);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Local search"));
    layout->addWidget(m_searchEdit, 2);

    auto *nextButton = new QPushButton(tr("Next"), this);
    connect(nextButton, &QPushButton::clicked, this, &IniEditorPage::findNext);
    layout->addWidget(nextButton);

    auto *prevButton = new QPushButton(tr("Prev"), this);
    connect(prevButton, &QPushButton::clicked, this, &IniEditorPage::findPrev);
    layout->addWidget(prevButton);

    m_replaceEdit = new QLineEdit(this);
    m_replaceEdit->setPlaceholderText(tr("Replace"));
    layout->addWidget(m_replaceEdit, 2);

    auto *replaceButton = new QPushButton(tr("Replace All"), this);
    connect(replaceButton, &QPushButton::clicked, this, &IniEditorPage::replaceAll);
    layout->addWidget(replaceButton);

    m_caseSensitiveCheck = new QCheckBox(tr("Case"), this);
    layout->addWidget(m_caseSensitiveCheck);

    m_regexCheck = new QCheckBox(tr("Regex"), this);
    layout->addWidget(m_regexCheck);

    m_globalSearchEdit = new QLineEdit(this);
    m_globalSearchEdit->setPlaceholderText(tr("Global search across project"));
    layout->addWidget(m_globalSearchEdit, 3);

    auto *globalButton = new QPushButton(tr("Search Files"), this);
    connect(globalButton, &QPushButton::clicked, this, &IniEditorPage::startGlobalSearch);
    layout->addWidget(globalButton);

    m_gotoLineEdit = new QLineEdit(this);
    m_gotoLineEdit->setPlaceholderText(tr("Line"));
    m_gotoLineEdit->setMaximumWidth(80);
    layout->addWidget(m_gotoLineEdit);

    auto *gotoButton = new QPushButton(tr("Go"), this);
    connect(gotoButton, &QPushButton::clicked, this, [this]() {
        goToLine(m_gotoLineEdit->text().toInt());
    });
    layout->addWidget(gotoButton);

    m_statusLabel = new QLabel(this);
    layout->addWidget(m_statusLabel, 2);

    connect(m_searchEdit, &QLineEdit::returnPressed, this, &IniEditorPage::findNext);
    connect(m_globalSearchEdit, &QLineEdit::returnPressed, this, &IniEditorPage::startGlobalSearch);
    connect(m_gotoLineEdit, &QLineEdit::returnPressed, this, [this]() {
        goToLine(m_gotoLineEdit->text().toInt());
    });
}

void IniEditorPage::setupWorkspace()
{
    auto *rootSplitter = new QSplitter(Qt::Vertical, this);
    static_cast<QVBoxLayout *>(layout())->addWidget(rootSplitter, 1);

    auto *workspaceSplitter = new QSplitter(Qt::Horizontal, rootSplitter);

    auto *leftPane = new QWidget(workspaceSplitter);
    auto *leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(4);
    leftLayout->addWidget(new QLabel(tr("Project"), leftPane));

    m_fileModel = new QFileSystemModel(this);
    m_fileModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Files);
    m_fileModel->setRootPath(QDir::currentPath());

    m_fileTree = new QTreeView(leftPane);
    m_fileTree->setObjectName(QStringLiteral("iniFileTree"));
    m_fileTree->setModel(m_fileModel);
    m_fileTree->setAlternatingRowColors(true);
    for (int column = 1; column < 4; ++column)
        m_fileTree->hideColumn(column);
    connect(m_fileTree, &QTreeView::doubleClicked, this, &IniEditorPage::openFromFileTree);
    leftLayout->addWidget(m_fileTree, 1);

    auto *centerSplitter = new QSplitter(Qt::Horizontal, workspaceSplitter);
    auto *editorMinimapSplitter = new QSplitter(Qt::Horizontal, centerSplitter);

    m_editor = new IniCodeEditor(editorMinimapSplitter);
    m_highlighter = new IniSyntaxHighlighter(m_editor->document());
    connect(m_editor, &QPlainTextEdit::textChanged, this, &IniEditorPage::onTextChanged);
    connect(m_editor, &QPlainTextEdit::cursorPositionChanged, this, &IniEditorPage::refreshCurrentSectionFromCursor);

    m_minimap = new QPlainTextEdit(editorMinimapSplitter);
    m_minimap->setReadOnly(true);
    m_minimap->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont minimapFont = m_editor->font();
    minimapFont.setPointSizeF(qMax(5.0, minimapFont.pointSizeF() * 0.6));
    m_minimap->setFont(minimapFont);
    m_minimap->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_minimap->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_minimap->setMaximumWidth(160);
    connect(m_editor->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() { syncMinimapScroll(); });

    auto *rightPane = new QWidget(centerSplitter);
    auto *rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(4);

    rightLayout->addWidget(new QLabel(tr("Sections"), rightPane));
    m_sectionFilterEdit = new QLineEdit(rightPane);
    m_sectionFilterEdit->setPlaceholderText(tr("Filter sections"));
    rightLayout->addWidget(m_sectionFilterEdit);

    m_sectionList = new QListWidget(rightPane);
    m_sectionList->setObjectName(QStringLiteral("iniSectionList"));
    m_sectionList->setContextMenuPolicy(Qt::CustomContextMenu);
    rightLayout->addWidget(m_sectionList, 3);

    rightLayout->addWidget(new QLabel(tr("Keys in current section"), rightPane));
    m_keyFilterEdit = new QLineEdit(rightPane);
    m_keyFilterEdit->setPlaceholderText(tr("Filter keys"));
    rightLayout->addWidget(m_keyFilterEdit);

    m_keyList = new QListWidget(rightPane);
    m_keyList->setObjectName(QStringLiteral("iniKeyList"));
    rightLayout->addWidget(m_keyList, 2);

    connect(m_sectionFilterEdit, &QLineEdit::textChanged, this, &IniEditorPage::populateSectionList);
    connect(m_keyFilterEdit, &QLineEdit::textChanged, this, &IniEditorPage::refreshCurrentSectionFromCursor);
    connect(m_sectionList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0)
            return;
        auto *item = m_sectionList->item(row);
        if (!item)
            return;
        const int startLine = item->data(StartLineRole).toInt();
        goToLine(startLine);
        populateKeyListForSectionStartLine(startLine);
    });
    connect(m_sectionList, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        auto *item = m_sectionList->itemAt(pos);
        if (!item)
            return;

        QMenu menu(this);
        auto *jumpAction = menu.addAction(tr("Jump to section"));
        auto *copyAction = menu.addAction(tr("Copy section"));
        auto *collapseAction = menu.addAction(tr("Collapse section"));
        const QAction *selected = menu.exec(m_sectionList->viewport()->mapToGlobal(pos));
        const int startLine = item->data(StartLineRole).toInt();
        if (selected == jumpAction) {
            goToLine(startLine);
        } else if (selected == copyAction) {
            IniSectionInfo section;
            section.name = item->text();
            section.startLine = startLine;
            section.endLine = item->data(EndLineRole).toInt();
            copySelectionToCollector(IniAnalysisService::sectionText(m_editor->toPlainText(), section),
                                     tr("Section %1").arg(item->text()));
        } else if (selected == collapseAction) {
            m_collapsedSectionLines.insert(startLine);
            applyFolding();
        }
    });

    setupBottomTabs();

    rootSplitter->addWidget(workspaceSplitter);
    rootSplitter->addWidget(m_bottomTabs);
    rootSplitter->setStretchFactor(0, 4);
    rootSplitter->setStretchFactor(1, 2);

    workspaceSplitter->setStretchFactor(0, 1);
    workspaceSplitter->setStretchFactor(1, 4);
    centerSplitter->setStretchFactor(0, 4);
    centerSplitter->setStretchFactor(1, 1);
    editorMinimapSplitter->setStretchFactor(0, 5);
    editorMinimapSplitter->setStretchFactor(1, 1);
}

void IniEditorPage::setupBottomTabs()
{
    m_bottomTabs = new QTabWidget(this);

    m_diagnosticsModel = new QStandardItemModel(this);
    m_diagnosticsModel->setHorizontalHeaderLabels({tr("Severity"), tr("Line"), tr("Section"), tr("Key"), tr("Message")});
    m_diagnosticsView = new QTreeView(m_bottomTabs);
    m_diagnosticsView->setObjectName(QStringLiteral("iniDiagnosticsView"));
    m_diagnosticsView->setModel(m_diagnosticsModel);
    m_diagnosticsView->setRootIsDecorated(false);
    m_diagnosticsView->setAlternatingRowColors(true);
    m_diagnosticsView->header()->setStretchLastSection(true);
    connect(m_diagnosticsView, &QTreeView::doubleClicked, this, [this](const QModelIndex &index) {
        const int lineNumber = m_diagnosticsModel->item(index.row(), 1)->text().toInt();
        goToLine(lineNumber);
    });
    m_bottomTabs->addTab(m_diagnosticsView, tr("Diagnostics"));

    m_searchResultsModel = new QStandardItemModel(this);
    m_searchResultsModel->setHorizontalHeaderLabels({tr("File"), tr("Line"), tr("Section"), tr("Key"), tr("Preview")});
    m_searchResultsView = new QTreeView(m_bottomTabs);
    m_searchResultsView->setObjectName(QStringLiteral("iniSearchResultsView"));
    m_searchResultsView->setModel(m_searchResultsModel);
    m_searchResultsView->setRootIsDecorated(false);
    m_searchResultsView->setAlternatingRowColors(true);
    m_searchResultsView->header()->setStretchLastSection(true);
    connect(m_searchResultsView, &QTreeView::doubleClicked, this, &IniEditorPage::openFromSearchResult);
    m_bottomTabs->addTab(m_searchResultsView, tr("Global Search"));

    m_clipboardList = new QListWidget(m_bottomTabs);
    m_clipboardList->setObjectName(QStringLiteral("iniClipboardList"));
    connect(m_clipboardList, &QListWidget::itemDoubleClicked, this, &IniEditorPage::activateClipboardItem);
    m_bottomTabs->addTab(m_clipboardList, tr("Clipboard Collector"));

    auto *historyPane = new QWidget(m_bottomTabs);
    auto *historyLayout = new QHBoxLayout(historyPane);
    historyLayout->setContentsMargins(0, 0, 0, 0);
    historyLayout->setSpacing(4);
    m_historyList = new QListWidget(historyPane);
    m_historyList->setObjectName(QStringLiteral("iniHistoryList"));
    connect(m_historyList, &QListWidget::currentRowChanged, this, [this](int) { updateSnapshotDiff(); });
    connect(m_historyList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) { activateSelectedSnapshot(); });
    historyLayout->addWidget(m_historyList, 1);
    m_historyDiffView = new QTextBrowser(historyPane);
    m_historyDiffView->setObjectName(QStringLiteral("iniHistoryDiffView"));
    historyLayout->addWidget(m_historyDiffView, 2);
    m_bottomTabs->addTab(historyPane, tr("Time Machine"));

    auto *recoveryPane = new QWidget(m_bottomTabs);
    auto *recoveryLayout = new QVBoxLayout(recoveryPane);
    recoveryLayout->setContentsMargins(6, 6, 6, 6);
    recoveryLayout->setSpacing(6);
    m_recoveryLabel = new QLabel(recoveryPane);
    recoveryLayout->addWidget(m_recoveryLabel);
    auto *buttonRow = new QHBoxLayout();
    m_restoreRecoveryButton = new QPushButton(tr("Restore autosave"), recoveryPane);
    connect(m_restoreRecoveryButton, &QPushButton::clicked, this, &IniEditorPage::restoreRecoverySnapshot);
    buttonRow->addWidget(m_restoreRecoveryButton);
    m_discardRecoveryButton = new QPushButton(tr("Discard autosave"), recoveryPane);
    connect(m_discardRecoveryButton, &QPushButton::clicked, this, &IniEditorPage::discardRecoverySnapshot);
    buttonRow->addWidget(m_discardRecoveryButton);
    buttonRow->addStretch();
    recoveryLayout->addLayout(buttonRow);
    recoveryLayout->addWidget(new QLabel(tr("Recent files"), recoveryPane));
    m_recentFilesList = new QListWidget(recoveryPane);
    m_recentFilesList->setObjectName(QStringLiteral("iniRecentFilesList"));
    connect(m_recentFilesList, &QListWidget::itemDoubleClicked, this, &IniEditorPage::activateRecentFile);
    recoveryLayout->addWidget(m_recentFilesList, 1);
    m_bottomTabs->addTab(recoveryPane, tr("Recovery"));
}

void IniEditorPage::setupShortcuts()
{
    auto *findAction = new QAction(this);
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, [this]() {
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
    });
    addAction(findAction);

    auto *copyAction = new QAction(this);
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, &IniEditorPage::copyCurrentSelection);
    addAction(copyAction);

    auto *cutAction = new QAction(this);
    cutAction->setShortcut(QKeySequence::Cut);
    connect(cutAction, &QAction::triggered, this, &IniEditorPage::cutCurrentSelection);
    addAction(cutAction);

    auto *saveAction = new QAction(this);
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, [this]() { save(); });
    addAction(saveAction);
}

bool IniEditorPage::openFile(const QString &filePath)
{
    bool wasBini = false;
    const QString diskText = IniAnalysisService::loadIniLikeText(filePath, &wasBini);
    if (diskText.isNull())
        return false;

    m_filePath = filePath;
    m_wasBini = wasBini;

    QString finalText = diskText;
    const QString recoveryPath = recoveryFilePathFor(filePath);
    if (QFileInfo::exists(recoveryPath)) {
        QFile recoveryFile(recoveryPath);
        if (recoveryFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString recoveredText = QString::fromUtf8(recoveryFile.readAll());
            recoveryFile.close();
            if (!recoveredText.isEmpty() && recoveredText != diskText) {
                QMessageBox prompt(this);
                prompt.setWindowTitle(tr("Autosave found"));
                prompt.setText(tr("A newer autosave was found for this file."));
                prompt.setInformativeText(tr("Restore the autosaved version or keep the file from disk?"));
                auto *restoreButton = prompt.addButton(tr("Restore autosave"), QMessageBox::AcceptRole);
                auto *keepButton = prompt.addButton(tr("Keep disk version"), QMessageBox::RejectRole);
                auto *cancelButton = prompt.addButton(QMessageBox::Cancel);
                prompt.exec();
                if (prompt.clickedButton() == cancelButton)
                    return false;
                m_pendingRecoveryText = recoveredText;
                if (prompt.clickedButton() == restoreButton)
                    finalText = recoveredText;
                if (prompt.clickedButton() == keepButton)
                    updateRecoveryUi();
            }
        }
    }

    m_loading = true;
    m_editor->setPlainText(finalText);
    m_loading = false;

    m_savedText = diskText;
    m_dirty = (finalText != m_savedText);
    m_collapsedSectionLines.clear();
    m_snapshots.clear();
    m_nextSnapshotId = 1;
    captureSnapshot(m_dirty ? tr("Opened with recovery state") : tr("Opened from disk"));

    rememberRecentFile(filePath);
    updateWorkspaceContext();
    runAnalysis();
    syncMinimapText();
    updateRecoveryUi();
    updateTitle();
    return true;
}

bool IniEditorPage::save()
{
    if (m_filePath.isEmpty())
        return false;
    return saveAs(m_filePath);
}

bool IniEditorPage::saveAs(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;

    file.write(m_editor->toPlainText().toUtf8());
    file.close();

    m_filePath = filePath;
    m_savedText = m_editor->toPlainText();
    m_dirty = false;
    m_wasBini = false;
    rememberRecentFile(filePath);
    clearRecoverySnapshot();
    captureSnapshot(tr("Saved"));
    updateWorkspaceContext();
    updateRecoveryUi();
    updateTitle();
    return true;
}

QString IniEditorPage::filePath() const
{
    return m_filePath;
}

QString IniEditorPage::fileName() const
{
    return m_filePath.isEmpty() ? tr("Untitled") : QFileInfo(m_filePath).fileName();
}

bool IniEditorPage::isDirty() const
{
    return m_dirty;
}

void IniEditorPage::focusSearch(const QString &text)
{
    m_searchEdit->setText(text);
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
    findNext();
}

void IniEditorPage::goToLine(int lineNumber)
{
    if (lineNumber <= 0)
        return;
    m_editor->goToLine(lineNumber);
    m_editor->setFocus();
    refreshCurrentSectionFromCursor();
}

void IniEditorPage::onTextChanged()
{
    if (m_loading)
        return;

    updateDirtyState();
    m_analysisTimer->start();
    m_snapshotTimer->start();
}

void IniEditorPage::runAnalysis()
{
    m_analysis = IniAnalysisService::analyzeText(m_editor->toPlainText());
    populateSectionList();
    refreshCurrentSectionFromCursor();
    updateDiagnosticsView();
    updateStatusSummary();
    syncMinimapText();
    applyFolding();
}

void IniEditorPage::refreshCurrentSectionFromCursor()
{
    const int lineNumber = m_editor->textCursor().blockNumber() + 1;
    const int sectionIndex = m_analysis.sectionIndexForLine(lineNumber);
    if (sectionIndex < 0 || sectionIndex >= m_analysis.sections.size()) {
        m_keyList->clear();
        return;
    }

    const int startLine = m_analysis.sections.at(sectionIndex).startLine;
    for (int row = 0; row < m_sectionList->count(); ++row) {
        auto *item = m_sectionList->item(row);
        if (item && item->data(StartLineRole).toInt() == startLine) {
            m_sectionList->setCurrentRow(row);
            break;
        }
    }
    populateKeyListForSectionStartLine(startLine);
    syncMinimapScroll();
}

void IniEditorPage::startGlobalSearch()
{
    if (m_searchWatcher->isRunning())
        return;

    IniSearchOptions options;
    options.query = m_globalSearchEdit->text().trimmed();
    options.useRegex = m_regexCheck->isChecked();
    options.caseSensitive = m_caseSensitiveCheck->isChecked();
    options.sectionFilter = m_sectionFilterEdit ? m_sectionFilterEdit->text() : QString();
    options.keyFilter = m_keyFilterEdit ? m_keyFilterEdit->text() : QString();

    const QString rootPath = preferredProjectRoot();
    if (rootPath.isEmpty() || options.query.isEmpty()) {
        m_statusLabel->setText(tr("Global search needs a query and project root."));
        return;
    }

    m_statusLabel->setText(tr("Searching project..."));
    m_searchWatcher->setFuture(QtConcurrent::run([rootPath, options]() {
        return IniAnalysisService::searchDirectory(rootPath, options);
    }));
}

void IniEditorPage::handleGlobalSearchFinished()
{
    const auto matches = m_searchWatcher->result();
    updateSearchResultsView(matches);
    m_bottomTabs->setCurrentWidget(m_searchResultsView);
    m_statusLabel->setText(tr("%1 matches").arg(matches.size()));
}

void IniEditorPage::openFromFileTree(const QModelIndex &index)
{
    const QString path = m_fileModel->filePath(index);
    if (QFileInfo(path).isDir()) {
        setTreeRootPath(path);
        return;
    }

    if (isIniLikePath(path))
        emit openFileRequested(path, QString(), 0);
}

void IniEditorPage::openFromSearchResult(const QModelIndex &index)
{
    const QString path = m_searchResultsModel->item(index.row(), 0)->data(FilePathRole).toString();
    const int lineNumber = m_searchResultsModel->item(index.row(), 1)->data(SearchLineRole).toInt();
    const QString searchText = m_searchResultsModel->item(index.row(), 4)->data(SearchTextRole).toString();
    if (!path.isEmpty())
        emit openFileRequested(path, searchText, lineNumber);
}

void IniEditorPage::activateSelectedSnapshot()
{
    auto *item = m_historyList->currentItem();
    if (!item)
        return;

    const int snapshotId = item->data(SnapshotIdRole).toInt();
    for (const auto &snapshot : m_snapshots) {
        if (snapshot.id != snapshotId)
            continue;
        m_loading = true;
        m_editor->setPlainText(snapshot.text);
        m_loading = false;
        updateDirtyState();
        runAnalysis();
        captureSnapshot(tr("Restored snapshot"));
        return;
    }
}

void IniEditorPage::activateClipboardItem(QListWidgetItem *item)
{
    if (!item)
        return;
    m_editor->insertPlainText(item->data(Qt::UserRole).toString());
}

void IniEditorPage::activateRecentFile(QListWidgetItem *item)
{
    if (!item)
        return;
    const QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty())
        emit openFileRequested(path, QString(), 0);
}

void IniEditorPage::restoreRecoverySnapshot()
{
    if (m_pendingRecoveryText.isEmpty())
        return;
    m_loading = true;
    m_editor->setPlainText(m_pendingRecoveryText);
    m_loading = false;
    updateDirtyState();
    runAnalysis();
    captureSnapshot(tr("Recovered autosave"));
}

void IniEditorPage::discardRecoverySnapshot()
{
    m_pendingRecoveryText.clear();
    clearRecoverySnapshot();
    updateRecoveryUi();
}

void IniEditorPage::updateTitle()
{
    QString title = fileName();
    if (m_wasBini)
        title += tr(" [BINI->Text]");
    if (m_dirty)
        title += QStringLiteral(" *");
    emit titleChanged(title);
}

void IniEditorPage::updateDirtyState()
{
    const bool dirtyNow = (m_editor->toPlainText() != m_savedText);
    if (m_dirty != dirtyNow) {
        m_dirty = dirtyNow;
        updateTitle();
    }
}

void IniEditorPage::updateWorkspaceContext()
{
    setTreeRootPath(preferredProjectRoot());
    updateBreadcrumbs();
    refreshRecentFiles();
}

void IniEditorPage::updateBreadcrumbs()
{
    auto *breadcrumbLayout = qobject_cast<QHBoxLayout *>(m_breadcrumbBar->layout());
    while (QLayoutItem *child = breadcrumbLayout->takeAt(0)) {
        if (child->widget())
            child->widget()->deleteLater();
        delete child;
    }

    if (m_filePath.isEmpty()) {
        breadcrumbLayout->addWidget(new QLabel(tr("No file loaded"), m_breadcrumbBar));
        breadcrumbLayout->addStretch();
        return;
    }

    const QString rootPath = preferredProjectRoot();
    const QString absoluteFilePath = QDir::fromNativeSeparators(QFileInfo(m_filePath).absoluteFilePath());
    const QString relativePath = rootPath.isEmpty() ? absoluteFilePath : QDir(rootPath).relativeFilePath(absoluteFilePath);
    const QStringList parts = relativePath.split(QLatin1Char('/'), Qt::SkipEmptyParts);

    QString currentPath = rootPath.isEmpty() ? QFileInfo(m_filePath).absolutePath() : rootPath;
    for (int index = 0; index < parts.size(); ++index) {
        const QString part = parts.at(index);
        if (index > 0)
            breadcrumbLayout->addWidget(new QLabel(QStringLiteral("/"), m_breadcrumbBar));

        const bool last = (index == parts.size() - 1);
        if (!last)
            currentPath = QDir(currentPath).filePath(part);

        auto *button = new QToolButton(m_breadcrumbBar);
        button->setText(part);
        button->setAutoRaise(true);
        if (!last) {
            const QString targetPath = currentPath;
            connect(button, &QToolButton::clicked, this, [this, targetPath]() {
                setTreeRootPath(targetPath);
            });
        } else {
            button->setEnabled(false);
        }
        breadcrumbLayout->addWidget(button);
    }
    breadcrumbLayout->addStretch();
}

void IniEditorPage::setTreeRootPath(const QString &rootPath)
{
    m_treeRootPath = rootPath;
    if (m_treeRootPath.isEmpty())
        m_treeRootPath = QFileInfo(m_filePath).absolutePath();
    if (m_treeRootPath.isEmpty())
        m_treeRootPath = QDir::currentPath();

    const QModelIndex rootIndex = m_fileModel->setRootPath(m_treeRootPath);
    m_fileTree->setRootIndex(rootIndex);
}

QString IniEditorPage::preferredProjectRoot() const
{
    const QString dataRoot = activeDataRoot();
    if (!dataRoot.isEmpty() && !m_filePath.isEmpty()) {
        const QString filePath = QFileInfo(m_filePath).absoluteFilePath();
        if (filePath.startsWith(QDir(dataRoot).absolutePath(), Qt::CaseInsensitive))
            return dataRoot;
    }

    if (!m_filePath.isEmpty())
        return QFileInfo(m_filePath).absolutePath();
    if (!m_treeRootPath.isEmpty())
        return m_treeRootPath;
    return dataRoot;
}

bool IniEditorPage::isIniLikePath(const QString &path) const
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QStringLiteral("ini")
        || suffix == QStringLiteral("cfg")
        || suffix == QStringLiteral("txt")
        || suffix == QStringLiteral("bini")
        || suffix.isEmpty();
}

void IniEditorPage::populateSectionList()
{
    const QString filter = m_sectionFilterEdit ? m_sectionFilterEdit->text().trimmed() : QString();
    const int selectedStartLine = m_sectionList->currentItem() ? m_sectionList->currentItem()->data(StartLineRole).toInt() : -1;

    m_sectionList->clear();
    for (const auto &section : m_analysis.sections) {
        if (!filter.isEmpty() && !section.name.contains(filter, Qt::CaseInsensitive))
            continue;

        QString label = section.name;
        if (section.duplicateSection)
            label += QStringLiteral(" [") + tr("duplicate") + QStringLiteral("]");
        auto *item = new QListWidgetItem(label, m_sectionList);
        item->setData(StartLineRole, section.startLine);
        item->setData(EndLineRole, section.endLine);
        if (section.startLine == selectedStartLine)
            m_sectionList->setCurrentItem(item);
    }
}

void IniEditorPage::populateKeyListForSectionStartLine(int startLine)
{
    m_keyList->clear();
    const QString keyFilter = m_keyFilterEdit ? m_keyFilterEdit->text().trimmed() : QString();
    for (const auto &section : m_analysis.sections) {
        if (section.startLine != startLine)
            continue;
        for (const auto &key : section.keys) {
            if (!keyFilter.isEmpty() && !key.key.contains(keyFilter, Qt::CaseInsensitive))
                continue;
            auto *item = new QListWidgetItem(QStringLiteral("%1 = %2").arg(key.key, key.value), m_keyList);
            item->setData(StartLineRole, key.lineNumber);
        }
        return;
    }
}

void IniEditorPage::updateDiagnosticsView()
{
    m_diagnosticsModel->removeRows(0, m_diagnosticsModel->rowCount());
    for (const auto &diagnostic : m_analysis.diagnostics) {
        m_diagnosticsModel->appendRow({
            new QStandardItem(severityLabel(diagnostic.severity)),
            new QStandardItem(QString::number(diagnostic.lineNumber)),
            new QStandardItem(diagnostic.sectionName),
            new QStandardItem(diagnostic.keyName),
            new QStandardItem(diagnostic.message),
        });
    }
    m_diagnosticsView->resizeColumnToContents(0);
    m_diagnosticsView->resizeColumnToContents(1);
}

void IniEditorPage::updateSearchResultsView(const QVector<IniSearchMatch> &matches)
{
    m_searchResultsModel->removeRows(0, m_searchResultsModel->rowCount());
    for (const auto &match : matches) {
        auto *fileItem = new QStandardItem(match.relativePath.isEmpty() ? QFileInfo(match.filePath).fileName() : match.relativePath);
        fileItem->setData(match.filePath, FilePathRole);
        auto *lineItem = new QStandardItem(QString::number(match.lineNumber));
        lineItem->setData(match.lineNumber, SearchLineRole);
        auto *sectionItem = new QStandardItem(match.sectionName);
        auto *keyItem = new QStandardItem(match.keyName);
        auto *previewItem = new QStandardItem(match.lineText);
        previewItem->setData(m_globalSearchEdit->text(), SearchTextRole);
        m_searchResultsModel->appendRow({fileItem, lineItem, sectionItem, keyItem, previewItem});
    }
    m_searchResultsView->resizeColumnToContents(0);
    m_searchResultsView->resizeColumnToContents(1);
}

void IniEditorPage::syncMinimapText()
{
    const QString text = m_editor->toPlainText();
    if (m_minimap->toPlainText() == text)
        return;
    m_minimap->setPlainText(text);
    syncMinimapScroll();
}

void IniEditorPage::syncMinimapScroll()
{
    const int editorMaximum = m_editor->verticalScrollBar()->maximum();
    const int minimapMaximum = m_minimap->verticalScrollBar()->maximum();
    if (editorMaximum <= 0 || minimapMaximum <= 0) {
        m_minimap->verticalScrollBar()->setValue(0);
        return;
    }

    const double ratio = static_cast<double>(m_editor->verticalScrollBar()->value()) / static_cast<double>(editorMaximum);
    m_minimap->verticalScrollBar()->setValue(static_cast<int>(ratio * minimapMaximum));
}

void IniEditorPage::updateStatusSummary()
{
    int errors = 0;
    int warnings = 0;
    for (const auto &diagnostic : m_analysis.diagnostics) {
        if (diagnostic.severity == IniDiagnosticSeverity::Error)
            ++errors;
        else if (diagnostic.severity == IniDiagnosticSeverity::Warning)
            ++warnings;
    }

    m_summaryLabel->setText(tr("Sections: %1 | Diagnostics: %2 errors, %3 warnings | Snapshots: %4")
                                .arg(m_analysis.sections.size())
                                .arg(errors)
                                .arg(warnings)
                                .arg(m_snapshots.size()));
}

bool IniEditorPage::findWithCurrentOptions(bool forward)
{
    const QString needle = m_searchEdit->text();
    if (needle.isEmpty())
        return false;

    QTextDocument::FindFlags flags;
    if (!forward)
        flags |= QTextDocument::FindBackward;
    if (m_caseSensitiveCheck->isChecked())
        flags |= QTextDocument::FindCaseSensitively;

    QTextCursor found;
    if (m_regexCheck->isChecked()) {
        QRegularExpression::PatternOptions patternOptions = QRegularExpression::NoPatternOption;
        if (!m_caseSensitiveCheck->isChecked())
            patternOptions |= QRegularExpression::CaseInsensitiveOption;
        const QRegularExpression regex(needle, patternOptions);
        if (!regex.isValid()) {
            m_statusLabel->setText(tr("Invalid regex"));
            return false;
        }
        found = m_editor->document()->find(regex, m_editor->textCursor(), flags);
    } else {
        found = m_editor->document()->find(needle, m_editor->textCursor(), flags);
    }

    if (found.isNull())
        return false;

    m_editor->setTextCursor(found);
    m_statusLabel->clear();
    return true;
}

void IniEditorPage::findNext()
{
    if (!findWithCurrentOptions(true))
        m_statusLabel->setText(tr("Not found"));
}

void IniEditorPage::findPrev()
{
    if (!findWithCurrentOptions(false))
        m_statusLabel->setText(tr("Not found"));
}

void IniEditorPage::replaceAll()
{
    const QString searchText = m_searchEdit->text();
    if (searchText.isEmpty())
        return;

    QString content = m_editor->toPlainText();
    int replacements = 0;

    if (m_regexCheck->isChecked()) {
        QRegularExpression::PatternOptions patternOptions = QRegularExpression::NoPatternOption;
        if (!m_caseSensitiveCheck->isChecked())
            patternOptions |= QRegularExpression::CaseInsensitiveOption;
        const QRegularExpression regex(searchText, patternOptions);
        if (!regex.isValid()) {
            m_statusLabel->setText(tr("Invalid regex"));
            return;
        }
        QRegularExpressionMatchIterator it = regex.globalMatch(content);
        while (it.hasNext()) {
            it.next();
            ++replacements;
        }
        content.replace(regex, m_replaceEdit->text());
    } else {
        const Qt::CaseSensitivity cs = m_caseSensitiveCheck->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
        int pos = 0;
        while ((pos = content.indexOf(searchText, pos, cs)) >= 0) {
            content.replace(pos, searchText.size(), m_replaceEdit->text());
            pos += m_replaceEdit->text().size();
            ++replacements;
        }
    }

    if (replacements > 0)
        m_editor->setPlainText(content);
    m_statusLabel->setText(tr("%1 replacements").arg(replacements));
}

QString IniEditorPage::currentSectionText() const
{
    const int lineNumber = m_editor->textCursor().blockNumber() + 1;
    const int sectionIndex = m_analysis.sectionIndexForLine(lineNumber);
    if (sectionIndex < 0 || sectionIndex >= m_analysis.sections.size())
        return {};
    return IniAnalysisService::sectionText(m_editor->toPlainText(), m_analysis.sections.at(sectionIndex));
}

void IniEditorPage::copySelectionToCollector(const QString &text, const QString &label)
{
    if (text.trimmed().isEmpty())
        return;
    auto *item = new QListWidgetItem(QStringLiteral("%1 | %2 chars").arg(label).arg(text.size()));
    item->setData(Qt::UserRole, text);
    m_clipboardList->insertItem(0, item);
    while (m_clipboardList->count() > 50)
        delete m_clipboardList->takeItem(m_clipboardList->count() - 1);
}

void IniEditorPage::copyCurrentSelection()
{
    const QString selected = m_editor->textCursor().selectedText().replace(QChar(0x2029), QLatin1Char('\n'));
    copySelectionToCollector(selected, tr("Selection"));
    m_editor->copy();
}

void IniEditorPage::cutCurrentSelection()
{
    const QString selected = m_editor->textCursor().selectedText().replace(QChar(0x2029), QLatin1Char('\n'));
    copySelectionToCollector(selected, tr("Cut"));
    m_editor->cut();
}

void IniEditorPage::copyCurrentSection()
{
    copySelectionToCollector(currentSectionText(), tr("Section"));
}

void IniEditorPage::captureSnapshot(const QString &label)
{
    const QString currentText = m_editor->toPlainText();
    if (!m_snapshots.isEmpty() && m_snapshots.last().text == currentText)
        return;

    Snapshot snapshot;
    snapshot.id = m_nextSnapshotId++;
    snapshot.label = label;
    snapshot.createdAt = QDateTime::currentDateTime();
    snapshot.text = currentText;
    m_snapshots.append(snapshot);
    while (m_snapshots.size() > 40)
        m_snapshots.removeFirst();

    refreshSnapshotList();
    updateStatusSummary();
}

void IniEditorPage::refreshSnapshotList()
{
    m_historyList->clear();
    for (int index = m_snapshots.size() - 1; index >= 0; --index) {
        const auto &snapshot = m_snapshots.at(index);
        auto *item = new QListWidgetItem(
            QStringLiteral("%1 | %2").arg(snapshot.createdAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")), snapshot.label));
        item->setData(SnapshotIdRole, snapshot.id);
        m_historyList->addItem(item);
    }
    if (m_historyList->count() > 0)
        m_historyList->setCurrentRow(0);
}

void IniEditorPage::updateSnapshotDiff()
{
    auto *item = m_historyList->currentItem();
    if (!item) {
        m_historyDiffView->clear();
        return;
    }

    const int snapshotId = item->data(SnapshotIdRole).toInt();
    for (const auto &snapshot : m_snapshots) {
        if (snapshot.id == snapshotId) {
            m_historyDiffView->setHtml(buildDiffHtml(snapshot.text, m_editor->toPlainText()));
            return;
        }
    }
}

QString IniEditorPage::buildDiffHtml(const QString &fromText, const QString &toText) const
{
    const QStringList fromLines = fromText.split(QLatin1Char('\n'));
    const QStringList toLines = toText.split(QLatin1Char('\n'));
    QString html = QStringLiteral("<html><body><table cellspacing='0' cellpadding='3' width='100%'>");
    const int maxLines = qMax(fromLines.size(), toLines.size());
    for (int index = 0; index < maxLines; ++index) {
        const QString left = index < fromLines.size() ? fromLines.at(index) : QString();
        const QString right = index < toLines.size() ? toLines.at(index) : QString();
        QString prefix = QStringLiteral("=");
        QString background = QStringLiteral("#1f1f1f");
        if (left != right) {
            prefix = left.isEmpty() ? QStringLiteral("+") : (right.isEmpty() ? QStringLiteral("-") : QStringLiteral("~"));
            background = left.isEmpty() ? QStringLiteral("#15391f") : (right.isEmpty() ? QStringLiteral("#4a1f1f") : QStringLiteral("#463915"));
        }
        html += QStringLiteral("<tr style='background:%1'><td width='24'>%2</td><td width='48'>%3</td><td><pre style='margin:0'>%4</pre></td><td><pre style='margin:0'>%5</pre></td></tr>")
                    .arg(background, prefix, QString::number(index + 1), diffCell(left), diffCell(right));
    }
    html += QStringLiteral("</table></body></html>");
    return html;
}

QString IniEditorPage::recoveryFilePathFor(const QString &filePath) const
{
    if (filePath.isEmpty())
        return {};
    const QString recoveryDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/ini-workspace/recovery");
    QDir().mkpath(recoveryDir);
    const QByteArray hash = QCryptographicHash::hash(filePath.toUtf8(), QCryptographicHash::Sha1).toHex();
    return recoveryDir + QStringLiteral("/") + QString::fromLatin1(hash) + QStringLiteral(".autosave.ini");
}

void IniEditorPage::writeRecoverySnapshot()
{
    const QString recoveryPath = recoveryFilePathFor(m_filePath);
    if (recoveryPath.isEmpty())
        return;

    QFile file(recoveryPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return;
    file.write(m_editor->toPlainText().toUtf8());
    file.close();
    m_pendingRecoveryText = m_editor->toPlainText();
    updateRecoveryUi();
}

void IniEditorPage::clearRecoverySnapshot()
{
    QFile::remove(recoveryFilePathFor(m_filePath));
    m_pendingRecoveryText.clear();
}

void IniEditorPage::updateRecoveryUi()
{
    const bool hasRecovery = !m_pendingRecoveryText.isEmpty() || QFileInfo(recoveryFilePathFor(m_filePath)).exists();
    m_recoveryLabel->setText(hasRecovery
                                 ? tr("Autosave data is available for this file.")
                                 : tr("No autosave data is currently stored for this file."));
    m_restoreRecoveryButton->setEnabled(hasRecovery);
    m_discardRecoveryButton->setEnabled(hasRecovery);
}

QStringList IniEditorPage::loadRecentFiles() const
{
    QSettings settings;
    return settings.value(QStringLiteral("iniEditor/recentFiles")).toStringList();
}

void IniEditorPage::rememberRecentFile(const QString &filePath)
{
    if (filePath.isEmpty())
        return;
    QStringList recentFiles = loadRecentFiles();
    recentFiles.removeAll(filePath);
    recentFiles.prepend(filePath);
    while (recentFiles.size() > 20)
        recentFiles.removeLast();
    QSettings settings;
    settings.setValue(QStringLiteral("iniEditor/recentFiles"), recentFiles);
    refreshRecentFiles();
}

void IniEditorPage::refreshRecentFiles()
{
    m_recentFilesList->clear();
    for (const auto &recentFile : loadRecentFiles()) {
        if (!QFileInfo::exists(recentFile))
            continue;
        auto *item = new QListWidgetItem(QFileInfo(recentFile).fileName() + QStringLiteral("  [") + recentFile + QStringLiteral("]"));
        item->setData(Qt::UserRole, recentFile);
        m_recentFilesList->addItem(item);
    }
}

void IniEditorPage::applyFolding()
{
    QTextBlock block = m_editor->document()->begin();
    while (block.isValid()) {
        block.setVisible(true);
        block.setLineCount(1);
        block = block.next();
    }

    for (const auto &section : m_analysis.sections) {
        if (!m_collapsedSectionLines.contains(section.startLine))
            continue;
        for (int line = section.startLine + 1; line <= section.endLine; ++line) {
            QTextBlock hiddenBlock = m_editor->document()->findBlockByNumber(line - 1);
            if (!hiddenBlock.isValid())
                break;
            hiddenBlock.setVisible(false);
            hiddenBlock.setLineCount(0);
        }
    }

    m_editor->document()->markContentsDirty(0, m_editor->document()->characterCount());
    m_editor->viewport()->update();
}

void IniEditorPage::collapseCurrentSection()
{
    const int lineNumber = m_editor->textCursor().blockNumber() + 1;
    const int sectionIndex = m_analysis.sectionIndexForLine(lineNumber);
    if (sectionIndex < 0 || sectionIndex >= m_analysis.sections.size())
        return;

    const int startLine = m_analysis.sections.at(sectionIndex).startLine;
    if (m_collapsedSectionLines.contains(startLine))
        m_collapsedSectionLines.remove(startLine);
    else
        m_collapsedSectionLines.insert(startLine);
    applyFolding();
}

void IniEditorPage::expandAllSections()
{
    m_collapsedSectionLines.clear();
    applyFolding();
}

} // namespace flatlas::editors
