/*****************************************************************************
 * subtitles.h: Dialog box for divx subtitle selection
 *****************************************************************************
 * Copyright (C) 2002-2003 VideoLAN
 * $Id: subtitles.h,v 1.1 2003/01/22 21:42:51 ipkiss Exp $
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

#ifndef subtitlesH
#define subtitlesH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Dialogs.hpp>
#include <Buttons.hpp>
//---------------------------------------------------------------------------
class TSubtitlesDlg : public TForm
{
__published:	// IDE-managed Components
    TOpenDialog *OpenDialog1;
    TGroupBox *GroupBoxSubtitles;
    TEdit *EditDelay;
    TEdit *EditFPS;
    TEdit *EditFile;
    TButton *ButtonBrowse;
    TBitBtn *BitBtnOK;
    TBitBtn *BitBtnCancel;
    TLabel *LabelDelay;
    TLabel *LabelFPS;
    void __fastcall ButtonBrowseClick( TObject *Sender );
    void __fastcall BitBtnOKClick( TObject *Sender );
private:	// User declarations
    intf_thread_t *p_intf;
public:		// User declarations
    __fastcall TSubtitlesDlg( TComponent* Owner, intf_thread_t *p_intf );
};
//---------------------------------------------------------------------------
#endif
