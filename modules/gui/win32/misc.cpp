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
#include <comctrls.hpp>
#pragma hdrstop

#include <vlc/vlc.h>

#include "misc.h"

/****************************************************************************
 * This function replaces "Hint", "Caption" and "Text" properties of each
 * component of the form by the appropriate translation.
 ****************************************************************************/
void __fastcall Translate( TForm *Form )
{
    // We must test the string because we don't want to get the default one
    if( Form->Hint != "" )
    {
        Form->Hint = _( Form->Hint.c_str() );
    }
    if( Form->Caption != "" )
    {
        Form->Caption = _( Form->Caption.c_str() );
    }

    for( int i = 0; i < Form->ComponentCount; i++ )
    {
        // Does this component need a translation ?
        if( Form->Components[i]->Tag > 0 )
        {
            TComponent *Component = Form->Components[i];

            // Note: the Text property isn't translated, because we don't want
            // to modify the content of TEdit or TComboBox objects, which
            // may have default values

            // Hint property
            if( Component->Tag & 1 )
            {
                if( Component->InheritsFrom( __classid( TControl ) ) )
                {
                    TControl *Object = (TControl *) Component;
                    if( Object->Hint != "" )
                    {
                        Object->Hint = _( Object->Hint.c_str() );
                    }
                }
                else if( Component->InheritsFrom( __classid( TMenuItem ) ) )
                {
                    TMenuItem *Object = (TMenuItem *) Component;
                    if( Object->Hint != "" )
                    {
                        Object->Hint = _( Object->Hint.c_str() );
                    }
                }
            }

            // Caption property
            if( Component->Tag & 2 )
            {
                if( Component->InheritsFrom( __classid( TMenuItem ) ) )
                {
                    TMenuItem *Object = (TMenuItem *) Component;
                    if( Object->Caption != "" )
                    {
                        Object->Caption = _( Object->Caption.c_str() );
                    }
               }
                else if( Component->InheritsFrom( __classid( TLabel ) ) )
                {
                    TLabel *Object = (TLabel *) Component;
                    if( Object->Caption != "" )
                    {
                        Object->Caption = _( Object->Caption.c_str() );
                    }
                }
                else if( Component->InheritsFrom( __classid( TButton ) ) )
                {
                    TButton *Object = (TButton *) Component;
                    if( Object->Caption != "" )
                    {
                        Object->Caption = _( Object->Caption.c_str() );
                    }
                }
                else if( Component->InheritsFrom( __classid( TRadioButton ) ) )
                {
                    TRadioButton *Object = (TRadioButton *) Component;
                    if( Object->Caption != "" )
                    {
                        Object->Caption = _( Object->Caption.c_str() );
                    }
                }
                else if( Component->InheritsFrom( __classid( TCheckBox ) ) )
                {
                    TCheckBox *Object = (TCheckBox *) Component;
                    if( Object->Caption != "" )
                    {
                        Object->Caption = _( Object->Caption.c_str() );
                    }
                }
                else if( Component->InheritsFrom( __classid( TRadioGroup ) ) )
                {
                    TRadioGroup *Object = (TRadioGroup *) Component;
                    if( Object->Caption != "" )
                    {
                        Object->Caption = _( Object->Caption.c_str() );
                    }
                }
                else if( Component->InheritsFrom( __classid( TGroupBox ) ) )
                {
                    TGroupBox *Object = (TGroupBox *) Component;
                    if( Object->Caption != "" )
                    {
                        Object->Caption = _( Object->Caption.c_str() );
                    }
                }
#if 0
                else if( Component->InheritsFrom( __classid( TToolButton ) ) )
                {
                    TToolButton *Object = (TToolButton *) Component;
                    if( Object->Caption != "" )
                    {
                        Object->Caption = _( Object->Caption.c_str() );
                    }
                }
#endif
                else if( Component->InheritsFrom( __classid( TListView ) ) )
                {
                    TListView *ListView = (TListView *) Component;
                    for( int iCol = 0; iCol < ListView->Columns->Count; iCol++ )
                    {
                        TListColumn *Object = ListView->Columns->Items[iCol];
                        if( Object->Caption != "" )
                        {
                            Object->Caption = _( Object->Caption.c_str() );
                        }
                    }
                }
            }
        }
    }
}
 
