/*****************************************************************************
 * InterfaceWindow.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: InterfaceWindow.cpp,v 1.27 2003/02/03 17:18:48 stippi Exp $
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Tony Castley <tony@castley.net>
 *          Richard Shepherd <richard@rshepherd.demon.co.uk>
 *          Stephan Aßmus <stippi@yellowbites.com>
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

/* System headers */
#include <kernel/OS.h>
#include <InterfaceKit.h>
#include <AppKit.h>
#include <StorageKit.h>
#include <SupportKit.h>
#include <malloc.h>
#include <scsi.h>
#include <scsiprobe_driver.h>
#include <fs_info.h>
#include <string.h>

/* VLC headers */
#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/intf.h>

/* BeOS interface headers */
#include "VlcWrapper.h"
#include "MsgVals.h"
#include "MediaControlView.h"
#include "PlayListWindow.h"
#include "PreferencesWindow.h"
#include "MessagesWindow.h"
#include "InterfaceWindow.h"

#define INTERFACE_UPDATE_TIMEOUT 80000 // 2 frames if at 25 fps
#define INTERFACE_LOCKING_TIMEOUT 5000

// make_sure_frame_is_on_screen
bool
make_sure_frame_is_on_screen( BRect& frame )
{
	BScreen screen( B_MAIN_SCREEN_ID );
	if (frame.IsValid() && screen.IsValid()) {
		if (!screen.Frame().Contains(frame)) {
			// make sure frame fits in the screen
			if (frame.Width() > screen.Frame().Width())
				frame.right -= frame.Width() - screen.Frame().Width() + 10.0;
			if (frame.Height() > screen.Frame().Height())
				frame.bottom -= frame.Height() - screen.Frame().Height() + 30.0;
			// frame is now at the most the size of the screen
			if (frame.right > screen.Frame().right)
				frame.OffsetBy(-(frame.right - screen.Frame().right), 0.0);
			if (frame.bottom > screen.Frame().bottom)
				frame.OffsetBy(0.0, -(frame.bottom - screen.Frame().bottom));
			if (frame.left < screen.Frame().left)
				frame.OffsetBy((screen.Frame().left - frame.left), 0.0);
			if (frame.top < screen.Frame().top)
				frame.OffsetBy(0.0, (screen.Frame().top - frame.top));
		}
		return true;
	}
	return false;
}

// make_sure_frame_is_within_limits
void
make_sure_frame_is_within_limits( BRect& frame, float minWidth, float minHeight,
                                  float maxWidth, float maxHeight )
{
    if ( frame.Width() < minWidth )
        frame.right = frame.left + minWidth;
    if ( frame.Height() < minHeight )
        frame.bottom = frame.top + minHeight;
    if ( frame.Width() > maxWidth )
        frame.right = frame.left + maxWidth;
    if ( frame.Height() > maxHeight )
        frame.bottom = frame.top + maxHeight;
}

// get_volume_info
bool
get_volume_info( BVolume& volume, BString& volumeName, bool& isCDROM, BString& deviceName )
{
	bool success = false;
	isCDROM = false;
	deviceName = "";
	volumeName = "";
	char name[B_FILE_NAME_LENGTH];
	if ( volume.GetName( name ) >= B_OK )	// disk is currently mounted
	{
		volumeName = name;
		dev_t dev = volume.Device();
		fs_info info;
		if ( fs_stat_dev( dev, &info ) == B_OK )
		{
			success = true;
			deviceName = info.device_name;
			if ( volume.IsReadOnly() )
			{
				int i_dev = open( info.device_name, O_RDONLY );
				if ( i_dev >= 0 )
				{
					device_geometry g;
					if ( ioctl( i_dev, B_GET_GEOMETRY, &g, sizeof( g ) ) >= 0 )
						isCDROM = ( g.device_type == B_CD );
					close( i_dev );
				}
			}
		}
 	}
 	return success;
}

// collect_folder_contents
void
collect_folder_contents( BDirectory& dir, BList& list, bool& deep, bool& asked, BEntry& entry )
{
	while ( dir.GetNextEntry( &entry, true ) == B_OK )
	{
		if ( !entry.IsDirectory() )
		{
			BPath path;
			// since the directory will give us the entries in reverse order,
			// we put them each at the same index, effectively reversing the
			// items while adding them
			if ( entry.GetPath( &path ) == B_OK )
			{
				BString* string = new BString( path.Path() );
				if ( !list.AddItem( string, 0 ) )
					delete string;	// at least don't leak
			}
		}
		else
		{
			if ( !asked )
			{
				// ask user if we should parse sub-folders as well
				BAlert* alert = new BAlert( "sub-folders?",
											"Open files from all sub-folders as well?",
											"No", "Yes", NULL, B_WIDTH_AS_USUAL,
											B_IDEA_ALERT );
				int32 buttonIndex = alert->Go();
				deep = buttonIndex == 1;
				asked = true;
				// never delete BAlerts!!
			}
			if ( deep )
			{
				BDirectory subDir( &entry );
				if ( subDir.InitCheck() == B_OK )
					collect_folder_contents( subDir, list,
											 deep, asked, entry );
			}
		}
	}
}


/*****************************************************************************
 * InterfaceWindow
 *****************************************************************************/

