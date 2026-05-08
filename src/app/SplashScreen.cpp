#include "SplashScreen.h"

#include <QApplication>
#include <QFontDatabase>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

class StartupSplashOverlay : public QWidget
{
public:
    explicit StartupSplashOverlay(QWidget *parent)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_TranslucentBackground, true);
    }

    void setProgress(int percent, const QString &message)
    {
        m_percent = qBound(0, percent, 100);
        if (!message.isEmpty())
            m_message = message;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        paintStatusText(&painter);
        paintProgressBar(&painter);
    }

private:
    static QFont statusFont()
    {
        const QStringList families = QFontDatabase::families();
        for (const QString &name : {QStringLiteral("Bahnschrift"),
                                    QStringLiteral("Orbitron"),
                                    QStringLiteral("Eurostile"),
                                    QStringLiteral("BankGothic Md BT"),
                                    QStringLiteral("Segoe UI"),
                                    QStringLiteral("Arial")}) {
            for (const QString &family : families) {
                if (family.compare(name, Qt::CaseInsensitive) == 0)
                    return QFont(family);
            }
        }
        return QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    }

    void paintStatusText(QPainter *painter)
    {
        const int width = qMax(1, this->width());
        const int height = qMax(1, this->height());
        QFont font = statusFont();
        font.setPointSize(qMax(8, static_cast<int>(height * 0.035)));
        font.setWeight(QFont::DemiBold);
        painter->setFont(font);

        const QRect rect = this->rect().adjusted(static_cast<int>(width * 0.18),
                                                 static_cast<int>(height * 0.682),
                                                 -static_cast<int>(width * 0.18),
                                                 -static_cast<int>(height * 0.275));
        const QString text = m_message.trimmed();
        painter->setPen(QColor(0, 10, 20, 180));
        painter->drawText(rect.adjusted(0, 1, 0, 1), Qt::AlignHCenter | Qt::AlignVCenter, text);
        painter->setPen(QColor(224, 246, 255, 235));
        painter->drawText(rect, Qt::AlignHCenter | Qt::AlignVCenter, text);
    }

    void paintProgressBar(QPainter *painter)
    {
        const double width = qMax(1, this->width());
        const double height = qMax(1, this->height());
        const double xPos = width * 0.175;
        const double yPos = height * 0.862;
        const double barWidth = width * 0.660;
        const double barHeight = qMax(10.0, height * 0.038);
        const double bevel = barHeight * 1.15;
        const double innerMargin = qMax(2.0, barHeight * 0.20);

        QPainterPath outer;
        outer.moveTo(xPos + bevel, yPos);
        outer.lineTo(xPos + barWidth - bevel, yPos);
        outer.lineTo(xPos + barWidth, yPos + barHeight * 0.5);
        outer.lineTo(xPos + barWidth - bevel, yPos + barHeight);
        outer.lineTo(xPos + bevel, yPos + barHeight);
        outer.lineTo(xPos, yPos + barHeight * 0.5);
        outer.closeSubpath();

        const QColor glow(24, 190, 255, 70);
        painter->setBrush(Qt::NoBrush);
        for (double grow : {5.0, 3.0}) {
            QPen pen(glow, grow);
            pen.setJoinStyle(Qt::RoundJoin);
            painter->setPen(pen);
            painter->drawPath(outer);
        }

        painter->setPen(QPen(QColor(57, 214, 255, 210), qMax(1.0, barHeight * 0.10)));
        painter->setBrush(QColor(4, 21, 38, 215));
        painter->drawPath(outer);

        const double innerX = xPos + bevel * 0.63;
        const double innerY = yPos + innerMargin;
        const double innerW = barWidth - bevel * 1.26;
        const double innerH = barHeight - innerMargin * 2.0;
        const double fillW = qMax(0.0, innerW * (static_cast<double>(m_percent) / 100.0));
        if (fillW <= 0.5)
            return;

        constexpr int segmentCount = 36;
        const double gap = qMax(1.0, width * 0.002);
        const double segmentW = (innerW - (gap * (segmentCount - 1))) / segmentCount;
        QLinearGradient gradient(innerX, innerY, innerX + qMax(1.0, fillW), innerY);
        gradient.setColorAt(0.0, QColor(33, 201, 236));
        gradient.setColorAt(0.62, QColor(78, 234, 255));
        gradient.setColorAt(1.0, QColor(184, 255, 255));
        painter->setPen(QPen(QColor(5, 93, 135, 180), 0.7));
        painter->setBrush(gradient);

        double remaining = fillW;
        double currentX = innerX;
        for (int index = 0; index < segmentCount; ++index) {
            Q_UNUSED(index);
            const double drawW = qMin(segmentW, remaining);
            if (drawW <= 0)
                break;
            painter->drawRoundedRect(QRectF(currentX, innerY, drawW, innerH), innerH * 0.18, innerH * 0.18);
            remaining -= segmentW + gap;
            currentX += segmentW + gap;
        }

        const double capX = innerX + fillW;
        painter->setPen(QPen(QColor(165, 248, 255, 220), qMax(1.0, innerH * 0.22)));
        painter->drawLine(QPointF(capX, innerY - 1), QPointF(capX, innerY + innerH + 1));
    }

    int m_percent = 0;
    QString m_message = QStringLiteral("Starting FL Atlas...");
};

SplashScreen::SplashScreen(QWidget * /*parent*/)
    : SplashScreen(QPixmap(QStringLiteral(":/images/Splash-Screen-Blue.png")))
{
}

SplashScreen::SplashScreen(const QPixmap &pixmap)
    : QSplashScreen(pixmap.scaled(500,
                                  1400,
                                  Qt::KeepAspectRatio,
                                  Qt::SmoothTransformation),
                    Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::SplashScreen)
    , m_overlay(new StartupSplashOverlay(this))
{
    m_overlay->setGeometry(rect());
    m_overlay->show();
}

void SplashScreen::resizeEvent(QResizeEvent *event)
{
    QSplashScreen::resizeEvent(event);
    if (m_overlay)
        m_overlay->setGeometry(rect());
}

void SplashScreen::setProgress(int percent, const QString &message)
{
    if (m_overlay)
        m_overlay->setProgress(percent, message);
    QApplication::processEvents();
}
