/******************************
 * Qt player using libVLC     *
 * By protonux                *
 *                            *
 * Under WTFPL                *
 ******************************/

#include "player.h"
#include <vlc/vlc.h>

#define qtu( i ) ((i).toUtf8().constData())

#include <QtGui>
#include <QMessageBox>
#include <QMenuBar>
#include <QAction>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>

Mwindow::Mwindow() {
    vlcPlayer = NULL;

    /* Initialize libVLC */
    vlcInstance = libvlc_new(0, NULL);

    /* Complain in case of broken installation */
    if (vlcInstance == NULL) {
        QMessageBox::critical(this, "Qt libVLC player", "Could not init libVLC");
        exit(1);
    }

    /* Interface initialization */
    initUI();
}

Mwindow::~Mwindow() {
    /* Release libVLC instance on quit */
    if (vlcInstance)
        libvlc_release(vlcInstance);
}

void Mwindow::initUI() {

    /* Menu */
    QMenu *fileMenu = menuBar()->addMenu("&File");
    QMenu *editMenu = menuBar()->addMenu("&Edit");

    QAction *Open    = new QAction("&Open", this);
    QAction *Quit    = new QAction("&Quit", this);
    QAction *playAc  = new QAction("&Play/Pause", this);
    QAction *fsAc  = new QAction("&Fullscreen", this);
    QAction *aboutAc = new QAction("&About", this);

    Open->setShortcut(QKeySequence("Ctrl+O"));
    Quit->setShortcut(QKeySequence("Ctrl+Q"));

    fileMenu->addAction(Open);
    fileMenu->addAction(aboutAc);
    fileMenu->addAction(Quit);
    editMenu->addAction(playAc);
    editMenu->addAction(fsAc);

    connect(Open,    SIGNAL(triggered()), this, SLOT(openFile()));
    connect(playAc,  SIGNAL(triggered()), this, SLOT(play()));
    connect(aboutAc, SIGNAL(triggered()), this, SLOT(about()));
    connect(fsAc,    SIGNAL(triggered()), this, SLOT(fullscreen()));
    connect(Quit,    SIGNAL(triggered()), qApp, SLOT(quit()));

    /* Buttons for the UI */
    playBut = new QPushButton("Play");
    QObject::connect(playBut, SIGNAL(clicked()), this, SLOT(play()));

    QPushButton *stopBut = new QPushButton("Stop");
    QObject::connect(stopBut, SIGNAL(clicked()), this, SLOT(stop()));

    QPushButton *muteBut = new QPushButton("Mute");
    QObject::connect(muteBut, SIGNAL(clicked()), this, SLOT(mute()));

    QPushButton *fsBut = new QPushButton("Fullscreen");
    QObject::connect(fsBut, SIGNAL(clicked()), this, SLOT(fullscreen()));

    volumeSlider = new QSlider(Qt::Horizontal);
    QObject::connect(volumeSlider, SIGNAL(sliderMoved(int)), this, SLOT(changeVolume(int)));
    volumeSlider->setValue(80);

    slider = new QSlider(Qt::Horizontal);
    slider->setMaximum(1000);
    QObject::connect(slider, SIGNAL(sliderMoved(int)), this, SLOT(changePosition(int)));

    /* A timer to update the sliders */
    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateInterface()));
    timer->start(100);

    /* Central Widgets */
    QWidget* centralWidget = new QWidget;
    videoWidget = new QWidget;

    videoWidget->setAutoFillBackground( true );
    QPalette plt = palette();
    plt.setColor( QPalette::Window, Qt::black );
    videoWidget->setPalette( plt );

    /* Put all in layouts */
    QHBoxLayout *layout = new QHBoxLayout;
    layout->setMargin(0);
    layout->addWidget(playBut);
    layout->addWidget(stopBut);
    layout->addWidget(muteBut);
    layout->addWidget(fsBut);
    layout->addWidget(volumeSlider);

    QVBoxLayout *layout2 = new QVBoxLayout;
    layout2->setMargin(0);
    layout2->addWidget(videoWidget);
    layout2->addWidget(slider);
    layout2->addLayout(layout);

    centralWidget->setLayout(layout2);
    setCentralWidget(centralWidget);
    resize( 600, 400);
}

