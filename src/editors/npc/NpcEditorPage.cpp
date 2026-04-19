#include "NpcEditorPage.h"
#include <QVBoxLayout>
#include <QLabel>
namespace flatlas::editors {
NpcEditorPage::NpcEditorPage(QWidget *parent) : QWidget(parent) {
    auto *l = new QVBoxLayout(this); l->addWidget(new QLabel(tr("NPC Editor – Phase 14")));
}
} // namespace flatlas::editors
