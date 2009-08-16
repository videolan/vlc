/*****************************************************************************
 * playlist_item.cpp : Manage playlist item
 ****************************************************************************
 * Copyright © 2006-2008 the VideoLAN team
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
#include "components/playlist/playlist_model.hpp"
#include <vlc_intf_strings.h>

#include <QSettings>

#include "sorting.h"

/*************************************************************************
 * Playlist item implementation
 *************************************************************************/

/*
   Playlist item is just a wrapper, an abstraction of the playlist_item
   in order to be managed by PLModel

   PLItem have a parent, and id and a input Id
*/


void PLItem::init( playlist_item_t *_playlist_item, PLItem *parent, PLModel *m, QSettings *settings )
{
    parentItem = parent;          /* Can be NULL, but only for the rootItem */
    i_id       = _playlist_item->i_id;           /* Playlist item specific id */
    model      = m;               /* PLModel (QAbsmodel) */
    p_input    = _playlist_item->p_input;
    vlc_gc_incref( p_input );

    assert( model );              /* We need a model */
}

/*
   Constructors
   Call the above function init
   */
PLItem::PLItem( playlist_item_t *p_item, PLItem *parent, PLModel *m )
{
    init( p_item, parent, m, NULL );
}

PLItem::PLItem( playlist_item_t * p_item, QSettings *settings, PLModel *m )
{
    init( p_item, NULL, m, settings );
}

PLItem::~PLItem()
{
    vlc_gc_decref( p_input );
    qDeleteAll( children );
    children.clear();
}

/* So far signal is always true.
   Using signal false would not call PLModel... Why ?
 */
void PLItem::insertChild( PLItem *item, int i_pos, bool signal )
{
    if( signal )
        model->beginInsertRows( model->index( this , 0 ), i_pos, i_pos );
    children.insert( i_pos, item );
    if( signal )
        model->endInsertRows();
}

void PLItem::remove( PLItem *removed )
{
    if( model->i_depth == DEPTH_SEL || parentItem )
    {
        int i_index = parentItem->children.indexOf( removed );
        model->beginRemoveRows( model->index( parentItem, 0 ),
                                i_index, i_index );
        parentItem->children.removeAt( i_index );
        model->endRemoveRows();
    }
}

/* This function is used to get one's parent's row number in the model */
int PLItem::row() const
{
    if( parentItem )
        return parentItem->children.indexOf( const_cast<PLItem*>(this) );
       // We don't ever inherit PLItem, yet, but it might come :D
    return 0;
}

/* update the PL Item, get the good names and so on */
void PLItem::update( playlist_item_t *p_item )
{
    assert( p_item->p_input == p_input);

}