InterfaceWindow::InterfaceWindow( BRect frame, const char* name,
                                  intf_thread_t* p_interface )
    : BWindow( frame, name, B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
               B_NOT_ZOOMABLE | B_WILL_ACCEPT_FIRST_CLICK | B_ASYNCHRONOUS_CONTROLS ),
      p_intf( p_interface ),
      fFilePanel( NULL ),
      fLastUpdateTime( system_time() ),
	  fSettings( new BMessage( 'sett' ) ),
	  p_wrapper( p_intf->p_sys->p_wrapper )
{
	// TODO: ?!? what about user settings?
    p_intf->p_sys->b_dvdmenus = false;
    
    fPlaylistIsEmpty = !( p_wrapper->PlaylistSize() > 0 );
    
    BScreen screen;
    BRect screen_rect = screen.Frame();
    BRect window_rect;
    window_rect.Set( ( screen_rect.right - PREFS_WINDOW_WIDTH ) / 2,
                     ( screen_rect.bottom - PREFS_WINDOW_HEIGHT ) / 2,
                     ( screen_rect.right + PREFS_WINDOW_WIDTH ) / 2,
                     ( screen_rect.bottom + PREFS_WINDOW_HEIGHT ) / 2 );
    fPreferencesWindow = new PreferencesWindow( p_intf, window_rect, "Preferences" );
    window_rect.Set( screen_rect.right - 500,
                     screen_rect.top + 50,
                     screen_rect.right - 150,
                     screen_rect.top + 250 );
    fPlaylistWindow = new PlayListWindow( window_rect, "Playlist", this, p_intf );
    window_rect.Set( screen_rect.right - 500,
                     screen_rect.top + 300,
                     screen_rect.right - 150,
                     screen_rect.top + 600 );
    fMessagesWindow = new MessagesWindow( p_intf, window_rect, "Messages" );

    // set the title bar
    SetName( "interface" );
    SetTitle( VOUT_TITLE );

    // the media control view
    p_mediaControl = new MediaControlView( BRect( 0.0, 0.0, 250.0, 50.0 ),
                                           p_intf );
    p_mediaControl->SetViewColor( ui_color( B_PANEL_BACKGROUND_COLOR ) );

    float width, height;
    p_mediaControl->GetPreferredSize( &width, &height );

    // set up the main menu
    fMenuBar = new BMenuBar( BRect(0.0, 0.0, width, 15.0), "main menu",
                             B_FOLLOW_NONE, B_ITEMS_IN_ROW, false );

    // make menu bar resize to correct height
    float menuWidth, menuHeight;
    fMenuBar->GetPreferredSize( &menuWidth, &menuHeight );
    fMenuBar->ResizeTo( width, menuHeight );    // don't change! it's a workarround!
    // take care of proper size for ourself
    height += fMenuBar->Bounds().Height();
    ResizeTo( width, height );

    p_mediaControl->MoveTo( fMenuBar->Bounds().LeftBottom() + BPoint(0.0, 1.0) );
    AddChild( fMenuBar );
    AddChild( p_mediaControl );

    // Add the file Menu
    BMenu* fileMenu = new BMenu( "File" );
    fMenuBar->AddItem( fileMenu );
    fileMenu->AddItem( new BMenuItem( "Open File" B_UTF8_ELLIPSIS,
                                      new BMessage( OPEN_FILE ), 'O') );
    
    fileMenu->AddItem( new CDMenu( "Open Disc" ) );

    fileMenu->AddItem( new BMenuItem( "Open Subtitles" B_UTF8_ELLIPSIS,
                                      new BMessage( LOAD_SUBFILE ) ) );
    
    fileMenu->AddSeparatorItem();
    BMenuItem* item = new BMenuItem( "About" B_UTF8_ELLIPSIS,
                                     new BMessage( B_ABOUT_REQUESTED ), 'A');
    item->SetTarget( be_app );
    fileMenu->AddItem( item );
    fileMenu->AddItem( new BMenuItem( "Quit", new BMessage( B_QUIT_REQUESTED ), 'Q') );

    fLanguageMenu = new LanguageMenu("Language", AUDIO_ES, p_wrapper);
    fSubtitlesMenu = new LanguageMenu("Subtitles", SPU_ES, p_wrapper);

    /* Add the Audio menu */
    fAudioMenu = new BMenu( "Audio" );
    fMenuBar->AddItem ( fAudioMenu );
    fAudioMenu->AddItem( fLanguageMenu );
    fAudioMenu->AddItem( fSubtitlesMenu );

    fPrevTitleMI = new BMenuItem( "Prev Title", new BMessage( PREV_TITLE ) );
    fNextTitleMI = new BMenuItem( "Next Title", new BMessage( NEXT_TITLE ) );
    fPrevChapterMI = new BMenuItem( "Prev Chapter", new BMessage( PREV_CHAPTER ) );
    fNextChapterMI = new BMenuItem( "Next Chapter", new BMessage( NEXT_CHAPTER ) );

    /* Add the Navigation menu */
    fNavigationMenu = new BMenu( "Navigation" );
    fMenuBar->AddItem( fNavigationMenu );
    fNavigationMenu->AddItem( fPrevTitleMI );
    fNavigationMenu->AddItem( fNextTitleMI );
    fNavigationMenu->AddItem( fTitleMenu = new TitleMenu( "Go to Title", p_intf ) );
    fNavigationMenu->AddSeparatorItem();
    fNavigationMenu->AddItem( fPrevChapterMI );
    fNavigationMenu->AddItem( fNextChapterMI );
    fNavigationMenu->AddItem( fChapterMenu = new ChapterMenu( "Go to Chapter", p_intf ) );

    /* Add the Speed menu */
    fSpeedMenu = new BMenu( "Speed" );
    fSpeedMenu->SetRadioMode( true );
    fSpeedMenu->AddItem( fSlowerMI = new BMenuItem( "Slower", new BMessage( SLOWER_PLAY ) ) );
    fNormalMI = new BMenuItem( "Normal", new BMessage( NORMAL_PLAY ) );
    fNormalMI->SetMarked(true); // default to normal speed
    fSpeedMenu->AddItem( fNormalMI );
    fSpeedMenu->AddItem( fFasterMI = new BMenuItem( "Faster", new BMessage( FASTER_PLAY) ) );
    fSpeedMenu->SetTargetForItems( this );
    fMenuBar->AddItem( fSpeedMenu );

    /* Add the Show menu */
    fShowMenu = new BMenu( "Window" );
    fShowMenu->AddItem( new BMenuItem( "Play List" B_UTF8_ELLIPSIS,
                                       new BMessage( OPEN_PLAYLIST ), 'P') );
    fShowMenu->AddItem( new BMenuItem( "Messages" B_UTF8_ELLIPSIS,
                                       new BMessage( OPEN_MESSAGES ), 'M' ) );
    fShowMenu->AddItem( new BMenuItem( "Settings" B_UTF8_ELLIPSIS,
                                       new BMessage( OPEN_PREFERENCES ), 'S' ) );
    fMenuBar->AddItem( fShowMenu );                            

    /* Prepare fow showing */
    _SetMenusEnabled( false );
    p_mediaControl->SetEnabled( false );

	_RestoreSettings();
    
/*    // Restore interface settings
	// main window size and position
    int i_width = config_GetInt( p_intf, "beos-intf-width" ),
        i_height = config_GetInt( p_intf, "beos-intf-height" ),
        i_xpos = config_GetInt( p_intf, "beos-intf-xpos" ),
        i_ypos = config_GetInt( p_intf, "beos-intf-ypos" );
    if( i_width > 20 && i_height > 20 && i_xpos >= 0 && i_ypos >= 0 )
    {
    	BRect r( i_xpos, i_ypos, i_xpos + i_width, i_ypos + i_height );

		float minWidth, maxWidth, minHeight, maxHeight;
		GetSizeLimits( &minWidth, &maxWidth, &minHeight, &maxHeight );

		make_sure_frame_is_within_limits( r, minWidth, minHeight, maxWidth, maxHeight );
		if ( make_sure_frame_is_on_screen( r ) )
		{
	        ResizeTo( r.Width(), r.Height() );
	        MoveTo( r.LeftTop() );
		}
    }
	// playlist window size and position
    i_width = config_GetInt( p_intf, "beos-playlist-width" ),
    i_height = config_GetInt( p_intf, "beos-playlist-height" ),
    i_xpos = config_GetInt( p_intf, "beos-playlist-xpos" ),
    i_ypos = config_GetInt( p_intf, "beos-playlist-ypos" );
    if( i_width > 20 && i_height > 20 && i_xpos >= 0 && i_ypos >= 0 )
    {
    	BRect r( i_xpos, i_ypos, i_xpos + i_width, i_ypos + i_height );

		float minWidth, maxWidth, minHeight, maxHeight;
		fPlaylistWindow->GetSizeLimits( &minWidth, &maxWidth, &minHeight, &maxHeight );

		make_sure_frame_is_within_limits( r, minWidth, minHeight, maxWidth, maxHeight );
		if ( make_sure_frame_is_on_screen( r ) )
		{
	        fPlaylistWindow->ResizeTo( r.Width(), r.Height() );
	        fPlaylistWindow->MoveTo( r.LeftTop() );
		}
    }
    // child windows are not running yet, that's why we aint locking them
    // playlist showing
    // messages window size and position
    i_width = config_GetInt( p_intf, "beos-messages-width" ),
    i_height = config_GetInt( p_intf, "beos-messages-height" ),
    i_xpos = config_GetInt( p_intf, "beos-messages-xpos" ),
    i_ypos = config_GetInt( p_intf, "beos-messages-ypos" );
    if( i_width && i_height && i_xpos && i_ypos )
    {
    	BRect r( i_xpos, i_ypos, i_xpos + i_width, i_ypos + i_height );

		float minWidth, maxWidth, minHeight, maxHeight;
		fMessagesWindow->GetSizeLimits( &minWidth, &maxWidth, &minHeight, &maxHeight );

		make_sure_frame_is_within_limits( r, minWidth, minHeight, maxWidth, maxHeight );
		if ( make_sure_frame_is_on_screen( r ) )
		{
	        fMessagesWindow->ResizeTo( r.Width(), r.Height() );
	        fMessagesWindow->MoveTo( r.LeftTop() );
		}
    }
    if( config_GetInt( p_intf, "beos-playlist-show" ) )
    {
		fPlaylistWindow->Show();
    }
    else
    {
		fPlaylistWindow->Hide();
		fPlaylistWindow->Show();
    }
	// messages window size and position
    i_width = config_GetInt( p_intf, "beos-messages-width" ),
    i_height = config_GetInt( p_intf, "beos-messages-height" ),
    i_xpos = config_GetInt( p_intf, "beos-messages-xpos" ),
    i_ypos = config_GetInt( p_intf, "beos-messages-ypos" );
    if( i_width > 20 && i_height > 20 && i_xpos >= 0 && i_ypos >= 0 )
    {
    	BRect r( i_xpos, i_ypos, i_xpos + i_width, i_ypos + i_height );
		float minWidth, maxWidth, minHeight, maxHeight;
		fMessagesWindow->GetSizeLimits( &minWidth, &maxWidth, &minHeight, &maxHeight );

		make_sure_frame_is_within_limits( r, minWidth, minHeight, maxWidth, maxHeight );
		if ( make_sure_frame_is_on_screen( r ) )
		{
	        fMessagesWindow->ResizeTo( r.Width(), r.Height() );
	        fMessagesWindow->MoveTo( r.LeftTop() );
		}
    }
    // messages showing
    if( config_GetInt( p_intf, "beos-messages-show" ) )
    {
		fMessagesWindow->Show();
    }
    else
    {
		fMessagesWindow->Hide();
		fMessagesWindow->Show();
    }*/
    
    Show();
}

