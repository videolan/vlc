/*****************************************************************************
 * main_interface.hpp : Main Interface
 ****************************************************************************
 * Copyright (C) 2006-2010 VideoLAN and AUTHORS
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef QVLC_MAIN_INTERFACE_H_
#define QVLC_MAIN_INTERFACE_H_

#include "qt4.hpp"

#include "util/qvlcframe.hpp"

#ifdef WIN32
 #include <vlc_windows_interfaces.h>
#endif

#include <QSystemTrayIcon>
#include <QStackedWidget>

class QSettings;
class QCloseEvent;
class QKeyEvent;
class QLabel;
class QEvent;
class InputManager;
class VideoWidget;
class BackgroundWidget;
class PlaylistWidget;
class VisualSelector;
class AdvControlsWidget;
class ControlsWidget;
class InputControlsWidget;
class FullscreenControllerWidget;
class SpeedControlWidget;
class QVBoxLayout;
class QMenu;
class QSize;

enum {
    CONTROLS_VISIBLE  = 0x1,
    CONTROLS_HIDDEN   = 0x2,
    CONTROLS_ADVANCED = 0x4,
};


class MainInterface : public QVLCMW
{
    Q_OBJECT

    friend class PlaylistWidget;

public:
    /* tors */
    MainInterface( intf_thread_t *);
    virtual ~MainInterface();

    /* Video requests from core */
    WId  getVideo( int *pi_x, int *pi_y,
                  unsigned int *pi_width, unsigned int *pi_height );
    void releaseVideo( void );
    int  controlVideo( int i_query, va_list args );

    /* Getters */
#ifndef HAVE_MAEMO
    QSystemTrayIcon *getSysTray() { return sysTray; }
    QMenu *getSysTrayMenu() { return systrayMenu; }
#endif
    int getControlsVisibilityStatus();
    bool isPlDocked() { return ( b_plDocked != false ); }
    bool isInterfaceFullScreen() { return b_interfaceFullScreen; }

protected:
    void dropEventPlay( QDropEvent *, bool);
#ifdef WIN32
    virtual bool winEvent( MSG *, long * );
#endif
    virtual void changeEvent( QEvent * );
    virtual void dropEvent( QDropEvent *);
    virtual void dragEnterEvent( QDragEnterEvent * );
    virtual void dragMoveEvent( QDragMoveEvent * );
    virtual void dragLeaveEvent( QDragLeaveEvent * );
    virtual void closeEvent( QCloseEvent *);
    virtual void keyPressEvent( QKeyEvent *);
    virtual void wheelEvent( QWheelEvent * );

private:
    /* Main Widgets Creation */
    void createMainWidget( QSettings* );
    void createStatusBar();
    void createPlaylist();

    /* Systray */
    void createSystray();
    void initSystray();
    void handleSystray();

    /* Central StackWidget Management */
    void showTab( QWidget *);
    void showVideo();
    void restoreStackOldWidget();

    /* */
    void setMinimalView( bool );
    void setInterfaceFullScreen( bool );
    void computeMinimumSize();

    /* */
    QSettings           *settings;
#ifndef HAVE_MAEMO
    QSystemTrayIcon     *sysTray;
    QMenu               *systrayMenu;
#endif

    QString              input_name;
    QVBoxLayout         *mainLayout;
    ControlsWidget      *controls;
    InputControlsWidget *inputC;
    FullscreenControllerWidget *fullscreenControls;

    /* Widgets */
    QStackedWidget      *stackCentralW;

    VideoWidget         *videoWidget;
    BackgroundWidget    *bgWidget;
    PlaylistWidget      *playlistWidget;
    //VisualSelector      *visualSelector;

    /* Status Bar */
    QLabel              *nameLabel;
    QLabel              *cryptedLabel;

    /* Status and flags */
    QWidget             *stackCentralOldWidget;

    QMap<QWidget *, QSize> stackWidgetsSizes;

    /* Flags */
    bool                 b_notificationEnabled; /// Systray Notifications
    bool                 b_autoresize;          ///< persistent resizable window
    bool                 b_videoEmbedded;       ///< Want an external Video Window
    bool                 b_videoFullScreen;     ///< --fullscreen
    bool                 b_hideAfterCreation;
    bool                 b_minimalView;         ///< Minimal video
    bool                 b_interfaceFullScreen;
    bool                 b_pauseOnMinimize;

    /* States */
    bool                 playlistVisible;       ///< Is the playlist visible ?
//    bool                 videoIsActive;       ///< Having a video now / THEMIM->hasV
//    bool                 b_visualSelectorEnabled;
    bool                 b_plDocked;            ///< Is the playlist docked ?

    bool                 b_hasPausedWhenMinimized;
    bool                 b_statusbarVisible;

#ifdef WIN32
    HIMAGELIST himl;
    LPTASKBARLIST3 p_taskbl;
    UINT taskbar_wmsg;
    void createTaskBarButtons();
#endif

public slots:
    void dockPlaylist( bool b_docked = true );
    void toggleMinimalView( bool );
    void togglePlaylist();
#ifndef HAVE_MAEMO
    void toggleUpdateSystrayMenu();
    void showUpdateSystrayMenu();
    void hideUpdateSystrayMenu();
#endif
    void toggleAdvancedButtons();
    void toggleInterfaceFullScreen();
    void toggleFSC();

    void setStatusBarVisibility(bool b_visible);

    void popupMenu( const QPoint& );
#ifdef WIN32
    void changeThumbbarButtons( int );
#endif

    /* Manage the Video Functions from the vout threads */
    void getVideoSlot( WId *p_id, int *pi_x, int *pi_y,
                       unsigned *pi_width, unsigned *pi_height );
    void releaseVideoSlot( void );

    void emitBoss();

private slots:
    void debug();
    void destroyPopupMenu();
    void recreateToolbars();
    void setName( const QString& );
    void setVLCWindowsTitle( const QString& title = "" );
#if 0
    void visual();
#endif
#ifndef HAVE_MAEMO
    void handleSystrayClick( QSystemTrayIcon::ActivationReason );
    void updateSystrayTooltipName( const QString& );
    void updateSystrayTooltipStatus( int );
#endif
    void showCryptedLabel( bool );

    void handleKeyPress( QKeyEvent * );

    void showBuffering( float );

    void resizeStack( int w, int h )
    {
        if( !isFullScreen() && !isMaximized() )
        {
            if( b_minimalView )
                resize( w, h ); /* Oh yes, it shouldn't
                                   be possible that size() - stackCentralW->size() < 0
                                   since stackCentralW is contained in the QMW... */
            else
                resize( size() - stackCentralW->size() + QSize( w, h ) );
        }
        debug();
    }

    void setVideoSize( unsigned int, unsigned int );
    void setVideoFullScreen( bool );
    void setVideoOnTop( bool );
    void setBoss();

signals:
    void askGetVideo( WId *p_id, int *pi_x, int *pi_y,
                      unsigned *pi_width, unsigned *pi_height );
    void askReleaseVideo( );
    void askVideoToResize( unsigned int, unsigned int );
    void askVideoSetFullScreen( bool );
    void askVideoOnTop( bool );
    void minimalViewToggled( bool );
    void fullscreenInterfaceToggled( bool );
    void askToQuit();
    void askBoss();

};

#endif
