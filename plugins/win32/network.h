/*****************************************************************************
 * network.h: the "network" dialog box
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

#ifndef networkH
#define networkH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Buttons.hpp>
#include <ComCtrls.hpp>
#include <ExtCtrls.hpp>
//---------------------------------------------------------------------------

#define NOT( var ) ( (var) ? false : true )

class TNetworkDlg : public TForm
{
__published:	// IDE-managed Components
    TRadioGroup *RadioGroupProtocol;
    TGroupBox *GroupBoxServer;
    TLabel *LabelAddress;
    TLabel *LabelPort;
    TCheckBox *CheckBoxBroadcast;
    TEdit *EditPort;
    TComboBox *ComboBoxAddress;
    TComboBox *ComboBoxBroadcast;
    TUpDown *UpDownPort;
    TGroupBox *GroupBoxChannels;
    TLabel *LabelPortCS;
    TCheckBox *CheckBoxChannel;
    TComboBox *ComboBoxChannel;
    TEdit *EditPortCS;
    TUpDown *UpDownPortCS;
    TBitBtn *BitBtnOk;
    TBitBtn *BitBtnCancel;
    void __fastcall FormShow( TObject *Sender );
    void __fastcall FormHide( TObject *Sender );
    void __fastcall BitBtnCancelClick(TObject *Sender);
    void __fastcall CheckBoxBroadcastClick( TObject *Sender );
    void __fastcall CheckBoxChannelClick( TObject *Sender );
    void __fastcall BitBtnOkClick(TObject *Sender);
private:	// User declarations
public:		// User declarations
    __fastcall TNetworkDlg( TComponent* Owner );
};
//---------------------------------------------------------------------------
#endif
