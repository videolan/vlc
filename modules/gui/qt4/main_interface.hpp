/*****************************************************************************
 * main_interface.hpp : Main Interface
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
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
#include "components/preferences_widgets.hpp" /* First Start */
#ifdef WIN32
 #include <vlc_windows_interfaces.h>
#endif

#include <QSystemTrayIcon>

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
class QStackedWidget;

enum {
    CONTROLS_VISIBLE  = 0x1,
    CONTROLS_HIDDEN   = 0x2,
    CONTROLS_ADVANCED = 0x4,
};


class MainInterface : public QVLCMW
{
    Q_OBJECT;

    friend class PlaylistWidget;

public:
    MainInterface( intf_thread_t *);
    virtual ~MainInterface();

    /* Video requests from core */
    WId getVideo( int *pi_x, int *pi_y,
                  unsigned int *pi_width, unsigned int *pi_height );
    void releaseVideo( void  );
    int controlVideo( int i_query, va_list args );

    /* Getters */
#ifndef HAVE_MAEMO
    QSystemTrayIcon *getSysTray() { return sysTray; }
    QMenu *getSysTrayMenu() { return systrayMenu; }
#endif
    int getControlsVisibilityStatus();
    bool isPlDocked() { return ( b_plDocked != false ); }

    /* Sizehint() */
    virtual QSize sizeHint() const;

protected:
    void dropEventPlay( QDropEvent *, bool);
#ifdef WIN32
    virtual bool winEvent( MSG *, long * );
#endif
    virtual void dropEvent( QDropEvent *);
    virtual void dragEnterEvent( QDragEnterEvent * );
    virtual void dragMoveEvent( QDragMoveEvent * );
    virtual void dragLeaveEvent( QDragLeaveEvent * );
    virtual void closeEvent( QCloseEvent *);
    virtual void customEvent( QEvent *);
    virtual void keyPressEvent( QKeyEvent *);
    virtual void wheelEvent( QWheelEvent * );
    virtual void resizeEvent( QResizeEvent * event );

private:
    /* Main Widgets Creation */
    void createMainWidget( QSettings* );
    void createStatusBar();
    void createPlaylist();

    /* Systray */
    void createSystray();
    void handleSystray();
    void initSystray();

    /* Mess about stackWidget */
    void showTab( int i_tab );
    void restoreStackOldWidget();
    void showVideo() { showTab( VIDEO_TAB ); }
    void showBg() { showTab( BACKG_TAB ); }
    void hideStackWidget() { showTab( HIDDEN_TAB ); }

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
    QStackedWidget      *stackCentralW;

    /* Video */
    VideoWidget         *videoWidget;

    BackgroundWidget    *bgWidget;
    VisualSelector      *visualSelector;
    PlaylistWidget      *playlistWidget;

    /* Status Bar */
    QLabel              *nameLabel;
    QLabel              *cryptedLabel;

    /* Status and flags */
    enum {
        HIDDEN_TAB = -1,
        BACKG_TAB  =  0,
        VIDEO_TAB  = 1,
        PLAYL_TAB  = 2,
    };
    int                  stackCentralOldState;

    /* Flags */
    bool                 b_notificationEnabled; /// Systray Notifications
    bool                 b_keep_size;         ///< persistent resizeable window
    bool                 b_videoEmbedded;   ///< Want an external Video Window
    bool                 b_hideAfterCreation;
    int                  i_visualmode;        ///< Visual Mode

    /* States */
    bool                 playlistVisible;     ///< Is the playlist visible ?
//    bool                 videoIsActive;       ///< Having a video now / THEMIM->hasV
//    bool                 b_visualSelectorEnabled;
    bool                 b_plDocked;          ///< Is the playlist docked ?

