/*****************************************************************************
 * history.c: vlc_history_t (web-browser-like back/forward history) handling
 *****************************************************************************
 * Copyright (C) 2004 Commonwealth Scientific and Industrial Research
 *                    Organisation (CSIRO) Australia
 * Copyright (C) 2004 the VideoLAN team
 *
 * $Id$
 *
 * Authors: Andre Pang <Andre.Pang@csiro.au>
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

#include <vlc_common.h>
#include <vlc_input.h>

#include "history.h"

#include "xarray.h"

#ifdef HAVE_STDLIB_H
#   include <stdlib.h>                                          /* realloc() */
#endif

#undef HISTORY_DEBUG

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void history_Dump( history_t *p_history );

/*****************************************************************************
 * Local structure lock
 *****************************************************************************/

/*****************************************************************************
 * Actual history code
 *****************************************************************************/

history_t *history_New( void )
{
   history_t *p_new_history;
 
   p_new_history = calloc( 1, sizeof( struct history_t ) );
   if( p_new_history == NULL ) return NULL;

   p_new_history->p_xarray = xarray_New( 0 );
   if( p_new_history->p_xarray == NULL )
   {
       free( p_new_history );
       return NULL;
   }

#ifndef HISTORY_DEBUG
   /* make dummy reference to history_Dump to avoid compiler warnings */
   while (0)
   {
       void *p_tmp;

       p_tmp = history_Dump;
   }
#endif

   return p_new_history;
}

bool history_GoBackSavingCurrentItem ( history_t *p_history,
                                             history_item_t *p_item )
{
    history_PruneAndInsert( p_history, p_item );

    /* PruneAndInsert will increment the index, so we need to go
     * back one position to reset the index to the place we were at
     * before saving the current state, and then go back one more to
     * actually go back */
    p_history->i_index -= 2;

#ifdef HISTORY_DEBUG
    history_Dump( p_history );
#endif
    return true;
}

static void history_Dump( history_t *p_history )
{
    unsigned int i_count;
    int i;

    if( xarray_Count( p_history->p_xarray, &i_count ) != XARRAY_SUCCESS )
        return;

    for (i = 0; i < (int) i_count; i++)
    {
        history_item_t *p_item;
        void *pv_item;

        xarray_ObjectAtIndex( p_history->p_xarray, i, &pv_item );

        p_item = (history_item_t *) pv_item;

        if( p_item == NULL )
            fprintf( stderr, "HISTORY: [%d] NULL\n", i );
        else
        {
            fprintf( stderr, "HISTORY: [%d] %p (%p->%s)\n", i, p_item,
                     p_item->psz_uri, p_item->psz_uri );
        }
    }
}

bool history_GoForwardSavingCurrentItem ( history_t *p_history,
                                                history_item_t *p_item )
{
#ifdef HISTORY_DEBUG
    history_Dump( p_history );
#endif

    if( xarray_ReplaceObject( p_history->p_xarray, p_history->i_index, p_item )
        == XARRAY_SUCCESS )
    {
        p_history->i_index++;
        return true;
    }
    else
    {
        return false;
    }
}

bool history_CanGoBack( history_t *p_history )
{
    if( p_history->i_index > 0 )
        return true;
    else
        return false;
}

bool history_CanGoForward( history_t *p_history )
{
    unsigned int i_count;

    if( xarray_Count( p_history->p_xarray, &i_count ) != XARRAY_SUCCESS )
        return false;

    if( p_history->i_index < i_count )
        return true;
    else
        return false;
}

history_item_t *history_Item( history_t *p_history )
{
    history_item_t *p_item;
    void *pv_item;

    if( xarray_ObjectAtIndex( p_history->p_xarray, p_history->i_index,
                              &pv_item )
        == XARRAY_SUCCESS )
    {
        p_item = (history_item_t *) pv_item;
        return p_item;
    }
    else
    {
        return NULL;
    }
}

void history_Prune( history_t *p_history )
{
    xarray_RemoveObjectsAfter( p_history->p_xarray, p_history->i_index );
    xarray_RemoveObject( p_history->p_xarray, p_history->i_index );
}

void history_PruneAndInsert( history_t *p_history, history_item_t *p_item )
{
    unsigned int i_count;

    xarray_Count( p_history->p_xarray, &i_count );

    if( i_count == 0 )
    {
        xarray_InsertObject( p_history->p_xarray, p_item, 0 );
        p_history->i_index = 1;
    }
    else
    {
        history_Prune( p_history );
        xarray_InsertObject( p_history->p_xarray, p_item, p_history->i_index );
        p_history->i_index++;
    }
}

unsigned int history_Count( history_t *p_history )
{
    unsigned int i_count;
    xarray_Count( p_history->p_xarray, &i_count );
    return i_count;
}

unsigned int history_Index( history_t *p_history )
{
    return p_history->i_index;
}

history_item_t * historyItem_New( char *psz_name, char *psz_uri )
{
    history_item_t *p_history_item = NULL;

    p_history_item = (history_item_t *) malloc( sizeof(history_item_t) );
    if( !p_history_item ) return NULL;

    p_history_item->psz_uri = strdup( psz_uri );
    p_history_item->psz_name = strdup( psz_name );

    return p_history_item;
}

