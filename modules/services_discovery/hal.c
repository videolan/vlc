/*****************************************************************************
 * hal.c :  HAL interface module
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * Copyright © 2006-2007 Rafaël Carré
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Rafaël Carré <funman at videolanorg>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_playlist.h>
#include <vlc_services_discovery.h>

#include <vlc_network.h>

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

/* store relation between item id and udi for ejection */
struct udi_input_id_t
{
    char            *psz_udi;
    input_item_t    *p_item;
};

struct services_discovery_sys_t
{
    vlc_thread_t            thread;
    LibHalContext           *p_ctx;
    DBusConnection          *p_connection;
    int                     i_devices_number;
    struct udi_input_id_t   **pp_devices;
};
static void *Run ( void * );
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

/* HAL callbacks */
void DeviceAdded( LibHalContext *p_ctx, const char *psz_udi );
void DeviceRemoved( LibHalContext *p_ctx, const char *psz_udi );

/* to retrieve p_sd in HAL callbacks */
services_discovery_t        *p_sd_global;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("HAL devices detection") )
    set_category( CAT_PLAYLIST )
    set_subcategory( SUBCAT_PLAYLIST_SD )

    set_capability( "services_discovery", 0 )
    set_callbacks( Open, Close )

vlc_module_end ()


/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys  = malloc(
                                    sizeof( services_discovery_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    DBusError           dbus_error;
    DBusConnection      *p_connection = NULL;

    p_sd_global = p_sd;
    p_sys->i_devices_number = 0;
    p_sys->pp_devices = NULL;

    p_sd->p_sys  = p_sys;

    dbus_error_init( &dbus_error );

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
        goto error;
    }
    libhal_ctx_set_dbus_connection( p_sys->p_ctx, p_connection );
    p_sys->p_connection = p_connection;
    if( !libhal_ctx_init( p_sys->p_ctx, &dbus_error ) )
    {
        msg_Err( p_sd, "hal not available : %s", dbus_error.message );
        goto error;
    }

    if( !libhal_ctx_set_device_added( p_sys->p_ctx, DeviceAdded ) ||
            !libhal_ctx_set_device_removed( p_sys->p_ctx, DeviceRemoved ) )
    {
        msg_Err( p_sd, "unable to add callback" );
        goto error;
    }

    if( vlc_clone( &p_sys->thread, Run, p_this, VLC_THREAD_PRIORITY_LOW ) )
        goto error;

    return VLC_SUCCESS;
error:
    if( p_connection )
        dbus_connection_unref( p_connection );
    dbus_error_free( &dbus_error );
    libhal_ctx_free( p_sys->p_ctx );
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys  = p_sd->p_sys;

    /*vlc_cancel( p_sys->thread );*/
    vlc_object_kill( p_sd );
    vlc_join( p_sys->thread, NULL );
    dbus_connection_unref( p_sys->p_connection );
    struct udi_input_id_t *p_udi_entry;

    while( p_sys->i_devices_number > 0 )
    {
        p_udi_entry = p_sys->pp_devices[0];
        free( p_udi_entry->psz_udi );
        TAB_REMOVE( p_sys->i_devices_number, p_sys->pp_devices,
                p_sys->pp_devices[0] );
        free( p_udi_entry );
    }
    p_sys->pp_devices = NULL;

    libhal_ctx_free( p_sys->p_ctx );

    free( p_sys );
}

static void AddItem( services_discovery_t *p_sd, input_item_t * p_input,
                    const char* psz_device )
{
    services_discovery_sys_t *p_sys  = p_sd->p_sys;
    services_discovery_AddItem( p_sd, p_input, NULL /* no category */ );

    struct udi_input_id_t *p_udi_entry;
    p_udi_entry = malloc( sizeof( struct udi_input_id_t ) );
    if( !p_udi_entry )
        return;
    p_udi_entry->psz_udi = strdup( psz_device );
    if( !p_udi_entry->psz_udi )
    {
        free( p_udi_entry );
        return;
    }

    vlc_gc_incref( p_input );
    p_udi_entry->p_item = p_input;
    TAB_APPEND( p_sys->i_devices_number, p_sys->pp_devices, p_udi_entry );
}

