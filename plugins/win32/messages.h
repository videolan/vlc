/*****************************************************************************
 * messages.h: log window
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

#ifndef messagesH
#define messagesH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ComCtrls.hpp>
#include <Menus.hpp>
//---------------------------------------------------------------------------
class TMessagesDlg : public TForm
{
__published:	// IDE-managed Components
    TRichEdit *RichEditMessages;
    TButton *ButtonOK;
    void __fastcall ButtonOKClick( TObject *Sender );
    void __fastcall FormHide( TObject *Sender );
    void __fastcall FormShow( TObject *Sender );
private:	// User declarations
public:		// User declarations
    __fastcall TMessagesDlg( TComponent* Owner );
    void __fastcall UpdateLog();
};
//---------------------------------------------------------------------------
#endif
