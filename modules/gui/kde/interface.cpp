/***************************************************************************
                          interface.cpp  -  description
                             -------------------
    begin                : Sun Mar 25 2001
    copyright            : (C) 2001 by andres
    email                : dae@chez.com
 ***************************************************************************/

#include "disc.h"
#include "interface.h"
#include "net.h"
#include "menu.h"
#include "slider.h"
#include "preferences.h"

#include <iostream.h>

#include <kaction.h>
#include <kfiledialog.h>
#include <klocale.h>
#include <kstdaction.h>
#include <kurl.h>
#include <kurldrag.h>
#include <qcursor.h>
#include <qdragobject.h>
#include <qtimer.h>
#include <kdialog.h>
#include <kstatusbar.h>

#define ID_STATUS_MSG       1
#define ID_DATE             2
#define ID_STREAM_SOURCE    3

KInterface::KInterface( intf_thread_t *p_intf, QWidget *parent,
        const char *name ) : KMainWindow(parent,name)
{
    setAcceptDrops(true);

    this->p_intf = p_intf;
    p_messagesWindow = new KMessagesWindow( p_intf, p_intf->p_sys->p_msg );
    p_messagesWindow->show();
    fDiskDialog = new KDiskDialog( this );
    fNetDialog = new KNetDialog( this );
    fTitleMenu = new KTitleMenu( p_intf, this );

    fSlider = new KVLCSlider( QSlider::Horizontal, this );
    fSlider->setMaxValue(10000);
    connect( fSlider, SIGNAL( userChanged( int ) ), this, SLOT( slotSliderMoved( int ) ) );
    connect( fSlider, SIGNAL( valueChanged( int ) ), this, SLOT( slotSliderChanged( int ) ) );
    setCentralWidget(fSlider);

    fTimer = new QTimer( this );
    connect( fTimer, SIGNAL( timeout() ), this, SLOT( slotManage() ) );
    fTimer->start( 100 );

    resize( 400, 30 );

    ///////////////////////////////////////////////////////////////////
    // call inits to invoke all other construction parts
    // XXX could we move this up ?
    initStatusBar();
    initActions();

    // add certain calls to the popup menu
    fileOpen->plug( fTitleMenu );
    fileOpenRecent->plug( fTitleMenu );
    diskOpen->plug( fTitleMenu );
    streamOpen->plug( fTitleMenu );
    play->plug( fTitleMenu );
    pause->plug( fTitleMenu );
    slow->plug( fTitleMenu );
    fast->plug( fTitleMenu );
    fileClose->plug( fTitleMenu );
    fileQuit->plug( fTitleMenu );
}

KInterface::~KInterface()
{
    ;
}

void KInterface::initActions()
{
    fileOpen = KStdAction::open(this, SLOT(slotFileOpen()), actionCollection());
    fileOpenRecent = KStdAction::openRecent(this, SLOT(slotFileOpenRecent(const KURL&)), actionCollection());
    fileClose = KStdAction::close(this, SLOT(slotFileClose()), actionCollection());
    preferences = KStdAction::preferences(this, SLOT(slotShowPreferences()), actionCollection());
    fileQuit = KStdAction::quit(this, SLOT(slotFileQuit()), actionCollection());
    viewToolBar = KStdAction::showToolbar(this, SLOT(slotViewToolBar()), actionCollection());
    viewStatusBar = KStdAction::showStatusbar(this, SLOT(slotViewStatusBar()), actionCollection());

    diskOpen = new KAction( i18n( "Open &Disk" ), 0, 0, this, SLOT( slotOpenDisk() ), actionCollection(), "open_disk" );
    streamOpen = new KAction( i18n( "Open &Stream" ), 0, 0, this, SLOT( slotOpenStream() ), actionCollection(), "open_stream" );
    backward = new KAction( i18n( "&Backward" ), 0, 0, this, SLOT( slotBackward() ), actionCollection(), "backward" );
    stop = new KAction( i18n( "&Stop" ), 0, 0, this, SLOT( slotStop() ), actionCollection(), "stop" );
    play = new KAction( i18n( "&Play" ), 0, 0, this, SLOT( slotPlay() ), actionCollection(), "play" );
    pause = new KAction( i18n( "P&ause" ), 0, 0, this, SLOT( slotPause() ), actionCollection(), "pause" );
    slow = new KAction( i18n( "&Slow" ), 0, 0, this, SLOT( slotSlow() ), actionCollection(), "slow" );
    fast = new KAction( i18n( "Fas&t" ), 0, 0, this, SLOT( slotFast() ), actionCollection(), "fast" );
    prev = new KAction( i18n( "Prev" ), 0, 0, this, SLOT( slotPrev() ), actionCollection(), "prev" );
    next = new KAction( i18n( "Next" ), 0, 0, this, SLOT( slotNext() ), actionCollection(), "next" );
    messages = new KAction( _( "Messages..." ), 0, 0, this, SLOT( slotShowMessages() ), actionCollection(), "view_messages");
    
    fileOpen->setStatusText(i18n("Opens an existing document"));
    fileOpenRecent->setStatusText(i18n("Opens a recently used file"));
    fileClose->setStatusText(i18n("Closes the actual document"));
    fileQuit->setStatusText(i18n("Quits the application"));
    viewToolBar->setStatusText(i18n("Enables/disables the toolbar"));
    viewStatusBar->setStatusText(i18n("Enables/disables the statusbar"));

    diskOpen->setStatusText( i18n( "Opens a disk") );
    streamOpen->setStatusText( i18n( "Opens a network stream" ) );
    backward->setStatusText( i18n( "Backward" ) );
    stop->setStatusText( i18n( "Stops playback" ) );
    play->setStatusText( i18n( "Starts playback" ) );
    pause->setStatusText( i18n( "Pauses playback" ) );
    slow->setStatusText( i18n( "Slow" ) );
    fast->setStatusText( i18n( "Fast" ) );
    prev->setStatusText( i18n( "Prev" ) );
    next->setStatusText( i18n( "Next" ) );
    // use the absolute path to your ktestui.rc file for testing purpose in createGUI();

    createGUI( DATA_PATH "/ui.rc" );
//    createGUI( "./modules/gui/kde/ui.rc" );
}

