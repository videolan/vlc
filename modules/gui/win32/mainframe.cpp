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

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>

#include "dragdrop.h"
#include "mainframe.h"
#include "menu.h"
#include "disc.h"
#include "network.h"
#include "about.h"
#include "preferences.h"
#include "messages.h"
#include "playlist.h"
#include "misc.h"
#include "win32_common.h"

#include "netutils.h"

//---------------------------------------------------------------------------
#pragma link "CSPIN"
#pragma resource "*.dfm"

extern int Win32Manage( intf_thread_t *p_intf );

//---------------------------------------------------------------------------
__fastcall TMainFrameDlg::TMainFrameDlg(
    TComponent* Owner, intf_thread_t *_p_intf ) : TForm( Owner )
{
    p_intf = _p_intf;

    Application->ShowHint = true;
    Application->OnHint = DisplayHint;

    TimerManage->Interval = INTF_IDLE_SLEEP / 1000;

    TrackBar->Max = SLIDER_MAX_VALUE;

    /* default height and caption */
    ClientHeight = 37 + ToolBar->Height;
    Caption = VOUT_TITLE " (Win32 interface)";

    StringListPref = new TStringList();

    Translate( this );

    /* drag and drop stuff */

    /* initialize the OLE library */
    OleInitialize( NULL );
    /* TDropTarget will send the WM_OLEDROP message to the form */
    lpDropTarget = (LPDROPTARGET)new TDropTarget( this->Handle );
    CoLockObjectExternal( lpDropTarget, true, true );
    /* register the form as a drop target */
    RegisterDragDrop( this->Handle, lpDropTarget );
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
    Win32Manage( p_intf );
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

    if( p_intf->p_sys->p_input != NULL )
    {
#define p_area p_intf->p_sys->p_input->stream.p_selected_area
        char psz_time[ OFFSETTOTIME_MAX_SIZE ];
        off_t Value = TrackBar->Position;

        GroupBoxSlider->Caption =
                input_OffsetToTime( p_intf->p_sys->p_input, psz_time,
                        ( p_area->i_size * Value ) / (off_t)SLIDER_MAX_VALUE );
#undef p_area
     }
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::FormClose( TObject *Sender,
      TCloseAction &Action )
{
    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->p_vlc->b_die = VLC_TRUE;
    vlc_mutex_unlock( &p_intf->change_lock );

    /* remove the form from the list of drop targets */
    RevokeDragDrop( this->Handle );
    lpDropTarget->Release();
    CoLockObjectExternal( lpDropTarget, false, true );

    /* uninitialize the OLE library */
    OleUninitialize();

    /* we don't destroy the form immediatly */
    Action = caHide;
}
//---------------------------------------------------------------------------


/*****************************************************************************
 * Main callbacks
 ****************************************************************************/
void __fastcall TMainFrameDlg::OpenFileActionExecute( TObject *Sender )
{
    if( OpenDialog1->Execute() )
    {
        /* add the new file to the interface playlist */
        for ( int i = 0 ; i < OpenDialog1->Files->Count ; i++ )
            p_intf->p_sys->p_playwin->Add( OpenDialog1->Files->Strings[i],
                    PLAYLIST_APPEND
                    | ( p_intf->p_sys->b_play_when_adding ? PLAYLIST_GO : 0 ),
                    PLAYLIST_END );
    };
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::OpenDiscActionExecute( TObject *Sender )
{
    TDiscDlg *p_disc = p_intf->p_sys->p_disc;
    if( p_disc == NULL )
    {
        p_disc = new TDiscDlg( this, p_intf );
        p_intf->p_sys->p_disc = p_disc;
    }
    p_disc->Show();
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::NetworkStreamActionExecute( TObject *Sender )
{
    TNetworkDlg *p_network = p_intf->p_sys->p_network;
    if( p_network == NULL )
    {
        p_network = new TNetworkDlg( this, p_intf );
        p_intf->p_sys->p_network = p_network;
    }
    p_network->Show();
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::ExitActionExecute( TObject *Sender )
{
    Close();
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::FullscreenActionExecute( TObject *Sender )
{
    vout_thread_t *p_vout;

    p_vout = (vout_thread_t *)vlc_object_find( p_intf->p_sys->p_input,
                                               VLC_OBJECT_VOUT, FIND_CHILD );
    if( p_vout == NULL )
    {
        return;
    }

    p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
    vlc_object_release( p_vout );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PlaylistActionExecute( TObject *Sender )
{
    TPlaylistDlg *p_playwin = p_intf->p_sys->p_playwin;
    if( p_playwin->Visible )
    {
        p_playwin->Hide();
    }
    else
    {
        p_playwin->UpdateGrid();
        p_playwin->Show();
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::MessagesActionExecute( TObject *Sender )
{
     p_intf->p_sys->p_messages->Show();
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PreferencesActionExecute( TObject *Sender )
{
    CreatePreferences( "main" );
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::AboutActionExecute( TObject *Sender )
{
    TAboutDlg *AboutDlg = new TAboutDlg( this, p_intf );
    AboutDlg->ShowModal();
    delete AboutDlg;
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::BackActionExecute( TObject *Sender )
{
    /* TODO */
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PlayActionExecute( TObject *Sender )
{
    p_intf->p_sys->p_playwin->Play();
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PauseActionExecute( TObject *Sender )
{
    p_intf->p_sys->p_playwin->Pause();
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::StopActionExecute( TObject *Sender )
{
    p_intf->p_sys->p_playwin->Stop();
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::SlowActionExecute( TObject *Sender )
{
    p_intf->p_sys->p_playwin->Slow();
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::FastActionExecute( TObject *Sender )
{
    p_intf->p_sys->p_playwin->Fast();
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PreviousActionExecute(TObject *Sender)
{
    p_intf->p_sys->p_playwin->Previous();
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::NextActionExecute(TObject *Sender)
{
    p_intf->p_sys->p_playwin->Next();
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::EjectActionExecute( TObject *Sender )
{
    AnsiString Device = "";
    char * psz_current;
    playlist_t * p_playlist;

    p_playlist = (playlist_t *)vlc_object_find( p_intf,
                                       VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    /*
     * Get the active input
     * Determine whether we can eject a media, ie it's a VCD or DVD
     * If it's neither a VCD nor a DVD, then return
     */

    vlc_mutex_lock( &p_playlist->object_lock );
    psz_current = p_playlist->pp_items[ p_playlist->i_index ]->psz_name;

    if( psz_current != NULL )
    {
        if( strncmp( psz_current, "dvd", 3 )
            || strncmp( psz_current, "vcd", 3 ) )
        {
            /* Determine the device name by omitting the first 4 characters
             * and keeping 3 characters */
            Device = strdup( ( psz_current + 4 ) );
            Device = Device.SubString( 1, 2 );
        }
    }

    vlc_mutex_unlock( &p_playlist->object_lock );
    vlc_object_release( p_playlist );

    if( Device == "" )
    {
        return;
    }

    /* If there's a stream playing, we aren't allowed to eject ! */
    if( p_intf->p_sys->p_input == NULL )
    {
        msg_Dbg( p_intf, "ejecting %s", Device.c_str() );

        intf_Eject( p_intf, Device.c_str() );
    }
}
//--------------------------------------------------------------------------
void __fastcall TMainFrameDlg::VolumeUpActionExecute( TObject *Sender )
{
    aout_instance_t *p_aout;
    p_aout = (aout_instance_t *)vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                                 FIND_ANYWHERE );
    if ( p_aout != NULL )
    {
        aout_VolumeUp( p_aout, 1, NULL );
        vlc_object_release( (vlc_object_t *)p_aout );
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::VolumeDownActionExecute( TObject *Sender )
{
    aout_instance_t *p_aout;
    p_aout = (aout_instance_t *)vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                                 FIND_ANYWHERE );
    if ( p_aout != NULL )
    {
        aout_VolumeDown( p_aout, 1, NULL );
        vlc_object_release( (vlc_object_t *)p_aout );
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::MuteActionExecute( TObject *Sender )
{
    aout_instance_t *p_aout;
    p_aout = (aout_instance_t *)vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                                 FIND_ANYWHERE );
    if ( p_aout != NULL )
    {
        aout_VolumeMute( p_aout, NULL );
        vlc_object_release( (vlc_object_t *)p_aout );

//        MenuMute->Checked = ! MenuMute->Checked;
    }
}
//---------------------------------------------------------------------------


/*****************************************************************************
 * External drop handling
 *****************************************************************************/
void __fastcall TMainFrameDlg::OnDrop( TMessage &Msg )
{
    /* find the number of files dropped */
    int num_files = DragQueryFile( (HDROP)Msg.WParam, 0xFFFFFFFF,
                                   (LPSTR)NULL, NULL );

    /* append each file to the playlist */
    for( int i = 0; i < num_files; i++ )
    {
        /* find the length of the filename */
        int name_length = DragQueryFile( (HDROP)Msg.WParam, i, NULL, NULL ) + 1;

        /* get the filename */
        char *FileName = new char[name_length];
        DragQueryFile( (HDROP)Msg.WParam, i, FileName, name_length );

        /* add the new file to the playlist */
        p_intf->p_sys->p_playwin->Add( FileName, PLAYLIST_APPEND | PLAYLIST_GO,
                                       PLAYLIST_END );

        delete[] FileName;
    }

    DragFinish( (HDROP)Msg.WParam );
    Msg.Result = 0;
}
//--------------------------------------------------------------------------


/*****************************************************************************
 * Menu and popup callbacks
 *****************************************************************************/
void __fastcall TMainFrameDlg::MenuHideinterfaceClick( TObject *Sender )
{
     this->SendToBack();
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PopupToggleInterfaceClick( TObject *Sender )
{
    this->BringToFront();
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PopupCloseClick( TObject *Sender )
{
    /* We do nothing, we just need a click on a menu item
     * to close the popup. Don't ask me why... */
    return;
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PopupJumpClick( TObject *Sender )
{
    /* TODO */
}
//---------------------------------------------------------------------------


/*****************************************************************************
 * Callbacks for DVD/VCD navigation
 ****************************************************************************/
void __fastcall TMainFrameDlg::PrevTitleActionExecute( TObject *Sender )
{
    input_area_t  * p_area;
    int             i_id;

    i_id = p_intf->p_sys->p_input->stream.p_selected_area->i_id - 1;

    /* Disallow area 0 since it is used for video_ts.vob */
    if( i_id > 0 )
    {
        p_area = p_intf->p_sys->p_input->stream.pp_areas[i_id];
        input_ChangeArea( p_intf->p_sys->p_input, (input_area_t*)p_area );

        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PLAY );

        p_intf->p_sys->b_title_update = 1;
        vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
        p_intf->p_sys->p_menus->SetupMenus();
        vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::NextTitleActionExecute( TObject *Sender )
{
    input_area_t  * p_area;
    unsigned int    i_id;

    i_id = p_intf->p_sys->p_input->stream.p_selected_area->i_id + 1;

    if( i_id < p_intf->p_sys->p_input->stream.i_area_nb )
    {
        p_area = p_intf->p_sys->p_input->stream.pp_areas[i_id];
        input_ChangeArea( p_intf->p_sys->p_input, (input_area_t*)p_area );

        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PLAY );

        p_intf->p_sys->b_title_update = 1;
        vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
        p_intf->p_sys->p_menus->SetupMenus();
        vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::PrevChapterActionExecute( TObject *Sender )
{
    input_area_t  * p_area;

    p_area = p_intf->p_sys->p_input->stream.p_selected_area;

    if( p_area->i_part > 0 )
    {
        p_area->i_part--;
        input_ChangeArea( p_intf->p_sys->p_input, (input_area_t*)p_area );

        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PLAY );

        p_intf->p_sys->b_chapter_update = 1;
        vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
        p_intf->p_sys->p_menus->SetupMenus();
        vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainFrameDlg::NextChapterActionExecute( TObject *Sender )
{
    input_area_t  * p_area;

    p_area = p_intf->p_sys->p_input->stream.p_selected_area;

    if( p_area->i_part < p_area->i_part_nb )
    {
        p_area->i_part++;
        input_ChangeArea( p_intf->p_sys->p_input, (input_area_t*)p_area );

        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PLAY );

        p_intf->p_sys->b_chapter_update = 1;
        vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
        p_intf->p_sys->p_menus->SetupMenus();
        vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
    }
}
//---------------------------------------------------------------------------


/*****************************************************************************
 * Callback for the 'go!' button
 ****************************************************************************/
void __fastcall TMainFrameDlg::ButtonGoClick( TObject *Sender )
{
    int i_channel;

    i_channel = SpinEditChannel->Value;
    msg_Dbg( p_intf, "joining channel %d", i_channel );

    vlc_mutex_lock( &p_intf->change_lock );
    network_ChannelJoin( p_intf, i_channel );
    vlc_mutex_unlock( &p_intf->change_lock );

//    input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PLAY );
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
    if( p_intf->p_sys->p_input != NULL )
    {
        switch( p_intf->p_sys->p_input->stream.i_method & 0xf0 )
        {
            case INPUT_METHOD_FILE:
                GroupBoxFile->Visible = true;
                ActiveGB = GroupBoxFile;
                LabelFileName->Caption = p_intf->p_sys->p_input->psz_source;
                break;
            case INPUT_METHOD_DISC:
                GroupBoxDisc->Visible = true;
                ActiveGB = GroupBoxDisc;
                break;
            case INPUT_METHOD_NETWORK:
                GroupBoxNetwork->Visible = true;
                ActiveGB = GroupBoxNetwork;
                LabelServer->Caption = p_intf->p_sys->p_input->psz_source;
                if( config_GetInt( p_intf, "network-channel" ) )
                {
                    LabelChannel->Visible = true;
                }
                else
                {
                    LabelChannel->Visible = false;
                }
                break;
            default:
                msg_Warn( p_intf, "cannot determine input method" );
                GroupBoxFile->Visible = true;
                ActiveGB = GroupBoxFile;
                LabelFileName->Caption = p_intf->p_sys->p_input->psz_source;
                break;
        }

        i_Height = StatusBar->Height + ActiveGB->Height + ToolBar->Height + 54;

        /* initialize and show slider for seekable streams */
        if( p_intf->p_sys->p_input->stream.b_seekable )
        {
            TrackBar->Position = p_intf->p_sys->OldValue = 0;
            GroupBoxSlider->Show();
            i_Height += GroupBoxSlider->Height;
        }

        /* control buttons for free pace streams */
        b_control = p_intf->p_sys->p_input->stream.b_pace_control;

        /* get ready for menu regeneration */
        p_intf->p_sys->b_program_update = 1;
        p_intf->p_sys->b_title_update = 1;
        p_intf->p_sys->b_chapter_update = 1;
        p_intf->p_sys->b_audio_update = 1;
        p_intf->p_sys->b_spu_update = 1;
        p_intf->p_sys->i_part = 0;

        p_intf->p_sys->p_input->stream.b_changed = 0;
        msg_Dbg( p_intf, "stream has changed, refreshing interface" );
    }
    else
    {
        i_Height = StatusBar->Height + ToolBar->Height + 47;

        if( config_GetInt( p_intf, "network-channel" ) )
        {
            GroupBoxNetwork->Visible = true;
            LabelChannel->Visible = true;
            i_Height += GroupBoxNetwork->Height + 7;
        }
        else
        {
            /* add space between tolbar and statusbar when
             * nothing is displayed; isn't it nicer ? :) */
            i_Height += 17;
        }

        /* unsensitize menus */
        MenuProgram->Enabled = false;
        MenuTitle->Enabled = false;
        MenuChapter->Enabled = false;
        MenuLanguage->Enabled = false;
        MenuSubtitles->Enabled = false;
        PopupNavigation->Enabled = false;
        PopupLanguage->Enabled = false;
        PopupSubtitles->Enabled = false;
    }

    /* resize main window */
    this->Height = i_Height;

    /* set control items */
    ToolButtonBack->Enabled = false;
    ToolButtonEject->Enabled = !b_control;
    StopAction->Enabled = true;
    PauseAction->Enabled = b_control;
    SlowAction->Enabled = b_control;
    FastAction->Enabled = b_control;
    PopupBack->Enabled = false;
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
        Preferences = new TPreferencesDlg( this, p_intf );
        Preferences->CreateConfigDialog( Name.c_str() );

        /* save it */
        i_pos = StringListPref->Add( Name );
        StringListPref->Objects[i_pos] = Preferences;
    }

    /* display the dialog */
    Preferences->Show();
}
//---------------------------------------------------------------------------

