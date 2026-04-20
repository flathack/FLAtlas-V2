#include "SolarObjectItem.h"
#include <QPen>
#include <QBrush>
#include <QFont>

namespace flatlas::rendering {

SolarObjectItem::SolarObjectItem(const QString &nickname,
                                  flatlas::domain::SolarObject::Type objType,
                                  QGraphicsItem *parent)
    : QGraphicsEllipseItem(parent)
    , m_nickname(nickname)
    , m_objType(objType)
{
    const qreal r = radiusForType(objType);
    setRect(-r, -r, 2 * r, 2 * r);

    const QColor col = colorForType(objType);
    setBrush(QBrush(col));
    setPen(QPen(col.darker(130), 0.5));

    setFlags(ItemIsSelectable | ItemIsMovable | ItemSendsGeometryChanges);
    setToolTip(nickname);

    m_labelItem = new QGraphicsSimpleTextItem(nickname, this);
    m_labelItem->setBrush(QColor(220, 220, 230));
    m_labelItem->setFont(QFont(QStringLiteral("Segoe UI"), 7));
    m_labelItem->setPos(r + 3.0, -r - 2.0);
    m_labelItem->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
}

QColor SolarObjectItem::colorForType(flatlas::domain::SolarObject::Type t)
{
    using T = flatlas::domain::SolarObject::Type;
    switch (t) {
    case T::Sun:         return QColor(0xFF, 0xD7, 0x00); // #FFD700
    case T::Planet:      return QColor(0x44, 0x88, 0xCC); // #4488CC
    case T::Station:     return QColor(0x44, 0xCC, 0x44); // #44CC44
    case T::JumpGate:    return QColor(0xFF, 0x44, 0x44); // #FF4444
    case T::JumpHole:    return QColor(0xFF, 0x88, 0x00); // #FF8800
    case T::TradeLane:   return QColor(0x00, 0xCC, 0xCC); // #00CCCC
    case T::DockingRing: return QColor(0xCC, 0x44, 0xCC); // #CC44CC
    case T::Wreck:       return QColor(0x88, 0x88, 0x88); // #888888
    default:             return QColor(0xCC, 0xCC, 0xCC); // #CCCCCC
    }
}

qreal SolarObjectItem::radiusForType(flatlas::domain::SolarObject::Type t)
{
    using T = flatlas::domain::SolarObject::Type;
    switch (t) {
    case T::Sun:       return 8.0;
    case T::Planet:    return 5.0;
    case T::Station:   return 4.0;
    case T::JumpGate:  return 3.5;
    case T::JumpHole:  return 3.5;
    case T::TradeLane: return 2.0;
    default:           return 3.0;
    }
}

void SolarObjectItem::updateFromObject(const flatlas::domain::SolarObject &obj)
{
    constexpr double kScale = 0.01;
    m_nickname = obj.nickname();
    m_archetype = obj.archetype();
    m_objType = obj.type();
    setPos(obj.position().x() * kScale, obj.position().z() * kScale);
    if (m_labelItem)
        m_labelItem->setText(m_nickname);
}

void SolarObjectItem::setLabelVisibleForScale(qreal scale)
{
    constexpr qreal kLabelScaleThreshold = 2.0;
    const bool shouldShow = m_objectVisibleByFilter
        && m_labelVisibleByFilter
        && (isSelected() || scale >= kLabelScaleThreshold);
    if (m_labelItem)
        m_labelItem->setVisible(shouldShow);
}

void SolarObjectItem::applyDisplayFilter(const SystemDisplayFilterSettings &settings, qreal scale)
{
    SolarObjectDisplayContext context;
    context.nickname = m_nickname;
    context.archetype = m_archetype;
    context.type = m_objType;

    bool objectVisible = settings.objectVisibleForType(m_objType);
    bool labelVisible = settings.labelVisibleForType(m_objType);

    for (const SystemDisplayFilterRule &rule : settings.rules) {
        if (!matchesDisplayFilterRule(rule, context))
            continue;

        const bool show = rule.action == DisplayFilterAction::Show;
        if (rule.target == DisplayFilterTarget::Object || rule.target == DisplayFilterTarget::Both)
            objectVisible = show;
        if (rule.target == DisplayFilterTarget::Label || rule.target == DisplayFilterTarget::Both)
            labelVisible = show;
    }

    m_objectVisibleByFilter = objectVisible;
    m_labelVisibleByFilter = labelVisible;
    setVisible(m_objectVisibleByFilter);
    setLabelVisibleForScale(scale);
}

} // namespace flatlas::rendering
