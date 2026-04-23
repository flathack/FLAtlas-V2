#include "IniEditorPage.h"

#include "IniCodeEditor.h"
#include "IniFindReplaceDialog.h"
#include "IniMiniMap.h"
#include "IniSyntaxHighlighter.h"
#include "core/EditingContext.h"
#include "core/PathUtils.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextDocumentLayout>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTabBar>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <functional>

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

QString activeProjectRoot()
{
    const auto &ctx = flatlas::core::EditingContext::instance();
    if (!ctx.hasContext())
        return {};
    return QDir::fromNativeSeparators(QFileInfo(ctx.primaryGamePath()).absoluteFilePath());
}

QString normalizeEditorText(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

QString normalizedPath(const QString &path)
{
    return QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath());
}

bool pathIsInsideRoot(const QString &path, const QString &rootPath)
{
    if (path.trimmed().isEmpty() || rootPath.trimmed().isEmpty())
        return false;

    const QString normalizedFilePath = normalizedPath(path);
    QString normalizedRootPath = normalizedPath(rootPath);
    if (!normalizedRootPath.endsWith(QLatin1Char('/')))
        normalizedRootPath += QLatin1Char('/');
    return normalizedFilePath.startsWith(normalizedRootPath, Qt::CaseInsensitive)
        || normalizedFilePath.compare(normalizedRootPath.left(normalizedRootPath.size() - 1), Qt::CaseInsensitive) == 0;
}

QString normalizeWorkspaceRoot(QString rootPath)
{
    rootPath = normalizedPath(rootPath);
    if (rootPath.isEmpty())
        return {};

    const QFileInfo info(rootPath);
    if (info.fileName().compare(QStringLiteral("DATA"), Qt::CaseInsensitive) == 0) {
        const QString parentPath = info.dir().absolutePath();
        if (!parentPath.isEmpty())
            return normalizedPath(parentPath);
    }

    return rootPath;
}

QString workspaceRootForFile(const QString &filePath)
{
    const QString projectRoot = activeProjectRoot();
    if (!projectRoot.isEmpty() && pathIsInsideRoot(filePath, projectRoot))
        return projectRoot;

    const QString dataRoot = activeDataRoot();
    if (!dataRoot.isEmpty() && pathIsInsideRoot(filePath, dataRoot))
        return normalizeWorkspaceRoot(dataRoot);

    QFileInfo info(filePath);
    QDir dir = info.isDir() ? QDir(info.absoluteFilePath()) : info.absoluteDir();
    while (dir.exists() && !dir.isRoot()) {
        if (dir.dirName().compare(QStringLiteral("DATA"), Qt::CaseInsensitive) == 0)
            return normalizeWorkspaceRoot(dir.absolutePath());
        if (!dir.cdUp())
            break;
    }

    return info.isDir() ? normalizedPath(info.absoluteFilePath()) : normalizedPath(info.absolutePath());
}

QWidget *focusedWidgetIn(QWidget *scope)
{
    QWidget *focus = QApplication::focusWidget();
    if (!focus)
        return nullptr;
    return (focus == scope || scope->isAncestorOf(focus)) ? focus : nullptr;
}

}

