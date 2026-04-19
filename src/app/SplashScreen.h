#pragma once

#include <QSplashScreen>
#include <QProgressBar>
#include <QLabel>

/// Splash-Screen mit Fortschrittsbalken und Statustext.
class SplashScreen : public QSplashScreen
{
    Q_OBJECT

public:
    explicit SplashScreen(QWidget *parent = nullptr);

    /// Fortschritt setzen (0-100) mit Statustext.
    void setProgress(int percent, const QString &message);

private:
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
};
