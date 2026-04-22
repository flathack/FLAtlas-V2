#include "IdsEditorPage.h"

#include "core/EditingContext.h"
#include "infrastructure/parser/XmlInfocard.h"

#include <QAbstractTableModel>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStackedWidget>
#include <QTableView>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTextStream>
#include <QToolBar>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace flatlas::editors {

using namespace flatlas::infrastructure;

namespace {

enum EntryRoles {
    EntryIdRole = Qt::UserRole + 1,
    EntrySearchRole,
    EntryTypeRole,
    EntryDllRole,
};

QString previewText(QString value)
{
    value.replace(QLatin1Char('\n'), QLatin1Char(' '));
    value = value.simplified();
    if (value.size() > 120)
        value = value.left(117) + QStringLiteral("...");
    return value;
}

QString issueSeverityLabel(IdsIssueSeverity severity)
{
    switch (severity) {
    case IdsIssueSeverity::Info:
        return QStringLiteral("Info");
    case IdsIssueSeverity::Warning:
        return QStringLiteral("Warning");
    case IdsIssueSeverity::Error:
        return QStringLiteral("Error");
    }
    return QStringLiteral("Info");
}

QString entryUsageLabel(const IdsEntryRecord &entry)
{
    if (!entry.usageTypes.isEmpty())
        return entry.usageTypes.join(QStringLiteral(", "));
    return entry.hasHtmlValue ? QStringLiteral("ids_info") : QStringLiteral("string");
}

QString entryValueForDisplay(const IdsEntryRecord &entry)
{
    if (entry.hasHtmlValue)
        return previewText(entry.plainText);
    return previewText(entry.stringValue);
}

class IdsEntryTableModel : public QAbstractTableModel {
public:
    explicit IdsEntryTableModel(QObject *parent = nullptr)
        : QAbstractTableModel(parent)
    {
    }

    void setEntries(QVector<IdsEntryRecord> entries)
    {
        beginResetModel();
        m_entries = std::move(entries);
        endResetModel();
    }

    const IdsEntryRecord *entryAt(int row) const
    {
        if (row < 0 || row >= m_entries.size())
            return nullptr;
        return &m_entries.at(row);
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : m_entries.size();
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : 6;
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
            return {};

        switch (section) {
        case 0:
            return QStringLiteral("ID");
        case 1:
            return QStringLiteral("Type");
        case 2:
            return QStringLiteral("Text");
        case 3:
            return QStringLiteral("DLL");
        case 4:
            return QStringLiteral("Usage");
        case 5:
            return QStringLiteral("Edit");
        default:
            return {};
        }
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        const IdsEntryRecord *entry = entryAt(index.row());
        if (!entry)
            return {};

        if (role == EntryIdRole)
            return entry->globalId;
        if (role == EntrySearchRole)
            return entry->searchBlob;
        if (role == EntryTypeRole)
            return IdsDataService::normalizedEntryTypeKey(*entry);
        if (role == EntryDllRole)
            return entry->dllName.toLower();
        if (role != Qt::DisplayRole)
            return {};

        switch (index.column()) {
        case 0:
            return entry->globalId;
        case 1:
            return IdsDataService::normalizedEntryTypeKey(*entry);
        case 2:
            return entryValueForDisplay(*entry);
        case 3:
            return entry->dllName;
        case 4:
            return entryUsageLabel(*entry);
        case 5:
            return entry->editable ? QStringLiteral("editable") : QStringLiteral("read-only");
        default:
            return {};
        }
    }

private:
    QVector<IdsEntryRecord> m_entries;
};

class IdsEntryFilterModel : public QSortFilterProxyModel {
public:
    explicit IdsEntryFilterModel(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
    }

    void setSearchText(QString value)
    {
        value = value.trimmed().toLower();
        if (m_searchText == value)
            return;
        m_searchText = std::move(value);
        invalidateFilter();
    }

    void setTypeFilter(QString value)
    {
        value = value.trimmed().toLower();
        if (m_typeFilter == value)
            return;
        m_typeFilter = std::move(value);
        invalidateFilter();
    }

    void setDllFilter(QString value)
    {
        value = value.trimmed().toLower();
        if (m_dllFilter == value)
            return;
        m_dllFilter = std::move(value);
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        const QModelIndex first = sourceModel()->index(sourceRow, 0, sourceParent);
        if (!first.isValid())
            return false;

        const QString type = sourceModel()->data(first, EntryTypeRole).toString();
        if (!m_typeFilter.isEmpty() && m_typeFilter != QStringLiteral("all") && type != m_typeFilter)
            return false;

        const QString dll = sourceModel()->data(first, EntryDllRole).toString();
        if (!m_dllFilter.isEmpty() && m_dllFilter != QStringLiteral("all") && dll != m_dllFilter)
            return false;

        if (m_searchText.isEmpty())
            return true;

        const QString blob = sourceModel()->data(first, EntrySearchRole).toString();
        return blob.contains(m_searchText);
    }

private:
    QString m_searchText;
    QString m_typeFilter = QStringLiteral("all");
    QString m_dllFilter = QStringLiteral("all");
};

} // namespace