InterfaceWindow::~InterfaceWindow()
{
    if( fPlaylistWindow )
        fPlaylistWindow->ReallyQuit();
    fPlaylistWindow = NULL;
    if( fMessagesWindow )
        fMessagesWindow->ReallyQuit();
    fMessagesWindow = NULL;
	delete fFilePanel;
	delete fSettings;
}

/*****************************************************************************
 * InterfaceWindow::FrameResized
 *****************************************************************************/
void
InterfaceWindow::FrameResized(float width, float height)
{
    BRect r(Bounds());
    fMenuBar->MoveTo(r.LeftTop());
    fMenuBar->ResizeTo(r.Width(), fMenuBar->Bounds().Height());
    r.top += fMenuBar->Bounds().Height() + 1.0;
    p_mediaControl->MoveTo(r.LeftTop());
    p_mediaControl->ResizeTo(r.Width(), r.Height());
}

/*****************************************************************************
 * InterfaceWindow::MessageReceived
 *****************************************************************************/
void InterfaceWindow::MessageReceived( BMessage * p_message )
{
    int playback_status;      // remember playback state
    playback_status = p_wrapper->InputStatus();

    switch( p_message->what )
    {
        case B_ABOUT_REQUESTED:
        {
            BAlert* alert = new BAlert( VOUT_TITLE,
                                        "BeOS " VOUT_TITLE "\n\n<www.videolan.org>", "Ok");
            alert->Go();
            break;
        }
        case TOGGLE_ON_TOP:
            break;
            
        case OPEN_FILE:
        	_ShowFilePanel( B_REFS_RECEIVED, "VideoLAN Client: Open Media Files" );
            break;

        case LOAD_SUBFILE:
        	_ShowFilePanel( SUBFILE_RECEIVED, "VideoLAN Client: Open Subtitle File" );
            break;

        case OPEN_PLAYLIST:
            if (fPlaylistWindow->Lock())
            {
                if (fPlaylistWindow->IsHidden())
                    fPlaylistWindow->Show();
                else
                    fPlaylistWindow->Activate();
                fPlaylistWindow->Unlock();
            }
            break;
        case OPEN_DVD:
            {
                const char *psz_device;
                BString type( "dvd" );
                if( p_message->FindString( "device", &psz_device ) == B_OK )
                {
                    BString device( psz_device );
                    p_wrapper->OpenDisc( type, device, 0, 0 );
                }
                _UpdatePlaylist();
            }
            break;
        
        case SUBFILE_RECEIVED:
        {
            entry_ref ref;
            if( p_message->FindRef( "refs", 0, &ref ) == B_OK )
            {
                BPath path( &ref );
                if ( path.InitCheck() == B_OK )
                    p_wrapper->LoadSubFile( path.Path() );
            }
            break;
        }
    
        case STOP_PLAYBACK:
            // this currently stops playback not nicely
            if (playback_status > UNDEF_S)
            {
                p_wrapper->PlaylistStop();
                p_mediaControl->SetStatus(NOT_STARTED_S, DEFAULT_RATE);
            }
            break;
    
        case START_PLAYBACK:
            /*  starts playing in normal mode */
    
        case PAUSE_PLAYBACK:
            /* toggle between pause and play */
            if (playback_status > UNDEF_S)
            {
                /* pause if currently playing */
                if ( playback_status == PLAYING_S )
                {
                    p_wrapper->PlaylistPause();
                }
                else
                {
                    p_wrapper->PlaylistPlay();
                }
            }
            else
            {
                /* Play a new file */
                p_wrapper->PlaylistPlay();
            }    
            break;
    
        case FASTER_PLAY:
            /* cycle the fast playback modes */
            if (playback_status > UNDEF_S)
            {
                p_wrapper->InputFaster();
            }
            break;
    
        case SLOWER_PLAY:
            /*  cycle the slow playback modes */
            if (playback_status > UNDEF_S)
            {
                p_wrapper->InputSlower();
            }
            break;
    
        case NORMAL_PLAY:
            /*  restore speed to normal if already playing */
            if (playback_status > UNDEF_S)
            {
                p_wrapper->PlaylistPlay();
            }
            break;
    
        case SEEK_PLAYBACK:
            /* handled by semaphores */
            break;
        // volume related messages
        case VOLUME_CHG:
            /* adjust the volume */
            if (playback_status > UNDEF_S)
            {
                p_wrapper->SetVolume( p_mediaControl->GetVolume() );
                p_mediaControl->SetMuted( p_wrapper->IsMuted() );
            }
            break;
    
        case VOLUME_MUTE:
            // toggle muting
            if( p_wrapper->IsMuted() )
                p_wrapper->VolumeRestore();
            else
                p_wrapper->VolumeMute();
            p_mediaControl->SetMuted( p_wrapper->IsMuted() );
            break;
    
        case SELECT_CHANNEL:
            if ( playback_status > UNDEF_S )
            {
                int32 channel;
                if ( p_message->FindInt32( "channel", &channel ) == B_OK )
                {
                    p_wrapper->ToggleLanguage( channel );
                }
            }
            break;
    
        case SELECT_SUBTITLE:
            if ( playback_status > UNDEF_S )
            {
                int32 subtitle;
                if ( p_message->FindInt32( "subtitle", &subtitle ) == B_OK )
                     p_wrapper->ToggleSubtitle( subtitle );
            }
            break;
    
        // specific navigation messages
        case PREV_TITLE:
        {
            p_wrapper->PrevTitle();
            break;
        }
        case NEXT_TITLE:
        {
            p_wrapper->NextTitle();
            break;
        }
        case TOGGLE_TITLE:
            if ( playback_status > UNDEF_S )
            {
                int32 index;
                if( p_message->FindInt32( "index", &index ) == B_OK )
                    p_wrapper->ToggleTitle( index );
            }
            break;
        case PREV_CHAPTER:
        {
            p_wrapper->PrevChapter();
            break;
        }
        case NEXT_CHAPTER:
        {
            p_wrapper->NextChapter();
            break;
        }
        case TOGGLE_CHAPTER:
            if ( playback_status > UNDEF_S )
            {
                int32 index;
                if( p_message->FindInt32( "index", &index ) == B_OK )
                    p_wrapper->ToggleChapter( index );
            }
            break;
        case PREV_FILE:
            p_wrapper->PlaylistPrev();
            break;
        case NEXT_FILE:
            p_wrapper->PlaylistNext();
            break;
        // general next/prev functionality (skips to whatever makes most sense)
        case NAVIGATE_PREV:
            p_wrapper->NavigatePrev();
            break;
        case NAVIGATE_NEXT:
            p_wrapper->NavigateNext();
            break;
        // drag'n'drop and system messages
        case MSG_SOUNDPLAY:
        	// convert soundplay drag'n'drop message (containing paths)
        	// to normal message (containing refs)
        	{
	        	const char* path;
	        	for ( int32 i = 0; p_message->FindString( "path", i, &path ) == B_OK; i++ )
	        	{
	        		entry_ref ref;
	        		if ( get_ref_for_path( path, &ref ) == B_OK )
		        		p_message->AddRef( "refs", &ref );
	        	}
        	}
        	// fall through
        case B_REFS_RECEIVED:
        case B_SIMPLE_DATA:
            {
                /* file(s) opened by the File menu -> append to the playlist;
                 * file(s) opened by drag & drop -> replace playlist;
                 * file(s) opened by 'shift' + drag & drop -> append */
                bool replace = false;
                bool reverse = false;
                if ( p_message->WasDropped() )
                {
                    replace = !( modifiers() & B_SHIFT_KEY );
                    reverse = true;
                }
                    
                // build list of files to be played from message contents
                entry_ref ref;
                BList files;
                
                // if we should parse sub-folders as well
           		bool askedAlready = false;
           		bool parseSubFolders = askedAlready;
           		// traverse refs in reverse order
           		int32 count;
           		type_code dummy;
           		if ( p_message->GetInfo( "refs", &dummy, &count ) == B_OK && count > 0 )
           		{
           			int32 i = reverse ? count - 1 : 0;
           			int32 increment = reverse ? -1 : 1;
	                for ( ; p_message->FindRef( "refs", i, &ref ) == B_OK; i += increment )
	                {
	                    BPath path( &ref );
	                    if ( path.InitCheck() == B_OK )
	                    {
	                        bool add = true;
	                        // has the user dropped a folder?
	                        BDirectory dir( &ref );
	                        if ( dir.InitCheck() == B_OK)
	                        {
		                        // has the user dropped a dvd disk icon?
								if ( dir.IsRootDirectory() )
								{
									BVolumeRoster volRoster;
									BVolume vol;
									BDirectory volumeRoot;
									status_t status = volRoster.GetNextVolume( &vol );
									while ( status == B_NO_ERROR )
									{
										if ( vol.GetRootDirectory( &volumeRoot ) == B_OK
											 && dir == volumeRoot )
										{
											BString volumeName;
											BString deviceName;
											bool isCDROM;
											if ( get_volume_info( vol, volumeName, isCDROM, deviceName )
												 && isCDROM )
											{
												BMessage msg( OPEN_DVD );
												msg.AddString( "device", deviceName.String() );
												PostMessage( &msg );
												add = false;
											}
									 		break;
										}
										else
										{
									 		vol.Unset();
											status = volRoster.GetNextVolume( &vol );
										}
									}
								}
	                        	if ( add )
	                        	{
	                        		add = false;
	                        		dir.Rewind();	// defensive programming
	                        		BEntry entry;
									collect_folder_contents( dir, files,
															 parseSubFolders,
															 askedAlready,
															 entry );
	                        	}
	                        }
	                        if ( add )
	                        {
	                        	BString* string = new BString( path.Path() );
	                        	if ( !files.AddItem( string, 0 ) )
	                        		delete string;	// at least don't leak
	                        }
	                    }
	                }
	                // give the list to VLC
	                // BString objects allocated here will be deleted there
	                int32 index;
	                if ( p_message->FindInt32("drop index", &index) != B_OK )
	                	index = -1;
	                p_wrapper->OpenFiles( &files, replace, index );
	                _UpdatePlaylist();
           		}
            }
            break;

        case OPEN_PREFERENCES:
        {
            if( fPreferencesWindow->Lock() )
            {
                if (fPreferencesWindow->IsHidden())
                    fPreferencesWindow->Show();
                else
                    fPreferencesWindow->Activate();
                fPreferencesWindow->Unlock();
            }
            break;
        }

        case OPEN_MESSAGES:
        {
            if( fMessagesWindow->Lock() )
            {
                if (fMessagesWindow->IsHidden())
                    fMessagesWindow->Show();
                else
                    fMessagesWindow->Activate();
                fMessagesWindow->Unlock();
            }
            break;
        }
        case MSG_UPDATE:
        	UpdateInterface();
        	break;
        default:
            BWindow::MessageReceived( p_message );
            break;
    }

}

