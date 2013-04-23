/*****************************************************************************
 * controller.hpp : Controller for the main interface
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
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

#ifndef QVLC_CONTROLLER_H_
#define QVLC_CONTROLLER_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt4.hpp"

#include <QFrame>
#include <QString>
#include <QSizeGrip>

#define MAIN_TB1_DEFAULT "64;39;64;38;65"
#define MAIN_TB2_DEFAULT "0-2;64;3;1;4;64;7;9;64;10;20;19;64-4;37;65;35-4"
#define ADV_TB_DEFAULT "12;11;13;14"
#define INPT_TB_DEFAULT "43;33-4;44"
#define FSC_TB_DEFAULT "0-2;64;3;1;4;64;37;64;38;64;8;65;25;35-4;34"

#define I_PLAY_TOOLTIP N_("Play\nIf the playlist is empty, open a medium")

class QPixmap;
class QLabel;

class QGridLayout;
class QBoxLayout;
class QHBoxLayout;
class QVBoxLayout;

class QAbstractSlider;
class QAbstractButton;
class SeekSlider;
class QToolButton;

class VolumeClickHandler;
class WidgetListing;

class QSignalMapper;
class QTimer;

typedef enum buttonType_e
{
    PLAY_BUTTON,
    STOP_BUTTON,
    OPEN_BUTTON,
    PREV_SLOW_BUTTON,
    NEXT_FAST_BUTTON,
    SLOWER_BUTTON,
    FASTER_BUTTON,
    FULLSCREEN_BUTTON,
    DEFULLSCREEN_BUTTON,
    EXTENDED_BUTTON,
    PLAYLIST_BUTTON,
    SNAPSHOT_BUTTON,
    RECORD_BUTTON,
    ATOB_BUTTON,
    FRAME_BUTTON,
    REVERSE_BUTTON,
    SKIP_BACK_BUTTON,
    SKIP_FW_BUTTON,
    QUIT_BUTTON,
    RANDOM_BUTTON,
    LOOP_BUTTON,
    INFO_BUTTON,
    PREVIOUS_BUTTON,
    NEXT_BUTTON,
    OPEN_SUB_BUTTON,
    FULLWIDTH_BUTTON,
    BUTTON_MAX,

    SPLITTER = 0x20,
    INPUT_SLIDER,
    TIME_LABEL,
    VOLUME,
    VOLUME_SPECIAL,
    MENU_BUTTONS,
    TELETEXT_BUTTONS,
    ADVANCED_CONTROLLER,
    PLAYBACK_BUTTONS,
    ASPECT_RATIO_COMBOBOX,
    SPEED_LABEL,
    TIME_LABEL_ELAPSED,
    TIME_LABEL_REMAINING,
    SPECIAL_MAX,

    WIDGET_SPACER = 0x40,
    WIDGET_SPACER_EXTEND,
    WIDGET_MAX,
} buttonType_e;


static const char* const nameL[BUTTON_MAX] = { N_("Play"), N_("Stop"), N_("Open"),
    N_("Previous / Backward"), N_("Next / Forward"), N_("Slower"), N_("Faster"), N_("Fullscreen"),
    N_("De-Fullscreen"), N_("Extended panel"), N_("Playlist"), N_("Snapshot"),
    N_("Record"), N_("A->B Loop"), N_("Frame By Frame"), N_("Trickplay Reverse"),
    N_("Step backward" ), N_("Step forward"), N_("Quit"), N_("Random"),
    N_("Loop / Repeat"), N_("Information"), N_("Previous"), N_("Next"),
    N_("Open subtitles"), N_("Dock fullscreen controller")
};
static const char* const tooltipL[BUTTON_MAX] = { I_PLAY_TOOLTIP,
    N_("Stop playback"), N_("Open a medium"),
    N_("Previous media in the playlist, skip backward when held"),
    N_("Next media in the playlist, skip forward when held"), N_("Slower"), N_("Faster"),
    N_("Toggle the video in fullscreen"), N_("Toggle the video out fullscreen"),
    N_("Show extended settings" ), N_( "Toggle playlist" ),
    N_( "Take a snapshot" ), N_( "Record" ),
    N_( "Loop from point A to point B continuously." ), N_("Frame by frame"),
    N_("Reverse"), N_("Step backward"), N_("Step forward"), N_("Quit"),
    N_("Random"), N_("Change the loop and repeat modes"), N_("Information"),
    N_("Previous media in the playlist"), N_("Next media in the playlist"),
    N_("Open subtitle file"),
    N_("Dock/undock fullscreen controller to/from bottom of screen")
};
static const QString iconL[BUTTON_MAX] ={ ":/toolbar/play_b", ":/toolbar/stop_b",
    ":/toolbar/eject", ":/toolbar/previous_b", ":/toolbar/next_b",
    ":/toolbar/slower", ":/toolbar/faster", ":/toolbar/fullscreen",
    ":/toolbar/defullscreen", ":/toolbar/extended", ":/toolbar/playlist",
    ":/toolbar/snapshot", ":/toolbar/record", ":/toolbar/atob_nob",
    ":/toolbar/frame", ":/toolbar/reverse", ":/toolbar/skip_back",
    ":/toolbar/skip_fw", ":/toolbar/clear", ":/buttons/playlist/shuffle_on",
    ":/buttons/playlist/repeat_all", ":/menu/info",
    ":/toolbar/previous_b", ":/toolbar/next_b", ":/toolbar/eject", ":/toolbar/space"
};

enum
{
   WIDGET_NORMAL = 0x0,
   WIDGET_FLAT   = 0x1,
   WIDGET_BIG    = 0x2,
   WIDGET_SHINY  = 0x4,
};

class AdvControlsWidget;
class AbstractController : public QFrame
{
    friend class WidgetListing; /* For ToolBar Edition HACKS */

    Q_OBJECT