void Mwindow::openFile() {

    /* The basic file-select box */
    QString fileOpen = QFileDialog::getOpenFileName(this, tr("Load a file"), "~");

    /* Stop if something is playing */
    if (vlcPlayer && libvlc_media_player_is_playing(vlcPlayer))
        stop();

    /* Create a new Media */
    libvlc_media_t *vlcMedia = libvlc_media_new_path(vlcInstance, qtu(fileOpen));
    if (!vlcMedia)
        return;

    /* Create a new libvlc player */
    vlcPlayer = libvlc_media_player_new_from_media (vlcMedia);

    /* Release the media */
    libvlc_media_release(vlcMedia);

    /* Integrate the video in the interface */
#if defined(Q_OS_MAC)
    libvlc_media_player_set_nsobject(vlcPlayer, (void *)videoWidget->winId());
#elif defined(Q_OS_UNIX)
    libvlc_media_player_set_xwindow(vlcPlayer, videoWidget->winId());
#elif defined(Q_OS_WIN)
    libvlc_media_player_set_hwnd(vlcPlayer, (HWND)videoWidget->winId());
#endif

    /* And start playback */
    libvlc_media_player_play (vlcPlayer);

    /* Update playback button */
    playBut->setText("Pause");
}

void Mwindow::play() {
    if (!vlcPlayer)
        return;

    if (libvlc_media_player_is_playing(vlcPlayer))
    {
        /* Pause */
        libvlc_media_player_pause(vlcPlayer);
        playBut->setText("Play");
    }
    else
    {
        /* Play again */
        libvlc_media_player_play(vlcPlayer);
        playBut->setText("Pause");
    }
}

int Mwindow::changeVolume(int vol) { /* Called on volume slider change */

    if (vlcPlayer)
        return libvlc_audio_set_volume (vlcPlayer,vol);

    return 0;
}

void Mwindow::changePosition(int pos) { /* Called on position slider change */

    if (vlcPlayer)
        libvlc_media_player_set_position(vlcPlayer, (float)pos/1000.0, true);
}

void Mwindow::updateInterface() { //Update interface and check if song is finished

    if (!vlcPlayer)
        return;

    /* update the timeline */
    float pos = libvlc_media_player_get_position(vlcPlayer);
    slider->setValue((int)(pos*1000.0));

    /* Stop the media */
    if (libvlc_media_player_get_state(vlcPlayer) == libvlc_Ended)
        this->stop();
}

void Mwindow::stop() {
    if(vlcPlayer) {
        /* stop the media player */
        libvlc_media_player_stop_async(vlcPlayer);

        /* release the media player */
        libvlc_media_player_release(vlcPlayer);

        /* Reset application values */
        vlcPlayer = NULL;
        slider->setValue(0);
        playBut->setText("Play");
    }
}

void Mwindow::mute() {
    if(vlcPlayer) {
        if(volumeSlider->value() == 0) { //if already muted...

                this->changeVolume(80);
                volumeSlider->setValue(80);

        } else { //else mute volume

                this->changeVolume(0);
                volumeSlider->setValue(0);

        }
    }
}

void Mwindow::about()
{
    QMessageBox::about(this, "Qt libVLC player demo", QString::fromUtf8(libvlc_get_version()) );
}

void Mwindow::fullscreen()
{
   if (isFullScreen()) {
       showNormal();
       menuWidget()->show();
   }
   else {
       showFullScreen();
       menuWidget()->hide();
   }
}

void Mwindow::closeEvent(QCloseEvent *event) {
    stop();
    event->accept();
}
