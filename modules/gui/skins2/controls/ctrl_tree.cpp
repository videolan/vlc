/*****************************************************************************
 * ctrl_tree.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
 *          Clément Stenac <zorglub@videolan.org>
 *          Erwan Tulou  <erwan10 At videolan DoT org>
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
#include "../utils/var_bool.hpp"
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
#include "../events/evt_dragndrop.hpp"
#include "../vars/playtree.hpp"
#include <vlc_keys.h>

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
                    VarBool *pVisible,
                    VarBool *pFlat ):
    CtrlGeneric( pIntf,rHelp, pVisible), m_rTree( rTree), m_rFont( rFont ),
    m_pBgBitmap( pBgBitmap ), m_pItemBitmap( pItemBitmap ),
    m_pOpenBitmap( pOpenBitmap ), m_pClosedBitmap( pClosedBitmap ),
    m_pScaledBitmap( NULL ), m_pImage( NULL ),
    m_fgColor( fgColor ), m_playColor( playColor ),
    m_bgColor1( bgColor1 ), m_bgColor2( bgColor2 ), m_selColor( selColor ),
    m_firstPos( m_rTree.end() ), m_lastClicked( m_rTree.end() ),
    m_itOver( m_rTree.end() ), m_flat( pFlat->get() ), m_capacity( -1.0 ),
    m_bRefreshOnDelete( false )
{
    // Observe the tree
    m_rTree.addObserver( this );
    m_rTree.setFlat( m_flat );
}

CtrlTree::~CtrlTree()
{
    m_rTree.delObserver( this );
    delete m_pImage;
    delete m_pScaledBitmap;
}

int CtrlTree::itemHeight()
{
    int itemHeight = m_rFont.getSize();
    if( !m_flat )
    {
        if( m_pClosedBitmap )
        {
            itemHeight = __MAX( m_pClosedBitmap->getHeight(), itemHeight );
        }
        if( m_pOpenBitmap )
        {
            itemHeight = __MAX( m_pOpenBitmap->getHeight(), itemHeight );
        }
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
    if( !m_flat )
    {
        if( m_pClosedBitmap )
        {
            bitmapWidth = __MAX( m_pClosedBitmap->getWidth(), bitmapWidth );
        }
        if( m_pOpenBitmap )
        {
            bitmapWidth = __MAX( m_pOpenBitmap->getWidth(), bitmapWidth );
        }
    }
    if( m_pItemBitmap )
    {
        bitmapWidth = __MAX( m_pItemBitmap->getWidth(), bitmapWidth );
    }
    return bitmapWidth + 2;
}

float CtrlTree::maxItems()
{
    const Position *pPos = getPosition();
    if( !pPos )
    {
        return -1;
    }
    return (float)pPos->getHeight() / itemHeight();
}

void CtrlTree::onUpdate( Subject<VarTree, tree_update> &rTree,
                         tree_update *arg )
{
    (void)rTree;
    if( arg->type == arg->ItemInserted )
    {
        if( isItemVisible( arg->it ) )
        {
            makeImage();
            notifyLayout();
        }
        setSliderFromFirst();
    }
    else if( arg->type == arg->ItemUpdated )
    {
        if( arg->it->isPlaying() )
        {
            m_rTree.ensureExpanded( arg->it );
            ensureVisible( arg->it );

            makeImage();
            notifyLayout();
            setSliderFromFirst();
        }
        else if( isItemVisible( arg->it ) )
        {
            makeImage();
            notifyLayout();
        }
    }
    else if( arg->type == arg->DeletingItem )
    {
        if( isItemVisible( arg->it ) )
            m_bRefreshOnDelete = true;
        // remove all references to arg->it
        // if it is the one about to be deleted
        if( m_firstPos == arg->it )
        {
            m_firstPos = getNearestItem( arg->it );
        }
        if( m_lastClicked == arg->it )
        {
            m_lastClicked = getNearestItem( arg->it );
            m_lastClicked->setSelected( arg->it->isSelected() );
        }
    }
    else if( arg->type == arg->ItemDeleted )
    {
        if( m_bRefreshOnDelete )
        {
            m_bRefreshOnDelete = false;

            makeImage();
            notifyLayout();
        }
        setSliderFromFirst();
    }
    else if( arg->type == arg->ResetAll )
    {
        m_lastClicked = m_rTree.end();
        m_firstPos = getFirstFromSlider();

        makeImage();
        notifyLayout();
        setSliderFromFirst();
    }
    else if( arg->type == arg->SliderChanged )
    {
        Iterator it = getFirstFromSlider();
        if( m_firstPos != it )
        {
            m_firstPos = it;
            makeImage();
            notifyLayout();
        }
    }
}

void CtrlTree::onResize()
{
    onPositionChange();
}

void CtrlTree::onPositionChange()
{
    m_capacity = maxItems();
    setScrollStep();
    m_firstPos = getFirstFromSlider();
    makeImage();
}

void CtrlTree::handleEvent( EvtGeneric &rEvent )
{
    bool needShow = false;
    bool needRefresh = false;
    Iterator toShow = m_firstPos;
    if( rEvent.getAsString().find( "key:down" ) != string::npos )
    {
        int key = ((EvtKey&)rEvent).getKey();

        /* Delete the selection */
        if( key == KEY_DELETE )
        {
            /* Delete selected stuff */
            m_rTree.delSelected();
        }
        else if( key == KEY_PAGEDOWN )
        {
            int numSteps = (int)m_capacity / 2;
            VarPercent &rVarPos = m_rTree.getPositionVar();
            rVarPos.increment( -numSteps );
        }
        else if( key == KEY_PAGEUP )
        {
            int numSteps = (int)m_capacity / 2;
            VarPercent &rVarPos = m_rTree.getPositionVar();
            rVarPos.increment( numSteps );
        }
        else if( key == KEY_UP )
        {
            // Scroll up one item
            m_rTree.unselectTree();
            if( m_lastClicked != m_rTree.end() )
            {
                if( --m_lastClicked != m_rTree.end() )
                {
                    m_lastClicked->setSelected( true );
                }
            }
            if( m_lastClicked == m_rTree.end() )
            {
                m_lastClicked = m_firstPos;
                if( m_lastClicked != m_rTree.end() )
                    m_lastClicked->setSelected( true );
            }
            needRefresh = true;
            needShow = true; toShow = m_lastClicked;
        }
        else if( key == KEY_DOWN )
        {
            // Scroll down one item
            m_rTree.unselectTree();
            if( m_lastClicked != m_rTree.end() )
            {
                Iterator it_old = m_lastClicked;
                if( ++m_lastClicked != m_rTree.end() )
                {
                    m_lastClicked->setSelected( true );
                }
                else
                {
                    it_old->setSelected( true );
                    m_lastClicked = it_old;
                }
            }
            else
            {
                m_lastClicked = m_firstPos;
                if( m_lastClicked != m_rTree.end() )
                    m_lastClicked->setSelected( true );
            }
            needRefresh = true;
            needShow = true; toShow = m_lastClicked;
        }
        else if( key == KEY_RIGHT )
        {
            // Go down one level (and expand node)
            Iterator& it = m_lastClicked;
            if( it != m_rTree.end() )
            {
                if( !m_flat && !it->isExpanded() && it->size() )
                {
                    it->setExpanded( true );
                    needRefresh = true;
                }
                else
                {
                    m_rTree.unselectTree();
                    Iterator it_old = m_lastClicked;
                    if( ++m_lastClicked != m_rTree.end() )
                    {
                        m_lastClicked->setSelected( true );
                    }
                    else
                    {
                        it_old->setSelected( true );
                        m_lastClicked = it_old;
                    }
                    needRefresh = true;
                    needShow = true; toShow = m_lastClicked;
                }
            }
        }
        else if( key == KEY_LEFT )
        {
            // Go up one level (and close node)
            Iterator& it = m_lastClicked;
            if( it != m_rTree.end() )
            {
                if( m_flat )
                {
                    m_rTree.unselectTree();
                    if( --m_lastClicked != m_rTree.end() )
                    {
                        m_lastClicked->setSelected( true );
                    }
                    else
                    {
                        m_lastClicked = m_firstPos;
                        if( m_lastClicked != m_rTree.end() )
                            m_lastClicked->setSelected( true );
                    }
                    needRefresh = true;
                    needShow = true; toShow = m_lastClicked;
                }
                else
                {
                    if( it->isExpanded() )
                    {
                        it->setExpanded( false );
                        needRefresh = true;
                    }
                    else
                    {
                        Iterator it_parent = it.getParent();
                        if( it_parent != m_rTree.end() )
                        {
                            it->setSelected( false );
                            m_lastClicked = it_parent;
                            m_lastClicked->setSelected( true );
                            needRefresh = true;
                            needShow = true; toShow = m_lastClicked;
                        }
                    }
                }
            }
        }
        else if( key == KEY_ENTER || key == ' ' )
        {
            // Go up one level (and close node)
            if( m_lastClicked != m_rTree.end() )
            {
                m_rTree.action( &*m_lastClicked );
            }
        }
        else
        {
            // other keys to be forwarded to vlc core
            EvtKey& rEvtKey = (EvtKey&)rEvent;
            var_SetInteger( getIntf()->p_libvlc, "key-pressed",
                            rEvtKey.getModKey() );
        }
    }

    else if( rEvent.getAsString().find( "mouse:left" ) != string::npos )
    {
        EvtMouse &rEvtMouse = (EvtMouse&)rEvent;
        const Position *pos = getPosition();
        int xPos = rEvtMouse.getXPos() - pos->getLeft();
        int yPos = ( rEvtMouse.getYPos() - pos->getTop() ) / itemHeight();

        Iterator itClicked = findItemAtPos( yPos );
        if( itClicked != m_rTree.end() )
        {
            if( rEvent.getAsString().find( "mouse:left:down:ctrl,shift" ) !=
                string::npos )
            {
                // Flag to know if the current item must be selected
                bool select = false;
                for( Iterator it = m_firstPos; it != m_rTree.end(); ++it )
                {
                    bool nextSelect = select;
                    if( it == itClicked || it == m_lastClicked )
                    {
                        if( select )
                        {
                            nextSelect = false;
                        }
                        else
                        {
                            select = true;
                            if( itClicked != m_lastClicked )
                                nextSelect = true;
                        }
                    }
                    it->setSelected( it->isSelected() || select );
                    select = nextSelect;
                    needRefresh = true;
                }
            }
            else if( rEvent.getAsString().find( "mouse:left:down:ctrl" ) !=
                     string::npos )
            {
                // Invert the selection of the item
                itClicked->toggleSelected();
                m_lastClicked = itClicked;
                needRefresh = true;
            }
            else if( rEvent.getAsString().find( "mouse:left:down:shift" ) !=
                     string::npos )
            {
                bool select = false;
                for( Iterator it = m_firstPos; it != m_rTree.end(); ++it )
                {
                    bool nextSelect = select;
                    if( it == itClicked || it == m_lastClicked )
                    {
                        if( select )
                        {
                            nextSelect = false;
                        }
                        else
                        {
                            select = true;
                            if( itClicked != m_lastClicked )
                                nextSelect = true;
                        }
                    }
                    it->setSelected( select );
                    select = nextSelect;
                }
                needRefresh = true;
            }
            else if( rEvent.getAsString().find( "mouse:left:down" ) !=
                     string::npos )
            {
                if( !m_flat &&
                    itClicked->size() &&
                    xPos > (itClicked->depth() - 1) * itemImageWidth() &&
                    xPos < itClicked->depth() * itemImageWidth() )
                {
                    // Fold/unfold the item
                    itClicked->toggleExpanded();
                }
                else
                {
                    // Unselect any previously selected item
                    m_rTree.unselectTree();
                    // Select the new item
                    itClicked->setSelected( true );
                    m_lastClicked = itClicked;
                }
                needRefresh = true;
            }
            else if( rEvent.getAsString().find( "mouse:left:dblclick" ) !=
                     string::npos )
            {
               // Execute the action associated to this item
               m_rTree.action( &*itClicked );
            }
        }
    }

    else if( rEvent.getAsString().find( "scroll" ) != string::npos )
    {
        int direction = static_cast<EvtScroll&>(rEvent).getDirection();
        if( direction == EvtScroll::kUp )
            m_rTree.getPositionVar().increment( +1 );
        else
            m_rTree.getPositionVar().increment( -1 );
    }

    else if( rEvent.getAsString().find( "drag:over" ) != string::npos )
    {
        EvtDragOver& evt = static_cast<EvtDragOver&>(rEvent);
        const Position *pos = getPosition();
        int yPos = ( evt.getYPos() - pos->getTop() ) / itemHeight();

        Iterator it = findItemAtPos( yPos );
        if( it != m_itOver )
        {
            m_itOver = it;
            needRefresh = true;
        }
    }

    else if( rEvent.getAsString().find( "drag:drop" ) != string::npos )
    {
        EvtDragDrop& evt = static_cast<EvtDragDrop&>(rEvent);
        Playtree& rPlaytree = static_cast<Playtree&>(m_rTree);
        VarTree& item = ( m_itOver != m_rTree.end() ) ? *m_itOver : m_rTree;
        rPlaytree.insertItems( item, evt.getFiles(), false );
        m_itOver = m_rTree.end();
        needRefresh = true;
    }

    else if( rEvent.getAsString().find( "drag:leave" ) != string::npos )
    {
        m_itOver = m_rTree.end();
        needRefresh = true;
    }

    if( needShow )
    {
        if( toShow == m_rTree.end() ||
            !ensureVisible( toShow ) )
            needRefresh = true;
    }
    if( needRefresh )
    {
        setSliderFromFirst();

        makeImage();
        notifyLayout();
    }
}

