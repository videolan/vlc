/*****************************************************************************
 * intf_playlist.c : Playlist management functions
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: intf_playlist.c,v 1.11 2001/12/07 18:33:08 sam Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */
#include <errno.h>                                                 /* ENOMEM */

#include "common.h"
#include "intf_msg.h"
#include "threads.h"

#include "intf_playlist.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void NextItem( playlist_t * p_playlist );

/*****************************************************************************
 * intf_PlaylistCreate: create playlist
 *****************************************************************************
 * Create a playlist structure.
 *****************************************************************************/
playlist_t * intf_PlaylistCreate ( void )
{
    playlist_t *p_playlist;

    /* Allocate structure */
    p_playlist = malloc( sizeof( playlist_t ) );
    if( !p_playlist )
    {
        intf_ErrMsg( "intf error: couldn't create playlist (%s)",
                     strerror( ENOMEM ) );
        return( NULL );
    }

    return( p_playlist );
}

/*****************************************************************************
 * intf_PlaylistInit: initialize playlist
 *****************************************************************************
 * Initialize a playlist structure.
 *****************************************************************************/
void intf_PlaylistInit ( playlist_t * p_playlist )
{
    vlc_mutex_init( &p_playlist->change_lock );

    p_playlist->i_index = -1;    /* -1 means we are not playing anything yet */
    p_playlist->i_size = 0;

    p_playlist->i_mode = PLAYLIST_FORWARD;
    p_playlist->i_seed = 0;
    p_playlist->b_stopped = 0;

    /* There is no current item */
    p_playlist->current.i_type = 0;
    p_playlist->current.i_status = 0;
    p_playlist->current.psz_name = NULL;

    /* The playlist is empty */
    p_playlist->p_item = NULL;

    intf_WarnMsg( 3, "intf: playlist initialized" );
}

/*****************************************************************************
 * intf_PlaylistAdd: add an item to the playlist
 *****************************************************************************
 * Add an item to the playlist at position i_pos. If i_pos is PLAYLIST_END,
 * add it at the end regardless of the playlist current size.
 *****************************************************************************/
int intf_PlaylistAdd( playlist_t * p_playlist, int i_pos,
                      const char * psz_item )
{
    int i_index;
    playlist_item_t * p_item;

    vlc_mutex_lock( &p_playlist->change_lock );

    if( i_pos == PLAYLIST_END )
    {
        i_pos = p_playlist->i_size;
    }
    else if( i_pos > p_playlist->i_size )
    {
        intf_ErrMsg( "intf error: inserting item beyond playlist size" );
        vlc_mutex_unlock( &p_playlist->change_lock );
        return( -1 );
    }

    /* Increment playlist size */
    p_playlist->i_size++;
    p_playlist->p_item = realloc( p_playlist->p_item,
                    p_playlist->i_size * sizeof( playlist_item_t ) );

    /* Move second place of the playlist to make room for new item */
    for( i_index = p_playlist->i_size - 1; i_index > i_pos; i_index-- )
    {
        p_playlist->p_item[ i_index ] = p_playlist->p_item[ i_index - 1 ];
    }

    /* Insert the new item */
    p_item = &p_playlist->p_item[ i_pos ];

    p_item->i_type = 0;
    p_item->i_status = 0;
    p_item->psz_name = strdup( psz_item );

    intf_WarnMsg( 3, "intf: added `%s' to playlist", psz_item );

    vlc_mutex_unlock( &p_playlist->change_lock );

    return( 0 );
}

/*****************************************************************************
 * intf_PlaylistNext: switch to next playlist item
 *****************************************************************************
 * Switch to the next item of the playlist. If there is no next item, the
 * position of the resulting item is set to -1.
 *****************************************************************************/
void intf_PlaylistNext( playlist_t * p_playlist )
{
    vlc_mutex_lock( &p_playlist->change_lock );

    NextItem( p_playlist );

    vlc_mutex_unlock( &p_playlist->change_lock );
}

/*****************************************************************************
 * intf_PlaylistPrev: switch to previous playlist item
 *****************************************************************************
 * Switch to the previous item of the playlist. If there is no previous
 * item, the position of the resulting item is set to -1.
 *****************************************************************************/
void intf_PlaylistPrev( playlist_t * p_playlist )
{
    vlc_mutex_lock( &p_playlist->change_lock );

    p_playlist->i_mode = -p_playlist->i_mode;
    NextItem( p_playlist );
    p_playlist->i_mode = -p_playlist->i_mode;

    vlc_mutex_unlock( &p_playlist->change_lock );
}

/*****************************************************************************
 * intf_PlaylistDelete: delete an item from the playlist
 *****************************************************************************
 * Delete the item in the playlist with position i_pos.
 *****************************************************************************/
