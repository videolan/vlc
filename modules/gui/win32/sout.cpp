/*****************************************************************************
 * sout.cpp: the stream ouput dialog box
 *****************************************************************************
 * Copyright (C) 2002-2003 VideoLAN
 * $Id: sout.cpp,v 1.2 2003/01/22 21:42:51 ipkiss Exp $
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

#include "sout.h"
#include "misc.h"
#include "win32_common.h"

//---------------------------------------------------------------------------
#pragma link "CSPIN"
#pragma resource "*.dfm"
//---------------------------------------------------------------------------
__fastcall TSoutDlg::TSoutDlg( TComponent* Owner, intf_thread_t *_p_intf )
    : TForm( Owner )
{
    p_intf = _p_intf;

    PanelAccess->BevelOuter = bvNone;
    PanelMux->BevelOuter = bvNone;

    Translate( this );
}
//---------------------------------------------------------------------------
void __fastcall TSoutDlg::ButtonBrowseClick( TObject *Sender )
{
    if( OpenDialog1->Execute() )
    {
        EditFile->Text = OpenDialog1->FileName;
        RebuildMrl();
    };
}
//---------------------------------------------------------------------------
void __fastcall TSoutDlg::CustomEditChange( TObject *Sender )
{
    RebuildMrl();
}
//---------------------------------------------------------------------------
void __fastcall TSoutDlg::RadioButtonMuxClick( TObject *Sender )
{
    RebuildMrl();
}
//---------------------------------------------------------------------------
void __fastcall TSoutDlg::RadioButtonAccessClick( TObject *Sender )
{
    bool b_file = RadioButtonFile->Checked;
    bool b_udp  = RadioButtonUDP->Checked;
    bool b_rtp  = RadioButtonRTP->Checked;

    EditFile->Enabled = b_file;
    ButtonBrowse->Enabled = b_file;
    LabelAddress->Enabled = b_udp | b_rtp;
    EditAddress->Enabled = b_udp | b_rtp;
    LabelPort->Enabled = b_udp | b_rtp;
    SpinEditPort->Enabled = b_udp | b_rtp;
    RadioButtonPS->Enabled = !b_rtp;

    if( b_rtp )
        RadioButtonTS->Checked = true;

    RebuildMrl();
}
//---------------------------------------------------------------------------
void __fastcall TSoutDlg::BitBtnOKClick( TObject *Sender )
{
    config_PutPsz( p_intf, "sout", EditMrl->Text.c_str() );
}
//---------------------------------------------------------------------------


/*****************************************************************************
 * Private functions
 *****************************************************************************/
void _fastcall TSoutDlg::RebuildMrl()
{
    AnsiString Mux, Mrl;

    if( RadioButtonPS->Checked )
        Mux = "ps";
    else
        Mux = "ts";

    if( RadioButtonFile->Checked )
        Mrl = "file/" + Mux + "://" + EditFile->Text;
    else if( RadioButtonUDP->Checked )
        Mrl = "udp/" + Mux + "://" + EditAddress->Text + ":"
              + SpinEditPort->Value;
    else
        Mrl = "rtp/" + Mux + "://" + EditAddress->Text + ":"
              + SpinEditPort->Value;

    EditMrl->Text = Mrl;
}
//---------------------------------------------------------------------------

