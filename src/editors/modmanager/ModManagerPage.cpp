#include "ModManagerPage.h"
#include <QVBoxLayout>
#include <QLabel>
namespace flatlas::editors {
ModManagerPage::ModManagerPage(QWidget *parent) : QWidget(parent) {
    auto *l = new QVBoxLayout(this); l->addWidget(new QLabel(tr("Mod Manager – Phase 13")));
}
} // namespace flatlas::editors
