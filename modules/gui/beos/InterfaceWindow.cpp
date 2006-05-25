/*****************************************************************************
 * InterfaceWindow.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Tony Castley <tony@castley.net>
 *          Richard Shepherd <richard@rshepherd.demon.co.uk>
 *          Stephan Aßmus <superstippi@gmx.de>
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
#include <vlc/input.h>

/* BeOS interface headers */
#include "MsgVals.h"
#include "MediaControlView.h"
#include "PlayListWindow.h"
#include "PreferencesWindow.h"
#include "MessagesWindow.h"
#include "InterfaceWindow.h"

#define INTERFACE_UPDATE_TIMEOUT  80000 // 2 frames if at 25 fps
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
    if ( volume.GetName( name ) >= B_OK )    // disk is currently mounted
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
                    delete string;    // at least don't leak
            }
        }
        else
        {
            if ( !asked )
            {
                // ask user if we should parse sub-folders as well
                BAlert* alert = new BAlert( "sub-folders?",
                                            _("Open files from all sub-folders as well?"),
                                            _("Cancel"), _("Open"), NULL, B_WIDTH_AS_USUAL,
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

static int PlaylistChanged( vlc_object_t *p_this, const char * psz_variable,
                            vlc_value_t old_val, vlc_value_t new_val,
                            void * param )
{
    InterfaceWindow * w = (InterfaceWindow *) param;
    w->UpdatePlaylist();
    return VLC_SUCCESS;
}

/*****************************************************************************
 * InterfaceWindow
 *****************************************************************************/

InterfaceWindow::InterfaceWindow( intf_thread_t * _p_intf, BRect frame,
                                  const char * name )
    : BWindow( frame, name, B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
               B_NOT_ZOOMABLE | B_WILL_ACCEPT_FIRST_CLICK | B_ASYNCHRONOUS_CONTROLS ),

      /* Initializations */
      p_intf( _p_intf ),
      p_input( NULL ),
      p_playlist( NULL ),

      fFilePanel( NULL ),
      fLastUpdateTime( system_time() ),
      fSettings( new BMessage( 'sett' ) )
{
    p_playlist = (playlist_t *)
        vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    var_AddCallback( p_playlist, "intf-change", PlaylistChanged, this );
    var_AddCallback( p_playlist, "item-change", PlaylistChanged, this );
    var_AddCallback( p_playlist, "item-append", PlaylistChanged, this );
    var_AddCallback( p_playlist, "item-deleted", PlaylistChanged, this );
    var_AddCallback( p_playlist, "playlist-current", PlaylistChanged, this );

    char psz_tmp[1024];
#define ADD_ELLIPSIS( a ) \
    memset( psz_tmp, 0, 1024 ); \
    snprintf( psz_tmp, 1024, "%s%s", a, B_UTF8_ELLIPSIS );

    BScreen screen;
    BRect screen_rect = screen.Frame();
    BRect window_rect;
    window_rect.Set( ( screen_rect.right - PREFS_WINDOW_WIDTH ) / 2,
                     ( screen_rect.bottom - PREFS_WINDOW_HEIGHT ) / 2,
                     ( screen_rect.right + PREFS_WINDOW_WIDTH ) / 2,
                     ( screen_rect.bottom + PREFS_WINDOW_HEIGHT ) / 2 );
    fPreferencesWindow = new PreferencesWindow( p_intf, window_rect, _("Preferences") );
    window_rect.Set( screen_rect.right - 500,
                     screen_rect.top + 50,
                     screen_rect.right - 150,
                     screen_rect.top + 250 );
#if 0
    fPlaylistWindow = new PlayListWindow( window_rect, _("Playlist"), this, p_intf );
    window_rect.Set( screen_rect.right - 550,
                     screen_rect.top + 300,
                     screen_rect.right - 150,
                     screen_rect.top + 500 );
#endif
    fMessagesWindow = new MessagesWindow( p_intf, window_rect, _("Messages") );

    // the media control view
    p_mediaControl = new MediaControlView( p_intf, BRect( 0.0, 0.0, 250.0, 50.0 ) );
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


    // Add the file Menu
    BMenu* fileMenu = new BMenu( _("File") );
    fMenuBar->AddItem( fileMenu );
    ADD_ELLIPSIS( _("Open File") );
    fileMenu->AddItem( new BMenuItem( psz_tmp, new BMessage( OPEN_FILE ), 'O') );
    fileMenu->AddItem( new CDMenu( _("Open Disc") ) );
    ADD_ELLIPSIS( _("Open Subtitles") );
    fileMenu->AddItem( new BMenuItem( psz_tmp, new BMessage( LOAD_SUBFILE ) ) );

    fileMenu->AddSeparatorItem();
    ADD_ELLIPSIS( _("About") );
    BMenuItem* item = new BMenuItem( psz_tmp, new BMessage( B_ABOUT_REQUESTED ), 'A');
    item->SetTarget( be_app );
    fileMenu->AddItem( item );
    fileMenu->AddItem( new BMenuItem( _("Quit"), new BMessage( B_QUIT_REQUESTED ), 'Q') );

    fLanguageMenu = new LanguageMenu( p_intf, _("Language"), "audio-es" );
    fSubtitlesMenu = new LanguageMenu( p_intf, _("Subtitles"), "spu-es" );

    /* Add the Audio menu */
    fAudioMenu = new BMenu( _("Audio") );
    fMenuBar->AddItem ( fAudioMenu );
    fAudioMenu->AddItem( fLanguageMenu );
    fAudioMenu->AddItem( fSubtitlesMenu );

    fPrevTitleMI = new BMenuItem( _("Prev Title"), new BMessage( PREV_TITLE ) );
    fNextTitleMI = new BMenuItem( _("Next Title"), new BMessage( NEXT_TITLE ) );
    fPrevChapterMI = new BMenuItem( _("Previous chapter"), new BMessage( PREV_CHAPTER ) );
    fNextChapterMI = new BMenuItem( _("Next chapter"), new BMessage( NEXT_CHAPTER ) );

    /* Add the Navigation menu */
    fNavigationMenu = new BMenu( _("Navigation") );
    fMenuBar->AddItem( fNavigationMenu );
    fNavigationMenu->AddItem( fPrevTitleMI );
    fNavigationMenu->AddItem( fNextTitleMI );
    fNavigationMenu->AddItem( fTitleMenu = new TitleMenu( _("Go to Title"), p_intf ) );
    fNavigationMenu->AddSeparatorItem();
    fNavigationMenu->AddItem( fPrevChapterMI );
    fNavigationMenu->AddItem( fNextChapterMI );
    fNavigationMenu->AddItem( fChapterMenu = new ChapterMenu( _("Go to Chapter"), p_intf ) );

    /* Add the Speed menu */
    fSpeedMenu = new BMenu( _("Speed") );
    fSpeedMenu->SetRadioMode( true );
    fSpeedMenu->AddItem(
        fHeighthMI = new BMenuItem( "1/8x", new BMessage( HEIGHTH_PLAY ) ) );
    fSpeedMenu->AddItem(
        fQuarterMI = new BMenuItem( "1/4x", new BMessage( QUARTER_PLAY ) ) );
    fSpeedMenu->AddItem(
        fHalfMI = new BMenuItem( "1/2x", new BMessage( HALF_PLAY ) ) );
    fSpeedMenu->AddItem(
        fNormalMI = new BMenuItem( "1x", new BMessage( NORMAL_PLAY ) ) );
    fSpeedMenu->AddItem(
        fTwiceMI = new BMenuItem( "2x", new BMessage( TWICE_PLAY ) ) );
    fSpeedMenu->AddItem(
        fFourMI = new BMenuItem( "4x", new BMessage( FOUR_PLAY ) ) );
    fSpeedMenu->AddItem(
        fHeightMI = new BMenuItem( "8x", new BMessage( HEIGHT_PLAY ) ) );
    fMenuBar->AddItem( fSpeedMenu );

    /* Add the Show menu */
    fShowMenu = new BMenu( _("Window") );
    ADD_ELLIPSIS( _("Playlist") );
    fShowMenu->AddItem( new BMenuItem( psz_tmp, new BMessage( OPEN_PLAYLIST ), 'P') );
    ADD_ELLIPSIS( _("Messages") );
    fShowMenu->AddItem( new BMenuItem( psz_tmp, new BMessage( OPEN_MESSAGES ), 'M' ) );
#if 0 
    ADD_ELLIPSIS( _("Preferences") );
    fShowMenu->AddItem( new BMenuItem( psz_tmp, new BMessage( OPEN_PREFERENCES ), 'S' ) );
#endif
    fMenuBar->AddItem( fShowMenu );

    // add the media control view after the menubar is complete
    // because it will set the window size limits in AttachedToWindow()
    // and the menubar needs to report the correct PreferredSize()
    AddChild( p_mediaControl );

    /* Prepare fow showing */
    _SetMenusEnabled( false );
    p_mediaControl->SetEnabled( false );

    _RestoreSettings();

    Show();
}

InterfaceWindow::~InterfaceWindow()
{
    if( p_input )
    {
        vlc_object_release( p_input );
    }
    if( p_playlist )
    {
        vlc_object_release( p_playlist );
    }
#if 0
    if( fPlaylistWindow )
    {
        fPlaylistWindow->ReallyQuit();
    }
#endif
    if( fMessagesWindow )
    {
        fMessagesWindow->ReallyQuit();
    }
    if( fPreferencesWindow )
    {
        fPreferencesWindow->ReallyQuit();
    }
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
    switch( p_message->what )
    {
        case B_ABOUT_REQUESTED:
        {
            BAlert * alert;

            alert = new BAlert( "VLC media player" VERSION,
                                "VLC media player" VERSION " (BeOS interface)\n\n"
                                "The VideoLAN team <videolan@videolan.org>\n"
                                "http://www.videolan.org/", _("OK") );
            alert->Go();
            break;
        }
        case TOGGLE_ON_TOP:
            break;

        case OPEN_FILE:
            _ShowFilePanel( B_REFS_RECEIVED, _("VLC media player: Open Media Files") );
            break;

        case LOAD_SUBFILE:
            _ShowFilePanel( SUBFILE_RECEIVED, _("VLC media player: Open Subtitle File") );
            break;
#if 0
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
#endif
        case OPEN_DVD:
            {
                const char * psz_device;
                if( p_playlist &&
                    p_message->FindString( "device", &psz_device ) == B_OK )
                {
                    char psz_uri[1024];
                    memset( psz_uri, 0, 1024 );
                    snprintf( psz_uri, 1024, "dvdnav:%s", psz_device );
                    playlist_PlaylistAdd( p_playlist, psz_uri, psz_device,
                                  PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
                }
                UpdatePlaylist();
            }
            break;

        case SUBFILE_RECEIVED:
        {
            entry_ref ref;
            if( p_message->FindRef( "refs", 0, &ref ) == B_OK )
            {
                BPath path( &ref );
                if ( path.InitCheck() == B_OK )
                    config_PutPsz( p_intf, "sub-file", path.Path() );
            }
            break;
        }

        case STOP_PLAYBACK:
            if( p_playlist )
            {
                playlist_Stop( p_playlist );
            }
            p_mediaControl->SetStatus(-1, INPUT_RATE_DEFAULT);
            break;

        case START_PLAYBACK:
        case PAUSE_PLAYBACK:
        {
            vlc_value_t val;
            val.i_int = PLAYING_S;
            if( p_input )
            {
                var_Get( p_input, "state", &val );
            }
            if( p_input && val.i_int != PAUSE_S )
            {
                val.i_int = PAUSE_S;
                var_Set( p_input, "state", val );
            }
            else
            {
                playlist_Play( p_playlist );
            }
            break;
        }
        case HEIGHTH_PLAY:
            if( p_input )
            {
                var_SetInteger( p_input, "rate", INPUT_RATE_DEFAULT * 8 );
            }
            break;

        case QUARTER_PLAY:
            if( p_input )
            {
                var_SetInteger( p_input, "rate", INPUT_RATE_DEFAULT * 4 );
            }
            break;

        case HALF_PLAY:
            if( p_input )
            {
                var_SetInteger( p_input, "rate", INPUT_RATE_DEFAULT * 2 );
            }
            break;

        case NORMAL_PLAY:
            if( p_input )
            {
                var_SetInteger( p_input, "rate", INPUT_RATE_DEFAULT );
            }
            break;

        case TWICE_PLAY:
            if( p_input )
            {
                var_SetInteger( p_input, "rate", INPUT_RATE_DEFAULT / 2 );
            }
            break;

        case FOUR_PLAY:
            if( p_input )
            {
                var_SetInteger( p_input, "rate", INPUT_RATE_DEFAULT / 4 );
            }
            break;

        case HEIGHT_PLAY:
            if( p_input )
            {
                var_SetInteger( p_input, "rate", INPUT_RATE_DEFAULT / 8 );
            }
            break;

        case SEEK_PLAYBACK:
            /* handled by semaphores */
            break;

        case VOLUME_CHG:
            aout_VolumeSet( p_intf, p_mediaControl->GetVolume() );
            break;

        case VOLUME_MUTE:
            aout_VolumeMute( p_intf, NULL );
            break;

        case SELECT_CHANNEL:
        {
            int32 channel;
            if( p_input )
            {
                if( p_message->FindInt32( "audio-es", &channel ) == B_OK )
                {
                    var_SetInteger( p_input, "audio-es", channel );
                }
                else if( p_message->FindInt32( "spu-es", &channel ) == B_OK )
                {
                    var_SetInteger( p_input, "spu-es", channel );
                }
            }
            break;
        }

        case PREV_TITLE:
            if( p_input )
            {
                var_SetVoid( p_input, "prev-title" );
            }
            break;

        case NEXT_TITLE:
            if( p_input )
            {
                var_SetVoid( p_input, "next-title" );
            }
            break;

        case TOGGLE_TITLE:
        {
            int32 index;
            if( p_input &&
                p_message->FindInt32( "index", &index ) == B_OK )
            {
                var_SetInteger( p_input, "title", index );
            }
            break;
        }

        case PREV_CHAPTER:
            if( p_input )
            {
                var_SetVoid( p_input, "prev-chapter" );
            }
            break;

        case NEXT_CHAPTER:
            if( p_input )
            {
                var_SetVoid( p_input, "next-chapter" );
            }
            break;

        case TOGGLE_CHAPTER:
        {
            int32 index;
            if( p_input &&
                p_message->FindInt32( "index", &index ) == B_OK )
            {
                var_SetInteger( p_input, "chapter", index );
            }
            break;
        }

        case PREV_FILE:
            if( p_playlist )
            {
                playlist_Prev( p_playlist );
            }
            break;

        case NEXT_FILE:
            if( p_playlist )
            {
                playlist_Next( p_playlist );
            }
            break;

        case NAVIGATE_PREV:
            if( p_input )
            {
                vlc_value_t val;

                /* First try to go to previous chapter */
                if( !var_Get( p_input, "chapter", &val ) )
                {
                    if( val.i_int > 1 )
                    {
                        var_SetVoid( p_input, "prev-chapter" );
                        break;
                    }
                }

                /* Try to go to previous title */
                if( !var_Get( p_input, "title", &val ) )
                {
                    if( val.i_int > 1 )
                    {
                        var_SetVoid( p_input, "prev-title" );
                        break;
                    }
                }

                /* Try to go to previous file */
                if( p_playlist )
                {
                    playlist_Prev( p_playlist );
                }
            }
            break;

        case NAVIGATE_NEXT:
            if( p_input )
            {
                vlc_value_t val, val_list;

                /* First try to go to next chapter */
                if( !var_Get( p_input, "chapter", &val ) )
                {
                    var_Change( p_input, "chapter", VLC_VAR_GETCHOICES,
                                &val_list, NULL );
                    if( val_list.p_list->i_count > val.i_int )
                    {
                        var_Change( p_input, "chapter", VLC_VAR_FREELIST,
                                    &val_list, NULL );
                        var_SetVoid( p_input, "next-chapter" );
                        break;
                    }
                    var_Change( p_input, "chapter", VLC_VAR_FREELIST,
                                &val_list, NULL );
                }

                /* Try to go to next title */
                if( !var_Get( p_input, "title", &val ) )
                {
                    var_Change( p_input, "title", VLC_VAR_GETCHOICES,
                                &val_list, NULL );
                    if( val_list.p_list->i_count > val.i_int )
                    {
                        var_Change( p_input, "title", VLC_VAR_FREELIST,
                                    &val_list, NULL );
                        var_SetVoid( p_input, "next-title" );
                        break;
                    }
                    var_Change( p_input, "title", VLC_VAR_FREELIST,
                                &val_list, NULL );
                }

                /* Try to go to next file */
                if( p_playlist )
                {
                    playlist_Next( p_playlist );
                }
            }
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
               file(s) opened by drag & drop -> replace playlist;
               file(s) opened by 'shift' + drag & drop -> append */

            int32 count;
            type_code dummy;
            if( p_message->GetInfo( "refs", &dummy, &count ) != B_OK ||
                count < 1 )
            {
                break;
            }

            vlc_bool_t b_remove = ( p_message->WasDropped() &&
                                    !( modifiers() & B_SHIFT_KEY ) );

            if( b_remove && p_playlist )
            {
                playlist_Clear( p_playlist );
            }

            entry_ref ref;
            for( int i = 0; p_message->FindRef( "refs", i, &ref ) == B_OK; i++ )
            {
                BPath path( &ref );

                /* TODO: find out if this is a DVD icon */

                if( p_playlist )
                {
                    playlist_PlaylistAdd( p_playlist, path.Path(), path.Path(),
                                  PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
                }
            }

            UpdatePlaylist();
            break;
        }

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
    if( p_playlist )
    {
        playlist_Stop( p_playlist );
    }
    p_mediaControl->SetStatus(-1, INPUT_RATE_DEFAULT);

     _StoreSettings();

    p_intf->b_die = 1;

    return( true );
}

/*****************************************************************************
 * InterfaceWindow::UpdateInterface
 *****************************************************************************/
void InterfaceWindow::UpdateInterface()
{
    if( !p_input )
    {
        p_input = (input_thread_t *)
            vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    }
    else if( p_input->b_dead )
    {
        vlc_object_release( p_input );
        p_input = NULL;
    }

    /* Get ready to update the interface */
    if( LockWithTimeout( INTERFACE_LOCKING_TIMEOUT ) != B_OK )
    {
        return;
    }

    if( b_playlist_update )
    {
#if 0
        if( fPlaylistWindow->Lock() )
        {
            fPlaylistWindow->UpdatePlaylist( true );
            fPlaylistWindow->Unlock();
            b_playlist_update = false;
        }
#endif
        p_mediaControl->SetEnabled( p_playlist->i_size );
    }

    if( p_input )
    {
        vlc_value_t val;
        p_mediaControl->SetEnabled( true );
        bool hasTitles   = !var_Get( p_input, "title", &val );
        bool hasChapters = !var_Get( p_input, "chapter", &val );
        p_mediaControl->SetStatus( var_GetInteger( p_input, "state" ),
                                   var_GetInteger( p_input, "rate" ) );
        var_Get( p_input, "position", &val );
        p_mediaControl->SetProgress( val.f_float );
        _SetMenusEnabled( true, hasChapters, hasTitles );
        _UpdateSpeedMenu( var_GetInteger( p_input, "rate" ) );

        // enable/disable skip buttons
#if 0
        bool canSkipPrev;
        bool canSkipNext;
        p_wrapper->GetNavCapabilities( &canSkipPrev, &canSkipNext );
        p_mediaControl->SetSkippable( canSkipPrev, canSkipNext );
#endif

        audio_volume_t i_volume;
        aout_VolumeGet( p_intf, &i_volume );
        p_mediaControl->SetAudioEnabled( true );
        p_mediaControl->SetMuted( i_volume );
    }
    else
    {
        p_mediaControl->SetAudioEnabled( false );

        _SetMenusEnabled( false );

        if( !playlist_IsEmpty( p_playlist ) )
        {
            p_mediaControl->SetProgress( 0 );

#if 0
            // enable/disable skip buttons
            bool canSkipPrev;
            bool canSkipNext;
            p_wrapper->GetNavCapabilities( &canSkipPrev, &canSkipNext );
            p_mediaControl->SetSkippable( canSkipPrev, canSkipNext );
#endif
        }
        else
        {
            p_mediaControl->SetEnabled( false );
        }
    }

    Unlock();
    fLastUpdateTime = system_time();
}

/*****************************************************************************
 * InterfaceWindow::UpdatePlaylist
 *****************************************************************************/
void
InterfaceWindow::UpdatePlaylist()
{
    b_playlist_update = true;
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
    BMenuItem * toMark = NULL;

    switch( rate )
    {
        case ( INPUT_RATE_DEFAULT * 8 ):
            toMark = fHeighthMI;
            break;

        case ( INPUT_RATE_DEFAULT * 4 ):
            toMark = fQuarterMI;
            break;

        case ( INPUT_RATE_DEFAULT * 2 ):
            toMark = fHalfMI;
            break;

        case ( INPUT_RATE_DEFAULT ):
            toMark = fNormalMI;
            break;

        case ( INPUT_RATE_DEFAULT / 2 ):
            toMark = fTwiceMI;
            break;

        case ( INPUT_RATE_DEFAULT / 4 ):
            toMark = fFourMI;
            break;

        case ( INPUT_RATE_DEFAULT / 8 ):
            toMark = fHeightMI;
            break;
    }

    if ( toMark && !toMark->IsMarked() )
    {
        toMark->SetMarked( true );
    }
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
#if 0            
        if (fSettings->FindRect( "playlist frame", &frame ) == B_OK )
            set_window_pos( fPlaylistWindow, frame );
#endif 
        if (fSettings->FindRect( "messages frame", &frame ) == B_OK )
            set_window_pos( fMessagesWindow, frame );
        if (fSettings->FindRect( "settings frame", &frame ) == B_OK )
        {
            /* FIXME: Preferences resizing doesn't work correctly yet */
            frame.right = frame.left + fPreferencesWindow->Frame().Width();
            frame.bottom = frame.top + fPreferencesWindow->Frame().Height();
            set_window_pos( fPreferencesWindow, frame );
        }

        bool showing;
#if 0
        if ( fSettings->FindBool( "playlist showing", &showing ) == B_OK )
            launch_window( fPlaylistWindow, showing );
#endif    
        if ( fSettings->FindBool( "messages showing", &showing ) == B_OK )
            launch_window( fMessagesWindow, showing );
        if ( fSettings->FindBool( "settings showing", &showing ) == B_OK )
            launch_window( fPreferencesWindow, showing );
#if 0
        uint32 displayMode;
        if ( fSettings->FindInt32( "playlist display mode", (int32*)&displayMode ) == B_OK )
            fPlaylistWindow->SetDisplayMode( displayMode );
#endif
    }
}

/*****************************************************************************
 * InterfaceWindow::_StoreSettings
 *****************************************************************************/
void
InterfaceWindow::_StoreSettings()
{
    /* Save the volume */
    config_PutInt( p_intf, "volume", p_mediaControl->GetVolume() );
    config_SaveConfigFile( p_intf, "main" );

    /* Save the windows positions */
    if ( fSettings->ReplaceRect( "main frame", Frame() ) != B_OK )
        fSettings->AddRect( "main frame", Frame() );
#if 0
    if ( fPlaylistWindow->Lock() )
    {
        if (fSettings->ReplaceRect( "playlist frame", fPlaylistWindow->Frame() ) != B_OK)
            fSettings->AddRect( "playlist frame", fPlaylistWindow->Frame() );
        if (fSettings->ReplaceBool( "playlist showing", !fPlaylistWindow->IsHidden() ) != B_OK)
            fSettings->AddBool( "playlist showing", !fPlaylistWindow->IsHidden() );
        fPlaylistWindow->Unlock();
    }
#endif
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
#if 0
    uint32 displayMode = fPlaylistWindow->DisplayMode();
    if (fSettings->ReplaceInt32( "playlist display mode", displayMode ) != B_OK )
        fSettings->AddInt32( "playlist display mode", displayMode );
#endif
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
    return 0;
}

/*****************************************************************************
 * LanguageMenu::LanguageMenu
 *****************************************************************************/
LanguageMenu::LanguageMenu( intf_thread_t * _p_intf, const char * psz_name,
                            char * _psz_variable )
    : BMenu( psz_name )
{
    p_intf       = _p_intf;
    psz_variable = strdup( _psz_variable );
}

/*****************************************************************************
 * LanguageMenu::~LanguageMenu
 *****************************************************************************/
LanguageMenu::~LanguageMenu()
{
    free( psz_variable );
}

/*****************************************************************************
 * LanguageMenu::AttachedToWindow
 *****************************************************************************/
void LanguageMenu::AttachedToWindow()
{
    BMenuItem * item;

    // remove all items
    while( ( item = RemoveItem( 0L ) ) )
    {
        delete item;
    }

    SetRadioMode( true );

    input_thread_t * p_input = (input_thread_t *)
            vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    if( !p_input )
    {
        return;
    }

    vlc_value_t val_list, text_list;
    BMessage * message;
    int i_current;

    i_current = var_GetInteger( p_input, psz_variable );
    var_Change( p_input, psz_variable, VLC_VAR_GETLIST, &val_list, &text_list );
    for( int i = 0; i < val_list.p_list->i_count; i++ )
    {
        message = new BMessage( SELECT_CHANNEL );
        message->AddInt32( psz_variable, val_list.p_list->p_values[i].i_int );
        item = new BMenuItem( text_list.p_list->p_values[i].psz_string, message );
        if( val_list.p_list->p_values[i].i_int == i_current )
        {
            item->SetMarked( true );
        }
        AddItem( item );
    }
    var_Change( p_input, psz_variable, VLC_VAR_FREELIST, &val_list, &text_list );

    vlc_object_release( p_input );

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
    BMenuItem * item;
    while( ( item = RemoveItem( 0L ) ) )
    {
        delete item;
    }

    input_thread_t * p_input;
    p_input = (input_thread_t *)
        vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    if( !p_input )
    {
        return;
    }

    vlc_value_t val;
    BMessage * message;
    if( !var_Get( p_input, "title", &val ) )
    {
        vlc_value_t val_list, text_list;
        var_Change( p_input, "title", VLC_VAR_GETCHOICES,
                    &val_list, &text_list );

        for( int i = 0; i < val_list.p_list->i_count; i++ )
        {
            message = new BMessage( TOGGLE_TITLE );
            message->AddInt32( "index", val_list.p_list->p_values[i].i_int );
            item = new BMenuItem( text_list.p_list->p_values[i].psz_string,
                                  message );
            if( val_list.p_list->p_values[i].i_int == val.i_int )
            {
                item->SetMarked( true );
            }
            AddItem( item );
        }

        var_Change( p_input, "title", VLC_VAR_FREELIST,
                    &val_list, &text_list );
    }
    vlc_object_release( p_input );
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
    BMenuItem * item;
    while( ( item = RemoveItem( 0L ) ) )
    {
        delete item;
    }

    input_thread_t * p_input;
    p_input = (input_thread_t *)
        vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    if( !p_input )
    {
        return;
    }

    vlc_value_t val;
    BMessage * message;
    if( !var_Get( p_input, "chapter", &val ) )
    {
        vlc_value_t val_list, text_list;
        var_Change( p_input, "chapter", VLC_VAR_GETCHOICES,
                    &val_list, &text_list );

        for( int i = 0; i < val_list.p_list->i_count; i++ )
        {
            message = new BMessage( TOGGLE_CHAPTER );
            message->AddInt32( "index", val_list.p_list->p_values[i].i_int );
            item = new BMenuItem( text_list.p_list->p_values[i].psz_string,
                                  message );
            if( val_list.p_list->p_values[i].i_int == val.i_int )
            {
                item->SetMarked( true );
            }
            AddItem( item );
        }

        var_Change( p_input, "chapter", VLC_VAR_FREELIST,
                    &val_list, &text_list );
    }
    vlc_object_release( p_input );
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
