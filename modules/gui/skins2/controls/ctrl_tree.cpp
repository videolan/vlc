/*****************************************************************************
 * ctrl_tree.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: ctrl_list.cpp 11009 2005-05-14 14:39:05Z ipkiss $
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
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

#include <math.h>
#include "ctrl_tree.hpp"
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
#include "vlc_keys.h"
#ifdef sun
#   include "solaris_specific.h" // for lrint
#endif

#define SCROLL_STEP 0.05
#define LINE_INTERVAL 1  // Number of pixels inserted between 2 lines

CtrlTree::CtrlTree( intf_thread_t *pIntf,
                  VarTree &rTree,
                  const GenericFont &rFont,
                  const GenericBitmap *pBgBitmap,
                  const GenericBitmap *pItemBitmap,
                  const GenericBitmap *pOpenBitmap,
                  const GenericBitmap *pClosedBitmap,
                  uint32_t fgColor,
                  uint32_t playColor,
                  uint32_t bgColor1,
                  uint32_t bgColor2,
                  uint32_t selColor,
                  const UString &rHelp,
                  VarBool *pVisible ):
    CtrlGeneric( pIntf,rHelp, pVisible), m_rTree( rTree), m_rFont( rFont ),
    m_pBgBitmap( pBgBitmap ), m_pItemBitmap( pItemBitmap ),
    m_pOpenBitmap( pOpenBitmap ), m_pClosedBitmap( pClosedBitmap ),
    m_fgColor( fgColor ), m_playColor( playColor ), m_bgColor1( bgColor1 ),
    m_bgColor2( bgColor2 ), m_selColor( selColor ),
    m_pLastSelected( NULL ), m_pImage( NULL )
{
    // Observe the tree and position variables
    m_rTree.addObserver( this );
    m_rTree.getPositionVar().addObserver( this );

    m_lastPos = m_rTree.begin();

    makeImage();
}

CtrlTree::~CtrlTree()
{
    m_rTree.getPositionVar().delObserver( this );
    m_rTree.delObserver( this );
    if( m_pImage )
    {
        delete m_pImage;
    }
}

int CtrlTree::itemHeight()
{
    int itemHeight = m_rFont.getSize();
    if( m_pClosedBitmap )
    {
        itemHeight = __MAX( m_pClosedBitmap->getHeight(), itemHeight );
    }
    if( m_pOpenBitmap )
    {
        itemHeight = __MAX( m_pOpenBitmap->getHeight(), itemHeight );
    }
    if( m_pItemBitmap )
    {
        itemHeight = __MAX( m_pItemBitmap->getHeight(), itemHeight );
    }
    itemHeight += LINE_INTERVAL;
    return itemHeight;
}

int CtrlTree::itemImageWidth()
{
    int bitmapWidth = 5;
    if( m_pClosedBitmap )
    {
        bitmapWidth = __MAX( m_pClosedBitmap->getWidth(), bitmapWidth );
    }
    if( m_pOpenBitmap )
    {
        bitmapWidth = __MAX( m_pOpenBitmap->getWidth(), bitmapWidth );
    }
    if( m_pItemBitmap )
    {
        bitmapWidth = __MAX( m_pItemBitmap->getWidth(), bitmapWidth );
    }
    return bitmapWidth + 2;
}

int CtrlTree::maxItems()
{
    const Position *pPos = getPosition();
    if( !pPos )
    {
        return -1;
    }
    return pPos->getHeight() / itemHeight();
}


void CtrlTree::onUpdate( Subject<VarTree> &rTree )
{
    autoScroll();
    m_pLastSelected = NULL;
}

void CtrlTree::onUpdate( Subject<VarPercent> &rPercent )
{
    // Determine what is the first item to display
    VarTree::Iterator it = m_rTree.begin();

    int excessItems = m_rTree.visibleItems() - maxItems();

    fprintf( stderr, "Hullo\n");

    if( excessItems > 0)
    {
        VarPercent &rVarPos = m_rTree.getPositionVar();
        // a simple (int)(...) causes rounding errors !
#ifdef _MSC_VER
#   define lrint (int)
#endif
        it = m_rTree.visibleItem(lrint( (1.0 - rVarPos.get()) * (double)excessItems ) + 1);
    }
    if( m_lastPos != it )
    {
        fprintf( stderr, "updating\n" );
        // Redraw the control if the position has changed
        m_lastPos = it;
        makeImage();
        notifyLayout();
    }
    else
        fprintf( stderr, "not updating\n" );
}

void CtrlTree::onResize()
{
// FIXME : shouldn't be the same as the onUpdate function ... but i'm lazy
    // Determine what is the first item to display
    VarTree::Iterator it = m_rTree.begin();

    int excessItems = m_rTree.visibleItems() - maxItems();

    if( excessItems > 0)
    {
        VarPercent &rVarPos = m_rTree.getPositionVar();
        // a simple (int)(...) causes rounding errors !
#ifdef _MSC_VER
#   define lrint (int)
#endif
        it = m_rTree.visibleItem(lrint( (1.0 - rVarPos.get()) * (double)excessItems ) + 1);
    }
    // Redraw the control if the position has changed
    m_lastPos = it;
    makeImage();
    notifyLayout();
#if 0
    // Determine what is the first item to display
    VarTree::Iterator it = m_rTree.begin();

    int excessItems = m_rTree.visibleItems() - maxItems();

    if( excessItems > 0)
    {
        /* FIXME VarPercent &rVarPos = m_rTree.getPositionVar();
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
        }*/
        it = m_rTree.visibleItem( excessItems );
    }
    makeImage();
    notifyLayout();
