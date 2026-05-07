#include "SplashScreen.h"
#include <QVBoxLayout>
#include <QPixmap>

SplashScreen::SplashScreen(QWidget * /*parent*/)
    : SplashScreen(QPixmap(QStringLiteral(":/images/Splash-Screen-Blue.png")))
{
}

SplashScreen::SplashScreen(const QPixmap &pixmap)
    : QSplashScreen()
{
    QPixmap effectivePixmap = pixmap;
    if (!effectivePixmap.isNull() && effectivePixmap.width() > 800) {
        effectivePixmap = effectivePixmap.scaledToWidth(
            800, Qt::SmoothTransformation);
    }

    if (effectivePixmap.isNull()) {
        QPixmap fallback(480, 320);
        fallback.fill(QColor(30, 30, 30));
        effectivePixmap = fallback;
    }

    setPixmap(effectivePixmap);

    auto *layout = new QVBoxLayout(this);
    layout->addStretch();

    m_statusLabel = new QLabel(tr("Starting..."), this);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "color: #d8f3ff; font-size: 12px; font-weight: 600; letter-spacing: 0px; "
        "background-color: rgba(3, 12, 24, 165); padding: 5px 10px; "
        "border: 1px solid rgba(83, 196, 255, 120); border-radius: 4px;"));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(8);
    m_progressBar->setStyleSheet(QStringLiteral(
        "QProgressBar { background: rgba(2, 9, 18, 185);"
        " border: 1px solid rgba(96, 205, 255, 135); border-radius: 4px; }"
        "QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        " stop:0 #1d78ff, stop:0.45 #35d4ff, stop:1 #b7f3ff);"
        " border-radius: 3px; }"
    ));
    layout->addWidget(m_progressBar);
    layout->setContentsMargins(20, 20, 20, 20);
}

void SplashScreen::setProgress(int percent, const QString &message)
{
    m_progressBar->setValue(percent);
    m_statusLabel->setText(message);
    showMessage(message, Qt::AlignBottom | Qt::AlignHCenter, Qt::white);
}
