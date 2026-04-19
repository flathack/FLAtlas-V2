#include "CenterTabWidget.h"
namespace flatlas::ui {
CenterTabWidget::CenterTabWidget(QWidget *parent) : QTabWidget(parent)
{
    setTabsClosable(true);
    setMovable(true);
    setDocumentMode(true);

    connect(this, &QTabWidget::tabCloseRequested, this, [this](int index) {
        // TODO Phase 3: Dirty-Check vor Schließen
        removeTab(index);
    });
}
} // namespace flatlas::ui
