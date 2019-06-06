/*****************************************************************************
 * ctrl_list.cpp
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

#include <math.h>
#include "ctrl_list.hpp"
#include "../src/os_factory.hpp"
#include "../src/os_graphics.hpp"
#include "../src/generic_bitmap.hpp"
#include "../src/generic_font.hpp"
#include "../src/scaled_bitmap.hpp"
#include "../utils/position.hpp"
#include "../utils/ustring.hpp"
#include "../events/evt_key.hpp"
#include "../events/evt_mouse.hpp"
#include "../events/evt_scroll.hpp"
#include <vlc_actions.h>

#define SCROLL_STEP 0.05f
#define LINE_INTERVAL 1  // Number of pixels inserted between 2 lines


CtrlList::CtrlList( intf_thread_t *pIntf, VarList &rList,
                    const GenericFont &rFont, const GenericBitmap *pBitmap,
                    uint32_t fgColor, uint32_t playColor, uint32_t bgColor1,
                    uint32_t bgColor2, uint32_t selColor,
                    const UString &rHelp, VarBool *pVisible ):
    CtrlGeneric( pIntf, rHelp, pVisible ), m_rList( rList ), m_rFont( rFont ),
    m_pBitmap( pBitmap ), m_fgColor( fgColor ), m_playColor( playColor ),
    m_bgColor1( bgColor1 ), m_bgColor2( bgColor2 ), m_selColor( selColor ),
    m_pLastSelected( NULL ), m_pImage( NULL ), m_lastPos( 0 )
{
    // Observe the list and position variables
    m_rList.addObserver( this );
    m_rList.getPositionVar().addObserver( this );

    makeImage();
}


CtrlList::~CtrlList()
{
    m_rList.getPositionVar().delObserver( this );
    m_rList.delObserver( this );
    delete m_pImage;
}


void CtrlList::onUpdate( Subject<VarList> &rList, void *arg  )
{
    (void)rList; (void)arg;
    autoScroll();
    m_pLastSelected = NULL;
}


void CtrlList::onUpdate( Subject<VarPercent> &rPercent, void *arg  )
{
    (void)rPercent; (void)arg;
    // Get the size of the control
    const Position *pPos = getPosition();
    if( !pPos )
        return;

    int height = pPos->getHeight();

    // How many lines can be displayed ?
    int itemHeight = m_rFont.getSize() + LINE_INTERVAL;
    int maxItems = height / itemHeight;

    // Determine what is the first item to display
    VarPercent &rVarPos = m_rList.getPositionVar();
    int firstItem = 0;
    int excessItems = m_rList.size() - maxItems;
    if( excessItems > 0 )
    {
        // a simple (int)(...) causes rounding errors !
#ifdef _MSC_VER
#   define lrint (int)
#endif
        firstItem = lrint( (1.0 - rVarPos.get()) * (double)excessItems );
    }
    if( m_lastPos != firstItem )
    {
        // Redraw the control if the position has changed
        m_lastPos = firstItem;
        makeImage();
        notifyLayout();
    }
}


void CtrlList::onResize()
{
    // Get the size of the control
    const Position *pPos = getPosition();
    if( !pPos )
        return;

    int height = pPos->getHeight();

    // How many lines can be displayed ?
    int itemHeight = m_rFont.getSize() + LINE_INTERVAL;
    int maxItems = height / itemHeight;

    // Update the position variable
    VarPercent &rVarPos = m_rList.getPositionVar();
    int excessItems = m_rList.size() - maxItems;
    if( excessItems > 0 )
    {
        double newVal = 1.0 - (double)m_lastPos / excessItems;
        if( newVal >= 0 )
        {
            // Change the position to keep the same first displayed item
            rVarPos.set( 1.0 - (double)m_lastPos / excessItems );
        }
        else
        {
            // We cannot keep the current first item
            m_lastPos = excessItems;
        }
    }

    makeImage();
}


void CtrlList::onPositionChange()
{
    makeImage();
}


void CtrlList::handleEvent( EvtGeneric &rEvent )
{
    if( rEvent.getAsString().find( "key:down" ) != std::string::npos )
    {
        int key = ((EvtKey&)rEvent).getKey();
        VarList::Iterator it = m_rList.begin();
        bool previousWasSelected = false;
        while( it != m_rList.end() )
        {
            VarList::Iterator next = it;
            ++next;
            if( key == KEY_UP )
            {
                // Scroll up one item
                if( it != m_rList.begin() || &*it != m_pLastSelected )
                {
                    bool nextWasSelected = ( &*next == m_pLastSelected );
                    (*it).m_selected = nextWasSelected;
                    if( nextWasSelected )
                    {
                        m_pLastSelected = &*it;
                    }
                }
            }
            else if( key == KEY_DOWN )
            {
                // Scroll down one item
                if( next != m_rList.end() || &*it != m_pLastSelected )
                {
                    (*it).m_selected = previousWasSelected;
                }
                if( previousWasSelected )
                {
                    m_pLastSelected = &*it;
                    previousWasSelected = false;
                }
                else
                {
                    previousWasSelected = ( &*it == m_pLastSelected );
                }
            }
            it = next;
        }

        // Redraw the control
        makeImage();
        notifyLayout();
    }

    else if( rEvent.getAsString().find( "mouse:left" ) != std::string::npos )
    {
        EvtMouse &rEvtMouse = (EvtMouse&)rEvent;
        const Position *pos = getPosition();
        int yPos = m_lastPos + ( rEvtMouse.getYPos() - pos->getTop() ) /
            (m_rFont.getSize() + LINE_INTERVAL);
        VarList::Iterator it;
        int index = 0;

        if( rEvent.getAsString().find( "mouse:left:down:ctrl,shift" ) !=
                 std::string::npos )
        {
            // Flag to know if the current item must be selected
            bool select = false;
            for( it = m_rList.begin(); it != m_rList.end(); ++it )
            {
                bool nextSelect = select;
                if( index == yPos || &*it == m_pLastSelected )
                {
                    if( select )
                    {
                        nextSelect = false;
                    }
                    else
                    {
                        select = true;
                        nextSelect = true;
                    }
                }
                (*it).m_selected = (*it).m_selected || select;
                select = nextSelect;
                index++;
            }
        }

        else if( rEvent.getAsString().find( "mouse:left:down:ctrl" ) !=
                 std::string::npos )
        {
            for( it = m_rList.begin(); it != m_rList.end(); ++it )
            {
                if( index == yPos )
                {
                    (*it).m_selected = ! (*it).m_selected;
                    m_pLastSelected = &*it;
                    break;
                }
                index++;
            }
        }

        else if( rEvent.getAsString().find( "mouse:left:down:shift" ) !=
                 std::string::npos )
        {
            // Flag to know if the current item must be selected
            bool select = false;
            for( it = m_rList.begin(); it != m_rList.end(); ++it )
            {
                bool nextSelect = select;
                if( index == yPos ||  &*it == m_pLastSelected )
                {
                    if( select )
                    {
                        nextSelect = false;
                    }
                    else
                    {
                        select = true;
                        nextSelect = true;
                    }
                }
                (*it).m_selected = select;
                select = nextSelect;
                index++;
            }
        }

        else if( rEvent.getAsString().find( "mouse:left:down" ) !=
                 std::string::npos )
        {
            for( it = m_rList.begin(); it != m_rList.end(); ++it )
            {
                if( index == yPos )
                {
                    (*it).m_selected = true;
                    m_pLastSelected = &*it;
                }
                else
                {
                    (*it).m_selected = false;
                }
                index++;
            }
        }

        else if( rEvent.getAsString().find( "mouse:left:dblclick" ) !=
                 std::string::npos )
        {
            for( it = m_rList.begin(); it != m_rList.end(); ++it )
            {
                if( index == yPos )
                {
                    (*it).m_selected = true;
                    m_pLastSelected = &*it;
                    // Execute the action associated to this item
                    m_rList.action( &*it );
                }
                else
                {
                    (*it).m_selected = false;
                }
                index++;
            }
        }

        // Redraw the control
        makeImage();
        notifyLayout();
    }

    else if( rEvent.getAsString().find( "scroll" ) != std::string::npos )
    {
        int direction = ((EvtScroll&)rEvent).getDirection();

        double percentage = m_rList.getPositionVar().get();
        double step = 2.0 / (double)m_rList.size();
        if( direction == EvtScroll::kUp )
        {
            percentage += step;
        }
        else
        {
            percentage -= step;
        }
        m_rList.getPositionVar().set( percentage );
    }
}


bool CtrlList::mouseOver( int x, int y ) const
{
    const Position *pPos = getPosition();
    if( pPos )
    {
        int width = pPos->getWidth();
        int height = pPos->getHeight();
        return ( x >= 0 && x <= width && y >= 0 && y <= height );
    }
    return false;
}


void CtrlList::draw( OSGraphics &rImage, int xDest, int yDest, int w, int h )
{
    const Position *pPos = getPosition();
    rect region( pPos->getLeft(), pPos->getTop(),
                 pPos->getWidth(), pPos->getHeight() );
    rect clip( xDest, yDest, w, h );
    rect inter;
    if( rect::intersect( region, clip, &inter ) && m_pImage )
    {
        rImage.drawGraphics( *m_pImage,
                      inter.x - pPos->getLeft(),
                      inter.y - pPos->getTop(),
                      inter.x, inter.y, inter.width, inter.height );
    }
}


void CtrlList::autoScroll()
{
    // Get the size of the control
    const Position *pPos = getPosition();
    if( !pPos )
    {
        return;
    }
    int height = pPos->getHeight();

    // How many lines can be displayed ?
    int itemHeight = m_rFont.getSize() + LINE_INTERVAL;
    int maxItems = height / itemHeight;

    // Find the current playing stream
    int playIndex = 0;
    VarList::ConstIterator it;
    for( it = m_rList.begin(); it != m_rList.end(); ++it )
    {
        if( (*it).m_playing )
        {
            break;
        }
        playIndex++;
    }
    if( it != m_rList.end() &&
        ( playIndex < m_lastPos || playIndex >= m_lastPos + maxItems ) )
    {
        // Scroll the list to have the playing stream visible
        VarPercent &rVarPos = m_rList.getPositionVar();
        rVarPos.set( 1.0 - (float)playIndex / (float)m_rList.size() );
        // The image will be changed by onUpdate(VarPercent&)
    }
    else
    {
        makeImage();
        notifyLayout();
    }
}


void CtrlList::makeImage()
{
    delete m_pImage;

    // Get the size of the control
    const Position *pPos = getPosition();
    if( !pPos )
    {
        return;
    }
    int width = pPos->getWidth();
    int height = pPos->getHeight();
    int itemHeight = m_rFont.getSize() + LINE_INTERVAL;

    // Create an image
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );
    m_pImage = pOsFactory->createOSGraphics( width, height );

    VarList::ConstIterator it = m_rList[m_lastPos];

    // Draw the background
    if( m_pBitmap )
    {
        // A background bitmap is given, so we scale it, ignoring the
        // background colors
        ScaledBitmap bmp( getIntf(), *m_pBitmap, width, height );
        m_pImage->drawBitmap( bmp, 0, 0 );

        // Take care of the selection color
        for( int yPos = 0; yPos < height; yPos += itemHeight )
        {
            int rectHeight = __MIN( itemHeight, height - yPos );
            if( it != m_rList.end() )
            {
                if( (*it).m_selected )
                {
                    m_pImage->fillRect( 0, yPos, width, rectHeight,
                                        m_selColor );
                }
                ++it;
            }
        }
    }
    else
    {
        // No background bitmap, so use the 2 background colors
        // Current background color
        uint32_t bgColor = m_bgColor1;
        for( int yPos = 0; yPos < height; yPos += itemHeight )
        {
            int rectHeight = __MIN( itemHeight, height - yPos );
            if( it != m_rList.end() )
            {
                uint32_t color = ( (*it).m_selected ? m_selColor : bgColor );
                m_pImage->fillRect( 0, yPos, width, rectHeight, color );
                ++it;
            }
            else
            {
                m_pImage->fillRect( 0, yPos, width, rectHeight, bgColor );
            }
            // Flip the background color
            bgColor = ( bgColor == m_bgColor1 ? m_bgColor2 : m_bgColor1 );
        }
    }

    // Draw the items
    int yPos = 0;
    for( it = m_rList[m_lastPos]; it != m_rList.end() && yPos < height; ++it )
    {
        UString *pStr = (UString*)(it->m_cString.get());
        uint32_t color = ( it->m_playing ? m_playColor : m_fgColor );

        // Draw the text
        GenericBitmap *pText = m_rFont.drawString( *pStr, color, width );
        if( !pText )
        {
            return;
        }
        yPos += itemHeight - pText->getHeight();
        int ySrc = 0;
        if( yPos < 0 )
        {
            ySrc = - yPos;
            yPos = 0;
        }
        int lineHeight = __MIN( pText->getHeight() - ySrc, height - yPos );
        m_pImage->drawBitmap( *pText, 0, ySrc, 0, yPos, pText->getWidth(),
                              lineHeight, true );
        yPos += (pText->getHeight() - ySrc );
        delete pText;

    }
}

