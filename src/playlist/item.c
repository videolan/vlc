/*****************************************************************************
 * item.c : Playlist item functions
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "vlc_input.h"
#include "vlc_playlist.h"

static void GuessType( input_item_t *p_item);

/**
 * Create a new item, without adding it to the playlist
 *
 * \param p_obj a vlc object (anyone will do)
 * \param psz_uri the mrl of the item
 * \param psz_name a text giving a name or description of the item
 * \return the new item or NULL on failure
 */

playlist_item_t * __playlist_ItemNew( vlc_object_t *p_obj,
                                      const char *psz_uri,
                                      const char *psz_name )
{
    return playlist_ItemNewWithType( p_obj, psz_uri,
                                     psz_name, ITEM_TYPE_UNKNOWN );
}

playlist_item_t * playlist_ItemNewWithType( vlc_object_t *p_obj,
                                            const char *psz_uri,
                                            const char *psz_name,
                                            int i_type )
{
    playlist_item_t * p_item;

    if( psz_uri == NULL) return NULL;

    p_item = malloc( sizeof( playlist_item_t ) );
    if( p_item == NULL ) return NULL;

    memset( p_item, 0, sizeof( playlist_item_t ) );

    vlc_input_item_Init( p_obj, &p_item->input );
    p_item->input.b_fixed_name = VLC_FALSE;

    p_item->input.psz_uri = strdup( psz_uri );

    if( psz_name != NULL ) p_item->input.psz_name = strdup( psz_name );
    else p_item->input.psz_name = strdup ( psz_uri );

    p_item->input.i_type = i_type;

    p_item->b_enabled = VLC_TRUE;
    p_item->i_nb_played = 0;

    p_item->i_children = -1;
    p_item->pp_children = NULL;

    p_item->i_flags = 0;
    p_item->i_flags |= PLAYLIST_SKIP_FLAG;
    p_item->i_flags |= PLAYLIST_SAVE_FLAG;

    p_item->input.i_duration = -1;
    p_item->input.ppsz_options = NULL;
    p_item->input.i_options = 0;

    vlc_mutex_init( p_obj, &p_item->input.lock );

    if( p_item->input.i_type == ITEM_TYPE_UNKNOWN )
        GuessType( &p_item->input );

    return p_item;
}

/**
 * Copy a playlist item
 *
 * Creates a new item with name, mrl and meta infor like the
 * source. Does not copy children for node type items.
 * \param p_obj any vlc object, needed for mutex init
 * \param p_item the item to copy
 * \return pointer to the new item, or NULL on error
 * \note function takes the lock on p_item
 */
playlist_item_t *__playlist_ItemCopy( vlc_object_t *p_obj,
                                      playlist_item_t *p_item )
{
    playlist_item_t *p_res;
    int i;
    vlc_mutex_lock( &p_item->input.lock );

    p_res = malloc( sizeof( playlist_item_t ) );
    if( p_res == NULL )
    {
        vlc_mutex_unlock( &p_item->input.lock );
        return NULL;
    }

    *p_res = *p_item;
    vlc_mutex_init( p_obj, &p_res->input.lock );

    if( p_item->input.i_options )
        p_res->input.ppsz_options =
            malloc( p_item->input.i_options * sizeof(char*) );
    for( i = 0; i < p_item->input.i_options; i++ )
    {
        p_res->input.ppsz_options[i] = strdup( p_item->input.ppsz_options[i] );
    }

    if( p_item->i_children != -1 )
    {
        msg_Warn( p_obj, "not copying playlist items children" );
        p_res->i_children = -1;
        p_res->pp_children = NULL;
    }
    p_res->i_parents = 0;
    p_res->pp_parents = NULL;
    
    if( p_item->input.psz_name )
        p_res->input.psz_name = strdup( p_item->input.psz_name );
    if( p_item->input.psz_uri )
        p_res->input.psz_uri = strdup( p_item->input.psz_uri );
    
    if( p_item->input.i_es )
    {
        p_res->input.es =
            (es_format_t**)malloc( p_item->input.i_es * sizeof(es_format_t*));
        for( i = 0; i < p_item->input.i_es; i++ )
        {
            p_res->input.es[ i ] = (es_format_t*)malloc(sizeof(es_format_t*));
            es_format_Copy( p_res->input.es[ i ],
                         p_item->input.es[ i ] );
        }
    }
    if( p_item->input.i_categories )
    {
        p_res->input.pp_categories = NULL;
        p_res->input.i_categories = 0;
        for( i = 0; i < p_item->input.i_categories; i++ )
        {
            info_category_t *p_incat;
            p_incat = p_item->input.pp_categories[i];
            if( p_incat->i_infos )
            {
                int j;
                for( j = 0; j < p_incat->i_infos; j++ )
                {
                    vlc_input_item_AddInfo( &p_res->input, p_incat->psz_name,
                                            p_incat->pp_infos[j]->psz_name,
                                            "%s", /* to be safe */
                                            p_incat->pp_infos[j]->psz_value );
                }
            }
        }
    }

    vlc_mutex_unlock( &p_item->input.lock );
    return p_res;
}

/**
 * Deletes a playlist item
 *
 * \param p_item the item to delete
 * \return nothing
 */