void KInterface::initStatusBar()
{
  ///////////////////////////////////////////////////////////////////
  // STATUSBAR
  // TODO: add your own items you need for displaying current application status.
    statusBar()->insertItem(i18n("Ready."), ID_STATUS_MSG, 1, false);
    statusBar()->setItemAlignment( ID_STATUS_MSG, AlignLeft | AlignVCenter );
    statusBar()->insertItem( "0:00:00", ID_DATE, 0, true );
}

/////////////////////////////////////////////////////////////////////
// SLOT IMPLEMENTATION
/////////////////////////////////////////////////////////////////////
void KInterface::slotShowMessages()
{
    p_messagesWindow->show();
}

void KInterface::slotFileOpen()
{
    playlist_t *p_playlist;

    slotStatusMsg( i18n( "Opening file..." ) );
    KURL url=KFileDialog::getOpenURL( QString::null,
            i18n( "*|All files" ), this, i18n( "Open File..." ) );

    if( !url.isEmpty() )
    {
        p_playlist = (playlist_t *)
            vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
        if( p_playlist )
        {
            fileOpenRecent->addURL( url );
            playlist_Add( p_playlist, url.path(),
                          PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
            vlc_object_release( p_playlist );
        }
    }

    slotStatusMsg( i18n( "Ready." ) );
}

void KInterface::slotFileOpenRecent(const KURL& url)
{
  slotStatusMsg(i18n("Opening file..."));
  slotStatusMsg(i18n("Ready."));
}

void KInterface::slotFileClose()
{
  slotStatusMsg(i18n("Closing file..."));
    
  close();

  slotStatusMsg(i18n("Ready."));
}

void KInterface::slotFileQuit()
{
    slotStatusMsg(i18n("Exiting..."));
    p_intf->p_vlc->b_die = VLC_TRUE;
    slotStatusMsg(i18n("Ready."));
}

void KInterface::slotViewToolBar()
{
  slotStatusMsg(i18n("Toggling toolbar..."));
  ///////////////////////////////////////////////////////////////////
  // turn Toolbar on or off
  if(!viewToolBar->isChecked())
  {
    toolBar("mainToolBar")->hide();
  }
  else
  {
    toolBar("mainToolBar")->show();
  }        

  slotStatusMsg(i18n("Ready."));
}

void KInterface::slotViewStatusBar()
{
  slotStatusMsg(i18n("Toggle the statusbar..."));
  ///////////////////////////////////////////////////////////////////
  //turn Statusbar on or off
  if(!viewStatusBar->isChecked())
  {
    statusBar()->hide();
  }
  else
  {
    statusBar()->show();
  }

  slotStatusMsg(i18n("Ready."));
}

void KInterface::slotShowPreferences()
{
    // Do something
    KPreferences(this->p_intf, "main", this, "preferences");
}

void KInterface::slotStatusMsg(const QString &text)
{
  ///////////////////////////////////////////////////////////////////
  // change status message permanently
  statusBar()->clear();
  statusBar()->changeItem(text, ID_STATUS_MSG);
}

void KInterface::slotManage()
{
    p_messagesWindow->update();
    p_intf->p_sys->p_app->processEvents();
    vlc_mutex_lock( &p_intf->change_lock );

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

    /* If the "display popup" flag has changed */
    if( p_intf->b_menu_change )
    {
        fTitleMenu->popup( ( QCursor::pos() ) );
        p_intf->b_menu_change = 0;
    }

    /* Update language/chapter menus after user request */
#if 0
    if( p_intf->p_sys->p_input != NULL && p_intf->p_sys->p_window != NULL &&
        p_intf->p_sys->b_menus_update )
    {
//        GnomeSetupMenu( p_intf );
    }
#endif

    /* Manage the slider */
#define p_area p_intf->p_sys->p_input->stream.p_selected_area
    if( p_intf->p_sys->p_input && p_area->i_size )
    {
	fSlider->setValue( ( 10000. * p_area->i_tell ) / p_area->i_size );
    }
#undef p_area

    if( p_intf->b_die )
    {
        p_intf->p_sys->p_app->quit();
    }

    vlc_mutex_unlock( &p_intf->change_lock );

}

void KInterface::slotSliderMoved( int position )
{
    if( p_intf->p_sys->p_input )
    {
        // XXX is this locking really useful ?
        vlc_mutex_lock( &p_intf->change_lock );

        off_t i_seek = ( position * p_intf->p_sys->p_input->stream.p_selected_area->i_size ) / 10000;
        input_Seek( p_intf->p_sys->p_input, i_seek, INPUT_SEEK_SET );

        vlc_mutex_unlock( &p_intf->change_lock );
    }
}

void KInterface::slotSliderChanged( int position )
{
    if( p_intf->p_sys->p_input != NULL )
    {
        char psz_time[ OFFSETTOTIME_MAX_SIZE ];

        vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );

#define p_area p_intf->p_sys->p_input->stream.p_selected_area
        statusBar()->changeItem( input_OffsetToTime( p_intf->p_sys->p_input, psz_time, ( p_area->i_size * position ) / 10000 ), ID_DATE );
#undef p_area

        vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
     }
}

