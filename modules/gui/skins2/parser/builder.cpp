/*****************************************************************************
 * builder.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id$
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

#include <string.h>
#include "builder.hpp"
#include "builder_data.hpp"
#include "interpreter.hpp"
#include "../src/png_bitmap.hpp"
#include "../src/os_factory.hpp"
#include "../src/generic_bitmap.hpp"
#include "../src/top_window.hpp"
#include "../src/anchor.hpp"
#include "../src/bitmap_font.hpp"
#include "../src/ft2_font.hpp"
#include "../src/theme.hpp"
#include "../controls/ctrl_button.hpp"
#include "../controls/ctrl_checkbox.hpp"
#include "../controls/ctrl_image.hpp"
#include "../controls/ctrl_list.hpp"
#include "../controls/ctrl_move.hpp"
#include "../controls/ctrl_resize.hpp"
#include "../controls/ctrl_slider.hpp"
#include "../controls/ctrl_radialslider.hpp"
#include "../controls/ctrl_text.hpp"
#include "../controls/ctrl_video.hpp"
#include "../utils/position.hpp"
#include "../utils/var_bool.hpp"
#include "../utils/var_text.hpp"

#include "vlc_image.h"


Builder::Builder( intf_thread_t *pIntf, const BuilderData &rData ):
    SkinObject( pIntf ), m_rData( rData ), m_pTheme( NULL )
{
    m_pImageHandler = image_HandlerCreate( pIntf );
}

Builder::~Builder()
{
    if( m_pImageHandler ) image_HandlerDelete( m_pImageHandler );
}

CmdGeneric *Builder::parseAction( const string &rAction )
{
    return Interpreter::instance( getIntf() )->parseAction( rAction, m_pTheme );
}


// Useful macro
#define ADD_OBJECTS( type ) \
    list<BuilderData::type>::const_iterator it##type; \
    for( it##type = m_rData.m_list##type.begin(); \
         it##type != m_rData.m_list##type.end(); it##type++ ) \
    { \
        add##type( *it##type ); \
    }


Theme *Builder::build()
{
    m_pTheme = new Theme( getIntf() );
    if( m_pTheme == NULL )
    {
        return NULL;
    }

    // Create everything from the data in the XML
    ADD_OBJECTS( Theme );
    ADD_OBJECTS( Bitmap );
    ADD_OBJECTS( BitmapFont );
    ADD_OBJECTS( Font );
    ADD_OBJECTS( Window );
    ADD_OBJECTS( Layout );
    ADD_OBJECTS( Anchor );
    ADD_OBJECTS( Button );
    ADD_OBJECTS( Checkbox );
    ADD_OBJECTS( Image );
    ADD_OBJECTS( Text );
    ADD_OBJECTS( RadialSlider );
    ADD_OBJECTS( Slider );
    ADD_OBJECTS( List );
    ADD_OBJECTS( Video );

    return m_pTheme;
}


// Macro to get a bitmap by its ID in the builder
#define GET_BMP( pBmp, id ) \
    if( id != "none" ) \
    { \
        pBmp = m_pTheme->getBitmapById(id); \
        if( pBmp == NULL ) \
        { \
            msg_Err( getIntf(), "unknown bitmap id: %s", id.c_str() ); \
            return; \
        } \
    }

void Builder::addTheme( const BuilderData::Theme &rData )
{
    WindowManager &rManager = m_pTheme->getWindowManager();
    rManager.setMagnetValue( rData.m_magnet );
    rManager.setAlphaValue( rData.m_alpha );
    rManager.setMoveAlphaValue( rData.m_moveAlpha );
    GenericFont *pFont = getFont( rData.m_tooltipfont );
    if( pFont )
    {
        rManager.createTooltip( *pFont );
    }
    else
    {
        msg_Warn( getIntf(), "Invalid tooltip font: %s",
                  rData.m_tooltipfont.c_str() );
    }
}


void Builder::addBitmap( const BuilderData::Bitmap &rData )
{
    GenericBitmap *pBmp =
        new PngBitmap( getIntf(), m_pImageHandler,
                       rData.m_fileName, rData.m_alphaColor );
    m_pTheme->m_bitmaps[rData.m_id] = GenericBitmapPtr( pBmp );
}


void Builder::addBitmapFont( const BuilderData::BitmapFont &rData )
{
    GenericBitmap *pBmp =
        new PngBitmap( getIntf(), m_pImageHandler, rData.m_file, 0 );
    m_pTheme->m_bitmaps[rData.m_id] = GenericBitmapPtr( pBmp );

    GenericFont *pFont = new BitmapFont( getIntf(), *pBmp, rData.m_type );
    if( pFont->init() )
    {
        m_pTheme->m_fonts[rData.m_id] = GenericFontPtr( pFont );
    }
    else
    {
        delete pFont;
    }
}


void Builder::addFont( const BuilderData::Font &rData )
{
    GenericFont *pFont = new FT2Font( getIntf(), rData.m_fontFile,
                                      rData.m_size );
    if( pFont->init() )
    {
        m_pTheme->m_fonts[rData.m_id] = GenericFontPtr( pFont );
    }
    else
    {
        delete pFont;
    }
}


void Builder::addWindow( const BuilderData::Window &rData )
{
    TopWindow *pWin =
        new TopWindow( getIntf(), rData.m_xPos, rData.m_yPos,
                           m_pTheme->getWindowManager(),
                           rData.m_dragDrop, rData.m_playOnDrop );

    m_pTheme->m_windows[rData.m_id] = TopWindowPtr( pWin );
}


void Builder::addLayout( const BuilderData::Layout &rData )
{
    TopWindow *pWin = m_pTheme->getWindowById(rData.m_windowId);
    if( pWin == NULL )
    {
        msg_Err( getIntf(), "unknown window id: %s", rData.m_windowId.c_str() );
        return;
    }

    int minWidth = rData.m_minWidth != -1 ? rData.m_minWidth : rData.m_width;
    int maxWidth = rData.m_maxWidth != -1 ? rData.m_maxWidth : rData.m_width;
    int minHeight = rData.m_minHeight != -1 ? rData.m_minHeight :
                    rData.m_height;
    int maxHeight = rData.m_maxHeight != -1 ? rData.m_maxHeight :
                    rData.m_height;
    GenericLayout *pLayout = new GenericLayout( getIntf(), rData.m_width,
                                                rData.m_height,
                                                minWidth, maxWidth, minHeight,
                                                maxHeight );
    m_pTheme->m_layouts[rData.m_id] = GenericLayoutPtr( pLayout );

    // Attach the layout to its window
    m_pTheme->getWindowManager().addLayout( *pWin, *pLayout );
}


void Builder::addAnchor( const BuilderData::Anchor &rData )
{
    GenericLayout *pLayout = m_pTheme->getLayoutById(rData.m_layoutId);
    if( pLayout == NULL )
    {
        msg_Err( getIntf(), "unknown layout id: %s", rData.m_layoutId.c_str() );
        return;
    }

    Bezier *pCurve = getPoints( rData.m_points.c_str() );
    if( pCurve == NULL )
    {
        msg_Err( getIntf(), "Invalid format in tag points=\"%s\"",
                 rData.m_points.c_str() );
        return;
    }
    m_pTheme->m_curves.push_back( BezierPtr( pCurve ) );

    Anchor *pAnc = new Anchor( getIntf(), rData.m_xPos, rData.m_yPos,
                               rData.m_range, rData.m_priority,
                               *pCurve, *pLayout );
    pLayout->addAnchor( pAnc );
}


void Builder::addButton( const BuilderData::Button &rData )
{
    // Get the bitmaps of the button
    GenericBitmap *pBmpUp = NULL;
    GET_BMP( pBmpUp, rData.m_upId );

    GenericBitmap *pBmpDown = pBmpUp;
    GET_BMP( pBmpDown, rData.m_downId );

    GenericBitmap *pBmpOver = pBmpUp;
    GET_BMP( pBmpOver, rData.m_overId );

    GenericLayout *pLayout = m_pTheme->getLayoutById(rData.m_layoutId);
    if( pLayout == NULL )
    {
        msg_Err( getIntf(), "unknown layout id: %s", rData.m_layoutId.c_str() );
        return;
    }

    CmdGeneric *pCommand = parseAction( rData.m_actionId );
    if( pCommand == NULL )
    {
        msg_Err( getIntf(), "Invalid action: %s", rData.m_actionId.c_str() );
        return;
    }

    // Get the visibility variable
    // XXX check when it is null
    Interpreter *pInterpreter = Interpreter::instance( getIntf() );
    VarBool *pVisible = pInterpreter->getVarBool( rData.m_visible, m_pTheme );

    CtrlButton *pButton = new CtrlButton( getIntf(), *pBmpUp, *pBmpOver,
        *pBmpDown, *pCommand, UString( getIntf(), rData.m_tooltip.c_str() ),
        UString( getIntf(), rData.m_help.c_str() ), pVisible );

    // Compute the position of the control
    // XXX (we suppose all the images have the same size...)
    const Position pos = makePosition( rData.m_leftTop, rData.m_rightBottom,
                                       rData.m_xPos, rData.m_yPos,
                                       pBmpUp->getWidth(),
                                       pBmpUp->getHeight(), *pLayout );

    pLayout->addControl( pButton, pos, rData.m_layer );

    m_pTheme->m_controls[rData.m_id] = CtrlGenericPtr( pButton );
}


void Builder::addCheckbox( const BuilderData::Checkbox &rData )
{
    // Get the bitmaps of the checkbox
    GenericBitmap *pBmpUp1 = NULL;
    GET_BMP( pBmpUp1, rData.m_up1Id );

    GenericBitmap *pBmpDown1 = pBmpUp1;
    GET_BMP( pBmpDown1, rData.m_down1Id );

    GenericBitmap *pBmpOver1 = pBmpUp1;
    GET_BMP( pBmpOver1, rData.m_over1Id );

    GenericBitmap *pBmpUp2 = NULL;
    GET_BMP( pBmpUp2, rData.m_up2Id );

    GenericBitmap *pBmpDown2 = pBmpUp2;
    GET_BMP( pBmpDown2, rData.m_down2Id );

    GenericBitmap *pBmpOver2 = pBmpUp2;
    GET_BMP( pBmpOver2, rData.m_over2Id );

    GenericLayout *pLayout = m_pTheme->getLayoutById(rData.m_layoutId);
    if( pLayout == NULL )
    {
        msg_Err( getIntf(), "unknown layout id: %s", rData.m_layoutId.c_str() );
        return;
    }

    CmdGeneric *pCommand1 = parseAction( rData.m_action1 );
    if( pCommand1 == NULL )
    {
        msg_Err( getIntf(), "Invalid action: %s", rData.m_action1.c_str() );
        return;
    }

    CmdGeneric *pCommand2 = parseAction( rData.m_action2 );
    if( pCommand2 == NULL )
    {
        msg_Err( getIntf(), "Invalid action: %s", rData.m_action2.c_str() );
        return;
    }

    // Get the state variable
    Interpreter *pInterpreter = Interpreter::instance( getIntf() );
    VarBool *pVar = pInterpreter->getVarBool( rData.m_state, m_pTheme );
    if( pVar == NULL )
    {
        // TODO: default state
        return;
    }

    // Get the visibility variable
    // XXX check when it is null
    VarBool *pVisible = pInterpreter->getVarBool( rData.m_visible, m_pTheme );

    // Create the control
    CtrlCheckbox *pCheckbox = new CtrlCheckbox( getIntf(), *pBmpUp1,
        *pBmpOver1, *pBmpDown1, *pBmpUp2, *pBmpOver2, *pBmpDown2, *pCommand1,
        *pCommand2, UString( getIntf(), rData.m_tooltip1.c_str() ),
        UString( getIntf(), rData.m_tooltip2.c_str() ), *pVar,
        UString( getIntf(), rData.m_help.c_str() ), pVisible );

    // Compute the position of the control
    // XXX (we suppose all the images have the same size...)
    const Position pos = makePosition( rData.m_leftTop, rData.m_rightBottom,
                                       rData.m_xPos, rData.m_yPos,
                                       pBmpUp1->getWidth(),
                                       pBmpUp1->getHeight(), *pLayout );

    pLayout->addControl( pCheckbox, pos, rData.m_layer );

    m_pTheme->m_controls[rData.m_id] = CtrlGenericPtr( pCheckbox );
}


void Builder::addImage( const BuilderData::Image &rData )
{
    GenericBitmap *pBmp = NULL;
    GET_BMP( pBmp, rData.m_bmpId );

    GenericLayout *pLayout = m_pTheme->getLayoutById(rData.m_layoutId);
    if( pLayout == NULL )
    {
        msg_Err( getIntf(), "unknown layout id: %s", rData.m_layoutId.c_str() );
        return;
    }

    TopWindow *pWindow = m_pTheme->getWindowById(rData.m_windowId);
    if( pWindow == NULL )
    {
        msg_Err( getIntf(), "unknown window id: %s", rData.m_windowId.c_str() );
        return;
    }

    // Get the visibility variable
    // XXX check when it is null
    Interpreter *pInterpreter = Interpreter::instance( getIntf() );
    VarBool *pVisible = pInterpreter->getVarBool( rData.m_visible, m_pTheme );

    CtrlImage *pImage = new CtrlImage( getIntf(), *pBmp,
        UString( getIntf(), rData.m_help.c_str() ), pVisible );

    // Compute the position of the control
    const Position pos = makePosition( rData.m_leftTop, rData.m_rightBottom,
                                       rData.m_xPos,
                                       rData.m_yPos, pBmp->getWidth(),
                                       pBmp->getHeight(), *pLayout );

    // XXX: test to be changed! XXX
    if( rData.m_actionId == "move" )
    {
        CtrlMove *pMove = new CtrlMove( getIntf(), m_pTheme->getWindowManager(),
             *pImage, *pWindow, UString( getIntf(), rData.m_help.c_str() ),
             NULL);
        pLayout->addControl( pMove, pos, rData.m_layer );
    }
    else if( rData.m_actionId == "resizeSE" )
    {
        CtrlResize *pResize = new CtrlResize( getIntf(), *pImage, *pLayout,
                UString( getIntf(), rData.m_help.c_str() ), NULL );
        pLayout->addControl( pResize, pos, rData.m_layer );
    }
    else
    {
        pLayout->addControl( pImage, pos, rData.m_layer );
    }

    m_pTheme->m_controls[rData.m_id] = CtrlGenericPtr( pImage );
}


void Builder::addText( const BuilderData::Text &rData )
{
    GenericLayout *pLayout = m_pTheme->getLayoutById(rData.m_layoutId);
    if( pLayout == NULL )
    {
        msg_Err( getIntf(), "unknown layout id: %s", rData.m_layoutId.c_str() );
        return;
    }

    GenericFont *pFont = getFont( rData.m_fontId );
    if( pFont == NULL )
    {
        msg_Err( getIntf(), "Unknown font id: %s", rData.m_fontId.c_str() );
        return;
    }

    // Create a text variable
    VarText *pVar = new VarText( getIntf() );
    UString msg( getIntf(), rData.m_text.c_str() );
    pVar->set( msg );
    m_pTheme->m_vars.push_back( VariablePtr( pVar ) );

    // Get the visibility variable
    // XXX check when it is null
    Interpreter *pInterpreter = Interpreter::instance( getIntf() );
    VarBool *pVisible = pInterpreter->getVarBool( rData.m_visible, m_pTheme );

    CtrlText *pText = new CtrlText( getIntf(), *pVar, *pFont,
        UString( getIntf(), rData.m_help.c_str() ), rData.m_color, pVisible );

    int height = pFont->getSize();

    // Compute the position of the control
    const Position pos = makePosition( rData.m_leftTop, rData.m_rightBottom,
                                       rData.m_xPos, rData.m_yPos,
                                       rData.m_width, height,
                                       *pLayout );

    pLayout->addControl( pText, pos, rData.m_layer );

    m_pTheme->m_controls[rData.m_id] = CtrlGenericPtr( pText );
}


void Builder::addRadialSlider( const BuilderData::RadialSlider &rData )
{
    // Get the bitmaps of the slider
    GenericBitmap *pSeq = NULL;
    GET_BMP( pSeq, rData.m_sequence );

    GenericLayout *pLayout = m_pTheme->getLayoutById(rData.m_layoutId);
    if( pLayout == NULL )
    {
        msg_Err( getIntf(), "unknown layout id: %s", rData.m_layoutId.c_str() );
        return;
    }

    // Get the variable associated to the slider
    Interpreter *pInterpreter = Interpreter::instance( getIntf() );
    VarPercent *pVar = pInterpreter->getVarPercent( rData.m_value, m_pTheme );
    if( pVar == NULL )
    {
        msg_Err( getIntf(), "Unknown slider value: %s", rData.m_value.c_str() );
        return;
    }

    // Get the visibility variable
    // XXX check when it is null
    VarBool *pVisible = pInterpreter->getVarBool( rData.m_visible, m_pTheme );

    // Create the control
    CtrlRadialSlider *pRadial =
        new CtrlRadialSlider( getIntf(), *pSeq, rData.m_nbImages, *pVar,
                              rData.m_minAngle, rData.m_maxAngle,
                              UString( getIntf(), rData.m_help.c_str() ),
                              pVisible );

    // XXX: resizing is not supported
    // Compute the position of the control
    const Position pos =
        makePosition( rData.m_leftTop, rData.m_rightBottom, rData.m_xPos,
                      rData.m_yPos, pSeq->getWidth(),
                      pSeq->getHeight() / rData.m_nbImages, *pLayout );

    pLayout->addControl( pRadial, pos, rData.m_layer );

    m_pTheme->m_controls[rData.m_id] = CtrlGenericPtr( pRadial );
}


void Builder::addSlider( const BuilderData::Slider &rData )
{
    // Get the bitmaps of the slider
    GenericBitmap *pBmpUp = NULL;
    GET_BMP( pBmpUp, rData.m_upId );

    GenericBitmap *pBmpDown = pBmpUp;
    GET_BMP( pBmpDown, rData.m_downId );

    GenericBitmap *pBmpOver = pBmpUp;
    GET_BMP( pBmpOver, rData.m_overId );

    GenericLayout *pLayout = m_pTheme->getLayoutById(rData.m_layoutId);
    if( pLayout == NULL )
    {
        msg_Err( getIntf(), "unknown layout id: %s", rData.m_layoutId.c_str() );
        return;
    }

    Bezier *pCurve = getPoints( rData.m_points.c_str() );
    if( pCurve == NULL )
    {
        msg_Err( getIntf(), "Invalid format in tag points=\"%s\"",
                 rData.m_points.c_str() );
        return;
    }
    m_pTheme->m_curves.push_back( BezierPtr( pCurve ) );

    // Get the visibility variable
    // XXX check when it is null
    Interpreter *pInterpreter = Interpreter::instance( getIntf() );
    VarBool *pVisible = pInterpreter->getVarBool( rData.m_visible, m_pTheme );

    // Get the variable associated to the slider
    VarPercent *pVar = pInterpreter->getVarPercent( rData.m_value, m_pTheme );
    if( pVar == NULL )
    {
        msg_Err( getIntf(), "Unknown slider value: %s", rData.m_value.c_str() );
        return;
    }

    // Create the cursor and background controls
    CtrlSliderCursor *pCursor = new CtrlSliderCursor( getIntf(), *pBmpUp,
        *pBmpOver, *pBmpDown, *pCurve, *pVar, pVisible,
        UString( getIntf(), rData.m_tooltip.c_str() ),
        UString( getIntf(), rData.m_help.c_str() ) );

    CtrlSliderBg *pBackground = new CtrlSliderBg( getIntf(), *pCursor,
        *pCurve, *pVar, rData.m_thickness, pVisible,
        UString( getIntf(), rData.m_help.c_str() ) );

    // Compute the position of the control
    const Position pos = makePosition( rData.m_leftTop, rData.m_rightBottom,
                                       rData.m_xPos, rData.m_yPos,
                                       pCurve->getWidth(), pCurve->getHeight(),
                                       *pLayout );

    pLayout->addControl( pBackground, pos, rData.m_layer );
    pLayout->addControl( pCursor, pos, rData.m_layer );

    m_pTheme->m_controls[rData.m_id] = CtrlGenericPtr( pCursor );
    m_pTheme->m_controls[rData.m_id + "_bg"] = CtrlGenericPtr( pBackground );
}


void Builder::addList( const BuilderData::List &rData )
{
    GenericLayout *pLayout = m_pTheme->getLayoutById(rData.m_layoutId);
    if( pLayout == NULL )
    {
        msg_Err( getIntf(), "unknown layout id: %s", rData.m_layoutId.c_str() );
        return;
    }

    GenericFont *pFont = getFont( rData.m_fontId );
    if( pFont == NULL )
    {
        msg_Err( getIntf(), "Unknown font id: %s", rData.m_fontId.c_str() );
        return;
    }

    // Get the list variable
    Interpreter *pInterpreter = Interpreter::instance( getIntf() );
    VarList *pVar = pInterpreter->getVarList( rData.m_var, m_pTheme );
    if( pVar == NULL )
    {
        msg_Err( getIntf(), "No such list variable: %s", rData.m_var.c_str() );
        return;
    }

    // Get the visibility variable
    // XXX check when it is null
    VarBool *pVisible = pInterpreter->getVarBool( rData.m_visible, m_pTheme );

    // Create the list control
    CtrlList *pList = new CtrlList( getIntf(), *pVar, *pFont,
       rData.m_fgColor, rData.m_playColor, rData.m_bgColor1,
       rData.m_bgColor2, rData.m_selColor,
       UString( getIntf(), rData.m_help.c_str() ), pVisible );

    // Compute the position of the control
    const Position pos = makePosition( rData.m_leftTop, rData.m_rightBottom,
                                       rData.m_xPos, rData.m_yPos,
                                       rData.m_width, rData.m_height,
                                       *pLayout );

    pLayout->addControl( pList, pos, rData.m_layer );

    m_pTheme->m_controls[rData.m_id] = CtrlGenericPtr( pList );
}


void Builder::addVideo( const BuilderData::Video &rData )
{
    GenericLayout *pLayout = m_pTheme->getLayoutById(rData.m_layoutId);
    if( pLayout == NULL )
    {
        msg_Err( getIntf(), "unknown layout id: %s", rData.m_layoutId.c_str() );
        return;
    }

    // Get the visibility variable
    // XXX check when it is null
    Interpreter *pInterpreter = Interpreter::instance( getIntf() );
    VarBool *pVisible = pInterpreter->getVarBool( rData.m_visible, m_pTheme );

    CtrlVideo *pVideo = new CtrlVideo( getIntf(),
        UString( getIntf(), rData.m_help.c_str() ), pVisible );

    // Compute the position of the control
    const Position pos = makePosition( rData.m_leftTop, rData.m_rightBottom,
                                       rData.m_xPos, rData.m_yPos,
                                       rData.m_width, rData.m_height,
                                       *pLayout );

    pLayout->addControl( pVideo, pos, rData.m_layer );

    m_pTheme->m_controls[rData.m_id] = CtrlGenericPtr( pVideo );
}


const Position Builder::makePosition( const string &rLeftTop,
                                      const string &rRightBottom,
                                      int xPos, int yPos, int width,
                                      int height, const Box &rBox ) const
{
    int left = 0, top = 0, right = 0, bottom = 0;
    Position::Ref_t refLeftTop = Position::kLeftTop;
    Position::Ref_t refRightBottom = Position::kLeftTop;

    int boxWidth = rBox.getWidth();
    int boxHeight = rBox.getHeight();

    // Position of the left top corner
    if( rLeftTop == "lefttop" )
    {
        left = xPos;
        top = yPos;
        refLeftTop = Position::kLeftTop;
    }
    else if( rLeftTop == "righttop" )
    {
        left = xPos - boxWidth + 1;
        top = yPos;
        refLeftTop = Position::kRightTop;
    }
    else if( rLeftTop == "leftbottom" )
    {
        left = xPos;
        top = yPos - boxHeight + 1;
        refLeftTop = Position::kLeftBottom;
    }
    else if( rLeftTop == "rightbottom" )
    {
        left = xPos - boxWidth + 1;
        top = yPos - boxHeight + 1;
        refLeftTop = Position::kRightBottom;
    }

    // Position of the right bottom corner
    if( rRightBottom == "lefttop" )
    {
        right = xPos + width - 1;
        bottom = yPos + height - 1;
        refRightBottom = Position::kLeftTop;
    }
    else if( rRightBottom == "righttop" )
    {
        right = xPos + width - boxWidth;
        bottom = yPos + height - 1;
        refRightBottom = Position::kRightTop;
    }
    else if( rRightBottom == "leftbottom" )
    {
        right = xPos + width - 1;
        bottom = yPos + height - boxHeight;
        refRightBottom = Position::kLeftBottom;
    }
    else if( rRightBottom == "rightbottom" )
    {
        right = xPos + width - boxWidth;
        bottom = yPos + height - boxHeight;
        refRightBottom = Position::kRightBottom;
    }

    return Position( left, top, right, bottom, rBox, refLeftTop,
                     refRightBottom );
}


GenericFont *Builder::getFont( const string &fontId )
{
    GenericFont *pFont = m_pTheme->getFontById(fontId);
    if( !pFont && fontId == "defaultfont" )
    {
        // Get the resource path and try to load the default font
        OSFactory *pOSFactory = OSFactory::instance( getIntf() );
        const list<string> &resPath = pOSFactory->getResourcePath();
        const string &sep = pOSFactory->getDirSeparator();

        list<string>::const_iterator it;
        for( it = resPath.begin(); it != resPath.end(); it++ )
        {
            string path = (*it) + sep + "fonts" + sep + "FreeSans.ttf";
            pFont = new FT2Font( getIntf(), path, 12 );
            if( pFont->init() )
            {
                // Font loaded successfully
                m_pTheme->m_fonts["defaultfont"] = GenericFontPtr( pFont );
                break;
            }
            else
            {
                delete pFont;
                pFont = NULL;
            }
        }
        if( !pFont )
        {
            msg_Err( getIntf(), "Failed to open the default font" );
        }
    }
    return pFont;
}


Bezier *Builder::getPoints( const char *pTag ) const
{
    vector<float> xBez, yBez;
    int x, y, n;
    while( 1 )
    {
        if( sscanf( pTag, "(%d,%d)%n", &x, &y, &n ) < 1 )
        {
            return NULL;
        }
#if 0
        if( x < 0 || y < 0 )
        {
            msg_Err( getIntf(),
                     "Slider points cannot have negative coordinates!" );
            return NULL;
        }
#endif
        xBez.push_back( x );
        yBez.push_back( y );
        pTag += n;
        if( *pTag == '\0' )
        {
            break;
        }
        if( *(pTag++) != ',' )
        {
            return NULL;
        }
    }

    // Create the Bezier curve
    return new Bezier( getIntf(), xBez, yBez );
}