    QSize                mainBasedSize;       ///< based Wnd (normal mode only)
    QSize                mainVideoSize;       ///< Wnd with video (all modes)
    int                  i_bg_height;         ///< Save height of bgWidget

#ifdef WIN32
    HIMAGELIST himl;
    LPTASKBARLIST3 p_taskbl;
    UINT taskbar_wmsg;
    void createTaskBarButtons();
#endif

public slots:
    void undockPlaylist();
    void dockPlaylist( bool b_docked = true );
    void toggleMinimalView( bool );
    void togglePlaylist();
#ifndef HAVE_MAEMO
    void toggleUpdateSystrayMenu();
#endif
    void toggleAdvanced();
    void toggleFullScreen();
    void toggleFSC();
    void popupMenu( const QPoint& );
    void changeThumbbarButtons( int );

    /* Manage the Video Functions from the vout threads */
    void getVideoSlot( WId *p_id, int *pi_x, int *pi_y,
                       unsigned *pi_width, unsigned *pi_height );
    void releaseVideoSlot( void );

private slots:
    void debug();
    void destroyPopupMenu();
    void recreateToolbars();
    void doComponentsUpdate();
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

signals:
    void askGetVideo( WId *p_id, int *pi_x, int *pi_y,
                      unsigned *pi_width, unsigned *pi_height );
    void askReleaseVideo( );
    void askVideoToResize( unsigned int, unsigned int );
    void askVideoSetFullScreen( bool );
    void askUpdate();
    void minimalViewToggled( bool );
    void fullscreenInterfaceToggled( bool );

};

#ifdef WIN32
#define WM_APPCOMMAND 0x0319

#define APPCOMMAND_VOLUME_MUTE            8
#define APPCOMMAND_VOLUME_DOWN            9
#define APPCOMMAND_VOLUME_UP              10
#define APPCOMMAND_MEDIA_NEXTTRACK        11
#define APPCOMMAND_MEDIA_PREVIOUSTRACK    12
#define APPCOMMAND_MEDIA_STOP             13
#define APPCOMMAND_MEDIA_PLAY_PAUSE       14
#define APPCOMMAND_LAUNCH_MEDIA_SELECT    16
#define APPCOMMAND_BASS_DOWN              19
#define APPCOMMAND_BASS_BOOST             20
#define APPCOMMAND_BASS_UP                21
#define APPCOMMAND_TREBLE_DOWN            22
#define APPCOMMAND_TREBLE_UP              23
#define APPCOMMAND_MICROPHONE_VOLUME_MUTE 24
#define APPCOMMAND_MICROPHONE_VOLUME_DOWN 25
#define APPCOMMAND_MICROPHONE_VOLUME_UP   26
#define APPCOMMAND_DICTATE_OR_COMMAND_CONTROL_TOGGLE    43
#define APPCOMMAND_MIC_ON_OFF_TOGGLE      44
#define APPCOMMAND_MEDIA_PLAY             46
#define APPCOMMAND_MEDIA_PAUSE            47
#define APPCOMMAND_MEDIA_RECORD           48
#define APPCOMMAND_MEDIA_FAST_FORWARD     49
#define APPCOMMAND_MEDIA_REWIND           50
#define APPCOMMAND_MEDIA_CHANNEL_UP       51
#define APPCOMMAND_MEDIA_CHANNEL_DOWN     52

#define FAPPCOMMAND_MOUSE 0x8000
#define FAPPCOMMAND_KEY   0
#define FAPPCOMMAND_OEM   0x1000
#define FAPPCOMMAND_MASK  0xF000

#define GET_APPCOMMAND_LPARAM(lParam) ((short)(HIWORD(lParam) & ~FAPPCOMMAND_MASK))
#define GET_DEVICE_LPARAM(lParam)     ((WORD)(HIWORD(lParam) & FAPPCOMMAND_MASK))
#define GET_MOUSEORKEY_LPARAM         GET_DEVICE_LPARAM
#define GET_FLAGS_LPARAM(lParam)      (LOWORD(lParam))
#define GET_KEYSTATE_LPARAM(lParam)   GET_FLAGS_LPARAM(lParam)
#endif
#endif
