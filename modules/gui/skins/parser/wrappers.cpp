/*****************************************************************************
 * wrappers.cpp: Wrappers around C++ objects
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: wrappers.cpp,v 1.4 2003/03/19 17:14:50 karibu Exp $
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
#include <stdlib.h>
#include <string>
using namespace std;

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>
extern intf_thread_t *g_pIntf;

//--- SKIN ------------------------------------------------------------------
#include "anchor.h"
#include "banks.h"
#include "controls.h"
#include "window.h"
#include "theme.h"
#include "skin_common.h"
#include "wrappers.h"


//---------------------------------------------------------------------------
// Local prototypes
//---------------------------------------------------------------------------
static bool ConvertBoolean( const char *value );
static int  ConvertColor( const char *transcolor );
static int  CheckCoords( const char *coord );
static void ConvertCoords( char *coord, double *p_coord );
static int  ConvertAlign( char *align );

//---------------------------------------------------------------------------
// Wrappers
//---------------------------------------------------------------------------
void AddBitmap( char *name, char *file, char *transcolor )
{
    g_pIntf->p_sys->p_theme->BmpBank->Add( name, file,
                                           ConvertColor( transcolor ) );
}
//---------------------------------------------------------------------------
void AddEvent( char *name, char *event, char *key )
{
    g_pIntf->p_sys->p_theme->EvtBank->Add( name, event, key );
}
//---------------------------------------------------------------------------
void AddFont( char *name, char *font, char *size, char *color,
              char *weight, char *italic, char *underline )
{
    g_pIntf->p_sys->p_theme->FntBank->Add(
        name, font, atoi( size ), ConvertColor( color ), atoi( weight ),
        ConvertBoolean( italic ), ConvertBoolean( underline ) );
}
//---------------------------------------------------------------------------
void AddThemeInfo( char *name, char *author, char *email, char *webpage )
{
    g_pIntf->p_sys->p_theme->ChangeClientWindowName(
        "VLC Media Player - " + (string)name );
}
//---------------------------------------------------------------------------
void StartWindow( char *name, char *x, char *y, char *visible, char *fadetime,
    char *alpha, char *movealpha, char *dragdrop )
{
    g_pIntf->p_sys->p_theme->AddWindow( name, atoi( x ), atoi( y ),
        ConvertBoolean( visible ), atoi( fadetime ), atoi( alpha ),
        atoi( movealpha ), ConvertBoolean( dragdrop ) );
}
//---------------------------------------------------------------------------
void EndWindow()
{
}
//---------------------------------------------------------------------------
void StartTheme( char *log, char *magnet )
{
    g_pIntf->p_sys->p_theme->StartTheme( ConvertBoolean( log ),
        atoi( magnet ) );
}
//---------------------------------------------------------------------------
void EndTheme()
{
}
//---------------------------------------------------------------------------
void StartControlGroup( char *x, char *y )
{
    g_pIntf->p_sys->p_theme->OffBank->PushOffSet( atoi( x ), atoi( y ) );
}
//---------------------------------------------------------------------------
void EndControlGroup()
{
    g_pIntf->p_sys->p_theme->OffBank->PopOffSet();
}

//---------------------------------------------------------------------------
void AddAnchor( char *x, char *y, char *len, char *priority )
{
    int XOff, YOff;
    Window *vlcWin = g_pIntf->p_sys->p_theme->WindowList.back();

    g_pIntf->p_sys->p_theme->OffBank->GetOffSet( XOff, YOff );
    vlcWin->AnchorList.push_back( new Anchor( g_pIntf, atoi( x ) + XOff,
                                  atoi( y ) + YOff, atoi( len ),
                                  atoi( priority ), vlcWin ) );
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// CONTROLS
//---------------------------------------------------------------------------
void AddImage( char *id, char *visible, char *x, char *y, char *image,
    char *event, char *help )
{
    int XOff, YOff;
    Window *vlcWin = g_pIntf->p_sys->p_theme->WindowList.back();

    g_pIntf->p_sys->p_theme->OffBank->GetOffSet( XOff, YOff );

    vlcWin->ControlList.push_back( new ControlImage( id,
        ConvertBoolean( visible ), atoi( x ) + XOff,
        atoi( y ) + YOff, image, event, help, vlcWin ) );
}
//---------------------------------------------------------------------------
void AddRectangle( char *id, char *visible, char *x, char *y, char *w, char *h,
    char *color, char *event, char *help )
{
    int XOff, YOff;
    Window *vlcWin = g_pIntf->p_sys->p_theme->WindowList.back();

    g_pIntf->p_sys->p_theme->OffBank->GetOffSet( XOff, YOff );

    vlcWin->ControlList.push_back( new ControlRectangle( id,
        ConvertBoolean( visible ), atoi( x ) + XOff, atoi( y ) + YOff,
        atoi( w ), atoi( h ), ConvertColor( color ), event, help, vlcWin ) );
}
//---------------------------------------------------------------------------
void AddButton(
    char *id,
    char *visible,
    char *x, char *y,
    char *up, char *down, char *disabled,
    char *onclick, char *onmouseover, char *onmouseout,
    char *tooltiptext, char *help )
{
    int XOff, YOff;
    Window *vlcWin = g_pIntf->p_sys->p_theme->WindowList.back();

    g_pIntf->p_sys->p_theme->OffBank->GetOffSet( XOff, YOff );

    vlcWin->ControlList.push_back( new ControlButton(
        id,
        ConvertBoolean( visible ),
        atoi( x ) + XOff, atoi( y ) + YOff,
        up, down, disabled,
        onclick, onmouseover, onmouseout,
        tooltiptext, help,
        vlcWin ) );
}
//---------------------------------------------------------------------------
void AddCheckBox(
    char *id,
    char *visible,
    char *x, char *y,
    char *img1, char *img2,
    char *clickimg1, char *clickimg2, char *disabled1, char *disabled2,
    char *onclick1, char *onclick2, char *onmouseover1, char *onmouseout1,
    char *onmouseover2, char *onmouseout2,
    char *tooltiptext1, char *tooltiptext2, char *help )
{
    int XOff, YOff;
    Window *vlcWin = g_pIntf->p_sys->p_theme->WindowList.back();

    g_pIntf->p_sys->p_theme->OffBank->GetOffSet( XOff, YOff );

    vlcWin->ControlList.push_back( new ControlCheckBox(
        id,
        ConvertBoolean( visible ),
        atoi( x ) + XOff, atoi( y ) + YOff,
        img1, img2, clickimg1, clickimg2, disabled1, disabled2,
        onclick1, onclick2, onmouseover1, onmouseout1, onmouseover2,
        onmouseout2,
        tooltiptext1, tooltiptext2, help, vlcWin ) );
}
//---------------------------------------------------------------------------
void AddSlider( char *id, char *visible, char *x, char *y, char *type, char *up,
    char *down, char *abs, char *ord, char *tooltiptext, char *help )
{
    int XOff, YOff, i;
    int res1 = CheckCoords( abs );
    int res2 = CheckCoords( ord );
    if( res1 < 2 || res2 < 2 )
    {
        msg_Warn( g_pIntf, "Cannot add slider: not enough points" );
        return;
    }
    if( res1 != res2 )
    {
        msg_Warn( g_pIntf, "Cannot add slider: invalid list of points" );
        return;
    }

    // now, res1 == res2
    double *p_abs, *p_ord;
    p_abs = new double[res1];
    p_ord = new double[res1];
    ConvertCoords( abs, p_abs );
    ConvertCoords( ord, p_ord );

    Window *vlcWin = g_pIntf->p_sys->p_theme->WindowList.back();

    // Move control
    g_pIntf->p_sys->p_theme->OffBank->GetOffSet( XOff, YOff );
    for( i = 0; i < res1; i++ )
    {
        p_abs[i] += XOff + atoi(x);
        p_ord[i] += YOff + atoi(y);
    }

    // Create Control
    if( g_pIntf->p_sys->p_theme->ConstructPlaylist )
    {
        GenericControl *playlist = vlcWin->ControlList.back();
        ( (ControlPlayList *)playlist )->InitSliderCurve( p_abs, p_ord, res1,
                                                          up, down );
    }
    else
    {
        vlcWin->ControlList.push_back( new ControlSlider( id,
            ConvertBoolean( visible ), type, up, down, p_abs, p_ord, res1,
            tooltiptext, help, vlcWin ) );
    }

    delete[] p_abs;
    delete[] p_ord;
}
//---------------------------------------------------------------------------
void AddPlayList( char *id, char *visible, char *x, char *y, char *width,
    char *infowidth, char *font, char *playfont, char *selcolor, char *abs,
    char *ord, char *longfilename, char *help )
{
    g_pIntf->p_sys->p_theme->ConstructPlaylist = true;

    int XOff, YOff, i;
    int res1 = CheckCoords( abs );
    int res2 = CheckCoords( ord );
    if( res1 < 2 || res2 < 2 )
    {
        msg_Warn( g_pIntf, "Cannot add slider: not enough points" );
        return;
    }
    if( res1 != res2 )
    {
        msg_Warn( g_pIntf, "Cannot add slider: invalid list of points" );
        return;
    }

    // now, res1 == res2
    double *p_abs, *p_ord;
    p_abs = new double[res1];
    p_ord = new double[res1];
    ConvertCoords( abs, p_abs );
    ConvertCoords( ord, p_ord );

    Window *vlcWin = g_pIntf->p_sys->p_theme->WindowList.back();

    // Move control
    g_pIntf->p_sys->p_theme->OffBank->GetOffSet( XOff, YOff );
    for( i = 0; i < res1; i++ )
    {
        p_abs[i] += XOff + atoi(x);
        p_ord[i] += YOff + atoi(y);
    }

    // Move control
    g_pIntf->p_sys->p_theme->OffBank->GetOffSet( XOff, YOff );

    vlcWin->ControlList.push_back( new ControlPlayList( id,
        ConvertBoolean( visible ), atoi( width ), atoi( infowidth ), font,
        playfont, ConvertColor( selcolor ), p_abs, p_ord, res1,
        ConvertBoolean( longfilename ), help, vlcWin ) );

    delete[] p_abs;
    delete[] p_ord;

}
//---------------------------------------------------------------------------
void AddPlayListEnd()
{
    g_pIntf->p_sys->p_theme->ConstructPlaylist = false;
}
//---------------------------------------------------------------------------
void AddText( char *id, char *visible, char *x, char *y, char *text, char *font,
    char *align, char *width, char *display, char *scroll, char *scrollspace,
    char *help )
{
    int XOff, YOff;
    Window *vlcWin = g_pIntf->p_sys->p_theme->WindowList.back();

    g_pIntf->p_sys->p_theme->OffBank->GetOffSet( XOff, YOff );

    vlcWin->ControlList.push_back( new ControlText( id,
        ConvertBoolean( visible ), atoi( x ) + XOff,
        atoi( y ) + YOff, text, font, ConvertAlign( align ), atoi( width ),
        display, ConvertBoolean( scroll ), atoi( scrollspace ), help,
        vlcWin ) );
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// Useful functions
//---------------------------------------------------------------------------
static bool ConvertBoolean( const char *value )
{
    return strcmp( value, "true" ) == 0;
}
//---------------------------------------------------------------------------
static int ConvertColor( const char *transcolor )
{
    int iRed, iGreen, iBlue;
    iRed = iGreen = iBlue = 0;
    sscanf( transcolor, "#%2X%2X%2X", &iRed, &iGreen, &iBlue );
    return ( 65536 * iBlue + 256 * iGreen + iRed );
}
//---------------------------------------------------------------------------
// Check that abs and ord contain the same number of comas
static int CheckCoords( const char *coord )
{
    int i_coord = 1;
    while( coord && *coord )
    {
        if( *coord == ',' )
            i_coord++;
        coord++;
    }
    return i_coord;
}
//---------------------------------------------------------------------------
static void ConvertCoords( char *coord, double *p_coord )
{
    int i = 0;
    char *ptr = coord;

    while( coord && *coord )
    {
        if( *coord == ',' )
        {
            *coord = '\0';
            p_coord[i] = atof( ptr );
            i++;
            ptr = coord + 1;
        }
        coord++;
    }
    p_coord[i] = atof( ptr );
}
//---------------------------------------------------------------------------
static int ConvertAlign( char *align )
{
    if( strcmp( align, "left" ) == 0 )
        return DT_LEFT;
    else if( strcmp( align, "right" ) == 0 )
        return DT_RIGHT;
    else if( strcmp( align, "center" ) == 0 )
        return DT_CENTER;
    else
        return DT_LEFT;
}
//---------------------------------------------------------------------------
