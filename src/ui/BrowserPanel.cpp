#include "BrowserPanel.h"
#include "core/EditingContext.h"
#include "infrastructure/freelancer/UniverseScanner.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QTreeView>
#include <QLabel>
#include <QPushButton>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QItemSelectionModel>
#include <QDir>
#include <algorithm>
namespace flatlas::ui {
BrowserPanel::BrowserPanel(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 8, 6, 6);
    layout->setSpacing(6);

    // Title: ★ System Browser
    m_titleLabel = new QLabel(QStringLiteral("\u2605 System Browser"), this);
    m_titleLabel->setStyleSheet(
        QStringLiteral("QLabel { font-size: 14px; font-weight: bold; color: #ccdde8; padding: 2px 0; }"));
    layout->addWidget(m_titleLabel);

    // Quick Access section
    auto *quickAccessLabel = new QLabel(tr("Quick Access"), this);
    quickAccessLabel->setStyleSheet(
        QStringLiteral("QLabel { color: #6699aa; font-size: 11px; margin-top: 4px; }"));
    layout->addWidget(quickAccessLabel);

    m_refreshBtn = new QPushButton(QStringLiteral("\u2611 ") + tr("Refresh Systems"), this);
    m_refreshBtn->setStyleSheet(
        QStringLiteral("QPushButton { background: #1a2535; color: #8899bb; border: 1px solid #2a3a4a;"
                        " padding: 5px; border-radius: 2px; text-align: left; }"
                        "QPushButton:hover { background: #223045; color: #aabbdd; }"));
    connect(m_refreshBtn, &QPushButton::clicked, this, &BrowserPanel::refreshFromContext);
    layout->addWidget(m_refreshBtn);

    // Systems header
    m_systemsLabel = new QLabel(tr("Systems:"), this);
    m_systemsLabel->setStyleSheet(
        QStringLiteral("QLabel { color: #7799aa; font-size: 11px; margin-top: 6px; }"));
    layout->addWidget(m_systemsLabel);

    // Filter
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Filter systems..."));
    layout->addWidget(m_filterEdit);

    m_model = new QStandardItemModel(this);
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setRecursiveFilteringEnabled(true);

    m_treeView = new QTreeView(this);
    m_treeView->setHeaderHidden(true);
    m_treeView->setModel(m_proxyModel);
    layout->addWidget(m_treeView, 1);

    // Status label at bottom
    m_statusLabel = new QLabel(tr("Set editing context to load systems"), this);
    m_statusLabel->setStyleSheet(
        QStringLiteral("QLabel { color: #4a9977; font-size: 11px; padding: 2px 0; }"));
    layout->addWidget(m_statusLabel);

    connect(m_filterEdit, &QLineEdit::textChanged,
            m_proxyModel, &QSortFilterProxyModel::setFilterFixedString);

    connect(m_treeView, &QTreeView::doubleClicked,
            this, [this](const QModelIndex &index) {
        if (!index.isValid())
            return;
        const QString nickname = index.data(Qt::UserRole).toString();
        const QString filePath = index.data(Qt::UserRole + 1).toString();
        if (!nickname.isEmpty() && !filePath.isEmpty())
            emit systemSelected(nickname, filePath);
    });

    // Connect to editing context changes
    connect(&flatlas::core::EditingContext::instance(),
            &flatlas::core::EditingContext::contextChanged,
            this, &BrowserPanel::refreshFromContext);

    // Load if context already set
    if (flatlas::core::EditingContext::instance().hasContext())
        refreshFromContext();
}

void BrowserPanel::refreshFromContext()
{
    auto &ctx = flatlas::core::EditingContext::instance();
    if (!ctx.hasContext()) {
        m_model->clear();
        m_systems.clear();
        m_dataDir.clear();
        m_statusLabel->setText(tr("Set editing context to load systems"));
        return;
    }

    m_dataDir = ctx.primaryGamePath() + QStringLiteral("/DATA");
    auto universeData = flatlas::infrastructure::UniverseScanner::scan(m_dataDir);
    m_systems = universeData.systems;

    // Sort by nickname
    std::sort(m_systems.begin(), m_systems.end(),
              [](const flatlas::domain::SystemInfo &a, const flatlas::domain::SystemInfo &b) {
        return a.nickname.compare(b.nickname, Qt::CaseInsensitive) < 0;
    });

    m_model->clear();
    for (const auto &sys : m_systems) {
        QString displayText = sys.nickname;
        if (!sys.displayName.isEmpty())
            displayText = sys.nickname + QStringLiteral(" - ") + sys.displayName;

        auto *item = new QStandardItem(displayText);
        item->setEditable(false);
        item->setData(sys.nickname, Qt::UserRole);
        // Resolve full path from DATA dir
        QString fullPath = QDir(m_dataDir).filePath(sys.filePath);
        item->setData(fullPath, Qt::UserRole + 1);
        m_model->appendRow(item);
    }

    m_statusLabel->setText(QStringLiteral("\u2714 %1 systems").arg(m_systems.size()));
}

void BrowserPanel::loadSystems(const QStringList &systemNames)
{
    m_model->clear();
    for (const auto &name : systemNames) {
        auto *item = new QStandardItem(name);
        item->setEditable(false);
        m_model->appendRow(item);
    }
    m_statusLabel->setText(QStringLiteral("\u2714 %1 systems").arg(systemNames.size()));
}
} // namespace flatlas::ui