/*****************************************************************************
 * InterfaceWindow::QuitRequested
 *****************************************************************************/
bool InterfaceWindow::QuitRequested()
{
    p_wrapper->PlaylistStop();
    p_mediaControl->SetStatus(NOT_STARTED_S, DEFAULT_RATE);

    /* Save interface settings */
/*    BRect frame = Frame();
    config_PutInt( p_intf, "beos-intf-width", (int)frame.Width() );
    config_PutInt( p_intf, "beos-intf-height", (int)frame.Height() );
    config_PutInt( p_intf, "beos-intf-xpos", (int)frame.left );
    config_PutInt( p_intf, "beos-intf-ypos", (int)frame.top );
    if( fPlaylistWindow->Lock() )
    {
        frame = fPlaylistWindow->Frame();
        config_PutInt( p_intf, "beos-playlist-width", (int)frame.Width() );
        config_PutInt( p_intf, "beos-playlist-height", (int)frame.Height() );
        config_PutInt( p_intf, "beos-playlist-xpos", (int)frame.left );
        config_PutInt( p_intf, "beos-playlist-ypos", (int)frame.top );
        config_PutInt( p_intf, "beos-playlist-show", !fPlaylistWindow->IsHidden() );
        fPlaylistWindow->Unlock();
    }
    if( fMessagesWindow->Lock() )
    {
        frame = fMessagesWindow->Frame();
        config_PutInt( p_intf, "beos-messages-width", (int)frame.Width() );
        config_PutInt( p_intf, "beos-messages-height", (int)frame.Height() );
        config_PutInt( p_intf, "beos-messages-xpos", (int)frame.left );
        config_PutInt( p_intf, "beos-messages-ypos", (int)frame.top );
        config_PutInt( p_intf, "beos-messages-show", !fMessagesWindow->IsHidden() );
        fMessagesWindow->Unlock();
    }*/
    config_SaveConfigFile( p_intf, "beos" );
 	_StoreSettings();
   
    p_intf->b_die = 1;

    return( true );
}

