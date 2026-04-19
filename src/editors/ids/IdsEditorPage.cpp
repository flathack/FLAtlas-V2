// editors/ids/IdsEditorPage.cpp – IDS-String-Editor (Phase 12)

#include "IdsEditorPage.h"
#include "infrastructure/freelancer/IdsStringTable.h"

#include <QVBoxLayout>
#include <QToolBar>
#include <QLineEdit>
#include <QLabel>
#include <QTableView>
#include <QHeaderView>
#include <QStandardItemModel>
#include <QFileDialog>
#include <QElapsedTimer>

using namespace flatlas::infrastructure;

namespace flatlas::editors {

IdsEditorPage::IdsEditorPage(QWidget *parent)
    : QWidget(parent)
    , m_table(new IdsStringTable)
{
    setupUi();
}

void IdsEditorPage::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setupToolBar();
    layout->addWidget(m_toolBar);

    // Model + proxy for filtering
    m_model = new QStandardItemModel(this);
    m_model->setColumnCount(2);
    m_model->setHorizontalHeaderLabels({tr("IDS"), tr("String")});

    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1); // search all columns

    m_tableView = new QTableView(this);
    m_tableView->setModel(m_proxy);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->setSortingEnabled(true);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(m_tableView);

    m_statusLabel = new QLabel(tr("No DLL loaded"), this);
    layout->addWidget(m_statusLabel);
}

void IdsEditorPage::setupToolBar()
{
    m_toolBar = new QToolBar(this);
    m_toolBar->setMovable(false);

    m_toolBar->addAction(tr("Load DLL..."), this, &IdsEditorPage::onLoadDllClicked);
    m_toolBar->addAction(tr("Import CSV..."), this, &IdsEditorPage::onImportCsvClicked);
    m_toolBar->addAction(tr("Export CSV..."), this, &IdsEditorPage::onExportCsvClicked);
    m_toolBar->addSeparator();

    m_toolBar->addWidget(new QLabel(tr(" Filter: "), m_toolBar));
    m_filterEdit = new QLineEdit(m_toolBar);
    m_filterEdit->setPlaceholderText(tr("Search by ID or text..."));
    m_filterEdit->setClearButtonEnabled(true);
    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &IdsEditorPage::onFilterChanged);
    m_toolBar->addWidget(m_filterEdit);
}

void IdsEditorPage::loadDll(const QString &dllPath)
{
    QElapsedTimer timer;
    timer.start();

    m_table->loadFromDll(dllPath);
    populateModel();

    m_statusLabel->setText(tr("Loaded %1 strings in %2 ms")
        .arg(m_table->count()).arg(timer.elapsed()));
    emit titleChanged(QStringLiteral("IDS Editor (%1)").arg(m_table->count()));
}

void IdsEditorPage::loadFreelancerDir(const QString &exeDir)
{
    QElapsedTimer timer;
    timer.start();

    m_table->loadFromFreelancerDir(exeDir);
    populateModel();

    m_statusLabel->setText(tr("Loaded %1 strings from Freelancer dir in %2 ms")
        .arg(m_table->count()).arg(timer.elapsed()));
    emit titleChanged(QStringLiteral("IDS Editor (%1)").arg(m_table->count()));
}

void IdsEditorPage::importCsv(const QString &csvPath)
{
    int count = m_table->importCsv(csvPath);
    populateModel();
    m_statusLabel->setText(tr("Imported %1 strings from CSV").arg(count));
    emit titleChanged(QStringLiteral("IDS Editor (%1)").arg(m_table->count()));
}

void IdsEditorPage::exportCsv(const QString &csvPath)
{
    if (m_table->exportCsv(csvPath))
        m_statusLabel->setText(tr("Exported %1 strings to CSV").arg(m_table->count()));
    else
        m_statusLabel->setText(tr("Export failed"));
}

int IdsEditorPage::stringCount() const
{
    return m_table->count();
}

void IdsEditorPage::populateModel()
{
    m_model->removeRows(0, m_model->rowCount());

    const auto &strings = m_table->strings();
    m_model->setRowCount(strings.size());

    int row = 0;
    for (auto it = strings.constBegin(); it != strings.constEnd(); ++it, ++row) {
        auto *idItem = new QStandardItem();
        idItem->setData(it.key(), Qt::DisplayRole);
        m_model->setItem(row, 0, idItem);
        m_model->setItem(row, 1, new QStandardItem(it.value()));
    }

    m_tableView->resizeColumnToContents(0);
}

void IdsEditorPage::onFilterChanged(const QString &text)
{
    m_proxy->setFilterFixedString(text);
    m_statusLabel->setText(tr("Showing %1 of %2 strings")
        .arg(m_proxy->rowCount()).arg(m_model->rowCount()));
}

void IdsEditorPage::onLoadDllClicked()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("Load DLL"), QString(),
        tr("DLL files (*.dll);;All files (*.*)"));
    if (!path.isEmpty())
        loadDll(path);
}

void IdsEditorPage::onExportCsvClicked()
{
    QString path = QFileDialog::getSaveFileName(
        this, tr("Export CSV"), QStringLiteral("ids_strings.csv"),
        tr("CSV files (*.csv);;All files (*.*)"));
    if (!path.isEmpty())
        exportCsv(path);
}

void IdsEditorPage::onImportCsvClicked()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("Import CSV"), QString(),
        tr("CSV files (*.csv);;All files (*.*)"));
    if (!path.isEmpty())
        importCsv(path);
}

} // namespace flatlas::editors
