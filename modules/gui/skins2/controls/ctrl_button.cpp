/*****************************************************************************
 * ctrl_button.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN (Centrale RÃ©seaux) and its contributors
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

#include "ctrl_button.hpp"
#include "../events/evt_generic.hpp"
#include "../src/generic_bitmap.hpp"
#include "../src/os_factory.hpp"
#include "../src/os_graphics.hpp"
#include "../commands/cmd_generic.hpp"


CtrlButton::CtrlButton( intf_thread_t *pIntf, const GenericBitmap &rBmpUp,
                        const GenericBitmap &rBmpOver,
                        const GenericBitmap &rBmpDown, CmdGeneric &rCommand,
                        const UString &rTooltip, const UString &rHelp,
                        VarBool *pVisible ):
    CtrlGeneric( pIntf, rHelp, pVisible ), m_fsm( pIntf ),
    m_rCommand( rCommand ), m_tooltip( rTooltip ),
    m_cmdUpOverDownOver( this, &transUpOverDownOver ),
    m_cmdDownOverUpOver( this, &transDownOverUpOver ),
    m_cmdDownOverDown( this, &transDownOverDown ),
    m_cmdDownDownOver( this, &transDownDownOver ),
    m_cmdUpOverUp( this, &transUpOverUp ),
    m_cmdUpUpOver( this, &transUpUpOver ),
    m_cmdDownUp( this, &transDownUp ),
    m_cmdUpHidden( this, &transUpHidden ),
    m_cmdHiddenUp( this, &transHiddenUp )
{
    // Build the images of the button
    OSFactory *pOsFactory = OSFactory::instance( pIntf );
    m_pImgUp = pOsFactory->createOSGraphics( rBmpUp.getWidth(),
                                             rBmpUp.getHeight() );
    m_pImgUp->drawBitmap( rBmpUp, 0, 0 );
    m_pImgDown = pOsFactory->createOSGraphics( rBmpDown.getWidth(),
                                               rBmpDown.getHeight() );
    m_pImgDown->drawBitmap( rBmpDown, 0, 0 );
    m_pImgOver = pOsFactory->createOSGraphics( rBmpOver.getWidth(),
                                               rBmpOver.getHeight() );
    m_pImgOver->drawBitmap( rBmpOver, 0, 0 );

    // States
    m_fsm.addState( "up" );
    m_fsm.addState( "down" );
    m_fsm.addState( "upOver" );
    m_fsm.addState( "downOver" );
    m_fsm.addState( "hidden" );

    // Transitions
    m_fsm.addTransition( "upOver", "mouse:left:down", "downOver",
                         &m_cmdUpOverDownOver );
    m_fsm.addTransition( "upOver", "mouse:left:dblclick", "downOver",
                         &m_cmdUpOverDownOver );
    m_fsm.addTransition( "downOver", "mouse:left:up", "upOver",
                         &m_cmdDownOverUpOver );
    m_fsm.addTransition( "downOver", "leave", "down", &m_cmdDownOverDown );
    m_fsm.addTransition( "down", "enter", "downOver", &m_cmdDownDownOver );
    m_fsm.addTransition( "upOver", "leave", "up", &m_cmdUpOverUp );
    m_fsm.addTransition( "up", "enter", "upOver", &m_cmdUpUpOver );
    m_fsm.addTransition( "down", "mouse:left:up", "up", &m_cmdDownUp );
    // XXX: It would be easy to use a "ANY" initial state to handle these
    // four lines in only one. But till now it isn't worthwhile...
    m_fsm.addTransition( "up", "special:hide", "hidden", &m_cmdUpHidden );
    m_fsm.addTransition( "down", "special:hide", "hidden", &m_cmdUpHidden );
    m_fsm.addTransition( "upOver", "special:hide", "hidden", &m_cmdUpHidden );
    m_fsm.addTransition( "downOver", "special:hide", "hidden", &m_cmdUpHidden );
    m_fsm.addTransition( "hidden", "special:show", "up", &m_cmdHiddenUp );

    // Initial state
    m_fsm.setState( "up" );
    m_pImg = m_pImgUp;
}


CtrlButton::~CtrlButton()
{
    SKINS_DELETE( m_pImgUp );
    SKINS_DELETE( m_pImgDown );
    SKINS_DELETE( m_pImgOver );
}


void CtrlButton::handleEvent( EvtGeneric &rEvent )
{
    m_fsm.handleTransition( rEvent.getAsString() );
}


bool CtrlButton::mouseOver( int x, int y ) const
{
    if( m_pImg )
    {
        return m_pImg->hit( x, y );
    }
    else
    {
        return false;
    }
}


void CtrlButton::draw( OSGraphics &rImage, int xDest, int yDest )
{
    if( m_pImg )
    {
        // Draw the current image
        rImage.drawGraphics( *m_pImg, 0, 0, xDest, yDest );
    }
}


void CtrlButton::transUpOverDownOver( SkinObject *pCtrl )
{
    CtrlButton *pThis = (CtrlButton*)pCtrl;
    pThis->captureMouse();
    const OSGraphics *pOldImg = pThis->m_pImg;
    pThis->m_pImg = pThis->m_pImgDown;
    pThis->notifyLayoutMaxSize( pOldImg, pThis->m_pImg );
}


void CtrlButton::transDownOverUpOver( SkinObject *pCtrl )
{
    CtrlButton *pThis = (CtrlButton*)pCtrl;
    pThis->releaseMouse();
    const OSGraphics *pOldImg = pThis->m_pImg;
    pThis->m_pImg = pThis->m_pImgUp;
    pThis->notifyLayoutMaxSize( pOldImg, pThis->m_pImg );
    // Execute the command associated to this button
    pThis->m_rCommand.execute();
}


void CtrlButton::transDownOverDown( SkinObject *pCtrl )
{
    CtrlButton *pThis = (CtrlButton*)pCtrl;
    const OSGraphics *pOldImg = pThis->m_pImg;
    pThis->m_pImg = pThis->m_pImgUp;
    pThis->notifyLayoutMaxSize( pOldImg, pThis->m_pImg );
}


void CtrlButton::transDownDownOver( SkinObject *pCtrl )
{
    CtrlButton *pThis = (CtrlButton*)pCtrl;
    const OSGraphics *pOldImg = pThis->m_pImg;
    pThis->m_pImg = pThis->m_pImgDown;
    pThis->notifyLayoutMaxSize( pOldImg, pThis->m_pImg );
}


void CtrlButton::transUpUpOver( SkinObject *pCtrl )
{
    CtrlButton *pThis = (CtrlButton*)pCtrl;
    const OSGraphics *pOldImg = pThis->m_pImg;
    pThis->m_pImg = pThis->m_pImgOver;
    pThis->notifyLayoutMaxSize( pOldImg, pThis->m_pImg );
}


void CtrlButton::transUpOverUp( SkinObject *pCtrl )
{
    CtrlButton *pThis = (CtrlButton*)pCtrl;
    const OSGraphics *pOldImg = pThis->m_pImg;
    pThis->m_pImg = pThis->m_pImgUp;
    pThis->notifyLayoutMaxSize( pOldImg, pThis->m_pImg );
}


void CtrlButton::transDownUp( SkinObject *pCtrl )
{
    CtrlButton *pThis = (CtrlButton*)pCtrl;
    pThis->releaseMouse();
}


void CtrlButton::transUpHidden( SkinObject *pCtrl )
{
    CtrlButton *pThis = (CtrlButton*)pCtrl;
    const OSGraphics *pOldImg = pThis->m_pImg;
    pThis->m_pImg = NULL;
    pThis->notifyLayoutMaxSize( pOldImg, pThis->m_pImg );
}


void CtrlButton::transHiddenUp( SkinObject *pCtrl )
{
    CtrlButton *pThis = (CtrlButton*)pCtrl;
    const OSGraphics *pOldImg = pThis->m_pImg;
    pThis->m_pImg = pThis->m_pImgUp;
    pThis->notifyLayoutMaxSize( pOldImg, pThis->m_pImg );
}

