/*****************************************************************************
 * disc.h: "Open disc" dialog box.
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

#ifndef discH
#define discH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Buttons.hpp>
#include <ComCtrls.hpp>
#include <ExtCtrls.hpp>
#include "CSPIN.h"
//---------------------------------------------------------------------------
class TDiscDlg : public TForm
{
__published:	// IDE-managed Components
    TLabel *LabelDevice;
    TGroupBox *GroupBoxPosition;
    TLabel *LabelTitle;
    TLabel *LabelChapter;
    TRadioGroup *RadioGroupType;
    TBitBtn *BitBtnOk;
    TBitBtn *BitBtnCancel;
    TEdit *EditDevice;
    TCSpinEdit *SpinEditTitle;
    TCSpinEdit *SpinEditChapter;
    void __fastcall FormShow( TObject *Sender );
    void __fastcall FormHide( TObject *Sender );
    void __fastcall BitBtnCancelClick( TObject *Sender );
    void __fastcall BitBtnOkClick( TObject *Sender);
    void __fastcall RadioGroupTypeClick( TObject *Sender );
private:	// User declarations
    intf_thread_t *p_intf;
public:		// User declarations
    __fastcall TDiscDlg( TComponent* Owner, intf_thread_t *_p_intf );
};
//---------------------------------------------------------------------------
#endif
