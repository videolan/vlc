/*****************************************************************************
 * window_manager.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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

#include "window_manager.hpp"
#include "generic_layout.hpp"
#include "generic_window.hpp"
#include "os_factory.hpp"
#include "anchor.hpp"
#include "tooltip.hpp"
#include "var_manager.hpp"


WindowManager::WindowManager( intf_thread_t *pIntf ):
    SkinObject( pIntf ), m_magnet( 0 ), m_alpha( 255 ), m_moveAlpha( 255 ),
    m_opacityEnabled( false ), m_opacity( 255 ), m_direction( kNone ),
    m_maximizeRect(0, 0, 50, 50), m_pTooltip( NULL ), m_pPopup( NULL )
{
    // Create and register a variable for the "on top" status
    VarManager *pVarManager = VarManager::instance( getIntf() );
    m_cVarOnTop = VariablePtr( new VarBoolImpl( getIntf() ) );
    pVarManager->registerVar( m_cVarOnTop, "vlc.isOnTop" );

    // transparency switched on or off by user
    m_opacityEnabled = var_InheritBool( getIntf(), "skins2-transparency" );

    // opacity overridden by user
    m_opacity = 255 * var_InheritFloat( getIntf(), "qt-opacity" );
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

    if( isOpacityNeeded() )
    {
        // Change the opacity of the moving windows
        WinSet_t::const_iterator it;
        for( it = m_movingWindows.begin(); it != m_movingWindows.end(); ++it )
        {
            (*it)->setOpacity( m_moveAlpha );
        }
    }
}


void WindowManager::stopMove()
{
    WinSet_t::const_iterator itWin1, itWin2;
    AncList_t::const_iterator itAnc1, itAnc2;

    if( isOpacityNeeded() )
    {
        // Restore the opacity of the moving windows
        WinSet_t::const_iterator it;
        for( it = m_movingWindows.begin(); it != m_movingWindows.end(); ++it )
        {
            (*it)->setOpacity( m_alpha );
        }
    }

    // Delete the dependencies
    m_dependencies.clear();

    // Now we rebuild the dependencies.
    // Iterate through all the windows
    for( itWin1 = m_allWindows.begin(); itWin1 != m_allWindows.end(); ++itWin1 )
    {
        // Get the anchors of the layout associated to the window
        const AncList_t &ancList1 =
            (*itWin1)->getActiveLayout().getAnchorList();

        // Iterate through all the windows, starting with (*itWin1)
        for( itWin2 = itWin1; itWin2 != m_allWindows.end(); ++itWin2 )
        {
            // A window can't anchor itself...
            if( (*itWin2) == (*itWin1) )
                continue;

            // Now, check for anchoring between the 2 windows
            const AncList_t &ancList2 =
                (*itWin2)->getActiveLayout().getAnchorList();
            for( itAnc1 = ancList1.begin(); itAnc1 != ancList1.end(); ++itAnc1 )
            {
                for( itAnc2 = ancList2.begin();
                     itAnc2 != ancList2.end(); ++itAnc2 )
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
    for( it = m_movingWindows.begin(); it != m_movingWindows.end(); ++it )
    {
        (*it)->move( (*it)->getLeft() + xOffset, (*it)->getTop() + yOffset );
    }
}


void WindowManager::startResize( GenericLayout &rLayout, Direction_t direction )
{
    m_direction = direction;

    // Rebuild the set of moving windows.
    // From the resized window, we only take into account the anchors which
    // are mobile with the current type of resizing, and that are hanging a
    // window. The hanged windows will come will all their dependencies.

    m_resizeMovingE.clear();
    m_resizeMovingS.clear();
    m_resizeMovingSE.clear();

    WinSet_t::const_iterator itWin;
    AncList_t::const_iterator itAnc1, itAnc2;
    // Get the anchors of the layout
    const AncList_t &ancList1 = rLayout.getAnchorList();

    // Iterate through all the hanged windows
    for( itWin = m_dependencies[rLayout.getWindow()].begin();
         itWin != m_dependencies[rLayout.getWindow()].end(); ++itWin )
    {
        // Now, check for anchoring between the 2 windows
        const AncList_t &ancList2 =
            (*itWin)->getActiveLayout().getAnchorList();
        for( itAnc1 = ancList1.begin(); itAnc1 != ancList1.end(); ++itAnc1 )
        {
            for( itAnc2 = ancList2.begin();
                 itAnc2 != ancList2.end(); ++itAnc2 )
            {
                if( (*itAnc1)->isHanging( **itAnc2 ) )
                {
                    // Add the dependencies of the hanged window to one of the
                    // lists of moving windows
                    Position::Ref_t aRefPos =
                        (*itAnc1)->getPosition().getRefLeftTop();
                    if( aRefPos == Position::kRightTop )
                        buildDependSet( m_resizeMovingE, *itWin );
                    else if( aRefPos == Position::kLeftBottom )
                        buildDependSet( m_resizeMovingS, *itWin );
                    else if( aRefPos == Position::kRightBottom )
                        buildDependSet( m_resizeMovingSE, *itWin );
                    break;
                }
            }
        }
    }

    // The checkAnchors() method will need to have m_movingWindows properly set
    // so let's insert in it the contents of the other sets
    m_movingWindows.clear();
    m_movingWindows.insert( rLayout.getWindow() );
    m_movingWindows.insert( m_resizeMovingE.begin(), m_resizeMovingE.end() );
    m_movingWindows.insert( m_resizeMovingS.begin(), m_resizeMovingS.end() );
    m_movingWindows.insert( m_resizeMovingSE.begin(), m_resizeMovingSE.end() );
}


void WindowManager::stopResize()
{
    // Nothing different from stopMove(), luckily
    stopMove();
}


void WindowManager::resize( GenericLayout &rLayout,
                            int width, int height ) const
{
    // TODO: handle anchored windows
    // Compute the real resizing offset
    int xOffset = width - rLayout.getWidth();
    int yOffset = height - rLayout.getHeight();

    // Check anchoring; this can change the values of xOffset and yOffset
    checkAnchors( rLayout.getWindow(), xOffset, yOffset );
    if( m_direction == kResizeS )
        xOffset = 0;
    if( m_direction == kResizeE )
        yOffset = 0;

    int newWidth = rLayout.getWidth() + xOffset;
    int newHeight = rLayout.getHeight() + yOffset;

    // Check boundaries
    if( newWidth < rLayout.getMinWidth() )
    {
        newWidth = rLayout.getMinWidth();
    }
    if( newWidth > rLayout.getMaxWidth() )
    {
        newWidth = rLayout.getMaxWidth();
    }
    if( newHeight < rLayout.getMinHeight() )
    {
        newHeight = rLayout.getMinHeight();
    }
    if( newHeight > rLayout.getMaxHeight() )
    {
        newHeight = rLayout.getMaxHeight();
    }

    if( newWidth == rLayout.getWidth() && newHeight == rLayout.getHeight() )
    {
        return;
    }

    // New offset, after the last corrections
    int xNewOffset = newWidth - rLayout.getWidth();
    int yNewOffset = newHeight - rLayout.getHeight();

    // Resize the window
    TopWindow *pWindow = rLayout.getWindow();
    pWindow->resize( newWidth, newHeight );

    // Do the actual resizing
    rLayout.resize( newWidth, newHeight );

    // refresh content
    rLayout.refreshAll();

    // Move all the anchored windows
    WinSet_t::const_iterator it;
    if( m_direction == kResizeE ||
        m_direction == kResizeSE )
    {
        for( it = m_resizeMovingE.begin(); it != m_resizeMovingE.end(); ++it )
        {
            (*it)->move( (*it)->getLeft() + xNewOffset,
                         (*it)->getTop() );
        }
    }
    if( m_direction == kResizeS ||
        m_direction == kResizeSE )
    {
        for( it = m_resizeMovingS.begin(); it != m_resizeMovingS.end(); ++it )
        {
            (*it)->move( (*it)->getLeft(),
                         (*it)->getTop( )+ yNewOffset );
        }
    }
    if( m_direction == kResizeE ||
        m_direction == kResizeS ||
        m_direction == kResizeSE )
    {
        for( it = m_resizeMovingSE.begin(); it != m_resizeMovingSE.end(); ++it )
        {
            (*it)->move( (*it)->getLeft() + xNewOffset,
                         (*it)->getTop() + yNewOffset );
        }
    }
}


void WindowManager::maximize( TopWindow &rWindow )
{
    // Save the current position/size of the window, to be able to restore it
    m_maximizeRect = SkinsRect( rWindow.getLeft(), rWindow.getTop(),
                               rWindow.getLeft() + rWindow.getWidth(),
                               rWindow.getTop() + rWindow.getHeight() );

    // maximise the window within the current screen (multiple screens allowed)
    SkinsRect workArea = OSFactory::instance( getIntf() )->getWorkArea();

    // Move the window
    startMove( rWindow );
    move( rWindow, workArea.getLeft(), workArea.getTop() );
    stopMove();
    // Now resize it
    // FIXME: Ugly const_cast
    GenericLayout &rLayout = (GenericLayout&)rWindow.getActiveLayout();
    startResize( rLayout, kResizeSE );
    resize( rLayout, workArea.getWidth(), workArea.getHeight() );
    stopResize();
    rWindow.m_pVarMaximized->set( true );

    // Make the window unmovable by unregistering it
//     unregisterWindow( rWindow );
}


void WindowManager::unmaximize( TopWindow &rWindow )
{
    // Register the window to allow moving it
//     registerWindow( rWindow );

    // Resize the window
    // FIXME: Ugly const_cast
    GenericLayout &rLayout = (GenericLayout&)rWindow.getActiveLayout();
    startResize( rLayout, kResizeSE );
    resize( rLayout, m_maximizeRect.getWidth(), m_maximizeRect.getHeight() );
    stopResize();
    // Now move it
    startMove( rWindow );
    move( rWindow, m_maximizeRect.getLeft(), m_maximizeRect.getTop() );
    stopMove();
    rWindow.m_pVarMaximized->set( false );
}


void WindowManager::synchVisibility() const
{
    WinSet_t::const_iterator it;
    for( it = m_allWindows.begin(); it != m_allWindows.end(); ++it )
    {
        // Show the window if it has to be visible
        if( (*it)->getVisibleVar().get() )
        {
            (*it)->innerShow();
        }
    }
}


void WindowManager::saveVisibility()
{
    WinSet_t::const_iterator it;
    m_savedWindows.clear();
    for( it = m_allWindows.begin(); it != m_allWindows.end(); ++it )
    {
        // Remember the window if it is visible
        if( (*it)->getVisibleVar().get() )
        {
            m_savedWindows.insert( *it );
        }
    }
}


void WindowManager::restoreVisibility() const
{
    // Warning in case we never called saveVisibility()
    if( m_savedWindows.size() == 0 )
    {
        msg_Warn( getIntf(), "restoring visibility for no window" );
    }

    WinSet_t::const_iterator it;
    for( it = m_savedWindows.begin(); it != m_savedWindows.end(); ++it )
    {
        (*it)->show();
    }
}


void WindowManager::raiseAll() const
{
    // Raise all the windows
    WinSet_t::const_iterator it;
    for( it = m_allWindows.begin(); it != m_allWindows.end(); ++it )
    {
        (*it)->raise();
    }
}


void WindowManager::showAll( bool firstTime ) const
{
    // Show all the windows
    WinSet_t::const_iterator it;
    for( it = m_allWindows.begin(); it != m_allWindows.end(); ++it )
    {
        // When the theme is opened for the first time,
        // only show the window if set as visible in the XML
        if( (*it)->getInitialVisibility() || !firstTime )
        {
            (*it)->show();
        }
    }
}


void WindowManager::show( TopWindow &rWindow ) const
{
    if( isOpacityNeeded() )
        rWindow.setOpacity( m_alpha );

    rWindow.show();
}


void WindowManager::hideAll() const
{
    WinSet_t::const_iterator it;
    for( it = m_allWindows.begin(); it != m_allWindows.end(); ++it )
    {
        (*it)->hide();
    }
}


void WindowManager::setOnTop( bool b_ontop )
{
    // Update the boolean variable
    VarBoolImpl *pVarOnTop = (VarBoolImpl*)m_cVarOnTop.get();
    pVarOnTop->set( b_ontop );

    // set/unset the "on top" status
    WinSet_t::const_iterator it;
    for( it = m_allWindows.begin(); it != m_allWindows.end(); ++it )
    {
        (*it)->toggleOnTop( b_ontop );
    }
}


void WindowManager::toggleOnTop()
{
    VarBoolImpl *pVarOnTop = (VarBoolImpl*)m_cVarOnTop.get();

    setOnTop( !pVarOnTop->get() );
}


void WindowManager::buildDependSet( WinSet_t &rWinSet,
                                    TopWindow *pWindow )
{
    // pWindow is in the set
    rWinSet.insert( pWindow );

    // Iterate through the anchored windows
    const WinSet_t &anchored = m_dependencies[pWindow];
    WinSet_t::const_iterator iter;
    for( iter = anchored.begin(); iter != anchored.end(); ++iter )
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
    (void)pWindow;
    WinSet_t::const_iterator itMov, itSta;
    AncList_t::const_iterator itAncMov, itAncSta;

    // Check magnetism with screen edges first (actually it is the work area)
    SkinsRect workArea = OSFactory::instance( getIntf() )->getWorkArea();
    // Iterate through the moving windows
    for( itMov = m_movingWindows.begin();
         itMov != m_movingWindows.end(); ++itMov )
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
        int right = workArea.getLeft() + workArea.getWidth();
        if( newLeft + (*itMov)->getWidth() > right - m_magnet &&
            newLeft + (*itMov)->getWidth() < right + m_magnet )
        {
            xOffset = right - (*itMov)->getLeft() - (*itMov)->getWidth();
        }
        int bottom = workArea.getTop() + workArea.getHeight();
        if( newTop + (*itMov)->getHeight() > bottom - m_magnet &&
            newTop + (*itMov)->getHeight() <  bottom + m_magnet )
        {
            yOffset =  bottom - (*itMov)->getTop() - (*itMov)->getHeight();
        }
    }

    // Iterate through the moving windows
    for( itMov = m_movingWindows.begin();
         itMov != m_movingWindows.end(); ++itMov )
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
             itSta != m_allWindows.end(); ++itSta )
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
                 itAncMov != movAnchors.end(); ++itAncMov )
            {
                for( itAncSta = staAnchors.begin();
                     itAncSta != staAnchors.end(); ++itAncSta )
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
        msg_Warn( getIntf(), "tooltip already created!" );
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