IdsEditorPage::IdsEditorPage(QWidget *parent)
    : QWidget(parent)
    , m_loadWatcher(new QFutureWatcher<IdsDataset>(this))
{
    setupUi();
    connect(m_loadWatcher, &QFutureWatcher<IdsDataset>::finished,
            this, &IdsEditorPage::handleLoadFinished);
}

void IdsEditorPage::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setupToolBar();
    layout->addWidget(m_toolBar);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    layout->addWidget(splitter, 1);

    auto *leftPane = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(8, 8, 8, 8);
    leftLayout->setSpacing(6);

    m_summaryLabel = new QLabel(tr("No IDS data loaded"), leftPane);
    leftLayout->addWidget(m_summaryLabel);

    auto *entryModel = new IdsEntryTableModel(this);
    m_proxy = new IdsEntryFilterModel(this);
    m_proxy->setSourceModel(entryModel);

    m_tableView = new QTableView(leftPane);
    m_tableView->setModel(m_proxy);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setSortingEnabled(true);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    leftLayout->addWidget(m_tableView, 1);
    splitter->addWidget(leftPane);

    auto *rightPane = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(8, 8, 8, 8);
    rightLayout->setSpacing(8);

    m_editorModeLabel = new QLabel(tr("Select an IDS entry or create a new one."), rightPane);
    rightLayout->addWidget(m_editorModeLabel);

    m_entryMetaLabel = new QLabel(rightPane);
    m_entryMetaLabel->setWordWrap(true);
    rightLayout->addWidget(m_entryMetaLabel);

    auto *form = new QFormLayout;
    m_idEdit = new QLineEdit(rightPane);
    m_idEdit->setReadOnly(true);
    form->addRow(tr("ID"), m_idEdit);

    m_dllEdit = new QLineEdit(rightPane);
    m_dllEdit->setReadOnly(true);
    form->addRow(tr("Current DLL"), m_dllEdit);

    m_targetDllCombo = new QComboBox(rightPane);
    form->addRow(tr("Save To DLL"), m_targetDllCombo);
    rightLayout->addLayout(form);

    auto *actions = new QHBoxLayout;
    m_createStringButton = new QPushButton(tr("New String"), rightPane);
    m_createInfocardButton = new QPushButton(tr("New ids_info"), rightPane);
    m_saveButton = new QPushButton(tr("Save"), rightPane);
    actions->addWidget(m_createStringButton);
    actions->addWidget(m_createInfocardButton);
    actions->addStretch(1);
    actions->addWidget(m_saveButton);
    rightLayout->addLayout(actions);

    m_editorStack = new QStackedWidget(rightPane);
    m_stringEdit = new QPlainTextEdit(rightPane);
    m_stringEdit->setPlaceholderText(tr("String text"));
    m_editorStack->addWidget(m_stringEdit);

    auto *infocardContainer = new QWidget(rightPane);
    auto *infocardLayout = new QVBoxLayout(infocardContainer);
    infocardLayout->setContentsMargins(0, 0, 0, 0);
    infocardLayout->setSpacing(6);

    auto *infocardButtons = new QHBoxLayout;
    m_validateButton = new QPushButton(tr("Validate"), infocardContainer);
    m_prettyPrintButton = new QPushButton(tr("Pretty Print"), infocardContainer);
    infocardButtons->addWidget(m_validateButton);
    infocardButtons->addWidget(m_prettyPrintButton);
    infocardButtons->addStretch(1);
    infocardLayout->addLayout(infocardButtons);

    m_infocardTabs = new QTabWidget(infocardContainer);
    m_infocardEdit = new QPlainTextEdit(infocardContainer);
    m_infocardEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_infocardTabs->addTab(m_infocardEdit, tr("Source"));
    m_infocardPreview = new QTextBrowser(infocardContainer);
    m_infocardTabs->addTab(m_infocardPreview, tr("Preview"));
    infocardLayout->addWidget(m_infocardTabs, 1);

    m_validationLabel = new QLabel(infocardContainer);
    m_validationLabel->setWordWrap(true);
    infocardLayout->addWidget(m_validationLabel);
    m_editorStack->addWidget(infocardContainer);
    rightLayout->addWidget(m_editorStack, 1);

    auto *tabs = new QTabWidget(rightPane);

    auto *usageTab = new QWidget(rightPane);
    auto *usageLayout = new QVBoxLayout(usageTab);
    usageLayout->setContentsMargins(0, 0, 0, 0);
    m_usageView = new QTableView(usageTab);
    m_usageModel = new QStandardItemModel(this);
    m_usageView->setModel(m_usageModel);
    m_usageView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_usageView->setAlternatingRowColors(true);
    m_usageView->verticalHeader()->setVisible(false);
    m_usageView->horizontalHeader()->setStretchLastSection(true);
    usageLayout->addWidget(m_usageView);
    auto *openUsageButton = new QPushButton(tr("Open Referenced INI"), usageTab);
    usageLayout->addWidget(openUsageButton);
    tabs->addTab(usageTab, tr("Usage"));

    auto *missingTab = new QWidget(rightPane);
    auto *missingLayout = new QVBoxLayout(missingTab);
    missingLayout->setContentsMargins(0, 0, 0, 0);
    m_missingView = new QTableView(missingTab);
    m_missingModel = new QStandardItemModel(this);
    m_missingView->setModel(m_missingModel);
    m_missingView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_missingView->setAlternatingRowColors(true);
    m_missingView->verticalHeader()->setVisible(false);
    m_missingView->horizontalHeader()->setStretchLastSection(true);
    missingLayout->addWidget(m_missingView);
    auto *missingButtons = new QHBoxLayout;
    auto *assignButton = new QPushButton(tr("Assign Selected Entry"), missingTab);
    auto *createAssignButton = new QPushButton(tr("Create + Assign"), missingTab);
    auto *openMissingButton = new QPushButton(tr("Open INI"), missingTab);
    missingButtons->addWidget(assignButton);
    missingButtons->addWidget(createAssignButton);
    missingButtons->addStretch(1);
    missingButtons->addWidget(openMissingButton);
    missingLayout->addLayout(missingButtons);
    tabs->addTab(missingTab, tr("Missing Assignments"));

    auto *issueTab = new QWidget(rightPane);
    auto *issueLayout = new QVBoxLayout(issueTab);
    issueLayout->setContentsMargins(0, 0, 0, 0);
    m_issuesView = new QTableView(issueTab);
    m_issueModel = new QStandardItemModel(this);
    m_issuesView->setModel(m_issueModel);
    m_issuesView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_issuesView->setAlternatingRowColors(true);
    m_issuesView->verticalHeader()->setVisible(false);
    m_issuesView->horizontalHeader()->setStretchLastSection(true);
    issueLayout->addWidget(m_issuesView);
    tabs->addTab(issueTab, tr("Checks"));

    rightLayout->addWidget(tabs, 1);
    splitter->addWidget(rightPane);
    splitter->setStretchFactor(0, 5);
    splitter->setStretchFactor(1, 4);

    m_statusLabel = new QLabel(tr("Ready"), this);
    layout->addWidget(m_statusLabel);

    connect(m_filterEdit, &QLineEdit::textChanged, this, &IdsEditorPage::onFilterChanged);
    connect(m_typeFilterCombo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        static_cast<IdsEntryFilterModel *>(m_proxy)->setTypeFilter(text.toLower());
        refreshStatusLabel();
    });
    connect(m_dllFilterCombo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        static_cast<IdsEntryFilterModel *>(m_proxy)->setDllFilter(text.toLower());
        refreshStatusLabel();
    });
    connect(m_createStringButton, &QPushButton::clicked, this, [this]() { beginCreateMode(IdsUsageType::IdsName); });
    connect(m_createInfocardButton, &QPushButton::clicked, this, [this]() { beginCreateMode(IdsUsageType::IdsInfo); });
    connect(m_saveButton, &QPushButton::clicked, this, [this]() { saveCurrentEntry(m_createMode); });
    connect(assignButton, &QPushButton::clicked, this, &IdsEditorPage::assignSelectedEntryToMissing);
    connect(createAssignButton, &QPushButton::clicked, this, &IdsEditorPage::createAndAssignToMissing);
    connect(openUsageButton, &QPushButton::clicked, this, &IdsEditorPage::openSelectedReference);
    connect(openMissingButton, &QPushButton::clicked, this, &IdsEditorPage::openSelectedMissingLocation);
    connect(m_validateButton, &QPushButton::clicked, this, &IdsEditorPage::validateInfocardEditor);
    connect(m_prettyPrintButton, &QPushButton::clicked, this, &IdsEditorPage::prettyPrintInfocardEditor);
    connect(m_infocardTabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == 1) {
            QString validationError;
            if (XmlInfocard::validate(m_infocardEdit->toPlainText(), validationError))
                m_infocardPreview->setHtml(XmlInfocard::toHtml(XmlInfocard::parse(m_infocardEdit->toPlainText())));
            else
                m_infocardPreview->setHtml(QStringLiteral("<html><body><pre>%1</pre></body></html>").arg(validationError.toHtmlEscaped()));
        }
    });

    beginCreateMode(IdsUsageType::IdsName);
}

