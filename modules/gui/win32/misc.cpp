/*****************************************************************************
 * misc.cpp: miscellaneous functions.
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

#include "misc.h"

/****************************************************************************
 * This function replaces "Hint", "Caption" and "Text" properties of each
 * component of the form by the appropriate translation.
 ****************************************************************************/
void __fastcall Translate( TForm *Form )
{
#if 0
    Form->Hint = N_( Form->Hint );
    Form->Caption = N_( Form->Caption );

    int i;
    for( i = 0; i < Form->ComponentCount; i++ )
    {
        // Does this component need a translation ?
        if( Form->Components[i]->Tag > 0 )
        {
            TComponent *Component = Form->Components[i];

            // Hint property
            if( Component->Tag & 1 )
            {
                if( Component->InheritsFrom( __classid( TControl ) ) )
                {
                    TControl *Object = (TControl *) Component;
                    Object->Hint = N_( Object->Hint );
                }
                else if( Component->InheritsFrom( __classid( TMenuItem ) ) )
                {
                    TMenuItem *Object = (TMenuItem *) Component;
                    Object->Hint = N_( Object->Hint );
                }
            }

            // Caption property
            if( Component->Tag & 2 )
            {
                if( Component->InheritsFrom( __classid( TMenuItem ) ) )
                {
                    TMenuItem *Object = (TMenuItem *) Component;
                    Object->Caption = N_( Object->Caption );
                }
                else if( Component->InheritsFrom( __classid( TLabel ) ) )
                {
                    TLabel *Object = (TLabel *) Component;
                    Object->Caption = N_( Object->Caption );
                }
                else if( Component->InheritsFrom( __classid( TButton ) ) )
                {
                    TButton *Object = (TButton *) Component;
                    Object->Caption = N_( Object->Caption );
                }
                else if( Component->InheritsFrom( __classid( TToolButton ) ) )
                {
                    TToolButton *Object = (TToolButton *) Component;
                    Object->Caption = N_( Object->Caption );
                }
                else if( Component->InheritsFrom( __classid( TRadioButton ) ) )
                {
                    TRadioButton *Object = (TRadioButton *) Component;
                    Object->Caption = N_( Object->Caption );
                }
                else if( Component->InheritsFrom( __classid( TCheckBox ) ) )
                {
                    TCheckBox *Object = (TCheckBox *) Component;
                    Object->Caption = N_( Object->Caption );
                }
                else if( Component->InheritsFrom( __classid( TRadioGroup ) ) )
                {
                    TRadioGroup *Object = (TRadioGroup *) Component;
                    Object->Caption = N_( Object->Caption );
                }
                else if( Component->InheritsFrom( __classid( TGroupBox ) ) )
                {
                    TGroupBox *Object = (TGroupBox *) Component;
                    Object->Caption = N_( Object->Caption );
                }
                else if( Component->InheritsFrom( __classid( TTabSheet ) ) )
                {
                    TTabSheet *Object = (TTabSheet *) Component;
                    Object->Caption = N_( Object->Caption );
                }
                else if( Component->InheritsFrom( __classid( TListView ) ) )
                {
                    TListView *Object = (TListView *) Component;
                    int iCol;
                    for( iCol = 0; iCol < Object->Columns->Count; iCol++ )
                        Object->Columns->Items[iCol]->Caption =
                                 N_( Object->Columns->Items[iCol]->Caption );
                }
            }

            // Text property
            if( Component->Tag & 4 )
            {
                if( Component->InheritsFrom( __classid( TEdit ) ) )
                {
                    TEdit *Object = (TEdit *) Component;
                    Object->Text = N_( Object->Text );
                }
                else if( Component->InheritsFrom( __classid( TComboBox ) ) )
                {
                    TComboBox *Object = (TComboBox *) Component;
                    Object->Text = N_( Object->Text );
                }
            }
        }
    }
#endif
}
 
