#include "SystemEditorPage.h"
#include <QVBoxLayout>
#include <QLabel>
namespace flatlas::editors {
SystemEditorPage::SystemEditorPage(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("System Editor – Phase 5"), this));
}
} // namespace flatlas::editors