void IdsEditorPage::setupToolBar()
{
    m_toolBar = new QToolBar(this);
    m_toolBar->setMovable(false);
    m_toolBar->addAction(tr("Load DLL..."), this, &IdsEditorPage::onLoadDllClicked);
    m_toolBar->addAction(tr("Import CSV..."), this, &IdsEditorPage::onImportCsvClicked);
    m_toolBar->addAction(tr("Export CSV..."), this, &IdsEditorPage::onExportCsvClicked);
    m_toolBar->addSeparator();

    m_reloadButton = new QPushButton(tr("Reload Context"), this);
    connect(m_reloadButton, &QPushButton::clicked, this, &IdsEditorPage::reloadCurrentContext);
    m_toolBar->addWidget(m_reloadButton);

    m_toolBar->addSeparator();
    m_toolBar->addWidget(new QLabel(tr("Search"), this));
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("ID, text, DLL, type"));
    m_filterEdit->setClearButtonEnabled(true);
    m_filterEdit->setMinimumWidth(220);
    m_toolBar->addWidget(m_filterEdit);

    m_toolBar->addWidget(new QLabel(tr("Type"), this));
    m_typeFilterCombo = new QComboBox(this);
    m_typeFilterCombo->addItems({QStringLiteral("all"), QStringLiteral("ids_name"), QStringLiteral("ids_info"), QStringLiteral("strid_name"), QStringLiteral("string")});
    m_toolBar->addWidget(m_typeFilterCombo);

    m_toolBar->addWidget(new QLabel(tr("DLL"), this));
    m_dllFilterCombo = new QComboBox(this);
    m_dllFilterCombo->addItem(QStringLiteral("all"));
    m_toolBar->addWidget(m_dllFilterCombo);
}

