// rendering/flight/FlightHud.cpp – 2D-HUD-Overlay (Phase 15)

#include "FlightHud.h"
#include "FlightController.h"

#include <QPainter>
#include <QPaintEvent>
#include <QDateTime>
#include <QtMath>

namespace flatlas::rendering {

FlightHud::FlightHud(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
}

void FlightHud::setMessage(const QString &msg, int durationMs)
{
    m_message = msg;
    m_messageExpiry = QDateTime::currentMSecsSinceEpoch() + durationMs;
}

void FlightHud::tick()
{
    update(); // schedule repaint
}

void FlightHud::paintEvent(QPaintEvent * /*event*/)
{
    if (!m_controller)
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    drawSpeedBar(p);
    drawCompass(p);
    drawTargetInfo(p);
    drawMessage(p);
}

void FlightHud::drawSpeedBar(QPainter &p)
{
    // Speed bar on the left side
    const int barX = 30;
    const int barY = height() / 2 - 100;
    const int barW = 16;
    const int barH = 200;

    float maxSpd = m_controller->state() == FlightController::Cruise
        ? m_controller->cruiseSpeed() : m_controller->maxSpeed();
    float ratio = maxSpd > 0 ? qBound(0.0f, m_controller->speed() / maxSpd, 1.0f) : 0.0f;

    // Background
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 120));
    p.drawRect(barX, barY, barW, barH);

    // Fill
    int fillH = static_cast<int>(barH * ratio);
    QColor fillColor = m_controller->afterburnerActive() ? QColor(255, 120, 0) :
                        m_controller->state() == FlightController::Cruise ? QColor(0, 200, 255) :
                        QColor(0, 255, 100);
    p.setBrush(fillColor);
    p.drawRect(barX, barY + barH - fillH, barW, fillH);

    // Speed text
    p.setPen(QColor(200, 255, 200));
    p.setFont(QFont(QStringLiteral("Consolas"), 10));
    p.drawText(barX + barW + 6, barY + barH / 2 + 4,
               QStringLiteral("%1 m/s").arg(static_cast<int>(m_controller->speed())));

    // State label
    QString stateStr;
    switch (m_controller->state()) {
    case FlightController::Docked:    stateStr = QStringLiteral("DOCKED"); break;
    case FlightController::Normal:    stateStr = QStringLiteral("NORMAL"); break;
    case FlightController::Cruise:    stateStr = QStringLiteral("CRUISE"); break;
    case FlightController::Formation: stateStr = QStringLiteral("FORMATION"); break;
    }
    p.drawText(barX, barY - 8, stateStr);
}

void FlightHud::drawCompass(QPainter &p)
{
    // Compass at the top center
    const int cx = width() / 2;
    const int cy = 30;
    const int w = 200;

    p.setPen(QColor(0, 0, 0, 120));
    p.setBrush(QColor(0, 0, 0, 120));
    p.drawRect(cx - w / 2, cy - 12, w, 24);

    // Heading from forward vector (yaw angle)
    float heading = qRadiansToDegrees(qAtan2(-m_controller->forward().x(),
                                              -m_controller->forward().z()));
    if (heading < 0) heading += 360.0f;

    p.setPen(QColor(200, 255, 200));
    p.setFont(QFont(QStringLiteral("Consolas"), 10));
    p.drawText(cx - 20, cy + 5, QStringLiteral("%1°").arg(static_cast<int>(heading)));
}

void FlightHud::drawTargetInfo(QPainter &p)
{
    if (m_targetName.isEmpty())
        return;

    const int tx = width() - 220;
    const int ty = height() / 2 - 40;

    p.setPen(QColor(0, 0, 0, 120));
    p.setBrush(QColor(0, 0, 0, 120));
    p.drawRect(tx, ty, 200, 60);

    p.setPen(QColor(200, 255, 200));
    p.setFont(QFont(QStringLiteral("Consolas"), 9));
    p.drawText(tx + 8, ty + 18, m_targetName);

    float dist = (m_targetPos - m_controller->position()).length();
    QString distStr;
    if (dist > 1000.0f)
        distStr = QStringLiteral("%1 km").arg(dist / 1000.0f, 0, 'f', 1);
    else
        distStr = QStringLiteral("%1 m").arg(static_cast<int>(dist));
    p.drawText(tx + 8, ty + 38, QStringLiteral("Dist: %1").arg(distStr));
}

void FlightHud::drawMessage(QPainter &p)
{
    if (m_message.isEmpty()) return;
    if (QDateTime::currentMSecsSinceEpoch() > m_messageExpiry) {
        m_message.clear();
        return;
    }

    p.setPen(QColor(255, 255, 100));
    p.setFont(QFont(QStringLiteral("Consolas"), 12, QFont::Bold));
    p.drawText(rect(), Qt::AlignHCenter | Qt::AlignBottom, m_message);
}

} // namespace flatlas::rendering
