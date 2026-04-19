#include "IdsEditorPage.h"
#include <QVBoxLayout>
#include <QLabel>
namespace flatlas::editors {
IdsEditorPage::IdsEditorPage(QWidget *parent) : QWidget(parent) {
    auto *l = new QVBoxLayout(this); l->addWidget(new QLabel(tr("IDS Editor – Phase 12")));
}
} // namespace flatlas::editors
