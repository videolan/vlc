/*****************************************************************************
 * lirc.c : lirc module for vlc
 *****************************************************************************
 * Copyright (C) 2003-2005 the VideoLAN team
 * $Id$
 *
 * Author: Sigmund Augdal Helberg <dnumgis@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <fcntl.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include <lirc/lirc_client.h>

/*****************************************************************************
 * intf_sys_t: description and status of FB interface
 *****************************************************************************/
struct intf_sys_t
{
    struct lirc_config *config;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );
static void Run     ( intf_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_CONTROL );
    set_description( _("Infrared remote control interface") );
    set_capability( "interface", 0 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: initialize interface
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    int i_fd;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        msg_Err( p_intf, "out of memory" );
        return 1;
    }

    p_intf->pf_run = Run;

    i_fd = lirc_init( "vlc", 1 );
    if( i_fd == -1 )
    {
        msg_Err( p_intf, "lirc initialisation failed" );
        free( p_intf->p_sys );
        return 1;
    }

    /* We want polling */
    fcntl( i_fd, F_SETFL, fcntl( i_fd, F_GETFL ) | O_NONBLOCK );

    if( lirc_readconfig( NULL, &p_intf->p_sys->config, NULL ) != 0 )
    {
        msg_Err( p_intf, "failure while reading lirc config" );
        lirc_deinit();
        free( p_intf->p_sys );
        return 1;
    }

    return 0;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Destroy structure */
    lirc_freeconfig( p_intf->p_sys->config );
    lirc_deinit();
    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    char *code, *c;

    while( !p_intf->b_die )
    {
        /* Sleep a bit */
        msleep( INTF_IDLE_SLEEP );

        /* We poll the lircsocket */
        if( lirc_nextcode(&code) != 0 )
        {
            break;
        }

        if( code == NULL )
        {
            continue;
        }

        while( !p_intf->b_die
                && lirc_code2char( p_intf->p_sys->config, code, &c ) == 0
                && c != NULL )
        {
            vlc_value_t keyval;

            if( strncmp( "key-", c, 4 ) )
            {
                msg_Err( p_intf, "this doesn't appear to be a valid keycombo lirc sent us. Please look at the doc/lirc/example.lirc file in VLC" );
                break;
            }
            keyval.i_int = config_GetInt( p_intf, c );
            var_Set( p_intf->p_vlc, "key-pressed", keyval );
        }
        free( code );
    }
}

