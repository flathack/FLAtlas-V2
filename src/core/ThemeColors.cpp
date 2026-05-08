#include "ThemeColors.h"

#include "Config.h"

#include <QObject>

namespace flatlas::core {
namespace {

QString configKey(const QString &key)
{
    return QStringLiteral("themeColors/%1").arg(key);
}

QColor defaultColorForKey(const QString &key)
{
    for (const ThemeColorChoice &choice : ThemeColors::choices()) {
        if (choice.key == key)
            return choice.defaultColor;
    }
    return QColor(155, 165, 175);
}

} // namespace

QVector<ThemeColorChoice> ThemeColors::choices()
{
    return {
        {QStringLiteral("uiAccent"), QObject::tr("UI color"), QColor(QStringLiteral("#00449D"))},
        {QStringLiteral("zoneNebula"), QObject::tr("Nebula fields"), QColor(45, 145, 255)},
        {QStringLiteral("zoneAsteroid"), QObject::tr("Asteroid fields"), QColor(140, 92, 48)},
        {QStringLiteral("zoneMinefield"), QObject::tr("Mine fields"), QColor(255, 145, 35)},
        {QStringLiteral("zonePopulation"), QObject::tr("Zone population fields"), QColor(40, 220, 220)},
        {QStringLiteral("zoneAtmosphere"), QObject::tr("Planet and sun atmospheres"), QColor(255, 190, 75)},
        {QStringLiteral("zoneDeath"), QObject::tr("Damage zones"), QColor(235, 45, 45)},
        {QStringLiteral("zoneGeneric"), QObject::tr("Generic zones"), QColor(155, 165, 175)},
    };
}

QColor ThemeColors::color(const QString &key)
{
    const QColor fallback = defaultColorForKey(key);
    const QString saved = Config::instance().getString(configKey(key), fallback.name(QColor::HexRgb));
    const QColor color(saved);
    return color.isValid() ? color : fallback;
}

void ThemeColors::setColor(const QString &key, const QColor &color)
{
    if (!color.isValid())
        return;
    Config::instance().setString(configKey(key), color.name(QColor::HexRgb));
}

void ThemeColors::resetToDefaults()
{
    for (const ThemeColorChoice &choice : choices())
        Config::instance().setString(configKey(choice.key), choice.defaultColor.name(QColor::HexRgb));
}

} // namespace flatlas::core
