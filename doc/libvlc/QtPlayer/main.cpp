/******************************
 * Qt player using libVLC     *
 * By protonux                *
 *                            *
 * Under WTFPL                *
 ******************************/

#include <QApplication>
#include "player.h"

#ifdef Q_WS_X11
    #include <X11/Xlib.h>
#endif

int main(int argc, char *argv[]) {
#ifdef Q_WS_X11
    XInitThreads();
#endif

    QApplication app(argc, argv);

    Mwindow player;
    player.show();

    return app.exec();
}
