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
#include "misc.h"
#include "win32_common.h"

//---------------------------------------------------------------------------
//#pragma package(smart_init)
#pragma link "CSPIN"
#pragma resource "*.dfm"

//---------------------------------------------------------------------------
__fastcall TDiscDlg::TDiscDlg( TComponent* Owner, intf_thread_t *_p_intf )
        : TForm( Owner )
{
    p_intf = _p_intf;
    /* Simulate a click to get the correct device name */
    RadioGroupTypeClick( RadioGroupType );
    Translate( this );
}
//---------------------------------------------------------------------------
void __fastcall TDiscDlg::FormShow( TObject *Sender )
{
    p_intf->p_sys->p_window->OpenDiscAction->Checked = true;
}
//---------------------------------------------------------------------------
void __fastcall TDiscDlg::FormHide( TObject *Sender )
{
    p_intf->p_sys->p_window->OpenDiscAction->Checked = false;
}
//---------------------------------------------------------------------------
void __fastcall TDiscDlg::ButtonCancelClick( TObject *Sender )
{
    Hide();
}
//---------------------------------------------------------------------------
void __fastcall TDiscDlg::ButtonOkClick( TObject *Sender )
{
    AnsiString  Device, Source;

    Hide();

    Device = EditDevice->Text;

    /* Build source name and add it to playlist */
    if ( CheckBoxMenus->Checked && RadioGroupType->ItemIndex == 0 )
    {
        Source = "dvdplay://" + Device;
    }
    else
    {
        AnsiString Method, Title, Chapter;
        /* Select title and chapter */
        Title.sprintf( "%d", SpinEditTitle->Value );
        Chapter.sprintf( "%d", SpinEditChapter->Value );

        /* Select access method */
        if( RadioGroupType->ItemIndex == 0 )
            Method = "dvdold";
        else
            Method = "vcd";

        /* build the MRL */
        Source = Method + "://" + Device + "@" + Title + "," + Chapter;
    }
    msg_Dbg (p_intf , Source.c_str());

    p_intf->p_sys->p_playwin->Add( Source, PLAYLIST_APPEND
            | ( p_intf->p_sys->b_play_when_adding ? PLAYLIST_GO : 0 ),
            PLAYLIST_END );
}
//---------------------------------------------------------------------------
void __fastcall TDiscDlg::RadioGroupTypeClick( TObject *Sender )
{
    TRadioGroup *RadioGroupType = (TRadioGroup *)Sender;
    char *psz_device;

    if( RadioGroupType->ItemIndex == 0 )
    {
        psz_device = config_GetPsz( p_intf, "dvd" );
        CheckBoxMenus->Enabled = true;
        DisableTitles( CheckBoxMenus->Checked );
    }
    else
    {
        psz_device = config_GetPsz( p_intf, "vcd" );
        CheckBoxMenus->Enabled = false;
        /* We don't support menus for vcds, so we use titles and chapters */
        DisableTitles( false );
    }

    if( psz_device )
    {
        EditDevice->Text = psz_device;
        free( psz_device );
    }
}
//---------------------------------------------------------------------------
void __fastcall TDiscDlg::CheckBoxMenusClick( TObject *Sender )
{
    DisableTitles( CheckBoxMenus->Checked );
}
//---------------------------------------------------------------------------
void __fastcall TDiscDlg::DisableTitles( Boolean disable )
{
    if( disable )
    {
        LabelTitle->Enabled = false;
        LabelChapter->Enabled = false;
        SpinEditTitle->Enabled = false;
        SpinEditTitle->Color = clBtnFace;
        SpinEditChapter->Enabled = false;
        SpinEditChapter->Color = clBtnFace;
    }
    else
    {
        LabelTitle->Enabled = true;
        LabelChapter->Enabled = true;
        SpinEditTitle->Enabled = true;
        SpinEditTitle->Color = clWindow;
        SpinEditChapter->Enabled = true;
        SpinEditChapter->Color = clWindow;
    }
}
//---------------------------------------------------------------------------

