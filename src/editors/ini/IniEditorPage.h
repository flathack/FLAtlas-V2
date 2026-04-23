#pragma once

#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QVector>
#include <QWidget>

#include "infrastructure/parser/IniAnalysisService.h"

class QAction;
class QCheckBox;
class QFileSystemModel;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QPushButton;
class QScrollBar;
class QStackedWidget;
class QTabBar;
class QTabWidget;
class QTextBrowser;
class QTextDocument;
class QTimer;
class QToolBar;
class QTreeView;
class QStandardItemModel;
class QModelIndex;
template <typename T>
class QFutureWatcher;

namespace flatlas::editors {

class IniCodeEditor;
class IniFindReplaceDialog;
class IniMiniMap;
class IniSyntaxHighlighter;

class IniEditorPage : public QWidget {
    Q_OBJECT
public:
    explicit IniEditorPage(QWidget *parent = nullptr);

    void openWorkspace(const QString &rootPath = QString());
    bool openFile(const QString &filePath);
    bool save();
    bool saveAs(const QString &filePath);
    bool saveAllDirtyTabs(QWidget *dialogParent = nullptr);

    QString filePath() const;
    QString fileName() const;
    bool isDirty() const;
    void focusSearch(const QString &text);
    void goToLine(int lineNumber);

signals:
    void titleChanged(const QString &title);
    void openFileRequested(const QString &filePath, const QString &searchText, int lineNumber);

private slots:
    void onTextChanged();
    void runAnalysis();
    void refreshCurrentSectionFromCursor();
    void startGlobalSearch();
    void handleGlobalSearchFinished();
    void onFileTabChanged(int index);
    void openFromFileTree(const QModelIndex &index);
    void openFromSearchResult(const QModelIndex &index);
    void activateSelectedSnapshot();
    void activateClipboardItem(QListWidgetItem *item);
    void activateRecentFile(QListWidgetItem *item);
    void restoreRecoverySnapshot();
    void discardRecoverySnapshot();

private:
    struct Snapshot {
        int id = 0;
        QString label;
        QDateTime createdAt;
        QString text;
    };

    struct FileSession {
        QString filePath;
        QString savedText;
        QString pendingRecoveryText;
        flatlas::infrastructure::IniAnalysisResult analysis;
        QVector<Snapshot> snapshots;
        QSet<int> collapsedSectionLines;
        QTextDocument *document = nullptr;
        IniSyntaxHighlighter *highlighter = nullptr;
        bool dirty = false;
        bool wasBini = false;
        int nextSnapshotId = 1;
        int cursorPosition = 0;
        int verticalScrollValue = 0;
        int horizontalScrollValue = 0;
        int selectedSectionStartLine = -1;
    };

    void setupUi();
    void setupToolbar();
    void setupWorkspace();
    void setupBottomTabs();
    void setupShortcuts();

    void updateTitle();
    void updateActiveTabText();
    void updateDirtyState();
    void updateWorkspaceContext();
    void updateBreadcrumbs();
    void setTreeRootPath(const QString &rootPath);
    void restoreTreeState();
    void saveTreeState() const;
    void revealPathInTree(const QString &path);
    QString preferredProjectRoot() const;
    bool isIniLikePath(const QString &path) const;
    FileSession *currentSession();
    const FileSession *currentSession() const;
    int findSessionIndex(const QString &filePath) const;
    void activateSession(int index);
    void storeCurrentSessionState();
    void bindActiveSession();
    void clearActiveSession();
    void setCurrentSessionIndex(int index);
    bool ensureWorkspaceRootForFile(const QString &filePath);
    bool openFileInSession(const QString &filePath, QString *errorMessage = nullptr);
    bool saveSession(FileSession &session, const QString &targetPath);
    bool confirmCloseSession(int index);
    void closeSession(int index, bool force = false);
    void updateTabBarVisibility();
    QString sessionDisplayName(const FileSession &session) const;
    QString workspaceSettingsKey() const;

    void populateSectionList();
    void populateKeyListForSectionStartLine(int startLine);
    void updateDiagnosticsView();
    void updateSearchResultsView(const QVector<flatlas::infrastructure::IniSearchMatch> &matches);
    void updateStatusSummary();

    bool findWithCurrentOptions(bool forward);
    void findNext();
    void findPrev();
    void replaceAll();
    void showFindReplaceDialog();
    QString currentSectionText() const;
    void copySelectionToCollector(const QString &text, const QString &label);
    void copyCurrentSelection();
    void cutCurrentSelection();
    void copyCurrentSection();

    void captureSnapshot(FileSession &session, const QString &label);
    void refreshSnapshotList();
    void updateSnapshotDiff();
    QString buildDiffHtml(const QString &fromText, const QString &toText) const;

    QString recoveryFilePathFor(const QString &filePath) const;
    void writeRecoverySnapshot(FileSession &session);
    void clearRecoverySnapshot(FileSession &session);
    void updateRecoveryUi();

    QStringList loadRecentFiles() const;
    void rememberRecentFile(const QString &filePath);
    void refreshRecentFiles();

    void applyFolding();
    void collapseCurrentSection();
    void expandAllSections();

    IniCodeEditor *m_editor = nullptr;
    IniMiniMap *m_minimap = nullptr;
    QToolBar *m_toolbar = nullptr;
    QWidget *m_breadcrumbBar = nullptr;
    IniFindReplaceDialog *m_findReplaceDialog = nullptr;
    QStackedWidget *m_editorStack = nullptr;
    QWidget *m_emptyEditorPage = nullptr;
    QTabBar *m_fileTabs = nullptr;
    QLineEdit *m_sectionFilterEdit = nullptr;
    QLineEdit *m_keyFilterEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QFileSystemModel *m_fileModel = nullptr;
    QTreeView *m_fileTree = nullptr;
    QListWidget *m_sectionList = nullptr;
    QListWidget *m_keyList = nullptr;
    QTabWidget *m_bottomTabs = nullptr;
    QTreeView *m_diagnosticsView = nullptr;
    QTreeView *m_searchResultsView = nullptr;
    QListWidget *m_clipboardList = nullptr;
    QListWidget *m_historyList = nullptr;
    QTextBrowser *m_historyDiffView = nullptr;
    QListWidget *m_recentFilesList = nullptr;
    QLabel *m_recoveryLabel = nullptr;
    QPushButton *m_restoreRecoveryButton = nullptr;
    QPushButton *m_discardRecoveryButton = nullptr;
    QStandardItemModel *m_diagnosticsModel = nullptr;
    QStandardItemModel *m_searchResultsModel = nullptr;
    QTimer *m_analysisTimer = nullptr;
    QTimer *m_autosaveTimer = nullptr;
    QFutureWatcher<QVector<flatlas::infrastructure::IniSearchMatch>> *m_searchWatcher = nullptr;

    QString m_treeRootPath;
    QVector<FileSession> m_sessions;
    bool m_loading = false;
    int m_currentSessionIndex = -1;
};

} // namespace flatlas::editors
