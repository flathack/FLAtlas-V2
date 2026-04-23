#pragma once

#include <QGraphicsObject>
#include <QString>

namespace flatlas::rendering {

class LightSourceItem : public QGraphicsObject {
    Q_OBJECT
public:
    enum { Type = UserType + 4 };

    explicit LightSourceItem(const QString &nickname, QGraphicsItem *parent = nullptr);

    int type() const override { return Type; }
    QString nickname() const { return m_nickname; }

    QRectF boundingRect() const override;
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

private:
    QString m_nickname;
};

} // namespace flatlas::rendering
