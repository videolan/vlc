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
#include <vlc/vlc.h>
#include "vlc_playlist.h"
#include "playlist_internal.h"

static void RunSD( services_discovery_t *p_sd );

int playlist_ServicesDiscoveryAdd( playlist_t *p_playlist,  const char *psz_modules )
{
    const char *psz_parser = psz_modules ?: "";
    int retval = VLC_SUCCESS;

    for (;;)
    {
        while( *psz_parser == ' ' || *psz_parser == ':' || *psz_parser == ',' )
            psz_parser++;

        if( *psz_parser == '\0' )
            break;

        const char *psz_next = strchr( psz_parser, ':' );
        if( psz_next == NULL )
            psz_next = psz_parser + strlen( psz_parser );

        char psz_plugin[psz_next - psz_parser + 1];
        memcpy (psz_plugin, psz_parser, sizeof (psz_plugin) - 1);
        psz_plugin[sizeof (psz_plugin) - 1] = '\0';
        psz_parser = psz_next;

        /* Perform the addition */
        msg_Dbg( p_playlist, "Add services_discovery %s", psz_plugin );
        services_discovery_t *p_sd = vlc_object_create( p_playlist,
                                                        VLC_OBJECT_SD );
        if( p_sd == NULL )
            return VLC_ENOMEM;

        p_sd->pf_run = NULL;
        p_sd->p_module = module_Need( p_sd, "services_discovery", psz_plugin, 0 );

        if( p_sd->p_module == NULL )
        {
            msg_Err( p_playlist, "no suitable services discovery module" );
            vlc_object_destroy( p_sd );
            retval = VLC_EGENERIC;
            continue;
        }
        p_sd->psz_module = strdup( psz_plugin );
        p_sd->b_die = VLC_FALSE;

        PL_LOCK;
        TAB_APPEND( p_playlist->i_sds, p_playlist->pp_sds, p_sd );
        PL_UNLOCK;

        if ((p_sd->pf_run != NULL)
         && vlc_thread_create( p_sd, "services_discovery", RunSD,
                               VLC_THREAD_PRIORITY_LOW, VLC_FALSE))
        {
            msg_Err( p_sd, "cannot create services discovery thread" );
            vlc_object_destroy( p_sd );
            retval = VLC_EGENERIC;
            continue;
        }
    }

    return retval;
}

int playlist_ServicesDiscoveryRemove( playlist_t * p_playlist,
                                       const char *psz_module )
{
    int i;
    services_discovery_t *p_sd = NULL;

    PL_LOCK;
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
        PL_UNLOCK;
        p_sd->b_die = VLC_TRUE;
        if( p_sd->pf_run ) vlc_thread_join( p_sd );

        free( p_sd->psz_module );
        module_Unneed( p_sd, p_sd->p_module );
        PL_LOCK;
        vlc_object_destroy( p_sd );
    }
    else
    {
        msg_Warn( p_playlist, "module %s is not loaded", psz_module );
        PL_UNLOCK;
        return VLC_EGENERIC;
    }
    PL_UNLOCK;
    return VLC_SUCCESS;
}

vlc_bool_t playlist_IsServicesDiscoveryLoaded( playlist_t * p_playlist,
                                              const char *psz_module )
{
    int i;
    PL_LOCK;

    for( i = 0 ; i< p_playlist->i_sds ; i ++ )
    {
        if( !strcmp( psz_module, p_playlist->pp_sds[i]->psz_module ) )
        {
            PL_UNLOCK;
            return VLC_TRUE;
        }
    }
    PL_UNLOCK;
    return VLC_FALSE;
}

static void RunSD( services_discovery_t *p_sd )
{
    p_sd->pf_run( p_sd );
    return;
}
