#pragma once
#include <QGraphicsEllipseItem>
#include <QGraphicsSimpleTextItem>
#include <QString>
#include <QColor>
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
    flatlas::domain::SolarObject::Type objectType() const { return m_objType; }

    void updateFromObject(const flatlas::domain::SolarObject &obj);

private:
    static QColor colorForType(flatlas::domain::SolarObject::Type t);
    static qreal radiusForType(flatlas::domain::SolarObject::Type t);

    QString m_nickname;
    flatlas::domain::SolarObject::Type m_objType;
    QGraphicsSimpleTextItem *m_labelItem = nullptr;
};

} // namespace flatlas::rendering
