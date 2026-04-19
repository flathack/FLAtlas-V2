#include "PropertiesPanel.h"
#include <QVBoxLayout>
#include <QTreeView>
#include <QLabel>
namespace flatlas::ui {
PropertiesPanel::PropertiesPanel(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *header = new QLabel(tr("Properties"), this);
    header->setStyleSheet(QStringLiteral("font-weight: bold; padding: 4px;"));
    layout->addWidget(header);

    m_treeView = new QTreeView(this);
    layout->addWidget(m_treeView);

    // TODO Phase 3/5: Custom Model für Objekt-Properties
}
void PropertiesPanel::showProperties(const QVariantMap &) {} // TODO
void PropertiesPanel::clear() {} // TODO
} // namespace flatlas::ui