/*****************************************************************************
 * InterfaceWindow::UpdateInterface
 *****************************************************************************/
void InterfaceWindow::UpdateInterface()
{
    if( p_wrapper->HasInput() )
    {
        if ( acquire_sem( p_mediaControl->fScrubSem ) == B_OK )
        {
            p_wrapper->SetTimeAsFloat( p_mediaControl->GetSeekTo() );
        }
        else if ( LockWithTimeout( INTERFACE_LOCKING_TIMEOUT ) == B_OK )
        {
            p_mediaControl->SetEnabled( true );
            bool hasTitles = p_wrapper->HasTitles();
            bool hasChapters = p_wrapper->HasChapters();
            p_mediaControl->SetStatus( p_wrapper->InputStatus(), 
                                       p_wrapper->InputRate() );
            p_mediaControl->SetProgress( p_wrapper->GetTimeAsFloat() );
            _SetMenusEnabled( true, hasChapters, hasTitles );

            _UpdateSpeedMenu( p_wrapper->InputRate() );

            // enable/disable skip buttons
            bool canSkipPrev;
            bool canSkipNext;
            p_wrapper->GetNavCapabilities( &canSkipPrev, &canSkipNext );
            p_mediaControl->SetSkippable( canSkipPrev, canSkipNext );

            if ( p_wrapper->HasAudio() )
            {
                p_mediaControl->SetAudioEnabled( true );
                p_mediaControl->SetMuted( p_wrapper->IsMuted() );
            } else
                p_mediaControl->SetAudioEnabled( false );

            Unlock();
        }
        // update playlist as well
        if ( fPlaylistWindow->LockWithTimeout( INTERFACE_LOCKING_TIMEOUT ) == B_OK )
        {
            fPlaylistWindow->UpdatePlaylist();
            fPlaylistWindow->Unlock();
        }
    }
    else
    {
		if ( LockWithTimeout(INTERFACE_LOCKING_TIMEOUT) == B_OK )
		{
	        _SetMenusEnabled( false );
	        if( !( p_wrapper->PlaylistSize() > 0 ) )
	            p_mediaControl->SetEnabled( false );
	        else
	        {
	            p_mediaControl->SetProgress( 0 );
	            // enable/disable skip buttons
	            bool canSkipPrev;
	            bool canSkipNext;
	            p_wrapper->GetNavCapabilities( &canSkipPrev, &canSkipNext );
	            p_mediaControl->SetSkippable( canSkipPrev, canSkipNext );
			}
            Unlock();
        }
    }

    /* always force the user-specified volume */
    /* FIXME : I'm quite sure there is a cleaner way to do this */
    int i_volume = p_mediaControl->GetVolume();
    if( p_wrapper->GetVolume() != i_volume )
    {
        p_wrapper->SetVolume( i_volume );
    }

	// strangly, someone is calling this function even after the object has been destructed!
	// even more strangly, this workarround seems to work
	if (fMessagesWindow)
	    fMessagesWindow->UpdateMessages();

    fLastUpdateTime = system_time();
}