void KInterface::slotOpenDisk()
{
    playlist_t *p_playlist;
    int r = fDiskDialog->exec();
    if ( r )
    {
        // Build source name
        QString source;
        source += fDiskDialog->type();
        source += ':';
        source += fDiskDialog->device();

        source += '@';
        source += fDiskDialog->title();
        source += ',';
        source += fDiskDialog->chapter();

        p_playlist = (playlist_t *)
            vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
        if( p_playlist )
        {
            // add it to playlist
            playlist_Add( p_playlist, source.latin1(),
                          PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
            vlc_object_release( p_playlist );
        }
    }
}

void KInterface::slotOpenStream()
{
    playlist_t *p_playlist;
    int r = fNetDialog->exec();
    if ( r )
    {
        // Build source name
        QString source;
        source += fNetDialog->protocol();
        source += "://";
        source += fNetDialog->server();
        source += ":";
        source += QString().setNum( fNetDialog->port() );

        p_playlist = (playlist_t *)
            vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
        if( p_playlist )
        {
            // add it to playlist
            playlist_Add( p_playlist, source.latin1(),
                          PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
            vlc_object_release( p_playlist );
        }
    }
}

void KInterface::slotPlay()
{
    if( p_intf->p_sys->p_input )
    {
        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PLAY );
    }
}

void KInterface::slotPause()
{
    if ( p_intf->p_sys->p_input )
    {
        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PAUSE );
    }
}

void KInterface::slotStop()
{
    playlist_t *p_playlist = (playlist_t *)
            vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist )
    {
        playlist_Stop( p_playlist );
        vlc_object_release( p_playlist );
    }
}

void KInterface::slotBackward()
{
    msg_Err( p_intf, "KInterface::slotBackward() - Unimplemented" );
}

void KInterface::slotPrev()
{
    playlist_t *p_playlist = (playlist_t *)
            vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist )
    {
        playlist_Prev( p_playlist );
        vlc_object_release( p_playlist );
    }
}

void KInterface::slotNext()
{
    playlist_t *p_playlist = (playlist_t *)
            vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist )
    {
        playlist_Next( p_playlist );
        vlc_object_release( p_playlist );
    }
}

void KInterface::slotSlow()
{
    if( p_intf->p_sys->p_input != NULL )
    {
        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_SLOWER );
    }
}

void KInterface::slotFast()
{
    if( p_intf->p_sys->p_input != NULL )
    {
        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_FASTER );
    }
}

void KInterface::dragEnterEvent( QDragEnterEvent *event )
{
    event->accept( QUriDrag::canDecode( event ) );
}

void KInterface::dropEvent( QDropEvent *event )
{
    KURL::List urlList;

    playlist_t *p_playlist = (playlist_t *)
            vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    if ( KURLDrag::decode( event, urlList ) )
    {
        for ( KURL::List::ConstIterator i = urlList.begin(); i != urlList.end(); i++ )
        {
            // XXX add a private function to add a KURL with checking
            // actually a whole class for core abstraction would be neat
            if( !(*i).isEmpty() )
            {
                fileOpenRecent->addURL( *i );
                playlist_Add( p_playlist, (*i).path(),
                          PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
            }
        }
    }

    vlc_object_release( p_playlist );
}
