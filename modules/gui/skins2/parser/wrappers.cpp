/*****************************************************************************
 * wrappers.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: wrappers.cpp,v 1.1 2004/01/03 23:31:33 asmax Exp $
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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

#include <math.h>
#include "wrappers.h"
#include "parser_context.hpp"

// Current DTD version
#define SKINS_DTD_VERSION "2.0"

// Local functions
static bool ConvertBoolean( const char *value );
static int  ConvertColor( const char *transcolor );

// Useful macros
#define DATA ((ParserContext*)pContext)->m_data
#define WIN_ID ((ParserContext*)pContext)->m_curWindowId
#define LAY_ID ((ParserContext*)pContext)->m_curLayoutId
#define CUR_LIST ((ParserContext*)pContext)->m_curListId
#define X_OFFSET ((ParserContext*)pContext)->m_xOffset
#define Y_OFFSET ((ParserContext*)pContext)->m_yOffset
#define LAYER ((ParserContext*)pContext)->m_curLayer


void AddBitmap( void *pContext, char *name, char *file,
                char *transcolor )
{
    const BuilderData::Bitmap bitmap( name, file,
                                      ConvertColor( transcolor ) );
    DATA.m_listBitmap.push_back( bitmap );
}


void AddEvent( void *pContext, char *name, char *event, char *key )
{
//    const BuilderData:: Command( name, event, key );
}


void AddFont( void *pContext, char *name, char *font, char *size,
              char *color, char *italic, char *underline )
{
    const BuilderData::Font fontData( name, font, atoi( size ) );
    DATA.m_listFont.push_back( fontData );
}


void AddThemeInfo( void *pContext, char *name, char *author,
                   char *email, char *webpage )
{
    msg_Warn( ((ParserContext*)pContext)->m_pIntf,
              "skin: %s  author: %s", name, author );
}


void StartWindow( void *pContext, char *name, char *x, char *y,
                  char *visible, char *dragdrop, char *playOnDrop )
{
    const BuilderData::Window window( name, atoi( x ) + X_OFFSET,
        atoi( y ) + Y_OFFSET, ConvertBoolean( visible ),
        ConvertBoolean( dragdrop ), ConvertBoolean( playOnDrop ));
    DATA.m_listWindow.push_back( window );
    WIN_ID = name;
}


void EndWindow( void *pContext )
{
//    ((Builder*)pContext)->endWindow();
}


void StartLayout( void *pContext, char *id, char *width, char *height,
                  char *minwidth, char *maxwidth, char *minheight,
                  char *maxheight )
{
    const BuilderData::Layout layout( id, atoi( width ), atoi( height ),
                                      atoi( minwidth ), atoi( maxwidth ),
                                      atoi( minheight ), atoi( maxheight ),
                                      WIN_ID );
    DATA.m_listLayout.push_back( layout );
    LAY_ID = id;
    LAYER = 0;
}


void EndLayout( void *pContext )
{
//    ((Builder*)pContext)->endLayout();
}


void StartTheme( void *pContext, char *version, char *magnet,
                 char *alpha, char *movealpha, char *fadetime )
{
    // Check the version
    if( strcmp( version, SKINS_DTD_VERSION ) )
    {
        msg_Err( ((ParserContext*)pContext)->m_pIntf, "Bad theme version :"
                 " %s (you need version %s)", version, SKINS_DTD_VERSION );
    }
    const BuilderData::Theme theme( atoi( magnet ),
        atoi( alpha ), atoi( movealpha ), atoi( fadetime ) );
    DATA.m_listTheme.push_back( theme );
}


void EndTheme( void *pContext )
{
 //   ((Builder*)pContext)->endTheme();
}


void StartGroup( void *pContext, char *x, char *y )
{
    X_OFFSET += atoi( x );
    Y_OFFSET += atoi( y );
    ((ParserContext*)pContext)->m_xOffsetList.push_back( atoi( x ) );
    ((ParserContext*)pContext)->m_yOffsetList.push_back( atoi( y ) );
}


void EndGroup( void *pContext )
{
    X_OFFSET -= ((ParserContext*)pContext)->m_xOffsetList.back();
    Y_OFFSET -= ((ParserContext*)pContext)->m_yOffsetList.back();
    ((ParserContext*)pContext)->m_xOffsetList.pop_back();
    ((ParserContext*)pContext)->m_yOffsetList.pop_back();
}


void AddAnchor( void *pContext, char *x, char *y, char *len,
                char *priority )
{
    const BuilderData::Anchor anchor( atoi( x ) + X_OFFSET,
        atoi( y ) + Y_OFFSET, atoi( len ), atoi( priority ), WIN_ID );
    DATA.m_listAnchor.push_back( anchor );
}


//---------------------------------------------------------------------------
// CONTROLS
//---------------------------------------------------------------------------
void AddImage( void *pContext, char *id, char *visible, char *x,
               char *y, char *lefttop, char *rightbottom, char *image,
               char *event, char *help )
{
    const BuilderData::Image imageData( id, atoi( x ) + X_OFFSET,
        atoi( y ) + Y_OFFSET, lefttop, rightbottom, ConvertBoolean( visible ),
        image, event, help, LAYER, WIN_ID, LAY_ID );
    LAYER++;
    DATA.m_listImage.push_back( imageData );
}


void AddRectangle( void *pContext, char *id, char *visible, char *x,
                   char *y, char *w, char *h, char *color, char *event,
                   char *help )
{
//    msg_Warn( ((Builder*)pContext)->getIntf(), "Do we _really_ need a Rectangle control?" );
}


void AddButton( void *pContext,
    char *id,
    char *x, char *y, char *lefttop, char *rightbottom,
    char *up, char *down, char *over,
    char *action, char *tooltiptext, char *help )
{
    const BuilderData::Button button( id, atoi( x ) + X_OFFSET,
        atoi( y ) + Y_OFFSET, lefttop, rightbottom, up, down, over, action,
        tooltiptext, help, LAYER, WIN_ID, LAY_ID );
    LAYER++;
    DATA.m_listButton.push_back( button );
}


void AddCheckBox( void *pContext, char *id,
                  char *x, char *y, char *lefttop, char *rightbottom,
                  char *up1, char *down1, char *over1, char *up2,
                  char *down2, char *over2, char *state, char *action1,
                  char *action2, char *tooltiptext1, char *tooltiptext2,
                  char *help )
{
    const BuilderData::Checkbox checkbox( id, atoi( x ) + X_OFFSET,
        atoi( y ) + Y_OFFSET, lefttop, rightbottom, up1, down1, over1, up2,
        down2, over2, state, action1, action2, tooltiptext1, tooltiptext2,
        help, LAYER, WIN_ID, LAY_ID );
    LAYER++;
    DATA.m_listCheckbox.push_back( checkbox );
}


void AddSlider( void *pContext, char *id, char *visible, char *x, char *y,
                char *lefttop, char *rightbottom,
                char *up, char *down, char *over, char *points,
                char *thickness, char *value, char *tooltiptext, char *help )
{
    string newValue = value;
    if( CUR_LIST != "" )
    {
        // Slider associated to a list
        // XXX
        newValue = "playlist.slider";
    }
    const BuilderData::Slider slider( id, visible, atoi( x ) + X_OFFSET,
        atoi( y ) + Y_OFFSET, lefttop, rightbottom, up, down, over, points,
        atoi( thickness ), newValue, tooltiptext, help, LAYER, WIN_ID, LAY_ID );
    LAYER++;
    DATA.m_listSlider.push_back( slider );
}


void AddRadialSlider( void *pContext, char *id, char *visible, char *x, char *y,
                      char *lefttop, char *rightbottom, char *sequence,
                      char *nbImages, char *minAngle, char *maxAngle,
                      char *value, char *tooltiptext, char *help )
{
    const BuilderData::RadialSlider radial( id, visible, atoi( x ) + X_OFFSET,
        atoi( y ) + Y_OFFSET, lefttop, rightbottom, sequence, atoi( nbImages ),
        atof( minAngle ) * M_PI / 180, atof( maxAngle ) * M_PI / 180, value,
        tooltiptext, help, LAYER, WIN_ID, LAY_ID );
    LAYER++;
    DATA.m_listRadialSlider.push_back( radial );
}


void AddPlaylist( void *pContext, char *id, char *visible, char *x,
                  char *y, char *width, char *height, char *lefttop,
                  char *rightbottom, char *font, char *var, char *fgcolor,
                  char *playcolor, char *bgcolor1, char *bgcolor2,
                  char *selcolor, char *help )
{
    const BuilderData::List listData( id, atoi( x ) + X_OFFSET,
        atoi( y ) + Y_OFFSET, atoi( width), atoi( height ), lefttop,
        rightbottom, font, var, ConvertColor( fgcolor ),
        ConvertColor( playcolor ), ConvertColor( bgcolor1 ),
        ConvertColor( bgcolor2 ), ConvertColor( selcolor ), help, LAYER,
        WIN_ID, LAY_ID );
    LAYER++;
    CUR_LIST = id;
    DATA.m_listList.push_back( listData );
}


void AddPlaylistEnd( void *pContext )
{
    CUR_LIST = "";
}


void AddText( void *pContext, char *id, char *visible, char *x,
              char *y, char *text, char *font, char *align, char *width,
              char *display, char *scroll, char *scrollspace, char *help )
{
    const BuilderData::Text textData( id, atoi( x ) + X_OFFSET,
        atoi( y ) + Y_OFFSET, font, text, atoi( width ), help, LAYER, WIN_ID,
        LAY_ID );
    LAYER++;
    DATA.m_listText.push_back( textData );
}




//---------------------------------------------------------------------------
// Useful functions
//---------------------------------------------------------------------------

static bool ConvertBoolean( const char *value )
{
    return strcmp( value, "true" ) == 0;
}


static int ConvertColor( const char *transcolor )
{
    unsigned long iRed, iGreen, iBlue;
    iRed = iGreen = iBlue = 0;
    sscanf( transcolor, "#%2lX%2lX%2lX", &iRed, &iGreen, &iBlue );
    return ( iRed << 16 | iGreen << 8 | iBlue );
}

