/*****************************************************************************
 * intf_qt.cpp: Qt interface
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: intf_qt.cpp,v 1.8 2001/11/28 15:08:05 massiot Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

extern "C"
{

#define MODULE_NAME qt
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */
#include <stdio.h>

#include "config.h"
#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "stream_control.h"
#include "input_ext-intf.h"

#include "intf_playlist.h"
#include "interface.h"

#include "main.h"

#include "modules.h"
#include "modules_export.h"

} /* extern "C" */

#include <qapplication.h>
#include <qmainwindow.h>
#include <qtoolbar.h>
#include <qtoolbutton.h>
#include <qwhatsthis.h>
#include <qpushbutton.h>
#include <qfiledialog.h>
#include <qslider.h>
#include <qlcdnumber.h>
#include <qmenubar.h>
#include <qstatusbar.h>
#include <qmessagebox.h>
#include <qlabel.h> 
#include <qtimer.h> 
#include <qiconset.h> 

#include <qvbox.h>
#include <qhbox.h>

/*****************************************************************************
 * Local Qt slider class
 *****************************************************************************/
class IntfSlider : public QSlider
{
    Q_OBJECT

public:
    IntfSlider( intf_thread_t *, QWidget * );  /* Constructor and destructor */
    ~IntfSlider();

    bool b_free;                                     /* Is the slider free ? */

    int  oldvalue   ( void ) { return i_oldvalue; };
    void setOldValue( int i_value ) { i_oldvalue = i_value; };

private slots:
    void SlideStart ( void ) { b_free = FALSE; };
    void SlideStop  ( void ) { b_free = TRUE; };

private:
    intf_thread_t *p_intf;
    int  i_oldvalue;
};

/*****************************************************************************
 * Local Qt interface window class
 *****************************************************************************/
class IntfWindow : public QMainWindow
{
    Q_OBJECT

public:
    IntfWindow( intf_thread_t * );
    ~IntfWindow();

private slots:
    void Manage( void );

    void FileOpen  ( void );
    void FileQuit  ( void ) { p_intf->b_die = 1; };

    void PlaybackPlay  ( void );
    void PlaybackPause ( void );
    void PlaybackSlow  ( void );
    void PlaybackFast  ( void );

    void PlaylistPrev  ( void );
    void PlaylistNext  ( void );

    void DateDisplay  ( int );
    void About ( void );

    void Unimplemented( void ) { intf_WarnMsg( 1, "intf warning: "
                                 "unimplemented function" ); };

private:
    intf_thread_t *p_intf;

    IntfSlider *p_slider;

    QToolBar   *p_toolbar;
    QPopupMenu *p_popup;
    QLabel     *p_date;
};

#ifdef BUILTIN
#   include "BUILTIN_intf_qt.moc"
#else
#   include "intf_qt.moc"
#endif

#define SLIDER_MIN    0x00000
#define SLIDER_MAX    0x10000
#define SLIDER_STEP   (SLIDER_MAX >> 4)

/*****************************************************************************
 * intf_sys_t: description and status of Qt interface
 *****************************************************************************/
typedef struct intf_sys_s
{
    QApplication *p_app;
    IntfWindow   *p_window;

} intf_sys_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  intf_Probe     ( probedata_t *p_data );
static int  intf_Open      ( intf_thread_t *p_intf );
static void intf_Close     ( intf_thread_t *p_intf );
static void intf_Run       ( intf_thread_t *p_intf );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
extern "C"
{

void _M( intf_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = intf_Probe;
    p_function_list->functions.intf.pf_open  = intf_Open;
    p_function_list->functions.intf.pf_close = intf_Close;
    p_function_list->functions.intf.pf_run   = intf_Run;
}

}

/*****************************************************************************
 * intf_Probe: probe the interface and return a score
 *****************************************************************************
 * This function tries to initialize Qt and returns a score to the
 * plugin manager so that it can select the best plugin.
 *****************************************************************************/
static int intf_Probe( probedata_t *p_data )
{
    if( TestMethod( INTF_METHOD_VAR, "qt" ) )
    {
        return( 999 );
    }

    if( TestProgram( "qvlc" ) )
    {
        return( 180 );
    }

    return( 80 );
}

/*****************************************************************************
 * intf_Open: initialize and create window
 *****************************************************************************/
