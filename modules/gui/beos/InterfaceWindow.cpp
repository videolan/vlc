/*****************************************************************************
 * InterfaceWindow.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: InterfaceWindow.cpp,v 1.18 2003/01/16 15:26:23 titer Exp $
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
#include "InterfaceWindow.h"

#define INTERFACE_UPDATE_TIMEOUT 80000 // 2 frames if at 25 fps


/*****************************************************************************
 * InterfaceWindow
 *****************************************************************************/

InterfaceWindow::InterfaceWindow( BRect frame, const char *name,
								  intf_thread_t  *p_interface )
	: BWindow( frame, name, B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
			   B_NOT_ZOOMABLE | B_WILL_ACCEPT_FIRST_CLICK | B_ASYNCHRONOUS_CONTROLS ),
	  p_intf( p_interface ),
	  fFilePanel( NULL ),
	  fSubtitlesPanel( NULL ),
	  fLastUpdateTime( system_time() ),
	  fSettings( new BMessage( 'sett' ) )
{
    p_intf = p_interface;
    p_wrapper = p_intf->p_sys->p_wrapper;
    
    fPlaylistIsEmpty = ( p_wrapper->PlaylistSize() < 0 );
    
    fPlaylistWindow = new PlayListWindow( BRect( 100.0, 100.0, 400.0, 350.0 ),
	  									  "Playlist",
	  									  this,
	  									  p_intf );
    BScreen *p_screen = new BScreen();
    BRect screen_rect = p_screen->Frame();
    delete p_screen;
    BRect window_rect;
    window_rect.Set( ( screen_rect.right - PREFS_WINDOW_WIDTH ) / 2,
                     ( screen_rect.bottom - PREFS_WINDOW_HEIGHT ) / 2,
                     ( screen_rect.right + PREFS_WINDOW_WIDTH ) / 2,
                     ( screen_rect.bottom + PREFS_WINDOW_HEIGHT ) / 2 );
	fPreferencesWindow = new PreferencesWindow( window_rect,
	                                            "Preferences",
	                                            p_intf );
    
	// set the title bar
	SetName( "interface" );
	SetTitle( VOUT_TITLE );

	// the media control view
	p_mediaControl = new MediaControlView( BRect( 0.0, 0.0, 250.0, 50.0 ),
	                                       p_intf );
	p_mediaControl->SetViewColor( ui_color( B_PANEL_BACKGROUND_COLOR ) );
	p_mediaControl->SetEnabled( !fPlaylistIsEmpty );

	float width, height;
	p_mediaControl->GetPreferredSize( &width, &height );

	// set up the main menu
	fMenuBar = new BMenuBar( BRect(0.0, 0.0, width, 15.0), "main menu",
							 B_FOLLOW_NONE, B_ITEMS_IN_ROW, false );

	// make menu bar resize to correct height
	float menuWidth, menuHeight;
	fMenuBar->GetPreferredSize( &menuWidth, &menuHeight );
	fMenuBar->ResizeTo( width, menuHeight );	// don't change! it's a workarround!
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

	fileMenu->AddItem( new BMenuItem( "Load a subtitle file" B_UTF8_ELLIPSIS,
									  new BMessage( LOAD_SUBFILE ) ) );
	
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem( new BMenuItem( "Play List" B_UTF8_ELLIPSIS,
									  new BMessage( OPEN_PLAYLIST ), 'P') );
	
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

    /* Add the Settings menu */
    fSettingsMenu = new BMenu( "Settings" );
    fSettingsMenu->AddItem( fPreferencesMI =
        new BMenuItem( "Preferences", new BMessage( OPEN_PREFERENCES ) ) );
	fMenuBar->AddItem( fSettingsMenu );							

	// prepare fow showing
	_SetMenusEnabled( false );
	p_mediaControl->SetEnabled( false );

	_RestoreSettings();

	Show();
}

