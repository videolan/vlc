/*****************************************************************************
 * mainframe.cpp: Win32 interface plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
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

#include <vcl.h>
#pragma hdrstop

#include <videolan/vlc.h>

#include "stream_control.h"
#include "input_ext-intf.h"

#include "video.h"
#include "video_output.h"

#include "interface.h"
#include "intf_playlist.h"
#include "intf_eject.h"

#include "mainframe.h"
#include "menu.h"
#include "control.h"
#include "disc.h"
#include "network.h"
#include "about.h"
#include "preferences.h"
#include "messages.h"
#include "playlist.h"
#include "win32_common.h"

#include "netutils.h"

//---------------------------------------------------------------------------
//#pragma package(smart_init)
#pragma resource "*.dfm"

extern struct intf_thread_s *p_intfGlobal;
extern int Win32Manage( intf_thread_t *p_intf );

//---------------------------------------------------------------------------
__fastcall TMainFrameDlg::TMainFrameDlg( TComponent* Owner )
        : TForm( Owner )
{
    Application->ShowHint = true;
    Application->OnHint = DisplayHint;

    TimerManage->Interval = INTF_IDLE_SLEEP / 1000;

    TrackBar->Max = SLIDER_MAX_VALUE;

    /* default height */
    ClientHeight = 37 + ToolBar->Height;

    StringListPref = new TStringList();
}
//---------------------------------------------------------------------------
__fastcall TMainFrameDlg::~TMainFrameDlg()
{
    delete StringListPref;
}
//---------------------------------------------------------------------------


/*****************************************************************************
 * Event handlers
 ****************************************************************************/
