/*****************************************************************************
 * window_manager.cpp
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

#include "window_manager.hpp"
#include "generic_window.hpp"
#include "os_factory.hpp"
#include "anchor.hpp"
#include "tooltip.hpp"
#include "../utils/position.hpp"


WindowManager::WindowManager( intf_thread_t *pIntf ):
    SkinObject( pIntf ), m_isOnTop( false ), m_magnet( 0 ), m_pTooltip( NULL )
{
}


WindowManager::~WindowManager()
{
    delete m_pTooltip;
}


void WindowManager::registerWindow( GenericWindow &rWindow )
{
    // Add the window to the set
    m_allWindows.insert( &rWindow );
}


void WindowManager::unregisterWindow( GenericWindow &rWindow )
{
    // Erase every possible reference to the window
    m_allWindows.erase( &rWindow );
    m_movingWindows.erase( &rWindow );
    m_dependencies.erase( &rWindow );
}


void WindowManager::startMove( GenericWindow &rWindow )
{
    // Rebuild the set of moving windows
    m_movingWindows.clear();
    buildDependSet( m_movingWindows, &rWindow );

    // Change the opacity of the moving windows
    WinSet_t::const_iterator it;
    for( it = m_movingWindows.begin(); it != m_movingWindows.end(); it++ )
    {
        (*it)->setOpacity( m_moveAlpha );
    }
}


void WindowManager::stopMove()
{
    WinSet_t::const_iterator itWin1, itWin2;
    AncList_t::const_iterator itAnc1, itAnc2;

    // Restore the opacity of the moving windows
    WinSet_t::const_iterator it;
    for( it = m_movingWindows.begin(); it != m_movingWindows.end(); it++ )
    {
        (*it)->setOpacity( m_alpha );
    }

    // Delete the dependencies
    m_dependencies.clear();

    // Now we rebuild the dependencies.
    // Iterate through all the windows
    for( itWin1 = m_allWindows.begin(); itWin1 != m_allWindows.end(); itWin1++ )
    {
        // Get the anchors of the window
        const AncList_t &ancList1 = (*itWin1)->getAnchorList();

        // Iterate through all the windows, starting with (*itWin1)
        for( itWin2 = itWin1; itWin2 != m_allWindows.end(); itWin2++ )
        {
            // A window can't anchor itself...
            if( (*itWin2) == (*itWin1) )
                continue;

            // Now, check for anchoring between the 2 windows
            const AncList_t &ancList2 = (*itWin2)->getAnchorList();
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


void WindowManager::move( GenericWindow &rWindow, int left, int top ) const
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


void WindowManager::raiseAll( GenericWindow &rWindow ) const
{
    // Raise all the windows
    WinSet_t::const_iterator it;
    for( it = m_allWindows.begin(); it != m_allWindows.end(); it++ )
    {
        if( *it !=  &rWindow )
        {
            (*it)->raise();
        }
    }
    // Make sure to raise the given window at the end, so that it is above
    rWindow.raise();
}


void WindowManager::showAll() const
{
    // Show all the windows
    WinSet_t::const_iterator it;
    for( it = m_allWindows.begin(); it != m_allWindows.end(); it++ )
    {
        (*it)->show();
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
    m_isOnTop = !m_isOnTop;
    WinSet_t::const_iterator it;
    for( it = m_allWindows.begin(); it != m_allWindows.end(); it++ )
    {
        (*it)->toggleOnTop( m_isOnTop );
    }
}


void WindowManager::buildDependSet( WinSet_t &rWinSet,
                                    GenericWindow *pWindow )
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


void WindowManager::checkAnchors( GenericWindow *pWindow,
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

        // Get the anchors of this moving window
        const AncList_t &movAnchors = (*itMov)->getAnchorList();

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

            // Get the anchors of this static window
            const AncList_t &staAnchors = (*itSta)->getAnchorList();

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
