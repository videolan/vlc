/*****************************************************************************
 * disc.cpp: "Open disc" dialog box.
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

#include "disc.h"
#include "win32_common.h"

//---------------------------------------------------------------------------
//#pragma package(smart_init)
#pragma resource "*.dfm"

extern  struct intf_thread_s *p_intfGlobal;

//---------------------------------------------------------------------------
__fastcall TDiscDlg::TDiscDlg( TComponent* Owner )
        : TForm( Owner )
{
    RadioGroupTypeClick( RadioGroupType );
}
//---------------------------------------------------------------------------
void __fastcall TDiscDlg::FormShow( TObject *Sender )
{
    p_intfGlobal->p_sys->p_window->MenuOpenDisc->Checked = true;
    p_intfGlobal->p_sys->p_window->PopupOpenDisc->Checked = true;
}
//---------------------------------------------------------------------------
void __fastcall TDiscDlg::FormHide( TObject *Sender )
{
    p_intfGlobal->p_sys->p_window->MenuOpenDisc->Checked = false;
    p_intfGlobal->p_sys->p_window->PopupOpenDisc->Checked = false;
}
//---------------------------------------------------------------------------
void __fastcall TDiscDlg::BitBtnCancelClick( TObject *Sender )
{
    Hide();
}
//---------------------------------------------------------------------------
void __fastcall TDiscDlg::BitBtnOkClick( TObject *Sender )
{
    AnsiString  Device, Source, Method, Title, Chapter;
    int         i_end = p_main->p_playlist->i_size;

    Hide();

    Device = EditDevice->Text;

    /* Check which method was activated */
    if( RadioGroupType->ItemIndex == 0 )
    {
        Method = "dvd";
    }
    else
    {
        Method = "vcd";
    }

    /* Select title and chapter */
    Title.sprintf( "%d", UpDownTitle->Position );
    Chapter.sprintf( "%d", UpDownChapter->Position );

    /* Build source name and add it to playlist */
    Source = Method + ":" + Device + "@" + Title + "," + Chapter;
    intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, Source.c_str() );

    /* update the display */
    p_intfGlobal->p_sys->p_playlist->UpdateGrid( p_main->p_playlist );

    /* stop current item, select added item */
    if( p_input_bank->pp_input[0] != NULL )
    {
        p_input_bank->pp_input[0]->b_eof = 1;
    }

    intf_PlaylistJumpto( p_main->p_playlist, i_end - 1 );
}
//---------------------------------------------------------------------------
void __fastcall TDiscDlg::RadioGroupTypeClick( TObject *Sender )
{
    TRadioGroup *RadioGroupType = (TRadioGroup *)Sender;
    char *psz_device;

    if( RadioGroupType->ItemIndex == 0 )
    {
        psz_device = config_GetPszVariable( "dvd" );
    }
    else
    {
        psz_device = config_GetPszVariable( "vcd" );
    }

    if( psz_device )
    {
        EditDevice->Text = psz_device;
        free( psz_device );
    }
}
//---------------------------------------------------------------------------

