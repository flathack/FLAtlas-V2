#pragma once
#include <QGraphicsScene>
#include <QStringList>
#include <QVector3D>
#include <functional>
#include <memory>

namespace flatlas::domain {
class SystemDocument;
class SolarObject;
class ZoneItem;
}

namespace flatlas::rendering {

class SolarObjectItem;
class ZoneItem2D;
class TradelaneItem;

class MapScene : public QGraphicsScene {
    Q_OBJECT
public:
    struct LightSourceVisual {
        QString nickname;
        QVector3D position;
    };

    explicit MapScene(QObject *parent = nullptr);

    void loadDocument(flatlas::domain::SystemDocument *doc,
                      const std::function<void(int current, int total)> &progressCallback = {});
    void clear();
    void setLightSources(const QVector<LightSourceVisual> &lightSources);

    void setGridVisible(bool visible);
    bool isGridVisible() const;
    void setMoveEnabled(bool enabled);
    bool isMoveEnabled() const;
    QStringList selectedNicknames() const;
    void selectNicknames(const QStringList &nicknames);

    // System map is a top-down projection: X/Z in Freelancer space -> X/Y in Qt.
    static QPointF flToQt(float x, float z);
    static QPointF qtToFl(qreal x, qreal y);

signals:
    void objectSelected(const QString &nickname);
    void selectionCleared();
    void selectionNicknamesChanged(const QStringList &nicknames);

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;

private:
    void addSolarObject(const std::shared_ptr<flatlas::domain::SolarObject> &obj);
    void addZone(const std::shared_ptr<flatlas::domain::ZoneItem> &zone);
    void addLightSource(const LightSourceVisual &lightSource);
    void clearTradeLaneSelectionOverlay();
    void updateTradeLaneSelectionOverlay(const QStringList &nicknames);

    flatlas::domain::SystemDocument *m_document = nullptr;
    bool m_gridVisible = true;
    bool m_moveEnabled = false;
    QVector<TradelaneItem *> m_tradeLaneSelectionOverlay;
    static constexpr double kScale = 0.01; // Freelancer coords → scene coords
};

} // namespace flatlas::rendering
