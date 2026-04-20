#include "SplashScreen.h"
#include <QVBoxLayout>
#include <QPixmap>

SplashScreen::SplashScreen(QWidget * /*parent*/)
    : SplashScreen(QPixmap(QStringLiteral(":/images/Splash-Screen.png")))
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
        "color: white; font-size: 12px; font-weight: 600; "
        "background-color: rgba(0, 0, 0, 110); padding: 4px 8px; border-radius: 4px;"));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setStyleSheet(QStringLiteral(
        "QProgressBar { background: rgba(15, 18, 24, 180); border: 1px solid rgba(255, 255, 255, 35); }"
        "QProgressBar::chunk { background: #4bb1ff; }"
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
