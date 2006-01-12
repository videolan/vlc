/*****************************************************************************
 * services_discovery.c : Manage playlist services_discovery modules
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
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
#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/sout.h>
#include <vlc/input.h>

#include "vlc_playlist.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static void RunSD( services_discovery_t *p_sd );


/***************************************************************************
***************************************************************************/

int playlist_ServicesDiscoveryAdd( playlist_t *p_playlist,
                                   const char *psz_module )
{
    services_discovery_t *p_sd;

    p_sd = vlc_object_create( p_playlist, VLC_OBJECT_SD );
    p_sd->pf_run = NULL;

    p_sd->p_module = module_Need( p_sd, "services_discovery", psz_module, 0 );

    if( p_sd->p_module == NULL )
    {
        msg_Err( p_playlist, "no suitable services discovery module" );
        vlc_object_destroy( p_sd );
        return VLC_EGENERIC;
    }

    p_sd->psz_module = strdup( psz_module );
    p_sd->b_die = VLC_FALSE;

    vlc_mutex_lock( &p_playlist->object_lock );

    INSERT_ELEM( p_playlist->pp_sds, p_playlist->i_sds, p_playlist->i_sds,
                 p_sd );

    vlc_mutex_unlock( &p_playlist->object_lock );

    if( vlc_thread_create( p_sd, "services_discovery", RunSD,
                           VLC_THREAD_PRIORITY_LOW, VLC_FALSE ) )
    {
        msg_Err( p_sd, "cannot create services discovery thread" );
        vlc_object_destroy( p_sd );
        return VLC_EGENERIC;
    }


    return VLC_SUCCESS;
}

int playlist_ServicesDiscoveryRemove( playlist_t * p_playlist,
                                       const char *psz_module )
{
    int i;
    services_discovery_t *p_sd = NULL;
    vlc_mutex_lock( &p_playlist->object_lock );

    for( i = 0 ; i< p_playlist->i_sds ; i ++ )
    {
        if( !strcmp( psz_module, p_playlist->pp_sds[i]->psz_module ) )
        {
            p_sd = p_playlist->pp_sds[i];
            REMOVE_ELEM( p_playlist->pp_sds, p_playlist->i_sds, i );
            break;
        }
    }

    if( p_sd )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        p_sd->b_die = VLC_TRUE;
        vlc_thread_join( p_sd );
        free( p_sd->psz_module );
        module_Unneed( p_sd, p_sd->p_module );
        vlc_mutex_lock( &p_playlist->object_lock );
        vlc_object_destroy( p_sd );
    }
    else
    {
        msg_Warn( p_playlist, "module %s is not loaded", psz_module );
        vlc_mutex_unlock( &p_playlist->object_lock );
        return VLC_EGENERIC;
    }

    vlc_mutex_unlock( &p_playlist->object_lock );
    return VLC_SUCCESS;
}

vlc_bool_t playlist_IsServicesDiscoveryLoaded( playlist_t * p_playlist,
                                              const char *psz_module )
{
    int i;
    vlc_mutex_lock( &p_playlist->object_lock );

    for( i = 0 ; i< p_playlist->i_sds ; i ++ )
    {
        if( !strcmp( psz_module, p_playlist->pp_sds[i]->psz_module ) )
        {
            vlc_mutex_unlock( &p_playlist->object_lock );
            return VLC_TRUE;
        }
    }
    vlc_mutex_unlock( &p_playlist->object_lock );
    return VLC_FALSE;
}


/**
 * Load all service discovery modules in a string
 *
 * \param p_playlist the playlist
 * \param psz_modules a list of modules separated by commads
 * return VLC_SUCCESS or an error
 */
int playlist_AddSDModules( playlist_t *p_playlist, char *psz_modules )
{
    if( psz_modules && *psz_modules )
    {
        char *psz_parser = psz_modules;
        char *psz_next;

        while( psz_parser && *psz_parser )
        {
            while( *psz_parser == ' ' || *psz_parser == ':' )
            {
                psz_parser++;
            }

            if( (psz_next = strchr( psz_parser, ':' ) ) )
            {
                *psz_next++ = '\0';
            }
            if( *psz_parser == '\0' )
            {
                break;
            }

            playlist_ServicesDiscoveryAdd( p_playlist, psz_parser );

            psz_parser = psz_next;
        }
    }
    return VLC_SUCCESS;
}

static void RunSD( services_discovery_t *p_sd )
{
    p_sd->pf_run( p_sd );
    return;
}
