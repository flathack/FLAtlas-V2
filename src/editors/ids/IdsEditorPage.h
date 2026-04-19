#pragma once
// editors/ids/IdsEditorPage.h – IDS-String-Editor (Phase 12)

#include <QWidget>
#include <QSortFilterProxyModel>

class QTableView;
class QToolBar;
class QLineEdit;
class QLabel;
class QStandardItemModel;

namespace flatlas::infrastructure { class IdsStringTable; }

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

signals:
    void titleChanged(const QString &title);

private:
    void setupUi();
    void setupToolBar();
    void populateModel();
    void onFilterChanged(const QString &text);
    void onLoadDllClicked();
    void onExportCsvClicked();
    void onImportCsvClicked();

    flatlas::infrastructure::IdsStringTable *m_table = nullptr;
    QToolBar *m_toolBar = nullptr;
    QLineEdit *m_filterEdit = nullptr;
    QTableView *m_tableView = nullptr;
    QLabel *m_statusLabel = nullptr;
    QStandardItemModel *m_model = nullptr;
    QSortFilterProxyModel *m_proxy = nullptr;
};

} // namespace flatlas::editors
