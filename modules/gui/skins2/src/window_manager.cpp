/*****************************************************************************
 * window_manager.cpp
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include "window_manager.hpp"
#include "generic_layout.hpp"
#include "generic_window.hpp"
#include "os_factory.hpp"
#include "anchor.hpp"
#include "tooltip.hpp"
#include "../utils/position.hpp"
#include "../src/var_manager.hpp"


WindowManager::WindowManager( intf_thread_t *pIntf ):
    SkinObject( pIntf ), m_magnet( 0 ), m_pTooltip( NULL )
{
    // Create and register a variable for the "on top" status
    VarManager *pVarManager = VarManager::instance( getIntf() );
    m_cVarOnTop = VariablePtr( new VarBoolImpl( getIntf() ) );
    pVarManager->registerVar( m_cVarOnTop, "vlc.isOnTop" );
}


WindowManager::~WindowManager()
{
    delete m_pTooltip;
}


void WindowManager::registerWindow( TopWindow &rWindow )
{
    // Add the window to the set
    m_allWindows.insert( &rWindow );
}


void WindowManager::unregisterWindow( TopWindow &rWindow )
{
    // Erase every possible reference to the window
    m_allWindows.erase( &rWindow );
    m_movingWindows.erase( &rWindow );
    m_dependencies.erase( &rWindow );
}


void WindowManager::startMove( TopWindow &rWindow )
{
    // Rebuild the set of moving windows
    m_movingWindows.clear();
    buildDependSet( m_movingWindows, &rWindow );

#ifdef WIN32
    if( config_GetInt( getIntf(), "skins2-transparency" ) )
    {
        // Change the opacity of the moving windows
        WinSet_t::const_iterator it;
        for( it = m_movingWindows.begin(); it != m_movingWindows.end(); it++ )
        {
            (*it)->setOpacity( m_moveAlpha );
        }

        // FIXME: We need to refresh the windows, because if 2 windows overlap
        // and one of them becomes transparent, the other one is not refreshed
        // automatically. I don't know why... -- Ipkiss
        for( it = m_allWindows.begin(); it != m_allWindows.end(); it++ )
        {
            (*it)->refresh( 0, 0, (*it)->getWidth(), (*it)->getHeight() );
        }
    }
#endif
}


void WindowManager::stopMove()
{
    WinSet_t::const_iterator itWin1, itWin2;
    AncList_t::const_iterator itAnc1, itAnc2;

#ifdef WIN32
    if( config_GetInt( getIntf(), "skins2-transparency" ) )
    {
        // Restore the opacity of the moving windows
        WinSet_t::const_iterator it;
        for( it = m_movingWindows.begin(); it != m_movingWindows.end(); it++ )
        {
            (*it)->setOpacity( m_alpha );
        }
    }
#endif

    // Delete the dependencies
    m_dependencies.clear();

    // Now we rebuild the dependencies.
    // Iterate through all the windows
    for( itWin1 = m_allWindows.begin(); itWin1 != m_allWindows.end(); itWin1++ )
    {
        // Get the anchors of the layout associated to the window
        const AncList_t &ancList1 =
            (*itWin1)->getActiveLayout().getAnchorList();

        // Iterate through all the windows, starting with (*itWin1)
        for( itWin2 = itWin1; itWin2 != m_allWindows.end(); itWin2++ )
        {
            // A window can't anchor itself...
            if( (*itWin2) == (*itWin1) )
                continue;

            // Now, check for anchoring between the 2 windows
            const AncList_t &ancList2 =
                (*itWin2)->getActiveLayout().getAnchorList();
            for( itAnc1 = ancList1.begin(); itAnc1 != ancList1.end(); itAnc1++ )
            {
                for( itAnc2 = ancList2.begin();
                     itAnc2 != ancList2.end(); itAnc2++ )
                {
                    if( (*itAnc1)->isHanging( **itAnc2 ) )
                    {
                        // (*itWin1) anchors (*itWin2)
                        m_dependencies[*itWin1].insert( *itWin2 );
                    }
                    else if( (*itAnc2)->isHanging( **itAnc1 ) )
                    {
                        // (*itWin2) anchors (*itWin1)
                        m_dependencies[*itWin2].insert( *itWin1 );
                    }
                }
            }
        }
    }
}


void WindowManager::move( TopWindow &rWindow, int left, int top ) const
{
    // Compute the real move offset
    int xOffset = left - rWindow.getLeft();
    int yOffset = top - rWindow.getTop();

    // Check anchoring; this can change the values of xOffset and yOffset
    checkAnchors( &rWindow, xOffset, yOffset );

    // Move all the windows
    WinSet_t::const_iterator it;
    for( it = m_movingWindows.begin(); it != m_movingWindows.end(); it++ )
    {
        (*it)->move( (*it)->getLeft() + xOffset, (*it)->getTop() + yOffset );
    }
}


void WindowManager::synchVisibility() const
{
    WinSet_t::const_iterator it;
    for( it = m_allWindows.begin(); it != m_allWindows.end(); it++ )
    {
        // Show the window if it has to be visible
        if( (*it)->getVisibleVar().get() )
        {
            (*it)->innerShow();
        }
    }
}


void WindowManager::raiseAll() const
{
    // Raise all the windows
    WinSet_t::const_iterator it;
    for( it = m_allWindows.begin(); it != m_allWindows.end(); it++ )
    {
        (*it)->raise();
    }
}


void WindowManager::showAll( bool firstTime ) const
{
    // Show all the windows
    WinSet_t::const_iterator it;
    for( it = m_allWindows.begin(); it != m_allWindows.end(); it++ )
    {
        // When the theme is opened for the first time,
        // only show the window if set as visible in the XML
        if ((*it)->isVisible() || !firstTime)
        {
            (*it)->show();
        }
        (*it)->setOpacity( m_alpha );
    }
}


void WindowManager::hideAll() const
{
    WinSet_t::const_iterator it;
    for( it = m_allWindows.begin(); it != m_allWindows.end(); it++ )
    {
        (*it)->hide();
    }
}


void WindowManager::toggleOnTop()
{
    // Update the boolean variable
    VarBoolImpl *pVarOnTop = (VarBoolImpl*)m_cVarOnTop.get();
    pVarOnTop->set( !pVarOnTop->get() );

    // Toggle the "on top" status
    WinSet_t::const_iterator it;
    for( it = m_allWindows.begin(); it != m_allWindows.end(); it++ )
    {
        (*it)->toggleOnTop( pVarOnTop->get() );
    }
}


void WindowManager::buildDependSet( WinSet_t &rWinSet,
                                    TopWindow *pWindow )
{
    // pWindow is in the set
    rWinSet.insert( pWindow );

    // Iterate through the anchored windows
    const WinSet_t &anchored = m_dependencies[pWindow];
    WinSet_t::const_iterator iter;
    for( iter = anchored.begin(); iter != anchored.end(); iter++ )
    {
        // Check that the window isn't already in the set before adding it
        if( rWinSet.find( *iter ) == rWinSet.end() )
        {
            buildDependSet( rWinSet, *iter );
        }
    }
}


void WindowManager::checkAnchors( TopWindow *pWindow,
                                  int &xOffset, int &yOffset ) const
{
    WinSet_t::const_iterator itMov, itSta;
    AncList_t::const_iterator itAncMov, itAncSta;

    // Check magnetism with screen edges first (actually it is the work area)
    Rect workArea = OSFactory::instance( getIntf() )->getWorkArea();
    // Iterate through the moving windows
    for( itMov = m_movingWindows.begin();
         itMov != m_movingWindows.end(); itMov++ )
    {
        // Skip the invisible windows
        if( ! (*itMov)->getVisibleVar().get() )
        {
            continue;
        }

        int newLeft = (*itMov)->getLeft() + xOffset;
        int newTop = (*itMov)->getTop() + yOffset;
        if( newLeft > workArea.getLeft() - m_magnet &&
            newLeft < workArea.getLeft() + m_magnet )
        {
            xOffset = workArea.getLeft() - (*itMov)->getLeft();
        }
        if( newTop > workArea.getTop() - m_magnet &&
            newTop < workArea.getTop() + m_magnet )
        {
            yOffset = workArea.getTop() - (*itMov)->getTop();
        }
        if( newLeft + (*itMov)->getWidth() > workArea.getRight() - m_magnet &&
            newLeft + (*itMov)->getWidth() < workArea.getRight() + m_magnet )
        {
            xOffset = workArea.getRight() - (*itMov)->getLeft()
                      - (*itMov)->getWidth();
        }
        if( newTop + (*itMov)->getHeight() > workArea.getBottom() - m_magnet &&
            newTop + (*itMov)->getHeight() <  workArea.getBottom() + m_magnet )
        {
            yOffset =  workArea.getBottom() - (*itMov)->getTop()
                       - (*itMov)->getHeight();
        }
    }

    // Iterate through the moving windows
    for( itMov = m_movingWindows.begin();
         itMov != m_movingWindows.end(); itMov++ )
    {
        // Skip the invisible windows
        if( ! (*itMov)->getVisibleVar().get() )
        {
            continue;
        }

        // Get the anchors in the main layout of this moving window
        const AncList_t &movAnchors =
            (*itMov)->getActiveLayout().getAnchorList();

        // Iterate through the static windows
        for( itSta = m_allWindows.begin();
             itSta != m_allWindows.end(); itSta++ )
        {
            // Skip the moving windows and the invisible ones
            if( m_movingWindows.find( (*itSta) ) != m_movingWindows.end() ||
                ! (*itSta)->getVisibleVar().get() )
            {
                continue;
            }

            // Get the anchors in the main layout of this static window
            const AncList_t &staAnchors =
                (*itSta)->getActiveLayout().getAnchorList();

            // Check if there is an anchoring between one of the movAnchors
            // and one of the staAnchors
            for( itAncMov = movAnchors.begin();
                 itAncMov != movAnchors.end(); itAncMov++ )
            {
                for( itAncSta = staAnchors.begin();
                     itAncSta != staAnchors.end(); itAncSta++ )
                {
                    if( (*itAncSta)->canHang( **itAncMov, xOffset, yOffset ) )
                    {
                        // We have found an anchoring!
                        // There is nothing to do here, since xOffset and
                        // yOffset are automatically modified by canHang()

                        // Don't check the other anchors, one is enough...
                        return;
                    }
                    else
                    {
                        // Temporary variables
                        int xOffsetTemp = -xOffset;
                        int yOffsetTemp = -yOffset;
                        if( (*itAncMov)->canHang( **itAncSta, xOffsetTemp,
                                                  yOffsetTemp ) )
                        {
                            // We have found an anchoring!
                            // xOffsetTemp and yOffsetTemp have been updated,
                            // we just need to change xOffset and yOffset
                            xOffset = -xOffsetTemp;
                            yOffset = -yOffsetTemp;

                            // Don't check the other anchors, one is enough...
                            return;
                        }
                    }
                }
            }
        }
    }
}


void WindowManager::createTooltip( const GenericFont &rTipFont )
{
    // Create the tooltip window
    if( !m_pTooltip )
    {
        m_pTooltip = new Tooltip( getIntf(), rTipFont, 500 );
    }
    else
    {
        msg_Warn( getIntf(), "Tooltip already created!");
    }
}


void WindowManager::showTooltip()
{
    if( m_pTooltip )
    {
        m_pTooltip->show();
    }
}


void WindowManager::hideTooltip()
{
    if( m_pTooltip )
    {
        m_pTooltip->hide();
    }
}


void WindowManager::addLayout( TopWindow &rWindow, GenericLayout &rLayout )
{
    rWindow.setActiveLayout( &rLayout );
}


void WindowManager::setActiveLayout( TopWindow &rWindow,
                                     GenericLayout &rLayout )
{
    rWindow.setActiveLayout( &rLayout );
    // Rebuild the dependencies
    stopMove();
}