bool CtrlTree::mouseOver( int x, int y ) const
{
    const Position *pPos = getPosition();
    return !pPos ? false :
        x >= 0 && x <= pPos->getWidth() && y >= 0 && y <= pPos->getHeight();
}

void CtrlTree::draw( OSGraphics &rImage, int xDest, int yDest, int w, int h)
{
    const Position *pPos = getPosition();
    rect region( pPos->getLeft(), pPos->getTop(),
                 pPos->getWidth(), pPos->getHeight() );
    rect clip( xDest, yDest, w, h );
    rect inter;

    if( rect::intersect( region, clip, &inter ) && m_pImage )
        rImage.drawGraphics( *m_pImage,
                      inter.x - pPos->getLeft(),
                      inter.y - pPos->getTop(),
                      inter.x, inter.y, inter.width, inter.height );
}

void CtrlTree::makeImage()
{
    delete m_pImage;

    // Get the size of the control
    const Position *pPos = getPosition();
    if( !pPos )
        return;
    int width = pPos->getWidth();
    int height = pPos->getHeight();

    int i_itemHeight = itemHeight();

    // Create an image
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );
    m_pImage = pOsFactory->createOSGraphics( width, height );

    Iterator it = m_firstPos;

    if( m_pBgBitmap )
    {
        // Draw the background bitmap
        if( !m_pScaledBitmap ||
            m_pScaledBitmap->getWidth() != width ||
            m_pScaledBitmap->getHeight() != height )
        {
            delete m_pScaledBitmap;
            m_pScaledBitmap =
                new ScaledBitmap( getIntf(), *m_pBgBitmap, width, height );
        }
        m_pImage->drawBitmap( *m_pScaledBitmap, 0, 0 );

        for( int yPos = 0;
             yPos < height && it != m_rTree.end();
             yPos += i_itemHeight, ++it )
        {
            if( it->isSelected() )
            {
                int rectHeight = __MIN( i_itemHeight, height - yPos );
                m_pImage->fillRect( 0, yPos, width, rectHeight, m_selColor );
            }
        }
    }
    else
    {
        // Fill background with background color
        uint32_t bgColor = m_bgColor1;
        m_pImage->fillRect( 0, 0, width, height, bgColor );
        // Overwrite with alternate colors (bgColor1, bgColor2)
        for( int yPos = 0; yPos < height; yPos += i_itemHeight )
        {
            int rectHeight = __MIN( i_itemHeight, height - yPos );
            if( it == m_rTree.end() )
                m_pImage->fillRect( 0, yPos, width, rectHeight, bgColor );
            else
            {
                uint32_t color = ( it->isSelected() ? m_selColor : bgColor );
                m_pImage->fillRect( 0, yPos, width, rectHeight, color );
                ++it;
            }
            bgColor = ( bgColor == m_bgColor1 ? m_bgColor2 : m_bgColor1 );
        }
    }

    int bitmapWidth = itemImageWidth();

    it = m_firstPos;
    for( int yPos = 0; yPos < height && it != m_rTree.end(); ++it )
    {
        const GenericBitmap *m_pCurBitmap;
        UString *pStr = it->getString();
        if( pStr != NULL )
        {
            uint32_t color = it->isPlaying() ? m_playColor : m_fgColor;
            int depth = m_flat ? 1 : it->depth();
            GenericBitmap *pText =
                m_rFont.drawString( *pStr, color, width-bitmapWidth*depth );
            if( !pText )
            {
                return;
            }
            if( it->size() )
                m_pCurBitmap =
                    it->isExpanded() ? m_pOpenBitmap : m_pClosedBitmap;
            else
                m_pCurBitmap = m_pItemBitmap;

            if( m_pCurBitmap )
            {
                // Make sure we are centered on the line
                int yPos2 = yPos+(i_itemHeight-m_pCurBitmap->getHeight()+1)/2;
                if( yPos2 >= height )
                {
                    delete pText;
                    break;
                }
                // Draw the icon in front of the text
                m_pImage->drawBitmap( *m_pCurBitmap, 0, 0,
                                      bitmapWidth * (depth - 1 ), yPos2,
                                      m_pCurBitmap->getWidth(),
                                      __MIN( m_pCurBitmap->getHeight(),
                                             height -  yPos2), true );
            }
            yPos += (i_itemHeight - pText->getHeight());
            if( yPos >= height )
            {
                delete pText;
                break;
            }

            int ySrc = 0;
            if( yPos < 0 )
            {
                ySrc = - yPos;
                yPos = 0;
            }
            int lineHeight = __MIN( pText->getHeight() - ySrc, height - yPos );
            // Draw the text
            m_pImage->drawBitmap( *pText, 0, ySrc, bitmapWidth * depth, yPos,
                                  pText->getWidth(),
                                  lineHeight, true );
            yPos += (pText->getHeight() - ySrc );

            if( it == m_itOver )
            {
                // Draw the underline bar below the text for drag&drop
                m_pImage->fillRect(
                    bitmapWidth * (depth - 1 ), yPos - 2,
                    bitmapWidth + pText->getWidth(), __MAX( lineHeight/5, 3 ),
                    m_selColor );
            }
            delete pText;
        }
    }
}

