#include "ToolboxPage.h"
#include "ToolIcons.h"

#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QList>
#include <QScrollArea>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

struct ToolTile
{
    QString key;
    QString iconKey;
    QString title;
    QString description;
};

QFrame *createTile(const ToolTile &tile, QWidget *parent, flatlas::ui::ToolboxPage *page)
{
    auto *frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("toolboxTile"));
    frame->setFrameShape(QFrame::StyledPanel);

    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *button = new QToolButton(frame);
    button->setObjectName(QStringLiteral("toolboxTileButton"));
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setIcon(flatlas::ui::toolIcon(tile.iconKey));
    button->setIconSize(QSize(32, 32));
    button->setText(tile.title);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QObject::connect(button, &QToolButton::clicked, page, [page, key = tile.key]() {
        emit page->toolRequested(key);
    });
    layout->addWidget(button);

    auto *description = new QLabel(tile.description, frame);
    description->setWordWrap(true);
    description->setTextInteractionFlags(Qt::NoTextInteraction);
    description->setObjectName(QStringLiteral("toolboxTileDescription"));
    layout->addWidget(description, 1);

    return frame;
}

} // namespace

namespace flatlas::ui {

ToolboxPage::ToolboxPage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("<h1>%1</h1>").arg(tr("Toolbox")), this);
    root->addWidget(title);

    auto *intro = new QLabel(
        tr("Open FLAtlas tools from one place. Each tile opens a tool tab or the matching tool dialog."),
        this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *content = new QWidget(scroll);
    auto *grid = new QGridLayout(content);
    grid->setContentsMargins(0, 8, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);

    const QList<ToolTile> tools = {
        {QStringLiteral("iniEditor"), QStringLiteral("iniEditor"), tr("File Editor"), tr("Browse and edit Freelancer INI files directly.")},
        {QStringLiteral("tradeRoutes"), QStringLiteral("tradeRoutes"), tr("Trade Routes"), tr("Analyze commodity routes and base prices.")},
        {QStringLiteral("idsEditor"), QStringLiteral("idsEditor"), tr("IDS Editor"), tr("Edit IDS names and infocards.")},
        {QStringLiteral("modManager"), QStringLiteral("modManager"), tr("Mod Manager"), tr("Manage installations, mods, and editing contexts.")},
        {QStringLiteral("modSettings"), QStringLiteral("modSettings"), tr("Mod Settings"), tr("Adjust shared mod settings for the active mod.")},
        {QStringLiteral("npcEditor"), QStringLiteral("npcEditor"), tr("NPC Editor"), tr("Edit mbases NPCs, bribes, rumors, outfits, and voices.")},
        {QStringLiteral("factionEditor"), QStringLiteral("factionEditor"), tr("Faction Editor"), tr("Edit factions and relationship data.")},
        {QStringLiteral("newsRumorEditor"), QStringLiteral("newsRumorEditor"), tr("News Editor"), tr("Edit news and rumor text assignments.")},
        {QStringLiteral("modelViewer"), QStringLiteral("modelViewer"), tr("3D Model Viewer"), tr("Browse and preview Freelancer models.")},
        {QStringLiteral("fieldCreator"), QStringLiteral("fieldCreator"), tr("Field Creator"), tr("Create asteroid and nebula field templates with placed assets.")},
        {QStringLiteral("shortestPath"), QStringLiteral("universe"), tr("Shortest Path"), tr("Find routes between systems in the current universe.")},
        {QStringLiteral("launchFreelancer"), QStringLiteral("launch"), tr("Launch Freelancer"), tr("Start Freelancer for the active editing context.")},
    };

    constexpr int columns = 3;
    for (int i = 0; i < tools.size(); ++i) {
        grid->addWidget(createTile(tools.at(i), content, this), i / columns, i % columns);
    }
    grid->setRowStretch((tools.size() + columns - 1) / columns, 1);
    for (int column = 0; column < columns; ++column)
        grid->setColumnStretch(column, 1);

    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    setStyleSheet(QStringLiteral(
        "QFrame#toolboxTile { border: 1px solid palette(mid); border-radius: 6px; background: palette(base); }"
        "QToolButton#toolboxTileButton { padding: 6px; text-align: left; font-weight: 600; }"
        "QLabel#toolboxTileDescription { color: palette(midlight); }"));
}

} // namespace flatlas::ui
