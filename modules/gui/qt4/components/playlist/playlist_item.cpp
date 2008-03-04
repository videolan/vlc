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

#include "pixmaps/type_unknown.xpm"

#include <QSettings>

/*************************************************************************
 * Playlist item implementation
 *************************************************************************/

/*
   Playlist item is just a wrapper, an abstraction of the playlist_item
   in order to be managed by PLModel

   PLItem have a parent, and id and a input Id
*/


void PLItem::init( int _i_id, int _i_input_id, PLItem *parent, PLModel *m )
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
            item_col_strings.append( "" );
        }
        else
        {
            QSettings settings( "vlc", "vlc-qt-interface" );
            i_showflags = settings.value( "qt-pl-showflags", 39 ).toInt();
            if( i_showflags < 1)
                i_showflags = 39; //reasonable default to show something;
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
    init( _i_id, _i_input_id, parent, m );
}

PLItem::PLItem( playlist_item_t * p_item, PLItem *parent, PLModel *m )
{
    init( p_item->i_id, p_item->p_input->i_id, parent, m );
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

    for( int i_index=1; i_index <= VLC_META_ENGINE_ART_URL; i_index *= 2 )
    {
        if( i_showflags & i_index )
        {
            switch( i_index )
            {
            case VLC_META_ENGINE_ARTIST:
                item_col_strings.append( qtr( VLC_META_ARTIST ) );
                break;
            case VLC_META_ENGINE_TITLE:
                item_col_strings.append( qtr( VLC_META_TITLE ) );
                break;
            case VLC_META_ENGINE_DESCRIPTION:
                item_col_strings.append( qtr( VLC_META_DESCRIPTION ) );
                break;
            case VLC_META_ENGINE_DURATION:
                item_col_strings.append( qtr( "Duration" ) );
                break;
            case VLC_META_ENGINE_GENRE:
                item_col_strings.append( qtr( VLC_META_GENRE ) );
                break;
            case VLC_META_ENGINE_COLLECTION:
                item_col_strings.append( qtr( VLC_META_COLLECTION ) );
                break;
            case VLC_META_ENGINE_SEQ_NUM:
                item_col_strings.append( qtr( VLC_META_SEQ_NUM ) );
                break;
            case VLC_META_ENGINE_TRACKID:
                item_col_strings.append( qtr( VLC_META_TRACKID ) );
                break;
            default:
                break;
            }
        }
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
    char psz_duration[MSTRTIME_MAX_SIZE];
    char *psz_meta;

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

#define ADD_META( item, meta ) \
    psz_meta = input_item_Get ## meta ( item->p_input ); \
    item_col_strings.append( qfu( psz_meta ) ); \
    free( psz_meta );

    for( int i_index=1; i_index <= VLC_META_ENGINE_ART_URL; i_index *= 2 )
    {
        if( parentItem->i_showflags & i_index )
        {
            switch( i_index )
            {
            case VLC_META_ENGINE_ARTIST:
                ADD_META( p_item, Artist );
                break;
            case VLC_META_ENGINE_TITLE:
                char *psz_title;
                psz_title = input_item_GetTitle( p_item->p_input );
                if( psz_title )
                {
                    ADD_META( p_item, Title );
                    free( psz_title );
                }
                else
                {
                    psz_title = input_item_GetName( p_item->p_input );
                    if( psz_title )
                    {
                        item_col_strings.append( qfu( psz_title ) );
                    }
                    free( psz_title );
                }
                break;
            case VLC_META_ENGINE_DESCRIPTION:
                ADD_META( p_item, Description );
                break;
            case VLC_META_ENGINE_DURATION:
                secstotimestr( psz_duration,
                    input_item_GetDuration( p_item->p_input ) / 1000000 );
                item_col_strings.append( QString( psz_duration ) );
                break;
            case VLC_META_ENGINE_GENRE:
                ADD_META( p_item, Genre );
                break;
            case VLC_META_ENGINE_COLLECTION:
                ADD_META( p_item, Album );
                break;
            case VLC_META_ENGINE_SEQ_NUM:
                ADD_META( p_item, TrackNum );
                break;
            case VLC_META_ENGINE_TRACKID:
                item_col_strings.append( QString::number( p_item->i_id ) );
                break;
            default:
                break;
            }
        }
    }
#undef ADD_META
}

