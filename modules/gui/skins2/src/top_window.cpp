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
#include "../commands/cmd_add_item.hpp"
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
#include "../events/evt_dragndrop.hpp"
#include "../utils/position.hpp"
#include "../utils/ustring.hpp"

#include <vlc_keys.h>
#include <list>


TopWindow::TopWindow( intf_thread_t *pIntf, int left, int top,
                      WindowManager &rWindowManager,
                      bool dragDrop, bool playOnDrop, bool visible,
                      GenericWindow::WindowType_t type ):
    GenericWindow( pIntf, left, top, dragDrop, playOnDrop, NULL, type ),
    m_initialVisibility( visible ), m_playOnDrop( playOnDrop ),
    m_rWindowManager( rWindowManager ),
    m_pActiveLayout( NULL ), m_pLastHitControl( NULL ),
    m_pCapturingControl( NULL ), m_pFocusControl( NULL ),
    m_pDragControl( NULL ), m_currModifier( 0 )
{
    // Register as a moving window
    m_rWindowManager.registerWindow( *this );

    // Create the "maximized" variable and register it in the manager
    m_pVarMaximized = new VarBoolImpl( pIntf );
    VarManager::instance( pIntf )->registerVar( VariablePtr( m_pVarMaximized ) );
}


TopWindow::~TopWindow()
{
    // Unregister from the window manager
    m_rWindowManager.unregisterWindow( *this );
}


void TopWindow::processEvent( EvtFocus &rEvtFocus )
{
    (void)rEvtFocus;
}


void TopWindow::processEvent( EvtMenu &rEvtMenu )
{
    Popup *pPopup = m_rWindowManager.getActivePopup();
    // We should never receive a menu event when there is no active popup!
    if( pPopup == NULL )
    {
        msg_Warn( getIntf(), "unexpected menu event, ignoring" );
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
    (void)rEvtLeave;

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

        if( m_pFocusControl != pNewHitControl )
        {
            if( m_pFocusControl )
            {
                // The previous control loses the focus
                EvtFocus evt( getIntf(), false );
                m_pFocusControl->handleEvent( evt );
                m_pFocusControl = NULL;
            }

            if( pNewHitControl && pNewHitControl->isFocusable() )
            {
                // The hit control gains the focus
                m_pFocusControl = pNewHitControl;
                EvtFocus evt( getIntf(), true );
                pNewHitControl->handleEvent( evt );
            }
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
    if( rEvtKey.getKeyState() == EvtKey::kDown )
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

        var_SetInteger( getIntf()->p_libvlc, "key-pressed",
                        rEvtKey.getModKey() );
    }

    // Always store the modifier, which can be needed for scroll events.
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

    // send a mouse event to the right control when scrollable
    // if none, send it directly to the vlc core
    CtrlGeneric *pHitControl = m_pCapturingControl ?
                               m_pCapturingControl : pNewHitControl;

    if( pHitControl && pHitControl->isScrollable() )
    {
        pHitControl->handleEvent( rEvtScroll );
    }
    else
    {
        // Treat the scroll event as a hotkey plus current modifiers
        int i = (rEvtScroll.getDirection() == EvtScroll::kUp ?
                 KEY_MOUSEWHEELUP : KEY_MOUSEWHEELDOWN) | m_currModifier;

        var_SetInteger( getIntf()->p_libvlc, "key-pressed", i );
    }
}

void TopWindow::processEvent( EvtDragDrop &rEvtDragDrop )
{
    // Get the control hit by the mouse
    int xPos = rEvtDragDrop.getXPos() - getLeft();
    int yPos = rEvtDragDrop.getYPos() - getTop();

    CtrlGeneric *pHitControl = findHitControl( xPos, yPos );
    if( pHitControl && pHitControl->getType() == "tree" )
    {
        // Send a dragDrop event
        EvtDragDrop evt( getIntf(), xPos, yPos, rEvtDragDrop.getFiles() );
        pHitControl->handleEvent( evt );
    }
    else
    {
        list<string> files = rEvtDragDrop.getFiles();
        list<string>::const_iterator it = files.begin();
        for( bool first = true; it != files.end(); ++it, first = false )
        {
            bool playOnDrop = m_playOnDrop && first;
            CmdAddItem( getIntf(), it->c_str(), playOnDrop ).execute();
        }
    }
    m_pDragControl = NULL;
}

void TopWindow::processEvent( EvtDragOver &rEvtDragOver )
{
    // Get the control hit by the mouse
    int xPos = rEvtDragOver.getXPos() - getLeft();
    int yPos = rEvtDragOver.getYPos() - getTop();

    CtrlGeneric *pHitControl = findHitControl( xPos, yPos );

    if( m_pDragControl && m_pDragControl != pHitControl )
    {
        EvtDragLeave evt( getIntf() );
        m_pDragControl->handleEvent( evt );
    }

    m_pDragControl = pHitControl;

    if( m_pDragControl )
    {
        // Send a dragOver event
        EvtDragOver evt( getIntf(), xPos, yPos );
        m_pDragControl->handleEvent( evt );
    }
}

void TopWindow::processEvent( EvtDragLeave &rEvtDragLeave )
{
    (void)rEvtDragLeave;
    if( m_pDragControl )
    {
        EvtDragLeave evt( getIntf() );
        m_pDragControl->handleEvent( evt );
        m_pDragControl = NULL;
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
    bool isVisible = getVisibleVar().get();
    if( m_pActiveLayout )
    {
        if( isVisible )
        {
            m_pActiveLayout->onHide();
        }
        // The current layout becomes inactive
        m_pActiveLayout->getActiveVar().set( false );

        // if both layouts have the same original size, infer a
        // subsequent resize of the active layout has to be applied
        // to the new layout about to become active
        if( pLayout->isTightlyCoupledWith( *m_pActiveLayout ) )
            pLayout->resize( m_pActiveLayout->getWidth(),
                             m_pActiveLayout->getHeight() );
    }

    pLayout->setWindow( this );
    m_pActiveLayout = pLayout;
    // Get the size of the layout and resize the window
    resize( pLayout->getWidth(), pLayout->getHeight() );

    if( isVisible )
    {
        pLayout->onShow();
    }

    // The new layout is active
    pLayout->getActiveVar().set( true );
}


const GenericLayout& TopWindow::getActiveLayout() const
{
    return *m_pActiveLayout;
}


void TopWindow::innerShow()
{
    // First, refresh the layout
    if( m_pActiveLayout )
    {
        m_pActiveLayout->onShow();
    }

    // Show the window
    GenericWindow::innerShow();
}


void TopWindow::innerHide()
{
    if( m_pActiveLayout )
    {
        // Notify the active layout
        m_pActiveLayout->onHide();
    }
    // Hide the window
    GenericWindow::innerHide();
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
        msg_Dbg( getIntf(), "control had not captured the mouse" );
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
        if( rCtrl.getTooltipText().size() )
        {
            // Set the tooltip text variable
            VarManager *pVarManager = VarManager::instance( getIntf() );
            pVarManager->getTooltipText().set( rCtrl.getTooltipText() );
            m_rWindowManager.showTooltip();
        }
        else
        {
            // Nothing to display, so hide the tooltip
            m_rWindowManager.hideTooltip();
        }
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
    for( iter = ctrlList.rbegin(); iter != ctrlList.rend(); ++iter )
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
            msg_Dbg( getIntf(), "control at NULL position" );
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

