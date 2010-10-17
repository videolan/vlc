/*****************************************************************************
 * item_list.h : Item list data structure for Watching system
 *****************************************************************************
 * Copyright (C) 2008-2010 the VideoLAN team  and AUTHORS
 * $Id$
 *
 * Authors: Srikanth Raju <srikiraju at gmail dot com>
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

#ifndef ML_ITEM_LIST_H
#define ML_ITEM_LIST_H

#include <vlc_input.h>
#include <vlc_arrays.h>

struct watch_thread_t;
typedef struct watch_thread_t watch_thread_t;
typedef struct item_list_t item_list_t;

/**
 * Definition of item_list_t
 */
struct item_list_t {
    input_item_t *p_item;     /**< Input item */
    ml_media_t   *p_media;    /**< Media item */
    item_list_t  *p_next;     /**< Next element in the list */
    int           i_media_id; /**< Media id */
    int           i_age;      /**< Time spent in this list without activity */
    int           i_refs;     /**< Number of important refs */
    int           i_update;   /**< Flag set when the input item is updated:
                                    0: no update,
                                    1: meta update,
                                    2: increment play count,
                                    3: both */
};

#define ML_ITEMLIST_HASH_LENGTH 40

#define il_foreachhashlist( a, b, c )                                          \
            for( int c = 0 ; c < ML_ITEMLIST_HASH_LENGTH ; c++ )         \
                for( item_list_t* b = a[c]; b; b = b->p_next )

#define il_foreachlist( a, b )  for( item_list_t* b = a ; b; b = b->p_next )

#define item_list_add( a, b, c ) __item_list_add( a, b, c, false )

int __item_list_add( watch_thread_t *p_wt, ml_media_t* p_media,
                     input_item_t *p_item, bool );
item_list_t* item_list_delMedia( watch_thread_t *p_wt, int i_media_id );
item_list_t* item_list_delItem( watch_thread_t *p_wt, input_item_t *p_item, bool );
item_list_t* item_list_listitemOfMediaId( watch_thread_t *p_wt, int i_media_id );
input_item_t* item_list_itemOfMediaId( watch_thread_t *p_wt, int i_media_id );
ml_media_t* item_list_mediaOfMediaId( watch_thread_t *p_wt, int i_media_id );
ml_media_t* item_list_mediaOfItem( watch_thread_t *p_wt, input_item_t* p_item, bool );
int item_list_mediaIdOfItem( watch_thread_t *p_wt, input_item_t *p_item );
int item_list_updateInput( watch_thread_t *p_wt, input_item_t *p_item,
                           bool b_play_count );
void item_list_destroy( watch_thread_t* p_wt );

/**
 * @brief Simple hash function
 * @param item_id Hash Key
 * @return Hash index
 */
static inline int item_hash( input_item_t* p_item )
{
    return DictHash( p_item->psz_uri, ML_ITEMLIST_HASH_LENGTH );
}

#endif /* ML_ITEM_LIST_H */