/*****************************************************************************
 * InterfaceWindow::IsStopped
 *****************************************************************************/
bool
InterfaceWindow::IsStopped() const
{
    return (system_time() - fLastUpdateTime > INTERFACE_UPDATE_TIMEOUT);
}

/*****************************************************************************
 * InterfaceWindow::_UpdatePlaylist
 *****************************************************************************/
void
InterfaceWindow::_UpdatePlaylist()
{
    if ( fPlaylistWindow->Lock() )
    {
        fPlaylistWindow->UpdatePlaylist( true );
        fPlaylistWindow->Unlock();
        p_mediaControl->SetEnabled( p_wrapper->PlaylistSize() );
    }
}

/*****************************************************************************
 * InterfaceWindow::_SetMenusEnabled
 *****************************************************************************/
void
InterfaceWindow::_SetMenusEnabled(bool hasFile, bool hasChapters, bool hasTitles)
{
    if (!hasFile)
    {
        hasChapters = false;
        hasTitles = false;
    }
    if ( LockWithTimeout( INTERFACE_LOCKING_TIMEOUT ) == B_OK)
    {
        if ( fNextChapterMI->IsEnabled() != hasChapters )
             fNextChapterMI->SetEnabled( hasChapters );
        if ( fPrevChapterMI->IsEnabled() != hasChapters )
             fPrevChapterMI->SetEnabled( hasChapters );
        if ( fChapterMenu->IsEnabled() != hasChapters )
             fChapterMenu->SetEnabled( hasChapters );
        if ( fNextTitleMI->IsEnabled() != hasTitles )
             fNextTitleMI->SetEnabled( hasTitles );
        if ( fPrevTitleMI->IsEnabled() != hasTitles )
             fPrevTitleMI->SetEnabled( hasTitles );
        if ( fTitleMenu->IsEnabled() != hasTitles )
             fTitleMenu->SetEnabled( hasTitles );
        if ( fAudioMenu->IsEnabled() != hasFile )
             fAudioMenu->SetEnabled( hasFile );
        if ( fNavigationMenu->IsEnabled() != hasFile )
             fNavigationMenu->SetEnabled( hasFile );
        if ( fLanguageMenu->IsEnabled() != hasFile )
             fLanguageMenu->SetEnabled( hasFile );
        if ( fSubtitlesMenu->IsEnabled() != hasFile )
             fSubtitlesMenu->SetEnabled( hasFile );
        if ( fSpeedMenu->IsEnabled() != hasFile )
             fSpeedMenu->SetEnabled( hasFile );
        Unlock();
    }
}

/*****************************************************************************
 * InterfaceWindow::_UpdateSpeedMenu
 *****************************************************************************/
