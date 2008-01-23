/*****************************************************************************
 * item.c: input_item management
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#include <vlc/vlc.h>
#include "vlc_playlist.h"
#include "vlc_interface.h"

#include "input_internal.h"

static void GuessType( input_item_t *p_item );

void input_item_SetMeta( input_item_t *p_i, vlc_meta_type_t meta_type, const char *psz_val )
{
    vlc_event_t event;

    vlc_mutex_lock( &p_i->lock );
    if( !p_i->p_meta )
        p_i->p_meta = vlc_meta_New();
    vlc_meta_Set( p_i->p_meta, meta_type, psz_val );
    vlc_mutex_unlock( &p_i->lock );

    /* Notify interested third parties */
    event.type = vlc_InputItemMetaChanged;
    event.u.input_item_meta_changed.meta_type = meta_type;
    vlc_event_send( &p_i->event_manager, &event );
}

/**
 * Get the item from an input thread
 */
input_item_t *input_GetItem( input_thread_t *p_input )
{
    assert( p_input && p_input->p );
    return p_input->p->input.p_item;
}

/**
 * Get a info item from a given category in a given input item.
 *
 * \param p_i The input item to get info from
 * \param psz_cat String representing the category for the info
 * \param psz_name String representing the name of the desired info
 * \return A pointer to the string with the given info if found, or an
 *         empty string otherwise. The caller should free the returned
 *         pointer.
 */
char *input_ItemGetInfo( input_item_t *p_i,
                              const char *psz_cat,
                              const char *psz_name )
{
    int i,j;

    vlc_mutex_lock( &p_i->lock );

    for( i = 0 ; i< p_i->i_categories  ; i++ )
    {
        info_category_t *p_cat = p_i->pp_categories[i];

        if( !psz_cat || strcmp( p_cat->psz_name, psz_cat ) )
            continue;

        for( j = 0; j < p_cat->i_infos ; j++ )
        {
            if( !strcmp( p_cat->pp_infos[j]->psz_name, psz_name ) )
            {
                char *psz_ret = strdup( p_cat->pp_infos[j]->psz_value );
                vlc_mutex_unlock( &p_i->lock );
                return psz_ret;
            }
        }
    }
    vlc_mutex_unlock( &p_i->lock );
    return strdup( "" );
}

static void input_ItemDestroy ( gc_object_t *p_this )
{
    vlc_object_t *p_obj = (vlc_object_t *)p_this->p_destructor_arg;
    input_item_t *p_input = (input_item_t *) p_this;
    int i;

    playlist_t *p_playlist = pl_Yield( p_obj );
    input_ItemClean( p_input );

    ARRAY_BSEARCH( p_playlist->input_items,->i_id, int, p_input->i_id, i);
    if( i != -1 )
        ARRAY_REMOVE( p_playlist->input_items, i);

    pl_Release( p_obj );
    free( p_input );
}

void input_ItemAddOption( input_item_t *p_input,
                          const char *psz_option )
{
    if( !psz_option ) return;
    vlc_mutex_lock( &p_input->lock );
    INSERT_ELEM( p_input->ppsz_options, p_input->i_options,
                 p_input->i_options, strdup( psz_option ) );
    vlc_mutex_unlock( &p_input->lock );
}

void input_ItemAddOptionNoDup( input_item_t *p_input,
                               const char *psz_option )
{
    int i;
    if( !psz_option ) return ;
    vlc_mutex_lock( &p_input->lock );
    for( i = 0 ; i< p_input->i_options; i++ )
    {
        if( !strcmp( p_input->ppsz_options[i], psz_option ) )
        {
            vlc_mutex_unlock(& p_input->lock );
            return;
        }
    }
    TAB_APPEND( p_input->i_options, p_input->ppsz_options, strdup( psz_option));    vlc_mutex_unlock( &p_input->lock );
}


int input_ItemAddInfo( input_item_t *p_i,
                            const char *psz_cat,
                            const char *psz_name,
                            const char *psz_format, ... )
{
    va_list args;
    int i;
    info_t *p_info = NULL;
    info_category_t *p_cat = NULL ;

    vlc_mutex_lock( &p_i->lock );

    for( i = 0 ; i < p_i->i_categories ; i ++ )
    {
        if( !strcmp( p_i->pp_categories[i]->psz_name, psz_cat ) )
        {
            p_cat = p_i->pp_categories[i];
            break;
        }
    }
    if( !p_cat )
    {
        if( !(p_cat = (info_category_t *)malloc( sizeof(info_category_t) )) )
        {
            vlc_mutex_unlock( &p_i->lock );
            return VLC_ENOMEM;
        }
        p_cat->psz_name = strdup( psz_cat );
        p_cat->i_infos = 0;
        p_cat->pp_infos = 0;
        INSERT_ELEM( p_i->pp_categories, p_i->i_categories, p_i->i_categories,
                     p_cat );
    }

    for( i = 0; i< p_cat->i_infos; i++ )
    {
        if( !strcmp( p_cat->pp_infos[i]->psz_name, psz_name ) )
        {
            p_info = p_cat->pp_infos[i];
            break;
        }
    }

    if( !p_info )
    {
        if( ( p_info = (info_t *)malloc( sizeof( info_t ) ) ) == NULL )
        {
            vlc_mutex_unlock( &p_i->lock );
            return VLC_ENOMEM;
        }
        INSERT_ELEM( p_cat->pp_infos, p_cat->i_infos, p_cat->i_infos, p_info );
        p_info->psz_name = strdup( psz_name );
    }
    else
    {
        free( p_info->psz_value );
    }

    va_start( args, psz_format );
    if( vasprintf( &p_info->psz_value, psz_format, args) )
        p_info->psz_value = NULL;
    va_end( args );

    vlc_mutex_unlock( &p_i->lock );

    return p_info->psz_value ? VLC_SUCCESS : VLC_ENOMEM;
}

