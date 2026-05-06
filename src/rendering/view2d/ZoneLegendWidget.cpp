#include "ZoneLegendWidget.h"

#include "core/ThemeColors.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QSizePolicy>

namespace flatlas::rendering {

namespace {

struct LegendEntry {
    QString label;
    QString colorKey;
};

QWidget *createSwatch(const QColor &color, QWidget *parent)
{
    auto *swatch = new QFrame(parent);
    swatch->setFixedSize(14, 14);
    swatch->setStyleSheet(QStringLiteral(
        "QFrame {"
        "  background-color: %1;"
        "  border: 1px solid rgba(0,0,0,110);"
        "  border-radius: 2px;"
        "}"
    ).arg(color.name(QColor::HexRgb)));
    return swatch;
}

} // namespace

ZoneLegendWidget::ZoneLegendWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("zoneLegendWidget"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    rebuild();
}

void ZoneLegendWidget::refreshThemeColors()
{
    rebuild();
}

void ZoneLegendWidget::rebuild()
{
    if (QLayout *oldLayout = layout()) {
        while (QLayoutItem *item = oldLayout->takeAt(0)) {
            if (QWidget *widget = item->widget())
                widget->deleteLater();
            delete item;
        }
        delete oldLayout;
    }

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(8, 4, 8, 4);
    row->setSpacing(10);

    const QVector<LegendEntry> entries{
        {tr("Nebula"), QStringLiteral("zoneNebula")},
        {tr("Asteroids"), QStringLiteral("zoneAsteroid")},
        {tr("Mine fields"), QStringLiteral("zoneMinefield")},
        {tr("Population"), QStringLiteral("zonePopulation")},
        {tr("Atmospheres"), QStringLiteral("zoneAtmosphere")},
        {tr("Damage"), QStringLiteral("zoneDeath")},
        {tr("Generic"), QStringLiteral("zoneGeneric")},
    };

    for (const LegendEntry &entry : entries) {
        auto *entryHost = new QWidget(this);
        auto *entryLayout = new QHBoxLayout(entryHost);
        entryLayout->setContentsMargins(0, 0, 0, 0);
        entryLayout->setSpacing(4);
        entryLayout->addWidget(createSwatch(flatlas::core::ThemeColors::color(entry.colorKey), entryHost));

        auto *label = new QLabel(entry.label, entryHost);
        label->setStyleSheet(QStringLiteral("font-size:11px; color:#9ca3af;"));
        entryLayout->addWidget(label);

        row->addWidget(entryHost);
    }

    row->addStretch(1);
}

} // namespace flatlas::rendering
