/*****************************************************************************
 * ctrl_generic.cpp
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

#include "ctrl_generic.hpp"
#include "../src/generic_layout.hpp"
#include "../src/top_window.hpp"
#include "../src/os_graphics.hpp"
#include "../utils/position.hpp"
#include "../utils/var_bool.hpp"

#include <assert.h>


CtrlGeneric::CtrlGeneric( intf_thread_t *pIntf, const UString &rHelp,
                          VarBool *pVisible):
    SkinObject( pIntf ), m_pLayout( NULL ), m_pVisible( pVisible ),
    m_pPosition( NULL ), m_help( rHelp )
{
    // Observe the visibility variable
    if( m_pVisible )
    {
        m_pVisible->addObserver( this );
    }
}


CtrlGeneric::~CtrlGeneric()
{
    if( m_pVisible )
    {
        m_pVisible->delObserver( this );
    }
}


void CtrlGeneric::setLayout( GenericLayout *pLayout,
                             const Position &rPosition )
{
    assert( !m_pLayout && pLayout);

    m_pLayout = pLayout;
    m_pPosition = new Position( rPosition );
    onPositionChange();
}

void CtrlGeneric::unsetLayout()
{
    assert( m_pLayout );

    delete m_pPosition;
    m_pPosition = NULL;
    m_pLayout = NULL;
}

void CtrlGeneric::notifyLayout( int width, int height,
                                int xOffSet, int yOffSet )
{
    // Notify the layout
    if( m_pLayout )
    {
        width = ( width > 0 ) ? width : m_pPosition->getWidth();
        height = ( height > 0 ) ? height : m_pPosition->getHeight();

        m_pLayout->onControlUpdate( *this, width, height, xOffSet, yOffSet );
    }
}


void CtrlGeneric::notifyLayoutMaxSize( const Box *pImg1, const Box *pImg2 )
{
    if( pImg1 == NULL )
    {
        if( pImg2 == NULL )
        {
            notifyLayout();
        }
        else
        {
            notifyLayout( pImg2->getWidth(), pImg2->getHeight() );
        }
    }
    else
    {
        if( pImg2 == NULL )
        {
            notifyLayout( pImg1->getWidth(), pImg1->getHeight() );
        }
        else
        {
            notifyLayout( max( pImg1->getWidth(), pImg2->getWidth() ),
                          max( pImg1->getHeight(), pImg2->getHeight() ) );
        }
    }
}


void CtrlGeneric::captureMouse() const
{
    // Tell the layout we want to capture the mouse
    if( m_pLayout )
    {
        m_pLayout->onControlCapture( *this );
    }
}


void CtrlGeneric::releaseMouse() const
{
    // Tell the layout we want to release the mouse
    if( m_pLayout )
    {
        m_pLayout->onControlRelease( *this );
    }
}


void CtrlGeneric::notifyTooltipChange() const
{
    TopWindow *pWin = getWindow();
    if( pWin )
    {
        // Notify the window
        pWin->onTooltipChange( *this );
    }
}


TopWindow *CtrlGeneric::getWindow() const
{
    if( m_pLayout )
    {
        return m_pLayout->getWindow();
    }
    return NULL;
}


bool CtrlGeneric::isVisible() const
{
    return !m_pVisible || m_pVisible->get();
}


void CtrlGeneric::onUpdate( Subject<VarBool> &rVariable, void *arg  )
{
    (void)arg;
    // Is it the visibility variable ?
    if( &rVariable == m_pVisible )
    {
        // Redraw the layout
        notifyLayout();
    }
    else
    {
        // Call the user-defined callback
        onVarBoolUpdate( (VarBool&)rVariable );
    }
}

