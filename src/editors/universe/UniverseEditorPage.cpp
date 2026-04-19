#include "UniverseEditorPage.h"
#include <QVBoxLayout>
#include <QLabel>
namespace flatlas::editors {
UniverseEditorPage::UniverseEditorPage(QWidget *parent) : QWidget(parent) {
    auto *l = new QVBoxLayout(this); l->addWidget(new QLabel(tr("Universe Editor – Phase 9")));
}
} // namespace flatlas::editors
