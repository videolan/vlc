/*****************************************************************************
 * playlist.c : Playlist groups management functions
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/
#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "vlc_playlist.h"

/**
 * Create a group
 *
 * Create a new group
 * \param p_playlist pointer to a playlist
 * \param psz_name the name of the group to be created
 * \return a pointer to the created group, or NULL on error
 */
playlist_group_t *playlist_CreateGroup( playlist_t *p_playlist, char *psz_name)
{
    playlist_group_t *p_group;
    int i;

    for( i = 0 ; i < p_playlist->i_groups; i++ )
    {
        if( !strcasecmp( p_playlist->pp_groups[i]->psz_name , psz_name ) )
        {
            msg_Info( p_playlist, "this group already exists");
            return p_playlist->pp_groups[i];
        }
    }

    /* Allocate the group structure */
    if( ( p_group = malloc( sizeof(playlist_group_t) ) ) == NULL )
    {
        msg_Err( p_playlist, "out of memory" );
        return NULL;
    }

    p_group->psz_name = strdup( psz_name );
    p_group->i_id = ++p_playlist->i_last_group;

    msg_Dbg(p_playlist,"creating group %s with id %i at position %i",
                     p_group->psz_name,
                     p_group->i_id,
                     p_playlist->i_groups);

    INSERT_ELEM ( p_playlist->pp_groups,
                  p_playlist->i_groups,
                  p_playlist->i_groups,
                  p_group );

    return p_group;
}

/**
 * Destroy a group
 *
 * \param p_playlist the playlist to remove the group from
 * \param i_id the identifier of the group to remove
 * \return VLC_SUCCESS
 */
int playlist_DeleteGroup( playlist_t *p_playlist, int i_id )
{
    int i;

    for( i=0 ; i<= p_playlist->i_groups; i++ )
    {
        playlist_group_t *p_group = p_playlist->pp_groups[i];

        if( p_group->i_id == i_id )
        {

            if( p_group->psz_name )
            {
                free( p_group->psz_name );
            }
            REMOVE_ELEM( p_playlist->pp_groups,
                         p_playlist->i_groups,
                         i );
            free( p_group );

            return VLC_SUCCESS;
        }
    }
    return VLC_SUCCESS;
}

/**
 * Find the name of the group given its ID
 *
 * \param p_playlist the playlist where to find the group
 * \param i_id the ID to search for
 * \return the name of the group
 */
char *playlist_FindGroup( playlist_t *p_playlist, int i_id )
{
    int i;
    for( i=0 ; i< p_playlist->i_groups; i++ )
    {
        if( p_playlist->pp_groups[i]->i_id == i_id )
        {
            if( p_playlist->pp_groups[i]->psz_name)
                return strdup( p_playlist->pp_groups[i]->psz_name );
        }
    }
    return NULL;
}

/**
 * Find the id of a group given its name
 *
 * \param p_playlist the playlist where to find the group
 * \param psz_name the name to search for
 * \return the id of the group
 */
int playlist_GroupToId( playlist_t *p_playlist, char *psz_name )
{
    int i;
    for( i = 0 ; i< p_playlist->i_groups; i++ )
    {
        if( p_playlist->pp_groups[i]->psz_name)
        {
            if( ! strcasecmp( p_playlist->pp_groups[i]->psz_name, psz_name ) )
            {
                return p_playlist->pp_groups[i]->i_id;
            }
        }
    }
    return VLC_SUCCESS;
}
