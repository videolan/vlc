/***************************************************************************
                          interface.cpp  -  description
                             -------------------
    begin                : Sun Mar 25 2001
    copyright            : (C) 2001 by andres
    email                : dae@chez.com
 ***************************************************************************/

#include "disc.h"
#include "info.h"
#include "interface.h"
#include "net.h"
#include "menu.h"
#include "slider.h"
#include "preferences.h"
#include "languagemenu.h"

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
    fDiskDialog = new KDiskDialog( this );
    fNetDialog = new KNetDialog( this );
    fTitleMenu = new KTitleMenu( p_intf, this );

    fSlider = new KVLCSlider( QSlider::Horizontal, this );
    fSlider->setMaxValue(10000);
    connect( fSlider, SIGNAL( userChanged( int ) ), this,
             SLOT( slotSliderMoved( int ) ) );
    connect( fSlider, SIGNAL( valueChanged( int ) ), this,
             SLOT( slotSliderChanged( int ) ) );
    connect( fSlider, SIGNAL( sliderMoved( int ) ), this,
             SLOT( slotSliderChanged( int ) ) );
    setCentralWidget(fSlider);

    fTimer = new QTimer( this );
    connect( fTimer, SIGNAL( timeout() ), this, SLOT( slotManage() ) );

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
    fileQuit->plug( fTitleMenu );
    fTimer->start( 0, FALSE );

}

KInterface::~KInterface()
{
    ;
}

void KInterface::initActions()
{
    languages = new KActionMenu( _( "Languages" ), actionCollection(),
                                 _("language") );
    languages->setEnabled( false );
    languageCollection = new KActionCollection( this );
    subtitleCollection = new KActionCollection( this );
    subtitles = new KActionMenu( _( "Subtitles" ), actionCollection(),
                                 "subtitles" );
    subtitles->setEnabled( false );
    fileOpen =
        KStdAction::open(this, SLOT(slotFileOpen()), actionCollection());
    fileOpenRecent =
        KStdAction::openRecent(this, SLOT(slotFileOpenRecent(const KURL&)),
                               actionCollection());
    preferences = KStdAction::preferences(this, SLOT(slotShowPreferences()),
                                          actionCollection());
    fileQuit = KStdAction::quit(this, SLOT(slotFileQuit()),
                                actionCollection());
    viewToolBar = KStdAction::showToolbar(this, SLOT(slotViewToolBar()),
                                          actionCollection());
    viewStatusBar = KStdAction::showStatusbar(this, SLOT(slotViewStatusBar()),
                                              actionCollection());

    diskOpen = new KAction( i18n( _("Open &Disk") ), 0, 0, this,
                            SLOT( slotOpenDisk() ), actionCollection(),
                            "open_disk" );
    streamOpen = new KAction( i18n( _("Open &Stream") ), 0, 0, this,
                              SLOT( slotOpenStream() ), actionCollection(),
                              "open_stream" );
    backward = new KAction( i18n( _("&Backward") ), 0, 0, this,
                            SLOT( slotBackward() ), actionCollection(),
                            "backward" );
    stop = new KAction( i18n( _("&Stop") ), 0, 0, this,
                        SLOT( slotStop() ), actionCollection(), "stop" );
    play = new KAction( i18n( _("&Play") ), 0, 0, this,
                        SLOT( slotPlay() ), actionCollection(), "play" );
    pause = new KAction( i18n( _("P&ause") ), 0, 0, this,
                         SLOT( slotPause() ), actionCollection(), "pause" );
    slow = new KAction( i18n( _("&Slow") ), 0, 0, this,
                        SLOT( slotSlow() ), actionCollection(), "slow" );
    fast = new KAction( i18n( _("Fas&t") ), 0, 0, this,
                        SLOT( slotFast() ), actionCollection(), "fast" );
    prev = new KAction( i18n( _("Prev") ), 0, 0, this,
                        SLOT( slotPrev() ), actionCollection(), "prev" );
    next = new KAction( i18n( _("Next") ), 0, 0, this,
                        SLOT( slotNext() ), actionCollection(), "next" );
    messages = new KAction( _( "Messages..." ), 0, 0, this,
                            SLOT( slotShowMessages() ), actionCollection(),
                            "view_messages");
    
    info = new KAction( _( "Stream info..." ), 0, 0, this,
                        SLOT( slotShowInfo() ), actionCollection(),
                        "view_stream_info");

    info->setEnabled( false );
    program = new KActionMenu( _( "Program" ), actionCollection(), "program" );
    program->setEnabled( false );
    title = new KActionMenu( _( "Title" ), actionCollection(), "title" );
    title->setEnabled( false );
    chapter = new KActionMenu( _( "Chapter" ), actionCollection(), "chapter" );
    chapter->setEnabled( false );
    fileOpen->setStatusText(i18n(_("Opens an existing document")));
    fileOpenRecent->setStatusText(i18n(_("Opens a recently used file")));
    fileQuit->setStatusText(i18n(_("Quits the application")));
    viewToolBar->setStatusText(i18n(_("Enables/disables the toolbar")));
    viewStatusBar->setStatusText(i18n(_("Enables/disables the status bar")));

    diskOpen->setStatusText( i18n( _("Opens a disk") ) );
    streamOpen->setStatusText( i18n( _("Opens a network stream") ) );
    backward->setStatusText( i18n( _("Backward") ) );
    stop->setStatusText( i18n( _("Stops playback") ) );
    play->setStatusText( i18n( _("Starts playback") ) );
    pause->setStatusText( i18n( _("Pauses playback") ) );
    slow->setStatusText( i18n( _("Slow") ) );
    fast->setStatusText( i18n( _("Fast") ) );
    prev->setStatusText( i18n( _("Prev") ) );
    next->setStatusText( i18n( _("Next") ) );
    // use the absolute path to your ktestui.rc file for testing purpose in createGUI();
    char *psz_uifile = config_GetPsz( p_intf, "kde-uirc" );
    createGUI( psz_uifile );
//    createGUI( "./modules/gui/kde/ui.rc" );
}