#endif
}

void CtrlTree::onPositionChange()
{
    makeImage();
    notifyLayout();
}

#define IT_DISP_LOOP_END( a )  \
                if( a ->m_expanded && a ->size() ) \
                { \
                    a = a ->begin(); \
                } \
                else \
                { \
                    VarTree::Iterator it_old = a; \
                    a ++; \
                    if( it_old->parent() && it_old->parent()->end() == a ) \
                    { \
                        a = it_old->uncle(); \
                    } \
                }
void CtrlTree::handleEvent( EvtGeneric &rEvent )
{
    // TODO TODO FIXME TODO TODO
    if( rEvent.getAsString().find( "key:down" ) != string::npos )
    {
        int key = ((EvtKey&)rEvent).getKey();
        VarTree::Iterator it = m_rTree.begin();
        bool previousWasSelected = false;
        while( it != m_rTree.end() )
        {
            VarTree::Iterator next = it;
            IT_DISP_LOOP_END( next );
            if( key == KEY_UP )
            {
                //Scroll up one item
                if( ( it->parent()
                      && it != it->parent()->begin() )
                    || &*it != m_pLastSelected )
                {
                    bool nextWasSelected = ( &*next == m_pLastSelected );
                    it->m_selected = nextWasSelected;
                    if( nextWasSelected )
                    {
                        m_pLastSelected = &*it;
                    }
                }
            }
            else if( key == KEY_DOWN )
            {
                // Scroll down one item
                if( ( it->parent()
                      && next != it->parent()->end() )
                    || &*it != m_pLastSelected )
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
            else if( key == KEY_RIGHT )
            {
                // Go down one level
                if( it->m_expanded )
                {
                    if( it->size() )
                    {
                        /* FIXME : finir */
                        m_pLastSelected = &*(it->begin());
                    }
                }
                else
                {
                    it->m_expanded = true;
                }
            }
            else if( key == KEY_LEFT )
            {
                // Go up one level (and close node)
                // TODO
                it->m_expanded = false;
            }
            it = next;
        }

        // Redraw the control
        makeImage();
        notifyLayout();
    }

    else if( rEvent.getAsString().find( "mouse:left" ) != string::npos )
    {
        EvtMouse &rEvtMouse = (EvtMouse&)rEvent;
        const Position *pos = getPosition();
        int yPos = ( rEvtMouse.getYPos() - pos->getTop() ) / itemHeight();
        int xPos = rEvtMouse.getXPos() - pos->getLeft();
        VarTree::Iterator it;
        int index = 0;

        if( rEvent.getAsString().find( "mouse:left:down:ctrl,shift" ) !=
            string::npos )
        {
            // Flag to know if the currend item must be selected
            bool select = false;
            index = -1;
            for( it = m_rTree.begin(); it != m_rTree.end(); )
            {
                bool nextSelect = select;
                if( it == m_lastPos ) index = 0;
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
                it->m_selected = (*it).m_selected || select;
                select = nextSelect;
                if( index != -1 )
                    index++;
                IT_DISP_LOOP_END( it );
            }
        }
        else if( rEvent.getAsString().find( "mouse:left:down:ctrl" ) !=
                 string::npos )
        {
            for( it = m_lastPos; it != m_rTree.end(); )
            {
                if( index == yPos )
                {
                    it->m_selected = !it->m_selected;
                    m_pLastSelected = &*it;
                    break;
                }
                index++;
                IT_DISP_LOOP_END( it );
            }
        }
        else if( rEvent.getAsString().find( "mouse:left:down:shift" ) !=
                 string::npos )
        {
            // Flag to know if the currend item must be selected
            bool select = false;
            index = -1;
            for( it = m_rTree.begin(); it != m_rTree.end(); )
            {
                bool nextSelect = select;
                if( it == m_lastPos ) index = 0;
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
                it->m_selected = select;
                select = nextSelect;
                if( index != -1 )
                    index++;
                IT_DISP_LOOP_END( it );
            }
        }
        else if( rEvent.getAsString().find( "mouse:left:down" ) !=
                 string::npos )
        {
            for( it = m_lastPos; it != m_rTree.end(); )
            {
                if( index == yPos )
                {
                    it->m_selected = true;
                    m_pLastSelected = &*it;
                }
                else
                {
                    it->m_selected = false;
                }
                index ++;
                IT_DISP_LOOP_END( it );
            }
        }

        else if( rEvent.getAsString().find( "mouse:left:dblclick" ) !=
                 string::npos )
        {
            for( it = m_lastPos; it != m_rTree.end(); )
            {
                if( index == yPos )
                {
                    if( it->size() && xPos < it->depth() * itemImageWidth() )
                    {
                        it->m_expanded = !it->m_expanded;
                    }
                    else
                    {
                        it->m_selected = true;
                        m_pLastSelected = &*it;
                        // Execute the action associated to this item
                        m_rTree.action( &*it );
                    }
                }
                else
                {
                    it->m_selected = false;
                }
                index ++;
                IT_DISP_LOOP_END( it );
            }
        }

        // Redraw the control
        makeImage();
        notifyLayout();
    }

    else if( rEvent.getAsString().find( "scroll" ) != string::npos )
    {
        int direction = ((EvtScroll&)rEvent).getDirection();

        double percentage = m_rTree.getPositionVar().get();
        double step = 2.0 / (double)m_rTree.visibleItems();
        if( direction == EvtScroll::kUp )
        {
            percentage += step;
        }
        else
        {
            percentage -= step;
        }
        m_rTree.getPositionVar().set( percentage );
    }
}

bool CtrlTree::mouseOver( int x, int y ) const
{
    const Position *pPos = getPosition();
    return ( pPos
       ? x >= 0 && x <= pPos->getWidth() && y >= 0 && y <= pPos->getHeight()
       : false);
}

void CtrlTree::draw( OSGraphics &rImage, int xDest, int yDest )
{
    if( m_pImage )
    {
        rImage.drawGraphics( *m_pImage, 0, 0, xDest, yDest );
    }
}

void CtrlTree::autoScroll()
{
    // Find the current playing stream
    int playIndex = 0;
    VarTree::Iterator it;
    for( it = m_rTree.begin(); it != m_rTree.end(); )
    {
        if( it->m_playing ) break;
        playIndex++;
        IT_DISP_LOOP_END( it );
    }

    if( it == m_rTree.end() ) return;

    // Find  m_lastPos
    int lastPosIndex = 0;
    for( it = m_rTree.begin(); it != m_rTree.end(); )
    {
        if( it == m_lastPos ) break;
        lastPosIndex++;
        IT_DISP_LOOP_END( it );
    }

    if( it == m_rTree.end() ) return;


    if( it != m_rTree.end()
        && ( playIndex < lastPosIndex
           || playIndex > lastPosIndex + maxItems() ) )
    {
        // Scroll to have the playing stream visible
        VarPercent &rVarPos = m_rTree.getPositionVar();
        rVarPos.set( 1.0 - (double)playIndex / (double)m_rTree.visibleItems() );
    }
    else
    {
        makeImage();
        notifyLayout();
    }
}

void CtrlTree::makeImage()
{
    fprintf( stderr, "CtrlTree::makeImage()\n");
    if( m_pImage )
    {
        delete m_pImage;
    }

    // Get the size of the control
    const Position *pPos = getPosition();
    if( !pPos )
    {
        return;
    }
    int width = pPos->getWidth();
    int height = pPos->getHeight();

    int i_itemHeight = itemHeight();

    // Create an image
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );
    m_pImage = pOsFactory->createOSGraphics( width, height );

    VarTree::Iterator it = m_lastPos;

    if( m_pBgBitmap )
    {
        // Draw the background bitmap
        ScaledBitmap bmp( getIntf(), *m_pBgBitmap, width, height );
        m_pImage->drawBitmap( bmp, 0, 0 );

        for( int yPos = 0; yPos < height; yPos += i_itemHeight )
        {
            int rectHeight = __MIN( i_itemHeight, height - yPos );
            if( it != m_rTree.end() )
            {
                if( (*it).m_selected )
                {
                    m_pImage->fillRect( 0, yPos, width, rectHeight,
                                        m_selColor );
                }
                IT_DISP_LOOP_END( it );
            }
        }
    }
    else
    {
        // FIXME (TRYME)
        // Fill background with background color
        uint32_t bgColor = m_bgColor1;
        m_pImage->fillRect( 0, 0, width, height, bgColor );
        for( int yPos = 0; yPos < height; yPos += i_itemHeight )
        {
            int rectHeight = __MIN( i_itemHeight, height - yPos );
            if( it != m_rTree.end() )
            {
                uint32_t color = ( it->m_selected ? m_selColor : bgColor );
                m_pImage->fillRect( 0, yPos, width, rectHeight, color );
                IT_DISP_LOOP_END( it );
            }
            else
            {
                m_pImage->fillRect( 0, yPos, width, rectHeight, bgColor );
            }
            bgColor = ( bgColor == m_bgColor1 ? m_bgColor2 : m_bgColor1 );
        }
    }
//    fprintf( stderr, "done\n");

    int bitmapWidth = itemImageWidth();

    int yPos = 0;
    it = m_lastPos;
    while( it != m_rTree.end() && yPos < height )
    {
        const GenericBitmap *m_pCurBitmap;
        UString *pStr = (UString*)(it->m_cString.get());
        uint32_t color = ( it->m_playing ? m_playColor : m_fgColor );
        // Draw the text
        if( pStr != NULL ){
            int depth = it->depth();
            GenericBitmap *pText = m_rFont.drawString( *pStr, color, width - bitmapWidth * depth );
            if( !pText )
            {
                return;
            }
            m_pCurBitmap = it->size() ? ( it->m_expanded ? m_pOpenBitmap : m_pClosedBitmap ) : m_pItemBitmap ;
            if( m_pCurBitmap )
            {
                int yPos2 = yPos+(i_itemHeight-m_pCurBitmap->getHeight()+1)/2;
                m_pImage->drawBitmap( *m_pCurBitmap, 0, 0, bitmapWidth * (depth - 1 ), yPos2, m_pCurBitmap->getWidth(), __MIN( m_pCurBitmap->getHeight(), height -  yPos2), true );
            }
            else
            {
                /* it would be nice to draw something */
            }
            yPos += i_itemHeight - pText->getHeight();
            int ySrc = 0;
            if( yPos < 0 )
            {
                ySrc = - yPos;
                yPos = 0;
            }
            int lineHeight =  __MIN( pText->getHeight() - ySrc, height - yPos );
            m_pImage->drawBitmap( *pText, 0, ySrc, bitmapWidth * depth, yPos,
                                  pText->getWidth(),
                                  lineHeight, true );
            yPos += (pText->getHeight() - ySrc );
            delete pText;
        }
        IT_DISP_LOOP_END( it );
    }
}
