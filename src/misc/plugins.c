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
#include "defs.h"

#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */

#if defined(SYS_LINUX) || defined(SYS_BSD) || defined(SYS_GNU)
#include <dlfcn.h>                           /* dlopen(), dlsym(), dlclose() */
#endif

#ifdef SYS_BEOS
#include <image.h>
#endif

#include "plugins.h"

#define PLUGIN_PATH_COUNT 5

int RequestPlugin ( plugin_id_t * p_plugin, char * psz_mask, char * psz_name )
{
    int i_count, i_length;
    char * psz_plugin;
    char * psz_plugin_path[ PLUGIN_PATH_COUNT ] =
    {
        ".", PLUGIN_PATH,
        "plugins/aout", "plugins/vout", "plugins/intf" /* these ones should disappear */
    };

    i_length = strlen( psz_mask ) + strlen( psz_name );

    for ( i_count = 0 ; i_count < PLUGIN_PATH_COUNT ; i_count++ )
    {
        psz_plugin = malloc( strlen(psz_plugin_path[i_count]) + i_length + 6 );
        sprintf( psz_plugin, "%s/%s_%s.so", psz_plugin_path[i_count], psz_mask, psz_name );
#ifdef SYS_BEOS
        *p_plugin = load_addon_image( psz_plugin );
#else  /* SYS_BEOS */
        *p_plugin = dlopen( psz_plugin, RTLD_NOW | RTLD_GLOBAL );
#endif /* SYS_BEOS */
        free( psz_plugin );

#ifdef SYS_BEOS
        if( *p_plugin >= 0 )
            return( 0 );
#else
        if( *p_plugin != NULL )
            return( 0 );
#endif
    }

    return( -1 );
}

void TrashPlugin ( plugin_id_t plugin )
{
#ifdef SYS_BEOS
    unload_add_on( plugin );
#else
    dlclose( plugin );
#endif
}

void * GetPluginFunction ( plugin_id_t plugin, char *psz_name )
{
#ifdef SYS_BEOS
    void * p_func;
    
    if( get_image_symbol( plugin, psz_name, B_SYMBOL_TYPE_TEXT, &p_func ) )
        return( NULL );
    else
        return( p_func );    
#else
    return( dlsym(plugin, psz_name) );
#endif
}

