#include "NewsRumorEditor.h"
#include <QVBoxLayout>
#include <QLabel>
namespace flatlas::editors {
NewsRumorEditor::NewsRumorEditor(QWidget *parent) : QWidget(parent) {
    auto *l = new QVBoxLayout(this); l->addWidget(new QLabel(tr("News/Rumor Editor – Phase 18")));
}
} // namespace flatlas::editors
