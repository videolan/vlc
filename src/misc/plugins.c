/*****************************************************************************
 * plugins.c : Dynamic plugin management functions
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/

#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <dlfcn.h>                           /* dlopen(), dlsym(), dlclose() */

#define PLUGIN_PATH_COUNT 5

void * RequestPlugin ( char * psz_mask, char * psz_name )
{
    int i_count, i_length;
    void * fd;
    char * psz_plugin;
    char * psz_plugin_path[ PLUGIN_PATH_COUNT ] =
    {
        ".",
        PLUGIN_PATH,
        /* these ones should disappear */
        "./audio_output",
        "./video_output",
        "./interface"
    };

    i_length = strlen( psz_mask ) + strlen( psz_name );

    for ( i_count = 0 ; i_count < PLUGIN_PATH_COUNT ; i_count++ )
    {
        psz_plugin = malloc( strlen(psz_plugin_path[i_count]) + i_length + 6 );
        sprintf( psz_plugin, "%s/%s_%s.so", psz_plugin_path[i_count], psz_mask, psz_name );
        fd = dlopen( psz_plugin, RTLD_NOW | RTLD_GLOBAL );
        free( psz_plugin );

        if( fd != NULL )
            return( fd );
    }

    return( 0 );
}

void TrashPlugin ( void * p_plugin )
{
    dlclose( p_plugin );
}

void *GetPluginFunction ( void *p_plugin, char *psz_name )
{
    return( dlsym(p_plugin, psz_name) );
}

