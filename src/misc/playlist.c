/*****************************************************************************
 * playlist.c : Playlist management functions
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
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

#include "config.h"

#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */
#include <errno.h>                                                 /* ENOMEM */

#include "common.h"

#include "intf_msg.h"
#include "playlist.h"

#include "main.h"

/* Local prototypes */
//int TestPlugin     ( plugin_id_t *p_plugin_id, char * psz_name );
//int AllocatePlugin ( plugin_id_t plugin_id, plugin_bank_t * p_bank );

playlist_t * playlist_Create ( void )
{
    playlist_t *p_playlist;

    /* Allocate structure */
    p_playlist = malloc( sizeof( playlist_t ) );
    if( !p_playlist )
    {
        intf_ErrMsg("playlist error: %s", strerror( ENOMEM ) );
        return( NULL );
    }

    p_playlist->i_index = 0;
    p_playlist->p_list = NULL;

    intf_Msg("Playlist initialized");
    return( p_playlist );
}

void playlist_Init( playlist_t * p_playlist, int i_optind )
{
    int i_list_index = 0;
    int i_index = 0;
    int i_argc = p_main->i_argc;

    if( i_optind < i_argc )
    {
        i_list_index = i_argc - i_optind;

        p_playlist->p_list = malloc( i_list_index * sizeof( int ) );

        while( i_argc - i_index > i_optind )
        {
            p_playlist->p_list[ i_index ] =
                            p_main->ppsz_argv[ i_argc - i_index - 1];
            i_index++;
        }
    }
    else
    {
        /* if no file was asked, get stream from the network */
        p_playlist->p_list = NULL;
    }

    p_main->p_playlist->i_index = i_list_index;
}

void playlist_Destroy( playlist_t * p_playlist )
{
    free( p_playlist );
}

/*
 * Following functions are local
 */

