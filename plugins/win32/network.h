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
    TBitBtn *BitBtnOk;
    TBitBtn *BitBtnCancel;
    TGroupBox *GroupBoxMode;
    TRadioButton *RadioButtonUDP;
    TRadioButton *RadioButtonMulticast;
    TRadioButton *RadioButtonCS;
    TRadioButton *RadioButtonHTTP;
    TLabel *LabelUDPPort;
    TEdit *EditUDPPort;
    TUpDown *UpDownUDPPort;
    TLabel *LabelMulticastPort;
    TEdit *EditMulticastPort;
    TUpDown *UpDownMulticastPort;
    TLabel *LabelCSPort;
    TEdit *EditCSPort;
    TUpDown *UpDownCSPort;
    TLabel *LabelMulticastAddress;
    TComboBox *ComboBoxMulticastAddress;
    TLabel *LabelCSAddress;
    TComboBox *ComboBoxCSAddress;
    TEdit *EditHTTPURL;
    TLabel *LabelHTTPURL;
    void __fastcall FormShow( TObject *Sender );
    void __fastcall FormHide( TObject *Sender );
    void __fastcall BitBtnCancelClick( TObject *Sender );
    void __fastcall BitBtnOkClick( TObject *Sender );
    void __fastcall RadioButtonUDPClick( TObject *Sender );
    void __fastcall RadioButtonMulticastClick( TObject *Sender );
    void __fastcall RadioButtonCSClick( TObject *Sender );
    void __fastcall RadioButtonHTTPClick( TObject *Sender );
private:	// User declarations
    int OldRadioValue;
    void __fastcall ChangeEnabled( int i_selected );
public:		// User declarations
    __fastcall TNetworkDlg( TComponent* Owner );
};
//---------------------------------------------------------------------------
#endif
