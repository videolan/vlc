/*****************************************************************************
 * vlcproc.cpp: VlcProc class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: vlcproc.cpp,v 1.14 2003/04/21 21:51:16 asmax Exp $
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
 *          Emmanuel Puig    <karibu@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/

#include <wx/wx.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/aout.h>
#include <vlc/vout.h>
#include <netutils.h>

//--- SKIN ------------------------------------------------------------------
#include "../os_api.h"
#include "event.h"
#include "banks.h"
#include "theme.h"
#include "../os_theme.h"
#include "themeloader.h"
#include "window.h"
#include "vlcproc.h"
#include "skin_common.h"
#include "wxdialogs.h"



//---------------------------------------------------------------------------
// VlcProc
//---------------------------------------------------------------------------
VlcProc::VlcProc( intf_thread_t *_p_intf )
{
    p_intf = _p_intf;
}
//---------------------------------------------------------------------------
bool VlcProc::EventProc( Event *evt )
{
    switch( evt->GetMessage() )
    {
        case VLC_STREAMPOS:
            MoveStream( evt->GetParam2() );
            return true;

        case VLC_VOLUME_CHANGE:
            ChangeVolume( evt->GetParam1(), evt->GetParam2() );
            return true;

        case VLC_FULLSCREEN:
            FullScreen();
            return true;

        case VLC_HIDE:
            for( list<SkinWindow *>::const_iterator win =
                    p_intf->p_sys->p_theme->WindowList.begin();
                 win != p_intf->p_sys->p_theme->WindowList.end(); win++ )
            {
                (*win)->OnStartThemeVisible = !(*win)->IsHidden();
            }
            p_intf->p_sys->i_close_status = (int)evt->GetParam1();
            OSAPI_PostMessage( NULL, WINDOW_CLOSE, 1, 0 );
            return true;

        case VLC_SHOW:
            for( list<SkinWindow *>::const_iterator win =
                    p_intf->p_sys->p_theme->WindowList.begin();
                 win != p_intf->p_sys->p_theme->WindowList.end(); win++ )
            {
                if( (*win)->OnStartThemeVisible )
                    OSAPI_PostMessage( (*win), WINDOW_OPEN, 1, 0 );
            }
            p_intf->p_sys->b_all_win_closed = false;
            return true;

        case VLC_OPEN:
            OpenFile( true );
            return true;

        case VLC_LOAD_SKIN:
            LoadSkin();
            return true;

        case VLC_DROP:
            DropFile( evt->GetParam1() );
            return true;

        case VLC_PLAY:
            PlayStream();
            return true;

        case VLC_PAUSE:
            PauseStream();
            return true;

        case VLC_STOP:
            StopStream();
            return true;

        case VLC_NEXT:
            NextStream();
            return true;

        case VLC_PREV:
            PrevStream();
            return true;

        case VLC_PLAYLIST_ADD_FILE:
            OpenFile( false );
            return true;

        case VLC_LOG_SHOW:
            p_intf->p_sys->MessagesDlg->Show(
                !p_intf->p_sys->MessagesDlg->IsShown() );
            return true;

        case VLC_LOG_CLEAR:
            return true;

        case VLC_PREFS_SHOW:
            p_intf->p_sys->PrefsDlg->Show(
                !p_intf->p_sys->PrefsDlg->IsShown() );
            return true;

        case VLC_INFO_SHOW:
            p_intf->p_sys->InfoDlg->Show(
                !p_intf->p_sys->InfoDlg->IsShown() );
            return true;

        case VLC_INTF_REFRESH:
            InterfaceRefresh( (bool)evt->GetParam2() );
            return true;

        case VLC_TEST_ALL_CLOSED:
            return EventProcEnd();

        case VLC_QUIT:
            return false;

        case VLC_CHANGE_TRAY:
            p_intf->p_sys->p_theme->ChangeTray();
            return true;

        case VLC_CHANGE_TASKBAR:
            p_intf->p_sys->p_theme->ChangeTaskbar();
            return true;

        case VLC_NET_ADDUDP:
            AddNetworkUDP( (int)evt->GetParam2() );
            return true;

        case VLC_NET_ADDCS:
            AddNetworkChannelServer( (char *)evt->GetParam1() );
            return true;

        default:
            return true;
    }
}
//---------------------------------------------------------------------------
bool VlcProc::EventProcEnd()
{
    if( p_intf->p_sys->b_all_win_closed )
        return true;

    list<SkinWindow *>::const_iterator win;

    // If a window has been closed, test if all are closed !
    for( win = p_intf->p_sys->p_theme->WindowList.begin();
         win != p_intf->p_sys->p_theme->WindowList.end(); win++ )
    {
        if( !(*win)->IsHidden() )   // Not all windows closed
        {
            return true;
        }
    }

    // All window are closed
    switch( p_intf->p_sys->i_close_status )
    {
        case VLC_QUIT:
            // Save config before exiting
            p_intf->p_sys->p_theme->SaveConfig();
            break;
    }

    // Send specified event
    OSAPI_PostMessage( NULL, p_intf->p_sys->i_close_status, 0, 0 );

    // Reset values
    p_intf->p_sys->i_close_status = VLC_NOTHING;
    p_intf->p_sys->b_all_win_closed = true;

    // Return true
    return true;
}
//---------------------------------------------------------------------------
bool VlcProc::IsClosing()
{
    if( p_intf->b_die && p_intf->p_sys->i_close_status != VLC_QUIT )
    {
        p_intf->p_sys->i_close_status = VLC_QUIT;
        OSAPI_PostMessage( NULL, VLC_HIDE, VLC_QUIT, 0 );
    }
    return true;
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// Private methods
//---------------------------------------------------------------------------
void VlcProc::InterfaceRefresh( bool All )
{
    // Shortcut pointers
    intf_sys_t  *Sys      = p_intf->p_sys;
    Theme       *Thema    = Sys->p_theme;
    playlist_t  *PlayList = Sys->p_playlist;

    // Refresh
    if( PlayList != NULL )
    {
        // Refresh stream control controls ! :)
        switch( PlayList->i_status )
        {
            case PLAYLIST_STOPPED:
                EnabledEvent( "time", false );
                EnabledEvent( "stop", false );
                EnabledEvent( "play", true );
                EnabledEvent( "pause", false );
                break;
            case PLAYLIST_RUNNING:
                EnabledEvent( "time", true );
                EnabledEvent( "stop", true );
                EnabledEvent( "play", false );
                EnabledEvent( "pause", true );
                break;
            case PLAYLIST_PAUSED:
                EnabledEvent( "time", true );
                EnabledEvent( "stop", true );
                EnabledEvent( "play", true );
                EnabledEvent( "pause", false );
                break;
        }

        // Refresh next and prev buttons
        if( PlayList->i_index == 0 || PlayList->i_size == 1 )
            EnabledEvent( "prev", false );
        else
            EnabledEvent( "prev", true );

        if( PlayList->i_index == PlayList->i_size - 1 || PlayList->i_size == 1 )
            EnabledEvent( "next", false );
        else
            EnabledEvent( "next", true );


        // Update file name text
        if( PlayList->i_index != Sys->i_index )
        {
            Thema->EvtBank->Get( "file_name" )->PostTextMessage(
                PlayList->pp_items[PlayList->i_index]->psz_name );
        }

        // Update playlists
        if( PlayList->i_index != Sys->i_index ||
            PlayList->i_size != Sys->i_size )
        {
            Thema->EvtBank->Get( "playlist_refresh" )->PostSynchroMessage();
            Sys->i_size  = PlayList->i_size;
            Sys->i_index = PlayList->i_index;
        }
    }
    else
    {
        EnabledEvent( "time", false );
        EnabledEvent( "stop",  false );
        EnabledEvent( "play",  false );
        EnabledEvent( "pause", false );
        EnabledEvent( "prev",  false );
        EnabledEvent( "next",  false );

        // Update playlists
        if( Sys->i_size > 0 )
        {
            Thema->EvtBank->Get( "playlist_refresh" )->PostSynchroMessage();
            Sys->i_size  = 0;
        }
    }

}
//---------------------------------------------------------------------------
void VlcProc::EnabledEvent( string type, bool state )
{
    OSAPI_PostMessage( NULL, CTRL_ENABLED, (unsigned int)
        p_intf->p_sys->p_theme->EvtBank->Get( type ), (int)state );
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// Common VLC procedures
//---------------------------------------------------------------------------
void VlcProc::LoadSkin()
{
    if( p_intf->p_sys->p_new_theme_file == NULL )
    {
        wxFileDialog dialog( NULL, _("Open a skin file"), "", "",
            "Skin files (*.vlt)|*.vlt|Skin files (*.xml)|*.xml|All files|*.*",
            wxOPEN );

        if( dialog.ShowModal() == wxID_OK )
        {
            p_intf->p_sys->p_new_theme_file =
                new char[dialog.GetPath().Length()];

            strcpy( p_intf->p_sys->p_new_theme_file,
                    dialog.GetPath().c_str() );

            // Tell vlc to change skin after hiding interface
            OSAPI_PostMessage( NULL, VLC_HIDE, VLC_LOAD_SKIN, 0 );
        }
    }
    else
    {
        // Place a new theme in the global structure, because it will
        // be filled by the parser
        // We save the old one to restore it in case of problem
        Theme *oldTheme = p_intf->p_sys->p_theme;
        p_intf->p_sys->p_theme = (Theme *)new OSTheme( p_intf );

        // Run the XML parser
        ThemeLoader *Loader = new ThemeLoader( p_intf );
        if( Loader->Load( p_intf->p_sys->p_new_theme_file ) )
        {
            // Everything went well
            msg_Dbg( p_intf, "New theme successfully loaded" );
            delete (OSTheme *)oldTheme;

            // Show the theme
            p_intf->p_sys->p_theme->InitTheme();
            p_intf->p_sys->p_theme->ShowTheme();
        }
        else
        {
            msg_Warn( p_intf, "A problem occurred when loading the new theme,"
                      " restoring the previous one" );
            delete (OSTheme *)p_intf->p_sys->p_theme;
            p_intf->p_sys->p_theme = oldTheme;

            // Show the theme
            p_intf->p_sys->p_theme->ShowTheme();
        }
        delete Loader;

        // Uninitialize new theme
        delete (char *)p_intf->p_sys->p_new_theme_file;
        p_intf->p_sys->p_new_theme_file = NULL;
    }
}
//---------------------------------------------------------------------------
void VlcProc::OpenFile( bool play )
{
    if( p_intf->p_sys->OpenDlg->ShowModal() != wxID_OK )
    {
        return;
    }

    // Check if playlist is available
    playlist_t *p_playlist = p_intf->p_sys->p_playlist;
    if( p_playlist == NULL )
    {
        return;
    }

    if( play )
    {
        // Append and play
        playlist_Add( p_playlist,
                      (char *)p_intf->p_sys->OpenDlg->mrl.c_str(),
                      PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );

        p_intf->p_sys->p_theme->EvtBank->Get( "play" )->SendEvent();
    }
    else
    {
        // Append only
        playlist_Add( p_playlist,
                        (char *)p_intf->p_sys->OpenDlg->mrl.c_str(),
                        PLAYLIST_APPEND, PLAYLIST_END );
    }

    // Refresh interface !
    p_intf->p_sys->p_theme->EvtBank->Get( "playlist_refresh" )
        ->PostSynchroMessage();
    InterfaceRefresh();
}
//---------------------------------------------------------------------------
void VlcProc::DropFile( unsigned int param )
{
    // Get pointer to file
    char *FileName = (char *)param;

    // Add the new file to the playlist
    if( p_intf->p_sys->p_playlist != NULL )
    {
        if( config_GetInt( p_intf, "enqueue" ) )
        {
            playlist_Add( p_intf->p_sys->p_playlist, FileName,
                          PLAYLIST_APPEND, PLAYLIST_END );
        }
        else
        {
            playlist_Add( p_intf->p_sys->p_playlist, FileName,
                          PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
        }
    }

    // VLC_DROP must be called with a pointer to a char else it will
    // ******** SEGFAULT ********
    // The delete is here because the processus in asynchronous
    delete[] FileName;

    // Refresh interface
    InterfaceRefresh();

}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// Stream Control
//---------------------------------------------------------------------------
void VlcProc::PauseStream()
{
    if( p_intf->p_sys->p_input == NULL )
        return;
    input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PAUSE );

    // Refresh interface
    InterfaceRefresh();
}
//---------------------------------------------------------------------------
void VlcProc::PlayStream()
{
    if( p_intf->p_sys->p_playlist == NULL )
        return;
    if( !p_intf->p_sys->p_playlist->i_size )
        return;

    playlist_Play( p_intf->p_sys->p_playlist );

    // Refresh interface
    InterfaceRefresh();
}
//---------------------------------------------------------------------------
void VlcProc::StopStream()
{
    if( p_intf->p_sys->p_playlist == NULL )
        return;
    playlist_Stop( p_intf->p_sys->p_playlist );

    // Refresh interface
    InterfaceRefresh();
}
//---------------------------------------------------------------------------
void VlcProc::NextStream()
{
    if( p_intf->p_sys->p_playlist == NULL )
        return;

    playlist_Next( p_intf->p_sys->p_playlist );

    // Refresh interface
    InterfaceRefresh();
}
//---------------------------------------------------------------------------
void VlcProc::PrevStream()
{
    if( p_intf->p_sys->p_playlist == NULL )
        return;

    playlist_Prev( p_intf->p_sys->p_playlist );

    // Refresh interface
    InterfaceRefresh();
}
//---------------------------------------------------------------------------
void VlcProc::MoveStream( long Pos )
{
    if( p_intf->p_sys->p_input == NULL )
        return;

    off_t i_seek = (off_t)(Pos *
        p_intf->p_sys->p_input->stream.p_selected_area->i_size
        / SLIDER_RANGE);

    input_Seek( p_intf->p_sys->p_input, i_seek, INPUT_SEEK_SET );

    // Refresh interface
    InterfaceRefresh();
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// Fullscreen
//---------------------------------------------------------------------------
void VlcProc::FullScreen()
{
    vout_thread_t *p_vout;

    if( p_intf->p_sys->p_input == NULL )
        return;

    p_vout = (vout_thread_t *)vlc_object_find( p_intf->p_sys->p_input,
                                               VLC_OBJECT_VOUT, FIND_CHILD );
    if( p_vout == NULL )
        return;

    p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
    vlc_object_release( p_vout );
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// Volume Control
//---------------------------------------------------------------------------
void VlcProc::ChangeVolume( unsigned int msg, long param )
{
    audio_volume_t volume;
    switch( msg )
    {
        case VLC_VOLUME_MUTE:
            aout_VolumeMute( p_intf, NULL );
            break;
        case VLC_VOLUME_UP:
            aout_VolumeUp( p_intf, 1, NULL );
            break;
        case VLC_VOLUME_DOWN:
            aout_VolumeDown( p_intf, 1, NULL );
            break;
        case VLC_VOLUME_SET:
            aout_VolumeSet( p_intf, param * AOUT_VOLUME_MAX / SLIDER_RANGE );
            break;
    }
    aout_VolumeGet( p_intf, &volume );

    OSAPI_PostMessage( NULL, CTRL_SET_SLIDER,
        (unsigned int)
            p_intf->p_sys->p_theme->EvtBank->Get( "volume_refresh" ),
        (int)( volume * SLIDER_RANGE / AOUT_VOLUME_MAX ) );
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// Network
//---------------------------------------------------------------------------
void VlcProc::AddNetworkUDP( int port )
{
    config_PutInt( p_intf, "network-channel", VLC_FALSE );

    // Build source name
    char *s_port = new char[5];
    sprintf( s_port, "%i", port );
    string source = "udp:@:" + (string)s_port;
    delete[] s_port;

    playlist_Add( p_intf->p_sys->p_playlist, (char *)source.c_str(),
        PLAYLIST_APPEND, PLAYLIST_END );

    // Refresh interface !
    p_intf->p_sys->p_theme->EvtBank->Get( "playlist_refresh" )
        ->PostSynchroMessage();
    InterfaceRefresh();
}
//---------------------------------------------------------------------------
void VlcProc::AddNetworkChannelServer( char *server )
{
    char *name = new char[MAX_PARAM_SIZE];
    int  port = 0;

    // Scan the server address
    int scan = sscanf( server, "%[^:]:%i", name, &port );

    if( scan != 2)
    {
        msg_Err( p_intf, "Invalid channel server: %s", server );
        delete[] name;
        return;
    }

    config_PutInt( p_intf, "network-channel", VLC_TRUE );
    config_PutPsz( p_intf, "channel-server", name );
    config_PutInt( p_intf, "channel-port", port );

    if( p_intf->p_vlc->p_channel == NULL )
    {
        network_ChannelCreate( p_intf );
    }

    delete[] name;

}
//---------------------------------------------------------------------------

