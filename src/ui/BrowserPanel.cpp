#include "BrowserPanel.h"
#include <QVBoxLayout>
#include <QLineEdit>
#include <QTreeView>
#include <QLabel>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QItemSelectionModel>
namespace flatlas::ui {
BrowserPanel::BrowserPanel(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

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
    layout->addWidget(m_treeView);

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
}
} // namespace flatlas::ui
