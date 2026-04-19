#include "SplashScreen.h"
#include <QVBoxLayout>
#include <QPixmap>

SplashScreen::SplashScreen(QWidget * /*parent*/)
    : QSplashScreen()
{
    // Splash-Bild laden (Platzhalter: einfarbiger Hintergrund)
    QPixmap pixmap(480, 320);
    pixmap.fill(QColor(30, 30, 30));
    setPixmap(pixmap);

    // Fortschrittsbalken und Status-Label als Overlay
    auto *layout = new QVBoxLayout(this);
    layout->addStretch();

    m_statusLabel = new QLabel(tr("Starting..."), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: white; font-size: 12px;"));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setStyleSheet(QStringLiteral(
        "QProgressBar { background: #333; border: none; }"
        "QProgressBar::chunk { background: #2a82da; }"
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
