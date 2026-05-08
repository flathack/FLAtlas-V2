#include "ZoneColorScheme.h"

#include "core/ThemeColors.h"

namespace flatlas::rendering {

namespace {

QColor withAlpha(QColor color, int alpha)
{
    color.setAlpha(qBound(0, alpha, 255));
    return color;
}

ZoneVisualStyle makeStyle(const QString &category, const QColor &base, int fillAlpha)
{
    ZoneVisualStyle style;
    style.category = category;
    style.fillColor = withAlpha(base, fillAlpha);
    style.wireColor = withAlpha(base.lighter(145), 220);
    style.selectedFillColor = withAlpha(base.lighter(155), qMin(190, fillAlpha + 80));
    style.selectedWireColor = QColor(255, 235, 95, 245);
    style.opacity = static_cast<float>(fillAlpha) / 255.0f;
    style.fillVisible = true;
    return style;
}

ZoneVisualStyle makeMixedStyle(const QString &category, const QColor &base, const QColor &accent, int fillAlpha)
{
    ZoneVisualStyle style = makeStyle(category, base, fillAlpha);
    style.wireColor = withAlpha(accent.lighter(135), 230);
    style.selectedWireColor = withAlpha(accent.lighter(170), 245);
    return style;
}

ZoneVisualStyle makeWireOnlyStyle(const QString &category, const QColor &wire)
{
    ZoneVisualStyle style = makeStyle(category, wire, 0);
    style.fillColor = withAlpha(wire, 0);
    style.wireColor = withAlpha(wire.lighter(135), 230);
    style.selectedFillColor = withAlpha(wire.lighter(145), 55);
    style.selectedWireColor = withAlpha(wire.lighter(170), 245);
    style.opacity = 0.0f;
    style.fillVisible = false;
    return style;
}

ZoneVisualStyle makeMinefieldStyle(const QColor &base, bool isPopulation)
{
    ZoneVisualStyle style = isPopulation
        ? makeMixedStyle(QStringLiteral("minefield/population"),
                         base,
                         flatlas::core::ThemeColors::color(QStringLiteral("zonePopulation")),
                         32)
        : makeStyle(QStringLiteral("minefield"), base, 32);
    if (!isPopulation)
        style.wireColor = withAlpha(base.lighter(155), 245);
    style.selectedWireColor = withAlpha(base.lighter(180), 255);
    style.denseWire = true;
    return style;
}

QString rawEntryValue(const flatlas::domain::ZoneItem &zone, const QString &key)
{
    const auto entries = zone.rawEntries();
    for (int index = entries.size() - 1; index >= 0; --index) {
        if (entries[index].first.compare(key, Qt::CaseInsensitive) == 0)
            return entries[index].second.trimmed();
    }
    return {};
}

} // namespace

ZoneVisualStyle ZoneColorScheme::styleForZone(const flatlas::domain::ZoneItem &zone)
{
    const QString name = zone.nickname().trimmed().toLower();
    const QString usage = zone.usage().trimmed().toLower();
    const QString popType = zone.popType().trimmed().toLower();
    const QString pathLabel = zone.pathLabel().trimmed().toLower();
    const QString zoneType = zone.zoneType().trimmed().toLower();
    const QString music = rawEntryValue(zone, QStringLiteral("music")).toLower();
    const QString propertyFlags = rawEntryValue(zone, QStringLiteral("property_flags")).toLower();
    const bool isPopulation = name.contains(QStringLiteral("pop"))
        || !popType.isEmpty()
        || usage.contains(QStringLiteral("population"))
        || pathLabel.contains(QStringLiteral("pop"));
    const bool isPath = usage.contains(QStringLiteral("patrol"))
        || name.contains(QStringLiteral("_path_"))
        || name.contains(QStringLiteral("patrol"))
        || !pathLabel.isEmpty()
        || popType.contains(QStringLiteral("trade_path"))
        || popType.contains(QStringLiteral("patrol"));

    if (name.contains(QStringLiteral("death")) || name.contains(QStringLiteral("destroy_vignette")))
        return makeStyle(QStringLiteral("death"), flatlas::core::ThemeColors::color(QStringLiteral("zoneDeath")), 30);

    if (name.contains(QStringLiteral("atmosphere"))
        || zoneType.contains(QStringLiteral("atmosphere")))
        return makeStyle(QStringLiteral("atmosphere"), flatlas::core::ThemeColors::color(QStringLiteral("zoneAtmosphere")), 24);

    bool hasMinefieldPropertyFlags = false;
    for (const QString &flag : propertyFlags.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        if (flag.trimmed() == QStringLiteral("4128")) {
            hasMinefieldPropertyFlags = true;
            break;
        }
    }
    const bool isMinefield = music.contains(QStringLiteral("mine"))
        || hasMinefieldPropertyFlags;
    if (isMinefield)
        return makeMinefieldStyle(flatlas::core::ThemeColors::color(QStringLiteral("zoneMinefield")), isPopulation);

    const bool isAsteroid = name.contains(QStringLiteral("asteroid"))
        || name.contains(QStringLiteral("debris"))
        || zoneType.contains(QStringLiteral("asteroid"))
        || zoneType.contains(QStringLiteral("debris"))
        || music.contains(QStringLiteral("asteroid"))
        || music.contains(QStringLiteral("debris"));
    if (isAsteroid) {
        const QColor base = flatlas::core::ThemeColors::color(QStringLiteral("zoneAsteroid"));
        if (isPopulation)
            return makeMixedStyle(QStringLiteral("asteroid/population"),
                                  base,
                                  flatlas::core::ThemeColors::color(QStringLiteral("zonePopulation")),
                                  28);
        if (isPath)
            return makeWireOnlyStyle(QStringLiteral("asteroid/path"), base);
        return makeStyle(QStringLiteral("asteroid"), base, 28);
    }

    const bool isNebula = name.contains(QStringLiteral("nebula"))
        || name.contains(QStringLiteral("badlands"))
        || zoneType.contains(QStringLiteral("nebula"));
    if (isNebula) {
        const QColor base = flatlas::core::ThemeColors::color(QStringLiteral("zoneNebula"));
        if (isPopulation)
            return makeMixedStyle(QStringLiteral("nebula/population"),
                                  base,
                                  flatlas::core::ThemeColors::color(QStringLiteral("zonePopulation")),
                                  28);
        if (isPath)
            return makeWireOnlyStyle(QStringLiteral("nebula/path"), base);
        return makeStyle(QStringLiteral("nebula"), base, 28);
    }

    if (isPopulation)
        return makeStyle(QStringLiteral("population"), flatlas::core::ThemeColors::color(QStringLiteral("zonePopulation")), 28);

    if (isPath)
        return makeWireOnlyStyle(QStringLiteral("path"), flatlas::core::ThemeColors::color(QStringLiteral("zonePopulation")));

    Q_UNUSED(usage);
    Q_UNUSED(pathLabel);
    return makeStyle(QStringLiteral("generic"), flatlas::core::ThemeColors::color(QStringLiteral("zoneGeneric")), 18);
}

} // namespace flatlas::rendering
