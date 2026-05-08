#pragma once

#include <QSplashScreen>

class StartupSplashOverlay;

/// Splash-Screen mit Fortschrittsbalken und Statustext.
class SplashScreen : public QSplashScreen
{
    Q_OBJECT

public:
    explicit SplashScreen(QWidget *parent = nullptr);
    explicit SplashScreen(const QPixmap &pixmap);

    /// Fortschritt setzen (0-100) mit Statustext.
    void setProgress(int percent, const QString &message);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    StartupSplashOverlay *m_overlay = nullptr;
};