public:
    AbstractController( intf_thread_t  *_p_i, QWidget *_parent = 0 );

protected:
    intf_thread_t       *p_intf;

    QSignalMapper       *toolbarActionsMapper;
    QBoxLayout          *controlLayout;
    /* Change to BoxLayout if both dir are needed */

    AdvControlsWidget   *advControls;

    void parseAndCreate( const QString& config, QBoxLayout *controlLayout );

    virtual void createAndAddWidget( QBoxLayout *controlLayout, int i_index,
                                     buttonType_e i_type, int i_option );

    QWidget *createWidget( buttonType_e, int options = WIDGET_NORMAL );
private:
    static void setupButton( QAbstractButton * );
    QFrame *discFrame();
    QFrame *telexFrame();
    void applyAttributes( QToolButton *, bool b_flat, bool b_big );

    QHBoxLayout         *buttonGroupLayout;
protected slots:
    virtual void setStatus( int );

signals:
    void inputExists( bool ); /// This might be useful in the IM ?
    void inputPlaying( bool ); /// This might be useful in the IM ?
    void inputIsRecordable( bool ); /// same ?
    void inputIsTrickPlayable( bool ); /// same ?
};

/* Advanced Button Bar */
class AdvControlsWidget : public AbstractController
{
    Q_OBJECT
public:
    AdvControlsWidget( intf_thread_t *, QWidget *_parent = 0 );
};

/* Slider Bar */
class InputControlsWidget : public AbstractController
{
    Q_OBJECT
public:
    InputControlsWidget( intf_thread_t * , QWidget *_parent = 0 );
};

/* Button Bar */
class ControlsWidget : public AbstractController
{
    Q_OBJECT
public:
    /* p_intf, advanced control visible or not, blingbling or not */
    ControlsWidget( intf_thread_t *_p_i, bool b_advControls,
                    QWidget *_parent = 0 );

    void setGripVisible( bool b_visible )
    { grip->setVisible( b_visible ); }

protected:
    friend class MainInterface;

    bool b_advancedVisible;

private:
    QSizeGrip *grip;

protected slots:
    void toggleAdvanced();

signals:
    void advancedControlsToggled( bool );
};


/* to trying transparency with fullscreen controller on windows enable that */
/* it can be enabled on-non windows systems,
   but it will be transparent only with composite manager */
#define HAVE_TRANSPARENCY 1

/* Default value of opacity for FS controller */
#define DEFAULT_OPACITY 0.70

/* Used to restore the minimum width after a full-width switch */
#define FSC_WIDTH 800

/***********************************
 * Fullscreen controller
 ***********************************/
class FullscreenControllerWidget : public AbstractController
{
    Q_OBJECT
public:
    FullscreenControllerWidget( intf_thread_t *, QWidget *_parent = 0  );
    virtual ~FullscreenControllerWidget();

    /* Vout */
    void fullscreenChanged( vout_thread_t *, bool b_fs, int i_timeout );
    void mouseChanged( vout_thread_t *, int i_mousex, int i_mousey );
    void toggleFullwidth();
    void updateFullwidthGeometry( int number );
    int targetScreen();

signals:
    void keyPressed( QKeyEvent * );

public slots:
    void setVoutList( vout_thread_t **, int );

protected:
    friend class MainInterface;

    virtual void mouseMoveEvent( QMouseEvent *event );
    virtual void mousePressEvent( QMouseEvent *event );
    virtual void mouseReleaseEvent( QMouseEvent *event );
    virtual void enterEvent( QEvent *event );
    virtual void leaveEvent( QEvent *event );
    virtual void keyPressEvent( QKeyEvent *event );

    virtual void customEvent( QEvent *event );

private slots:
    void showFSC();
    void planHideFSC();
    void hideFSC() { hide(); }
    void slowHideFSC();
    void restoreFSC();
    void centerFSC( int );

private:
    QTimer *p_hideTimer;
#if HAVE_TRANSPARENCY
    QTimer *p_slowHideTimer;
    bool b_slow_hide_begin;
    int  i_slow_hide_timeout;
    float f_opacity;
#endif

    int i_mouse_last_x, i_mouse_last_y;
    bool b_mouse_over;
    int i_screennumber;
    QRect screenRes;
    QRect previousScreenRes;
    QPoint previousPosition;

    /* List of vouts currently tracked */
    QList<vout_thread_t *> vout;

    /* Shared variable between FSC and VLC (protected by a lock) */
    vlc_mutex_t lock;
    bool        b_fullscreen;
    int         i_hide_timeout;  /* FSC hiding timeout, same as mouse hiding timeout */
    int i_mouse_last_move_x;
    int i_mouse_last_move_y;

    bool isWideFSC;
};

#endif