int intf_PlaylistDelete( playlist_t * p_playlist, int i_pos )
{
    int i_index;
    char * psz_name;

    vlc_mutex_lock( &p_playlist->change_lock );

    if( !p_playlist->i_size || i_pos >= p_playlist->i_size )
    {
        intf_ErrMsg( "intf error: deleting item beyond playlist size" );
        vlc_mutex_unlock( &p_playlist->change_lock );
        return( -1 );
    }

    /* Store the location of the item's name */
    psz_name = p_playlist->p_item[ i_pos ].psz_name;

    /* Fill the room by moving the next items */
    for( i_index = i_pos; i_index < p_playlist->i_size - 1; i_index++ )
    {
        p_playlist->p_item[ i_index ] = p_playlist->p_item[ i_index + 1 ];
    }

    if( i_pos < p_playlist->i_index )
        p_playlist->i_index--;


    /* Decrement playlist size */
    p_playlist->i_size--;
    p_playlist->p_item = realloc( p_playlist->p_item,
                    p_playlist->i_size * sizeof( playlist_item_t ) );

    intf_WarnMsg( 3, "intf: removed `%s' from playlist", psz_name );


    /* Delete the item */
    free( psz_name );

    vlc_mutex_unlock( &p_playlist->change_lock );

    return( 0 );
}

/*****************************************************************************
 * intf_PlaylistDestroy: destroy the playlist
 *****************************************************************************
 * Delete all items in the playlist and free the playlist structure.
 *****************************************************************************/
void intf_PlaylistDestroy( playlist_t * p_playlist )
{
    int i_index;

    for( i_index = p_playlist->i_size - 1; p_playlist->i_size; i_index-- )
    {
        intf_PlaylistDelete( p_playlist, i_index );
    }

    vlc_mutex_destroy( &p_playlist->change_lock );

    if( p_playlist->current.psz_name != NULL )
    {
        free( p_playlist->current.psz_name );
    }

    free( p_playlist );

    intf_WarnMsg( 3, "intf: playlist destroyed" );
}

/*****************************************************************************
 * intf_PlaylistJumpto: go to a specified position in playlist.
 *****************************************************************************/
void intf_PlaylistJumpto( playlist_t * p_playlist , int i_pos)
{
    vlc_mutex_lock( &p_playlist->change_lock );

    p_playlist->i_index = i_pos;

    if( p_playlist->i_index != -1 )
    {
        if( p_playlist->current.psz_name != NULL )
        {
            free( p_playlist->current.psz_name );
        }

        p_playlist->current = p_playlist->p_item[ p_playlist->i_index ];
        p_playlist->current.psz_name
                            = strdup( p_playlist->current.psz_name );

    }
    p_main->p_playlist->b_stopped = 0;

    vlc_mutex_unlock( &p_playlist->change_lock );
}

/* URL-decode a file: URL path, return NULL if it's not what we expect */
void intf_UrlDecode( char *encoded_path )
{
    char *tmp = NULL, *cur = NULL, *ext = NULL;
    int realchar;

    if( !encoded_path || *encoded_path == '\0' )
    {
        return;
    }

    cur = encoded_path ;

    tmp = calloc(strlen(encoded_path) + 1,  sizeof(char) );

    while ( ( ext = strchr(cur, '%') ) != NULL)
    {
        strncat(tmp, cur, (ext - cur) / sizeof(char));
        ext++;

        if (!sscanf(ext, "%2x", &realchar))
        {
            free(tmp);
            return;
        }

        tmp[strlen(tmp)] = (char)realchar;

        cur = ext + 2;
    }

    strcat(tmp, cur);
    strcpy(encoded_path,tmp);
}

/*****************************************************************************
 * Following functions are local
 *****************************************************************************/

/*****************************************************************************
 * NextItem: select next playlist item
 *****************************************************************************
 * This function copies the next playlist item to the current structure,
 * depending on the playlist browsing mode.
 *****************************************************************************/
static void NextItem( playlist_t * p_playlist )
{
    if( !p_playlist->i_size )
    {
        p_playlist->i_index = -1;
    }
    else
    {
        switch( p_playlist->i_mode )
        {
        case PLAYLIST_FORWARD:
            p_playlist->i_index++;
            if( p_playlist->i_index > p_playlist->i_size - 1 )
            {
                p_playlist->i_index = -1;
            }
        break;

        case PLAYLIST_FORWARD_LOOP:
            p_playlist->i_index++;
            if( p_playlist->i_index > p_playlist->i_size - 1 )
            {
                p_playlist->i_index = 0;
            }
        break;

        case PLAYLIST_BACKWARD:
            p_playlist->i_index--;
            if( p_playlist->i_index < 0 )
            {
                p_playlist->i_index = -1;
            }
        break;

        case PLAYLIST_BACKWARD_LOOP:
            p_playlist->i_index--;
            if( p_playlist->i_index < 0 )
            {
                p_playlist->i_index = p_playlist->i_size - 1;
            }
        break;

        case PLAYLIST_REPEAT_CURRENT:
            /* Just repeat what we were doing */
            if( p_playlist->i_index < 0
                    || p_playlist->i_index > p_playlist->i_size - 1 )
            {
                p_playlist->i_index = 0;
            }
        break;

        case PLAYLIST_RANDOM:
            /* FIXME: TODO ! */
            p_playlist->i_index++;
            if( p_playlist->i_index > p_playlist->i_size - 1 )
            {
                p_playlist->i_index = 0;
            }
        break;
        }

        /* Duplicate the playlist entry */
        if( p_playlist->i_index != -1 )
        {
            if( p_playlist->current.psz_name != NULL )
            {
                free( p_playlist->current.psz_name );
            }
            p_playlist->current = p_playlist->p_item[ p_playlist->i_index ];
            p_playlist->current.psz_name
                                = strdup( p_playlist->current.psz_name );
        }
    }
}

