/*****************************************************************************
 * intf.cpp: Qt interface
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: intf.cpp,v 1.1 2002/08/04 17:23:43 sam Exp $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */
#include <stdio.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>

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
    void Manage ( void );

    void FileOpen  ( void );
    void FileQuit  ( void );

    void PlaybackPlay  ( void );
    void PlaybackPause ( void );
    void PlaybackSlow  ( void );
    void PlaybackFast  ( void );

    void PlaylistPrev  ( void );
    void PlaylistNext  ( void );

    void DateDisplay  ( int );
    void About ( void );

    void Unimplemented( void ) { msg_Warn( p_intf, "unimplemented" ); };

private:
    intf_thread_t *p_intf;

    IntfSlider *p_slider;

    QToolBar   *p_toolbar;
    QPopupMenu *p_popup;
    QLabel     *p_date;
};

#include "intf.moc"

#define SLIDER_MIN    0x00000
#define SLIDER_MAX    0x10000
#define SLIDER_STEP   (SLIDER_MAX >> 4)

/*****************************************************************************
 * intf_sys_t: description and status of Qt interface
 *****************************************************************************/
struct intf_sys_t
{
    QApplication *p_app;
    IntfWindow   *p_window;

    input_thread_t *p_input;
};

/*****************************************************************************
 * Local prototype
 *****************************************************************************/
static void Run ( intf_thread_t *p_intf );

/*****************************************************************************
 * Open: initialize and create window
 *****************************************************************************/
int E_(Open) ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;
    char *pp_argv[] = { "" };
    int   i_argc    = 1;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = (intf_sys_t *)malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        msg_Err( p_intf, "out of memory" );
        return 1;
    }

    p_intf->pf_run = Run;

    /* Create the C++ objects */
    p_intf->p_sys->p_app = new QApplication( i_argc, pp_argv );
    p_intf->p_sys->p_window = new IntfWindow( p_intf );

    /* Tell the world we are here */
    p_intf->p_sys->p_window->setCaption( VOUT_TITLE " (Qt interface)" );

    p_intf->p_sys->p_input = NULL;

    return 0;
}

/*****************************************************************************
 * Close: destroy interface window
 *****************************************************************************/
void E_(Close) ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;

    if( p_intf->p_sys->p_input )
    {
        vlc_object_release( p_intf->p_sys->p_input );
    }

    /* Get rid of the C++ objects */
    delete p_intf->p_sys->p_window;
    delete p_intf->p_sys->p_app;

    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: Qt thread
 *****************************************************************************
 * This part of the interface is in a separate thread so that we can call
 * exec() from within it without annoying the rest of the program.
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
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
    if( p_intf->p_sys->p_input )
    {
        char psz_time[ OFFSETTOTIME_MAX_SIZE ];

        vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
        p_date->setText( input_OffsetToTime( p_intf->p_sys->p_input, psz_time,
           ( p_intf->p_sys->p_input->stream.p_selected_area->i_size * i_range )
               / SLIDER_MAX ) );
        vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
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
    playlist_t *p_playlist;
    QString file = QFileDialog::getOpenFileName( QString::null,
                                                 QString::null, this );

    if( file.isEmpty() )
    {
        statusBar()->message( "No file loaded", 2000 );
    }
    else
    {
        p_playlist = (playlist_t *)
                vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
        if( p_playlist == NULL )
        {
            return;
        }

        playlist_Add( p_playlist, file.latin1(),
                      PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
        vlc_object_release( p_playlist );
    }
}

/*****************************************************************************
 * FileQuit: terminate vlc
 *****************************************************************************/
void IntfWindow::FileQuit( void )
{
    p_intf->p_vlc->b_die = VLC_TRUE;
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
        "(C) 1996, 1997, 1998, 1999, 2000, 2001, 2002 - the VideoLAN Team\n"
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
    /* Update the input */
    if( p_intf->p_sys->p_input == NULL )
    {
        p_intf->p_sys->p_input = (input_thread_t *)
                vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    }
    else if( p_intf->p_sys->p_input->b_dead )
    {
        vlc_object_release( p_intf->p_sys->p_input );
        p_intf->p_sys->p_input = NULL;
    }

    /* Manage the slider */
    if( p_intf->p_sys->p_input && p_intf->p_sys->p_input->stream.b_seekable )
    {
        int i_value = p_slider->value();

#define p_area p_intf->p_sys->p_input->stream.p_selected_area
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

            input_Seek( p_intf->p_sys->p_input, i_seek, INPUT_SEEK_SET );

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

    if( p_intf->b_die )
    {
        qApp->quit();
    }
}

/*****************************************************************************
 * PlaybackPlay: play
 *****************************************************************************/
void IntfWindow::PlaybackPlay( void )
{
    if( p_intf->p_sys->p_input != NULL )
    {
        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PLAY );
    }
}

/*****************************************************************************
 * PlaybackPause: pause
 *****************************************************************************/
void IntfWindow::PlaybackPause( void )
{
    if( p_intf->p_sys->p_input != NULL )
    {
        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PAUSE );
    }
}

/*****************************************************************************
 * PlaybackSlow: slow
 *****************************************************************************/
void IntfWindow::PlaybackSlow( void )
{
    if( p_intf->p_sys->p_input != NULL )
    {
        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_SLOWER );
    }
}

/*****************************************************************************
 * PlaybackFast: fast
 *****************************************************************************/
void IntfWindow::PlaybackFast( void )
{
    if( p_intf->p_sys->p_input != NULL )
    {
        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_FASTER );
    }
}

/*****************************************************************************
 * PlaylistPrev: previous playlist entry
 *****************************************************************************/
void IntfWindow::PlaylistPrev( void )
{
    playlist_t *p_playlist = (playlist_t *)
        vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    if( p_playlist == NULL )
    {
        return;
    }

    playlist_Prev( p_playlist );
    vlc_object_release( p_playlist );
}

/*****************************************************************************
 * PlaylistNext: next playlist entry
 *****************************************************************************/
void IntfWindow::PlaylistNext( void )
{
    playlist_t *p_playlist = (playlist_t *)
        vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    if( p_playlist == NULL )
    {
        return;
    }

    playlist_Next( p_playlist );
    vlc_object_release( p_playlist );
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


