#include "TradeRoutePage.h"
#include <QVBoxLayout>
#include <QLabel>
namespace flatlas::editors {
TradeRoutePage::TradeRoutePage(QWidget *parent) : QWidget(parent) {
    auto *l = new QVBoxLayout(this); l->addWidget(new QLabel(tr("Trade Routes – Phase 11")));
}
} // namespace flatlas::editors