static void AddDvd( services_discovery_t *p_sd, const char *psz_device )
{
    char *psz_name;
    char *psz_uri;
    char *psz_blockdevice;
    input_item_t        *p_input;

    psz_name = libhal_device_get_property_string( p_sd->p_sys->p_ctx,
                                        psz_device, "volume.label", NULL );
    psz_blockdevice = libhal_device_get_property_string( p_sd->p_sys->p_ctx,
                                        psz_device, "block.device", NULL );

    if( asprintf( &psz_uri, "dvd://%s", psz_blockdevice ) == -1 )
        return;
    /* Create the playlist item here */
    p_input = input_item_New( p_sd, psz_uri, psz_name );
    free( psz_uri );
    if( !p_input )
    {
        return;
    }

    AddItem( p_sd, p_input, psz_device );

    vlc_gc_decref( p_input );
}

static void DelItem( services_discovery_t *p_sd, const char* psz_udi )
{
    services_discovery_sys_t    *p_sys  = p_sd->p_sys;

    int i;
    for( i = 0; i < p_sys->i_devices_number; i++ )
    { /*  looks for a matching udi */
        if( strcmp( psz_udi, p_sys->pp_devices[i]->psz_udi ) == 0 )
        { /* delete the corresponding item */    
            services_discovery_RemoveItem( p_sd, p_sys->pp_devices[i]->p_item );
            vlc_gc_decref( p_sys->pp_devices[i]->p_item );
            free( p_sys->pp_devices[i]->psz_udi );
            TAB_REMOVE( p_sys->i_devices_number, p_sys->pp_devices,
                    p_sys->pp_devices[i] );
        }
    }
}

static void AddCdda( services_discovery_t *p_sd, const char *psz_device )
{
    char *psz_uri;
    char *psz_blockdevice;
    input_item_t     *p_input;

    psz_blockdevice = libhal_device_get_property_string( p_sd->p_sys->p_ctx,
                                            psz_device, "block.device", NULL );

    if( asprintf( &psz_uri, "cdda://%s", psz_blockdevice ) == -1 )
        return;
    /* Create the item here */
    p_input = input_item_New( p_sd, psz_uri, "Audio CD" );
    free( psz_uri );
    if( !p_input )
        return;

    AddItem( p_sd, p_input, psz_device );

    vlc_gc_decref( p_input );
}

static void ParseDevice( services_discovery_t *p_sd, const char *psz_device )
{
    char *psz_disc_type;
    services_discovery_sys_t    *p_sys  = p_sd->p_sys;

    if( !libhal_device_property_exists( p_sys->p_ctx, psz_device,
                                       "volume.disc.type", NULL ) )
        return;

    psz_disc_type = libhal_device_get_property_string( p_sys->p_ctx,
                                                    psz_device,
                                                    "volume.disc.type",
                                                    NULL );
    if( !strncmp( psz_disc_type, "dvd_r", 5 ) )
    {
        if (libhal_device_get_property_bool( p_sys->p_ctx, psz_device,
                                     "volume.disc.is_videodvd", NULL ) )
        AddDvd( p_sd, psz_device );
    }
    else if( !strncmp( psz_disc_type, "cd_r", 4 ) )
    {
        if( libhal_device_get_property_bool( p_sys->p_ctx, psz_device,
                                     "volume.disc.has_audio" , NULL ) )
            AddCdda( p_sd, psz_device );
    }
}

/*****************************************************************************
 * Run: main HAL thread
 *****************************************************************************/
static void *Run( void *data )
{
    services_discovery_t     *p_sd  = data;
    services_discovery_sys_t *p_sys = p_sd->p_sys;
    char **devices;
    int i, i_devices;
    int canc = vlc_savecancel();

    /* parse existing devices first */
    if( ( devices = libhal_get_all_devices( p_sys->p_ctx, &i_devices, NULL ) ) )
    {
        for( i = 0; i < i_devices; i++ )
        {
            ParseDevice( p_sd, devices[ i ] );
            libhal_free_string( devices[ i ] );
        }
        free( devices );
    }

    /* FIXME: Totally lame. There are DBus watch functions to do this properly.
     * -- Courmisch, 28/08/2008 */
    while( vlc_object_alive (p_sd) )
    {
        /* look for events on the bus, blocking 1 second */
        dbus_connection_read_write_dispatch( p_sys->p_connection, 1000 );
        /* HAL 0.5.8.1 can use libhal_ctx_get_dbus_connection(p_sys->p_ctx) */
    }
    vlc_restorecancel (canc);
    return NULL;
}

void DeviceAdded( LibHalContext *p_ctx, const char *psz_udi )
{
    VLC_UNUSED(p_ctx);
    ParseDevice( p_sd_global, psz_udi );
}

void DeviceRemoved( LibHalContext *p_ctx, const char *psz_udi )
{
    VLC_UNUSED(p_ctx);
    DelItem( p_sd_global, psz_udi );
}
