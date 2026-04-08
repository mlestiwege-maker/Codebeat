#include "runtime/mainwindow.hpp"
#include "runtime/splash.hpp"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    SplashScreen splash;
    splash.show();
    QApplication::processEvents();

    MainWindow window;
    QObject::connect(&splash, &SplashScreen::finished, [&]() {
        splash.close();
        window.show();
    });

    QObject::connect(&window, &MainWindow::lockRequested, [&]() {
        window.hide();
        splash.prepareForAuth();
        splash.show();
        QApplication::processEvents();
    });

    return app.exec();
}