input_item_t *input_ItemGetById( playlist_t *p_playlist, int i_id )
{
    int i;
    ARRAY_BSEARCH( p_playlist->input_items, ->i_id, int, i_id, i);
    if( i != -1 )
        return ARRAY_VAL( p_playlist->input_items, i);
    return NULL;
}

input_item_t *__input_ItemNewExt( vlc_object_t *p_obj, const char *psz_uri,
                                  const char *psz_name,
                                  int i_options,
                                  const char *const *ppsz_options,
                                  mtime_t i_duration )
{
    return input_ItemNewWithType( p_obj, psz_uri, psz_name,
                                  i_options, ppsz_options,
                                  i_duration, ITEM_TYPE_UNKNOWN );
}


input_item_t *input_ItemNewWithType( vlc_object_t *p_obj, const char *psz_uri,
                                const char *psz_name,
                                int i_options,
                                const char *const *ppsz_options,
                                mtime_t i_duration,
                                int i_type )
{
    playlist_t *p_playlist = pl_Yield( p_obj );
    DECMALLOC_NULL( p_input, input_item_t );

    input_ItemInit( p_obj, p_input );
    vlc_gc_init( p_input, input_ItemDestroy, (void *)p_obj );

    PL_LOCK;
    p_input->i_id = ++p_playlist->i_last_input_id;
    ARRAY_APPEND( p_playlist->input_items, p_input );
    PL_UNLOCK;
    pl_Release( p_obj );

    p_input->b_fixed_name = VLC_FALSE;

    if( psz_uri )
        p_input->psz_uri = strdup( psz_uri );
    else
        p_input->psz_uri = NULL;

    p_input->i_type = i_type;
    p_input->b_prefers_tree = VLC_FALSE;

    if( p_input->i_type == ITEM_TYPE_UNKNOWN )
        GuessType( p_input );

    if( psz_name != NULL )
        p_input->psz_name = strdup( psz_name );
    else if( p_input->i_type == ITEM_TYPE_FILE )
    {
        const char *psz_filename = strrchr( p_input->psz_uri, DIR_SEP_CHAR );
        if( psz_filename && *psz_filename == DIR_SEP_CHAR )
            psz_filename++;
        p_input->psz_name = strdup( psz_filename && *psz_filename
                                    ? psz_filename : p_input->psz_uri );
    }
    else
        p_input->psz_name = strdup( p_input->psz_uri );

    p_input->i_duration = i_duration;
    p_input->ppsz_options = NULL;
    p_input->i_options = 0;

    for( int i = 0; i < i_options; i++ )
        input_ItemAddOption( p_input, ppsz_options[i] );
    return p_input;
}

/* Guess the type of the item using the beginning of the mrl */
static void GuessType( input_item_t *p_item)
{
    int i;
    static struct { const char *psz_search; int i_type; }  types_array[] =
    {
        { "http", ITEM_TYPE_NET },
        { "dvd", ITEM_TYPE_DISC },
        { "cdda", ITEM_TYPE_CDDA },
        { "mms", ITEM_TYPE_NET },
        { "rtsp", ITEM_TYPE_NET },
        { "udp", ITEM_TYPE_NET },
        { "rtp", ITEM_TYPE_NET },
        { "vcd", ITEM_TYPE_DISC },
        { "v4l", ITEM_TYPE_CARD },
        { "dshow", ITEM_TYPE_CARD },
        { "pvr", ITEM_TYPE_CARD },
        { "dvb", ITEM_TYPE_CARD },
        { "qpsk", ITEM_TYPE_CARD },
        { "sdp", ITEM_TYPE_NET },
        { NULL, 0 }
    };

    for( i = 0; types_array[i].psz_search != NULL; i++ )
    {
        if( !strncmp( p_item->psz_uri, types_array[i].psz_search,
                      strlen( types_array[i].psz_search ) ) )
        {
            p_item->i_type = types_array[i].i_type;
            return;
        }
    }
    p_item->i_type = ITEM_TYPE_FILE;
}
