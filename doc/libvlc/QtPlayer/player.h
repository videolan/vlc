/******************************
 * Qt player using libVLC     *
 * By protonux                *
 *                            *
 * Under WTFPL                *
 ******************************/

#ifndef PLAYER
#define PLAYER

#include <QtGui>
#include <vlc/vlc.h>

class Mwindow : public QMainWindow {

    Q_OBJECT

        public:
               Mwindow();
               virtual ~Mwindow();

        private slots:
               void openFile();
               void play();
               void stop();
               void mute();

               int changeVolume(int);
               void changePosition(int);
               void updateInterface();

        protected:
               virtual void closeEvent(QCloseEvent*);

        private:
               QString current;
               QPushButton *playBut;
               QPushButton *stopBut;
               QPushButton *muteBut;
               QSlider *volumeSlider;
               QSlider *slider;
               QWidget *videoWidget;
               QWidget *centralWidget;
               libvlc_instance_t *vlcObject;
               libvlc_media_player_t *vlcPlayer;

               void initMenus();
               void initComponents();
};


#endif
