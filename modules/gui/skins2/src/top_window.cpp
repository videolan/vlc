/*****************************************************************************
 * top_window.cpp
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

#include "top_window.hpp"
#include "generic_layout.hpp"
#include "os_graphics.hpp"
#include "os_window.hpp"
#include "os_factory.hpp"
#include "theme.hpp"
#include "var_manager.hpp"
#include "../commands/cmd_on_top.hpp"
#include "../commands/cmd_dialogs.hpp"
#include "../controls/ctrl_generic.hpp"
#include "../events/evt_refresh.hpp"
#include "../events/evt_enter.hpp"
#include "../events/evt_focus.hpp"
#include "../events/evt_leave.hpp"
#include "../events/evt_menu.hpp"
#include "../events/evt_motion.hpp"
#include "../events/evt_mouse.hpp"
#include "../events/evt_key.hpp"
#include "../events/evt_special.hpp"
#include "../events/evt_scroll.hpp"
#include "../utils/position.hpp"
#include "../utils/ustring.hpp"

#include <vlc_keys.h>


TopWindow::TopWindow( intf_thread_t *pIntf, int left, int top,
                      WindowManager &rWindowManager,
                      bool dragDrop, bool playOnDrop, bool visible ):
    GenericWindow( pIntf, left, top, dragDrop, playOnDrop, NULL ),
    m_visible( visible ), m_rWindowManager( rWindowManager ),
    m_pActiveLayout( NULL ), m_pLastHitControl( NULL ),
    m_pCapturingControl( NULL ), m_pFocusControl( NULL ), m_currModifier( 0 )
{
    // Register as a moving window
    m_rWindowManager.registerWindow( *this );
}


TopWindow::~TopWindow()
{
    // Unregister from the window manager
    m_rWindowManager.unregisterWindow( *this );
}


void TopWindow::processEvent( EvtRefresh &rEvtRefresh )
{
    // We override the behaviour defined in GenericWindow, because we don't
    // want to draw on a video control!
    if( m_pActiveLayout == NULL )
    {
        GenericWindow::processEvent( rEvtRefresh );
    }
    else
    {
        m_pActiveLayout->refreshRect( rEvtRefresh.getXStart(),
                                      rEvtRefresh.getYStart(),
                                      rEvtRefresh.getWidth(),
                                      rEvtRefresh.getHeight() );
    }
}


void TopWindow::processEvent( EvtFocus &rEvtFocus )
{
//    fprintf(stderr, rEvtFocus.getAsString().c_str());
}


void TopWindow::processEvent( EvtMenu &rEvtMenu )
{
    Popup *pPopup = m_rWindowManager.getActivePopup();
    // We should never receive a menu event when there is no active popup!
    if( pPopup == NULL )
    {
        msg_Warn( getIntf(), "Unexpected menu event, ignoring" );
        return;
    }

    pPopup->handleEvent( rEvtMenu );
}


void TopWindow::processEvent( EvtMotion &rEvtMotion )
{
    // New control hit by the mouse
    CtrlGeneric *pNewHitControl =
        findHitControl( rEvtMotion.getXPos() - getLeft(),
                        rEvtMotion.getYPos() - getTop() );

    setLastHit( pNewHitControl );

    /// Update the help text
    VarManager *pVarManager = VarManager::instance( getIntf() );
    if( pNewHitControl )
    {
        pVarManager->getHelpText().set( pNewHitControl->getHelpText() );
    }

    // Send a motion event to the hit control, or to the control
    // that captured the mouse, if any
    CtrlGeneric *pActiveControl = pNewHitControl;
    if( m_pCapturingControl )
    {
        pActiveControl = m_pCapturingControl;
    }
    if( pActiveControl )
    {
        // Compute the coordinates relative to the window
        int xPos = rEvtMotion.getXPos() - getLeft();
        int yPos = rEvtMotion.getYPos() - getTop();
        // Send a motion event
        EvtMotion evt( getIntf(), xPos, yPos );
        pActiveControl->handleEvent( evt );
    }
}


void TopWindow::processEvent( EvtLeave &rEvtLeave )
{
    // No more hit control
    setLastHit( NULL );

    if( !m_pCapturingControl )
    {
        m_rWindowManager.hideTooltip();
    }
}


void TopWindow::processEvent( EvtMouse &rEvtMouse )
{
    // Get the control hit by the mouse
    CtrlGeneric *pNewHitControl = findHitControl( rEvtMouse.getXPos(),
                                                  rEvtMouse.getYPos() );
    setLastHit( pNewHitControl );

    // Change the focused control
    if( rEvtMouse.getAction() == EvtMouse::kDown )
    {
        // Raise the window
        m_rWindowManager.raise( *this );

        if( pNewHitControl && pNewHitControl->isFocusable() )
        {
            // If a new control gains the focus, the previous one loses it
            if( m_pFocusControl && m_pFocusControl != pNewHitControl )
            {
                EvtFocus evt( getIntf(), false );
                m_pFocusControl->handleEvent( evt );
            }
            if( pNewHitControl != m_pFocusControl )
            {
                m_pFocusControl = pNewHitControl;
                EvtFocus evt( getIntf(), false );
                pNewHitControl->handleEvent( evt );
            }
        }
        else if( m_pFocusControl )
        {
            // The previous control loses the focus
            EvtFocus evt( getIntf(), false );
            m_pFocusControl->handleEvent( evt );
            m_pFocusControl = NULL;
        }
    }

    // Send a mouse event to the hit control, or to the control
    // that captured the mouse, if any
    CtrlGeneric *pActiveControl = pNewHitControl;
    if( m_pCapturingControl )
    {
        pActiveControl = m_pCapturingControl;
    }
    if( pActiveControl )
    {
        pActiveControl->handleEvent( rEvtMouse );
    }
}


void TopWindow::processEvent( EvtKey &rEvtKey )
{
    // Forward the event to the focused control, if any
    if( m_pFocusControl )
    {
        m_pFocusControl->handleEvent( rEvtKey );
        return;
    }

    // Only do the action when the key is down
    if( rEvtKey.getAsString().find( "key:down") != string::npos )
    {
        //XXX not to be hardcoded!
        // Ctrl-S = Change skin
        if( (rEvtKey.getMod() & EvtInput::kModCtrl) &&
            rEvtKey.getKey() == 's' )
        {
            CmdDlgChangeSkin cmd( getIntf() );
            cmd.execute();
            return;
        }

        //XXX not to be hardcoded!
        // Ctrl-T = Toggle on top
        if( (rEvtKey.getMod() & EvtInput::kModCtrl) &&
            rEvtKey.getKey() == 't' )
        {
            CmdOnTop cmd( getIntf() );
            cmd.execute();
            return;
        }

        vlc_value_t val;
        // Set the key
        val.i_int = rEvtKey.getKey();
        // Set the modifiers
        if( rEvtKey.getMod() & EvtInput::kModAlt )
        {
            val.i_int |= KEY_MODIFIER_ALT;
        }
        if( rEvtKey.getMod() & EvtInput::kModCtrl )
        {
            val.i_int |= KEY_MODIFIER_CTRL;
        }
        if( rEvtKey.getMod() & EvtInput::kModShift )
        {
            val.i_int |= KEY_MODIFIER_SHIFT;
        }

        var_Set( getIntf()->p_vlc, "key-pressed", val );
    }

    // Always store the modifier, which can be needed for scroll events
    m_currModifier = rEvtKey.getMod();
}


void TopWindow::processEvent( EvtScroll &rEvtScroll )
{
    // Raise the windows
    raise();

    // Get the control hit by the mouse
    CtrlGeneric *pNewHitControl = findHitControl( rEvtScroll.getXPos(),
                                                  rEvtScroll.getYPos());
    setLastHit( pNewHitControl );

    // Send a mouse event to the hit control, or to the control
    // that captured the mouse, if any
    CtrlGeneric *pActiveControl = pNewHitControl;

    if( m_pCapturingControl )
    {
        pActiveControl = m_pCapturingControl;
    }
    if( pActiveControl )
    {
        pActiveControl->handleEvent( rEvtScroll );
    }
    else
    {
        // Treat the scroll event as a hotkey
        vlc_value_t val;
        if( rEvtScroll.getDirection() == EvtScroll::kUp )
        {
            val.i_int = KEY_MOUSEWHEELUP;
        }
        else
        {
            val.i_int = KEY_MOUSEWHEELDOWN;
        }
        // Add the modifiers
        val.i_int |= m_currModifier;

        var_Set( getIntf()->p_vlc, "key-pressed", val );
    }
}


void TopWindow::forwardEvent( EvtGeneric &rEvt, CtrlGeneric &rCtrl )
{
    // XXX: We should do some checks here
    rCtrl.handleEvent( rEvt );
}


void TopWindow::refresh( int left, int top, int width, int height )
{
    if( m_pActiveLayout )
    {
        m_pActiveLayout->getImage()->copyToWindow( *getOSWindow(), left, top,
                                                   width, height, left, top );
    }
}


void TopWindow::setActiveLayout( GenericLayout *pLayout )
{
    pLayout->setWindow( this );
    m_pActiveLayout = pLayout;
    // Get the size of the layout and resize the window
    resize( pLayout->getWidth(), pLayout->getHeight() );
    updateShape();
    pLayout->refreshAll();
}


const GenericLayout& TopWindow::getActiveLayout() const
{
    return *m_pActiveLayout;
}


void TopWindow::innerShow()
{
    // First, refresh the layout and update the shape of the window
    if( m_pActiveLayout )
    {
        updateShape();
        m_pActiveLayout->refreshAll();
    }
    // Show the window
    GenericWindow::innerShow();
}

 
void TopWindow::updateShape()
{
    // Set the shape of the window
    if( m_pActiveLayout )
    {
        OSGraphics *pImage = m_pActiveLayout->getImage();
        if( pImage )
        {
            pImage->applyMaskToWindow( *getOSWindow() );
        }
    }
}


void TopWindow::onControlCapture( const CtrlGeneric &rCtrl )
{
    // Set the capturing control
    m_pCapturingControl = (CtrlGeneric*) &rCtrl;
}


void TopWindow::onControlRelease( const CtrlGeneric &rCtrl )
{
    // Release the capturing control
    if( m_pCapturingControl == &rCtrl )
    {
        m_pCapturingControl = NULL;
    }
    else
    {
        msg_Dbg( getIntf(), "Control had not captured the mouse" );
    }

    // Send an enter event to the control under the mouse, if it doesn't
    // have received it yet
    if( m_pLastHitControl && m_pLastHitControl != &rCtrl )
    {
        EvtEnter evt( getIntf() );
        m_pLastHitControl->handleEvent( evt );

        // Show the tooltip
        m_rWindowManager.hideTooltip();
        UString tipText = m_pLastHitControl->getTooltipText();
        if( tipText.length() > 0 )
        {
            // Set the tooltip text variable
            VarManager *pVarManager = VarManager::instance( getIntf() );
            pVarManager->getTooltipText().set( tipText );
            m_rWindowManager.showTooltip();
        }
    }
}


void TopWindow::onTooltipChange( const CtrlGeneric &rCtrl )
{
    // Check that the control is the active one
    if( m_pLastHitControl && m_pLastHitControl == &rCtrl )
    {
        // Set the tooltip text variable
        VarManager *pVarManager = VarManager::instance( getIntf() );
        pVarManager->getTooltipText().set( rCtrl.getTooltipText() );
    }
}


CtrlGeneric *TopWindow::findHitControl( int xPos, int yPos )
{
    if( m_pActiveLayout == NULL )
    {
        return NULL;
    }

    // Get the controls in the active layout
    const list<LayeredControl> &ctrlList = m_pActiveLayout->getControlList();
    list<LayeredControl>::const_reverse_iterator iter;

    // New control hit by the mouse
    CtrlGeneric *pNewHitControl = NULL;

    // Loop on the control list to find the uppest hit control
    for( iter = ctrlList.rbegin(); iter != ctrlList.rend(); iter++ )
    {
        // Get the position of the control in the layout
        const Position *pos = (*iter).m_pControl->getPosition();
        if( pos != NULL )
        {
            // Compute the coordinates of the mouse relative to the control
            int xRel = xPos - pos->getLeft();
            int yRel = yPos - pos->getTop();

            CtrlGeneric *pCtrl = (*iter).m_pControl;
            // Control hit ?
            if( pCtrl->isVisible() && pCtrl->mouseOver( xRel, yRel ) )
            {
                pNewHitControl = (*iter).m_pControl;
                break;
            }
        }
        else
        {
            msg_Dbg( getIntf(), "Control at NULL position" );
        }
    }

    // If the hit control has just been entered, send it an enter event
    if( pNewHitControl && (pNewHitControl != m_pLastHitControl) )
    {
        // Don't send the event if another control captured the mouse
        if( !m_pCapturingControl || (m_pCapturingControl == pNewHitControl ) )
        {
            EvtEnter evt( getIntf() );
            pNewHitControl->handleEvent( evt );

            if( !m_pCapturingControl )
            {
                // Show the tooltip
                m_rWindowManager.hideTooltip();
                UString tipText = pNewHitControl->getTooltipText();
                if( tipText.length() > 0 )
                {
                    // Set the tooltip text variable
                    VarManager *pVarManager = VarManager::instance( getIntf() );
                    pVarManager->getTooltipText().set( tipText );
                    m_rWindowManager.showTooltip();
                }
            }
        }
    }

    return pNewHitControl;
}



void TopWindow::setLastHit( CtrlGeneric *pNewHitControl )
{
    // Send a leave event to the left control
    if( m_pLastHitControl && (pNewHitControl != m_pLastHitControl) )
    {
        // Don't send the event if another control captured the mouse
        if( !m_pCapturingControl || (m_pCapturingControl == m_pLastHitControl))
        {
            EvtLeave evt( getIntf() );
            m_pLastHitControl->handleEvent( evt );
        }
    }

    m_pLastHitControl = pNewHitControl;
}