void IdsEditorPage::loadDll(const QString &dllPath)
{
    applyDataset(IdsDataService::loadFromDllPath(dllPath));
}

void IdsEditorPage::loadFreelancerDir(const QString &exeDir)
{
    loadGameRootAsync(QFileInfo(exeDir).absolutePath());
}

void IdsEditorPage::loadGameRootAsync(const QString &gameRoot)
{
    if (gameRoot.trimmed().isEmpty())
        return;
    m_statusLabel->setText(tr("Loading IDS data from %1 ...").arg(gameRoot));
    m_reloadButton->setEnabled(false);
    m_loadWatcher->setFuture(QtConcurrent::run([gameRoot]() { return IdsDataService::loadFromGameRoot(gameRoot); }));
}

void IdsEditorPage::handleLoadFinished()
{
    m_reloadButton->setEnabled(true);
    applyDataset(m_loadWatcher->result());
}

void IdsEditorPage::applyDataset(IdsDataset dataset)
{
    m_dataset = std::move(dataset);

    auto *entryModel = static_cast<IdsEntryTableModel *>(m_proxy->sourceModel());
    entryModel->setEntries(m_dataset.entries);

    m_dllFilterCombo->blockSignals(true);
    m_targetDllCombo->blockSignals(true);
    m_dllFilterCombo->clear();
    m_dllFilterCombo->addItem(QStringLiteral("all"));
    m_targetDllCombo->clear();
    for (const QString &dllName : std::as_const(m_dataset.resourceDlls)) {
        m_dllFilterCombo->addItem(dllName.toLower());
        m_targetDllCombo->addItem(dllName);
    }
    m_dllFilterCombo->blockSignals(false);
    m_targetDllCombo->blockSignals(false);

    populateAuxiliaryModels();
    refreshIssueSummary();
    refreshStatusLabel();

    connect(m_tableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &IdsEditorPage::refreshEntrySelection, Qt::UniqueConnection);
    emit titleChanged(QStringLiteral("IDS Editor (%1)").arg(m_dataset.entries.size()));
}

