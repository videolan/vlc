/*****************************************************************************
 * preferences.h: the "Preferences" dialog box
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 *
 * Authors: Olivier Teuliere <ipkiss@via.ecp.fr>
 *          Boris Dores <babal@via.ecp.fr>
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
#include <CheckLst.hpp>
#include <ExtCtrls.hpp>
#include "CSPIN.h"
//---------------------------------------------------------------------------
/* A TCheckListBox that automatically disposes any TObject
   associated with the string items */
class TCleanCheckListBox : public TCheckListBox
{
public:
    __fastcall TCleanCheckListBox(Classes::TComponent* AOwner)
        : TCheckListBox( AOwner ) { };
    virtual __fastcall ~TCleanCheckListBox();
};
//---------------------------------------------------------------------------
/* A THintWindow with a limited width */
class TNarrowHintWindow : public THintWindow
{
public:
   virtual void __fastcall ActivateHint(const Windows::TRect &Rect,
       const System::AnsiString AHint);
};
//---------------------------------------------------------------------------
/* Just a wrapper to embed an AnsiString into a TObject */
class TObjectString : public TObject
{
private:
    AnsiString FString;
public:
    __fastcall TObjectString(char * String);
    AnsiString __fastcall String();
};
//---------------------------------------------------------------------------
class TPanelPref : public TPanel
{
public:
    __fastcall TPanelPref( TComponent* Owner, module_config_t *p_config_arg );
    module_config_t *p_config;
    virtual void __fastcall UpdateChanges();
    TCleanCheckListBox * __fastcall CreateCleanCheckListBox( TWinControl *Parent,
            int Left, int Width, int Top, int Height );
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
class TPanelPlugin : public TPanelPref
{
public:
    __fastcall TPanelPlugin( TComponent* Owner, module_config_t *p_config,
        intf_thread_t *_p_intf );
    TCleanCheckListBox *CleanCheckListBox;
    TButton *ButtonConfig;
    TLabel *Label;
    module_t *ModuleSelected;
    void __fastcall UpdateChanges();
    void __fastcall CheckListBoxClick( TObject *Sender );
    void __fastcall CheckListBoxClickCheck( TObject *Sender );
    void __fastcall ButtonConfigClick( TObject *Sender );
private:
    intf_thread_t *p_intf;
};
//---------------------------------------------------------------------------
class TPanelString : public TPanelPref
{
public:
    __fastcall TPanelString( TComponent* Owner, module_config_t *p_config );
    TLabel *Label;
    TEdit *Edit;
    void __fastcall UpdateChanges();
};
//---------------------------------------------------------------------------
class TPanelInteger : public TPanelPref
{
public:
    __fastcall TPanelInteger( TComponent* Owner, module_config_t *p_config );
    TLabel *Label;
    TCSpinEdit *SpinEdit;
    void __fastcall UpdateChanges();
};
//---------------------------------------------------------------------------
class TPanelBool : public TPanelPref
{
public:
    __fastcall TPanelBool( TComponent* Owner, module_config_t *p_config );
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
    void __fastcall ButtonOkClick( TObject *Sender );
    void __fastcall ButtonApplyClick( TObject *Sender );
    void __fastcall ButtonSaveClick( TObject *Sender );
    void __fastcall ButtonCancelClick( TObject *Sender );
    void __fastcall FormClose( TObject *Sender, TCloseAction &Action );
private:	// User declarations
    intf_thread_t *p_intf;
public:		// User declarations
    __fastcall TPreferencesDlg( TComponent* Owner, intf_thread_t *_p_intf );
    void __fastcall CreateConfigDialog( char *psz_module_name );
    void __fastcall SaveValue( module_config_t *p_config );
};
//---------------------------------------------------------------------------
#endif
