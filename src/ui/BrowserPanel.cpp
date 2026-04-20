#include "BrowserPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QTreeView>
#include <QLabel>
#include <QPushButton>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QItemSelectionModel>
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
    layout->addWidget(m_refreshBtn);

    // Systems header
    m_systemsLabel = new QLabel(tr("Systems (click to load):"), this);
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

    // Dummy Freelancer systems organized by faction
    struct FactionSystems { const char *faction; QStringList systems; };
    const FactionSystems factions[] = {
        {"Liberty",       {"New York", "California", "Colorado", "Texas"}},
        {"Bretonia",      {"New London", "Cambridge", "Leeds", "Edinburgh"}},
        {"Kusari",        {"New Tokyo", "Hokkaido", "Kyushu", "Shikoku"}},
        {"Rheinland",     {"New Berlin", "Hamburg", "Stuttgart", "Frankfurt"}},
        {"Border Worlds", {"Omega-3", "Omega-5", "Omega-7", "Omega-11"}},
    };
    for (const auto &f : factions) {
        auto *factionItem = new QStandardItem(QString::fromLatin1(f.faction));
        factionItem->setEditable(false);
        for (const auto &sys : f.systems) {
            auto *sysItem = new QStandardItem(sys);
            sysItem->setEditable(false);
            factionItem->appendRow(sysItem);
        }
        m_model->appendRow(factionItem);
    }

    m_treeView = new QTreeView(this);
    m_treeView->setHeaderHidden(true);
    m_treeView->setModel(m_proxyModel);
    m_treeView->expandAll();
    layout->addWidget(m_treeView, 1);

    // Status label at bottom
    m_statusLabel = new QLabel(QStringLiteral("\u2714 20 systems"), this);
    m_statusLabel->setStyleSheet(
        QStringLiteral("QLabel { color: #4a9977; font-size: 11px; padding: 2px 0; }"));
    layout->addWidget(m_statusLabel);

    connect(m_filterEdit, &QLineEdit::textChanged,
            m_proxyModel, &QSortFilterProxyModel::setFilterFixedString);

    connect(m_treeView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex &current, const QModelIndex &) {
        if (!current.isValid() || current.model()->hasChildren(current))
            return; // ignore faction-level clicks
        emit systemSelected(current.data().toString());
    });
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
