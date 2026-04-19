#include "HelpBrowser.h"
#include <QVBoxLayout>
#include <QLabel>
namespace flatlas::tools {
HelpBrowser::HelpBrowser(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Help"));
    setMinimumSize(600, 400);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Help system – Phase 23")));
}
void HelpBrowser::showTopic(const QString &) {} // TODO Phase 23
} // namespace flatlas::tools
