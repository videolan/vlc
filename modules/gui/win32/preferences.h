/*****************************************************************************
 * preferences.h: the "Preferences" dialog box
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

#ifndef preferencesH
#define preferencesH
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
class TGroupBoxPref : public TGroupBox
{
public:
    __fastcall TGroupBoxPref( TComponent* Owner, module_config_t *p_config_arg );
    module_config_t *p_config;
    virtual void __fastcall UpdateChanges();
    TListView * __fastcall CreateListView( TWinControl *Parent,
            int Left, int Width, int Top, int Height, TViewStyle ViewStyle );
    TButton * __fastcall CreateButton( TWinControl *Parent,
            int Left, int Width, int Top, int Height, AnsiString Caption );
    TCheckBox * __fastcall CreateCheckBox( TWinControl *Parent,
            int Left, int Width, int Top, int Height, AnsiString Caption );
    TLabel * __fastcall CreateLabel( TWinControl *Parent,
            int Left, int Width, int Top, int Height, AnsiString Caption,
            bool WordWrap );
    TEdit * __fastcall CreateEdit( TWinControl *Parent,
            int Left, int Width, int Top, int Height, AnsiString Text );
    TCSpinEdit * __fastcall CreateSpinEdit( TWinControl *Parent,
            int Left, int Width, int Top, int Height,
            long Min, long Max, long Value );
};
//---------------------------------------------------------------------------
class TGroupBoxPlugin : public TGroupBoxPref
{
public:
    __fastcall TGroupBoxPlugin( TComponent* Owner, module_config_t *p_config );
    TListView *ListView;
    TButton *ButtonConfig;
    TButton *ButtonSelect;
    TLabel *LabelDesc;
    TLabel *LabelHint;
    TLabel *LabelSelected;
    TEdit *Edit;
    module_t *ModuleSelected;
    void __fastcall UpdateChanges();
    void __fastcall ListViewSelectItem( TObject *Sender, TListItem *Item,
                                        bool Selected );
    void __fastcall ButtonSelectClick( TObject *Sender );
    void __fastcall ButtonConfigClick( TObject *Sender );
};
//---------------------------------------------------------------------------
class TGroupBoxString : public TGroupBoxPref
{
public:
    __fastcall TGroupBoxString( TComponent* Owner, module_config_t *p_config );
    TLabel *LabelDesc;
    TEdit *Edit;
    void __fastcall UpdateChanges();
};
//---------------------------------------------------------------------------
class TGroupBoxInteger : public TGroupBoxPref
{
public:
    __fastcall TGroupBoxInteger( TComponent* Owner, module_config_t *p_config );
    TLabel *LabelDesc;
    TCSpinEdit *SpinEdit;
    void __fastcall UpdateChanges();
};
//---------------------------------------------------------------------------
class TGroupBoxBool : public TGroupBoxPref
{
public:
    __fastcall TGroupBoxBool( TComponent* Owner, module_config_t *p_config );
    TLabel *LabelDesc;
    TCheckBox *CheckBox;
    void __fastcall UpdateChanges();
};
//---------------------------------------------------------------------------
class TPreferencesDlg : public TForm
{
__published:	// IDE-managed Components
    TPageControl *PageControlPref;
    TButton *ButtonApply;
    TButton *ButtonSave;
    TButton *ButtonOK;
    TButton *ButtonCancel;
    void __fastcall FormShow( TObject *Sender );
    void __fastcall FormHide( TObject *Sender );
    void __fastcall ButtonOkClick( TObject *Sender );
    void __fastcall ButtonApplyClick( TObject *Sender );
    void __fastcall ButtonSaveClick( TObject *Sender );
    void __fastcall ButtonCancelClick( TObject *Sender );
    void __fastcall FormClose( TObject *Sender, TCloseAction &Action );
private:	// User declarations
public:		// User declarations
    __fastcall TPreferencesDlg( TComponent* Owner );
    void __fastcall CreateConfigDialog( char *psz_module_name );
    void __fastcall SaveValue( module_config_t *p_config );
};
//---------------------------------------------------------------------------
#endif
