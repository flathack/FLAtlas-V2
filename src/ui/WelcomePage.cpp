#include "WelcomePage.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include "core/Config.h"
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

    auto *hint = new QLabel(tr("Open a Freelancer installation to get started.\nFile \u2192 Open..."), this);
    hint->setAlignment(Qt::AlignCenter);
    layout->addWidget(hint);

    layout->addSpacing(20);

    auto *recentHeader = new QLabel(tr("Recent Files"), this);
    recentHeader->setAlignment(Qt::AlignCenter);
    layout->addWidget(recentHeader);

    m_recentList = new QListWidget(this);
    m_recentList->setMaximumWidth(500);
    m_recentList->setAlternatingRowColors(true);
    layout->addWidget(m_recentList, 0, Qt::AlignCenter);

    connect(m_recentList, &QListWidget::itemClicked,
            this, [this](QListWidgetItem *item) {
        emit openFileRequested(item->data(Qt::UserRole).toString());
    });

    updateRecentFiles();
}

void WelcomePage::updateRecentFiles()
{
    m_recentList->clear();
    const QStringList recent = flatlas::core::Config::instance().getStringList(QStringLiteral("recentFiles"));
    for (const auto &path : recent) {
        auto *item = new QListWidgetItem(path, m_recentList);
        item->setData(Qt::UserRole, path);
    }
    m_recentList->setVisible(!recent.isEmpty());
}
} // namespace flatlas::ui