void __fastcall TMainFrameDlg::TimerManageTimer( TObject *Sender )
{
    Win32Manage( p_intfGlobal );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::DisplayHint( TObject *Sender )
{
    StatusBar->SimpleText = GetLongHint( Application->Hint );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::TrackBarChange( TObject *Sender )
{
    /* This function displays the current date related to the position in
     * the stream. It is called whenever the slider changes its value.
     * The lock has to be taken before the function is called */

//    vlc_mutex_lock( &p_input_bank->pp_input[0]->stream.stream_lock );

    if( p_input_bank->pp_input[0] != NULL )
    {
#define p_area p_input_bank->pp_input[0]->stream.p_selected_area
        char psz_time[ OFFSETTOTIME_MAX_SIZE ];
        off_t Value = TrackBar->Position;

        GroupBoxSlider->Caption =
                input_OffsetToTime( p_input_bank->pp_input[0], psz_time,
                        ( p_area->i_size * Value ) / (off_t)SLIDER_MAX_VALUE );
#undef p_area
     }

//    vlc_mutex_unlock( &p_input_bank->pp_input[0]->stream.stream_lock );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::FormClose( TObject *Sender,
      TCloseAction &Action )
{
    intf_thread_t *p_intf = p_intfGlobal;

    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->b_die = 1;
    vlc_mutex_unlock( &p_intf->change_lock );

    /* we don't destroy the form immediatly */
    Action = caHide;
}
//---------------------------------------------------------------------------


/*****************************************************************************
 * Menu callbacks
 ****************************************************************************/
void __fastcall TMainFrameDlg::MenuOpenFileClick( TObject *Sender )
{
    int             i_end = p_main->p_playlist->i_size;
    AnsiString      FileName;
    if( OpenDialog1->Execute() )
    {
        /* add the new file to the interface playlist */
        FileName = OpenDialog1->FileName;
        intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END,
            (char*)FileName.c_str() );

        /* update the plugin display */
        p_intfGlobal->p_sys->p_playlist->UpdateGrid( p_main->p_playlist );

        /* end current item, select added item  */
        if( p_input_bank->pp_input[0] != NULL )
        {
            p_input_bank->pp_input[0]->b_eof = 1;
        }

        intf_PlaylistJumpto( p_main->p_playlist, i_end - 1 );
    };
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::MenuOpenDiscClick( TObject *Sender )
{
    TDiscDlg *p_disc = p_intfGlobal->p_sys->p_disc;
    if( p_disc == NULL )
    {
        p_disc = new TDiscDlg( this );
        p_intfGlobal->p_sys->p_disc = p_disc;
    }
    p_disc->Show();
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::MenuNetworkStreamClick( TObject *Sender )
{
    TNetworkDlg *p_network = p_intfGlobal->p_sys->p_network;
    if( p_network == NULL )
    {
        p_network = new TNetworkDlg( this );
        p_intfGlobal->p_sys->p_network = p_network;
    }
    p_network->Show();
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::MenuExitClick( TObject *Sender )
{
    Close();
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::MenuFullscreenClick( TObject *Sender )
{
    if( p_vout_bank->i_count )
    {
        vlc_mutex_lock( &p_vout_bank->pp_vout[0]->change_lock );

        p_vout_bank->pp_vout[0]->i_changes |= VOUT_FULLSCREEN_CHANGE;

        vlc_mutex_unlock( &p_vout_bank->pp_vout[0]->change_lock );
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::MenuPlaylistClick( TObject *Sender )
{
    TPlaylistDlg *p_playlist = p_intfGlobal->p_sys->p_playlist;
    if( p_playlist->Visible )
    {
        p_playlist->Hide();
    }
    else
    {
        p_playlist->UpdateGrid( p_main->p_playlist );
        p_playlist->Show();
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::MenuMessagesClick( TObject *Sender )
{
     p_intfGlobal->p_sys->p_messages->Show();
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::MenuPreferencesClick( TObject *Sender )
{
    CreatePreferences( "main" );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::MenuAboutClick( TObject *Sender )
{
    p_intfGlobal->p_sys->p_about = new TAboutDlg( this );
    p_intfGlobal->p_sys->p_about->ShowModal();
    delete p_intfGlobal->p_sys->p_about;
}
//---------------------------------------------------------------------------


/*****************************************************************************
 * Toolbar callbacks
 ****************************************************************************/
void __fastcall TMainFrameDlg::ToolButtonFileClick( TObject *Sender )
{
    MenuOpenFileClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::ToolButtonDiscClick( TObject *Sender )
{
    MenuOpenDiscClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::ToolButtonNetClick( TObject *Sender )
{
    MenuNetworkStreamClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::ToolButtonPlaylistClick( TObject *Sender )
{
    MenuPlaylistClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::ToolButtonBackClick( TObject *Sender )
{
    ControlBack( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::ToolButtonStopClick( TObject *Sender )
{
    ControlStop( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::ToolButtonPlayClick( TObject *Sender )
{
    ControlPlay( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::ToolButtonPauseClick( TObject *Sender )
{
    ControlPause( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::ToolButtonSlowClick( TObject *Sender )
{
    ControlSlow( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::ToolButtonFastClick( TObject *Sender )
{
    ControlFast( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::ToolButtonPrevClick( TObject *Sender )
{
    p_intfGlobal->p_sys->p_playlist->Previous();
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::ToolButtonNextClick( TObject *Sender )
{
    p_intfGlobal->p_sys->p_playlist->Next();
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::ToolButtonEjecttempClick( TObject *Sender )
{
    AnsiString Device = "";

    /*
     * Get the active input
     * Determine whether we can eject a media, ie it's a VCD or DVD
     * If it's neither a VCD nor a DVD, then return
     */

    if( p_main->p_playlist->current.psz_name != NULL )
    {
        if( strncmp( p_main->p_playlist->current.psz_name, "dvd", 3 )
            || strncmp( p_main->p_playlist->current.psz_name, "vcd", 3 ) )
        {
            /* Determine the device name by omitting the first 4 characters
             * and keeping 3 characters */
            Device = strdup( ( p_main->p_playlist->current.psz_name + 4 ) );
            Device = Device.SubString( 1, 2 );
        }
    }

    if( Device == "" )
    {
        return;
    }

    /* If there's a stream playing, we aren't allowed to eject ! */
    if( p_input_bank->pp_input[0] == NULL )
    {
        intf_WarnMsg( 4, "intf: ejecting %s", Device.c_str() );

        intf_Eject( Device.c_str() );
    }
}
//--------------------------------------------------------------------------


/*****************************************************************************
 * Popup callbacks
 ****************************************************************************/
void __fastcall TMainFrameDlg::PopupCloseClick( TObject *Sender )
{
    /* We do nothing, we just need a click on a menu item
     * to close the popup. Don't ask me why... */
    return;
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PopupPlayClick( TObject *Sender )
{
    ToolButtonPlayClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PopupPauseClick( TObject *Sender )
{
    ToolButtonPauseClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PopupStopClick( TObject *Sender )
{
    ToolButtonStopClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PopupBackClick( TObject *Sender )
{
    ToolButtonBackClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PopupSlowClick( TObject *Sender )
{
    ToolButtonSlowClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PopupFastClick( TObject *Sender )
{
    ToolButtonFastClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PopupToggleInterfaceClick( TObject *Sender )
{
    this->BringToFront();
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PopupFullscreenClick( TObject *Sender )
{
    MenuFullscreenClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PopupNextClick( TObject *Sender )
{
    ToolButtonNextClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PopupPrevClick( TObject *Sender )
{
    ToolButtonPrevClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PopupJumpClick( TObject *Sender )
{
    // TODO
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PopupPlaylistClick( TObject *Sender )
{
    MenuPlaylistClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PopupPreferencesClick( TObject *Sender )
{
    MenuPreferencesClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PopupExitClick( TObject *Sender )
{
    MenuExitClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PopupOpenFileClick( TObject *Sender )
{
    MenuOpenFileClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PopupOpenDiscClick( TObject *Sender )
{
    MenuOpenDiscClick( Sender );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PopupNetworkStreamClick( TObject *Sender )
{
    MenuNetworkStreamClick( Sender );
}
//---------------------------------------------------------------------------


/*****************************************************************************
 * Callbacks for DVD/VCD navigation
 ****************************************************************************/
void __fastcall TMainFrameDlg::ButtonTitlePrevClick( TObject *Sender )
{
    intf_thread_t * p_intf;
    input_area_t  * p_area;
    int             i_id;

    p_intf = p_intfGlobal;
    i_id = p_input_bank->pp_input[0]->stream.p_selected_area->i_id - 1;

    /* Disallow area 0 since it is used for video_ts.vob */
    if( i_id > 0 )
    {
        p_area = p_input_bank->pp_input[0]->stream.pp_areas[i_id];
        input_ChangeArea( p_input_bank->pp_input[0], (input_area_t*)p_area );

        input_SetStatus( p_input_bank->pp_input[0], INPUT_STATUS_PLAY );

        p_intf->p_sys->b_title_update = 1;
        vlc_mutex_lock( &p_input_bank->pp_input[0]->stream.stream_lock );
        SetupMenus( p_intf );
        vlc_mutex_unlock( &p_input_bank->pp_input[0]->stream.stream_lock );
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::ButtonTitleNextClick( TObject *Sender )
{
    intf_thread_t * p_intf;
    input_area_t  * p_area;
    int             i_id;

    p_intf = p_intfGlobal;
    i_id = p_input_bank->pp_input[0]->stream.p_selected_area->i_id + 1;

    if( i_id < p_input_bank->pp_input[0]->stream.i_area_nb )
    {
        p_area = p_input_bank->pp_input[0]->stream.pp_areas[i_id];   
        input_ChangeArea( p_input_bank->pp_input[0], (input_area_t*)p_area );
                  
        input_SetStatus( p_input_bank->pp_input[0], INPUT_STATUS_PLAY );

        p_intf->p_sys->b_title_update = 1;
        vlc_mutex_lock( &p_input_bank->pp_input[0]->stream.stream_lock );
        SetupMenus( p_intf );
        vlc_mutex_unlock( &p_input_bank->pp_input[0]->stream.stream_lock );
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::ButtonChapterPrevClick( TObject *Sender )
{
    intf_thread_t * p_intf = p_intfGlobal;
    input_area_t  * p_area;

    p_area = p_input_bank->pp_input[0]->stream.p_selected_area;

    if( p_area->i_part > 0 )
    {
        p_area->i_part--;
        input_ChangeArea( p_input_bank->pp_input[0], (input_area_t*)p_area );

        input_SetStatus( p_input_bank->pp_input[0], INPUT_STATUS_PLAY );

        p_intf->p_sys->b_chapter_update = 1;
        vlc_mutex_lock( &p_input_bank->pp_input[0]->stream.stream_lock );
        SetupMenus( p_intf );
        vlc_mutex_unlock( &p_input_bank->pp_input[0]->stream.stream_lock );
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::ButtonChapterNextClick( TObject *Sender )
{
    intf_thread_t * p_intf = p_intfGlobal;
    input_area_t  * p_area;

    p_area = p_input_bank->pp_input[0]->stream.p_selected_area;
    
    if( p_area->i_part < p_area->i_part_nb )
    {
        p_area->i_part++;
        input_ChangeArea( p_input_bank->pp_input[0], (input_area_t*)p_area );

        input_SetStatus( p_input_bank->pp_input[0], INPUT_STATUS_PLAY );

        p_intf->p_sys->b_chapter_update = 1;
        vlc_mutex_lock( &p_input_bank->pp_input[0]->stream.stream_lock );
        SetupMenus( p_intf );
        vlc_mutex_unlock( &p_input_bank->pp_input[0]->stream.stream_lock );
    }
}
//---------------------------------------------------------------------------


/*****************************************************************************
 * Callback for the 'go!' button
 ****************************************************************************/
void __fastcall TMainFrameDlg::ButtonGoClick( TObject *Sender )
{
    intf_thread_t *p_intf = p_intfGlobal;
    int i_channel;

    i_channel = UpDownChannel->Position;
    intf_WarnMsg( 3, "intf info: joining channel %d", i_channel );

    vlc_mutex_lock( &p_intf->change_lock );
    if( p_input_bank->pp_input[0] != NULL )
    {
        /* end playing item */
        p_input_bank->pp_input[0]->b_eof = 1;

        /* update playlist */
        vlc_mutex_lock( &p_main->p_playlist->change_lock );

        p_main->p_playlist->i_index--;
        p_main->p_playlist->b_stopped = 1;

        vlc_mutex_unlock( &p_main->p_playlist->change_lock );

        /* FIXME: ugly hack to close input and outputs */
        p_intf->pf_manage( p_intf );
    }

    network_ChannelJoin( i_channel );

    /* FIXME 2 */
    p_main->p_playlist->b_stopped = 0;
    p_intf->pf_manage( p_intf );

    vlc_mutex_unlock( &p_intf->change_lock );

//    input_SetStatus( p_input_bank->pp_input[0], INPUT_STATUS_PLAY );
}
//---------------------------------------------------------------------------


/*****************************************************************************
 * ModeManage: actualise the aspect of the interface whenever the input
 *             changes.
 *****************************************************************************
 * The lock has to be taken before you call the function.
 *****************************************************************************/
void __fastcall TMainFrameDlg::ModeManage()
{
    intf_thread_t * p_intf = p_intfGlobal;
    TGroupBox     * ActiveGB;
    int             i_Height;
    bool            b_control;

    /* hide all boxes */
    GroupBoxFile->Visible = false;
    GroupBoxNetwork->Visible = false;
    GroupBoxDisc->Visible = false;

    /* hide slider */
    GroupBoxSlider->Hide();

    /* controls unavailable */
    b_control = 0;

    /* show the box related to current input mode */
    if( p_input_bank->pp_input[0] != NULL )
    {
        switch( p_input_bank->pp_input[0]->stream.i_method & 0xf0 )
        {    
            case INPUT_METHOD_FILE:
                GroupBoxFile->Visible = true;
                ActiveGB = GroupBoxFile;
                LabelFileName->Caption = p_input_bank->pp_input[0]->psz_source;
                break;
            case INPUT_METHOD_DISC:
                GroupBoxDisc->Visible = true;
                ActiveGB = GroupBoxDisc;
                break;
            case INPUT_METHOD_NETWORK:
                GroupBoxNetwork->Visible = true;
                ActiveGB = GroupBoxNetwork;
                LabelServer->Caption = p_input_bank->pp_input[0]->psz_source;
                if( config_GetIntVariable( "network_channel" ) )
                {
                    LabelChannel->Visible = true;
                }
                else
                {
                    LabelChannel->Visible = false;
                }
                break;
            default:
                intf_WarnMsg( 3, "intf: can't determine input method" );
                GroupBoxFile->Visible = true;
                ActiveGB = GroupBoxFile;
                LabelFileName->Caption = p_input_bank->pp_input[0]->psz_source;
                break;
        }

        i_Height = StatusBar->Height + ActiveGB->Height + ToolBar->Height + 47;

        /* initialize and show slider for seekable streams */
        if( p_input_bank->pp_input[0]->stream.b_seekable )
        {
            TrackBar->Position = p_intf->p_sys->OldValue = 0;
            GroupBoxSlider->Show();
            i_Height += GroupBoxSlider->Height;
        }

        /* control buttons for free pace streams */
        b_control = p_input_bank->pp_input[0]->stream.b_pace_control;

        /* get ready for menu regeneration */
        p_intf->p_sys->b_program_update = 1;
        p_intf->p_sys->b_title_update = 1;
        p_intf->p_sys->b_chapter_update = 1;
        p_intf->p_sys->b_audio_update = 1;
        p_intf->p_sys->b_spu_update = 1;
        p_intf->p_sys->i_part = 0;

        p_input_bank->pp_input[0]->stream.b_changed = 0;
        intf_WarnMsg( 3, "intf: stream has changed, refreshing interface" );
    }
    else
    {
        i_Height = StatusBar->Height + ToolBar->Height + 47;

        if( config_GetIntVariable( "network_channel" ) )
        {
            GroupBoxNetwork->Visible = true;
            LabelChannel->Visible = true;
            i_Height += GroupBoxNetwork->Height;
        }
        else
        {
            /* unsensitize menus */
            MenuProgram->Enabled = false;
            MenuTitle->Enabled = false;
            MenuChapter->Enabled = false;
            MenuAudio->Enabled = false;
            MenuSubtitles->Enabled = false;
            PopupNavigation->Enabled = false;
            PopupAudio->Enabled = false;
            PopupSubtitles->Enabled = false;
        }
    }

    /* resize main window */
    this->Height = i_Height;

    /* set control items */
    ToolButtonBack->Enabled = false;
    ToolButtonStop->Enabled = true;
    ToolButtonEject->Enabled = !b_control;
    ToolButtonPause->Enabled = b_control;
    ToolButtonSlow->Enabled = b_control;
    ToolButtonFast->Enabled = b_control;
    PopupBack->Enabled = false;
    PopupPause->Enabled = b_control;
    PopupSlow->Enabled = b_control;
    PopupFast->Enabled = b_control;
}
//---------------------------------------------------------------------------


/*****************************************************************************
 * CreateConfig: create a configuration dialog and save it for further use
 *****************************************************************************
 * Check if the dialog box is already opened, if so this will save us
 * quite a bit of work. (the interface will be destroyed when you actually
 * close the main window, but remember that it is only hidden if you
 * clicked on the action buttons). This trick also allows us not to
 * duplicate identical dialog windows.
 *****************************************************************************/
void __fastcall TMainFrameDlg::CreatePreferences( AnsiString Name )
{
    TPreferencesDlg *Preferences;
    int i_index, i_pos;

    i_index = StringListPref->IndexOf( Name );
    if( i_index != -1 )
    {
        /* config dialog already exists */
        Preferences = (TPreferencesDlg *)StringListPref->Objects[i_index];
    }
    else
    {
        /* create the config dialog */
        Preferences = new TPreferencesDlg( this );
        Preferences->CreateConfigDialog( Name.c_str() );

        /* save it */
        i_pos = StringListPref->Add( Name );
        StringListPref->Objects[i_pos] = Preferences;
    }

    /* display the dialog */
    Preferences->Show();
}
//---------------------------------------------------------------------------


