/*****************************************************************************
 * hal.c :  HAL probing module
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_devices.h>

#include <hal/libhal.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct probe_sys_t
{
    DBusConnection *p_connection;
    LibHalContext *p_ctx;
    int            i_devices;
    device_t     **pp_devices;
};

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

static void Update ( device_probe_t *p_probe );
static void UpdateMedia( device_probe_t *p_probe, device_t *p_dev );
static void AddDevice( device_probe_t * p_probe, device_t *p_dev );
static device_t * ParseDisc( device_probe_t *p_probe,  char *psz_device );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("HAL devices detection") )
    set_capability( "devices probe", 0 )
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    device_probe_t *p_probe = (device_probe_t *)p_this;
    DBusError       dbus_error;
    DBusConnection *p_connection;
    probe_sys_t    *p_sys;

    p_probe->p_sys = p_sys = malloc( sizeof( probe_sys_t ) );
    p_probe->p_sys->i_devices = 0;
    p_probe->p_sys->pp_devices = NULL;

    p_probe->pf_run = Update;

    dbus_error_init( &dbus_error );

    p_sys->p_ctx = libhal_ctx_new();
    if( !p_sys->p_ctx )
    {
        msg_Err( p_probe, "unable to create HAL context") ;
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_connection = dbus_bus_get( DBUS_BUS_SYSTEM, &dbus_error );
    if( dbus_error_is_set( &dbus_error ) )
    {
        msg_Err( p_probe, "unable to connect to DBUS: %s", dbus_error.message );
        goto error;
    }
    p_sys->p_connection = p_connection;
    libhal_ctx_set_dbus_connection( p_sys->p_ctx, p_connection );
    if( !libhal_ctx_init( p_sys->p_ctx, &dbus_error ) )
    {
        msg_Err( p_probe, "hal not available : %s", dbus_error.message );
        dbus_connection_unref( p_connection );
        goto error;
    }
    return VLC_SUCCESS;
error:
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
    device_probe_t *p_probe = (device_probe_t *) p_this;
    probe_sys_t *p_sys = p_probe->p_sys;
    dbus_connection_unref( p_sys->p_connection );
    libhal_ctx_free( p_sys->p_ctx );
    free( p_sys );
}
#if 0
static int GetAllDevices( device_probe_t *p_probe, device_t ***ppp_devices )
{
    /// \todo : fill the dst array
    return p_probe->p_sys->i_devices;
}
#endif
static void Update( device_probe_t * p_probe )
{
    probe_sys_t *p_sys = p_probe->p_sys;
    int i, i_devices, j;
    char **devices;
    bool b_exists;
    int canc = vlc_savecancel();

    for ( j = 0 ; j < p_sys->i_devices; j++ )
        p_sys->pp_devices[j]->b_seen = false;

    /* CD/DVD */
    if( ( devices = libhal_find_device_by_capability( p_sys->p_ctx,
                              "storage.cdrom",
                              &i_devices, NULL ) ) )
    {
        for( i = 0; i < i_devices; i++ )
        {
            device_t *p_dev = ParseDisc( p_probe, devices[ i ] );
            b_exists = false;

            for ( j = 0 ; j < p_sys->i_devices; j++ )
            {
                if( !strcmp( p_sys->pp_devices[j]->psz_uri,
                             p_dev->psz_uri ) )
                {
                    b_exists = true;
                    p_dev->b_seen = true;
                    UpdateMedia( p_probe, p_dev );
                    break;
                }
                if( !b_exists )
                    AddDevice( p_probe, p_dev );
            }
        }
    }
    /// \todo Remove unseen devices
    vlc_restorecancel( canc );
}


static void AddDevice( device_probe_t * p_probe, device_t *p_dev )
{
    INSERT_ELEM( p_probe->p_sys->pp_devices,
                 p_probe->p_sys->i_devices,
                 p_probe->p_sys->i_devices,
                 p_dev );
    /// \todo : emit variable
}

static device_t * ParseDisc( device_probe_t *p_probe,  char *psz_device )
{
    probe_sys_t *p_sys = p_probe->p_sys;
    device_t *p_dev;
    char *block_dev;
    dbus_bool_t b_dvd;

    if( !libhal_device_property_exists( p_sys->p_ctx, psz_device,
                                        "storage.cdrom.dvd", NULL ) )
        return NULL;

    p_dev = (device_t *)malloc( sizeof( device_t ) );
    p_dev->i_media_type = p_dev->i_capabilities = 0;
    p_dev->psz_name = p_dev->psz_uri = NULL;

    block_dev =  libhal_device_get_property_string( p_sys->p_ctx, psz_device,
                                                   "block.device" , NULL );
    if( block_dev )
    {
        p_dev->psz_uri = strdup( block_dev );
        libhal_free_string( block_dev );
    }

    b_dvd = libhal_device_get_property_bool( p_sys->p_ctx, psz_device,
                                            "storage.cdrom.dvd", NULL  );
    if( b_dvd )
        p_dev->i_capabilities = DEVICE_CAN_DVD | DEVICE_CAN_CD;
    else
        p_dev->i_capabilities = DEVICE_CAN_CD;

    UpdateMedia( p_probe, p_dev );
    return p_dev;
}

static void UpdateMedia( device_probe_t *p_probe, device_t *p_dev )
{
    probe_sys_t *p_sys = p_probe->p_sys;
    char **matching_media;
    int i_matching, i;
    bool b_changed = false;;
    int i_old_type = p_dev->i_media_type;
    p_dev->i_media_type = 0;

    /* Find the media in the drive */
    matching_media = libhal_manager_find_device_string_match( p_sys->p_ctx,
                                            "block.device", p_dev->psz_uri,
                                            &i_matching, NULL );
    for( i = 0; i < i_matching; i++ )
    {
        if( libhal_device_property_exists( p_sys->p_ctx, matching_media[i],
                                           "volume.disc.type", NULL ) )
        {
            char *psz_media_name = libhal_device_get_property_string(
                                            p_sys->p_ctx,
                                            matching_media[i],
                                            "volume.label", NULL );
            if( psz_media_name )
            {
                if( p_dev->psz_name && strcmp( p_dev->psz_name, psz_media_name))
                {
                    free( p_dev->psz_name );
                    p_dev->psz_name = NULL;
                    b_changed = true;
                }
                if( !p_dev->psz_name )
                    p_dev->psz_name = strdup( psz_media_name );
                libhal_free_string( psz_media_name );
            }

            if( libhal_device_get_property_bool( p_sys->p_ctx,
                                             matching_media[i],
                                             "volume.disc.is_videodvd", NULL) )
                p_dev->i_media_type = MEDIA_TYPE_DVD;
            else if( libhal_device_get_property_bool( p_sys->p_ctx,
                                             matching_media[i],
                                             "volume.disc.is_vcd", NULL) ||
                     libhal_device_get_property_bool( p_sys->p_ctx,
                                             matching_media[i],
                                             "volume.disc.is_svcd", NULL) )
               p_dev->i_media_type = MEDIA_TYPE_VCD;
            else if( libhal_device_get_property_bool( p_sys->p_ctx,
                                             matching_media[i],
                                             "volume.disc.has_audio", NULL) )
               p_dev->i_media_type = MEDIA_TYPE_CDDA;

            break;
        }
    }
    if( b_changed || p_dev->i_media_type != i_old_type )
    {
        /// \todo emit changed signal
    }
}
