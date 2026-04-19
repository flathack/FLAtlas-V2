#pragma once
#include <QGraphicsEllipseItem>
#include <QString>
#include "domain/ZoneItem.h"

namespace flatlas::rendering {

class ZoneItem2D : public QGraphicsEllipseItem {
public:
    enum { Type = UserType + 2 };
    int type() const override { return Type; }

    explicit ZoneItem2D(const QString &nickname,
                        flatlas::domain::ZoneItem::Shape shape,
                        QGraphicsItem *parent = nullptr);

    QString nickname() const { return m_nickname; }
    void updateFromZone(const flatlas::domain::ZoneItem &zone);

private:
    QString m_nickname;
    flatlas::domain::ZoneItem::Shape m_shape;
};

} // namespace flatlas::rendering
