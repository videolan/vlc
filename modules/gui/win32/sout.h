/*****************************************************************************
 * sout.h: the stream ouput dialog box
 *****************************************************************************
 * Copyright (C) 2002-2003 VideoLAN
 * $Id: sout.h,v 1.1 2003/01/21 19:49:09 ipkiss Exp $
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

#ifndef soutH
#define soutH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ExtCtrls.hpp>
#include "CSPIN.h"
#include <Dialogs.hpp>
#include <Buttons.hpp>
//---------------------------------------------------------------------------
class TSoutDlg : public TForm
{
__published:	// IDE-managed Components
    TGroupBox *GroupBoxStreamOut;
    TEdit *EditMrl;
    TPanel *PanelAccess;
    TRadioButton *RadioButtonFile;
    TRadioButton *RadioButtonUDP;
    TRadioButton *RadioButtonRTP;
    TOpenDialog *OpenDialog1;
    TButton *ButtonBrowse;
    TEdit *EditFile;
    TCSpinEdit *SpinEditPort;
    TEdit *EditAddress;
    TLabel *LabelPort;
    TLabel *LabelAddress;
    TBitBtn *BitBtnOK;
    TBitBtn *BitBtnCancel;
    TPanel *PanelMux;
    TRadioButton *RadioButtonPS;
    TRadioButton *RadioButtonTS;
    void __fastcall ButtonBrowseClick( TObject *Sender );
    void __fastcall CustomEditChange( TObject *Sender );
    void __fastcall RadioButtonMuxClick( TObject *Sender );
    void __fastcall RadioButtonAccessClick( TObject *Sender );
    void __fastcall BitBtnOKClick( TObject *Sender );
private:	// User declarations
    void __fastcall RebuildMrl();
    intf_thread_t *p_intf;
public:		// User declarations
    __fastcall TSoutDlg( TComponent* Owner, intf_thread_t *_p_intf );
};
//---------------------------------------------------------------------------
#endif