int playlist_ItemDelete( playlist_item_t *p_item )
{
    vlc_mutex_lock( &p_item->input.lock );

    if( p_item->input.psz_name ) free( p_item->input.psz_name );
    if( p_item->input.psz_uri ) free( p_item->input.psz_uri );

    /* Free the info categories */
    if( p_item->input.i_categories > 0 )
    {
        int i, j;

        for( i = 0; i < p_item->input.i_categories; i++ )
        {
            info_category_t *p_category = p_item->input.pp_categories[i];

            for( j = 0; j < p_category->i_infos; j++)
            {
                if( p_category->pp_infos[j]->psz_name )
                {
                    free( p_category->pp_infos[j]->psz_name);
                }
                if( p_category->pp_infos[j]->psz_value )
                {
                    free( p_category->pp_infos[j]->psz_value );
                }
                free( p_category->pp_infos[j] );
            }

            if( p_category->i_infos ) free( p_category->pp_infos );
            if( p_category->psz_name ) free( p_category->psz_name );
            free( p_category );
        }

        free( p_item->input.pp_categories );
    }

    for( ; p_item->input.i_options > 0; p_item->input.i_options-- )
    {
        free( p_item->input.ppsz_options[p_item->input.i_options - 1] );
        if( p_item->input.i_options == 1 ) free( p_item->input.ppsz_options );
    }

    for( ; p_item->i_parents > 0 ; )
    {
        struct item_parent_t *p_parent =  p_item->pp_parents[0];
        REMOVE_ELEM( p_item->pp_parents, p_item->i_parents, 0 );
        free( p_parent );
    }

    vlc_mutex_unlock( &p_item->input.lock );
    vlc_mutex_destroy( &p_item->input.lock );

    free( p_item );

    return VLC_SUCCESS;
}

/**
 *  Add a option to one item ( no need for p_playlist )
 *
 * \param p_item the item on which we want the info
 * \param psz_option the option
 * \return 0 on success
 */
int playlist_ItemAddOption( playlist_item_t *p_item, const char *psz_option )
{
    if( !psz_option ) return VLC_EGENERIC;

    vlc_mutex_lock( &p_item->input.lock );
    INSERT_ELEM( p_item->input.ppsz_options, p_item->input.i_options,
                 p_item->input.i_options, strdup( psz_option ) );
    vlc_mutex_unlock( &p_item->input.lock );

    return VLC_SUCCESS;
}

/**
 * Add a parent to an item
 *
 * \param p_item the item
 * \param i_view the view in which the parent is
 * \param p_parent the parent to add
 * \return nothing
 */
int playlist_ItemAddParent( playlist_item_t *p_item, int i_view,
                            playlist_item_t *p_parent )
{
   vlc_bool_t b_found = VLC_FALSE;
   int i;

   for( i= 0; i< p_item->i_parents ; i++ )
   {
       if( p_item->pp_parents[i]->i_view == i_view )

       {
           b_found = VLC_TRUE;
           break;
       }
   }
   if( b_found == VLC_FALSE )
   {

       struct item_parent_t *p_ip = (struct item_parent_t *)
               malloc(sizeof(struct item_parent_t) );
       p_ip->i_view = i_view;
       p_ip->p_parent = p_parent;

       INSERT_ELEM( p_item->pp_parents,
                    p_item->i_parents, p_item->i_parents,
                    p_ip );
   }
   return VLC_SUCCESS;
}

/**
 * Copy all parents from parent to child
 */
int playlist_CopyParents( playlist_item_t *p_parent,
                           playlist_item_t *p_child )
{
    int i=0;
    for( i= 0 ; i< p_parent->i_parents; i ++ )
    {
        playlist_ItemAddParent( p_child,
                                p_parent->pp_parents[i]->i_view,
                                p_parent );
    }
    return VLC_SUCCESS;
}


/**********************************************************************
 * playlist_item_t structure accessors
 * These functions give access to the fields of the playlist_item_t
 * structure
 **********************************************************************/


/**
 * Set the name of a playlist item
 *
 * \param p_item the item
 * \param psz_name the new name
 * \return VLC_SUCCESS on success, VLC_EGENERIC on failure
 */
int playlist_ItemSetName( playlist_item_t *p_item, char *psz_name )
{
    if( psz_name && p_item )
    {
        if( p_item->input.psz_name ) free( p_item->input.psz_name );
        p_item->input.psz_name = strdup( psz_name );
        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

/**
 * Set the duration of a playlist item
 * This function must be entered with the item lock
 *
 * \param p_item the item
 * \param i_duration the new duration
 * \return VLC_SUCCESS on success, VLC_EGENERIC on failure
 */
int playlist_ItemSetDuration( playlist_item_t *p_item, mtime_t i_duration )
{
    char psz_buffer[MSTRTIME_MAX_SIZE];
    if( p_item )
    {
        p_item->input.i_duration = i_duration;
        if( i_duration != -1 )
        {
            secstotimestr( psz_buffer, (int)(i_duration/1000000) );
        }
        else
        {
            memcpy( psz_buffer, "--:--:--", sizeof("--:--:--") );
        }
        vlc_input_item_AddInfo( &p_item->input, _("General") , _("Duration"),
                                "%s", psz_buffer );

        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

/*
 * Guess the type of the item using the beginning of the mrl */
static void GuessType( input_item_t *p_item)
{
    int i;
    static struct { char *psz_search; int i_type; }  types_array[] =
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

#if 0 /* Unused */
    static struct { char *psz_search; int i_type; } exts_array[] =
    {
        { "mp3", ITEM_TYPE_AFILE },
        { NULL, 0 }
    };
#endif

    for( i = 0; types_array[i].psz_search != NULL; i++ )
    {
        if( !strncmp( p_item->psz_uri, types_array[i].psz_search,
                      strlen( types_array[i].psz_search ) ) )
        {
            p_item->i_type = types_array[i].i_type;
            return;
        }
    }
    p_item->i_type = ITEM_TYPE_VFILE;
}
