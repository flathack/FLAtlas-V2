#pragma once
#include <QGraphicsEllipseItem>
#include <QPointer>
#include <QPixmap>
#include <QGraphicsSimpleTextItem>
#include <QString>
#include <QColor>
#include "../SystemDisplayFilter.h"
#include "domain/SolarObject.h"

class QVariantAnimation;

namespace flatlas::rendering {

class SolarObjectItem : public QGraphicsEllipseItem {
public:
    enum { Type = UserType + 1 };
    int type() const override { return Type; }
    ~SolarObjectItem() override;

    explicit SolarObjectItem(const QString &nickname,
                              flatlas::domain::SolarObject::Type objType,
                              QGraphicsItem *parent = nullptr);

    QString nickname() const { return m_nickname; }
    QString archetype() const { return m_archetype; }
    flatlas::domain::SolarObject::Type objectType() const { return m_objType; }

    void updateFromObject(const flatlas::domain::SolarObject &obj);
    void setLabelVisibleForScale(qreal scale);
    void applyDisplayFilter(const SystemDisplayFilterSettings &settings, qreal scale);
    // Suppress this item's inline label because it would overlap with others
    // nearby. The caller is expected to follow up with setLabelVisibleForScale
    // (or applyDisplayFilter) so the visibility state is refreshed.
    void setLabelSuppressedByCluster(bool suppressed);
    bool isLabelSuppressedByCluster() const { return m_labelSuppressedByCluster; }
    qreal currentRadius() const { return m_currentRadius; }
    QRectF selectionCircleLocalRect() const;
    QRectF boundingRect() const override;
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    static QColor colorForType(flatlas::domain::SolarObject::Type t);
    static qreal radiusForType(flatlas::domain::SolarObject::Type t);
    static qreal radiusForObject(const flatlas::domain::SolarObject &obj, qreal modelRadius);
    void applyRotationFromObject(const flatlas::domain::SolarObject &obj);
    void syncLabelRotation();
    void ensureHoverAnimation();
    QPen hoverPen() const;
    QPen selectionGlowPen() const;
    qreal displayRadiusForCurrentStyle() const;

    QString m_nickname;
    QString m_archetype;
    flatlas::domain::SolarObject::Type m_objType;
    QGraphicsSimpleTextItem *m_labelItem = nullptr;
    QPointer<QVariantAnimation> m_hoverAnimation;
    QPixmap m_topViewIcon;
    qreal m_baseRadius = 0.0;
    qreal m_currentRadius = 0.0;
    qreal m_atmosphereRadius = 0.0;
    qreal m_hoverProgress = 0.0;
    bool m_objectVisibleByFilter = true;
    bool m_labelVisibleByFilter = true;
    bool m_labelSuppressedByCluster = false;
};

} // namespace flatlas::rendering
