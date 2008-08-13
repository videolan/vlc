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

#include <fcntl.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_osd.h>

#include <lirc/lirc_client.h>

#define LIRC_TEXT N_("Change the lirc configuration file.")
#define LIRC_LONGTEXT N_( \
    "Tell lirc to read this configuration file. By default it " \
    "searches in the users home directory." )

/*****************************************************************************
 * intf_sys_t: description and status of FB interface
 *****************************************************************************/
struct intf_sys_t
{
    char *psz_file;
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
    set_shortname( N_("Infrared") );
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_CONTROL );
    set_description( N_("Infrared remote control interface") );
    set_capability( "interface", 0 );
    set_callbacks( Open, Close );

    add_string( "lirc-file", NULL, NULL,
                LIRC_TEXT, LIRC_LONGTEXT, true );
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

    p_intf->p_sys->psz_file = var_CreateGetNonEmptyString( p_intf, "lirc-file" );

    i_fd = lirc_init( "vlc", 1 );
    if( i_fd == -1 )
    {
        msg_Err( p_intf, "lirc initialisation failed" );
        free( p_intf->p_sys->psz_file );
        free( p_intf->p_sys );
        return 1;
    }

    /* We want polling */
    fcntl( i_fd, F_SETFL, fcntl( i_fd, F_GETFL ) | O_NONBLOCK );

    if( lirc_readconfig( p_intf->p_sys->psz_file, &p_intf->p_sys->config, NULL ) != 0 )
    {
        msg_Err( p_intf, "failure while reading lirc config" );
        lirc_deinit();
        free( p_intf->p_sys->psz_file );
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
    free( p_intf->p_sys->psz_file );
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

    while( !intf_ShouldDie( p_intf ) )
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

        while( !intf_ShouldDie( p_intf )
                && (lirc_code2char( p_intf->p_sys->config, code, &c ) == 0)
                && (c != NULL) )
        {
            vlc_value_t keyval;

            if( !strncmp( "key-", c, 4 ) )
            {
                keyval.i_int = config_GetInt( p_intf, c );
                var_Set( p_intf->p_libvlc, "key-pressed", keyval );
            }
            else if( !strncmp( "menu ", c, 5)  )
            {
                if( !strncmp( c, "menu on", 7 ) ||
                    !strncmp( c, "menu show", 9 ))
                    osd_MenuShow( VLC_OBJECT(p_intf) );
                else if( !strncmp( c, "menu off", 8 ) ||
                         !strncmp( c, "menu hide", 9 ) )
                    osd_MenuHide( VLC_OBJECT(p_intf) );
                else if( !strncmp( c, "menu up", 7 ) )
                    osd_MenuUp( VLC_OBJECT(p_intf) );
                else if( !strncmp( c, "menu down", 9 ) )
                    osd_MenuDown( VLC_OBJECT(p_intf) );
                else if( !strncmp( c, "menu left", 9 ) )
                    osd_MenuPrev( VLC_OBJECT(p_intf) );
                else if( !strncmp( c, "menu right", 10 ) )
                    osd_MenuNext( VLC_OBJECT(p_intf) );
                else if( !strncmp( c, "menu select", 11 ) )
                    osd_MenuActivate( VLC_OBJECT(p_intf) );
                else
                {
                    msg_Err( p_intf, "Please provide one of the following parameters:" );
                    msg_Err( p_intf, "[on|off|up|down|left|right|select]" );
                    break;
                }
            }
            else
            {
                msg_Err( p_intf, "this doesn't appear to be a valid keycombo lirc sent us. Please look at the doc/lirc/example.lirc file in VLC" );
                break;
            }
        }
        free( code );
    }
}
