/***************************************************************************
                          kinterfacemain.cpp  -  description
                             -------------------
    begin                : Sun Mar 25 2001
    copyright            : (C) 2001 by andres
    email                : dae@chez.com
 ***************************************************************************/

#include "kdiskdialog.h"
#include "kinterfacemain.h"
#include "knetdialog.h"
#include "ktitlemenu.h"
#include "kvlcslider.h"

#include <iostream>

#include <kaction.h>
#include <kfiledialog.h>
#include <klocale.h>
#include <kstdaction.h>
#include <kurl.h>
#include <kurldrag.h>
#include <qcursor.h>
#include <qdragobject.h>
#include <qtimer.h>

#define ID_STATUS_MSG			1
#define ID_DATE							2
#define ID_STREAM_SOURCE	3

KInterfaceMain::KInterfaceMain( intf_thread_t *p_intf, QWidget *parent,
		const char *name ) : KMainWindow(parent,name)
{
	setAcceptDrops(true);

	fInterfaceThread = p_intf;

	fDiskDialog = new KDiskDialog( this );
	fNetDialog = new KNetDialog( this );
	fTitleMenu = new KTitleMenu( fInterfaceThread, this );

	fSlider = new KVLCSlider( QSlider::Horizontal, this );
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

KInterfaceMain::~KInterfaceMain()
{
}

void KInterfaceMain::initActions()
{
	fileOpen = KStdAction::open(this, SLOT(slotFileOpen()), actionCollection());
	fileOpenRecent = KStdAction::openRecent(this, SLOT(slotFileOpenRecent(const KURL&)), actionCollection());
	fileClose = KStdAction::close(this, SLOT(slotFileClose()), actionCollection());
	fileQuit = KStdAction::quit(this, SLOT(slotFileQuit()), actionCollection());
	viewToolBar = KStdAction::showToolbar(this, SLOT(slotViewToolBar()), actionCollection());
	viewStatusBar = KStdAction::showStatusbar(this, SLOT(slotViewStatusBar()), actionCollection());

	diskOpen = new KAction( i18n( "Open &Disk" ), 0, 0, this, SLOT( slotOpenDisk() ), actionCollection(), "open_disk" );
	streamOpen = new KAction( i18n( "Open &Stream" ), 0, 0, this, SLOT( slotOpenStream() ), actionCollection(), "open_stream" );
	play = new KAction( i18n( "&Play" ), 0, 0, this, SLOT( slotPlay() ), actionCollection(), "play" );
	pause = new KAction( i18n( "P&ause" ), 0, 0, this, SLOT( slotPause() ), actionCollection(), "pause" );
	stop = new KAction( i18n( "&Stop" ), 0, 0, this, SLOT( slotStop() ), actionCollection(), "stop" );
	backward = new KAction( i18n( "&Backward" ), 0, 0, this, SLOT( slotBackward() ), actionCollection(), "backward" );
	forward = new KAction( i18n( "&Forward" ), 0, 0, this, SLOT( slotForward() ), actionCollection(), "forward" );
	slow = new KAction( i18n( "&Slow" ), 0, 0, this, SLOT( slotSlow() ), actionCollection(), "slow" );
	fast = new KAction( i18n( "Fas&t" ), 0, 0, this, SLOT( slotFast() ), actionCollection(), "fast" );

	fileOpen->setStatusText(i18n("Opens an existing document"));
	fileOpenRecent->setStatusText(i18n("Opens a recently used file"));
	fileClose->setStatusText(i18n("Closes the actual document"));
	fileQuit->setStatusText(i18n("Quits the application"));
	viewToolBar->setStatusText(i18n("Enables/disables the toolbar"));
	viewStatusBar->setStatusText(i18n("Enables/disables the statusbar"));

	diskOpen->setStatusText( i18n( "Opens a disk") );
	streamOpen->setStatusText( i18n( "Opens a network stream" ) );
	play->setStatusText( i18n( "Starts playback" ) );
	pause->setStatusText( i18n( "Pauses playback" ) );
	stop->setStatusText( i18n( "Stops playback" ) );
	backward->setStatusText( i18n( "Backward" ) );
	forward->setStatusText( i18n( "Forward" ) );
	slow->setStatusText( i18n( "Slow" ) );
	fast->setStatusText( i18n( "Fast" ) );
	// use the absolute path to your ktestui.rc file for testing purpose in createGUI();

	createGUI("plugins/kde/kvlcui.rc");
}

void KInterfaceMain::initStatusBar()
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

void KInterfaceMain::slotFileOpen()
{
	slotStatusMsg( i18n( "Opening file..." ) );
	KURL url=KFileDialog::getOpenURL( QString::null,
			i18n( "*|All files" ), this, i18n( "Open File..." ) );

	if( !url.isEmpty() )
	{
		fileOpenRecent->addURL( url );
		intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, url.path() );
	}

	slotStatusMsg( i18n( "Ready." ) );
}

void KInterfaceMain::slotFileOpenRecent(const KURL& url)
{
  slotStatusMsg(i18n("Opening file..."));
  slotStatusMsg(i18n("Ready."));
}

void KInterfaceMain::slotFileClose()
{
  slotStatusMsg(i18n("Closing file..."));
	
  close();

  slotStatusMsg(i18n("Ready."));
}

void KInterfaceMain::slotFileQuit()
{
	slotStatusMsg(i18n("Exiting..."));
	slotStatusMsg(i18n("Ready."));
}

void KInterfaceMain::slotViewToolBar()
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

void KInterfaceMain::slotViewStatusBar()
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

void KInterfaceMain::slotStatusMsg(const QString &text)
{
  ///////////////////////////////////////////////////////////////////
  // change status message permanently
  statusBar()->clear();
  statusBar()->changeItem(text, ID_STATUS_MSG);
}