void KInterface::initStatusBar()
{
  ///////////////////////////////////////////////////////////////////
  // STATUSBAR
  // TODO: add your own items you need for displaying current application status.
    statusBar()->insertItem(i18n(_("Ready.")), ID_STATUS_MSG, 1, false);
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

void KInterface::slotShowInfo()
{
    if ( p_intf->p_sys->p_input )
    {
        new KInfoWindow(p_intf, p_intf->p_sys->p_input);
    }
}

void KInterface::slotFileOpen()
{
    playlist_t *p_playlist;

    slotStatusMsg( i18n( _("Opening file...") ) );
    KURL url=KFileDialog::getOpenURL( QString::null,
            i18n( "*|All files" ), this, i18n( _("Open File...") ) );

    if( !url.isEmpty() )
    {
        p_playlist = (playlist_t *)
            vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
        if( p_playlist )
        {
            fileOpenRecent->addURL( url );
            playlist_Add( p_playlist, url.path(), url.path(), 
                          PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
            vlc_object_release( p_playlist );
        }
    }

    slotStatusMsg( i18n( _("Ready.") ) );
}

void KInterface::slotFileOpenRecent(const KURL& url)
{
  slotStatusMsg(i18n(_("Opening file...")));
  slotStatusMsg(i18n(_("Ready.")));
}

void KInterface::slotFileQuit()
{
    slotStatusMsg(i18n(_("Exiting...")));
    p_intf->p_vlc->b_die = VLC_TRUE;
    slotStatusMsg(i18n(_("Ready.")));
}

void KInterface::slotViewToolBar()
{
  slotStatusMsg(i18n(_("Toggling toolbar...")));
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

  slotStatusMsg(i18n(_("Ready.")));
}

void KInterface::slotViewStatusBar()
{
  slotStatusMsg(i18n(_("Toggle the status bar...")));
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

  slotStatusMsg(i18n(_("Ready.")));
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
//    p_intf->p_sys->p_app->processEvents();
    vlc_mutex_lock( &p_intf->change_lock );

    /* Update the input */
    if( p_intf->p_sys->p_input == NULL )
    {
        p_intf->p_sys->p_input = (input_thread_t *)
                vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_ANYWHERE );
        if ( p_intf->p_sys->p_input )
        {
            languages->setEnabled( true );
            subtitles->setEnabled( true );
            info->setEnabled( true );
        }
            
    }
    else if( p_intf->p_sys->p_input->b_dead )
    {
        vlc_object_release( p_intf->p_sys->p_input );
        p_intf->p_sys->p_input = NULL;
        languages->setEnabled( false );
        subtitles->setEnabled( false );
        info->setEnabled( false );
    }

    /* If the "display popup" flag has changed */
    if( p_intf->b_menu_change )
    {
        fTitleMenu->popup( ( QCursor::pos() ) );
        p_intf->b_menu_change = 0;
    }

    if( p_intf->p_sys->p_input )
    {
        input_thread_t *p_input = p_intf->p_sys->p_input;
                
        vlc_mutex_lock( &p_input->stream.stream_lock );
        if( !p_input->b_die )
        {
            /* New input or stream map change */
            if( p_input->stream.b_changed )
            {
                //            E_(GtkModeManage)( p_intf );
                //GtkSetupMenus( p_intf );
                slotUpdateLanguages();

                p_intf->p_sys->b_playing = 1;
                p_input->stream.b_changed = 0;
            }

            /* Manage the slider. fSlider->setValue triggers
             * slotSliderChanged which needs to grab the stream lock*/
#define p_area p_input->stream.p_selected_area
            if( p_area->i_size ) {
                vlc_mutex_unlock( &p_input->stream.stream_lock );
                fSlider->setValue( ( 10000 * p_area->i_tell )
                                   / p_area->i_size );
                vlc_mutex_lock( &p_input->stream.stream_lock );

            }
#undef p_area
            
            //         if( p_intf->p_sys->i_part !=
            //    p_input->stream.p_selected_area->i_part )
            //{
                //      p_intf->p_sys->b_chapter_update = 1;
                //GtkSetupMenus( p_intf );
            //}
        }
        vlc_mutex_unlock( &p_input->stream.stream_lock );

    }
    
    else if( p_intf->p_sys->b_playing && !p_intf->b_die )
    {
        //E_(GtkModeManage)( p_intf );
        p_intf->p_sys->b_playing = 0;
    }

    if( p_intf->b_die )
    {
        p_intf->p_sys->p_app->quit();
    }

    vlc_mutex_unlock( &p_intf->change_lock );
    msleep( 100 );

}

void KInterface::slotSliderMoved( int position )
{
    if( p_intf->p_sys->p_input )
    {
        // XXX is this locking really useful ?
        vlc_mutex_lock( &p_intf->change_lock );

        var_SetFloat( p_intf->p_sys->p_input, "position",
                       (double)position / 10000.0 );
        vlc_mutex_unlock( &p_intf->change_lock );
    }
}

void KInterface::slotUpdateLanguages()
{

    es_descriptor_t *   p_spu_es;
    es_descriptor_t *   p_audio_es;
    /* look for selected ES */
    p_audio_es = NULL;
    p_spu_es = NULL;

    for( unsigned int i = 0 ;
         i < p_intf->p_sys->p_input->stream.i_selected_es_number ;
         i++
        )
    {
        if( p_intf->p_sys->p_input->stream.pp_selected_es[i]->i_cat
            == AUDIO_ES )
        {
            p_audio_es = p_intf->p_sys->p_input->stream.pp_selected_es[i];
        }

        if( p_intf->p_sys->p_input->stream.pp_selected_es[i]->i_cat == SPU_ES )
        {
            p_spu_es = p_intf->p_sys->p_input->stream.pp_selected_es[i];
        }
    }
    languages->setEnabled( false );
    subtitles->setEnabled( false );
    languageCollection->clear();
    subtitleCollection->clear();
    languages->popupMenu()->clear();
    subtitles->popupMenu()->clear();
    /* audio menus */
    /* find audio root menu */
    languageMenus( languages, p_audio_es, AUDIO_ES );

    /* sub picture menus */
    /* find spu root menu */
    languageMenus( subtitles, p_spu_es, SPU_ES );

}


/*
 * called with stream lock
 */
void KInterface::languageMenus(KActionMenu *root, es_descriptor_t *p_es,
                          int i_cat)
{
    int i_item = 0;
    if ( i_cat != AUDIO_ES )
    {
        KLanguageMenuAction *p_item =
            new KLanguageMenuAction( p_intf, _( "Off" ), 0, this );
        subtitleCollection->insert( p_item );
        root->insert( p_item );
        root->insert( new KActionSeparator( this ) );
        p_item->setExclusiveGroup( QString().sprintf( "%d", i_cat ) );
        p_item->setChecked( p_es == 0 );
    }
    
#define ES p_intf->p_sys->p_input->stream.pp_es[i]
    /* create a set of language buttons and append them to the container */
    for( unsigned int i = 0 ;
         i < p_intf->p_sys->p_input->stream.i_es_number ;
         i++ )
    {
        if( ( ES->i_cat == i_cat ) &&
            ( !ES->p_pgrm ||
              ES->p_pgrm ==
                 p_intf->p_sys->p_input->stream.p_selected_program ) )
        {
            i_item++;
            QString name = p_intf->p_sys->p_input->stream.pp_es[i]->psz_desc;
            if( name.isEmpty() )
            {
                name.sprintf( "Language %d", i_item );
            }
            KLanguageMenuAction *p_item;
            if ( i_cat == AUDIO_ES )
            {
                p_item = new KLanguageMenuAction( p_intf, name, ES,
                                                  this );
                languageCollection->insert(p_item);
            }
            else
            {
                p_item = new KLanguageMenuAction( p_intf, name, ES,
                                                  this );
                subtitleCollection->insert(p_item);
            }
            p_item->setExclusiveGroup( QString().sprintf( "%d", i_cat ) );
            root->insert( p_item );
            
            if( p_es == p_intf->p_sys->p_input->stream.pp_es[i] )
            {
                /* don't lose p_item when we append into menu */
                //p_item_active = p_item;
                p_item->setChecked( true );
            }
            connect( p_item, SIGNAL( toggled( bool, es_descriptor_t * ) ),
                     this, SLOT( slotSetLanguage( bool, es_descriptor_t * ) ));

        }
    }

    root->setEnabled( true );
}


void KInterface::slotSetLanguage( bool on, es_descriptor_t *p_es )
{
    if( p_es )
        var_SetInteger( p_intf->p_sys->p_input, "audio-es", p_es->i_id );
    else
        var_SetInteger( p_intf->p_sys->p_input, "audio-es", -1 );
}

void KInterface::slotSliderChanged( int position )
{
    if( p_intf->p_sys->p_input != NULL )
    {
        char psz_time[ MSTRTIME_MAX_SIZE ];
        int64_t i_seconds;

        i_seconds = var_GetTime( p_intf->p_sys->p_input, "time" ) / I64C(1000000 );
        secstotimestr( psz_time, i_seconds );

        statusBar()->changeItem( psz_time, ID_DATE );
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
            playlist_Add( p_playlist, source.latin1(), source.latin1(),
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
            playlist_Add( p_playlist, source.latin1(), source.latin1(),
                          PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
            vlc_object_release( p_playlist );
        }
    }
}

void KInterface::slotPlay()
{
    if( p_intf->p_sys->p_input )
    {
        var_SetInteger( p_intf->p_sys->p_input, "state", PLAYING_S );
    }
}

void KInterface::slotPause()
{
    if ( p_intf->p_sys->p_input )
    {
        var_SetInteger( p_intf->p_sys->p_input, "state", PAUSE_S );
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
        var_SetVoid( p_intf->p_sys->p_input, "rate-slower" );
    }
}

void KInterface::slotFast()
{
    if( p_intf->p_sys->p_input != NULL )
    {
        var_SetVoid( p_intf->p_sys->p_input, "rate-faster" );
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
                playlist_Add( p_playlist, (*i).path(), (*i).path(),
                          PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
            }
        }
    }

    vlc_object_release( p_playlist );
}
