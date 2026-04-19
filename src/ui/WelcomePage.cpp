#include "WelcomePage.h"
#include <QVBoxLayout>
#include <QLabel>
namespace flatlas::ui {
WelcomePage::WelcomePage(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);

    auto *title = new QLabel(QStringLiteral("<h1>FLAtlas V2</h1>"), this);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto *subtitle = new QLabel(tr("Freelancer Editor Suite"), this);
    subtitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(subtitle);

    layout->addSpacing(20);

    auto *hint = new QLabel(tr("Open a Freelancer installation to get started.\nFile → Open..."), this);
    hint->setAlignment(Qt::AlignCenter);
    layout->addWidget(hint);

    // TODO Phase 3: Recent-Files-Liste, Quick-Actions
}
} // namespace flatlas::ui
