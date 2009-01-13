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


void PLItem::init( int _i_id, int _i_input_id, PLItem *parent, PLModel *m, QSettings *settings )
{
    parentItem = parent;          /* Can be NULL, but only for the rootItem */
    i_id       = _i_id;           /* Playlist item specific id */
    i_input_id = _i_input_id;     /* Identifier of the input */
    model      = m;               /* PLModel (QAbsmodel) */
    i_type     = -1;              /* Item type - Avoid segfault */
    b_current  = false;           /* Is the item the current Item or not */

    assert( model );              /* We need a model */

    /* No parent, should be the 2 main ones */
    if( parentItem == NULL )
    {
        if( model->i_depth == DEPTH_SEL )  /* Selector Panel */
        {
            i_showflags = 0;
            item_col_strings.append( "" );
        }
        else
        {
            i_showflags = settings->value( "qt-pl-showflags", COLUMN_DEFAULT ).toInt();
            if( i_showflags < 1)
                i_showflags = COLUMN_DEFAULT; /* reasonable default to show something; */
            else if ( i_showflags >= COLUMN_END )
                i_showflags = COLUMN_END - 1; /* show everything */

            updateColumnHeaders();
        }
    }
    else
    {
        i_showflags = parentItem->i_showflags;
        //Add empty string and update() handles data appending
        item_col_strings.append( "" );
    }
}

/*
   Constructors
   Call the above function init
   So far the first constructor isn't used...
   */
PLItem::PLItem( int _i_id, int _i_input_id, PLItem *parent, PLModel *m )
{
    init( _i_id, _i_input_id, parent, m, NULL );
}

PLItem::PLItem( playlist_item_t * p_item, PLItem *parent, PLModel *m )
{
    init( p_item->i_id, p_item->p_input->i_id, parent, m, NULL );
}

PLItem::PLItem( playlist_item_t * p_item, QSettings *settings, PLModel *m )
{
    init( p_item->i_id, p_item->p_input->i_id, NULL, m, settings );
}

PLItem::~PLItem()
{
    qDeleteAll( children );
    children.clear();
}

/* Column manager */
void PLItem::updateColumnHeaders()
{
    item_col_strings.clear();

    assert( i_showflags < COLUMN_END );

    for( uint32_t i_index=1; i_index < COLUMN_END; i_index <<= 1 )
    {
        if( i_showflags & i_index )
            item_col_strings.append( qfu( psz_column_title( i_index ) ) );
    }
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
/* This function may not be the best way to do it
   It destroys everything and gets everything again instead of just
   building the necessary columns.
   This does extra work if you re-display the same column. Slower...
   On the other hand, this way saves memory.
   There must be a more clever way.
   */
void PLItem::update( playlist_item_t *p_item, bool iscurrent )
{
    assert( p_item->p_input->i_id == i_input_id );

    /* Useful for the model */
    i_type = p_item->p_input->i_type;
    b_current = iscurrent;

    item_col_strings.clear();

    if( model->i_depth == 1 )  /* Selector Panel */
    {
        item_col_strings.append( qfu( p_item->p_input->psz_name ) );
        return;
    }

    i_showflags = parentItem ? parentItem->i_showflags : i_showflags;

    /* Meta: ID */
    if( i_showflags & COLUMN_NUMBER )
    {
        QModelIndex idx = model->index( this, 0 );
        item_col_strings.append( QString::number( idx.row() + 1 ) );
    }
    /* Other meta informations */
    for( uint32_t i_index=2; i_index < COLUMN_END; i_index <<= 1 )
    {
        if( i_showflags & i_index )
        {
            char *psz = psz_column_meta( p_item->p_input, i_index );
            item_col_strings.append( qfu( psz ) );
            free( psz );
        }
    }
}

