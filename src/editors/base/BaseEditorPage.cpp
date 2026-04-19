#include "BaseEditorPage.h"
#include <QVBoxLayout>
#include <QLabel>
namespace flatlas::editors {
BaseEditorPage::BaseEditorPage(QWidget *parent) : QWidget(parent) {
    auto *l = new QVBoxLayout(this); l->addWidget(new QLabel(tr("Base Editor – Phase 10")));
}
} // namespace flatlas::editors