void
InterfaceWindow::_UpdateSpeedMenu( int rate )
{
    if ( rate == DEFAULT_RATE )
    {
        if ( !fNormalMI->IsMarked() )
            fNormalMI->SetMarked( true );
    }
    else if ( rate < DEFAULT_RATE )
    {
        if ( !fFasterMI->IsMarked() )
            fFasterMI->SetMarked( true );
    }
    else
    {
        if ( !fSlowerMI->IsMarked() )
            fSlowerMI->SetMarked( true );
    }
}

/*****************************************************************************
 * InterfaceWindow::_InputStreamChanged
 *****************************************************************************/
void
InterfaceWindow::_InputStreamChanged()
{
    // TODO: move more stuff from updateInterface() here!
    snooze( 400000 );
    p_wrapper->SetVolume( p_mediaControl->GetVolume() );
}

/*****************************************************************************
 * InterfaceWindow::_ShowFilePanel
 *****************************************************************************/
void
InterfaceWindow::_ShowFilePanel( uint32 command, const char* windowTitle )
{
	if( !fFilePanel )
	{
		fFilePanel = new BFilePanel( B_OPEN_PANEL, NULL, NULL,
									 B_FILE_NODE | B_DIRECTORY_NODE );
		fFilePanel->SetTarget( this );
	}
	fFilePanel->Window()->SetTitle( windowTitle );
	BMessage message( command );
	fFilePanel->SetMessage( &message );
	if ( !fFilePanel->IsShowing() )
	{
		fFilePanel->Refresh();
		fFilePanel->Show();
	}
}

// set_window_pos
void
set_window_pos( BWindow* window, BRect frame )
{
	// sanity checks: make sure window is not too big/small
	// and that it's not off-screen
	float minWidth, maxWidth, minHeight, maxHeight;
	window->GetSizeLimits( &minWidth, &maxWidth, &minHeight, &maxHeight );

	make_sure_frame_is_within_limits( frame,
									  minWidth, minHeight, maxWidth, maxHeight );
	if ( make_sure_frame_is_on_screen( frame ) )
	{
		window->MoveTo( frame.LeftTop() );
		window->ResizeTo( frame.Width(), frame.Height() );
	}
}

// set_window_pos
void
launch_window( BWindow* window, bool showing )
{
	if ( window->Lock() )
	{
		if ( showing )
		{
			if ( window->IsHidden() )
				window->Show();
		}
		else
		{
			if ( !window->IsHidden() )
				window->Hide();
		}
		window->Unlock();
	}
}

/*****************************************************************************
 * InterfaceWindow::_RestoreSettings
 *****************************************************************************/
void
InterfaceWindow::_RestoreSettings()
{
	if ( load_settings( fSettings, "interface_settings", "VideoLAN Client" ) == B_OK )
	{
		BRect frame;
		if ( fSettings->FindRect( "main frame", &frame ) == B_OK )
			set_window_pos( this, frame );
		if (fSettings->FindRect( "playlist frame", &frame ) == B_OK )
			set_window_pos( fPlaylistWindow, frame );
		if (fSettings->FindRect( "messages frame", &frame ) == B_OK )
			set_window_pos( fMessagesWindow, frame );
		if (fSettings->FindRect( "settings frame", &frame ) == B_OK )
			set_window_pos( fPreferencesWindow, frame );
		
		bool showing;
		if ( fSettings->FindBool( "playlist showing", &showing ) == B_OK )
			launch_window( fPlaylistWindow, showing );
		if ( fSettings->FindBool( "messages showing", &showing ) == B_OK )
			launch_window( fMessagesWindow, showing );
		if ( fSettings->FindBool( "settings showing", &showing ) == B_OK )
			launch_window( fPreferencesWindow, showing );

		uint32 displayMode;
		if ( fSettings->FindInt32( "playlist display mode", (int32*)&displayMode ) == B_OK )
			fPlaylistWindow->SetDisplayMode( displayMode );
	}
}

/*****************************************************************************
 * InterfaceWindow::_StoreSettings
 *****************************************************************************/
void
InterfaceWindow::_StoreSettings()
{
	if ( fSettings->ReplaceRect( "main frame", Frame() ) != B_OK )
		fSettings->AddRect( "main frame", Frame() );
	if ( fPlaylistWindow->Lock() )
	{
		if (fSettings->ReplaceRect( "playlist frame", fPlaylistWindow->Frame() ) != B_OK)
			fSettings->AddRect( "playlist frame", fPlaylistWindow->Frame() );
		if (fSettings->ReplaceBool( "playlist showing", !fPlaylistWindow->IsHidden() ) != B_OK)
			fSettings->AddBool( "playlist showing", !fPlaylistWindow->IsHidden() );
		fPlaylistWindow->Unlock();
	}
	if ( fMessagesWindow->Lock() )
	{
		if (fSettings->ReplaceRect( "messages frame", fMessagesWindow->Frame() ) != B_OK)
			fSettings->AddRect( "messages frame", fMessagesWindow->Frame() );
		if (fSettings->ReplaceBool( "messages showing", !fMessagesWindow->IsHidden() ) != B_OK)
			fSettings->AddBool( "messages showing", !fMessagesWindow->IsHidden() );
		fMessagesWindow->Unlock();
	}
	if ( fPreferencesWindow->Lock() )
	{
		if (fSettings->ReplaceRect( "settings frame", fPreferencesWindow->Frame() ) != B_OK)
			fSettings->AddRect( "settings frame", fPreferencesWindow->Frame() );
		if (fSettings->ReplaceBool( "settings showing", !fPreferencesWindow->IsHidden() ) != B_OK)
			fSettings->AddBool( "settings showing", !fPreferencesWindow->IsHidden() );
		fPreferencesWindow->Unlock();
	}

	uint32 displayMode = fPlaylistWindow->DisplayMode();
	if (fSettings->ReplaceInt32( "playlist display mode", displayMode ) != B_OK )
		fSettings->AddInt32( "playlist display mode", displayMode );

	save_settings( fSettings, "interface_settings", "VideoLAN Client" );
}






