/*****************************************************************************
 * generic_window.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: generic_window.cpp,v 1.1 2004/01/03 23:31:33 asmax Exp $
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

#include "generic_window.hpp"
#include "generic_layout.hpp"
#include "os_graphics.hpp"
#include "os_window.hpp"
#include "os_factory.hpp"
#include "theme.hpp"
#include "ft2_font.hpp"
#include "tooltip.hpp"
#include "dialogs.hpp"
#include "var_manager.hpp"
#include "../commands/cmd_on_top.hpp"
#include "../controls/ctrl_generic.hpp"
#include "../events/evt_enter.hpp"
#include "../events/evt_focus.hpp"
#include "../events/evt_leave.hpp"
#include "../events/evt_motion.hpp"
#include "../events/evt_mouse.hpp"
#include "../events/evt_key.hpp"
#include "../events/evt_refresh.hpp"
#include "../events/evt_special.hpp"
#include "../events/evt_scroll.hpp"
#include "../utils/position.hpp"
#include "../utils/ustring.hpp"

#include <vlc_keys.h>


GenericWindow::GenericWindow( intf_thread_t *pIntf, int left, int top,
                              WindowManager &rWindowManager,
                              const GenericFont &rTipFont,
                              bool dragDrop, bool playOnDrop ):
    SkinObject( pIntf ), m_rWindowManager( rWindowManager ),
    m_left( left ), m_top( top ), m_width( 0 ), m_height( 0 ),
    m_pActiveLayout( NULL ), m_pLastHitControl( NULL ),
    m_pCapturingControl( NULL ), m_pFocusControl( NULL ), m_varVisible( pIntf )
{
    // Register as a moving window
    m_rWindowManager.registerWindow( this );

    // Get the OSFactory
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );

    // Create an OSWindow to handle OS specific processing
    m_pOsWindow = pOsFactory->createOSWindow( *this, dragDrop, playOnDrop );

    // Create the tooltip window
    m_pTooltip = new Tooltip( getIntf(), rTipFont, 500 );

    // Observe the visibility variable
    m_varVisible.addObserver( this );
}


GenericWindow::~GenericWindow()
{
    m_varVisible.delObserver( this );
    // Unregister from the window manager
    m_rWindowManager.unregisterWindow( this );

    if( m_pTooltip )
    {
        delete m_pTooltip;
    }
    if( m_pOsWindow )
    {
        delete m_pOsWindow;
    }
}


void GenericWindow::processEvent( EvtFocus &rEvtFocus )
{
//    fprintf(stderr, rEvtFocus.getAsString().c_str()) ;
}


void GenericWindow::processEvent( EvtMotion &rEvtMotion )
{
    // New control hit by the mouse
    CtrlGeneric *pNewHitControl =
        findHitControl( rEvtMotion.getXPos() - m_left,
                        rEvtMotion.getYPos() - m_top );

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
        int xPos = rEvtMotion.getXPos() - m_left;
        int yPos = rEvtMotion.getYPos() - m_top;
        // Send a motion event
        EvtMotion evt( getIntf(), xPos, yPos );
        pActiveControl->handleEvent( evt );
    }
}


void GenericWindow::processEvent( EvtLeave &rEvtLeave )
{
    // No more hit control
    setLastHit( NULL );

    if( !m_pCapturingControl )
    {
        m_pTooltip->hide();
    }
}


void GenericWindow::processEvent( EvtMouse &rEvtMouse )
{
    // Raise the window and its anchored windows
    m_rWindowManager.raise( this );

    // Get the control hit by the mouse
    CtrlGeneric *pNewHitControl = findHitControl( rEvtMouse.getXPos(),
                                                  rEvtMouse.getYPos() );

    setLastHit( pNewHitControl );

    // Change the focused control
    if( rEvtMouse.getAction() == EvtMouse::kDown )
    {
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


void GenericWindow::processEvent( EvtKey &rEvtKey )
{
    // Forward the event to the focused control, if any
    if( m_pFocusControl )
    {
        m_pFocusControl->handleEvent( rEvtKey );
    }

    // Only do the action when the key is down
    else if( rEvtKey.getAsString().find( "key:down") != string::npos )
    {
        //XXX not to be hardcoded !
        // Ctrl-S = Change skin
        if( (rEvtKey.getMod() & EvtInput::kModCtrl) &&
            rEvtKey.getKey() == 's' )
        {
            Dialogs::instance( getIntf() )->showChangeSkin();
            return;
        }

        //XXX not to be hardcoded !
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
}


void GenericWindow::processEvent( EvtRefresh &rEvtRefresh )
{
    // Refresh the given area
    refresh( rEvtRefresh.getXStart(), rEvtRefresh.getYStart(),
             rEvtRefresh.getWidth(), rEvtRefresh.getHeight() );
}


void GenericWindow::processEvent( EvtScroll &rEvtScroll )
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
}


void GenericWindow::forwardEvent( EvtGeneric &rEvt, CtrlGeneric &rCtrl )
{
    // XXX: We should do some checks here
    rCtrl.handleEvent( rEvt );
}


void GenericWindow::show()
{
    m_varVisible.set( true );
}


void GenericWindow::hide()
{
    m_varVisible.set( false );
}


void GenericWindow::refresh( int left, int top, int width, int height )
{
    if( m_pActiveLayout )
    {
        m_pActiveLayout->getImage()->copyToWindow( *m_pOsWindow, left, top,
                                                   width, height, left, top );
    }
}


void GenericWindow::move( int left, int top )
{
    // Update the window coordinates
    m_left = left;
    m_top = top;

    m_pOsWindow->moveResize( left, top, m_width, m_height );
}


void GenericWindow::resize( int width, int height )
{
    // Update the window size
    m_width = width;
    m_height = height;

    m_pOsWindow->moveResize( m_left, m_top, width, height );
}


void GenericWindow::raise()
{
    m_pOsWindow->raise();
}


void GenericWindow::setOpacity( uint8_t value )
{
    m_pOsWindow->setOpacity( value );
}


void GenericWindow::toggleOnTop( bool onTop )
{
    m_pOsWindow->toggleOnTop( onTop );
}


void GenericWindow::setActiveLayout( GenericLayout *pLayout )
{
    pLayout->setWindow( this );
    m_pActiveLayout = pLayout;
    // Get the size of the layout and resize the window
    m_width = pLayout->getWidth();
    m_height = pLayout->getHeight();
    m_pOsWindow->moveResize( m_left, m_top, m_width, m_height );
    updateShape();
    pLayout->refreshAll();
}


void GenericWindow::updateShape()
{
    // Set the shape of the window
    if( m_pActiveLayout )
    {
        OSGraphics *pImage = m_pActiveLayout->getImage();
        if( pImage )
        {
            pImage->applyMaskToWindow( *m_pOsWindow );
        }
    }
}


const list<Anchor*> GenericWindow::getAnchorList() const
{
    return m_anchorList;
}


void GenericWindow::addAnchor( Anchor *pAnchor )
{
    m_anchorList.push_back( pAnchor );
}


void GenericWindow::onControlCapture( const CtrlGeneric &rCtrl )
{
    // Set the capturing control
    m_pCapturingControl = (CtrlGeneric*) &rCtrl;
}


void GenericWindow::onControlRelease( const CtrlGeneric &rCtrl )
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
        m_pTooltip->hide();
        UString tipText = m_pLastHitControl->getTooltipText();
        if( tipText.length() > 0 )
        {
            // Set the tooltip text variable
            VarManager *pVarManager = VarManager::instance( getIntf() );
            pVarManager->getTooltipText().set( tipText );
            m_pTooltip->show();
        }
    }
}


void GenericWindow::onTooltipChange( const CtrlGeneric &rCtrl )
{
    // Check that the control is the active one
    if( m_pLastHitControl && m_pLastHitControl == &rCtrl )
    {
        // Set the tooltip text variable
        VarManager *pVarManager = VarManager::instance( getIntf() );
        pVarManager->getTooltipText().set( rCtrl.getTooltipText() );
    }
}


void GenericWindow::onUpdate( Subject<VarBool> &rVariable )
{
    if( m_varVisible.get() )
    {
        innerShow();
    }
    else
    {
        innerHide();
    }
}


void GenericWindow::innerShow()
{
    // First, refresh the layout and update the shape of the window
    if( m_pActiveLayout )
    {
        updateShape();
        m_pActiveLayout->refreshAll();
    }

    if( m_pOsWindow )
    {
        m_pOsWindow->show( m_left, m_top );
    }
}


void GenericWindow::innerHide()
{
    if( m_pOsWindow )
    {
        m_pOsWindow->hide();
    }
}


CtrlGeneric *GenericWindow::findHitControl( int xPos, int yPos )
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

            // Control hit ?
            if( (*iter).m_pControl->mouseOver( xRel, yRel ) )
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
                m_pTooltip->hide();
                UString tipText = pNewHitControl->getTooltipText();
                if( tipText.length() > 0 )
                {
                    // Set the tooltip text variable
                    VarManager *pVarManager = VarManager::instance( getIntf() );
                    pVarManager->getTooltipText().set( tipText );
                    m_pTooltip->show();
                }
            }
        }
    }

    return pNewHitControl;
}



void GenericWindow::setLastHit( CtrlGeneric *pNewHitControl )
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

