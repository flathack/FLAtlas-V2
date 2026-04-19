// FLAtlas V2 – Entry Point
// Startet die Qt-Anwendung mit Splash-Screen und Hauptfenster.

#include "app/Application.h"
#include "app/SplashScreen.h"
#include "ui/MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    // Qt-Attribute vor QApplication setzen
#ifdef Q_OS_WIN
    // OpenGL als Fallback für Qt3D auf Windows
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#endif

    Application app(argc, argv);

    // Splash-Screen anzeigen
    SplashScreen splash;
    splash.show();
    app.processEvents();

    splash.setProgress(6, QObject::tr("Initializing runtime..."));
    app.processEvents();

    // Hauptfenster erstellen
    splash.setProgress(20, QObject::tr("Building interface..."));
    MainWindow mainWindow;

    // Theme anwenden
    splash.setProgress(45, QObject::tr("Applying theme..."));
    app.applyTheme();

    // Fenster anzeigen
    splash.setProgress(97, QObject::tr("Almost ready..."));
    mainWindow.show();

    splash.setProgress(100, QObject::tr("Ready"));
    splash.finish(&mainWindow);

    return app.exec();
}
