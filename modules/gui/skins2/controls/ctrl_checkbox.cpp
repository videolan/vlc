/*****************************************************************************
 * ctrl_checkbox.cpp
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

#include "ctrl_checkbox.hpp"
#include "../events/evt_generic.hpp"
#include "../commands/cmd_generic.hpp"
#include "../src/generic_bitmap.hpp"
#include "../src/os_factory.hpp"
#include "../src/os_graphics.hpp"
#include "../utils/var_bool.hpp"


CtrlCheckbox::CtrlCheckbox( intf_thread_t *pIntf,
                            const GenericBitmap &rBmpUp1,
                            const GenericBitmap &rBmpOver1,
                            const GenericBitmap &rBmpDown1,
                            const GenericBitmap &rBmpUp2,
                            const GenericBitmap &rBmpOver2,
                            const GenericBitmap &rBmpDown2,
                            CmdGeneric &rCommand1, CmdGeneric &rCommand2,
                            const UString &rTooltip1,
                            const UString &rTooltip2,
                            VarBool &rVariable, const UString &rHelp,
                            VarBool *pVisible ):
    CtrlGeneric( pIntf, rHelp, pVisible ), m_fsm( pIntf ),
    m_rVariable( rVariable ),
    m_rCommand1( rCommand1 ), m_rCommand2( rCommand2 ),
    m_tooltip1( rTooltip1 ), m_tooltip2( rTooltip2 ),
    m_imgUp1( pIntf, rBmpUp1 ), m_imgOver1( pIntf, rBmpOver1 ),
    m_imgDown1( pIntf, rBmpDown1 ), m_imgUp2( pIntf, rBmpUp2 ),
    m_imgOver2( pIntf, rBmpOver2 ), m_imgDown2( pIntf, rBmpDown2 ),
    m_cmdUpOverDownOver( this ), m_cmdDownOverUpOver( this ),
    m_cmdDownOverDown( this ), m_cmdDownDownOver( this ),
    m_cmdUpOverUp( this ), m_cmdUpUpOver( this ),
    m_cmdDownUp( this ), m_cmdUpHidden( this ),
    m_cmdHiddenUp( this )
{
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

    // Observe the variable
    m_rVariable.addObserver( this );

    // Initial state
    m_fsm.setState( "up" );
    if( !m_rVariable.get() )
    {
        m_pImgUp = &m_imgUp1;
        m_pImgOver = &m_imgOver1;
        m_pImgDown = &m_imgDown1;
        m_pImgCurrent = m_pImgUp;
        m_pCommand = &m_rCommand1;
        m_pTooltip = &m_tooltip1;
    }
    else
    {
        m_pImgUp = &m_imgUp2;
        m_pImgOver = &m_imgOver2;
        m_pImgDown = &m_imgDown2;
        m_pImgCurrent = m_pImgUp;
        m_pCommand = &m_rCommand2;
        m_pTooltip = &m_tooltip2;
    }
}


CtrlCheckbox::~CtrlCheckbox()
{
    if( m_pImgCurrent )
    {
        m_pImgCurrent->stopAnim();
        m_pImgCurrent->delObserver( this );
    }
    m_rVariable.delObserver( this );
}


void CtrlCheckbox::handleEvent( EvtGeneric &rEvent )
{
    m_fsm.handleTransition( rEvent.getAsString() );
}


bool CtrlCheckbox::mouseOver( int x, int y ) const
{
    if( m_pImgCurrent )
    {
        return m_pImgCurrent->hit( x, y );
    }
    else
    {
        return false;
    }
}


void CtrlCheckbox::draw( OSGraphics &rImage, int xDest, int yDest, int w, int h )
{
    if( !m_pImgCurrent )
        return;

    const Position *pPos = getPosition();
    // rect region( pPos->getLeft(), pPos->getTop(),
    //              pPos->getWidth(), pPos->getHeight() );
    rect region( pPos->getLeft(), pPos->getTop(),
                 m_pImgCurrent->getWidth(), m_pImgCurrent->getHeight() );
    rect clip( xDest, yDest, w, h );
    rect inter;
    if( rect::intersect( region, clip, &inter ) )
    {
        // Draw the current image
        m_pImgCurrent->draw( rImage,
                      inter.x, inter.y, inter.width, inter.height,
                      inter.x - pPos->getLeft(),
                      inter.y - pPos->getTop() );
    }
}


void CtrlCheckbox::setImage( AnimBitmap *pImg )
{
    if( pImg == m_pImgCurrent )
        return;

    AnimBitmap *pOldImg = m_pImgCurrent;
    m_pImgCurrent = pImg;

    if( pOldImg )
    {
        pOldImg->stopAnim();
        pOldImg->delObserver( this );
    }

    if( pImg )
    {
        pImg->startAnim();
        pImg->addObserver( this );
    }

    notifyLayoutMaxSize( pOldImg, pImg );
}


void CtrlCheckbox::CmdUpOverDownOver::execute()
{
    m_pParent->captureMouse();
    m_pParent->setImage( m_pParent->m_pImgDown );
}


void CtrlCheckbox::CmdDownOverUpOver::execute()
{
    m_pParent->releaseMouse();
    m_pParent->setImage( m_pParent->m_pImgUp );
    // Execute the command
    m_pParent->m_pCommand->execute();
}


void CtrlCheckbox::CmdDownOverDown::execute()
{
    m_pParent->setImage( m_pParent->m_pImgUp );
}


void CtrlCheckbox::CmdDownDownOver::execute()
{
    m_pParent->setImage( m_pParent->m_pImgDown );
}


void CtrlCheckbox::CmdUpUpOver::execute()
{
    m_pParent->setImage( m_pParent->m_pImgOver );
}


void CtrlCheckbox::CmdUpOverUp::execute()
{
    m_pParent->setImage( m_pParent->m_pImgUp );
}


void CtrlCheckbox::CmdDownUp::execute()
{
    m_pParent->releaseMouse();
}


void CtrlCheckbox::CmdUpHidden::execute()
{
    m_pParent->setImage( NULL );
}


void CtrlCheckbox::CmdHiddenUp::execute()
{
    m_pParent->setImage( m_pParent->m_pImgUp );
}


void CtrlCheckbox::onVarBoolUpdate( VarBool &rVariable )
{
    (void)rVariable;
    changeButton();
}


void CtrlCheckbox::onUpdate( Subject<AnimBitmap> &rBitmap, void *arg )
{
    (void)rBitmap;(void)arg;
    notifyLayout( m_pImgCurrent->getWidth(), m_pImgCurrent->getHeight() );
}


void CtrlCheckbox::changeButton()
{
    // Are we using the first set of images or the second one?
    if( m_pImgUp == &m_imgUp1 )
    {
        m_pImgUp = &m_imgUp2;
        m_pImgOver = &m_imgOver2;
        m_pImgDown = &m_imgDown2;
        m_pTooltip = &m_tooltip2;
        m_pCommand = &m_rCommand2;
    }
    else
    {
        m_pImgUp = &m_imgUp1;
        m_pImgOver = &m_imgOver1;
        m_pImgDown = &m_imgDown1;
        m_pTooltip = &m_tooltip1;
        m_pCommand = &m_rCommand1;
    }
    // XXX: We assume that the checkbox is up
    setImage( m_pImgUp );

    // Notify the window the tooltip has changed
    notifyTooltipChange();
}

