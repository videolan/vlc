/*****************************************************************************
 * playlist.cpp: Playlist control
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: playlist.cpp,v 1.12 2003/06/08 15:22:03 asmax Exp $
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


//--- GENERAL ---------------------------------------------------------------
#include <math.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "../os_api.h"
#include "../src/bitmap.h"
#include "../src/banks.h"
#include "../src/bezier.h"
#include "../src/graphics.h"
#include "../os_graphics.h"
#include "../src/font.h"
#include "../os_font.h"
#include "generic.h"
#include "slider.h"
#include "playlist.h"
#include "../src/event.h"
#include "../src/theme.h"
#include "../src/window.h"
#include "../src/skin_common.h"



//---------------------------------------------------------------------------
// Control Playlist
//---------------------------------------------------------------------------
ControlPlayList::ControlPlayList( string id, bool visible, int width,
    int infowidth, string font, string playfont, int selcolor, double *ptx,
    double *pty, int nb, bool longfilename, string help, SkinWindow *Parent )
    : GenericControl( id, visible, help, Parent )
{
    Left          = 0;
    Top           = 0;
    Column        = 1;
    Line          = 1;
    Margin        = 1;
    Enabled       = true;
    FontName      = font;
    PlayFontName  = playfont;

    // Text zone
    CaseWidth    = width;
    InfoWidth    = infowidth;
    NumWidth     = 0;
    FileWidth    = 0;
    CaseHeight   = 1;
    NumOfItems   = 0;
    SelectColor  = selcolor;
    TextCurve    = new Bezier( ptx, pty, nb, BEZIER_PTS_Y );
    LongFileName = longfilename;

    // Scroll
    StartIndex  = 0;
}
//---------------------------------------------------------------------------
ControlPlayList::~ControlPlayList()
{
    if( PlayList != NULL )
    {
        vlc_object_release( PlayList );
    }
}
//---------------------------------------------------------------------------
void ControlPlayList::Init()
{
    int i, j, h;
    int *x, *y;

    // Font & events
    UpdateEvent = p_intf->p_sys->p_theme->EvtBank->Get( "playlist_refresh" );
    TextFont    = p_intf->p_sys->p_theme->FntBank->Get( FontName );
    if( PlayFontName == "none" )
        PlayFont = p_intf->p_sys->p_theme->FntBank->Get( FontName );
    else
        PlayFont = p_intf->p_sys->p_theme->FntBank->Get( PlayFontName );

    TextFont->GetSize( "lp", h, CaseHeight );

    // Get bitmap from list
    Img = NULL;

    // Get points for Text curve
    h  = TextCurve->GetNumOfDifferentPoints();
    x  = new int[h + 1];
    y  = new int[h + 1];
    TextCurve->GetDifferentPoints( x, y, 0, 0 );

    // Get top of first point
    TextCurve->GetPoint( 0, i, TextTop );

    // Set number of lines
    Line = 0;
    for( i = 0; i < h; i++ )
    {
        if( ( Line + 1 ) * CaseHeight < y[i] - TextTop )
            Line++;
    }
    CaseLeft     = new int[Line];
    CaseRight    = new int[Line];
    CaseTextLeft = new int[Line];
    for( i = 0; i < Line; i++ )
    {
        CaseLeft[i]     = x[i * CaseHeight];
        CaseTextLeft[i] = x[i * CaseHeight];
        for( j = 1; j < CaseHeight; j++ )
        {
            if( x[i * CaseHeight + j] < CaseLeft[i] )
                CaseLeft[i] = x[i * CaseHeight + j];
            if( x[i * CaseHeight + j] > CaseTextLeft[i] )
                CaseTextLeft[i] = x[i * CaseHeight + j];
        }
        CaseRight[i] = CaseTextLeft[i] + CaseWidth;
    }

    // Get size of text zone
    TextHeight = Line * CaseHeight;
    TextLeft   = CaseLeft[0];
    TextWidth  = CaseRight[0];
    for( i = 1; i < Line; i++ )
    {
        if( CaseLeft[i] < TextLeft )
            TextLeft = CaseLeft[i];
        if( CaseRight[i] > TextWidth )
            TextWidth = CaseRight[i];
    }
    TextWidth -= TextLeft;

    // Set Text Clipping Region
    TextClipRgn = (SkinRegion *)new OSRegion;
    for( i = 0; i < Line; i++ )
    {
        for( j = 0; j < CaseHeight; j++ )
        {
            h = i * CaseHeight + j;
            TextClipRgn->AddRectangle( x[h] - TextLeft, h, CaseWidth, 1 );
        }
    }

    // Curve is no more needed so delete it
    delete TextCurve;
    delete[] x;
    delete[] y;

    // Get size of control
    Left   = Slider->GetLeft();
    Top    = Slider->GetTop();
    Width  = Slider->GetLeft() + Slider->GetWidth();
    Height = Slider->GetTop()  + Slider->GetHeight();
    if( TextLeft < Left )
        Left = TextLeft;
    if( TextTop  < Top  )
        Top  = TextTop;
    if( TextLeft + TextWidth  > Width )
        Width  = TextLeft + TextWidth;
    if( TextTop + TextHeight > Height )
        Height  = TextTop  + TextHeight;
    Width  -= Left;
    Height -= Top;

    // Getting playlist
    PlayList = (playlist_t *)
        vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( PlayList == NULL )
        msg_Err( p_intf, "cannot find a playlist object" );

    Slider->Init();
    Slider->Enable( p_intf->p_sys->p_theme->EvtBank->Get( "none" ), true );
    RefreshList();
}
//---------------------------------------------------------------------------
bool ControlPlayList::ProcessEvent( Event *evt )
{
    switch( evt->GetMessage() )
    {
        case CTRL_ENABLED:
            Enable( (Event*)evt->GetParam1(), (bool)evt->GetParam2() );
            break;

        case CTRL_SYNCHRO:
            if( UpdateEvent->IsEqual( (Event*)evt->GetParam1() ) )
            {
                RefreshList();
                RefreshAll();
            }
            break;

        case PLAYLIST_ID_DEL:
            if( (GenericControl *)evt->GetParam1() == this )
            {
                for( int i = PlayList->i_size - 1; i >= 0; i-- )
                {
                    if( Select[i] && i != PlayList->i_index )
                        playlist_Delete( PlayList, i );
                }
                RefreshList();
                RefreshAll();
            }
            break;
    }
    return false;
}
//---------------------------------------------------------------------------
void ControlPlayList::RefreshList()
{
    vlc_mutex_lock( &PlayList->object_lock );
    
    if( NumOfItems != PlayList->i_size )
    {
        if( NumOfItems > 0 )
            delete[] Select;
        NumOfItems = PlayList->i_size;
        if( PlayList->i_size > 0 )
        {
            Select = new bool[NumOfItems];
            for( int i = 0; i < NumOfItems; i++ )
                Select[i] = false;
            int h;
            sprintf( Num, " %i", NumOfItems + 1 );
            TextFont->GetSize( Num, NumWidth, h);
            FileWidth = CaseWidth - NumWidth - InfoWidth;
        }

        int Range = PlayList->i_size - Line * Column;
        if( Range < 0 )
            Range = 0;

        Slider->ChangeSliderRange( Range );
        StartIndex = Slider->GetCursorPosition();
    }

    vlc_mutex_unlock( &PlayList->object_lock );
}
//---------------------------------------------------------------------------
void ControlPlayList::RefreshAll()
{
    ParentWindow->Refresh( TextLeft, TextTop, TextWidth, TextHeight );
}
//---------------------------------------------------------------------------
void ControlPlayList::Draw( int x, int y, int w, int h, Graphics *dest )
{
    if( !Visible )
        return;

    int xI, yI, wI, hI;
    // Slider Image
    Slider->Draw( x, y, w, h, dest );

    // TextZone
    if( GetIntersectRgn( x, y, w, h, TextLeft, TextTop, TextWidth, TextHeight,
                         xI, yI, wI, hI) )
    {
        // Change clipping region
        SkinRegion *destClipRgn = (SkinRegion *)new OSRegion( 0, 0, w, h );
        TextClipRgn->Move( TextLeft - x, TextTop - y );
        dest->SetClipRegion( TextClipRgn );

        // Draw each line
        DrawAllCase( dest, x, y, wI, hI );

        // Reset clipping region to old region
        dest->SetClipRegion( destClipRgn );
        delete destClipRgn;
        TextClipRgn->Move( x - TextLeft, y - TextTop );
    }
}
//---------------------------------------------------------------------------
void ControlPlayList::DrawAllCase( Graphics *dest, int x, int y, int w, int h )
{
    int i;
    for( i = 0; i < NumOfItems - StartIndex && i < Line * Column; i++ )
    {
        DrawCase( dest, i + StartIndex, x, y, w, h );
    }
}
//---------------------------------------------------------------------------
void ControlPlayList::DrawCase( Graphics *dest, int i, int x, int y, int w,
                                int h )
{
    // Test if case is in range
    int j = i - StartIndex;
    if( j < 0 || j >= Line * Column )
        return;

    // Draw background if selected
    if( Select[i] )
    {
        dest->DrawRect(
            CaseLeft[j] - x,
            TextTop + j * CaseHeight - y,
            CaseRight[j] - CaseLeft[j],
            CaseHeight,
            SelectColor
        );
    }

    // Choose font
    SkinFont *F;
    if( PlayList->i_index == i )
        F = PlayFont;
    else
        F = TextFont;

    // Print number
    sprintf( Num, "%i", i + 1 );
    F->Print( dest,
        Num,
        CaseTextLeft[j] - x, TextTop + j * CaseHeight - y,
        NumWidth - Margin, CaseHeight, VLC_FONT_ALIGN_RIGHT );

    // Print name
    F->Print( dest,
        GetFileName( i ),
        NumWidth + Margin + CaseTextLeft[j] - x,
        TextTop + j * CaseHeight - y,
        FileWidth - 2 * Margin, CaseHeight, VLC_FONT_ALIGN_LEFT );

    // Print info
    F->Print( dest,
        "no info",
        NumWidth + FileWidth + Margin + CaseTextLeft[j] - x,
        TextTop + j * CaseHeight - y,
        InfoWidth - Margin, CaseHeight, VLC_FONT_ALIGN_CENTER );
}
//---------------------------------------------------------------------------
char * ControlPlayList::GetFileName( int i )
{
    if( LongFileName )
    {
        return PlayList->pp_items[i]->psz_name;
    }
    else
    {
        string f = PlayList->pp_items[i]->psz_name;
        int pos  = f.rfind( DIRECTORY_SEPARATOR, f.size() );
        return PlayList->pp_items[i]->psz_name + pos + 1;
    }
}
//---------------------------------------------------------------------------
void ControlPlayList::MoveRelative( int xOff, int yOff )
{
    Slider->MoveRelative( xOff, yOff );
    Left     += xOff;
    Top      += yOff;
    TextLeft += xOff;
    TextTop  += yOff;
    for( int i = 1; i < Line; i++ )
    {
        CaseLeft[i]     += xOff;
        CaseTextLeft[i] += xOff;
        CaseRight[i]    += xOff;
    }
}
//---------------------------------------------------------------------------
double ControlPlayList::Dist( int x1, int y1, int x2, int y2 )
{
    return sqrt( (x1-x2)*(x1-x2) + (y1-y2)*(y1-y2) );
}
//---------------------------------------------------------------------------
bool ControlPlayList::MouseDown( int x, int y, int button )
{
    if( !Enabled )
        return false;

    // If hit into slider
    if( Slider->MouseDown( x, y, button ) )
    {
        int New = Slider->GetCursorPosition();
        if( New != StartIndex )
        {
            StartIndex = New;
            ParentWindow->Refresh( TextLeft,TextTop,TextWidth,TextHeight );
        }
        return true;
    }

    if( !TextClipRgn->Hit( x - TextLeft, y - TextTop ) )
        return false;

    // If hit in a case
    int i, j;
    for( i = 0; i < PlayList->i_size - StartIndex && i < Line * Column; i++)
    {
        if( x >= CaseLeft[i] && x <= CaseRight[i] && y >= TextTop +
            i * CaseHeight  && y < TextTop + (i + 1) * CaseHeight )
        {
            for( j = 0; j < NumOfItems; j++ )
            {
                if( j == i + StartIndex )
                    Select[j] = !Select[j];
            }
            RefreshAll();
            return true;
        }
    }

    return false;
}
//---------------------------------------------------------------------------
bool ControlPlayList::MouseUp( int x, int y, int button )
{
    if( !Enabled )
        return false;

    // If hit into slider
    if( Slider->MouseUp( x, y, button ) )
        return true;

    return false;
}
//---------------------------------------------------------------------------
bool ControlPlayList::MouseMove( int x, int y, int button )
{
    if( !Enabled || !button )
        return false;

    // If hit into slider
    if( Slider->MouseMove( x, y, button ) )
    {
        int New = Slider->GetCursorPosition();
        if( New != StartIndex )
        {
            StartIndex = New;
            RefreshAll();
        }
        return true;
    }

    return false;
}
//---------------------------------------------------------------------------
bool ControlPlayList::MouseScroll( int x, int y, int direction )
{
    if( !Enabled )
        return false;

    if( !TextClipRgn->Hit( x - Left, y - Top ) && !Slider->MouseOver( x, y ) )
        return false;

    long pos = StartIndex;
    switch( direction )
    {
        case MOUSE_SCROLL_UP:
            if( pos > 0 ) pos--;
            break;
        case MOUSE_SCROLL_DOWN:
            if( pos + Line  < NumOfItems ) pos++;
            break;
    }
    StartIndex = pos;
    Slider->SetCursorPosition( pos );
    RefreshAll();
    return true;
}
//---------------------------------------------------------------------------
bool ControlPlayList::MouseOver( int x, int y )
{
    if( TextClipRgn->Hit( x - Left, y - Top ) || Slider->MouseOver( x, y ) )
        return true;
    else
        return false;
}
//---------------------------------------------------------------------------
bool ControlPlayList::MouseDblClick( int x, int y, int button )
{
    if( !Enabled || button != 1 )
        return false;

    int i;

    if( !TextClipRgn->Hit( x - TextLeft, y - TextTop ) )
        return false;

    for( i = 0; i < PlayList->i_size - StartIndex && i < Line * Column; i++ )
    {
        if( x >= CaseLeft[i] && x <= CaseRight[i] && y >=
            TextTop + i * CaseHeight  && y < TextTop + (i + 1) * CaseHeight )
        {
            playlist_Goto( PlayList, i + StartIndex );
            OSAPI_PostMessage( NULL, VLC_INTF_REFRESH, 0, (int)false );
            return true;
        }
    }
    return false;
}
//---------------------------------------------------------------------------
bool ControlPlayList::ToolTipTest( int x, int y )
{
    if( !Enabled )
        return false;

    int i, w, h;
    for( i = 0; i < PlayList->i_size - StartIndex && i < Line * Column; i++ )
    {
        if( x >= CaseLeft[i] && x <= CaseRight[i] && y >=
            TextTop + i * CaseHeight  && y < TextTop + (i + 1) * CaseHeight )
        {
            TextFont->GetSize( PlayList->pp_items[i + StartIndex]->psz_name, w,
                               h );
            if( w > FileWidth )
            {
                ParentWindow->ChangeToolTipText(
                    (string)PlayList->pp_items[i + StartIndex]->psz_name );
                return true;
            }
        }
    }
    return false;
}
//---------------------------------------------------------------------------
void ControlPlayList::InitSliderCurve( double *ptx, double *pty, int nb,
                                       string scroll_up, string scroll_down )
{
    Slider = new ControlSlider( "none", true, "none", scroll_up, scroll_down,
        ptx, pty, nb, "none", "", ParentWindow );
}
//---------------------------------------------------------------------------

