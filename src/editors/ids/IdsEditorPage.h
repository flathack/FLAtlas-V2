#pragma once
// editors/ids/IdsEditorPage.h – IDS Editor

#include "infrastructure/freelancer/IdsDataService.h"

#include <QFutureWatcher>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSortFilterProxyModel;
class QStackedWidget;
class QTableView;
class QTextBrowser;
class QTabWidget;
class QToolBar;
class QStandardItemModel;

namespace flatlas::editors {

class IdsEditorPage : public QWidget {
    Q_OBJECT
public:
    explicit IdsEditorPage(QWidget *parent = nullptr);

    /// Load strings from a DLL file.
    void loadDll(const QString &dllPath);

    /// Load from Freelancer EXE directory (all standard DLLs).
    void loadFreelancerDir(const QString &exeDir);

    /// Import from CSV.
    void importCsv(const QString &csvPath);

    /// Export to CSV.
    void exportCsv(const QString &csvPath);

    int stringCount() const;
    QString currentGameRoot() const;

signals:
    void titleChanged(const QString &title);
    void openIniRequested(const QString &filePath, const QString &searchText);

private:
    void setupUi();
    void setupToolBar();
    void applyDataset(flatlas::infrastructure::IdsDataset dataset);
    void populateAuxiliaryModels();
    void refreshEntrySelection();
    void refreshUsageView();
    void refreshIssueSummary();
    void refreshStatusLabel(const QString &message = QString());
    void resetEditorState();
    void populateEditorFromEntry(const flatlas::infrastructure::IdsEntryRecord &entry);
    void beginCreateMode(flatlas::infrastructure::IdsUsageType usageType);
    void reloadCurrentContext();
    void loadGameRootAsync(const QString &gameRoot);
    void handleLoadFinished();
    void saveCurrentEntry(bool createNew);
    void assignSelectedEntryToMissing();
    void createAndAssignToMissing();
    void openSelectedReference();
    void openSelectedMissingLocation();
    void validateInfocardEditor();
    void prettyPrintInfocardEditor();
    QString currentEditorTargetDll() const;
    int currentEditorTargetId() const;
    void refreshTargetDllSelection();
    void refreshCreatePreviewId();
    void selectEntryByGlobalId(int globalId);
    const flatlas::infrastructure::IdsEntryRecord *selectedEntry() const;
    flatlas::infrastructure::IdsUsageType selectedMissingUsageType() const;
    int selectedMissingRow() const;
    QString selectedReferenceSearchText() const;
    void onFilterChanged(const QString &text);
    void onLoadDllClicked();
    void onExportCsvClicked();
    void onImportCsvClicked();

    flatlas::infrastructure::IdsDataset m_dataset;
    bool m_createMode = false;
    flatlas::infrastructure::IdsUsageType m_createUsageType = flatlas::infrastructure::IdsUsageType::IdsName;
    int m_selectedEntryId = 0;
    int m_pendingSelectionId = 0;
    bool m_ignoreSelectionRefresh = false;

    QFutureWatcher<flatlas::infrastructure::IdsDataset> *m_loadWatcher = nullptr;
    QToolBar *m_toolBar = nullptr;
    QLineEdit *m_filterEdit = nullptr;
    QComboBox *m_typeFilterCombo = nullptr;
    QComboBox *m_dllFilterCombo = nullptr;
    QPushButton *m_reloadButton = nullptr;
    QTableView *m_tableView = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QLabel *m_editorModeLabel = nullptr;
    QLabel *m_entryMetaLabel = nullptr;
    QLineEdit *m_idEdit = nullptr;
    QLineEdit *m_dllEdit = nullptr;
    QComboBox *m_targetDllCombo = nullptr;
    QStackedWidget *m_editorStack = nullptr;
    QPlainTextEdit *m_stringEdit = nullptr;
    QTabWidget *m_infocardTabs = nullptr;
    QPlainTextEdit *m_infocardEdit = nullptr;
    QTextBrowser *m_infocardPreview = nullptr;
    QLabel *m_validationLabel = nullptr;
    QPushButton *m_validateButton = nullptr;
    QPushButton *m_prettyPrintButton = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_createStringButton = nullptr;
    QPushButton *m_createInfocardButton = nullptr;
    QTableView *m_usageView = nullptr;
    QTableView *m_missingView = nullptr;
    QTableView *m_issuesView = nullptr;
    QStandardItemModel *m_usageModel = nullptr;
    QStandardItemModel *m_missingModel = nullptr;
    QStandardItemModel *m_issueModel = nullptr;
    QSortFilterProxyModel *m_proxy = nullptr;
};

} // namespace flatlas::editors
