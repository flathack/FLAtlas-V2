#pragma once
// rendering/flight/FlightHud.h – 2D-HUD-Overlay (Phase 15)

#include <QWidget>
#include <QVector3D>

namespace flatlas::rendering {

class FlightController;

/// 2D overlay painted on top of the 3D view: speed, target, compass.
class FlightHud : public QWidget {
    Q_OBJECT
public:
    explicit FlightHud(QWidget *parent = nullptr);

    void setController(FlightController *ctrl) { m_controller = ctrl; }
    void setTargetName(const QString &name) { m_targetName = name; }
    void setTargetPosition(const QVector3D &pos) { m_targetPos = pos; }
    void setMessage(const QString &msg, int durationMs = 3000);

    /// Call every frame to repaint.
    void tick();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void drawSpeedBar(QPainter &p);
    void drawCompass(QPainter &p);
    void drawTargetInfo(QPainter &p);
    void drawMessage(QPainter &p);

    FlightController *m_controller = nullptr;
    QString m_targetName;
    QVector3D m_targetPos;
    QString m_message;
    qint64 m_messageExpiry = 0;
};

} // namespace flatlas::rendering
