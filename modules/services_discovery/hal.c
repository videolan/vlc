/*****************************************************************************
 * hal.c :  HAL interface module
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
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
#include <vlc/intf.h>

#include <vlc/input.h>

#include "network.h"

#include <errno.h>                                                 /* ENOMEM */

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif

#include <hal/libhal.h>

#define MAX_LINE_LENGTH 256

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct services_discovery_sys_t
{
    LibHalContext *p_ctx;
    playlist_item_t *p_node_cat;
    playlist_item_t *p_node_one;
};
static void AddItem( services_discovery_t *p_sd, input_item_t * p_input );
static void Run    ( services_discovery_t *p_intf );

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("HAL devices detection") );
    set_category( CAT_PLAYLIST );
    set_subcategory( SUBCAT_PLAYLIST_SD );

    set_capability( "services_discovery", 0 );
    set_callbacks( Open, Close );

vlc_module_end();


/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys  = malloc(
                                    sizeof( services_discovery_sys_t ) );

    vlc_value_t         val;
    playlist_t          *p_playlist;

    DBusError           dbus_error;
    DBusConnection      *p_connection;

    p_sd->pf_run = Run;
    p_sd->p_sys  = p_sys;

    dbus_error_init( &dbus_error );

#ifdef HAVE_HAL_1
    p_sys->p_ctx = libhal_ctx_new();
    if( !p_sys->p_ctx )
    {
        msg_Err( p_sd, "unable to create HAL context") ;
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_connection = dbus_bus_get( DBUS_BUS_SYSTEM, &dbus_error );
    if( dbus_error_is_set( &dbus_error ) )
    {
        msg_Err( p_sd, "unable to connect to DBUS: %s", dbus_error.message );
        dbus_error_free( &dbus_error );
        free( p_sys );
        return VLC_EGENERIC;
    }
    libhal_ctx_set_dbus_connection( p_sys->p_ctx, p_connection );
    if( !libhal_ctx_init( p_sys->p_ctx, &dbus_error ) )
#else
    if( !(p_sys->p_ctx = hal_initialize( NULL, FALSE ) ) )
#endif
    {
        msg_Err( p_sd, "hal not available : %s", dbus_error.message );
        dbus_error_free( &dbus_error );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Create our playlist node */
    p_playlist = (playlist_t *)vlc_object_find( p_sd, VLC_OBJECT_PLAYLIST,
                                                FIND_ANYWHERE );
    if( !p_playlist )
    {
        msg_Warn( p_sd, "unable to find playlist, cancelling HAL listening");
        return VLC_EGENERIC;
    }

    playlist_NodesCreateForSD( p_playlist, _("Devices"),
                               &p_sys->p_node_cat, &p_sys->p_node_one );
    vlc_object_release( p_playlist );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys  = p_sd->p_sys;
    playlist_t *p_playlist =  (playlist_t *) vlc_object_find( p_sd,
                                 VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist )
    {
        playlist_NodeDelete( p_playlist, p_sys->p_node_cat, VLC_TRUE,VLC_TRUE );
        playlist_NodeDelete( p_playlist, p_sys->p_node_one, VLC_TRUE,VLC_TRUE );
        vlc_object_release( p_playlist );
    }
    free( p_sys );
}

static void AddDvd( services_discovery_t *p_sd, char *psz_device )
{
    char *psz_name;
    char *psz_uri;
    char *psz_blockdevice;
    input_item_t        *p_input;
#ifdef HAVE_HAL_1
    psz_name = libhal_device_get_property_string( p_sd->p_sys->p_ctx,
                                        psz_device, "volume.label", NULL );
    psz_blockdevice = libhal_device_get_property_string( p_sd->p_sys->p_ctx,
                                        psz_device, "block.device", NULL );
#else
    psz_name = hal_device_get_property_string( p_sd->p_sys->p_ctx,
                                               psz_device, "volume.label" );
    psz_blockdevice = hal_device_get_property_string( p_sd->p_sys->p_ctx,
                                                 psz_device, "block.device" );
#endif
    asprintf( &psz_uri, "dvd://%s", psz_blockdevice );
    /* Create the playlist item here */
    p_input = input_ItemNew( p_sd, psz_uri, psz_name );
    free( psz_uri );
#ifdef HAVE_HAL_1
    libhal_free_string( psz_device );
#else
    hal_free_string( psz_device );
#endif
    if( !p_input )
    {
        return;
    }
    AddItem( p_sd, p_input );
}

static void AddItem( services_discovery_t *p_sd, input_item_t * p_input )
{
    playlist_item_t *p_item;
    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_sd,
                                        VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( !p_playlist )
    {
        msg_Err( p_sd, "playlist not found" );
        return;
    }
    p_item = playlist_NodeAddInput( p_playlist, p_input,p_sd->p_sys->p_node_cat,
                                    PLAYLIST_APPEND, PLAYLIST_END );
    p_item->i_flags &= ~PLAYLIST_SKIP_FLAG;
    p_item = playlist_NodeAddInput( p_playlist, p_input,p_sd->p_sys->p_node_one,
                                    PLAYLIST_APPEND, PLAYLIST_END );
    p_item->i_flags &= ~PLAYLIST_SKIP_FLAG;

    vlc_object_release( p_playlist );
}

static void AddCdda( services_discovery_t *p_sd, char *psz_device )
{
    char *psz_name = "Audio CD";
    char *psz_uri;
    char *psz_blockdevice;
    input_item_t     *p_input;
#ifdef HAVE_HAL_1
    psz_blockdevice = libhal_device_get_property_string( p_sd->p_sys->p_ctx,
                                            psz_device, "block.device", NULL );
#else
    psz_blockdevice = hal_device_get_property_string( p_sd->p_sys->p_ctx,
                                                 psz_device, "block.device" );
#endif
    asprintf( &psz_uri, "cdda://%s", psz_blockdevice );
    /* Create the playlist item here */
    p_input = input_ItemNew( p_sd, psz_uri, psz_name );
    free( psz_uri );
#ifdef HAVE_HAL_1
    libhal_free_string( psz_device );
#else
    hal_free_string( psz_device );
#endif
    if( !p_input )
        return;
    AddItem( p_sd, p_input );
}

static void ParseDevice( services_discovery_t *p_sd, char *psz_device )
{
    char *psz_disc_type;
    services_discovery_sys_t    *p_sys  = p_sd->p_sys;
#ifdef HAVE_HAL_1
    if( libhal_device_property_exists( p_sys->p_ctx, psz_device,
                                       "volume.disc.type", NULL ) )
    {
        psz_disc_type = libhal_device_get_property_string( p_sys->p_ctx,
                                                        psz_device,
                                                        "volume.disc.type",
                                                        NULL );
#else
    if( hal_device_property_exists( p_sys->p_ctx, psz_device,
                                    "volume.disc.type" ) )
    {
        psz_disc_type = hal_device_get_property_string( p_sys->p_ctx,
                                                        psz_device,
                                                        "volume.disc.type" );
#endif
        if( !strcmp( psz_disc_type, "dvd_rom" ) )
        {
            AddDvd( p_sd, psz_device );
        }
        else if( !strcmp( psz_disc_type, "cd_rom" ) )
        {
#ifdef HAVE_HAL_1
            if( libhal_device_get_property_bool( p_sys->p_ctx, psz_device,
                                         "volume.disc.has_audio" , NULL ) )
#else
            if( hal_device_get_property_bool( p_sys->p_ctx, psz_device,
                                         "volume.disc.has_audio" ) )
#endif
            {
                AddCdda( p_sd, psz_device );
            }
        }
#ifdef HAVE_HAL_1
        libhal_free_string( psz_disc_type );
#else
        hal_free_string( psz_disc_type );
#endif
    }
}

/*****************************************************************************
 * Run: main HAL thread
 *****************************************************************************/
static void Run( services_discovery_t *p_sd )
{
    int i, i_devices;
    char **devices;
    services_discovery_sys_t    *p_sys  = p_sd->p_sys;

    /* parse existing devices first */
#ifdef HAVE_HAL_1
    if( ( devices = libhal_get_all_devices( p_sys->p_ctx, &i_devices, NULL ) ) )
#else
    if( ( devices = hal_get_all_devices( p_sys->p_ctx, &i_devices ) ) )
#endif
    {
        for( i = 0; i < i_devices; i++ )
        {
            ParseDevice( p_sd, devices[ i ] );
        }
    }

    while( !p_sd->b_die )
    {
        msleep( 100000 );
    }
}