CtrlTree::Iterator CtrlTree::findItemAtPos( int pos )
{
    // The first item is m_firstPos.
    // We decrement pos as we try the other items, until pos == 0.
    Iterator it = m_firstPos;
    for( ; it != m_rTree.end() && pos != 0; ++it, pos-- );

    return it;
}

CtrlTree::Iterator CtrlTree::getFirstFromSlider()
{
    // a simple (int)(...) causes rounding errors !
#ifdef _MSC_VER
#       define lrint (int)
#endif
    VarPercent &rVarPos = m_rTree.getPositionVar();
    double percentage = rVarPos.get();

    int excessItems = m_flat ? (m_rTree.countLeafs() - (int)m_capacity)
                             : (m_rTree.visibleItems() - (int)m_capacity);

    int index = (excessItems > 0 ) ?
        lrint( (1.0 - percentage)*(double)excessItems ) :
        0;

    Iterator it_first = m_rTree.getItem( index );

    return it_first;
}

void CtrlTree::setScrollStep()
{
    VarPercent &rVarPos = m_rTree.getPositionVar();

    int excessItems = m_flat ? (m_rTree.countLeafs() - (int)m_capacity)
                             : (m_rTree.visibleItems() - (int)m_capacity);

    if( excessItems > 0 )
        rVarPos.setStep( (float)1 / excessItems );
    else
        rVarPos.setStep( 1.0 );
}

