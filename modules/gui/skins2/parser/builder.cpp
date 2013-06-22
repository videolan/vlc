/*****************************************************************************
 * builder.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "builder.hpp"
#include "builder_data.hpp"
#include "interpreter.hpp"
#include "skin_parser.hpp"
#include "../src/file_bitmap.hpp"
#include "../src/os_factory.hpp"
#include "../src/generic_bitmap.hpp"
#include "../src/top_window.hpp"
#include "../src/fsc_window.hpp"
#include "../src/anchor.hpp"
#include "../src/bitmap_font.hpp"
#include "../src/ft2_font.hpp"
#include "../src/ini_file.hpp"
#include "../src/generic_layout.hpp"
#include "../src/popup.hpp"
#include "../src/theme.hpp"
#include "../src/window_manager.hpp"
#include "../src/vout_manager.hpp"
#include "../commands/cmd_generic.hpp"
#include "../controls/ctrl_button.hpp"
#include "../controls/ctrl_checkbox.hpp"
#include "../controls/ctrl_image.hpp"
#include "../controls/ctrl_list.hpp"
#include "../controls/ctrl_move.hpp"
#include "../controls/ctrl_resize.hpp"
#include "../controls/ctrl_slider.hpp"
#include "../controls/ctrl_radialslider.hpp"
#include "../controls/ctrl_text.hpp"
#include "../controls/ctrl_tree.hpp"
#include "../controls/ctrl_video.hpp"
#include "../utils/bezier.hpp"
#include "../utils/position.hpp"
#include "../utils/var_bool.hpp"
#include "../utils/var_text.hpp"

#include <vlc_image.h>
#include <fstream>


Builder::Builder( intf_thread_t *pIntf, const BuilderData &rData,
                  const string &rPath ):
    SkinObject( pIntf ), m_rData( rData ), m_path( rPath ), m_pTheme( NULL )
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

template<class T> inline
void Builder::add_objects(const std::list<T> &list,
                          void (Builder::*addfn)(const T &))
{
    typename std::list<T>::const_iterator i;
    for( i = list.begin(); i != list.end(); ++i )
        (this->*addfn)( *i );
}

Theme *Builder::build()
{
#define ADD_OBJECTS( type ) \
    add_objects(m_rData.m_list##type,&Builder::add##type)

    m_pTheme = new (std::nothrow) Theme( getIntf() );
    if( m_pTheme == NULL )
        return NULL;

    // Create everything from the data in the XML
    ADD_OBJECTS( Theme );
    ADD_OBJECTS( IniFile );
    ADD_OBJECTS( Bitmap );
    ADD_OBJECTS( SubBitmap );
    ADD_OBJECTS( BitmapFont );
    ADD_OBJECTS( Font );
    ADD_OBJECTS( Window );
    // XXX: PopupMenus are created after the windows, so that the Win32Factory
    // (at least) can give a valid window handle to the OSPopup objects
    ADD_OBJECTS( PopupMenu );
    ADD_OBJECTS( Layout );
    ADD_OBJECTS( Panel );
    ADD_OBJECTS( Anchor );
    ADD_OBJECTS( Button );
    ADD_OBJECTS( Checkbox );
    ADD_OBJECTS( Image );
    ADD_OBJECTS( Text );
    ADD_OBJECTS( RadialSlider );
    ADD_OBJECTS( Slider );
    ADD_OBJECTS( List );
    ADD_OBJECTS( Tree );
    ADD_OBJECTS( Video );
    // MenuItems must be created after all the rest, so that the IDs of the
    // other elements exist and can be parsed in the actions
    ADD_OBJECTS( MenuItem );
    ADD_OBJECTS( MenuSeparator );

    return m_pTheme;

#undef  ADD_OBJECTS
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

// macro to check bitmap size consistency for button and checkbox
#define CHECK_BMP( pBmp, pBmpRef, id) \
    if( pBmp != pBmpRef ) \
    { \
        int w_ref = pBmpRef->getWidth(); \
        int h_ref = pBmpRef->getHeight() / pBmpRef->getNbFrames(); \
        int w = pBmp->getWidth(); \
        int h = pBmp->getHeight() / pBmp->getNbFrames(); \
        if( w != w_ref || h != h_ref ) \
            msg_Err( getIntf(), "pls, check bitmap sizes for id: %s", id.c_str() ); \
    }

// macro to check resize policy of button and checkbox
#define CHECK_RESIZE_POLICY( lefttop, rightbottom, xkeepratio, ykeepratio, id )\
    if( (!xkeepratio && lefttop != rightbottom) || \
        (!ykeepratio && lefttop != rightbottom) ) \
    { \
        msg_Err( getIntf(), "pls, check resize policy for id: %s", \
                            id.c_str() ); \
        rightbottom = lefttop; \
    }

// Macro to get the parent box of a control, given the panel ID
#define GET_BOX( pRect, id, pLayout ) \
    if( id == "none" ) \
        pRect = &pLayout->getRect(); \
    else \
    { \
        const Position *pParent = \
            m_pTheme->getPositionById( rData.m_panelId ); \
        if( pParent == NULL ) \
        { \
            msg_Err( getIntf(), "parent panel could not be found: %s", \
                     rData.m_panelId.c_str() ); \
            return; \
        } \
        pRect = pParent; \
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
        msg_Warn( getIntf(), "invalid tooltip font: %s",
                  rData.m_tooltipfont.c_str() );
    }
}


void Builder::addIniFile( const BuilderData::IniFile &rData )
{
    // Parse the INI file
    string full_path = getFilePath( rData.m_file );
    if( !full_path.size() )
        return;

    IniFile iniFile( getIntf(), rData.m_id, full_path );
    iniFile.parseFile();
}


void Builder::addBitmap( const BuilderData::Bitmap &rData )
{
    string full_path = getFilePath( rData.m_fileName );
    if( !full_path.size() )
        return;

    GenericBitmap *pBmp =
        new FileBitmap( getIntf(), m_pImageHandler,
                        full_path, rData.m_alphaColor,
                        rData.m_nbFrames, rData.m_fps, rData.m_nbLoops );
    if( !pBmp->getData() )
    {
        // Invalid bitmap
        delete pBmp;
        return;
    }
    m_pTheme->m_bitmaps[rData.m_id] = GenericBitmapPtr( pBmp );
}


void Builder::addSubBitmap( const BuilderData::SubBitmap &rData )
{
    if( m_pTheme->m_bitmaps.find( rData.m_id ) != m_pTheme->m_bitmaps.end() )
    {
        msg_Dbg( getIntf(), "bitmap %s already exists", rData.m_id.c_str() );
        return;
    }

    // Get the parent bitmap
    GenericBitmap *pParentBmp = NULL;
    GET_BMP( pParentBmp, rData.m_parent );

    // Copy a region of the parent bitmap to the new one
    BitmapImpl *pBmp =
        new BitmapImpl( getIntf(), rData.m_width, rData.m_height,
                        rData.m_nbFrames, rData.m_fps, rData.m_nbLoops );
    bool res = pBmp->drawBitmap( *pParentBmp, rData.m_x, rData.m_y, 0, 0,
                                 rData.m_width, rData.m_height );
    if( !res )
    {
        // Invalid sub-bitmap
        delete pBmp;
        msg_Warn( getIntf(), "sub-bitmap %s ignored", rData.m_id.c_str() );
        return;
    }
    m_pTheme->m_bitmaps[rData.m_id] = GenericBitmapPtr( pBmp );
}


void Builder::addBitmapFont( const BuilderData::BitmapFont &rData )
{
    if( m_pTheme->m_fonts.find( rData.m_id ) != m_pTheme->m_fonts.end() )
    {
        msg_Dbg( getIntf(), "font %s already exists", rData.m_id.c_str() );
        return;
    }

    string full_path = getFilePath( rData.m_file );
    if( !full_path.size() )
        return;

    GenericBitmap *pBmp =
        new FileBitmap( getIntf(), m_pImageHandler, full_path, 0 );
    if( !pBmp->getData() )
    {
        // Invalid bitmap
        delete pBmp;
        return;
    }

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
    // Try to load the font from the theme directory
    string full_path = getFilePath( rData.m_fontFile );
    if( full_path.size() )
    {
        GenericFont *pFont = new FT2Font( getIntf(), full_path, rData.m_size );
        if( pFont->init() )
        {
            m_pTheme->m_fonts[rData.m_id] = GenericFontPtr( pFont );
            return;
        }
        delete pFont;
    }

    // Font not found; try in the resource path
    OSFactory *pOSFactory = OSFactory::instance( getIntf() );
    const list<string> &resPath = pOSFactory->getResourcePath();
    const string &sep = pOSFactory->getDirSeparator();

    list<string>::const_iterator it;
    for( it = resPath.begin(); it != resPath.end(); ++it )
    {
        string path = (*it) + sep + "fonts" + sep + rData.m_fontFile;
        GenericFont *pFont = new FT2Font( getIntf(), path, rData.m_size );
        if( pFont->init() )
        {
            // Font loaded successfully
            m_pTheme->m_fonts[rData.m_id] = GenericFontPtr( pFont );
            return;
        }
        delete pFont;
    }
}


void Builder::addPopupMenu( const BuilderData::PopupMenu &rData )
{
    Popup *pPopup = new Popup( getIntf(), m_pTheme->getWindowManager() );

    m_pTheme->m_popups[rData.m_id] = PopupPtr( pPopup );
}


void Builder::addMenuItem( const BuilderData::MenuItem &rData )
{
    Popup *pPopup = m_pTheme->getPopupById( rData.m_popupId );
    if( pPopup == NULL )
    {
        msg_Err( getIntf(), "unknown popup id: %s", rData.m_popupId.c_str() );
        return;
    }

    CmdGeneric *pCommand = parseAction( rData.m_action );
    if( pCommand == NULL )
    {
        msg_Err( getIntf(), "invalid action: %s", rData.m_action.c_str() );
        return;
    }

    pPopup->addItem( rData.m_label, *pCommand, rData.m_pos );
}


void Builder::addMenuSeparator( const BuilderData::MenuSeparator &rData )
{
    Popup *pPopup = m_pTheme->getPopupById( rData.m_popupId );
    if( pPopup == NULL )
    {
        msg_Err( getIntf(), "unknown popup id: %s", rData.m_popupId.c_str() );
        return;
    }

    pPopup->addSeparator( rData.m_pos );
}


void Builder::addWindow( const BuilderData::Window &rData )
{
    TopWindow *pWin;
    if( rData.m_id == "fullscreenController" )
    {
        pWin = new FscWindow( getIntf(), rData.m_xPos, rData.m_yPos,
                       m_pTheme->getWindowManager(),
                       rData.m_dragDrop, rData.m_playOnDrop,
                       rData.m_visible );
    }
    else
    {
        pWin = new TopWindow( getIntf(), rData.m_xPos, rData.m_yPos,
                       m_pTheme->getWindowManager(),
                       rData.m_dragDrop, rData.m_playOnDrop,
                       rData.m_visible );
    }
    m_pTheme->m_windows[rData.m_id] = TopWindowPtr( pWin );
}


void Builder::addLayout( const BuilderData::Layout &rData )
{
    TopWindow *pWin = m_pTheme->getWindowById( rData.m_windowId );
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
    GenericLayout *pLayout = m_pTheme->getLayoutById( rData.m_layoutId );
    if( pLayout == NULL )
    {
        msg_Err( getIntf(), "unknown layout id: %s", rData.m_layoutId.c_str() );
        return;
    }

    Bezier *pCurve = getPoints( rData.m_points.c_str() );
    if( pCurve == NULL )
    {
        msg_Err( getIntf(), "invalid format in tag points=\"%s\"",
                 rData.m_points.c_str() );
        return;
    }
    m_pTheme->m_curves.push_back( BezierPtr( pCurve ) );

    // Compute the position of the anchor
    const Position pos = makePosition( rData.m_leftTop, rData.m_leftTop,
                                       rData.m_xPos, rData.m_yPos,
                                       pCurve->getWidth(),
                                       pCurve->getHeight(),
                                       pLayout->getRect() );

    Anchor *pAnc = new Anchor( getIntf(), pos, rData.m_range, rData.m_priority,
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

    GenericLayout *pLayout = m_pTheme->getLayoutById( rData.m_layoutId );
    if( pLayout == NULL )
    {
        msg_Err( getIntf(), "unknown layout id: %s", rData.m_layoutId.c_str() );
        return;
    }

    CmdGeneric *pCommand = parseAction( rData.m_actionId );
    if( pCommand == NULL )
    {
        msg_Err( getIntf(), "invalid action: %s", rData.m_actionId.c_str() );
        return;
    }

    // Get the visibility variable
    // XXX check when it is null
    Interpreter *pInterpreter = Interpreter::instance( getIntf() );
    VarBool *pVisible = pInterpreter->getVarBool( rData.m_visible, m_pTheme );

    CtrlButton *pButton = new CtrlButton( getIntf(), *pBmpUp, *pBmpOver,
        *pBmpDown, *pCommand, UString( getIntf(), rData.m_tooltip.c_str() ),
        UString( getIntf(), rData.m_help.c_str() ), pVisible );
    m_pTheme->m_controls[rData.m_id] = CtrlGenericPtr( pButton );

    // width and height are set up from the 'up' bitmap size
    int width = pBmpUp->getWidth();
    int height = pBmpUp->getHeight() / pBmpUp->getNbFrames();
    bool xkeepratio = rData.m_xKeepRatio;
    bool ykeepratio = rData.m_yKeepRatio;
    string lefttop( rData.m_leftTop );
    string rightbottom( rData.m_rightBottom );

    // various checks to help skin developers debug their skin
    CHECK_BMP( pBmpDown, pBmpUp, rData.m_id );
    CHECK_BMP( pBmpOver, pBmpUp, rData.m_id );
    CHECK_RESIZE_POLICY( lefttop, rightbottom, xkeepratio, ykeepratio,
                         rData.m_id );

    // Compute the position of the control
    // XXX (we suppose all the images have the same size...)
    const GenericRect *pRect;
    GET_BOX( pRect, rData.m_panelId , pLayout);
    const Position pos = makePosition( lefttop, rightbottom,
                             rData.m_xPos, rData.m_yPos, width, height,
                             *pRect, xkeepratio, ykeepratio );

    pLayout->addControl( pButton, pos, rData.m_layer );
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

    GenericLayout *pLayout = m_pTheme->getLayoutById( rData.m_layoutId );
    if( pLayout == NULL )
    {
        msg_Err( getIntf(), "unknown layout id: %s", rData.m_layoutId.c_str() );
        return;
    }

    CmdGeneric *pCommand1 = parseAction( rData.m_action1 );
    if( pCommand1 == NULL )
    {
        msg_Err( getIntf(), "invalid action: %s", rData.m_action1.c_str() );
        return;
    }

    CmdGeneric *pCommand2 = parseAction( rData.m_action2 );
    if( pCommand2 == NULL )
    {
        msg_Err( getIntf(), "invalid action: %s", rData.m_action2.c_str() );
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

    // width and height are set up from the 'up1' bitmap size
    int width = pBmpUp1->getWidth();
    int height = pBmpUp1->getHeight() / pBmpUp1->getNbFrames();
    bool xkeepratio = rData.m_xKeepRatio;
    bool ykeepratio = rData.m_yKeepRatio;
    string lefttop( rData.m_leftTop );
    string rightbottom( rData.m_rightBottom );


    // various checks to help skin developers debug their skin
    CHECK_BMP( pBmpDown1, pBmpUp1, rData.m_id );
    CHECK_BMP( pBmpOver1, pBmpUp1, rData.m_id );
    CHECK_BMP( pBmpUp2, pBmpUp1, rData.m_id );
    CHECK_BMP( pBmpDown2, pBmpUp1, rData.m_id );
    CHECK_BMP( pBmpOver2, pBmpUp1, rData.m_id );
    CHECK_RESIZE_POLICY( lefttop, rightbottom, xkeepratio, ykeepratio,
                         rData.m_id );

    // Create the control
    CtrlCheckbox *pCheckbox = new CtrlCheckbox( getIntf(), *pBmpUp1,
        *pBmpOver1, *pBmpDown1, *pBmpUp2, *pBmpOver2, *pBmpDown2, *pCommand1,
        *pCommand2, UString( getIntf(), rData.m_tooltip1.c_str() ),
        UString( getIntf(), rData.m_tooltip2.c_str() ), *pVar,
        UString( getIntf(), rData.m_help.c_str() ), pVisible );
    m_pTheme->m_controls[rData.m_id] = CtrlGenericPtr( pCheckbox );

    // Compute the position of the control
    // XXX (we suppose all the images have the same size...)
    const GenericRect *pRect;
    GET_BOX( pRect, rData.m_panelId , pLayout);
    const Position pos = makePosition( lefttop, rightbottom,
                             rData.m_xPos, rData.m_yPos, width, height,
                             *pRect, xkeepratio, ykeepratio );

    pLayout->addControl( pCheckbox, pos, rData.m_layer );
}


void Builder::addImage( const BuilderData::Image &rData )
{
    GenericBitmap *pBmp = NULL;
    GET_BMP( pBmp, rData.m_bmpId );

    GenericLayout *pLayout = m_pTheme->getLayoutById( rData.m_layoutId );
    if( pLayout == NULL )
    {
        msg_Err( getIntf(), "unknown layout id: %s", rData.m_layoutId.c_str() );
        return;
    }

    TopWindow *pWindow = m_pTheme->getWindowById( rData.m_windowId );
    if( pWindow == NULL )
    {
        msg_Err( getIntf(), "unknown window id: %s", rData.m_windowId.c_str() );
        return;
    }

    CmdGeneric *pCommand = parseAction( rData.m_action2Id );
    if( pCommand == NULL )
    {
        msg_Err( getIntf(), "invalid action: %s", rData.m_action2Id.c_str() );
        return;
    }

    // Get the visibility variable
    // XXX check when it is null
    Interpreter *pInterpreter = Interpreter::instance( getIntf() );
    VarBool *pVisible = pInterpreter->getVarBool( rData.m_visible, m_pTheme );

    CtrlImage::resize_t resizeMethod =
        rData.m_resize == "scale"  ? CtrlImage::kScale :
        rData.m_resize == "mosaic" ? CtrlImage::kMosaic :
                                     CtrlImage::kScaleAndRatioPreserved;
    CtrlImage *pImage = new CtrlImage( getIntf(), *pBmp, *pCommand,
        resizeMethod, UString( getIntf(), rData.m_help.c_str() ), pVisible,
        rData.m_art );
    m_pTheme->m_controls[rData.m_id] = CtrlGenericPtr( pImage );

    // Compute the position of the control
    const GenericRect *pRect;
    int width = (rData.m_width > 0) ? rData.m_width : pBmp->getWidth();
    int height = (rData.m_height > 0) ? rData.m_height : pBmp->getHeight();
    GET_BOX( pRect, rData.m_panelId , pLayout);
    const Position pos = makePosition( rData.m_leftTop, rData.m_rightBottom,
                                       rData.m_xPos, rData.m_yPos,
                                       width, height,
                                       *pRect, rData.m_xKeepRatio,
                                       rData.m_yKeepRatio );

    if( rData.m_actionId == "move" )
    {
        CtrlMove *pMove = new CtrlMove( getIntf(), m_pTheme->getWindowManager(),
             *pImage, *pWindow, UString( getIntf(), rData.m_help.c_str() ),
             pVisible );
        m_pTheme->m_controls[rData.m_id + "_move"] = CtrlGenericPtr( pMove );
        pLayout->addControl( pMove, pos, rData.m_layer );
    }
    else if( rData.m_actionId == "resizeS" )
    {
        CtrlResize *pResize = new CtrlResize( getIntf(),
                m_pTheme->getWindowManager(), *pImage, *pLayout,
                UString( getIntf(), rData.m_help.c_str() ), pVisible,
                WindowManager::kResizeS );
        m_pTheme->m_controls[rData.m_id + "_rsz"] = CtrlGenericPtr( pResize );
        pLayout->addControl( pResize, pos, rData.m_layer );
    }
    else if( rData.m_actionId == "resizeE" )
    {
        CtrlResize *pResize = new CtrlResize( getIntf(),
                m_pTheme->getWindowManager(), *pImage, *pLayout,
                UString( getIntf(), rData.m_help.c_str() ), pVisible,
                WindowManager::kResizeE );
        m_pTheme->m_controls[rData.m_id + "_rsz"] = CtrlGenericPtr( pResize );
        pLayout->addControl( pResize, pos, rData.m_layer );
    }
    else if( rData.m_actionId == "resizeSE" )
    {
        CtrlResize *pResize = new CtrlResize( getIntf(),
                m_pTheme->getWindowManager(), *pImage, *pLayout,
                UString( getIntf(), rData.m_help.c_str() ), pVisible,
                WindowManager::kResizeSE );
        m_pTheme->m_controls[rData.m_id + "_rsz"] = CtrlGenericPtr( pResize );
        pLayout->addControl( pResize, pos, rData.m_layer );
    }
    else
    {
        pLayout->addControl( pImage, pos, rData.m_layer );
    }
}


void Builder::addPanel( const BuilderData::Panel &rData )
{
    // This method makes the assumption that the Panels are created in the
    // order of the XML, because each child Panel expects its parent Panel
    // in order to be fully created

    GenericLayout *pLayout = m_pTheme->getLayoutById( rData.m_layoutId );
    if( pLayout == NULL )
    {
        msg_Err( getIntf(), "unknown layout id: %s", rData.m_layoutId.c_str() );
        return;
    }

    const GenericRect *pRect;
    GET_BOX( pRect, rData.m_panelId , pLayout);
    Position *pPos =
        new Position( makePosition( rData.m_leftTop, rData.m_rightBottom,
                                    rData.m_xPos, rData.m_yPos,
                                    rData.m_width, rData.m_height,
                                    *pRect, rData.m_xKeepRatio,
                                    rData.m_yKeepRatio ) );
    m_pTheme->m_positions[rData.m_id] = PositionPtr( pPos );
}


void Builder::addText( const BuilderData::Text &rData )
{
    GenericLayout *pLayout = m_pTheme->getLayoutById( rData.m_layoutId );
    if( pLayout == NULL )
    {
        msg_Err( getIntf(), "unknown layout id: %s", rData.m_layoutId.c_str() );
        return;
    }

    GenericFont *pFont = getFont( rData.m_fontId );
    if( pFont == NULL )
    {
        msg_Err( getIntf(), "unknown font id: %s", rData.m_fontId.c_str() );
        return;
    }

    // Convert the scrolling mode
    CtrlText::Scrolling_t scrolling;
    if( rData.m_scrolling == "auto" )
        scrolling = CtrlText::kAutomatic;
    else if( rData.m_scrolling == "manual" )
        scrolling = CtrlText::kManual;
    else if( rData.m_scrolling == "none" )
        scrolling = CtrlText::kNone;
    else
    {
        msg_Err( getIntf(), "invalid scrolling mode: %s",
                 rData.m_scrolling.c_str() );
        return;
    }

    // Convert the alignment
    CtrlText::Align_t alignment;
    if( rData.m_alignment == "left" )
        alignment = CtrlText::kLeft;
    else if( rData.m_alignment == "center" || rData.m_alignment == "centre" )
        alignment = CtrlText::kCenter;
    else if( rData.m_alignment == "right" )
        alignment = CtrlText::kRight;
    else
    {
        msg_Err( getIntf(), "invalid alignment: %s",
                 rData.m_alignment.c_str() );
        return;
    }

    // Create a text variable
    VarText *pVar = new VarText( getIntf() );
    m_pTheme->m_vars.push_back( VariablePtr( pVar ) );

    // Set the text of the control
    UString msg( getIntf(), rData.m_text.c_str() );
    pVar->set( msg );

    // Get the visibility variable
    // XXX check when it is null
    Interpreter *pInterpreter = Interpreter::instance( getIntf() );
    VarBool *pVisible = pInterpreter->getVarBool( rData.m_visible, m_pTheme );
    VarBool *pFocus = pInterpreter->getVarBool( rData.m_focus, m_pTheme );

    CtrlText *pText = new CtrlText( getIntf(), *pVar, *pFont,
        UString( getIntf(), rData.m_help.c_str() ), rData.m_color,
        pVisible, pFocus, scrolling, alignment );
    m_pTheme->m_controls[rData.m_id] = CtrlGenericPtr( pText );

    int height = pFont->getSize();

    // Compute the position of the control
    const GenericRect *pRect;
    GET_BOX( pRect, rData.m_panelId , pLayout);
    const Position pos = makePosition( rData.m_leftTop, rData.m_rightBottom,
                                       rData.m_xPos, rData.m_yPos,
                                       rData.m_width, height, *pRect,
                                       rData.m_xKeepRatio, rData.m_yKeepRatio );

    pLayout->addControl( pText, pos, rData.m_layer );

}


void Builder::addRadialSlider( const BuilderData::RadialSlider &rData )
{
    // Get the bitmaps of the slider
    GenericBitmap *pSeq = NULL;
    GET_BMP( pSeq, rData.m_sequence );

    GenericLayout *pLayout = m_pTheme->getLayoutById( rData.m_layoutId );
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
        msg_Err( getIntf(), "unknown slider value: %s", rData.m_value.c_str() );
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
    m_pTheme->m_controls[rData.m_id] = CtrlGenericPtr( pRadial );

    // XXX: resizing is not supported
    // Compute the position of the control
    const GenericRect *pRect;
    GET_BOX( pRect, rData.m_panelId , pLayout);
    const Position pos = makePosition( rData.m_leftTop, rData.m_rightBottom,
                                       rData.m_xPos, rData.m_yPos,
                                       pSeq->getWidth(),
                                       pSeq->getHeight() / rData.m_nbImages,
                                       *pRect,
                                       rData.m_xKeepRatio, rData.m_yKeepRatio );

    pLayout->addControl( pRadial, pos, rData.m_layer );
}


void Builder::addSlider( const BuilderData::Slider &rData )
{
    // Add the background first, so that we will still have something almost
    // functional if the cursor cannot be created properly (this happens for
    // some winamp2 skins, where the images of the cursor are not always
    // present)

    // Get the bitmaps of the background
    GenericBitmap *pBgImage = NULL;
    GET_BMP( pBgImage, rData.m_imageId );

    GenericLayout *pLayout = m_pTheme->getLayoutById( rData.m_layoutId );
    if( pLayout == NULL )
    {
        msg_Err( getIntf(), "unknown layout id: %s", rData.m_layoutId.c_str() );
        return;
    }

    Bezier *pCurve = getPoints( rData.m_points.c_str() );
    if( pCurve == NULL )
    {
        msg_Err( getIntf(), "invalid format in tag points=\"%s\"",
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
        msg_Err( getIntf(), "unknown slider value: %s", rData.m_value.c_str() );
        return;
    }

    // Create the background control
    CtrlSliderBg *pBackground = new CtrlSliderBg( getIntf(),
        *pCurve, *pVar, rData.m_thickness, pBgImage, rData.m_nbHoriz,
        rData.m_nbVert, rData.m_padHoriz, rData.m_padVert,
        pVisible, UString( getIntf(), rData.m_help.c_str() ) );
    m_pTheme->m_controls[rData.m_id + "_bg"] = CtrlGenericPtr( pBackground );

    // Compute the position of the control
    int width = (rData.m_width > 0) ? rData.m_width : pCurve->getWidth();
    int height = (rData.m_height > 0) ? rData.m_height : pCurve->getHeight();
    const GenericRect *pRect;
    GET_BOX( pRect, rData.m_panelId , pLayout);
    const Position pos = makePosition( rData.m_leftTop, rData.m_rightBottom,
                                       rData.m_xPos, rData.m_yPos,
                                       width, height, *pRect,
                                       rData.m_xKeepRatio, rData.m_yKeepRatio );

    pLayout->addControl( pBackground, pos, rData.m_layer );

    // Get the bitmaps of the cursor
    GenericBitmap *pBmpUp = NULL;
    GET_BMP( pBmpUp, rData.m_upId );

    GenericBitmap *pBmpDown = pBmpUp;
    GET_BMP( pBmpDown, rData.m_downId );

    GenericBitmap *pBmpOver = pBmpUp;
    GET_BMP( pBmpOver, rData.m_overId );

    // Create the cursor control
    CtrlSliderCursor *pCursor = new CtrlSliderCursor( getIntf(), *pBmpUp,
        *pBmpOver, *pBmpDown, *pCurve, *pVar, pVisible,
        UString( getIntf(), rData.m_tooltip.c_str() ),
        UString( getIntf(), rData.m_help.c_str() ) );
    m_pTheme->m_controls[rData.m_id] = CtrlGenericPtr( pCursor );

    pLayout->addControl( pCursor, pos, rData.m_layer );

    // Associate the cursor to the background
    pBackground->associateCursor( *pCursor );
}


void Builder::addList( const BuilderData::List &rData )
{
    // Get the background bitmap, if any
    GenericBitmap *pBgBmp = NULL;
    GET_BMP( pBgBmp, rData.m_bgImageId );

    GenericLayout *pLayout = m_pTheme->getLayoutById( rData.m_layoutId );
    if( pLayout == NULL )
    {
        msg_Err( getIntf(), "unknown layout id: %s", rData.m_layoutId.c_str() );
        return;
    }

    GenericFont *pFont = getFont( rData.m_fontId );
    if( pFont == NULL )
    {
        msg_Err( getIntf(), "unknown font id: %s", rData.m_fontId.c_str() );
        return;
    }

    // Get the list variable
    Interpreter *pInterpreter = Interpreter::instance( getIntf() );
    VarList *pVar = pInterpreter->getVarList( rData.m_var, m_pTheme );
    if( pVar == NULL )
    {
        msg_Err( getIntf(), "no such list variable: %s", rData.m_var.c_str() );
        return;
    }

    // Get the visibility variable
    // XXX check when it is null
    VarBool *pVisible = pInterpreter->getVarBool( rData.m_visible, m_pTheme );

    // Get the color values
    uint32_t fgColor = getColor( rData.m_fgColor );
    uint32_t playColor = getColor( rData.m_playColor );
    uint32_t bgColor1 = getColor( rData.m_bgColor1 );
    uint32_t bgColor2 = getColor( rData.m_bgColor2 );
    uint32_t selColor = getColor( rData.m_selColor );

    // Create the list control
    CtrlList *pList = new CtrlList( getIntf(), *pVar, *pFont, pBgBmp,
       fgColor, playColor, bgColor1, bgColor2, selColor,
       UString( getIntf(), rData.m_help.c_str() ), pVisible );
    m_pTheme->m_controls[rData.m_id] = CtrlGenericPtr( pList );

    // Compute the position of the control
    const GenericRect *pRect;
    GET_BOX( pRect, rData.m_panelId , pLayout);
    const Position pos = makePosition( rData.m_leftTop, rData.m_rightBottom,
                                       rData.m_xPos, rData.m_yPos,
                                       rData.m_width, rData.m_height,
                                       *pRect,
                                       rData.m_xKeepRatio, rData.m_yKeepRatio );

    pLayout->addControl( pList, pos, rData.m_layer );
}

void Builder::addTree( const BuilderData::Tree &rData )
{
    // Get the bitmaps, if any
    GenericBitmap *pBgBmp = NULL;
    GenericBitmap *pItemBmp = NULL;
    GenericBitmap *pOpenBmp = NULL;
    GenericBitmap *pClosedBmp = NULL;
    GET_BMP( pBgBmp, rData.m_bgImageId );
    GET_BMP( pItemBmp, rData.m_itemImageId );
    GET_BMP( pOpenBmp, rData.m_openImageId );
    GET_BMP( pClosedBmp, rData.m_closedImageId );

    GenericLayout *pLayout = m_pTheme->getLayoutById( rData.m_layoutId );
    if( pLayout == NULL )
    {
        msg_Err( getIntf(), "unknown layout id: %s", rData.m_layoutId.c_str() );
        return;
    }

    GenericFont *pFont = getFont( rData.m_fontId );
    if( pFont == NULL )
    {
        msg_Err( getIntf(), "unknown font id: %s", rData.m_fontId.c_str() );
        return;
    }

    // Get the list variable
    Interpreter *pInterpreter = Interpreter::instance( getIntf() );
    VarTree *pVar = pInterpreter->getVarTree( rData.m_var, m_pTheme );
    if( pVar == NULL )
    {
        msg_Err( getIntf(), "no such list variable: %s", rData.m_var.c_str() );
        return;
    }

    // Get the visibility variable
    // XXX check when it is null
    VarBool *pVisible = pInterpreter->getVarBool( rData.m_visible, m_pTheme );
    VarBool *pFlat = pInterpreter->getVarBool( rData.m_flat, m_pTheme );

    // Get the color values
    uint32_t fgColor = getColor( rData.m_fgColor );
    uint32_t playColor = getColor( rData.m_playColor );
    uint32_t bgColor1 = getColor( rData.m_bgColor1 );
    uint32_t bgColor2 = getColor( rData.m_bgColor2 );
    uint32_t selColor = getColor( rData.m_selColor );

    // Create the list control
    CtrlTree *pTree = new CtrlTree( getIntf(), *pVar, *pFont, pBgBmp,
       pItemBmp, pOpenBmp, pClosedBmp,
       fgColor, playColor, bgColor1, bgColor2, selColor,
       UString( getIntf(), rData.m_help.c_str() ), pVisible, pFlat );
    m_pTheme->m_controls[rData.m_id] = CtrlGenericPtr( pTree );

    // Compute the position of the control
    const GenericRect *pRect;
    GET_BOX( pRect, rData.m_panelId , pLayout);
    const Position pos = makePosition( rData.m_leftTop, rData.m_rightBottom,
                                       rData.m_xPos, rData.m_yPos,
                                       rData.m_width, rData.m_height,
                                       *pRect,
                                       rData.m_xKeepRatio, rData.m_yKeepRatio );

    pLayout->addControl( pTree, pos, rData.m_layer );
}

void Builder::addVideo( const BuilderData::Video &rData )
{
    GenericLayout *pLayout = m_pTheme->getLayoutById( rData.m_layoutId );
    if( pLayout == NULL )
    {
        msg_Err( getIntf(), "unknown layout id: %s", rData.m_layoutId.c_str() );
        return;
    }

    BuilderData::Video Data = rData;
    if( Data.m_autoResize )
    {
        // force autoresize to false if the control is not able to
        // freely resize within its container
        if( Data.m_xKeepRatio || Data.m_yKeepRatio ||
            !( Data.m_leftTop == "lefttop" &&
               Data.m_rightBottom == "rightbottom" ) )
        {
            msg_Err( getIntf(),
                "video: resize policy and autoresize are not compatible" );
            Data.m_autoResize = false;
        }
    }
    if( !(Data.m_width > 0 && Data.m_height > 0) )
    {
        msg_Err( getIntf(),
            "pls, provide a valid size for the video control id: %s "
             "(dropping the video control)", Data.m_id.c_str() );
        return;
    }

    // Get the visibility variable
    // XXX check when it is null
    Interpreter *pInterpreter = Interpreter::instance( getIntf() );
    VarBool *pVisible = pInterpreter->getVarBool( Data.m_visible, m_pTheme );

    CtrlVideo *pVideo = new CtrlVideo( getIntf(), *pLayout,
        Data.m_autoResize, UString( getIntf(), Data.m_help.c_str() ),
        pVisible );
    m_pTheme->m_controls[Data.m_id] = CtrlGenericPtr( pVideo );

    // Compute the position of the control
    const GenericRect *pRect;
    GET_BOX( pRect, rData.m_panelId , pLayout);
    const Position pos = makePosition( Data.m_leftTop, Data.m_rightBottom,
                                       Data.m_xPos, Data.m_yPos,
                                       Data.m_width, Data.m_height,
                                       *pRect,
                                       Data.m_xKeepRatio, Data.m_yKeepRatio );

    pLayout->addControl( pVideo, pos, Data.m_layer );
}


const Position Builder::makePosition( const string &rLeftTop,
                                      const string &rRightBottom,
                                      int xPos, int yPos, int width,
                                      int height, const GenericRect &rRect,
                                      bool xKeepRatio, bool yKeepRatio ) const
{
    int left = 0, top = 0, right = 0, bottom = 0;
    Position::Ref_t refLeftTop = Position::kLeftTop;
    Position::Ref_t refRightBottom = Position::kLeftTop;

    int boxWidth = rRect.getWidth();
    int boxHeight = rRect.getHeight();

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

    // In "keep ratio" mode, overwrite the previously computed values with the
    // actual ones
    // XXX: this coupling between makePosition and the Position class should
    // be reduced...
    if( xKeepRatio )
    {
        left = xPos;
        right = xPos + width;
    }
    if( yKeepRatio )
    {
        top = yPos;
        bottom = yPos + height;
    }

    return Position( left, top, right, bottom, rRect, refLeftTop,
                     refRightBottom, xKeepRatio, yKeepRatio );
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
        for( it = resPath.begin(); it != resPath.end(); ++it )
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
            msg_Err( getIntf(), "failed to open the default font" );
        }
    }
    return pFont;
}


string Builder::getFilePath( const string &rFileName ) const
{
    OSFactory *pFactory = OSFactory::instance( getIntf() );
    const string &sep = pFactory->getDirSeparator();

    string file = rFileName;
    if( file.find( "\\" ) != string::npos )
    {
        // For skins to be valid on both Linux and Win32,
        // slash should be used as path separator for both OSs.
        msg_Warn( getIntf(), "use of '/' is preferred to '\\' for paths" );
        string::size_type pos;
        while( ( pos = file.find( "\\" ) ) != string::npos )
           file[pos] = '/';
    }

#if defined( _WIN32 ) || defined( __OS2__ )
    string::size_type pos;
    while( ( pos = file.find( "/" ) ) != string::npos )
       file.replace( pos, 1, sep );
#endif

    string full_path = m_path + sep + sFromLocale( file );

    // check that the file exists and can be read
    if( ifstream( full_path.c_str() ).fail() )
    {
        msg_Err( getIntf(), "missing file: %s", file.c_str() );
        full_path = "";
    }

    return full_path;
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


uint32_t Builder::getColor( const string &rVal ) const
{
    // Check it the value is a registered constant
    Interpreter *pInterpreter = Interpreter::instance( getIntf() );
    string val = pInterpreter->getConstant( rVal );

    // Convert to an int value
    return SkinParser::convertColor( val.c_str() );
}