IniEditorPage::IniEditorPage(QWidget *parent)
    : QWidget(parent)
    , m_analysisTimer(new QTimer(this))
    , m_autosaveTimer(new QTimer(this))
    , m_searchWatcher(new QFutureWatcher<QVector<IniSearchMatch>>(this))
{
    setupUi();

    m_analysisTimer->setSingleShot(true);
    m_analysisTimer->setInterval(250);
    connect(m_analysisTimer, &QTimer::timeout, this, &IniEditorPage::runAnalysis);

    m_autosaveTimer->setInterval(15000);
    connect(m_autosaveTimer, &QTimer::timeout, this, [this]() {
        for (auto &session : m_sessions) {
            if (session.dirty)
                writeRecoverySnapshot(session);
        }
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
    rootLayout->addWidget(m_toolbar);

    m_breadcrumbBar = new QWidget(this);
    auto *breadcrumbLayout = new QHBoxLayout(m_breadcrumbBar);
    breadcrumbLayout->setContentsMargins(8, 6, 8, 6);
    breadcrumbLayout->setSpacing(4);
    rootLayout->addWidget(m_breadcrumbBar);

    m_fileTabs = new QTabBar(this);
    m_fileTabs->setObjectName(QStringLiteral("iniFileTabs"));
    m_fileTabs->setDocumentMode(true);
    m_fileTabs->setTabsClosable(true);
    m_fileTabs->setMovable(true);
    m_fileTabs->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_fileTabs, &QTabBar::currentChanged, this, &IniEditorPage::onFileTabChanged);
    connect(m_fileTabs, &QTabBar::tabCloseRequested, this, [this](int index) { closeSession(index); });
    connect(m_fileTabs, &QTabBar::tabMoved, this, [this](int from, int to) {
        if (from < 0 || to < 0 || from >= m_sessions.size() || to >= m_sessions.size() || from == to)
            return;
        m_sessions.move(from, to);
        if (m_currentSessionIndex == from)
            m_currentSessionIndex = to;
        else if (from < m_currentSessionIndex && to >= m_currentSessionIndex)
            --m_currentSessionIndex;
        else if (from > m_currentSessionIndex && to <= m_currentSessionIndex)
            ++m_currentSessionIndex;
        updateTitle();
    });
    connect(m_fileTabs, &QTabBar::customContextMenuRequested, this, [this](const QPoint &pos) {
        const int tabIndex = m_fileTabs->tabAt(pos);
        if (tabIndex < 0)
            return;

        QMenu menu(this);
        auto *closeAction = menu.addAction(tr("Tab schließen"));
        auto *closeLeftAction = menu.addAction(tr("Alle links schließen"));
        auto *closeRightAction = menu.addAction(tr("Alle rechts schließen"));
        auto *closeAllAction = menu.addAction(tr("Alle Tabs schließen"));
        const QAction *selected = menu.exec(m_fileTabs->mapToGlobal(pos));
        if (selected == closeAction) {
            closeSession(tabIndex);
            return;
        }
        if (selected == closeLeftAction) {
            for (int i = tabIndex - 1; i >= 0; --i)
                closeSession(i);
            return;
        }
        if (selected == closeRightAction) {
            for (int i = m_fileTabs->count() - 1; i > tabIndex; --i)
                closeSession(i);
            return;
        }
        if (selected == closeAllAction) {
            for (int i = m_fileTabs->count() - 1; i >= 0; --i)
                closeSession(i);
        }
    });
    rootLayout->addWidget(m_fileTabs);

    setupWorkspace();
    setupShortcuts();

    m_findReplaceDialog = new IniFindReplaceDialog(this);
    connect(m_findReplaceDialog, &IniFindReplaceDialog::findNextRequested, this, &IniEditorPage::findNext);
    connect(m_findReplaceDialog, &IniFindReplaceDialog::findPreviousRequested, this, &IniEditorPage::findPrev);
    connect(m_findReplaceDialog, &IniFindReplaceDialog::replaceAllRequested, this, &IniEditorPage::replaceAll);
    connect(m_findReplaceDialog, &IniFindReplaceDialog::globalSearchRequested, this, &IniEditorPage::startGlobalSearch);
    connect(m_findReplaceDialog, &IniFindReplaceDialog::goToLineRequested, this, &IniEditorPage::goToLine);

    updateTabBarVisibility();
    clearActiveSession();
}

void IniEditorPage::setupToolbar()
{
    m_toolbar = new QToolBar(this);
    m_toolbar->setMovable(false);

    auto *openAction = m_toolbar->addAction(tr("Open"));
    connect(openAction, &QAction::triggered, this, [this]() {
        const QString selected = QFileDialog::getOpenFileName(this,
                                                              tr("Open File Editor File"),
                                                              preferredProjectRoot(),
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
                                                              filePath(),
                                                              tr("INI Files (*.ini);;All Files (*)"));
        if (!selected.isEmpty())
            saveAs(selected);
    });

    auto *findAction = m_toolbar->addAction(tr("Find / Replace"));
    connect(findAction, &QAction::triggered, this, &IniEditorPage::showFindReplaceDialog);

    m_toolbar->addSeparator();

    auto *undoAction = m_toolbar->addAction(tr("Undo"));
    connect(undoAction, &QAction::triggered, this, [this]() {
        if (m_editorStack->currentWidget() != m_emptyEditorPage)
            m_editor->undo();
    });
    auto *redoAction = m_toolbar->addAction(tr("Redo"));
    connect(redoAction, &QAction::triggered, this, [this]() {
        if (m_editorStack->currentWidget() != m_emptyEditorPage)
            m_editor->redo();
    });

    m_toolbar->addSeparator();

    auto *cutAction = m_toolbar->addAction(tr("Cut"));
    connect(cutAction, &QAction::triggered, this, &IniEditorPage::cutCurrentSelection);
    auto *copyAction = m_toolbar->addAction(tr("Copy"));
    connect(copyAction, &QAction::triggered, this, &IniEditorPage::copyCurrentSelection);
    auto *pasteAction = m_toolbar->addAction(tr("Paste"));
    connect(pasteAction, &QAction::triggered, this, [this]() {
        if (m_editorStack->currentWidget() != m_emptyEditorPage)
            m_editor->paste();
    });

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

    m_statusLabel = new QLabel(this);
    m_toolbar->addWidget(m_statusLabel);

    m_toolbar->addSeparator();
    m_summaryLabel = new QLabel(this);
    m_toolbar->addWidget(m_summaryLabel);
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
    m_fileTree->setUniformRowHeights(true);
    for (int column = 1; column < 4; ++column)
        m_fileTree->hideColumn(column);
    connect(m_fileTree, &QTreeView::doubleClicked, this, &IniEditorPage::openFromFileTree);
    connect(m_fileTree, &QTreeView::expanded, this, [this](const QModelIndex &) { saveTreeState(); });
    connect(m_fileTree, &QTreeView::collapsed, this, [this](const QModelIndex &) { saveTreeState(); });
    connect(m_fileTree->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) { saveTreeState(); });
    connect(m_fileTree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex &, const QModelIndex &) { saveTreeState(); });
    leftLayout->addWidget(m_fileTree, 1);

    auto *centerSplitter = new QSplitter(Qt::Horizontal, workspaceSplitter);

    m_editorStack = new QStackedWidget(centerSplitter);
    m_emptyEditorPage = new QWidget(m_editorStack);
    auto *emptyLayout = new QVBoxLayout(m_emptyEditorPage);
    emptyLayout->addStretch();
    auto *emptyLabel = new QLabel(tr("Select a readable INI-like file from the tree to open it in a sub-tab."), m_emptyEditorPage);
    emptyLabel->setWordWrap(true);
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyLabel);
    emptyLayout->addStretch();
    m_editorStack->addWidget(m_emptyEditorPage);

    auto *editorHost = new QWidget(m_editorStack);
    auto *editorHostLayout = new QHBoxLayout(editorHost);
    editorHostLayout->setContentsMargins(0, 0, 0, 0);
    editorHostLayout->setSpacing(4);

    auto *editorMinimapSplitter = new QSplitter(Qt::Horizontal, editorHost);
    m_editor = new IniCodeEditor(editorMinimapSplitter);
    m_editor->setObjectName(QStringLiteral("iniCodeEditor"));
    connect(m_editor, &QPlainTextEdit::textChanged, this, &IniEditorPage::onTextChanged);
    connect(m_editor, &QPlainTextEdit::cursorPositionChanged, this, &IniEditorPage::refreshCurrentSectionFromCursor);

    m_minimap = new IniMiniMap(editorMinimapSplitter);
    m_minimap->setObjectName(QStringLiteral("iniMiniMap"));
    QFont minimapFont = m_editor->font();
    minimapFont.setPointSizeF(qMax(5.0, minimapFont.pointSizeF() * 0.6));
    m_minimap->setFont(minimapFont);
    m_minimap->setMaximumWidth(160);
    editorMinimapSplitter->setStretchFactor(0, 5);
    editorMinimapSplitter->setStretchFactor(1, 1);
    editorHostLayout->addWidget(editorMinimapSplitter, 1);

    m_editorStack->addWidget(editorHost);
    centerSplitter->addWidget(m_editorStack);

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

    rightLayout->addWidget(new QLabel(tr("Keys in selected section"), rightPane));
    m_keyFilterEdit = new QLineEdit(rightPane);
    m_keyFilterEdit->setPlaceholderText(tr("Filter keys"));
    rightLayout->addWidget(m_keyFilterEdit);

    m_keyList = new QListWidget(rightPane);
    m_keyList->setObjectName(QStringLiteral("iniKeyList"));
    rightLayout->addWidget(m_keyList, 2);

    connect(m_sectionFilterEdit, &QLineEdit::textChanged, this, &IniEditorPage::populateSectionList);
    connect(m_keyFilterEdit, &QLineEdit::textChanged, this, [this]() {
        if (const auto *session = currentSession())
            populateKeyListForSectionStartLine(session->selectedSectionStartLine);
    });
    connect(m_sectionList, &QListWidget::currentRowChanged, this, [this](int row) {
        auto *session = currentSession();
        if (!session) {
            m_keyList->clear();
            return;
        }

        if (row < 0) {
            session->selectedSectionStartLine = -1;
            m_keyList->clear();
            return;
        }

        auto *item = m_sectionList->item(row);
        if (!item)
            return;
        session->selectedSectionStartLine = item->data(StartLineRole).toInt();
        populateKeyListForSectionStartLine(session->selectedSectionStartLine);
    });
    connect(m_sectionList, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        auto *item = m_sectionList->itemAt(pos);
        auto *session = currentSession();
        if (!item || !session)
            return;

        QMenu menu(this);
        auto *jumpAction = menu.addAction(tr("Zur Sektion springen"));
        auto *copyAction = menu.addAction(tr("Sektion kopieren"));
        auto *collapseAction = menu.addAction(tr("Sektion einklappen"));
        const QAction *selected = menu.exec(m_sectionList->viewport()->mapToGlobal(pos));
        const int startLine = item->data(StartLineRole).toInt();
        if (selected == jumpAction) {
            goToLine(startLine);
        } else if (selected == copyAction) {
            IniSectionInfo section;
            section.name = item->text();
            section.startLine = startLine;
            section.endLine = item->data(EndLineRole).toInt();
            copySelectionToCollector(IniAnalysisService::sectionText(session->document->toPlainText(), section),
                                     tr("Section %1").arg(item->text()));
        } else if (selected == collapseAction) {
            session->collapsedSectionLines.insert(startLine);
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
    findAction->setShortcuts(QKeySequence::keyBindings(QKeySequence::Find));
    findAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(findAction, &QAction::triggered, this, &IniEditorPage::showFindReplaceDialog);
    addAction(findAction);

    auto *openAction = new QAction(this);
    openAction->setShortcuts(QKeySequence::keyBindings(QKeySequence::Open));
    openAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(openAction, &QAction::triggered, this, [this]() {
        const QString selected = QFileDialog::getOpenFileName(this,
                                                              tr("Open File Editor File"),
                                                              preferredProjectRoot(),
                                                              tr("INI-like Files (*.ini *.cfg *.txt *.bini);;All Files (*)"));
        if (!selected.isEmpty())
            emit openFileRequested(selected, QString(), 0);
    });
    addAction(openAction);

    auto *copyAction = new QAction(this);
    copyAction->setShortcuts(QKeySequence::keyBindings(QKeySequence::Copy));
    copyAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(copyAction, &QAction::triggered, this, [this]() {
        QWidget *focus = focusedWidgetIn(this);
        if (auto *lineEdit = qobject_cast<QLineEdit *>(focus)) {
            lineEdit->copy();
            return;
        }
        if (auto *plainEdit = qobject_cast<QPlainTextEdit *>(focus)) {
            plainEdit->copy();
            return;
        }
        if (auto *textEdit = qobject_cast<QTextEdit *>(focus)) {
            textEdit->copy();
            return;
        }
        copyCurrentSelection();
    });
    addAction(copyAction);

    auto *cutAction = new QAction(this);
    cutAction->setShortcuts(QKeySequence::keyBindings(QKeySequence::Cut));
    cutAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(cutAction, &QAction::triggered, this, [this]() {
        QWidget *focus = focusedWidgetIn(this);
        if (auto *lineEdit = qobject_cast<QLineEdit *>(focus)) {
            lineEdit->cut();
            return;
        }
        if (auto *plainEdit = qobject_cast<QPlainTextEdit *>(focus)) {
            plainEdit->cut();
            return;
        }
        if (auto *textEdit = qobject_cast<QTextEdit *>(focus)) {
            textEdit->cut();
            return;
        }
        cutCurrentSelection();
    });
    addAction(cutAction);

    auto *pasteAction = new QAction(this);
    pasteAction->setShortcuts(QKeySequence::keyBindings(QKeySequence::Paste));
    pasteAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(pasteAction, &QAction::triggered, this, [this]() {
        QWidget *focus = focusedWidgetIn(this);
        if (auto *lineEdit = qobject_cast<QLineEdit *>(focus)) {
            lineEdit->paste();
            return;
        }
        if (auto *plainEdit = qobject_cast<QPlainTextEdit *>(focus)) {
            plainEdit->paste();
            return;
        }
        if (auto *textEdit = qobject_cast<QTextEdit *>(focus)) {
            textEdit->paste();
            return;
        }
        if (m_editorStack->currentWidget() != m_emptyEditorPage)
            m_editor->paste();
    });
    addAction(pasteAction);

    auto *undoAction = new QAction(this);
    undoAction->setShortcuts(QKeySequence::keyBindings(QKeySequence::Undo));
    undoAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(undoAction, &QAction::triggered, this, [this]() {
        QWidget *focus = focusedWidgetIn(this);
        if (auto *lineEdit = qobject_cast<QLineEdit *>(focus)) {
            lineEdit->undo();
            return;
        }
        if (auto *plainEdit = qobject_cast<QPlainTextEdit *>(focus)) {
            plainEdit->undo();
            return;
        }
        if (auto *textEdit = qobject_cast<QTextEdit *>(focus)) {
            textEdit->undo();
            return;
        }
    });
    addAction(undoAction);

    auto *redoAction = new QAction(this);
    redoAction->setShortcuts(QKeySequence::keyBindings(QKeySequence::Redo));
    redoAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(redoAction, &QAction::triggered, this, [this]() {
        QWidget *focus = focusedWidgetIn(this);
        if (auto *lineEdit = qobject_cast<QLineEdit *>(focus)) {
            lineEdit->redo();
            return;
        }
        if (auto *plainEdit = qobject_cast<QPlainTextEdit *>(focus)) {
            plainEdit->redo();
            return;
        }
        if (auto *textEdit = qobject_cast<QTextEdit *>(focus)) {
            textEdit->redo();
            return;
        }
    });
    addAction(redoAction);

    auto *selectAllAction = new QAction(this);
    selectAllAction->setShortcuts(QKeySequence::keyBindings(QKeySequence::SelectAll));
    selectAllAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(selectAllAction, &QAction::triggered, this, [this]() {
        QWidget *focus = focusedWidgetIn(this);
        if (auto *lineEdit = qobject_cast<QLineEdit *>(focus)) {
            lineEdit->selectAll();
            return;
        }
        if (auto *plainEdit = qobject_cast<QPlainTextEdit *>(focus)) {
            plainEdit->selectAll();
            return;
        }
        if (auto *textEdit = qobject_cast<QTextEdit *>(focus)) {
            textEdit->selectAll();
            return;
        }
    });
    addAction(selectAllAction);

    auto *saveAction = new QAction(this);
    saveAction->setShortcuts(QKeySequence::keyBindings(QKeySequence::Save));
    saveAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(saveAction, &QAction::triggered, this, [this]() { save(); });
    addAction(saveAction);
}

void IniEditorPage::openWorkspace(const QString &rootPath)
{
    setTreeRootPath(rootPath);
    refreshRecentFiles();
    updateBreadcrumbs();
    updateRecoveryUi();
    updateStatusSummary();
    updateTitle();
}

bool IniEditorPage::openFile(const QString &filePath)
{
    if (filePath.isEmpty())
        return false;

    const int existingIndex = findSessionIndex(filePath);
    if (existingIndex >= 0) {
        activateSession(existingIndex);
        return true;
    }

    ensureWorkspaceRootForFile(filePath);

    QString errorMessage;
    if (!openFileInSession(filePath, &errorMessage))
        return false;

    rememberRecentFile(filePath);
    updateWorkspaceContext();
    updateRecoveryUi();
    updateTitle();
    return true;
}

bool IniEditorPage::save()
{
    auto *session = currentSession();
    if (!session || session->filePath.isEmpty())
        return false;
    return saveSession(*session, session->filePath);
}

bool IniEditorPage::saveAs(const QString &filePath)
{
    auto *session = currentSession();
    if (!session || filePath.isEmpty())
        return false;
    return saveSession(*session, filePath);
}

bool IniEditorPage::saveAllDirtyTabs(QWidget *dialogParent)
{
    QWidget *promptParent = dialogParent ? dialogParent : this;
    for (auto &session : m_sessions) {
        if (!session.dirty)
            continue;

        QString targetPath = session.filePath;
        if (targetPath.isEmpty()) {
            targetPath = QFileDialog::getSaveFileName(promptParent,
                                                      tr("Save INI File"),
                                                      QString(),
                                                      tr("INI Files (*.ini);;All Files (*)"));
            if (targetPath.isEmpty())
                return false;
        }

        if (!saveSession(session, targetPath))
            return false;
    }

    return true;
}

QString IniEditorPage::filePath() const
{
    const auto *session = currentSession();
    return session ? session->filePath : QString();
}

QString IniEditorPage::fileName() const
{
    const auto *session = currentSession();
    return session && !session->filePath.isEmpty()
        ? QFileInfo(session->filePath).fileName()
        : tr("Untitled");
}

bool IniEditorPage::isDirty() const
{
    for (const auto &session : m_sessions) {
        if (session.dirty)
            return true;
    }
    return false;
}

void IniEditorPage::focusSearch(const QString &text)
{
    showFindReplaceDialog();
    m_findReplaceDialog->setFindText(text);
    findNext();
}

void IniEditorPage::goToLine(int lineNumber)
{
    if (lineNumber <= 0 || m_editorStack->currentWidget() == m_emptyEditorPage)
        return;
    m_editor->goToLine(lineNumber);
    m_editor->setFocus();
    refreshCurrentSectionFromCursor();
}

void IniEditorPage::onTextChanged()
{
    if (m_loading || !currentSession())
        return;

    updateDirtyState();
    m_analysisTimer->start();
}

void IniEditorPage::runAnalysis()
{
    auto *session = currentSession();
    if (!session) {
        m_sectionList->clear();
        m_keyList->clear();
        m_diagnosticsModel->removeRows(0, m_diagnosticsModel->rowCount());
        updateStatusSummary();
        updateRecoveryUi();
        return;
    }

    session->analysis = IniAnalysisService::analyzeText(session->document->toPlainText());
    populateSectionList();
    refreshCurrentSectionFromCursor();
    updateDiagnosticsView();
    updateStatusSummary();
    applyFolding();
}

void IniEditorPage::refreshCurrentSectionFromCursor()
{
    auto *session = currentSession();
    if (!session) {
        m_keyList->clear();
        return;
    }

    const int lineNumber = m_editor->textCursor().blockNumber() + 1;
    const int sectionIndex = session->analysis.sectionIndexForLine(lineNumber);
    if (sectionIndex < 0 || sectionIndex >= session->analysis.sections.size()) {
        session->selectedSectionStartLine = -1;
        m_keyList->clear();
        return;
    }

    const int startLine = session->analysis.sections.at(sectionIndex).startLine;
    session->selectedSectionStartLine = startLine;
    QSignalBlocker blocker(m_sectionList);
    for (int row = 0; row < m_sectionList->count(); ++row) {
        auto *item = m_sectionList->item(row);
        if (item && item->data(StartLineRole).toInt() == startLine) {
            m_sectionList->setCurrentRow(row);
            break;
        }
    }
    populateKeyListForSectionStartLine(startLine);
}

void IniEditorPage::startGlobalSearch()
{
    if (m_searchWatcher->isRunning() || !m_findReplaceDialog)
        return;

    IniSearchOptions options;
    options.query = m_findReplaceDialog->globalSearchText().trimmed();
    options.useRegex = m_findReplaceDialog->useRegex();
    options.caseSensitive = m_findReplaceDialog->caseSensitive();
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

void IniEditorPage::onFileTabChanged(int index)
{
    if (index == m_currentSessionIndex)
        return;
    activateSession(index);
}

void IniEditorPage::openFromFileTree(const QModelIndex &index)
{
    const QString path = m_fileModel->filePath(index);
    if (QFileInfo(path).isDir())
        return;
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
    auto *session = currentSession();
    auto *item = m_historyList->currentItem();
    if (!session || !item)
        return;

    const int snapshotId = item->data(SnapshotIdRole).toInt();
    for (const auto &snapshot : session->snapshots) {
        if (snapshot.id != snapshotId)
            continue;
        m_loading = true;
        session->document->setPlainText(snapshot.text);
        m_loading = false;
        updateDirtyState();
        runAnalysis();
        return;
    }
}

void IniEditorPage::activateClipboardItem(QListWidgetItem *item)
{
    if (!item || m_editorStack->currentWidget() == m_emptyEditorPage)
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
    auto *session = currentSession();
    if (!session || session->pendingRecoveryText.isEmpty())
        return;
    m_loading = true;
    session->document->setPlainText(session->pendingRecoveryText);
    m_loading = false;
    updateDirtyState();
    runAnalysis();
}

void IniEditorPage::discardRecoverySnapshot()
{
    auto *session = currentSession();
    if (!session)
        return;
    session->pendingRecoveryText.clear();
    clearRecoverySnapshot(*session);
    updateRecoveryUi();
}

void IniEditorPage::updateTitle()
{
    QString title = tr("File Editor");
    if (isDirty())
        title += QStringLiteral(" *");
    emit titleChanged(title);
}

void IniEditorPage::updateActiveTabText()
{
    if (!m_fileTabs)
        return;
    for (int i = 0; i < m_sessions.size(); ++i)
        m_fileTabs->setTabText(i, sessionDisplayName(m_sessions.at(i)));
}

void IniEditorPage::updateDirtyState()
{
    auto *session = currentSession();
    if (!session)
        return;

    const bool dirtyNow = (session->document->toPlainText() != session->savedText);
    if (session->dirty != dirtyNow) {
        session->dirty = dirtyNow;
        updateActiveTabText();
        updateTitle();
    }
}

void IniEditorPage::updateWorkspaceContext()
{
    if (m_treeRootPath.isEmpty())
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

    const QString workspaceRoot = preferredProjectRoot();
    const QString activePath = !filePath().isEmpty() ? normalizedPath(filePath()) : normalizedPath(workspaceRoot);
    if (activePath.isEmpty()) {
        breadcrumbLayout->addWidget(new QLabel(tr("No workspace loaded"), m_breadcrumbBar));
        breadcrumbLayout->addStretch();
        return;
    }

    QString relativePath = workspaceRoot.isEmpty()
        ? activePath
        : QDir(workspaceRoot).relativeFilePath(activePath);
    QStringList parts = relativePath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.isEmpty() && !workspaceRoot.isEmpty())
        parts.append(QFileInfo(workspaceRoot).fileName());

    QString currentPath = workspaceRoot.isEmpty() ? QFileInfo(activePath).absolutePath() : workspaceRoot;
    for (int index = 0; index < parts.size(); ++index) {
        const QString part = parts.at(index);
        if (index > 0)
            breadcrumbLayout->addWidget(new QLabel(QStringLiteral("/"), m_breadcrumbBar));

        const bool last = (index == parts.size() - 1);
        auto *button = new QToolButton(m_breadcrumbBar);
        button->setText(part);
        button->setAutoRaise(true);
        if (!last) {
            currentPath = QDir(currentPath).filePath(part);
            const QString targetPath = currentPath;
            connect(button, &QToolButton::clicked, this, [this, targetPath]() {
                revealPathInTree(targetPath);
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
    QString resolvedRoot = rootPath;
    if (resolvedRoot.isEmpty())
        resolvedRoot = QFileInfo(filePath()).absolutePath();
    if (resolvedRoot.isEmpty())
        resolvedRoot = activeProjectRoot();
    if (resolvedRoot.isEmpty())
        resolvedRoot = activeDataRoot();
    if (resolvedRoot.isEmpty())
        resolvedRoot = QDir::currentPath();

    m_treeRootPath = normalizeWorkspaceRoot(resolvedRoot);
    const QModelIndex rootIndex = m_fileModel->setRootPath(m_treeRootPath);
    m_fileTree->setRootIndex(rootIndex);
    m_fileTree->setCurrentIndex(rootIndex);
    restoreTreeState();
}

void IniEditorPage::restoreTreeState()
{
    const QString settingsKey = workspaceSettingsKey();
    if (settingsKey.isEmpty())
        return;

    QSettings settings;
    const QStringList expandedPaths = settings.value(settingsKey + QStringLiteral("/expandedPaths")).toStringList();
    const QString selectedPath = settings.value(settingsKey + QStringLiteral("/selectedPath")).toString();
    const int scrollValue = settings.value(settingsKey + QStringLiteral("/scrollValue"), 0).toInt();
    const QString currentRoot = m_treeRootPath;
    QTimer::singleShot(0, this, [this, currentRoot, expandedPaths, selectedPath, scrollValue]() {
        if (currentRoot != m_treeRootPath)
            return;

        for (const QString &relativePath : expandedPaths) {
            const QString absolutePath = QDir(m_treeRootPath).filePath(relativePath);
            const QModelIndex index = m_fileModel->index(absolutePath);
            if (index.isValid())
                m_fileTree->expand(index);
        }

        if (!selectedPath.isEmpty()) {
            const QModelIndex selectedIndex = m_fileModel->index(QDir(m_treeRootPath).filePath(selectedPath));
            if (selectedIndex.isValid())
                m_fileTree->setCurrentIndex(selectedIndex);
        }

        m_fileTree->verticalScrollBar()->setValue(scrollValue);
    });
}

void IniEditorPage::saveTreeState() const
{
    const QString settingsKey = workspaceSettingsKey();
    if (settingsKey.isEmpty() || !m_fileTree || !m_fileModel)
        return;

    QStringList expandedPaths;
    std::function<void(const QModelIndex &)> collectExpanded = [&](const QModelIndex &parent) {
        const int rowCount = m_fileModel->rowCount(parent);
        for (int row = 0; row < rowCount; ++row) {
            const QModelIndex index = m_fileModel->index(row, 0, parent);
            if (!index.isValid())
                continue;
            const QString path = m_fileModel->filePath(index);
            if (!QFileInfo(path).isDir())
                continue;
            if (m_fileTree->isExpanded(index)) {
                expandedPaths.append(QDir(m_treeRootPath).relativeFilePath(path));
                collectExpanded(index);
            }
        }
    };
    collectExpanded(m_fileTree->rootIndex());

    QSettings settings;
    settings.setValue(settingsKey + QStringLiteral("/expandedPaths"), expandedPaths);
    const QModelIndex currentIndex = m_fileTree->currentIndex();
    const QString selectedPath = currentIndex.isValid()
        ? QDir(m_treeRootPath).relativeFilePath(m_fileModel->filePath(currentIndex))
        : QString();
    settings.setValue(settingsKey + QStringLiteral("/selectedPath"), selectedPath);
    settings.setValue(settingsKey + QStringLiteral("/scrollValue"), m_fileTree->verticalScrollBar()->value());
}

void IniEditorPage::revealPathInTree(const QString &path)
{
    if (path.trimmed().isEmpty())
        return;

    QModelIndex index = m_fileModel->index(path);
    if (!index.isValid())
        return;

    QModelIndex parent = index.parent();
    while (parent.isValid()) {
        m_fileTree->expand(parent);
        parent = parent.parent();
    }
    m_fileTree->setCurrentIndex(index);
    m_fileTree->scrollTo(index, QAbstractItemView::PositionAtCenter);
    saveTreeState();
}

QString IniEditorPage::preferredProjectRoot() const
{
    if (!m_treeRootPath.isEmpty())
        return m_treeRootPath;

    const QString projectRoot = activeProjectRoot();
    if (!projectRoot.isEmpty() && !filePath().isEmpty() && pathIsInsideRoot(filePath(), projectRoot))
        return projectRoot;

    const QString dataRoot = activeDataRoot();
    if (!dataRoot.isEmpty() && !filePath().isEmpty() && pathIsInsideRoot(filePath(), dataRoot))
        return normalizeWorkspaceRoot(dataRoot);
    if (!filePath().isEmpty())
        return workspaceRootForFile(filePath());
    if (!projectRoot.isEmpty())
        return projectRoot;
    return normalizeWorkspaceRoot(dataRoot);
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

IniEditorPage::FileSession *IniEditorPage::currentSession()
{
    return (m_currentSessionIndex >= 0 && m_currentSessionIndex < m_sessions.size())
        ? &m_sessions[m_currentSessionIndex]
        : nullptr;
}

const IniEditorPage::FileSession *IniEditorPage::currentSession() const
{
    return (m_currentSessionIndex >= 0 && m_currentSessionIndex < m_sessions.size())
        ? &m_sessions[m_currentSessionIndex]
        : nullptr;
}

int IniEditorPage::findSessionIndex(const QString &filePath) const
{
    const QString normalizedFilePath = normalizedPath(filePath);
    for (int i = 0; i < m_sessions.size(); ++i) {
        if (normalizedPath(m_sessions.at(i).filePath).compare(normalizedFilePath, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

void IniEditorPage::activateSession(int index)
{
    if (index < 0 || index >= m_sessions.size()) {
        clearActiveSession();
        return;
    }

    storeCurrentSessionState();
    setCurrentSessionIndex(index);
    bindActiveSession();
}

void IniEditorPage::storeCurrentSessionState()
{
    auto *session = currentSession();
    if (!session || m_editorStack->currentWidget() == m_emptyEditorPage)
        return;

    session->cursorPosition = m_editor->textCursor().position();
    session->verticalScrollValue = m_editor->verticalScrollBar()->value();
    session->horizontalScrollValue = m_editor->horizontalScrollBar()->value();
}

void IniEditorPage::bindActiveSession()
{
    auto *session = currentSession();
    if (!session) {
        clearActiveSession();
        return;
    }

    m_loading = true;
    m_editor->setDocument(session->document);
    m_minimap->setSourceEditor(m_editor);
    QTextCursor cursor(session->document);
    cursor.setPosition(qBound(0, session->cursorPosition, qMax(0, session->document->characterCount() - 1)));
    m_editor->setTextCursor(cursor);
    m_loading = false;

    m_editorStack->setCurrentIndex(1);
    m_editor->verticalScrollBar()->setValue(session->verticalScrollValue);
    m_editor->horizontalScrollBar()->setValue(session->horizontalScrollValue);
    populateSectionList();
    refreshCurrentSectionFromCursor();
    updateDiagnosticsView();
    refreshSnapshotList();
    updateRecoveryUi();
    updateStatusSummary();
    applyFolding();
    updateBreadcrumbs();
    updateActiveTabText();
    updateTitle();
}

void IniEditorPage::clearActiveSession()
{
    m_currentSessionIndex = -1;
    if (m_editorStack)
        m_editorStack->setCurrentWidget(m_emptyEditorPage);
    if (m_minimap)
        m_minimap->setSourceEditor(nullptr);
    m_sectionList->clear();
    m_keyList->clear();
    m_diagnosticsModel->removeRows(0, m_diagnosticsModel->rowCount());
    m_historyList->clear();
    m_historyDiffView->clear();
    updateRecoveryUi();
    updateStatusSummary();
    updateBreadcrumbs();
    updateTitle();
}

void IniEditorPage::setCurrentSessionIndex(int index)
{
    m_currentSessionIndex = index;
    QSignalBlocker blocker(m_fileTabs);
    m_fileTabs->setCurrentIndex(index);
}

bool IniEditorPage::ensureWorkspaceRootForFile(const QString &filePath)
{
    if (!m_treeRootPath.isEmpty() && pathIsInsideRoot(filePath, m_treeRootPath))
        return true;

    setTreeRootPath(workspaceRootForFile(filePath));
    return true;
}

bool IniEditorPage::openFileInSession(const QString &filePath, QString *errorMessage)
{
    bool wasBini = false;
    const QString loadedDiskText = IniAnalysisService::loadIniLikeText(filePath, &wasBini);
    const QString diskText = normalizeEditorText(loadedDiskText);
    if (diskText.isNull()) {
        if (errorMessage)
            *errorMessage = tr("Could not read the selected file.");
        return false;
    }

    QString finalText = diskText;
    QString pendingRecoveryText;
    const QString recoveryPath = recoveryFilePathFor(filePath);
    if (QFileInfo::exists(recoveryPath)) {
        QFile recoveryFile(recoveryPath);
        if (recoveryFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString recoveredText = normalizeEditorText(QString::fromUtf8(recoveryFile.readAll()));
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
                pendingRecoveryText = recoveredText;
                if (prompt.clickedButton() == restoreButton)
                    finalText = recoveredText;
                if (prompt.clickedButton() == keepButton)
                    updateRecoveryUi();
                Q_UNUSED(restoreButton);
            }
        }
    }

    FileSession session;
    session.filePath = normalizedPath(filePath);
    session.savedText = diskText;
    session.pendingRecoveryText = pendingRecoveryText;
    session.document = new QTextDocument(this);
    session.document->setDocumentLayout(new QPlainTextDocumentLayout(session.document));
    session.document->setPlainText(finalText);
    session.highlighter = new IniSyntaxHighlighter(session.document);
    session.dirty = (session.document->toPlainText() != session.savedText);
    session.wasBini = wasBini;
    session.analysis = IniAnalysisService::analyzeText(session.document->toPlainText());

    m_sessions.append(session);
    m_fileTabs->addTab(sessionDisplayName(m_sessions.last()));
    updateTabBarVisibility();
    activateSession(m_sessions.size() - 1);
    return true;
}

bool IniEditorPage::saveSession(FileSession &session, const QString &targetPath)
{
    QFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;

    file.write(session.document->toPlainText().toUtf8());
    file.close();

    const QString oldPath = session.filePath;
    if (!oldPath.isEmpty() && normalizedPath(oldPath).compare(normalizedPath(targetPath), Qt::CaseInsensitive) != 0)
        QFile::remove(recoveryFilePathFor(oldPath));

    session.filePath = normalizedPath(targetPath);
    session.savedText = session.document->toPlainText();
    session.dirty = false;
    session.wasBini = false;
    rememberRecentFile(session.filePath);
    clearRecoverySnapshot(session);
    captureSnapshot(session, tr("Saved"));
    updateWorkspaceContext();
    updateRecoveryUi();
    updateActiveTabText();
    updateTitle();
    return true;
}

bool IniEditorPage::confirmCloseSession(int index)
{
    if (index < 0 || index >= m_sessions.size())
        return false;

    FileSession &session = m_sessions[index];
    if (!session.dirty)
        return true;

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Ungespeicherte Änderungen"));
    box.setText(tr("Im Sub-Tab \"%1\" gibt es ungespeicherte Änderungen.").arg(sessionDisplayName(session)));
    box.setInformativeText(tr("Möchtest du die Änderungen speichern, bevor geschlossen wird?"));
    auto *saveButton = box.addButton(tr("Speichern"), QMessageBox::AcceptRole);
    auto *discardButton = box.addButton(tr("Verwerfen"), QMessageBox::DestructiveRole);
    auto *cancelButton = box.addButton(tr("Abbrechen"), QMessageBox::RejectRole);
    box.setDefaultButton(qobject_cast<QPushButton *>(saveButton));
    box.exec();

    if (box.clickedButton() == cancelButton)
        return false;
    if (box.clickedButton() == discardButton) {
        clearRecoverySnapshot(session);
        return true;
    }
    if (box.clickedButton() == saveButton) {
        QString targetPath = session.filePath;
        if (targetPath.isEmpty()) {
            targetPath = QFileDialog::getSaveFileName(this,
                                                      tr("Save INI File"),
                                                      QString(),
                                                      tr("INI Files (*.ini);;All Files (*)"));
            if (targetPath.isEmpty())
                return false;
        }
        return saveSession(session, targetPath);
    }
    return false;
}

void IniEditorPage::closeSession(int index, bool force)
{
    if (index < 0 || index >= m_sessions.size())
        return;
    if (!force && !confirmCloseSession(index))
        return;

    FileSession session = m_sessions.at(index);
    clearRecoverySnapshot(session);
    if (session.document)
        session.document->deleteLater();

    const bool closingCurrent = (index == m_currentSessionIndex);
    if (!closingCurrent && index < m_currentSessionIndex)
        --m_currentSessionIndex;
    else if (closingCurrent)
        m_currentSessionIndex = -1;

    m_sessions.removeAt(index);
    {
        QSignalBlocker blocker(m_fileTabs);
        m_fileTabs->removeTab(index);
    }
    updateTabBarVisibility();

    if (m_sessions.isEmpty()) {
        clearActiveSession();
        return;
    }

    if (closingCurrent) {
        const int nextIndex = qMin(index, m_sessions.size() - 1);
        activateSession(nextIndex);
    } else {
        setCurrentSessionIndex(m_currentSessionIndex);
        updateActiveTabText();
        updateRecoveryUi();
        updateStatusSummary();
        updateBreadcrumbs();
        updateTitle();
    }
}

void IniEditorPage::updateTabBarVisibility()
{
    if (m_fileTabs)
        m_fileTabs->setVisible(m_fileTabs->count() > 0);
}

QString IniEditorPage::sessionDisplayName(const FileSession &session) const
{
    QString label = session.filePath.isEmpty()
        ? tr("Untitled")
        : QFileInfo(session.filePath).fileName();
    if (session.dirty)
        label += QStringLiteral(" *");
    return label;
}

QString IniEditorPage::workspaceSettingsKey() const
{
    if (m_treeRootPath.isEmpty())
        return {};
    const QByteArray hash = QCryptographicHash::hash(m_treeRootPath.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QStringLiteral("iniEditor/workspaces/") + QString::fromLatin1(hash);
}

void IniEditorPage::populateSectionList()
{
    const auto *session = currentSession();
    const QString filter = m_sectionFilterEdit ? m_sectionFilterEdit->text().trimmed() : QString();
    const int scrollValue = m_sectionList->verticalScrollBar()->value();
    const int selectedStartLine = session ? session->selectedSectionStartLine : -1;

    QSignalBlocker blocker(m_sectionList);
    m_sectionList->clear();
    if (!session)
        return;

    for (const auto &section : session->analysis.sections) {
        const QString label = section.displayLabel.isEmpty()
            ? QStringLiteral("[%1]").arg(section.name)
            : section.displayLabel;
        if (!filter.isEmpty() && !label.contains(filter, Qt::CaseInsensitive))
            continue;

        auto *item = new QListWidgetItem(label, m_sectionList);
        item->setData(StartLineRole, section.startLine);
        item->setData(EndLineRole, section.endLine);
        if (section.startLine == selectedStartLine)
            m_sectionList->setCurrentItem(item);
    }

    m_sectionList->verticalScrollBar()->setValue(scrollValue);
}

void IniEditorPage::populateKeyListForSectionStartLine(int startLine)
{
    m_keyList->clear();
    const auto *session = currentSession();
    if (!session || startLine <= 0)
        return;

    const QString keyFilter = m_keyFilterEdit ? m_keyFilterEdit->text().trimmed() : QString();
    for (const auto &section : session->analysis.sections) {
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
    const auto *session = currentSession();
    if (!session)
        return;

    for (const auto &diagnostic : session->analysis.diagnostics) {
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
        previewItem->setData(m_findReplaceDialog ? m_findReplaceDialog->findText() : QString(), SearchTextRole);
        m_searchResultsModel->appendRow({fileItem, lineItem, sectionItem, keyItem, previewItem});
    }
    m_searchResultsView->resizeColumnToContents(0);
    m_searchResultsView->resizeColumnToContents(1);
}

void IniEditorPage::updateStatusSummary()
{
    const auto *session = currentSession();
    if (!session) {
        m_summaryLabel->setText(tr("Open tabs: %1").arg(m_sessions.size()));
        return;
    }

    int errors = 0;
    int warnings = 0;
    for (const auto &diagnostic : session->analysis.diagnostics) {
        if (diagnostic.severity == IniDiagnosticSeverity::Error)
            ++errors;
        else if (diagnostic.severity == IniDiagnosticSeverity::Warning)
            ++warnings;
    }

    m_summaryLabel->setText(tr("Sections: %1 | Diagnostics: %2 errors, %3 warnings | Saved states: %4")
                                .arg(session->analysis.sections.size())
                                .arg(errors)
                                .arg(warnings)
                                .arg(session->snapshots.size()));
}

bool IniEditorPage::findWithCurrentOptions(bool forward)
{
    if (!m_findReplaceDialog)
        return false;

    const QString needle = m_findReplaceDialog->findText();
    if (needle.isEmpty())
        return false;

    QTextDocument::FindFlags flags;
    if (!forward)
        flags |= QTextDocument::FindBackward;
    if (m_findReplaceDialog->caseSensitive())
        flags |= QTextDocument::FindCaseSensitively;

    QTextCursor startCursor = m_editor->textCursor();
    QTextCursor found;
    if (m_findReplaceDialog->useRegex()) {
        QRegularExpression::PatternOptions patternOptions = QRegularExpression::NoPatternOption;
        if (!m_findReplaceDialog->caseSensitive())
            patternOptions |= QRegularExpression::CaseInsensitiveOption;
        const QRegularExpression regex(needle, patternOptions);
        if (!regex.isValid()) {
            m_statusLabel->setText(tr("Invalid regex"));
            return false;
        }
        found = m_editor->document()->find(regex, startCursor, flags);
        if (found.isNull()) {
            QTextCursor wrapCursor(m_editor->document());
            wrapCursor.movePosition(forward ? QTextCursor::Start : QTextCursor::End);
            found = m_editor->document()->find(regex, wrapCursor, flags);
        }
    } else {
        found = m_editor->document()->find(needle, startCursor, flags);
        if (found.isNull()) {
            QTextCursor wrapCursor(m_editor->document());
            wrapCursor.movePosition(forward ? QTextCursor::Start : QTextCursor::End);
            found = m_editor->document()->find(needle, wrapCursor, flags);
        }
    }

    if (found.isNull())
        return false;

    m_editor->setTextCursor(found);
    m_editor->ensureCursorVisible();
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
    auto *session = currentSession();
    if (!session || !m_findReplaceDialog)
        return;

    const QString searchText = m_findReplaceDialog->findText();
    if (searchText.isEmpty())
        return;

    QString content = session->document->toPlainText();
    int replacements = 0;
    const QString replacement = m_findReplaceDialog->replaceText();

    if (m_findReplaceDialog->useRegex()) {
        QRegularExpression::PatternOptions patternOptions = QRegularExpression::NoPatternOption;
        if (!m_findReplaceDialog->caseSensitive())
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
        content.replace(regex, replacement);
    } else {
        const Qt::CaseSensitivity cs = m_findReplaceDialog->caseSensitive() ? Qt::CaseSensitive : Qt::CaseInsensitive;
        int pos = 0;
        while ((pos = content.indexOf(searchText, pos, cs)) >= 0) {
            content.replace(pos, searchText.size(), replacement);
            pos += replacement.size();
            ++replacements;
        }
    }

    if (replacements > 0) {
        m_loading = true;
        session->document->setPlainText(content);
        m_loading = false;
        updateDirtyState();
        runAnalysis();
    }
    m_statusLabel->setText(tr("%1 replacements").arg(replacements));
}

void IniEditorPage::showFindReplaceDialog()
{
    if (!m_findReplaceDialog)
        return;
    m_findReplaceDialog->show();
    m_findReplaceDialog->raise();
    m_findReplaceDialog->focusFindField();
}

QString IniEditorPage::currentSectionText() const
{
    const auto *session = currentSession();
    if (!session)
        return {};

    const int lineNumber = m_editor->textCursor().blockNumber() + 1;
    const int sectionIndex = session->analysis.sectionIndexForLine(lineNumber);
    if (sectionIndex < 0 || sectionIndex >= session->analysis.sections.size())
        return {};
    return IniAnalysisService::sectionText(session->document->toPlainText(), session->analysis.sections.at(sectionIndex));
}

void IniEditorPage::copySelectionToCollector(const QString &text, const QString &label)
{
    if (text.trimmed().isEmpty())
        return;

    if (QClipboard *clipboard = QApplication::clipboard())
        clipboard->setText(text, QClipboard::Clipboard);

    auto *item = new QListWidgetItem(QStringLiteral("%1 | %2 chars").arg(label).arg(text.size()));
    item->setData(Qt::UserRole, text);
    m_clipboardList->insertItem(0, item);
    while (m_clipboardList->count() > 50)
        delete m_clipboardList->takeItem(m_clipboardList->count() - 1);
}

void IniEditorPage::copyCurrentSelection()
{
    if (m_editorStack->currentWidget() == m_emptyEditorPage)
        return;
    const QString selected = m_editor->textCursor().selectedText().replace(QChar(0x2029), QLatin1Char('\n'));
    copySelectionToCollector(selected, tr("Selection"));
    m_editor->copy();
}

void IniEditorPage::cutCurrentSelection()
{
    if (m_editorStack->currentWidget() == m_emptyEditorPage)
        return;
    const QString selected = m_editor->textCursor().selectedText().replace(QChar(0x2029), QLatin1Char('\n'));
    copySelectionToCollector(selected, tr("Cut"));
    m_editor->cut();
}

void IniEditorPage::copyCurrentSection()
{
    copySelectionToCollector(currentSectionText(), tr("Section"));
}

void IniEditorPage::captureSnapshot(FileSession &session, const QString &label)
{
    const QString currentText = session.document->toPlainText();
    if (!session.snapshots.isEmpty() && session.snapshots.last().text == currentText)
        return;

    Snapshot snapshot;
    snapshot.id = session.nextSnapshotId++;
    snapshot.label = label;
    snapshot.createdAt = QDateTime::currentDateTime();
    snapshot.text = currentText;
    session.snapshots.append(snapshot);
    while (session.snapshots.size() > 40)
        session.snapshots.removeFirst();

    if (&session == currentSession()) {
        refreshSnapshotList();
        updateStatusSummary();
    }
}

void IniEditorPage::refreshSnapshotList()
{
    m_historyList->clear();
    const auto *session = currentSession();
    if (!session)
        return;

    for (int index = session->snapshots.size() - 1; index >= 0; --index) {
        const auto &snapshot = session->snapshots.at(index);
        auto *item = new QListWidgetItem(
            QStringLiteral("%1 | %2").arg(snapshot.createdAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")), snapshot.label));
        item->setData(SnapshotIdRole, snapshot.id);
        m_historyList->addItem(item);
    }
    if (m_historyList->count() > 0)
        m_historyList->setCurrentRow(0);
    else
        m_historyDiffView->clear();
}

void IniEditorPage::updateSnapshotDiff()
{
    const auto *session = currentSession();
    auto *item = m_historyList->currentItem();
    if (!session || !item) {
        m_historyDiffView->clear();
        return;
    }

    const int snapshotId = item->data(SnapshotIdRole).toInt();
    for (const auto &snapshot : session->snapshots) {
        if (snapshot.id == snapshotId) {
            m_historyDiffView->setHtml(buildDiffHtml(snapshot.text, session->document->toPlainText()));
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

void IniEditorPage::writeRecoverySnapshot(FileSession &session)
{
    const QString recoveryPath = recoveryFilePathFor(session.filePath);
    if (recoveryPath.isEmpty())
        return;

    QFile file(recoveryPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return;
    file.write(session.document->toPlainText().toUtf8());
    file.close();
    session.pendingRecoveryText = session.document->toPlainText();
    if (&session == currentSession())
        updateRecoveryUi();
}

void IniEditorPage::clearRecoverySnapshot(FileSession &session)
{
    QFile::remove(recoveryFilePathFor(session.filePath));
    session.pendingRecoveryText.clear();
}

void IniEditorPage::updateRecoveryUi()
{
    const auto *session = currentSession();
    if (!session) {
        m_recoveryLabel->setText(tr("Recovery data is shown per open file."));
        m_restoreRecoveryButton->setEnabled(false);
        m_discardRecoveryButton->setEnabled(false);
        return;
    }

    const bool hasRecovery = !session->pendingRecoveryText.isEmpty() || QFileInfo(recoveryFilePathFor(session->filePath)).exists();
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
    auto *session = currentSession();
    if (!session)
        return;

    QTextBlock block = m_editor->document()->begin();
    while (block.isValid()) {
        block.setVisible(true);
        block.setLineCount(1);
        block = block.next();
    }

    for (const auto &section : session->analysis.sections) {
        if (!session->collapsedSectionLines.contains(section.startLine))
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
    auto *session = currentSession();
    if (!session)
        return;

    const int lineNumber = m_editor->textCursor().blockNumber() + 1;
    const int sectionIndex = session->analysis.sectionIndexForLine(lineNumber);
    if (sectionIndex < 0 || sectionIndex >= session->analysis.sections.size())
        return;

    const int startLine = session->analysis.sections.at(sectionIndex).startLine;
    if (session->collapsedSectionLines.contains(startLine))
        session->collapsedSectionLines.remove(startLine);
    else
        session->collapsedSectionLines.insert(startLine);
    applyFolding();
}

void IniEditorPage::expandAllSections()
{
    auto *session = currentSession();
    if (!session)
        return;
    session->collapsedSectionLines.clear();
    applyFolding();
}

} // namespace flatlas::editors