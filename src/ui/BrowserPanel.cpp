#include "BrowserPanel.h"
#include <QVBoxLayout>
#include <QLineEdit>
#include <QTreeView>
#include <QLabel>
namespace flatlas::ui {
BrowserPanel::BrowserPanel(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Filter systems..."));
    layout->addWidget(m_filterEdit);

    m_treeView = new QTreeView(this);
    m_treeView->setHeaderHidden(true);
    layout->addWidget(m_treeView);

    // TODO Phase 3: QAbstractItemModel für Systemliste
}
} // namespace flatlas::ui
