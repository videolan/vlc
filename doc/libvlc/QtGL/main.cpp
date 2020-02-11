#include <QApplication>
#include <QDesktopWidget>
#include <QSurfaceFormat>
#include <QMainWindow>

#include <QtPlugin>
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)


#include "qtvlcwidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // this important so we can call makeCurrent from our rendering thread
    QCoreApplication::setAttribute(Qt::AA_DontCheckOpenGLContextThreadAffinity);

    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(fmt);

    QMainWindow mainWindow;

    QtVLCWidget *glWidget = new QtVLCWidget;
    mainWindow.setCentralWidget(glWidget);

    mainWindow.resize(mainWindow.sizeHint());
    int desktopArea = QApplication::desktop()->width() *
                     QApplication::desktop()->height();
    int widgetArea = mainWindow.width() * mainWindow.height();
    if (((float)widgetArea / (float)desktopArea) < 0.75f)
        mainWindow.show();
    else
        mainWindow.showMaximized();

    glWidget->playMedia(argv[1]);

    return app.exec();
}
