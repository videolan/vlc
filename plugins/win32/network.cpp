/*****************************************************************************
 * network.cpp: the "network" dialog box
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 *
 * Authors: Olivier Teuliere <ipkiss@via.ecp.fr>
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

#include "interface.h"
#include "intf_playlist.h"

#include "network.h"
#include "win32_common.h"

#include "netutils.h"

//---------------------------------------------------------------------------
//#pragma package(smart_init)
#pragma resource "*.dfm"

extern struct intf_thread_s *p_intfGlobal;

//---------------------------------------------------------------------------
__fastcall TNetworkDlg::TNetworkDlg( TComponent* Owner )
        : TForm( Owner )
{
        char *psz_channel_server;

        /* server port */
        UpDownPort->Position = config_GetIntVariable( "server-port" );

        /* channel server */
        if( config_GetIntVariable( "network-channel" ) )
        {
            CheckBoxChannel->Checked = true;
        }

        psz_channel_server = config_GetPszVariable( "channel-server" );
        if( psz_channel_server )
        {
            ComboBoxChannel->Text = psz_channel_server;
            free( psz_channel_server );
        }

        UpDownPortCS->Position = config_GetIntVariable( "channel-port" );
}
//---------------------------------------------------------------------------
void __fastcall TNetworkDlg::FormShow( TObject *Sender )
{
    p_intfGlobal->p_sys->p_window->MenuNetworkStream->Checked = true;
    p_intfGlobal->p_sys->p_window->PopupNetworkStream->Checked = true;
}
//---------------------------------------------------------------------------
void __fastcall TNetworkDlg::FormHide( TObject *Sender )
{
    p_intfGlobal->p_sys->p_window->MenuNetworkStream->Checked = false;
    p_intfGlobal->p_sys->p_window->PopupNetworkStream->Checked = false;
}
//---------------------------------------------------------------------------
void __fastcall TNetworkDlg::BitBtnCancelClick( TObject *Sender )
{
    Hide();
}
//---------------------------------------------------------------------------
void __fastcall TNetworkDlg::CheckBoxBroadcastClick( TObject *Sender )
{
    ComboBoxBroadcast->Enabled = NOT( ComboBoxBroadcast->Enabled );
}
//---------------------------------------------------------------------------
void __fastcall TNetworkDlg::CheckBoxChannelClick( TObject *Sender )
{
    LabelAddress->Enabled = NOT( LabelAddress->Enabled );
    ComboBoxAddress->Enabled = NOT( ComboBoxAddress->Enabled );
    LabelPort->Enabled = NOT( LabelPort->Enabled );
    EditPort->Enabled = NOT( EditPort->Enabled );
    UpDownPort->Enabled = NOT( UpDownPort->Enabled );
    CheckBoxBroadcast->Enabled = NOT( CheckBoxBroadcast->Enabled );
    ComboBoxBroadcast->Enabled = ( NOT( ComboBoxBroadcast->Enabled ) &&
                                   CheckBoxBroadcast->Checked );
    ComboBoxChannel->Enabled = NOT( ComboBoxChannel->Enabled );
    LabelPortCS->Enabled = NOT( LabelPortCS->Enabled );
    EditPortCS->Enabled = NOT( EditPortCS->Enabled );
    UpDownPortCS->Enabled = NOT( UpDownPortCS->Enabled );
}
//---------------------------------------------------------------------------
void __fastcall TNetworkDlg::BitBtnOkClick( TObject *Sender )
{
    AnsiString      Source, Protocol, Server;
    boolean_t       b_channel;
    boolean_t       b_broadcast;
    unsigned int    i_port;
    int             i_end = p_main->p_playlist->i_size;

    Hide();
    Server = ComboBoxAddress->Text;

    /* select added item */
    if( p_input_bank->pp_input[0] != NULL )
    {
        p_input_bank->pp_input[0]->b_eof = 1;
    }

    /* Check which protocol was activated */
    switch( RadioGroupProtocol->ItemIndex )
    {
        case 0:
            Protocol = "udp";
            break;
        case 1:
            intf_ErrMsg( "intf error: rtp protocol not yet implemented" );
            return;
        case 2:
            Protocol = "http";
            break;
    }

    /* Manage channel server */
    b_channel = CheckBoxChannel->Checked ? TRUE : FALSE;
    config_PutIntVariable( "network-channel", b_channel );
    if( b_channel )
    {
        AnsiString      Channel = ComboBoxChannel->Text;
        unsigned int    i_channel_port = UpDownPortCS->Position;

        if( p_main->p_channel == NULL )
        {
            network_ChannelCreate();
        }

        config_PutPszVariable( "channel-server", Channel.c_str() );
        if( i_channel_port < 65536 )
        {
            config_PutIntVariable( "channel-port", i_channel_port );
        }

        p_intfGlobal->p_sys->b_playing = 1;
    }
    else
    {
        /* Get the port number and make sure it will not
         * overflow 5 characters */
        i_port = UpDownPort->Position;
        if( i_port > 65535 )
        {
            intf_ErrMsg( "intf error: invalid port %i", i_port );
        }

        /* do we have a broadcast address */
        b_broadcast = CheckBoxBroadcast->Checked ? TRUE : FALSE;
        if( b_broadcast )
        {
            AnsiString Broadcast = ComboBoxBroadcast->Text;

            /* Build source name */
            Source = Protocol + "://" + Server + "@:" + IntToStr( i_port )
                     + "/" + Broadcast;
        }
        else
        {
            /* Build source name */
            Source = Protocol + "://" + Server + "@:" + IntToStr( i_port );
        }

        intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, Source.c_str() );
        
        /* update the display */
        p_intfGlobal->p_sys->p_playlist->UpdateGrid( p_main->p_playlist );

        intf_PlaylistJumpto( p_main->p_playlist, i_end - 1 );
    }
}
//---------------------------------------------------------------------------

