/*****************************************************************************
 * about.cpp: The "About" dialog box
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

#include "about.h"
#include "win32_common.h"

//---------------------------------------------------------------------------
//#pragma package(smart_init)
#pragma resource "*.dfm"

extern  struct intf_thread_s *p_intfGlobal;

//---------------------------------------------------------------------------
__fastcall TAboutDlg::TAboutDlg( TComponent* Owner )
        : TForm( Owner )
{
    Image1->Picture->Icon = p_intfGlobal->p_sys->p_window->Icon;
}
//---------------------------------------------------------------------------