void IdsEditorPage::populateAuxiliaryModels()
{
    m_missingModel->clear();
    m_missingModel->setHorizontalHeaderLabels({tr("Field"), tr("File"), tr("Section"), tr("Nickname"), tr("Archetype"), tr("Current")});
    for (const auto &record : std::as_const(m_dataset.missingAssignments)) {
        QList<QStandardItem *> row;
        auto *fieldItem = new QStandardItem(IdsDataService::usageTypeLabel(record.usageType));
        fieldItem->setData(record.sectionIndex, Qt::UserRole + 1);
        fieldItem->setData(record.filePath, Qt::UserRole + 2);
        fieldItem->setData(static_cast<int>(record.usageType), Qt::UserRole + 3);
        row << fieldItem
            << new QStandardItem(record.relativePath)
            << new QStandardItem(record.sectionName)
            << new QStandardItem(record.nickname)
            << new QStandardItem(record.archetype)
            << new QStandardItem(QString::number(record.currentValue));
        m_missingModel->appendRow(row);
    }

    m_issueModel->clear();
    m_issueModel->setHorizontalHeaderLabels({tr("Severity"), tr("Code"), tr("Target"), tr("Message")});
    for (const auto &issue : std::as_const(m_dataset.issues)) {
        QList<QStandardItem *> row;
        row << new QStandardItem(issueSeverityLabel(issue.severity))
            << new QStandardItem(issue.code)
            << new QStandardItem(issue.globalId > 0 ? QString::number(issue.globalId) : issue.dllName)
            << new QStandardItem(issue.message);
        m_issueModel->appendRow(row);
    }
}

void IdsEditorPage::refreshEntrySelection()
{
    const IdsEntryRecord *entry = selectedEntry();
    if (!entry) {
        if (!m_createMode)
            resetEditorState();
        m_usageModel->clear();
        return;
    }

    m_createMode = false;
    m_selectedEntryId = entry->globalId;
    populateEditorFromEntry(*entry);
    refreshUsageView();
}

void IdsEditorPage::refreshUsageView()
{
    m_usageModel->clear();
    m_usageModel->setHorizontalHeaderLabels({tr("Type"), tr("File"), tr("Section"), tr("Nickname"), tr("Archetype"), tr("Line")});
    const IdsEntryRecord *entry = selectedEntry();
    if (!entry)
        return;

    for (const auto &reference : std::as_const(m_dataset.references)) {
        if (reference.globalId != entry->globalId)
            continue;
        QList<QStandardItem *> row;
        auto *typeItem = new QStandardItem(IdsDataService::usageTypeLabel(reference.usageType));
        typeItem->setData(reference.filePath, Qt::UserRole + 1);
        typeItem->setData(reference.lineText.isEmpty() ? QString::number(reference.globalId) : reference.lineText, Qt::UserRole + 2);
        row << typeItem
            << new QStandardItem(reference.relativePath)
            << new QStandardItem(reference.sectionName)
            << new QStandardItem(reference.nickname)
            << new QStandardItem(reference.archetype)
            << new QStandardItem(reference.lineNumber > 0 ? QString::number(reference.lineNumber) : QStringLiteral("-"));
        m_usageModel->appendRow(row);
    }
}

void IdsEditorPage::refreshIssueSummary()
{
    int errors = 0;
    int warnings = 0;
    for (const auto &issue : std::as_const(m_dataset.issues)) {
        if (issue.severity == IdsIssueSeverity::Error)
            ++errors;
        else if (issue.severity == IdsIssueSeverity::Warning)
            ++warnings;
    }

    m_summaryLabel->setText(tr("Entries: %1 | References: %2 | Missing: %3 | Errors: %4 | Warnings: %5")
                                .arg(m_dataset.entries.size())
                                .arg(m_dataset.references.size())
                                .arg(m_dataset.missingAssignments.size())
                                .arg(errors)
                                .arg(warnings));
}

void IdsEditorPage::refreshStatusLabel(const QString &message)
{
    if (!message.isEmpty()) {
        m_statusLabel->setText(message);
        return;
    }
    m_statusLabel->setText(tr("Showing %1 of %2 IDS entries").arg(m_proxy->rowCount()).arg(m_dataset.entries.size()));
}

void IdsEditorPage::resetEditorState()
{
    beginCreateMode(m_createUsageType);
    m_validationLabel->clear();
    m_infocardPreview->clear();
}

void IdsEditorPage::populateEditorFromEntry(const IdsEntryRecord &entry)
{
    m_editorModeLabel->setText(tr("Editing existing IDS entry"));
    m_entryMetaLabel->setText(tr("Type: %1 | Usage: %2 | Local ID: %3 | %4")
                                  .arg(IdsDataService::normalizedEntryTypeKey(entry),
                                       entryUsageLabel(entry),
                                       QString::number(entry.localId),
                                       entry.editable ? tr("editable") : tr("read-only")));
    m_idEdit->setText(QString::number(entry.globalId));
    m_dllEdit->setText(entry.dllName);
    const int targetIndex = m_targetDllCombo->findText(entry.dllName);
    if (targetIndex >= 0)
        m_targetDllCombo->setCurrentIndex(targetIndex);

    if (entry.hasHtmlValue) {
        m_editorStack->setCurrentIndex(1);
        m_infocardEdit->setPlainText(entry.htmlValue);
        m_validationLabel->setText(tr("Use Validate before saving ids_info changes."));
    } else {
        m_editorStack->setCurrentIndex(0);
        m_stringEdit->setPlainText(entry.stringValue);
        m_validationLabel->clear();
    }
}