/*****************************************************************************
 * CDMenu::CDMenu
 *****************************************************************************/
CDMenu::CDMenu(const char *name)
      : BMenu(name)
{
}

/*****************************************************************************
 * CDMenu::~CDMenu
 *****************************************************************************/
CDMenu::~CDMenu()
{
}

/*****************************************************************************
 * CDMenu::AttachedToWindow
 *****************************************************************************/
void CDMenu::AttachedToWindow(void)
{
    // remove all items
    while ( BMenuItem* item = RemoveItem( 0L ) )
        delete item;
    GetCD( "/dev/disk" );
    BMenu::AttachedToWindow();
}

/*****************************************************************************
 * CDMenu::GetCD
 *****************************************************************************/
int CDMenu::GetCD( const char *directory )
{
	BVolumeRoster volRoster;
	BVolume vol;
	BDirectory dir;
	status_t status = volRoster.GetNextVolume( &vol );
	while ( status ==  B_NO_ERROR )
	{
		BString deviceName;
		BString volumeName;
		bool isCDROM;
		if ( get_volume_info( vol, volumeName, isCDROM, deviceName )
			 && isCDROM )
		{
			BMessage* msg = new BMessage( OPEN_DVD );
			msg->AddString( "device", deviceName.String() );
			BMenuItem* item = new BMenuItem( volumeName.String(), msg );
			AddItem( item );
		}
 		vol.Unset();
		status = volRoster.GetNextVolume( &vol );
	}
}

/*****************************************************************************
 * LanguageMenu::LanguageMenu
 *****************************************************************************/
LanguageMenu::LanguageMenu( const char *name, int menu_kind, 
                            VlcWrapper *p_wrapper )
    :BMenu(name)
{
    kind = menu_kind;
    this->p_wrapper = p_wrapper;
}

/*****************************************************************************
 * LanguageMenu::~LanguageMenu
 *****************************************************************************/
LanguageMenu::~LanguageMenu()
{
}

/*****************************************************************************
 * LanguageMenu::AttachedToWindow
 *****************************************************************************/
void LanguageMenu::AttachedToWindow()
{
    // remove all items
    while ( BMenuItem* item = RemoveItem( 0L ) )
        delete item;

    SetRadioMode( true );
	if ( BList *list = p_wrapper->GetChannels( kind ) )
	{
	    for ( int32 i = 0; BMenuItem* item = (BMenuItem*)list->ItemAt( i ); i++ )
	        AddItem( item );
	    
	    if ( list->CountItems() > 1 )
	        AddItem( new BSeparatorItem(), 1 );
	}
    BMenu::AttachedToWindow();
}

/*****************************************************************************
 * TitleMenu::TitleMenu
 *****************************************************************************/
TitleMenu::TitleMenu( const char *name, intf_thread_t  *p_interface )
    : BMenu(name),
    p_intf( p_interface )
{
}

/*****************************************************************************
 * TitleMenu::~TitleMenu
 *****************************************************************************/
TitleMenu::~TitleMenu()
{
}

/*****************************************************************************
 * TitleMenu::AttachedToWindow
 *****************************************************************************/
void TitleMenu::AttachedToWindow()
{
    while( BMenuItem* item = RemoveItem( 0L ) )
        delete item;

    if ( BList *list = p_intf->p_sys->p_wrapper->GetTitles() )
	{    
		for( int i = 0; BMenuItem* item = (BMenuItem*)list->ItemAt( i ); i++ )
	        AddItem( item );
	}
    BMenu::AttachedToWindow();
}


/*****************************************************************************
 * ChapterMenu::ChapterMenu
 *****************************************************************************/
ChapterMenu::ChapterMenu( const char *name, intf_thread_t  *p_interface )
    : BMenu(name),
    p_intf( p_interface )
{
}

/*****************************************************************************
 * ChapterMenu::~ChapterMenu
 *****************************************************************************/
ChapterMenu::~ChapterMenu()
{
}

/*****************************************************************************
 * ChapterMenu::AttachedToWindow
 *****************************************************************************/
void ChapterMenu::AttachedToWindow()
{
    while( BMenuItem* item = RemoveItem( 0L ) )
        delete item;

    if ( BList* list = p_intf->p_sys->p_wrapper->GetChapters() )
	{    
	    for( int i = 0; BMenuItem* item = (BMenuItem*)list->ItemAt( i ); i++ )
	        AddItem( item );
	}
    
    BMenu::AttachedToWindow();
}









/*****************************************************************************
 * load_settings
 *****************************************************************************/
status_t
load_settings( BMessage* message, const char* fileName, const char* folder )
{
	status_t ret = B_BAD_VALUE;
	if ( message )
	{
		BPath path;
		if ( ( ret = find_directory( B_USER_SETTINGS_DIRECTORY, &path ) ) == B_OK )
		{
			// passing folder is optional
			if ( folder )
				ret = path.Append( folder );
			if ( ret == B_OK && ( ret = path.Append( fileName ) ) == B_OK )
			{
				BFile file( path.Path(), B_READ_ONLY );
				if ( ( ret = file.InitCheck() ) == B_OK )
				{
					ret = message->Unflatten( &file );
					file.Unset();
				}
			}
		}
	}
	return ret;
}

/*****************************************************************************
 * save_settings
 *****************************************************************************/
status_t
save_settings( BMessage* message, const char* fileName, const char* folder )
{
	status_t ret = B_BAD_VALUE;
	if ( message )
	{
		BPath path;
		if ( ( ret = find_directory( B_USER_SETTINGS_DIRECTORY, &path ) ) == B_OK )
		{
			// passing folder is optional
			if ( folder && ( ret = path.Append( folder ) ) == B_OK )
				ret = create_directory( path.Path(), 0777 );
			if ( ret == B_OK && ( ret = path.Append( fileName ) ) == B_OK )
			{
				BFile file( path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE );
				if ( ( ret = file.InitCheck() ) == B_OK )
				{
					ret = message->Flatten( &file );
					file.Unset();
				}
			}
		}
	}
	return ret;
}
