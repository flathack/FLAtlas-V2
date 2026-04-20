#include "ZoneItem2D.h"

#include <QBrush>
#include <QPainter>
#include <QPen>
#include <QQuaternion>
#include <QVector3D>

#include <cmath>

namespace flatlas::rendering {

ZoneItem2D::ZoneItem2D(const QString &nickname,
                        flatlas::domain::ZoneItem::Shape shape,
                        QGraphicsItem *parent)
    : QGraphicsEllipseItem(parent)
    , m_nickname(nickname)
    , m_shape(shape)
{
    using S = flatlas::domain::ZoneItem::Shape;

    QColor fill;
    switch (shape) {
    case S::Sphere:
    case S::Ellipsoid:
    case S::Ring:
        fill = QColor(60, 100, 180, 40);
        break;
    case S::Box:
    case S::Cylinder:
        fill = QColor(60, 180, 100, 40);
        break;
    default:
        fill = QColor(120, 120, 120, 40);
        break;
    }

    setBrush(QBrush(fill));

    QPen pen(fill.lighter(160));
    pen.setStyle(Qt::DotLine);
    pen.setWidthF(0.5);
    setPen(pen);

    setFlag(ItemIsSelectable);
    setZValue(-1);
    setToolTip(nickname);
}

QQuaternion ZoneItem2D::rotationQuaternionFromFl(float rx, float ry, float rz)
{
    constexpr float tolerance = 0.25f;
    float rxValue = rx;
    float ryValue = ry;
    float rzValue = rz;

    if (std::abs(std::abs(rxValue) - 180.0f) <= tolerance
        && std::abs(std::abs(rzValue) - 180.0f) <= tolerance) {
        rxValue = 0.0f;
        ryValue += 180.0f;
        rzValue = 0.0f;
        if (ryValue > 180.0f)
            ryValue -= 360.0f;
        else if (ryValue < -180.0f)
            ryValue += 360.0f;
    }

    return QQuaternion::fromEulerAngles(rxValue, ryValue, rzValue);
}

bool ZoneItem2D::usesLegacyCylinderYaw(const QString &nickname)
{
    const QString lowered = nickname.trimmed().toLower();
    return lowered.contains(QStringLiteral("path"))
        || lowered.contains(QStringLiteral("patrol"))
        || lowered.contains(QStringLiteral("exclusion"));
}

qreal ZoneItem2D::boxScreenRotation(const flatlas::domain::ZoneItem &zone, qreal sx, qreal sz)
{
    const QVector3D rotation = zone.rotation();
    const QQuaternion quat = rotationQuaternionFromFl(rotation.x(), rotation.y(), rotation.z());

    if (std::abs(sx) >= std::abs(sz)) {
        const QVector3D axis = quat.rotatedVector(QVector3D(1.0f, 0.0f, 0.0f));
        return std::atan2(axis.z(), axis.x()) * (180.0 / M_PI);
    }

    const QVector3D axis = quat.rotatedVector(QVector3D(0.0f, 0.0f, 1.0f));
    return -std::atan2(axis.x(), axis.z()) * (180.0 / M_PI);
}

QPen ZoneItem2D::penForZone(const flatlas::domain::ZoneItem &zone)
{
    const QString name = zone.nickname().toLower();
    const QString usage = zone.usage().toLower();
    const QString popType = zone.popType().toLower();
    const QString pathLabel = zone.pathLabel().toLower();
    const int damage = zone.damage();

    if (usage == QStringLiteral("patrol") || name.contains(QStringLiteral("_path_")) || !pathLabel.isEmpty()) {
        QPen pen(QColor(150, 150, 150, 77));
        pen.setWidthF(1.2);
        pen.setStyle(Qt::DotLine);
        return pen;
    }
    if (popType.contains(QStringLiteral("trade_path"))) {
        QPen pen(QColor(90, 190, 220, 170));
        pen.setWidthF(1.0);
        pen.setStyle(Qt::DashLine);
        return pen;
    }
    if (name.contains(QStringLiteral("pop_ambient")) || popType.contains(QStringLiteral("background"))) {
        QPen pen(QColor(210, 155, 70, 180));
        pen.setWidthF(1.1);
        return pen;
    }
    if (name.contains(QStringLiteral("death")) || name.contains(QStringLiteral("destroy_vignette")) || damage > 0) {
        QPen pen(QColor(150, 150, 150, 77));
        pen.setWidthF(1.5);
        pen.setStyle(Qt::DotLine);
        return pen;
    }
    if (name.contains(QStringLiteral("nebula")) || name.contains(QStringLiteral("badlands"))) {
        QPen pen(QColor(150, 80, 220, 180));
        pen.setWidthF(1.0);
        return pen;
    }
    if (name.contains(QStringLiteral("debris")) || name.contains(QStringLiteral("asteroid"))) {
        QPen pen(QColor(180, 130, 60, 180));
        pen.setWidthF(1.0);
        return pen;
    }
    if (name.contains(QStringLiteral("tradelane"))) {
        QPen pen(QColor(60, 180, 220, 160));
        pen.setWidthF(1.0);
        pen.setStyle(Qt::DashLine);
        return pen;
    }
    if (name.contains(QStringLiteral("jumpgate")) || name.contains(QStringLiteral("hole"))) {
        QPen pen(QColor(180, 100, 220, 200));
        pen.setWidthF(1.5);
        return pen;
    }
    if (name.contains(QStringLiteral("exclusion"))) {
        QPen pen(QColor(220, 100, 50, 140));
        pen.setWidthF(1.0);
        pen.setStyle(Qt::DotLine);
        return pen;
    }
    if (name.contains(QStringLiteral("path")) || name.contains(QStringLiteral("patrol")) || name.contains(QStringLiteral("vignette"))) {
        QPen pen(QColor(100, 100, 150, 90));
        pen.setWidthF(1.0);
        pen.setStyle(Qt::DotLine);
        return pen;
    }

    QPen pen(QColor(80, 160, 200, 150));
    pen.setWidthF(1.0);
    return pen;
}

QBrush ZoneItem2D::brushForZone(const flatlas::domain::ZoneItem &zone)
{
    const QString name = zone.nickname().toLower();
    const QString usage = zone.usage().toLower();
    const QString popType = zone.popType().toLower();
    const QString pathLabel = zone.pathLabel().toLower();
    const int damage = zone.damage();

    if (usage == QStringLiteral("patrol") || name.contains(QStringLiteral("_path_")) || !pathLabel.isEmpty())
        return QBrush(Qt::NoBrush);
    if (popType.contains(QStringLiteral("trade_path")))
        return QBrush(Qt::NoBrush);
    if (name.contains(QStringLiteral("pop_ambient")) || popType.contains(QStringLiteral("background")))
        return QBrush(QColor(190, 145, 60, 16));
    if (name.contains(QStringLiteral("death")) || name.contains(QStringLiteral("destroy_vignette")) || damage > 0)
        return QBrush(Qt::NoBrush);
    if (name.contains(QStringLiteral("nebula")) || name.contains(QStringLiteral("badlands")))
        return QBrush(QColor(120, 60, 200, 18));
    if (name.contains(QStringLiteral("debris")) || name.contains(QStringLiteral("asteroid")))
        return QBrush(QColor(160, 120, 50, 18));
    if (name.contains(QStringLiteral("tradelane")))
        return QBrush(QColor(60, 180, 220, 12));
    if (name.contains(QStringLiteral("jumpgate")) || name.contains(QStringLiteral("hole")))
        return QBrush(QColor(160, 80, 200, 18));
    if (name.contains(QStringLiteral("exclusion")))
        return QBrush(QColor(200, 80, 40, 8));
    if (name.contains(QStringLiteral("path")) || name.contains(QStringLiteral("patrol")) || name.contains(QStringLiteral("vignette")))
        return QBrush(Qt::NoBrush);

    return QBrush(QColor(60, 140, 180, 14));
}

void ZoneItem2D::updateFromZone(const flatlas::domain::ZoneItem &zone)
{
    constexpr qreal kScale = 0.01;

    m_shape = zone.shape();
    m_nickname = zone.nickname();
    setPen(penForZone(zone));
    setBrush(brushForZone(zone));

    setPos(zone.position().x() * kScale, zone.position().z() * kScale);
    setRotation(0.0);
    m_drawAsRect = false;
    m_drawCylinderAsEllipse = false;

    const qreal sx = zone.size().x() * kScale;
    const qreal sy = zone.size().y() * kScale;
    const qreal sz = zone.size().z() * kScale;
    qreal halfWidth = sx;
    qreal halfHeight = sz;

    switch (zone.shape()) {
    case flatlas::domain::ZoneItem::Sphere:
        halfWidth = sx;
        halfHeight = sx;
        break;

    case flatlas::domain::ZoneItem::Ellipsoid:
    case flatlas::domain::ZoneItem::Ring:
        halfWidth = sx;
        halfHeight = sz;
        break;

    case flatlas::domain::ZoneItem::Box:
        halfWidth = sx * 0.5;
        halfHeight = sz * 0.5;
        m_drawAsRect = true;
        setRotation(boxScreenRotation(zone, sx, sz));
        break;

    case flatlas::domain::ZoneItem::Cylinder: {
        const qreal radius = sx;
        const qreal length = sy;
        const QVector3D rotation = zone.rotation();

        if (usesLegacyCylinderYaw(m_nickname)) {
            halfWidth = radius;
            halfHeight = length * 0.5;
            qreal yaw = rotation.y();
            constexpr qreal tolerance = 0.25;
            if (std::abs(std::abs(rotation.x()) - 90.0) <= tolerance
                && std::abs(std::abs(rotation.z()) - 180.0) <= tolerance) {
                yaw = -yaw;
            }
            m_drawAsRect = true;
            setRotation(-yaw);
        } else {
            const QQuaternion quat = rotationQuaternionFromFl(rotation.x(), rotation.y(), rotation.z());
            const QVector3D axis = quat.rotatedVector(QVector3D(0.0f, 1.0f, 0.0f));
            const qreal projectedAxisLength = length * std::hypot(axis.x(), axis.z());
            m_drawCylinderAsEllipse = std::abs(axis.y()) >= 0.85 || projectedAxisLength <= radius * 0.35;

            if (m_drawCylinderAsEllipse) {
                halfWidth = radius;
                halfHeight = radius;
            } else {
                halfWidth = std::max<qreal>(radius, projectedAxisLength * 0.5);
                halfHeight = radius;
                m_drawAsRect = true;
                setRotation(-(std::atan2(axis.x(), axis.z()) * (180.0 / M_PI)));
            }
        }
        break;
    }
    }

    setRect(-halfWidth, -halfHeight, halfWidth * 2.0, halfHeight * 2.0);
}

void ZoneItem2D::applyDisplayFilter(const SystemDisplayFilterSettings &settings)
{
    SolarObjectDisplayContext context;
    context.nickname = m_nickname;
    context.typeNameOverride = QStringLiteral("Zone");

    bool visible = true;
    for (const SystemDisplayFilterRule &rule : settings.rules) {
        if (!matchesDisplayFilterRule(rule, context))
            continue;
        if (rule.target == DisplayFilterTarget::Object || rule.target == DisplayFilterTarget::Both)
            visible = (rule.action == DisplayFilterAction::Show);
    }

    m_visibleByFilter = visible;
    setVisible(m_visibleByFilter);
}

void ZoneItem2D::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    painter->setPen(pen());
    painter->setBrush(brush());

    if (m_shape == flatlas::domain::ZoneItem::Cylinder && m_drawCylinderAsEllipse) {
        painter->drawEllipse(rect());
        return;
    }

    if (m_drawAsRect)
        painter->drawRect(rect());
    else
        painter->drawEllipse(rect());
}

} // namespace flatlas::rendering
