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
class QMenu;
class QSize;
class DialogHandler;

enum {
    CONTROLS_VISIBLE = 0x1,
    CONTROLS_HIDDEN = 0x2,
    CONTROLS_ADVANCED = 0x4,
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

    friend class PlaylistWidget;

public:
    MainInterface( intf_thread_t *);
    virtual ~MainInterface();

    /* Video requests from core */
    WId requestVideo( vout_thread_t *p_nvout, int *pi_x,
                      int *pi_y, unsigned int *pi_width,
                      unsigned int *pi_height );
    void releaseVideo( void  );
    int controlVideo( int i_query, va_list args );

    /* Getters */
    QSystemTrayIcon *getSysTray() { return sysTray; };
    QMenu *getSysTrayMenu() { return systrayMenu; };
    int getControlsVisibilityStatus();

    /* Sizehint() */
    virtual QSize sizeHint() const;

protected:
    void dropEventPlay( QDropEvent *, bool);
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
    QSettings           *settings;
    QSystemTrayIcon     *sysTray;
    QMenu               *systrayMenu;
    QString              input_name;
    QVBoxLayout         *mainLayout;
    ControlsWidget      *controls;
    InputControlsWidget *inputC;
    FullscreenControllerWidget *fullscreenControls;
    DialogHandler       *dialogHandler;

    void createMainWidget( QSettings* );
    void createStatusBar();

    void askForPrivacy();
    int  privacyDialog( QList<ConfigControl *> *controls );

    /* Systray */
    void handleSystray();
    void createSystray();
    void initSystray();


    /* Video */
    VideoWidget         *videoWidget;

    BackgroundWidget    *bgWidget;
    VisualSelector      *visualSelector;
    PlaylistWidget      *playlistWidget;

    bool                 videoIsActive;       ///< Having a video now / THEMIM->hasV
    bool                 videoEmbeddedFlag;   ///< Want an external Video Window
    bool                 playlistVisible;     ///< Is the playlist visible ?
    bool                 visualSelectorEnabled;
    bool                 notificationEnabled; /// Systray Notifications
    bool                 bgWasVisible;
    bool                 b_keep_size;         ///< persistent resizeable window
    QSize                mainBasedSize;       ///< based Wnd (normal mode only)
    QSize                mainVideoSize;       ///< Wnd with video (all modes)
    int                  i_visualmode;        ///< Visual Mode
    pl_dock_e            i_pl_dock;
    bool                 isDocked() { return ( i_pl_dock != PL_UNDOCKED ); }
    int                  i_bg_height;         ///< Save height of bgWidget
    bool                 b_shouldHide;

    /* Status Bar */
    QLabel              *nameLabel;
    QLabel              *cryptedLabel;

public slots:
    void undockPlaylist();
    void dockPlaylist( pl_dock_e i_pos = PL_BOTTOM );
    void toggleMinimalView( bool );
    void togglePlaylist();
    void toggleUpdateSystrayMenu();
    void toggleAdvanced();
    void toggleFullScreen();
    void toggleFSC();
    void popupMenu( const QPoint& );

    /* Manage the Video Functions from the vout threads */
    void releaseVideoSlot( void );

private slots:
    void debug();
    void destroyPopupMenu();
    void recreateToolbars();
    void doComponentsUpdate();
    void setName( QString );
    void setVLCWindowsTitle( QString title = "" );
#if 0
    void visual();
#endif
    void handleSystrayClick( QSystemTrayIcon::ActivationReason );
    void updateSystrayTooltipName( QString );
    void updateSystrayTooltipStatus( int );

    void showCryptedLabel( bool );
signals:
    void askReleaseVideo( );
    void askVideoToResize( unsigned int, unsigned int );
    void askVideoToShow( unsigned int, unsigned int );
    void askBgWidgetToToggle();
    void askUpdate();
    void minimalViewToggled( bool );
    void fullscreenInterfaceToggled( bool );
};

#endif
