#include "InfocardEditor.h"
#include <QVBoxLayout>
#include <QLabel>
namespace flatlas::editors {
InfocardEditor::InfocardEditor(QWidget *parent) : QWidget(parent) {
    auto *l = new QVBoxLayout(this); l->addWidget(new QLabel(tr("Infocard Editor – Phase 17")));
}
} // namespace flatlas::editors
