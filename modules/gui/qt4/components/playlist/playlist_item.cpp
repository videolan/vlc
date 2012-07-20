/*****************************************************************************
 * playlist_item.cpp : Manage playlist item
 ****************************************************************************
 * Copyright © 2006-2011 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include "qt4.hpp"
#include "playlist_item.hpp"

/*************************************************************************
 * Playlist item implementation
 *************************************************************************/

void AbstractPLItem::clearChildren()
{
    qDeleteAll( children );
    children.clear();
}

/*
   Playlist item is just a wrapper, an abstraction of the playlist_item
   in order to be managed by PLModel

   PLItem have a parent, and id and a input Id
*/

void PLItem::init( playlist_item_t *_playlist_item, PLItem *parent )
{
    parentItem = parent;          /* Can be NULL, but only for the rootItem */
    i_id       = _playlist_item->i_id;           /* Playlist item specific id */
    p_input    = _playlist_item->p_input;
    vlc_gc_incref( p_input );
}

/*
   Constructors
   Call the above function init
   */
PLItem::PLItem( playlist_item_t *p_item, PLItem *parent )
{
    init( p_item, parent );
}

PLItem::PLItem( playlist_item_t * p_item )
{
    init( p_item, NULL );
}

PLItem::~PLItem()
{
    vlc_gc_decref( p_input );
    qDeleteAll( children );
    children.clear();
}

void PLItem::removeChild( PLItem *item )
{
    children.removeOne( item );
    delete item;
}

void PLItem::takeChildAt( int index )
{
    AbstractPLItem *child = children[index];
    child->parentItem = NULL;
    children.removeAt( index );
}

/* This function is used to get one's parent's row number in the model */
int PLItem::row()
{
    if( parentItem )
        return parentItem->indexOf( this );
    return 0;
}

bool PLItem::operator< ( PLItem& other )
{
    AbstractPLItem *item1 = this;
    while( item1->parentItem )
    {
        AbstractPLItem *item2 = &other;
        while( item2->parentItem )
        {
            if( item1 == item2->parentItem ) return true;
            if( item2 == item1->parentItem ) return false;
            if( item1->parentItem == item2->parentItem )
                return item1->parentItem->indexOf( item1 ) <
                       item1->parentItem->indexOf( item2 );
            item2 = item2->parentItem;
        }
        item1 = item1->parentItem;
    }
    return false;
}
