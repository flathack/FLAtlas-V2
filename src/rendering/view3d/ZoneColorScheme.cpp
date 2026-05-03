#include "ZoneColorScheme.h"

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

} // namespace

ZoneVisualStyle ZoneColorScheme::styleForZone(const flatlas::domain::ZoneItem &zone)
{
    const QString name = zone.nickname().trimmed().toLower();
    const QString usage = zone.usage().trimmed().toLower();
    const QString popType = zone.popType().trimmed().toLower();
    const QString pathLabel = zone.pathLabel().trimmed().toLower();
    const QString zoneType = zone.zoneType().trimmed().toLower();
    const int damage = zone.damage();

    if (name.contains(QStringLiteral("death")) || name.contains(QStringLiteral("destroy_vignette")) || damage > 0)
        return makeStyle(QStringLiteral("death"), QColor(235, 45, 45), 30);

    if (name.contains(QStringLiteral("nebula"))
        || name.contains(QStringLiteral("badlands"))
        || name.contains(QStringLiteral("asteroid"))
        || name.contains(QStringLiteral("debris"))
        || zoneType.contains(QStringLiteral("nebula"))
        || zoneType.contains(QStringLiteral("asteroid"))
        || zoneType.contains(QStringLiteral("debris"))) {
        return makeStyle(QStringLiteral("asteroid/nebula"), QColor(45, 145, 255), 28);
    }

    if (name.contains(QStringLiteral("pop"))
        || !popType.isEmpty()
        || usage.contains(QStringLiteral("population"))
        || pathLabel.contains(QStringLiteral("pop"))) {
        return makeStyle(QStringLiteral("population"), QColor(245, 205, 55), 28);
    }

    Q_UNUSED(usage);
    Q_UNUSED(pathLabel);
    return makeStyle(QStringLiteral("generic"), QColor(155, 165, 175), 18);
}

} // namespace flatlas::rendering