static int intf_Open( intf_thread_t *p_intf )
{
    char *pp_argv[] = { "" };
    int   i_argc    = 1;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = (intf_sys_t *)malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        intf_ErrMsg( "intf error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    /* Create the C++ objects */
    p_intf->p_sys->p_app = new QApplication( i_argc, pp_argv );
    p_intf->p_sys->p_window = new IntfWindow( p_intf );

    /* Tell the world we are here */
    p_intf->p_sys->p_window->setCaption( VOUT_TITLE " (Qt interface)" );

    return( 0 );
}

/*****************************************************************************
 * intf_Close: destroy interface window
 *****************************************************************************/
static void intf_Close( intf_thread_t *p_intf )
{
    /* Get rid of the C++ objects */
    delete p_intf->p_sys->p_window;
    delete p_intf->p_sys->p_app;

    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * intf_Run: Qt thread
 *****************************************************************************
 * This part of the interface is in a separate thread so that we can call
 * exec() from within it without annoying the rest of the program.
 *****************************************************************************/
static void intf_Run( intf_thread_t *p_intf )
{
    p_intf->p_sys->p_window->show();

    p_intf->p_sys->p_app->exec();
}

/* following functions are local */

/*****************************************************************************
 * IntfWindow: interface window creator
 *****************************************************************************
 * This function creates the interface window, and populates it with a
 * menu bar, a toolbar and a slider.
 *****************************************************************************/
IntfWindow::IntfWindow( intf_thread_t *p_intf )
           :QMainWindow( 0 )
{
    setUsesTextLabel( TRUE );

    this->p_intf = p_intf;

    /*
     * Create the toolbar
     */

    p_toolbar = new QToolBar( this, "toolbar" );
    p_toolbar->setHorizontalStretchable( TRUE );

    QIconSet * set = new QIconSet();
    QPixmap pixmap = set->pixmap( QIconSet::Automatic, QIconSet::Normal );

#define addbut( l, t, s ) new QToolButton( pixmap, l, t, this, s, p_toolbar );
    addbut( "Open", "Open a File", SLOT(FileOpen()) );
    addbut( "Disc", "Open a DVD or VCD", SLOT(Unimplemented()) );
    addbut( "Net", "Select a Network Stream", SLOT(Unimplemented()) );
    p_toolbar->addSeparator();
    addbut( "Back", "Rewind Stream", SLOT(Unimplemented()) );
    addbut( "Stop", "Stop Stream", SLOT(Unimplemented()) );
    addbut( "Play", "Play Stream", SLOT(PlaybackPlay()) );
    addbut( "Pause", "Pause Stream", SLOT(PlaybackPause()) );
    addbut( "Slow", "Play Slower", SLOT(PlaybackSlow()) );
    addbut( "Fast", "Play Faster", SLOT(PlaybackFast()) );
    p_toolbar->addSeparator();
    addbut( "Playlist", "Open Playlist", SLOT(Unimplemented()) );
    addbut( "Prev", "Previous File", SLOT(PlaylistPrev()) );
    addbut( "Next", "Next File", SLOT(PlaylistNext()) );
#undef addbut

    /* 
     * Create the menubar
     */

    QPopupMenu * p_tmpmenu = new QPopupMenu( this );

#define instmp( x, y... ) p_tmpmenu->insertItem( x, this, ## y )
    menuBar()->insertItem( "&File", p_tmpmenu );
    instmp( "&Open File...", SLOT(FileOpen()), Key_F3 );
    instmp( "Open &Disc...", SLOT(Unimplemented()), Key_F4 );
    instmp( "&Network Stream...", SLOT(Unimplemented()), Key_F5 );
    p_tmpmenu->insertSeparator();
    instmp( "&Exit", SLOT(FileQuit()), CTRL+Key_Q );

    p_tmpmenu = new QPopupMenu( this );
    menuBar()->insertItem( "&View", p_tmpmenu );
    instmp( "&Playlist...", SLOT(Unimplemented()) );
    instmp( "&Modules...", SLOT(Unimplemented()) );

    p_tmpmenu = new QPopupMenu( this );
    menuBar()->insertItem( "&Settings", p_tmpmenu );
    instmp( "&Preferences...", SLOT(Unimplemented()) );

    p_tmpmenu = new QPopupMenu( this );
    menuBar()->insertItem( "&Help", p_tmpmenu );
    instmp( "&About...", SLOT(About()) );
#undef instmp

    /*
     * Create the popup menu
     */

    p_popup = new QPopupMenu( /* floating menu */ );

#define inspop( x, y... ) p_popup->insertItem( x, this, ## y )
    inspop( "&Play", SLOT(PlaybackPlay()) );
    inspop( "Pause", SLOT(PlaybackPause()) );
    inspop( "&Slow", SLOT(PlaybackSlow()) );
    inspop( "&Fast", SLOT(PlaybackFast()) );
    p_popup->insertSeparator();
    inspop( "&Open File...", SLOT(FileOpen()), Key_F3 );
    inspop( "Open &Disc...", SLOT(Unimplemented()), Key_F4 );
    inspop( "&Network Stream...", SLOT(Unimplemented()), Key_F5 );
    p_popup->insertSeparator();
    inspop( "&About...", SLOT(About()) );
    inspop( "&Exit", SLOT(FileQuit()) );
#undef inspop

    /* Activate the statusbar */
    statusBar();

    /* Add the vertical box */
    QVBox * p_vbox = new QVBox( this );
    setCentralWidget( p_vbox );

        /* The horizontal box */
        QHBox * p_hbox = new QHBox( p_vbox );

            /* The date label */
            p_date  = new QLabel( p_hbox );
            p_date->setAlignment( AlignHCenter | AlignVCenter );
            p_date->setText( "-:--:--" );

            /* The status label */
            QLabel *p_label  = new QLabel( p_hbox );
            p_label->setAlignment( AlignHCenter | AlignVCenter );
            p_label->setText( "Status: foo" );

            /* The bar label */
            p_label  = new QLabel( p_hbox );
            p_label->setAlignment( AlignHCenter | AlignVCenter );
            p_label->setText( "Bar: baz quux" );

        /* Create the slider and connect it to the date label */
        p_slider = new IntfSlider( p_intf, p_vbox );

        connect( p_slider, SIGNAL(valueChanged(int)),
                 this, SLOT(DateDisplay(int)) );

    /* The timer */
    QTimer *p_timer = new QTimer( this );
    connect( p_timer, SIGNAL(timeout()), this, SLOT(Manage()) );
    p_timer->start( INTF_IDLE_SLEEP / 1000 );

    /* Everything worked fine */
    resize( 620, 30 );
}

/*****************************************************************************
 * ~IntfWindow: interface window destructor
 *****************************************************************************
 * This function is called when the interface window is destroyed.
 *****************************************************************************/
IntfWindow::~IntfWindow( void )
{
    /* FIXME: remove everything cleanly */
}

/*****************************************************************************
 * DateDisplay: display date
 *****************************************************************************
 * This function displays the current date in the date label.
 *****************************************************************************/
void IntfWindow::DateDisplay( int i_range )
{
    if( p_intf->p_input != NULL )
    {
        char psz_time[ OFFSETTOTIME_MAX_SIZE ];

        vlc_mutex_lock( &p_intf->p_input->stream.stream_lock );
        p_date->setText( input_OffsetToTime( p_intf->p_input, psz_time,
               ( p_intf->p_input->stream.p_selected_area->i_size * i_range )
                   / SLIDER_MAX ) );
        vlc_mutex_unlock( &p_intf->p_input->stream.stream_lock );
    }
}

/*****************************************************************************
 * FileOpen: open a file
 *****************************************************************************
 * This function opens a file requester and adds the selected file to
 * the playlist.
 *****************************************************************************/
void IntfWindow::FileOpen( void )
{
    QString file = QFileDialog::getOpenFileName( QString::null,
                                                 QString::null, this );

    if( file.isEmpty() )
    {
        statusBar()->message( "No file loaded", 2000 );
    }
    else
    {
        intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, file.latin1() );
    }
}

/*****************************************************************************
 * About: display the "about" box
 *****************************************************************************
 * This function displays a simple "about" box with copyright information.
 *****************************************************************************/
void IntfWindow::About( void )
{
    QMessageBox::about( this, "About",
        "VideoLAN Client\n"
        "(C) 1996, 1997, 1998, 1999, 2000, 2001 - the VideoLAN Team\n"
        "\n"
        "This is the VideoLAN client, a DVD and MPEG player.\n"
        "It can play MPEG and MPEG 2 files from a file "
            "or from a network source.\n"
        "\n"
        "More information: http://www.videolan.org/" );
}

/*****************************************************************************
 * Manage: manage main thread messages
 *****************************************************************************
 * In this function, called approx. 10 times a second, we check what the
 * main program wanted to tell us.
 *****************************************************************************/
void IntfWindow::Manage( void )
{
    /* Manage the slider */
    if( p_intf->p_input != NULL && p_intf->p_input->stream.b_seekable )
    {
        int i_value = p_slider->value();

#define p_area p_intf->p_input->stream.p_selected_area
        /* If the user hasn't touched the slider since the last time,
         * then the input can safely change it */
        if( i_value == p_slider->oldvalue() )
        {
            i_value = ( SLIDER_MAX * p_area->i_tell ) / p_area->i_size;

            p_slider->setValue( i_value );
            p_slider->setOldValue( i_value );
        }
        /* Otherwise, send message to the input if the user has
         * finished dragging the slider */
        else if( p_slider->b_free )
        {
            off_t i_seek = ( i_value * p_area->i_size ) / SLIDER_MAX;

            input_Seek( p_intf->p_input, i_seek );

            /* Update the old value */
            p_slider->setOldValue( i_value );
        }
#undef p_area
    }

    /* If the "display popup" flag has changed, popup the context menu */
    if( p_intf->b_menu_change )
    {
        p_popup->popup( QCursor::pos() );
        p_intf->b_menu_change = 0;
    }

    /* Manage core vlc functions through the callback */
    p_intf->pf_manage( p_intf );

    if( p_intf->b_die )
    {
        /* Prepare to die, young Skywalker */
        qApp->quit();

        /* Just in case */
        return;
    }
}

/*****************************************************************************
 * PlaybackPlay: play
 *****************************************************************************/
void IntfWindow::PlaybackPlay( void )
{
    if( p_intf->p_input != NULL )
    {
        input_SetStatus( p_intf->p_input, INPUT_STATUS_PLAY );
    }
}

/*****************************************************************************
 * PlaybackPause: pause
 *****************************************************************************/
void IntfWindow::PlaybackPause( void )
{
    if( p_intf->p_input != NULL )
    {
        input_SetStatus( p_intf->p_input, INPUT_STATUS_PAUSE );
    }
}

/*****************************************************************************
 * PlaybackSlow: slow
 *****************************************************************************/
void IntfWindow::PlaybackSlow( void )
{
    if( p_intf->p_input != NULL )
    {
        input_SetStatus( p_intf->p_input, INPUT_STATUS_SLOWER );
    }
}

/*****************************************************************************
 * PlaybackFast: fast
 *****************************************************************************/
void IntfWindow::PlaybackFast( void )
{
    if( p_intf->p_input != NULL )
    {
        input_SetStatus( p_intf->p_input, INPUT_STATUS_FASTER );
    }
}

/*****************************************************************************
 * PlaylistPrev: previous playlist entry
 *****************************************************************************/
void IntfWindow::PlaylistPrev( void )
{
    if( p_intf->p_input != NULL )
    {
        /* FIXME: temporary hack */
        intf_PlaylistPrev( p_main->p_playlist );
        intf_PlaylistPrev( p_main->p_playlist );
        p_intf->p_input->b_eof = 1;
    }
}

/*****************************************************************************
 * PlaylistNext: next playlist entry
 *****************************************************************************/
void IntfWindow::PlaylistNext( void )
{
    if( p_intf->p_input != NULL )
    {
        /* FIXME: temporary hack */
        p_intf->p_input->b_eof = 1;
    }
}

/*****************************************************************************
 * IntfSlider: slider creator
 *****************************************************************************
 * This function creates the slider, sets its default values, and connects
 * the interesting signals.
 *****************************************************************************/
IntfSlider::IntfSlider( intf_thread_t *p_intf, QWidget *p_parent )
           :QSlider( Horizontal, p_parent )
{
    this->p_intf = p_intf;

    setRange( SLIDER_MIN, SLIDER_MAX );
    setPageStep( SLIDER_STEP );

    setValue( SLIDER_MIN );
    setOldValue( SLIDER_MIN );

    setTracking( TRUE );
    b_free = TRUE;

    connect( this, SIGNAL(sliderMoved(int)), this, SLOT(SlideStart()) );
    connect( this, SIGNAL(sliderPressed()), this, SLOT(SlideStart()) );
    connect( this, SIGNAL(sliderReleased()), this, SLOT(SlideStop()) );
}

/*****************************************************************************
 * ~IntfSlider: slider destructor
 *****************************************************************************
 * This function is called when the interface slider is destroyed.
 *****************************************************************************/
IntfSlider::~IntfSlider( void )
{
    /* We don't need to remove anything */
}