void CtrlTree::setSliderFromFirst()
{
    VarPercent &rVarPos = m_rTree.getPositionVar();

    int excessItems = m_flat ? (m_rTree.countLeafs() - (int)m_capacity)
                             : (m_rTree.visibleItems() - (int)m_capacity);

    int index = m_rTree.getIndex( m_firstPos );
    if( excessItems > 0 )
    {
        rVarPos.set( 1.0 - (float)index/(float)excessItems );
        rVarPos.setStep( 1.0 / excessItems );
    }
    else
    {
        rVarPos.set( 1.0 );
        rVarPos.setStep( 1.0 );
    }
}

bool CtrlTree::isItemVisible( const Iterator& it_ref )
{
    if( it_ref == m_rTree.end() )
        return false;

    Iterator it = m_firstPos;
    if( it == m_rTree.end() )
        return true;

    // Ensure a partially visible last item is taken into account
    int max = (int)m_capacity;
    if( (float)max < m_capacity )
        max++;

    for( int i = 0; i < max && it != m_rTree.end(); ++it, i++ )
    {
        if( it == it_ref )
            return true;
    }
    return false;
}

bool CtrlTree::ensureVisible( const Iterator& item )
{
    Iterator it = m_firstPos;
    int max = (int)m_capacity;
    for( int i = 0; i < max && it != m_rTree.end(); ++it, i++ )
    {
        if( it == item )
            return false;
    }

    m_rTree.setSliderFromItem( item );
    return true;
}

CtrlTree::Iterator CtrlTree::getNearestItem( const Iterator& item )
{
    // return the previous item if it exists
    Iterator newItem = item;
    if( --newItem != m_rTree.end() && newItem != item )
        return newItem;

    // return the next item if no previous item found
    newItem = item;
    return ++newItem;
}