void IdsEditorPage::beginCreateMode(IdsUsageType usageType)
{
    m_createMode = true;
    m_createUsageType = usageType;
    m_selectedEntryId = 0;
    m_tableView->clearSelection();

    m_editorModeLabel->setText(tr("Creating new %1 entry").arg(IdsDataService::usageTypeLabel(usageType)));
    m_entryMetaLabel->setText(tr("New entries are created through the current DLL workflow."));
    m_idEdit->clear();
    m_dllEdit->clear();

    if (usageType == IdsUsageType::IdsInfo) {
        m_editorStack->setCurrentIndex(1);
        if (m_infocardEdit->toPlainText().trimmed().isEmpty())
            m_infocardEdit->setPlainText(QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-16\"?>\n<RDL><PUSH/><TEXT></TEXT><PARA/><POP/></RDL>"));
    } else {
        m_editorStack->setCurrentIndex(0);
        if (m_stringEdit->toPlainText().trimmed().isEmpty())
            m_stringEdit->clear();
    }
}

void IdsEditorPage::reloadCurrentContext()
{
    const auto &ctx = flatlas::core::EditingContext::instance();
    if (!ctx.hasContext()) {
        refreshStatusLabel(tr("No active editing context."));
        return;
    }
    loadGameRootAsync(ctx.primaryGamePath());
}

QString IdsEditorPage::currentEditorTargetDll() const
{
    return m_targetDllCombo->currentText().trimmed();
}

const IdsEntryRecord *IdsEditorPage::selectedEntry() const
{
    const QModelIndex current = m_tableView->currentIndex();
    if (!current.isValid())
        return nullptr;
    const QModelIndex source = m_proxy->mapToSource(m_proxy->index(current.row(), 0));
    return static_cast<IdsEntryTableModel *>(m_proxy->sourceModel())->entryAt(source.row());
}

int IdsEditorPage::selectedMissingRow() const
{
    const QModelIndex current = m_missingView->currentIndex();
    return current.isValid() ? current.row() : -1;
}

IdsUsageType IdsEditorPage::selectedMissingUsageType() const
{
    const int row = selectedMissingRow();
    if (row < 0)
        return IdsUsageType::Unknown;
    return static_cast<IdsUsageType>(m_missingModel->item(row, 0)->data(Qt::UserRole + 3).toInt());
}

QString IdsEditorPage::selectedReferenceSearchText() const
{
    const QModelIndex current = m_usageView->currentIndex();
    if (!current.isValid())
        return {};
    return m_usageModel->item(current.row(), 0)->data(Qt::UserRole + 2).toString();
}

void IdsEditorPage::saveCurrentEntry(bool createNew)
{
    if (m_dataset.freelancerIniPath.isEmpty() && m_dataset.resourceDlls.isEmpty()) {
        QMessageBox::warning(this, tr("IDS Editor"), tr("This dataset is not attached to a writable Freelancer context."));
        return;
    }

    const IdsEntryRecord *entry = selectedEntry();
    const bool htmlMode = createNew ? (m_createUsageType == IdsUsageType::IdsInfo) : (entry && entry->hasHtmlValue);

    int newGlobalId = 0;
    QString errorMessage;
    if (htmlMode) {
        const QString xmlText = m_infocardEdit->toPlainText().trimmed();
        QString validationError;
        if (!XmlInfocard::validate(xmlText, validationError)) {
            QMessageBox::warning(this, tr("ids_info validation"), validationError);
            return;
        }
        if (!IdsDataService::writeInfocardEntry(m_dataset,
                                                currentEditorTargetDll(),
                                                createNew || !entry ? 0 : entry->globalId,
                                                xmlText,
                                                &newGlobalId,
                                                &errorMessage)) {
            QMessageBox::warning(this, tr("Save ids_info"), errorMessage);
            return;
        }
    } else {
        const QString text = m_stringEdit->toPlainText().trimmed();
        if (text.isEmpty()) {
            QMessageBox::warning(this, tr("Save IDS string"), tr("Text must not be empty."));
            return;
        }
        if (!IdsDataService::writeStringEntry(m_dataset,
                                              currentEditorTargetDll(),
                                              createNew || !entry ? 0 : entry->globalId,
                                              text,
                                              &newGlobalId,
                                              &errorMessage)) {
            QMessageBox::warning(this, tr("Save IDS string"), errorMessage);
            return;
        }
    }

    refreshStatusLabel(tr("Saved IDS entry %1").arg(newGlobalId));
    reloadCurrentContext();
}