void KInterfaceMain::slotManage()
{
	vlc_mutex_lock( &fInterfaceThread->change_lock );

    /* If the "display popup" flag has changed */
	if( fInterfaceThread->b_menu_change )
	{
		fTitleMenu->popup( ( QCursor::pos() ) );
		fInterfaceThread->b_menu_change = 0;
	}

    /* Update language/chapter menus after user request */
//    if( fInterface->p_input != NULL && p_intf->p_sys->p_window != NULL &&
//        p_intf->p_sys->b_menus_update )
//    {
////        GnomeSetupMenu( p_intf );
//    }

    /* Manage the slider */
	if( fInterfaceThread->p_input != NULL )
	{
#define p_area fInterfaceThread->p_input->stream.p_selected_area
		fSlider->setValue( ( 100 * p_area->i_tell ) / p_area->i_size );
#undef p_area
    }

	/* Manage core vlc functions through the callback */
	fInterfaceThread->pf_manage(fInterfaceThread);

	if( fInterfaceThread->b_die )
	{
		cerr << "we shoud die\n";
	}

	vlc_mutex_unlock( &fInterfaceThread->change_lock );
}

void KInterfaceMain::slotSliderMoved( int position )
{
// XXX is this locking really useful ?
	vlc_mutex_lock( &fInterfaceThread->change_lock );

	off_t i_seek = ( position * fInterfaceThread->p_input->stream.p_selected_area->i_size ) / 100;
	input_Seek( fInterfaceThread->p_input, i_seek );

	vlc_mutex_unlock( &fInterfaceThread->change_lock );
}

void KInterfaceMain::slotSliderChanged( int position )
{
		if( fInterfaceThread->p_input != NULL )
		{
		char psz_time[ OFFSETTOTIME_MAX_SIZE ];

		vlc_mutex_lock( &fInterfaceThread->p_input->stream.stream_lock );

#define p_area fInterfaceThread->p_input->stream.p_selected_area
		statusBar()->changeItem( input_OffsetToTime( fInterfaceThread->p_input, psz_time, ( p_area->i_size * position ) / 100 ), ID_DATE );
#undef p_area

        vlc_mutex_unlock( &fInterfaceThread->p_input->stream.stream_lock );
     }
}

void KInterfaceMain::slotOpenDisk()
{
	int r = fDiskDialog->exec();
	if ( r )
	{
		// Build source name
		QString source;
		source += fDiskDialog->type();
		source += ':';
		source += fDiskDialog->device();

		// Select title and chapter
		main_PutIntVariable( INPUT_TITLE_VAR, fDiskDialog->title() );
		main_PutIntVariable( INPUT_CHAPTER_VAR, fDiskDialog->chapter() );

		// add it to playlist
		intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, source.latin1() );

		// Select added item and switch to disk interface
		intf_PlaylistJumpto( p_main->p_playlist, p_main->p_playlist->i_size-2 );
		if( fInterfaceThread->p_input != NULL )
		{
			fInterfaceThread->p_input->b_eof = 1;
		}
	}
}

void KInterfaceMain::slotOpenStream()
{
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

		// add it to playlist
		intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, source.latin1() );
		intf_PlaylistJumpto( p_main->p_playlist, p_main->p_playlist->i_size-2 );

		if( fInterfaceThread->p_input != NULL )
		{
			fInterfaceThread->p_input->b_eof = 1;
		}
	}
}

void KInterfaceMain::slotPlay()
{
	if( fInterfaceThread->p_input != NULL )
	{
		input_SetStatus( fInterfaceThread->p_input, INPUT_STATUS_PLAY );
	}
}

void KInterfaceMain::slotPause()
{
	if ( fInterfaceThread->p_input != NULL )
	{
		input_SetStatus( fInterfaceThread->p_input, INPUT_STATUS_PAUSE );
	}
}

void KInterfaceMain::slotStop()
{
	cerr << "KInterfaceMain::slotStop() - Unimplemented\n";
}

void KInterfaceMain::slotBackward()
{
    if( fInterfaceThread->p_input != NULL )
    {
        /* FIXME: temporary hack */
        intf_PlaylistPrev( p_main->p_playlist );
        intf_PlaylistPrev( p_main->p_playlist );
        fInterfaceThread->p_input->b_eof = 1;
    }
}

void KInterfaceMain::slotForward()
{
	if( fInterfaceThread->p_input != NULL )
	{
		/* FIXME: temporary hack */
		fInterfaceThread->p_input->b_eof = 1;
	}
}

void KInterfaceMain::slotSlow()
{
	if( fInterfaceThread->p_input != NULL )
	{
		input_SetStatus( fInterfaceThread->p_input, INPUT_STATUS_SLOWER );
	}
}

void KInterfaceMain::slotFast()
{
	if( fInterfaceThread->p_input != NULL )
	{
		input_SetStatus( fInterfaceThread->p_input, INPUT_STATUS_FASTER );
	}
}

void KInterfaceMain::dragEnterEvent( QDragEnterEvent *event )
{
	event->accept( QUriDrag::canDecode( event ) );
}

void KInterfaceMain::dropEvent( QDropEvent *event )
{
	KURL::List urlList;

	if ( KURLDrag::decode( event, urlList ) ) {
		for ( KURL::List::ConstIterator i = urlList.begin(); i != urlList.end(); i++ )
		{
			// XXX add a private function to add a KURL with checking
			// actually a whole class for core abstraction would be neat
			if( !(*i).isEmpty() )
			{
				fileOpenRecent->addURL( *i );
				intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, (*i).path() );
			}
		}
	}
}
