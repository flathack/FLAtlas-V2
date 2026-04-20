#pragma once
#include <QGraphicsEllipseItem>
#include <QGraphicsSimpleTextItem>
#include <QString>
#include <QColor>
#include "../SystemDisplayFilter.h"
#include "domain/SolarObject.h"

namespace flatlas::rendering {

class SolarObjectItem : public QGraphicsEllipseItem {
public:
    enum { Type = UserType + 1 };
    int type() const override { return Type; }

    explicit SolarObjectItem(const QString &nickname,
                              flatlas::domain::SolarObject::Type objType,
                              QGraphicsItem *parent = nullptr);

    QString nickname() const { return m_nickname; }
    QString archetype() const { return m_archetype; }
    flatlas::domain::SolarObject::Type objectType() const { return m_objType; }

    void updateFromObject(const flatlas::domain::SolarObject &obj);
    void setLabelVisibleForScale(qreal scale);
    void applyDisplayFilter(const SystemDisplayFilterSettings &settings, qreal scale);

private:
    static QColor colorForType(flatlas::domain::SolarObject::Type t);
    static qreal radiusForType(flatlas::domain::SolarObject::Type t);
    static qreal radiusForObject(const flatlas::domain::SolarObject &obj);

    QString m_nickname;
    QString m_archetype;
    flatlas::domain::SolarObject::Type m_objType;
    QGraphicsSimpleTextItem *m_labelItem = nullptr;
    qreal m_currentRadius = 0.0;
    bool m_objectVisibleByFilter = true;
    bool m_labelVisibleByFilter = true;
};

} // namespace flatlas::rendering
