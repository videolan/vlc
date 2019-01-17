/*****************************************************************************
 * intf.c: interface configuration handling
 *****************************************************************************
 * Copyright (C) 2001-2007 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <assert.h>

/* Adds an extra interface to the configuration */
void config_AddIntf( const char *psz_intf )
{
    assert( psz_intf );

    char *psz_config, *psz_parser;
    size_t i_len = strlen( psz_intf );

    psz_config = psz_parser = config_GetPsz( "control" );
    while( psz_parser )
    {
        if( !strncmp( psz_intf, psz_parser, i_len ) )
        {
            free( psz_config );
            return;
        }
        psz_parser = strchr( psz_parser, ':' );
        if( psz_parser ) psz_parser++; /* skip the ':' */
    }
    free( psz_config );

    psz_config = psz_parser = config_GetPsz( "extraintf" );
    while( psz_parser )
    {
        if( !strncmp( psz_intf, psz_parser, i_len ) )
        {
            free( psz_config );
            return;
        }
        psz_parser = strchr( psz_parser, ':' );
        if( psz_parser ) psz_parser++; /* skip the ':' */
    }

    /* interface not found in the config, let's add it */
    if( psz_config && strlen( psz_config ) > 0 )
    {
        char *psz_newconfig;
        if( asprintf( &psz_newconfig, "%s:%s", psz_config, psz_intf ) != -1 )
        {
            config_PutPsz( "extraintf", psz_newconfig );
            free( psz_newconfig );
        }
    }
    else
        config_PutPsz( "extraintf", psz_intf );

    free( psz_config );
}

/* Removes an extra interface from the configuration */
void config_RemoveIntf( const char *psz_intf )
{
    assert( psz_intf );

    char *psz_config, *psz_parser;
    size_t i_len = strlen( psz_intf );

    psz_config = psz_parser = config_GetPsz( "extraintf" );
    while( psz_parser )
    {
        if( !strncmp( psz_intf, psz_parser, i_len ) )
        {
            char *psz_newconfig;
            char *psz_end = psz_parser + i_len;
            if( *psz_end == ':' ) psz_end++;
            *psz_parser = '\0';
            if( asprintf( &psz_newconfig, "%s%s", psz_config, psz_end ) != -1 )
            {
                config_PutPsz( "extraintf", psz_newconfig );
                free( psz_newconfig );
            }
            break;
        }
        psz_parser = strchr( psz_parser, ':' );
        if( psz_parser ) psz_parser++; /* skip the ':' */
    }
    free( psz_config );

    psz_config = psz_parser = config_GetPsz( "control" );
    while( psz_parser )
    {
        if( !strncmp( psz_intf, psz_parser, i_len ) )
        {
            char *psz_newconfig;
            char *psz_end = psz_parser + i_len;
            if( *psz_end == ':' ) psz_end++;
            *psz_parser = '\0';
            if( asprintf( &psz_newconfig, "%s%s", psz_config, psz_end ) != -1 )
            {
                config_PutPsz( "control", psz_newconfig );
                free( psz_newconfig );
            }
            break;
        }
        psz_parser = strchr( psz_parser, ':' );
        if( psz_parser ) psz_parser++; /* skip the ':' */
    }
    free( psz_config );
}

/*
 * Returns true if the specified extra interface is present in the
 * configuration, false if not
 */
bool config_ExistIntf( const char *psz_intf )
{
    assert( psz_intf );

    char *psz_config, *psz_parser;
    size_t i_len = strlen( psz_intf );

    psz_config = psz_parser = config_GetPsz( "extraintf" );
    while( psz_parser )
    {
        if( !strncmp( psz_parser, psz_intf, i_len ) )
        {
            free( psz_config );
            return true;
        }
        psz_parser = strchr( psz_parser, ':' );
        if( psz_parser ) psz_parser++; /* skip the ':' */
    }
    free( psz_config );

    psz_config = psz_parser = config_GetPsz( "control" );
    while( psz_parser )
    {
        if( !strncmp( psz_parser, psz_intf, i_len ) )
        {
            free( psz_config );
            return true;
        }
        psz_parser = strchr( psz_parser, ':' );
        if( psz_parser ) psz_parser++; /* skip the ':' */
    }
    free( psz_config );

    return false;
}

