/*****************************************************************************
 * subtitles.cpp: Dialog box for divx subtitle selection
 *****************************************************************************
 * Copyright (C) 2002-2003 VideoLAN
 * $Id: subtitles.cpp,v 1.3 2003/02/12 02:11:58 ipkiss Exp $
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

#include "subtitles.h"
#include "misc.h"
#include "win32_common.h"

//---------------------------------------------------------------------------
#pragma resource "*.dfm"
//---------------------------------------------------------------------------
__fastcall TSubtitlesDlg::TSubtitlesDlg( TComponent* Owner,
    intf_thread_t *_p_intf ) : TForm( Owner )
{
    p_intf = _p_intf;

    Constraints->MinWidth = Width;
    Constraints->MinHeight = Height;

    Translate( this );
}
//---------------------------------------------------------------------------
void __fastcall TSubtitlesDlg::ButtonBrowseClick( TObject *Sender )
{
    if( OpenDialog1->Execute() )
    {
        EditFile->Text = OpenDialog1->FileName;
    }
}
//---------------------------------------------------------------------------
void __fastcall TSubtitlesDlg::ButtonOKClick( TObject *Sender )
{
    int delay = (int) (10 * atof( EditDelay->Text.c_str() ));
    float fps = atof( EditFPS->Text.c_str() );
    config_PutPsz( p_intf, "sub-file", EditFile->Text.c_str() );
    config_PutInt( p_intf, "sub-delay", delay );
    config_PutFloat( p_intf, "sub-fps", fps );
}
//---------------------------------------------------------------------------