void IdsEditorPage::assignSelectedEntryToMissing()
{
    const int row = selectedMissingRow();
    if (row < 0) {
        QMessageBox::information(this, tr("Missing assignment"), tr("Select a missing assignment first."));
        return;
    }

    const IdsEntryRecord *entry = selectedEntry();
    if (!entry) {
        QMessageBox::information(this, tr("Missing assignment"), tr("Select an IDS entry first."));
        return;
    }

    const IdsUsageType usageType = selectedMissingUsageType();
    if (usageType == IdsUsageType::IdsInfo && !entry->hasHtmlValue) {
        QMessageBox::warning(this, tr("Missing assignment"), tr("The selected missing target requires an ids_info entry."));
        return;
    }
    if (usageType != IdsUsageType::IdsInfo && entry->hasHtmlValue) {
        QMessageBox::warning(this, tr("Missing assignment"), tr("The selected missing target requires a string entry, not ids_info."));
        return;
    }

    QString errorMessage;
    const QString filePath = m_missingModel->item(row, 0)->data(Qt::UserRole + 2).toString();
    const int sectionIndex = m_missingModel->item(row, 0)->data(Qt::UserRole + 1).toInt();
    const QString fieldName = IdsDataService::usageTypeKey(usageType);
    if (!IdsDataService::assignFieldValue(filePath, sectionIndex, fieldName, entry->globalId, &errorMessage)) {
        QMessageBox::warning(this, tr("Missing assignment"), errorMessage);
        return;
    }

    refreshStatusLabel(tr("Assigned %1 to %2").arg(entry->globalId).arg(fieldName));
    reloadCurrentContext();
}

void IdsEditorPage::createAndAssignToMissing()
{
    const int row = selectedMissingRow();
    if (row < 0) {
        QMessageBox::information(this, tr("Create + assign"), tr("Select a missing assignment first."));
        return;
    }

    const IdsUsageType usageType = selectedMissingUsageType();
    int newGlobalId = 0;
    QString errorMessage;
    if (usageType == IdsUsageType::IdsInfo) {
        const QString xmlText = m_infocardEdit->toPlainText().trimmed();
        QString validationError;
        if (!XmlInfocard::validate(xmlText, validationError)) {
            QMessageBox::warning(this, tr("Create + assign"), validationError);
            return;
        }
        if (!IdsDataService::writeInfocardEntry(m_dataset,
                                                currentEditorTargetDll(),
                                                0,
                                                xmlText,
                                                &newGlobalId,
                                                &errorMessage)) {
            QMessageBox::warning(this, tr("Create + assign"), errorMessage);
            return;
        }
    } else {
        const QString text = m_stringEdit->toPlainText().trimmed();
        if (text.isEmpty()) {
            QMessageBox::warning(this, tr("Create + assign"), tr("Text must not be empty."));
            return;
        }
        if (!IdsDataService::writeStringEntry(m_dataset,
                                              currentEditorTargetDll(),
                                              0,
                                              text,
                                              &newGlobalId,
                                              &errorMessage)) {
            QMessageBox::warning(this, tr("Create + assign"), errorMessage);
            return;
        }
    }

    const QString filePath = m_missingModel->item(row, 0)->data(Qt::UserRole + 2).toString();
    const int sectionIndex = m_missingModel->item(row, 0)->data(Qt::UserRole + 1).toInt();
    const QString fieldName = IdsDataService::usageTypeKey(usageType);
    if (!IdsDataService::assignFieldValue(filePath, sectionIndex, fieldName, newGlobalId, &errorMessage)) {
        QMessageBox::warning(this, tr("Create + assign"), errorMessage);
        return;
    }

    refreshStatusLabel(tr("Created %1 and assigned it to %2").arg(newGlobalId).arg(fieldName));
    reloadCurrentContext();
}

void IdsEditorPage::openSelectedReference()
{
    const QModelIndex current = m_usageView->currentIndex();
    if (!current.isValid())
        return;
    emit openIniRequested(m_usageModel->item(current.row(), 0)->data(Qt::UserRole + 1).toString(),
                          selectedReferenceSearchText());
}

