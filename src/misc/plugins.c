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
#include <sys/types.h>                                               /* open */
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>                                                 /* close */

#if defined(HAVE_DLFCN_H)                                /* Linux, BSD, Hurd */
#include <dlfcn.h>                           /* dlopen(), dlsym(), dlclose() */

#elif defined(HAVE_IMAGE_H)                                          /* BeOS */
#include <image.h>

#else
#error no dynamic plugins available on your system !
#endif

#ifdef SYS_BEOS
#include "beos_specific.h"
#endif

#include "common.h"

#include "intf_msg.h"
#include "plugins.h"

/* Local prototypes */
char * TestPlugin     ( plugin_id_t *p_plugin_id, char * psz_name );
int    AllocatePlugin ( plugin_id_t plugin_id, plugin_bank_t * p_bank,
                        char * psz_filename );

plugin_bank_t * bank_Create( void )
{
    plugin_bank_t *p_bank;
    int i;

    /* Allocate structure */
    p_bank = malloc( sizeof( plugin_bank_t ) );
    if( !p_bank )
    {
        intf_ErrMsg("plugin bank error: %s", strerror( ENOMEM ) );
        return( NULL );
    }

    /* Initialize structure */
    for( i = 0 ; i < MAX_PLUGIN_COUNT ; i++ )
    {
        p_bank->p_info[ i ] = NULL;
    }
    p_bank->i_plugin_count = MAX_PLUGIN_COUNT;

    intf_Msg("Plugin bank initialized");
    return( p_bank );
}

void bank_Init( plugin_bank_t * p_bank )
{
    plugin_id_t tmp;
    char * psz_filename;

    /* FIXME: we should browse all directories to get plugins */
#define SEEK_PLUGIN( name ) \
    psz_filename = TestPlugin( &tmp, name ); \
    if( psz_filename ) AllocatePlugin( tmp, p_bank, psz_filename );

    /* Arch plugins */
    SEEK_PLUGIN( "beos" );

    /* Low level Video */
    SEEK_PLUGIN( "x11" );
    SEEK_PLUGIN( "fb" );
    SEEK_PLUGIN( "glide" );
    SEEK_PLUGIN( "mga" );
     
    /* High level Video */
    SEEK_PLUGIN( "gnome" );
    SEEK_PLUGIN( "ggi" );
    SEEK_PLUGIN( "sdl" );
   
    /* Video calculus */
    SEEK_PLUGIN( "yuvmmx" );
    SEEK_PLUGIN( "yuv" );

    /* Audio pluins */
    SEEK_PLUGIN( "dsp" );
    SEEK_PLUGIN( "esd" );
    SEEK_PLUGIN( "alsa" );
    
    /* Dummy plugin */
    SEEK_PLUGIN( "dummy" );
    SEEK_PLUGIN( "null" );

#undef SEEK_PLUGIN
}

void bank_Destroy( plugin_bank_t * p_bank )
{
    int i;
    for( i = 0 ; i < p_bank->i_plugin_count ; i++ )
    {
        if( p_bank->p_info[ i ] != NULL )
        {
            free( p_bank->p_info[ i ]-> psz_filename );
        }
    }

    free( p_bank );
}

/*
 * Following functions are local
 */

char * TestPlugin ( plugin_id_t *p_plugin_id, char * psz_name )
{
    int i_count, i_length, i_fd;
    char * psz_plugin;
    char * psz_plugin_path[ ] =
    {
        ".",
        "lib", /* this one should disappear */
        PLUGIN_PATH,
        NULL
    };

    i_length = strlen( psz_name );

    for ( i_count = 0 ; psz_plugin_path[ i_count ] ; i_count++ )
    {
#ifdef SYS_BEOS
        char * psz_program_path;
        
        psz_program_path = beos_GetProgramPath();
        psz_plugin = malloc( strlen(psz_plugin_path[i_count]) +
                             strlen(psz_program_path) + i_length + 6 );
        sprintf( psz_plugin, "%s/%s/%s.so", psz_program_path,
                 psz_plugin_path[i_count], psz_name );        

        *p_plugin_id = load_add_on( psz_plugin );
#else
        psz_plugin = malloc( strlen(psz_plugin_path[i_count]) + i_length + 5 );
        sprintf( psz_plugin, "%s/%s.so", psz_plugin_path[i_count], psz_name );

        /* Try to open the plugin before dlopen()ing it. */
        i_fd = open( psz_plugin, O_RDONLY );
        if( i_fd == -1 )
        {
            free( psz_plugin );
            continue;
        }
        close( i_fd );
        
        *p_plugin_id = dlopen( psz_plugin, RTLD_NOW | RTLD_GLOBAL );
#endif

#ifdef SYS_BEOS
        if( *p_plugin_id >= 0 )
#else
        if( *p_plugin_id != NULL )
#endif
        {
            /* plugin successfuly dlopened */
            return( psz_plugin );
        }

#ifndef SYS_BEOS
        intf_ErrMsg( "plugin error: cannot open %s (%s)", psz_plugin, dlerror() );
#endif

        free( psz_plugin );
    }

    return( NULL );
}


int AllocatePlugin( plugin_id_t plugin_id, plugin_bank_t * p_bank,
                    char * psz_filename )
{
    typedef plugin_info_t * ( get_config_t ) ( void );
    get_config_t * p_func;   
    int i;

    for( i = 0 ; i < p_bank->i_plugin_count ; i++ )
    {
        if( p_bank->p_info[ i ] == NULL )
        {
            break;
        }
    }

    /* no room to store that plugin, quit */
    if( i == p_bank->i_plugin_count )
    {
        intf_ErrMsg( "plugin bank error: reached max plugin count (%i), "
                     "increase MAX_PLUGIN_COUNT", p_bank->i_plugin_count );
        return( -1 );
    }

    /* system-specific dynamic symbol loading */
    GET_PLUGIN( p_func, plugin_id, "GetConfig" );

    /* if it failed, just quit */
    if( !p_func )
    {
        return( -1 );
    }

    /* run the plugin function to initialize the structure */
    p_bank->p_info[ i ]            = p_func( );
    p_bank->p_info[ i ]->plugin_id = plugin_id;
    p_bank->p_info[ i ]->psz_filename = strdup( psz_filename );


    /* Tell the world we found it */
    intf_Msg( "Plugin %i: %s %s [0x%x]", i,
              p_bank->p_info[ i ]->psz_name,
              p_bank->p_info[ i ]->psz_version,
              p_bank->p_info[ i ]->i_score );

    /* return nicely */
    return( 0 );
}

#if 0
void TrashPlugin ( plugin_id_t plugin_id )
{
#ifdef SYS_BEOS
    unload_add_on( plugin_id );
#else
    dlclose( plugin_id );
#endif
}
#endif