InterfaceWindow::~InterfaceWindow()
{
	if (fPlaylistWindow)
		fPlaylistWindow->ReallyQuit();
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
	int playback_status;	  // remember playback state
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
			if( fFilePanel )
			{
				fFilePanel->Show();
				break;
			}
			fFilePanel = new BFilePanel();
			fFilePanel->SetTarget( this );
			fFilePanel->Show();
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
					p_wrapper->openDisc( type, device, 0, 0 );
				}
				_UpdatePlaylist();
			}
			break;
		
		case LOAD_SUBFILE:
			if( fSubtitlesPanel )
			{
				fSubtitlesPanel->Show();
				break;
			}
			fSubtitlesPanel = new BFilePanel();
			fSubtitlesPanel->SetTarget( this );
			fSubtitlesPanel->SetMessage( new BMessage( SUBFILE_RECEIVED ) );
			fSubtitlesPanel->Show();
			break;

		case SUBFILE_RECEIVED:
		{
			entry_ref ref;
			if( p_message->FindRef( "refs", 0, &ref ) == B_OK )
			{
				BPath path( &ref );
				if ( path.InitCheck() == B_OK )
					p_wrapper->LoadSubFile( (char*)path.Path() );
			}
			break;
		}
	
		case STOP_PLAYBACK:
			// this currently stops playback not nicely
			if (playback_status > UNDEF_S)
			{
				snooze( 400000 );
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
				if ( p_message->FindInt32( "index", &index ) == B_OK )
					p_wrapper->toggleTitle( index );
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
				if ( p_message->FindInt32( "index", &index ) == B_OK )
                     p_wrapper->toggleChapter( index );
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
			p_wrapper->navigatePrev();
			break;
		case NAVIGATE_NEXT:
			p_wrapper->navigateNext();
			break;
		// drag'n'drop and system messages
		case B_REFS_RECEIVED:
		case B_SIMPLE_DATA:
			{
				/* file(s) opened by the File menu -> append to the playlist;
				 * file(s) opened by drag & drop -> replace playlist;
				 * file(s) opened by 'shift' + drag & drop -> append */
				bool replace = false;
				if ( p_message->WasDropped() )
					replace = !( modifiers() & B_SHIFT_KEY );
					
				// build list of files to be played from message contents
				entry_ref ref;
				BList files;
				for ( int i = 0; p_message->FindRef( "refs", i, &ref ) == B_OK; i++ )
				{
					BPath path( &ref );
					if ( path.InitCheck() == B_OK )
					{
						bool add = true;
						// has the user dropped a dvd disk icon?
						BDirectory dir( &ref );
						if ( dir.InitCheck() == B_OK && dir.IsRootDirectory() )
						{
							BVolumeRoster volRoster;
							BVolume vol;
							BDirectory volumeRoot;
							status_t status = volRoster.GetNextVolume( &vol );
							while( status == B_NO_ERROR )
							{
								if( vol.GetRootDirectory( &volumeRoot ) == B_OK
									&& dir == volumeRoot )
								{
									BString volumeName;
									BString deviceName;
									bool isCDROM = false;
									bool success = false;
									deviceName = "";
									volumeName = "";
									char name[B_FILE_NAME_LENGTH];
									if ( vol.GetName( name ) >= B_OK )	// disk is currently mounted
									{
										volumeName = name;
										dev_t dev = vol.Device();
										fs_info info;
										if ( fs_stat_dev( dev, &info ) == B_OK )
										{
											success = true;
											deviceName = info.device_name;
											if ( vol.IsReadOnly() )
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
									
									if( success && isCDROM )
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
						if( add )
						{
							files.AddItem( new BString( (char*)path.Path() ) );
						}
					}
				}
				// give the list to VLC
				p_wrapper->openFiles(&files, replace);
				_UpdatePlaylist();
			}
			break;

		case OPEN_PREFERENCES:
		    if( fPreferencesWindow->Lock() )
		    {
			    if (fPreferencesWindow->IsHidden())
				    fPreferencesWindow->Show();
			    else
				    fPreferencesWindow->Activate();
				fPreferencesWindow->Unlock();
			}
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
	
	p_intf->b_die = 1;

	_StoreSettings();

	return( true );
}

/*****************************************************************************
 * InterfaceWindow::updateInterface
 *****************************************************************************/
void InterfaceWindow::updateInterface()
{
    if( p_wrapper->HasInput() )
    {
		if ( acquire_sem( p_mediaControl->fScrubSem ) == B_OK )
		{
		    p_wrapper->setTimeAsFloat(p_mediaControl->GetSeekTo());
		}
		else if ( Lock() )
		{
//			p_mediaControl->SetEnabled( true );
			bool hasTitles = p_wrapper->HasTitles();
			bool hasChapters = p_wrapper->HasChapters();
			p_mediaControl->SetStatus( p_wrapper->InputStatus(), 
									   p_wrapper->InputRate() );
			p_mediaControl->SetProgress( p_wrapper->getTimeAsFloat() );
			_SetMenusEnabled( true, hasChapters, hasTitles );

			_UpdateSpeedMenu( p_wrapper->InputRate() );

			// enable/disable skip buttons
			bool canSkipPrev;
			bool canSkipNext;
			p_wrapper->getNavCapabilities( &canSkipPrev, &canSkipNext );
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
		if ( fPlaylistWindow->Lock() )
		{
			fPlaylistWindow->UpdatePlaylist();
			fPlaylistWindow->Unlock();
		}
	}
    else
    {
    	_SetMenusEnabled( false );
//		p_mediaControl->SetEnabled( false );
    }

    /* always force the user-specified volume */
    /* FIXME : I'm quite sure there is a cleaner way to do this */
    if( p_wrapper->GetVolume() != p_mediaControl->GetVolume() )
    {
        p_wrapper->SetVolume( p_mediaControl->GetVolume() );
    }

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
	if (Lock())
	{
		if (fNextChapterMI->IsEnabled() != hasChapters)
			fNextChapterMI->SetEnabled(hasChapters);
		if (fPrevChapterMI->IsEnabled() != hasChapters)
			fPrevChapterMI->SetEnabled(hasChapters);
		if (fChapterMenu->IsEnabled() != hasChapters)
			fChapterMenu->SetEnabled(hasChapters);
		if (fNextTitleMI->IsEnabled() != hasTitles)
			fNextTitleMI->SetEnabled(hasTitles);
		if (fPrevTitleMI->IsEnabled() != hasTitles)
			fPrevTitleMI->SetEnabled(hasTitles);
		if (fTitleMenu->IsEnabled() != hasTitles)
			fTitleMenu->SetEnabled(hasTitles);
		if (fAudioMenu->IsEnabled() != hasFile)
			fAudioMenu->SetEnabled(hasFile);
		if (fNavigationMenu->IsEnabled() != hasFile)
			fNavigationMenu->SetEnabled(hasFile);
		if (fLanguageMenu->IsEnabled() != hasFile)
			fLanguageMenu->SetEnabled(hasFile);
		if (fSubtitlesMenu->IsEnabled() != hasFile)
			fSubtitlesMenu->SetEnabled(hasFile);
		if (fSpeedMenu->IsEnabled() != hasFile)
			fSpeedMenu->SetEnabled(hasFile);
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
//printf("InterfaceWindow::_InputStreamChanged()\n");
	// TODO: move more stuff from updateInterface() here!
	snooze( 400000 );
	p_wrapper->SetVolume( p_mediaControl->GetVolume() );
}

/*****************************************************************************
 * InterfaceWindow::_LoadSettings
 *****************************************************************************/
status_t
InterfaceWindow::_LoadSettings( BMessage* message, const char* fileName, const char* folder )
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
 * InterfaceWindow::_SaveSettings
 *****************************************************************************/
status_t
InterfaceWindow::_SaveSettings( BMessage* message, const char* fileName, const char* folder )
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

/*****************************************************************************
 * InterfaceWindow::_RestoreSettings
 *****************************************************************************/
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

/*****************************************************************************
 * InterfaceWindow::_RestoreSettings
 *****************************************************************************/
void
InterfaceWindow::_RestoreSettings()
{
	if ( _LoadSettings( fSettings, "interface_settings", "VideoLAN Client" ) == B_OK )
	{
		BRect mainFrame;
		if ( fSettings->FindRect( "main frame", &mainFrame ) == B_OK )
		{
			// sanity checks: make sure window is not too big/small
			// and that it's not off-screen
			float minWidth, maxWidth, minHeight, maxHeight;
			GetSizeLimits( &minWidth, &maxWidth, &minHeight, &maxHeight );

			make_sure_frame_is_within_limits( mainFrame,
											  minWidth, minHeight, maxWidth, maxHeight );
			make_sure_frame_is_on_screen( mainFrame );


			MoveTo( mainFrame.LeftTop() );
			ResizeTo( mainFrame.Width(), mainFrame.Height() );
		}
		if ( fPlaylistWindow->Lock() )
		{
			BRect playlistFrame;
			if (fSettings->FindRect( "playlist frame", &playlistFrame ) == B_OK )
			{
				// sanity checks: make sure window is not too big/small
				// and that it's not off-screen
				float minWidth, maxWidth, minHeight, maxHeight;
				fPlaylistWindow->GetSizeLimits( &minWidth, &maxWidth, &minHeight, &maxHeight );

				make_sure_frame_is_within_limits( playlistFrame,
												  minWidth, minHeight, maxWidth, maxHeight );
				make_sure_frame_is_on_screen( playlistFrame );

				fPlaylistWindow->MoveTo( playlistFrame.LeftTop() );
				fPlaylistWindow->ResizeTo( playlistFrame.Width(), playlistFrame.Height() );
			}
			
			bool showing;
			if ( fSettings->FindBool( "playlist showing", &showing ) == B_OK )
			{
				if ( showing )
				{
					if ( fPlaylistWindow->IsHidden() )
						fPlaylistWindow->Show();
				}
				else
				{
					if ( !fPlaylistWindow->IsHidden() )
						fPlaylistWindow->Hide();
				}
			}

			fPlaylistWindow->Unlock();
		}
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
	_SaveSettings( fSettings, "interface_settings", "VideoLAN Client" );
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
	while (BMenuItem* item = RemoveItem(0L))
		delete item;
	GetCD("/dev/disk");
	BMenu::AttachedToWindow();
}

/*****************************************************************************
 * CDMenu::GetCD
 *****************************************************************************/
int CDMenu::GetCD( const char *directory )
{
	BVolumeRoster *volRoster;
	BVolume	   *vol;
	BDirectory	*dir;
	int		   status;
	int		   mounted;   
	char		  name[B_FILE_NAME_LENGTH]; 
	fs_info	   info;
	dev_t		 dev;
	
	volRoster = new BVolumeRoster();
	vol = new BVolume();
	dir = new BDirectory();
	status = volRoster->GetNextVolume(vol);
	status = vol->GetRootDirectory(dir);
	while (status ==  B_NO_ERROR)
	{
		mounted = vol->GetName(name);	
		if ((mounted == B_OK) && /* Disk is currently Mounted */
			(vol->IsReadOnly()) ) /* Disk is read-only */
		{
			dev = vol->Device();
			fs_stat_dev(dev, &info);
			
			device_geometry g;
			int i_dev;
			i_dev = open( info.device_name, O_RDONLY );
		   
			if( i_dev >= 0 )
			{
				if( ioctl(i_dev, B_GET_GEOMETRY, &g, sizeof(g)) >= 0 )
				{
					if( g.device_type == B_CD ) //ensure the drive is a CD-ROM
					{
						BMessage *msg;
						msg = new BMessage( OPEN_DVD );
						msg->AddString( "device", info.device_name );
						BMenuItem *menu_item;
						menu_item = new BMenuItem( name, msg );
						AddItem( menu_item );
					}
					close(i_dev);
				}
			}
 		}
 		vol->Unset();
		status = volRoster->GetNextVolume(vol);
	}
	return 0;
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
	_GetChannels();
	BMenu::AttachedToWindow();
}

/*****************************************************************************
 * LanguageMenu::_GetChannels
 *****************************************************************************/
void LanguageMenu::_GetChannels()
{
    BMenuItem *item;
    BList *list;
    
    if( ( list = p_wrapper->InputGetChannels( kind ) ) == NULL )
        return;
    
    for( int i = 0; i < list->CountItems(); i++ )
    {
        item = (BMenuItem*)list->ItemAt( i );
        AddItem( item );
    }
    
    if( list->CountItems() > 1 )
        AddItem( new BSeparatorItem(), 1 );
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
	// make title menu empty
	while ( BMenuItem* item = RemoveItem( 0L ) )
		delete item;

#if 0
	input_thread_t* input = p_intf->p_sys->p_input;
	if ( input )
	{
		// lock stream access
		vlc_mutex_lock( &input->stream.stream_lock );
		// populate menu according to current stream
		int32 numTitles = input->stream.i_area_nb;
		if ( numTitles > 1 )
		{
			// disallow title 0!
			for ( int32 i = 1; i < numTitles; i++ )
			{
				BMessage* message = new BMessage( TOGGLE_TITLE );
				message->AddInt32( "index", i );
				BString helper( "" );
				helper << i;
				BMenuItem* item = new BMenuItem( helper.String(), message );
				item->SetMarked( input->stream.p_selected_area->i_id == i );
				AddItem( item );
			}
		}
		// done messing with stream
		vlc_mutex_unlock( &input->stream.stream_lock );
	}
#endif
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
	// make title menu empty
	while ( BMenuItem* item = RemoveItem( 0L ) )
		delete item;

#if 0
	input_thread_t* input = p_intf->p_sys->p_input;
	if ( input )
	{
		// lock stream access
		vlc_mutex_lock( &input->stream.stream_lock );
		// populate menu according to current stream
		int32 numChapters = input->stream.p_selected_area->i_part_nb;
		if ( numChapters > 1 )
		{
			for ( int32 i = 0; i < numChapters; i++ )
			{
				BMessage* message = new BMessage( TOGGLE_CHAPTER );
				message->AddInt32( "index", i );
				BString helper( "" );
				helper << i + 1;
				BMenuItem* item = new BMenuItem( helper.String(), message );
				item->SetMarked( input->stream.p_selected_area->i_part == i );
				AddItem( item );
			}
		}
		// done messing with stream
		vlc_mutex_unlock( &input->stream.stream_lock );
	}
	BMenu::AttachedToWindow();
#endif
}

