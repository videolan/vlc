/*****************************************************************************
 * text.cpp: Text control
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: text.cpp,v 1.4 2003/04/17 13:08:02 karibu Exp $
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
 *          Emmanuel Puig    <karibu@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/


//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "../os_api.h"
#include "../src/bitmap.h"
#include "../src/banks.h"
#include "../src/graphics.h"
#include "../os_graphics.h"
#include "../src/font.h"
#include "generic.h"
#include "text.h"
#include "../src/event.h"
#include "../src/theme.h"
#include "../src/window.h"
#include "../os_window.h"
#include "../src/skin_common.h"



//---------------------------------------------------------------------------
// Scrolling : one for each OS
//---------------------------------------------------------------------------

    #if defined( WIN32 )
    //-----------------------------------------------------------------------
    // Win32 methods
    //-----------------------------------------------------------------------
    void CALLBACK ScrollingTextTimer( HWND hwnd, UINT uMsg, UINT_PTR idEvent,
        DWORD dwTime )
    {
        if( (ControlText *)idEvent != NULL
            && !( (ControlText *)idEvent )->GetSelected() )
        {
            ( (ControlText *)idEvent )->DoScroll();
        }

    }
    //-----------------------------------------------------------------------
    void ControlText::StartScrolling()
    {
        SetTimer( ( (Win32Window *)ParentWindow )->GetHandle(), (UINT_PTR)this,
                  100, (TIMERPROC)ScrollingTextTimer );
    }
    //-----------------------------------------------------------------------
    void ControlText::StopScrolling()
    {
        KillTimer( ( (Win32Window *)ParentWindow )->GetHandle(),
                   (UINT_PTR)this );
    }
    //-----------------------------------------------------------------------

    #else

    //-----------------------------------------------------------------------
    // Gtk2 methods
    //-----------------------------------------------------------------------
    gboolean ScrollingTextTimer( gpointer data )
    {
        if( (ControlText *)data != NULL )
        {
            if( !( (ControlText *)data )->IsScrolling() )
                return false;

            /* FIXME
            if( !( (ControlText *)data )->GetSelected() )
               ( (ControlText *)data )->DoScroll();
            */

            return true;
        }
        else
        {
            return false;
        }
    }
    //-----------------------------------------------------------------------
    void ControlText::StartScrolling()
    {
        g_timeout_add( 100, (GSourceFunc)ScrollingTextTimer, (gpointer)this );
    }
    //-----------------------------------------------------------------------
    void ControlText::StopScrolling()
    {
    }
    //-----------------------------------------------------------------------


    #endif
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// CONTROL TEXT
//---------------------------------------------------------------------------
ControlText::ControlText( string id, bool visible, int x, int y, string text,
    string font, int align, int width, string display, bool scroll,
    int scrollspace, string help, Window *Parent )
    : GenericControl( id, visible, help, Parent )
{
    InitLeft         = x;
    Top              = y;
    InitWidth        = width;
    FontName         = font;
    Text             = text;
    Align            = align;
    Selected         = false;

    // Scrolling parameters
    InitScroll       = scroll;
    Scroll           = false;
    ScrollSpace      = scrollspace;
    PauseScroll      = false;

    // Initialize display
    if( display != "none" )
    {
        int begin = 0;
        int pos = display.find( ';', 0 );
        while( pos > 0 )
        {
            DisplayList.push_back( display.substr( begin, pos - begin ) );
            begin = pos + 1;
            pos = display.find( ';', begin );
        }
        DisplayList.push_back(
            display.substr( begin, display.size() - begin ) );
        Display = DisplayList.begin();
    }

}
//---------------------------------------------------------------------------
ControlText::~ControlText()
{
    if( TextClipRgn != NULL )
        delete TextClipRgn;
    TextWidth = 0;
    SetScrolling();
}
//---------------------------------------------------------------------------
void ControlText::Init()
{
    TextFont     = p_intf->p_sys->p_theme->FntBank->Get( FontName );

    // Init clipping region
    TextClipRgn = NULL;

    // Get size of control
    SetSize();
    SetScrolling();
}
//---------------------------------------------------------------------------
void ControlText::SetScrolling()
{
    if( !Scroll && TextWidth > Width )
    {
        if( InitScroll )
        {
            Scroll = true;
            StartScrolling();
        }
    }
    else if( Scroll && TextWidth <= Width )
    {
        Scroll = false;
        StopScrolling();
    }
}
//---------------------------------------------------------------------------
void ControlText::SetSize()
{
    // Get size parameters
    int w, h;
    TextFont->GetSize( Text, w, h );
    TextWidth = w;

    // Get width if not set
    if( InitWidth <= 0 )
        Width  = w;
    else
        Width  = InitWidth;

    // Set height
    Height = h;

    // Set position wether alignment
    if( Align == DT_CENTER )
    {
        Left     = InitLeft - Width / 2;
        TextLeft = InitLeft - TextWidth / 2;
    }
    else if( Align == DT_RIGHT )
    {
        Left     = InitLeft - Width;
        TextLeft = InitLeft - TextWidth;
    }
    else
    {
        Left     = InitLeft;
        TextLeft = InitLeft;
    }

    // Create clipping region
    if( TextClipRgn != NULL )
        delete TextClipRgn;

    TextClipRgn = (Region *)new OSRegion( Left, Top, Width, Height );

}
//---------------------------------------------------------------------------
bool ControlText::ProcessEvent( Event *evt )
{
    unsigned int msg = evt->GetMessage();
    unsigned int p1  = evt->GetParam1();
    long         p2  = evt->GetParam2();

    switch( msg )
    {
        case CTRL_SET_TEXT:
            if( DisplayList.size() > 0 )
            {
                if( p_intf->p_sys->p_theme->EvtBank->Get( (*Display) )
                    ->IsEqual( (Event*)p1 ) )
                {
                    SetText( (char *)p2 );
                }
            }
            break;
    }
    return false;
}
//---------------------------------------------------------------------------
void ControlText::Draw( int x, int y, int w, int h, Graphics *dest )
{
    if( !Visible )
        return;

    // Test if control is in refresh zone
    int xI, yI, wI, hI;
    if( !GetIntersectRgn( x,y,w,h, Left,Top,Width,Height, xI,yI,wI,hI) )
        return;

    // Change clipping region
    TextClipRgn->Move( -x, -y );
    dest->SetClipRegion( TextClipRgn );

    // Draw text
    if( TextWidth <= Width || !Scroll )
    {
        TextFont->Print( dest, Text, Left - x, Top - y, Width, Height, Align );
    }
    else
    {
        if( TextLeft > Left + ScrollSpace )
        {
            TextFont->Print( dest, Text, TextLeft - x, Top - y,
                         TextWidth, Height, Align );
            TextFont->Print( dest, Text, TextLeft - x - TextWidth - ScrollSpace,
                         Top - y, TextWidth, Height, Align );
        }
        else if( TextLeft + TextWidth + ScrollSpace < Left + Width )
        {
            TextFont->Print( dest, Text, TextLeft - x, Top - y,
                         TextWidth, Height, Align );
            TextFont->Print( dest, Text, TextLeft - x + TextWidth + ScrollSpace,
                         Top - y, TextWidth, Height, Align );
        }
        else
        {
            TextFont->Print( dest, Text, TextLeft - x, Top - y,
                         TextWidth, Height, Align );
        }
    }

    // Reset clipping region to old region
    Region *destClipRgn = (Region *)new OSRegion( 0, 0, w, h );
    dest->SetClipRegion( destClipRgn );
    delete destClipRgn;
    TextClipRgn->Move( x, y );
}
//---------------------------------------------------------------------------
void ControlText::SetText( const string newText )
{
    if( Text != newText )
    {
        Selected = false;
        Text     = newText;
        SetSize();
        SetScrolling();
        ParentWindow->Refresh( Left, Top, Width, Height );
    }
}
//---------------------------------------------------------------------------
void ControlText::DoScroll()
{
    if( !PauseScroll )
    {
        TextLeft -= 2;
        if( TextLeft + TextWidth < Left )
            TextLeft += TextWidth + ScrollSpace;

        ParentWindow->Refresh( Left, Top, Width, Height );
    }
}
//---------------------------------------------------------------------------
void ControlText::MoveRelative( int xOff, int yOff )
{
    InitLeft += xOff;
    Top      += yOff;
    SetSize();
}
//---------------------------------------------------------------------------
bool ControlText::MouseUp( int x, int y, int button )
{
    Selected = false;
    if( MouseOver( x, y ) && button == 1 )
    {
        if( DisplayList.size() > 1 || TextWidth > Width )
            return true;
    }
    return false;

}
//---------------------------------------------------------------------------
bool ControlText::MouseDown( int x, int y, int button )
{
    if( MouseOver( x, y ) && button == 1 )
    {
        if( TextWidth > Width )
        {
            PauseScroll = !PauseScroll;
            OSAPI_GetMousePos( MouseX, MouseY );
            SelectedX = MouseX;
            Selected = true;
            return true;
        }
        else if( DisplayList.size() > 1 )
        {
            return true;
        }
    }
    return false;
}
//---------------------------------------------------------------------------
bool ControlText::MouseMove( int x, int y, int button )
{
    if( Selected && button == 1 )
    {
        OSAPI_GetMousePos( MouseX, MouseY );

        if( MouseX != SelectedX )
        {
            TextLeft += MouseX - SelectedX;
            SelectedX = MouseX;

            while( TextLeft + TextWidth < Left )
                TextLeft += TextWidth + ScrollSpace;

            while( TextLeft > Left + ScrollSpace )
                TextLeft -= TextWidth + ScrollSpace;

            ParentWindow->Refresh( Left, Top, Width, Height );
        }
    }
    return false;
}
//---------------------------------------------------------------------------
bool ControlText::MouseOver( int x, int y )
{
    if( x >= Left && x < Left + Width && y >= Top && y < Top + Height )
        return true;
    else
        return false;
}
//---------------------------------------------------------------------------
bool ControlText::MouseDblClick( int x, int y, int button )
{
    Selected = false;
    if( x >= Left && x < Left + Width && y >= Top && y < Top + Height
        && button == 1 && DisplayList.size() > 1 )
    {
        Display++;
        if( Display == DisplayList.end() )
            Display = DisplayList.begin();
        return true;
    }
    else
    {
        return false;
    }
}
//---------------------------------------------------------------------------

