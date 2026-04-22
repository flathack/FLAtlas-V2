#pragma once

#include <QDateTime>
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
class QTabWidget;
class QTextBrowser;
class QTimer;
class QToolBar;
class QTreeView;
class QStandardItemModel;
class QModelIndex;
template <typename T>
class QFutureWatcher;

namespace flatlas::editors {

class IniCodeEditor;
class IniSyntaxHighlighter;

class IniEditorPage : public QWidget {
    Q_OBJECT
public:
    explicit IniEditorPage(QWidget *parent = nullptr);

    void openWorkspace(const QString &rootPath = QString());
    bool openFile(const QString &filePath);
    bool save();
    bool saveAs(const QString &filePath);

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

    void setupUi();
    void setupToolbar();
    void setupSearchStrip();
    void setupWorkspace();
    void setupBottomTabs();
    void setupShortcuts();

    void updateTitle();
    void updateDirtyState();
    void updateWorkspaceContext();
    void updateBreadcrumbs();
    void setTreeRootPath(const QString &rootPath);
    QString preferredProjectRoot() const;
    bool isIniLikePath(const QString &path) const;

    void populateSectionList();
    void populateKeyListForSectionStartLine(int startLine);
    void updateDiagnosticsView();
    void updateSearchResultsView(const QVector<flatlas::infrastructure::IniSearchMatch> &matches);
    void syncMinimapText();
    void syncMinimapScroll();
    void updateStatusSummary();

    bool findWithCurrentOptions(bool forward);
    void findNext();
    void findPrev();
    void replaceAll();
    QString currentSectionText() const;
    void copySelectionToCollector(const QString &text, const QString &label);
    void copyCurrentSelection();
    void cutCurrentSelection();
    void copyCurrentSection();

    void captureSnapshot(const QString &label);
    void refreshSnapshotList();
    void updateSnapshotDiff();
    QString buildDiffHtml(const QString &fromText, const QString &toText) const;

    QString recoveryFilePathFor(const QString &filePath) const;
    void writeRecoverySnapshot();
    void clearRecoverySnapshot();
    void updateRecoveryUi();

    QStringList loadRecentFiles() const;
    void rememberRecentFile(const QString &filePath);
    void refreshRecentFiles();

    void applyFolding();
    void collapseCurrentSection();
    void expandAllSections();

    IniCodeEditor *m_editor = nullptr;
    IniSyntaxHighlighter *m_highlighter = nullptr;
    QPlainTextEdit *m_minimap = nullptr;
    QToolBar *m_toolbar = nullptr;
    QWidget *m_breadcrumbBar = nullptr;
    QWidget *m_searchStrip = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLineEdit *m_replaceEdit = nullptr;
    QLineEdit *m_globalSearchEdit = nullptr;
    QLineEdit *m_gotoLineEdit = nullptr;
    QLineEdit *m_sectionFilterEdit = nullptr;
    QLineEdit *m_keyFilterEdit = nullptr;
    QCheckBox *m_caseSensitiveCheck = nullptr;
    QCheckBox *m_regexCheck = nullptr;
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
    QTimer *m_snapshotTimer = nullptr;
    QTimer *m_autosaveTimer = nullptr;
    QFutureWatcher<QVector<flatlas::infrastructure::IniSearchMatch>> *m_searchWatcher = nullptr;

    QString m_filePath;
    QString m_savedText;
    QString m_pendingRecoveryText;
    QString m_treeRootPath;
    flatlas::infrastructure::IniAnalysisResult m_analysis;
    QVector<Snapshot> m_snapshots;
    QSet<int> m_collapsedSectionLines;
    bool m_dirty = false;
    bool m_wasBini = false;
    bool m_loading = false;
    int m_nextSnapshotId = 1;
};

} // namespace flatlas::editors
