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

Mwindow::Mwindow() {
    vlcPlayer = NULL;

    /* Init libVLC */
    if((vlcObject = libvlc_new(0,NULL)) == NULL) {
        printf("Could not init libVLC");
        exit(1);
    }

    /* Display libVLC version */
    printf("libVLC version: %s\n",libvlc_get_version());

    /* Interface initialisation */
    initMenus();
    initComponents();
}

Mwindow::~Mwindow() {
    if(vlcObject)
        libvlc_release(vlcObject);
}

void Mwindow::initMenus() {

    centralWidget = new QWidget;

    videoWidget = new QWidget;
    videoWidget->setAutoFillBackground( true );
    QPalette plt = palette();
    plt.setColor( QPalette::Window, Qt::black );
    videoWidget->setPalette( plt );

    QMenu *fileMenu = menuBar()->addMenu("&File");
    QMenu *editMenu = menuBar()->addMenu("&Edit");

    QAction *Open = new QAction("&Open", this);
    QAction *Quit = new QAction("&Quit", this);
    QAction *playAc = new QAction("&Play/Pause", this);

    Open->setShortcut(QKeySequence("Ctrl+O"));
    Quit->setShortcut(QKeySequence("Ctrl+Q"));

    fileMenu->addAction(Open);
    fileMenu->addAction(Quit);
    editMenu->addAction(playAc);

    connect(Open, SIGNAL(triggered()), this, SLOT(openFile()));
    connect(playAc, SIGNAL(triggered()), this, SLOT(play()));
    connect(Quit, SIGNAL(triggered()), qApp, SLOT(quit()));
}

void Mwindow::initComponents() {

    playBut = new QPushButton("Play");
    QObject::connect(playBut, SIGNAL(clicked()), this, SLOT(play()));

    stopBut = new QPushButton("Stop");
    QObject::connect(stopBut, SIGNAL(clicked()), this, SLOT(stop()));

    muteBut = new QPushButton("Mute");
    QObject::connect(muteBut, SIGNAL(clicked()), this, SLOT(mute()));

    volumeSlider = new QSlider(Qt::Horizontal);
    QObject::connect(volumeSlider, SIGNAL(sliderMoved(int)), this, SLOT(changeVolume(int)));
    volumeSlider->setValue(80);

    slider = new QSlider(Qt::Horizontal);
    slider->setMaximum(1000);
    QObject::connect(slider, SIGNAL(sliderMoved(int)), this, SLOT(changePosition(int)));

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateInterface()));
    timer->start(100);

    QHBoxLayout *layout = new QHBoxLayout;
    layout->addWidget(playBut);
    layout->addWidget(stopBut);
    layout->addWidget(muteBut);
    layout->addWidget(volumeSlider);

    QVBoxLayout *layout2 = new QVBoxLayout;
    layout2->addWidget(videoWidget);
    layout2->addWidget(slider);
    layout2->addLayout(layout);

    centralWidget->setLayout(layout2);
    setCentralWidget(centralWidget);
    resize( 600, 400);
}

void Mwindow::openFile() {
    /* Just the basic file-select box */
    QString fileOpen = QFileDialog::getOpenFileName(this,tr("Load a file"), "~");

    /* Stop if something is playing */
    if( vlcPlayer && libvlc_media_player_is_playing(vlcPlayer) )
        stop();

    /* New Media */
    libvlc_media_t *vlcMedia = libvlc_media_new_path(vlcObject,qtu(fileOpen));
    if( !vlcMedia )
        return;

    vlcPlayer = libvlc_media_player_new_from_media (vlcMedia);
    libvlc_media_release(vlcMedia);

    /* Integrate the video in the interface */
#if defined(Q_OS_MAC)
    libvlc_media_player_set_nsobject(vlcPlayer, videoWidget->winId());
#elif defined(Q_OS_UNIX)
    libvlc_media_player_set_xwindow(vlcPlayer, videoWidget->winId());
#elif defined(Q_OS_WIN)
    libvlc_media_player_set_hwnd(vlcPlayer, videoWidget->winId());
#endif

    /* And play */
    libvlc_media_player_play (vlcPlayer);

    //Set vars and text correctly
    playBut->setText("Pause");
}

void Mwindow::play() {

    if(vlcPlayer)
    {
        if (libvlc_media_player_is_playing(vlcPlayer))
        {
            libvlc_media_player_pause(vlcPlayer);
            playBut->setText("Play");
        }
        else
        {
            libvlc_media_player_play(vlcPlayer);
            playBut->setText("Pause");
        }
    }
}

int Mwindow::changeVolume(int vol) { //Called if you change the volume slider

    if(vlcPlayer)
        return libvlc_audio_set_volume (vlcPlayer,vol);

    return 0;
}

void Mwindow::changePosition(int pos) { //Called if you change the position slider

    if(vlcPlayer) //It segfault if vlcPlayer don't exist
        libvlc_media_player_set_position(vlcPlayer,(float)pos/(float)1000);
}

void Mwindow::updateInterface() { //Update interface and check if song is finished

    if(vlcPlayer) //It segfault if vlcPlayer don't exist
    {
        /* update the timeline */
        float pos = libvlc_media_player_get_position(vlcPlayer);
        int siderPos=(int)(pos*(float)(1000));
        slider->setValue(siderPos);

        /* Stop the media */
        if (libvlc_media_player_get_state(vlcPlayer) == 6) { this->stop(); }
    }
}

void Mwindow::stop() {
    if(vlcPlayer) {
        libvlc_media_player_stop(vlcPlayer);
        libvlc_media_player_release(vlcPlayer);
        slider->setValue(0);
        playBut->setText("Play");
    }
    vlcPlayer = NULL;
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

void Mwindow::closeEvent(QCloseEvent *event) {
    stop();
    event->accept();
}
