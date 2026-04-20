#pragma once
#include <QGraphicsScene>
#include <memory>

namespace flatlas::domain {
class SystemDocument;
class SolarObject;
class ZoneItem;
}

namespace flatlas::rendering {

class SolarObjectItem;
class ZoneItem2D;

class MapScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit MapScene(QObject *parent = nullptr);

    void loadDocument(flatlas::domain::SystemDocument *doc);
    void clear();

    void setGridVisible(bool visible);
    bool isGridVisible() const;

    // System map is a top-down projection: X/Z in Freelancer space -> X/Y in Qt.
    static QPointF flToQt(float x, float z);
    static QPointF qtToFl(qreal x, qreal y);

signals:
    void objectSelected(const QString &nickname);
    void selectionCleared();

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;

private:
    void addSolarObject(const std::shared_ptr<flatlas::domain::SolarObject> &obj);
    void addZone(const std::shared_ptr<flatlas::domain::ZoneItem> &zone);

    flatlas::domain::SystemDocument *m_document = nullptr;
    bool m_gridVisible = true;
    static constexpr double kScale = 0.01; // Freelancer coords → scene coords
};

} // namespace flatlas::rendering
