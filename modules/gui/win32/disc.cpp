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

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "disc.h"
#include "misc.h";
#include "win32_common.h"

//---------------------------------------------------------------------------
//#pragma package(smart_init)
#pragma link "CSPIN"
#pragma resource "*.dfm"

extern  intf_thread_t *p_intfGlobal;

//---------------------------------------------------------------------------
__fastcall TDiscDlg::TDiscDlg( TComponent* Owner )
        : TForm( Owner )
{
    /* Simulate a click to get the correct device name */
    RadioGroupTypeClick( RadioGroupType );
    Translate( this );
}
//---------------------------------------------------------------------------
void __fastcall TDiscDlg::FormShow( TObject *Sender )
{
    p_intfGlobal->p_sys->p_window->OpenDiscAction->Checked = true;
}
//---------------------------------------------------------------------------
void __fastcall TDiscDlg::FormHide( TObject *Sender )
{
    p_intfGlobal->p_sys->p_window->OpenDiscAction->Checked = false;
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
    playlist_t *    p_playlist;

    p_playlist = (playlist_t *)
        vlc_object_find( p_intfGlobal, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL )
    {   
        return;
    }                        

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
    Title.sprintf( "%d", SpinEditTitle->Value );
    Chapter.sprintf( "%d", SpinEditChapter->Value );

    /* Build source name and add it to playlist */
    Source = Method + ":" + Device + "@" + Title + "," + Chapter;
    playlist_Add( p_playlist, Source.c_str(),
                  PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );

    /* update the display */
    p_intfGlobal->p_sys->p_playwin->UpdateGrid( p_playlist );

    vlc_object_release( p_playlist );
}
//---------------------------------------------------------------------------
void __fastcall TDiscDlg::RadioGroupTypeClick( TObject *Sender )
{
    TRadioGroup *RadioGroupType = (TRadioGroup *)Sender;
    char *psz_device;

    if( RadioGroupType->ItemIndex == 0 )
    {
        psz_device = config_GetPsz( p_intfGlobal, "dvd" );
    }
    else
    {
        psz_device = config_GetPsz( p_intfGlobal, "vcd" );
    }

    if( psz_device )
    {
        EditDevice->Text = psz_device;
        free( psz_device );
    }
}
//---------------------------------------------------------------------------