void IdsEditorPage::openSelectedMissingLocation()
{
    const int row = selectedMissingRow();
    if (row < 0)
        return;
    emit openIniRequested(m_missingModel->item(row, 0)->data(Qt::UserRole + 2).toString(),
                          m_missingModel->item(row, 3)->text());
}

void IdsEditorPage::validateInfocardEditor()
{
    QString errorMessage;
    if (XmlInfocard::validate(m_infocardEdit->toPlainText(), errorMessage)) {
        m_validationLabel->setText(tr("ids_info XML/RDL is valid."));
        return;
    }
    m_validationLabel->setText(tr("Validation error: %1").arg(errorMessage));
}

void IdsEditorPage::prettyPrintInfocardEditor()
{
    QString errorMessage;
    const QString pretty = IdsDataService::prettyInfocardXml(m_infocardEdit->toPlainText(), &errorMessage);
    if (pretty.isEmpty()) {
        QMessageBox::warning(this, tr("Pretty Print ids_info"), errorMessage);
        return;
    }
    m_infocardEdit->setPlainText(pretty);
    m_validationLabel->setText(tr("ids_info formatted successfully."));
}

void IdsEditorPage::onFilterChanged(const QString &text)
{
    static_cast<IdsEntryFilterModel *>(m_proxy)->setSearchText(text);
    refreshStatusLabel();
}

void IdsEditorPage::onLoadDllClicked()
{
    const QString path = QFileDialog::getOpenFileName(this,
                                                      tr("Load DLL"),
                                                      QString(),
                                                      tr("DLL files (*.dll);;All files (*.*)"));
    if (!path.isEmpty())
        loadDll(path);
}

void IdsEditorPage::onExportCsvClicked()
{
    const QString path = QFileDialog::getSaveFileName(this,
                                                      tr("Export CSV"),
                                                      QStringLiteral("ids_entries.csv"),
                                                      tr("CSV files (*.csv);;All files (*.*)"));
    if (!path.isEmpty())
        exportCsv(path);
}

void IdsEditorPage::onImportCsvClicked()
{
    const QString path = QFileDialog::getOpenFileName(this,
                                                      tr("Import CSV"),
                                                      QString(),
                                                      tr("CSV files (*.csv);;All files (*.*)"));
    if (!path.isEmpty())
        importCsv(path);
}

void IdsEditorPage::importCsv(const QString &csvPath)
{
    QFile file(csvPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Import CSV"), tr("Could not read %1").arg(csvPath));
        return;
    }

    QVector<IdsEntryRecord> imported = m_dataset.entries;
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    if (!in.atEnd())
        in.readLine();

    while (!in.atEnd()) {
        const QString line = in.readLine();
        const QStringList parts = line.split(QLatin1Char(';'));
        if (parts.size() < 4)
            continue;

        bool ok = false;
        const int globalId = parts.at(0).trimmed().toInt(&ok);
        if (!ok)
            continue;

        IdsEntryRecord entry;
        entry.globalId = globalId;
        entry.dllName = parts.at(2).trimmed();
        const QString type = parts.at(1).trimmed().toLower();
        const QString text = parts.mid(3).join(QStringLiteral(";"));
        if (type == QStringLiteral("ids_info")) {
            entry.hasHtmlValue = true;
            entry.htmlValue = text;
            entry.plainText = XmlInfocard::parseToPlainText(text);
        } else {
            entry.hasStringValue = true;
            entry.stringValue = text;
        }
        entry.searchBlob = QStringLiteral("%1 %2 %3").arg(entry.globalId).arg(entry.dllName, text).toLower();
        imported.append(entry);
    }

    auto importedDataset = m_dataset;
    importedDataset.entries = std::move(imported);
    applyDataset(std::move(importedDataset));
}

void IdsEditorPage::exportCsv(const QString &csvPath)
{
    QFile file(csvPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export CSV"), tr("Could not write %1").arg(csvPath));
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "id;type;dll;text\n";
    for (const auto &entry : std::as_const(m_dataset.entries)) {
        QString text = entry.hasHtmlValue ? entry.htmlValue : entry.stringValue;
        text.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        if (text.contains(QLatin1Char(';')) || text.contains(QLatin1Char('\n')) || text.contains(QLatin1Char('"')))
            text = QLatin1Char('"') + text + QLatin1Char('"');
        out << entry.globalId << ';' << IdsDataService::normalizedEntryTypeKey(entry) << ';' << entry.dllName << ';' << text << '\n';
    }

    refreshStatusLabel(tr("Exported %1 entries").arg(m_dataset.entries.size()));
}

int IdsEditorPage::stringCount() const
{
    return m_dataset.entries.size();
}

QString IdsEditorPage::currentGameRoot() const
{
    return m_dataset.gameRoot;
}

} // namespace flatlas::editors
