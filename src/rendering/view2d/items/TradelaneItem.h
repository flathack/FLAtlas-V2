#pragma once
#include <QGraphicsLineItem>
#include <QString>

namespace flatlas::rendering {

class TradelaneItem : public QGraphicsLineItem {
public:
    enum { Type = UserType + 3 };
    int type() const override { return Type; }

    explicit TradelaneItem(const QString &nickname,
                           const QPointF &start, const QPointF &end,
                           QGraphicsItem *parent = nullptr);

    QString nickname() const { return m_nickname; }
    void setHighlighted(bool highlighted);

private:
    QString m_nickname;
};

} // namespace flatlas::rendering
