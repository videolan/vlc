/*****************************************************************************
 * main_interface.hpp : Main Interface
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
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

#ifndef _MAIN_INTERFACE_H_
#define _MAIN_INTERFACE_H_

#include "qt4.hpp"
#include "util/qvlcframe.hpp"
#include "components/preferences_widgets.hpp"

#include <vlc_aout.h>

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
class FullscreenControllerWidget;
class SpeedControlWidget;
class QMenu;
class QSize;
//class QDockWidet;

enum {
    CONTROLS_HIDDEN = 0x0,
    CONTROLS_VISIBLE = 0x1,
    CONTROLS_ADVANCED = 0x2
};

typedef enum pl_dock_e {
    PL_UNDOCKED,
    PL_BOTTOM,
    PL_RIGHT,
    PL_LEFT
} pl_dock_e;

class MainInterface : public QVLCMW
{
    Q_OBJECT;

    friend class VolumeClickHandler;
    friend class InteractionDialog;
    friend class PlaylistWidget;

public:
    MainInterface( intf_thread_t *);
    virtual ~MainInterface();

    /* Video requests from core */
    void *requestVideo( vout_thread_t *p_nvout, int *pi_x,
                        int *pi_y, unsigned int *pi_width,
                        unsigned int *pi_height );
    void releaseVideo( void * );
    int controlVideo( void *p_window, int i_query, va_list args );

    /* Getters */
    QSystemTrayIcon *getSysTray() { return sysTray; };
    QMenu *getSysTrayMenu() { return systrayMenu; };
    int getControlsVisibilityStatus();

    /* Sizehint() */
    QSize sizeHint() const;
protected:
//    void resizeEvent( QResizeEvent * );
    void dropEvent( QDropEvent *);
    void dragEnterEvent( QDragEnterEvent * );
    void dragMoveEvent( QDragMoveEvent * );
    void dragLeaveEvent( QDragLeaveEvent * );
    void closeEvent( QCloseEvent *);

private:
    QSettings           *settings;
    QSystemTrayIcon     *sysTray;
    QMenu               *systrayMenu;
    QString              input_name;
    QVBoxLayout         *mainLayout;
    ControlsWidget      *controls;
    FullscreenControllerWidget *fullscreenControls;
    QMenu               *speedControlMenu;
    SpeedControlWidget  *speedControl;

    void handleMainUi( QSettings* );
    void askForPrivacy();
    int  privacyDialog( QList<ConfigControl *> *controls );

    /* Systray */
    void handleSystray();
    void createSystray();

    void createStatusBar();
    void initSystray();

    /* Video */
    VideoWidget         *videoWidget;

    BackgroundWidget    *bgWidget;
    VisualSelector      *visualSelector;
    PlaylistWidget      *playlistWidget;

    bool                 videoIsActive; ///< Having a video now / THEMIM->hasV
    bool                 videoEmbeddedFlag; ///< Want an external Video Window
    bool                 playlistVisible; ///< Is the playlist visible ?
    bool                 visualSelectorEnabled;
    bool                 notificationEnabled; /// Systray Notifications
    bool                 b_remainingTime; /* Show elapsed or remaining time */
    bool                 bgWasVisible;
    int                  i_visualmode; ///< Visual Mode
    pl_dock_e            i_pl_dock;
    bool                 isDocked() { return ( i_pl_dock != PL_UNDOCKED ); }

    input_thread_t      *p_input;    ///< Main input associated to the playlist

    /* Status Bar */
    QLabel              *timeLabel;
    QLabel              *speedLabel;
    QLabel              *nameLabel;

    virtual void customEvent( QEvent *);
    virtual void keyPressEvent( QKeyEvent *);
    virtual void wheelEvent( QWheelEvent * );

public slots:
    void undockPlaylist();
    void dockPlaylist( pl_dock_e i_pos = PL_BOTTOM );
    void toggleMinimalView();
    void togglePlaylist();
    void toggleUpdateSystrayMenu();
    void toggleAdvanced();
    void toggleFullScreen();
    void toggleFSC();

    /* Manage the Video Functions from the vout threads */
    void releaseVideoSlot( void * );

private slots:
    void debug();
    void updateOnTimer();
    void doComponentsUpdate();
    void setStatus( int );
    void setRate( int );
    void setName( QString );
    void setVLCWindowsTitle( QString title = "" );
    void setDisplayPosition( float, int, int );
    void toggleTimeDisplay();
#if 0
    void visual();
#endif
    void handleSystrayClick( QSystemTrayIcon::ActivationReason );
    void updateSystrayTooltipName( QString );
    void updateSystrayTooltipStatus( int );
    void showSpeedMenu( QPoint );
signals:
    void askReleaseVideo( void * );
    void askVideoToResize( unsigned int, unsigned int );
    void askVideoToToggle();
    void askBgWidgetToToggle();
    void askUpdate();
};

#endif
