/*****************************************************************************
 * messages.cpp: log window.
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

#include "interface.h"

#include "win32_common.h"
#include "messages.h"

//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"

extern  struct intf_thread_s *p_intfGlobal;

//---------------------------------------------------------------------------
__fastcall TMessagesDlg::TMessagesDlg( TComponent* Owner )
    : TForm( Owner )
{
    Icon = p_intfGlobal->p_sys->p_window->Icon;
}
//---------------------------------------------------------------------------
void __fastcall TMessagesDlg::ButtonOKClick( TObject *Sender )
{
    Hide();
}
//---------------------------------------------------------------------------
void __fastcall TMessagesDlg::FormHide( TObject *Sender )
{
    p_intfGlobal->p_sys->p_window->MenuMessages->Checked = false;
}
//---------------------------------------------------------------------------
void __fastcall TMessagesDlg::FormShow( TObject *Sender )
{
    p_intfGlobal->p_sys->p_window->MenuMessages->Checked = true;
}
//---------------------------------------------------------------------------
void __fastcall TMessagesDlg::UpdateLog()
{
    intf_subscription_t *p_sub = p_intfGlobal->p_sys->p_sub;
    int                 i_start, i_stop, i_del, i_count;
    int                 i_max_lines;

    vlc_mutex_lock( p_sub->p_lock );
    i_stop = *p_sub->pi_stop;
    vlc_mutex_unlock( p_sub->p_lock );

    if( p_sub->i_start != i_stop )
    {
        for( i_start = p_sub->i_start;
             i_start != i_stop;
             i_start = (i_start+1) % INTF_MSG_QSIZE )
        {
            /* Append all messages to log window */
            switch( p_sub->p_msg[i_start].i_type )
            {
            case INTF_MSG_ERR:
                RichEditMessages->SelAttributes->Color = clRed;
                break;
            case INTF_MSG_WARN:
                RichEditMessages->SelAttributes->Color = clBlack;
                break;
            default:
                RichEditMessages->SelAttributes->Color = clBlue;
                break;
            }

            /* Limit log size */
            i_count = RichEditMessages->Lines->Count;
            i_max_lines = config_GetIntVariable( "intfwin-max-lines" );
            if( i_max_lines > 0 )
            {
                for( i_del = 0; i_del <= i_count - i_max_lines; i_del++ )
                {
                    RichEditMessages->Lines->Delete( 0 );
                }
            }

            /* Add message */
            if( i_max_lines )
            {
                RichEditMessages->Lines->Add( p_sub->p_msg[i_start].psz_msg );
            }
        }

        vlc_mutex_lock( p_sub->p_lock );
        p_sub->i_start = i_start;
        vlc_mutex_unlock( p_sub->p_lock );
    } 
}
//---------------------------------------------------------------------------
