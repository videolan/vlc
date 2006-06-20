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

#include <vlc/vlc.h>
#include <vlc_devices.h>

#include <hal/libhal.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct intf_sys_t
{
    LibHalContext *p_ctx;
    int            i_devices;
    device_t     **pp_devices;
};

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

static void Update ( intf_thread_t *p_intf );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("HAL devices detection") );
    set_capability( "devices probe", 0 );
    set_callbacks( Open, Close );
vlc_module_end();


/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    vlc_value_t         val;
    DBusError           dbus_error;
    DBusConnection      *p_connection;

    p_intf->p_sys = (intf_sys_t*)malloc( sizeof( intf_sys_t ) );
    p_intf->p_sys->i_drives = 0;

    p_intf->pf_run = Run;

    dbus_error_init( &dbus_error );

    p_intf->p_sys->p_ctx = libhal_ctx_new();
    if( !p_intf->p_sys->p_ctx )
    {
        msg_Err( p_intf, "unable to create HAL context") ;
        free( p_intf->p_sys );
        return VLC_EGENERIC;
    }
    p_connection = dbus_bus_get( DBUS_BUS_SYSTEM, &dbus_error );
    if( dbus_error_is_set( &dbus_error ) )
    {
        msg_Err( p_intf, "unable to connect to DBUS: %s", dbus_error.message );
        dbus_error_free( &dbus_error );
        free( p_intf->p_sys );
        return VLC_EGENERIC;
    }
    libhal_ctx_set_dbus_connection( p_intf->p_sys->p_ctx, p_connection );
    if( !libhal_ctx_init( p_intf->p_sys->p_ctx, &dbus_error ) )
    {
        msg_Err( p_intf, "hal not available : %s", dbus_error.message );
        dbus_error_free( &dbus_error );
        free( p_sys );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *) p_this;
    intf_sys_t *p_sys = p_intf->p_sys;
    free( p_sys );
}

static int GetAllDevices( intf_thread_t *p_intf, device_t ***ppp_devices )
{
    /* Todo : fill the dst array */
    return p_intf->p_sys->i_devices;
}

static void Update( intf_thread_t * p_intf )
{
    intf_sys_t *p_sys = p_intf->p_sys;
    int i, i_devices, j;
    char **devices;
    vlc_bool_t b_exists;

    for ( j = 0 ; j < p_sys->i_devices; j++ )
        p_dev->b_seen = VLC_FALSE;

    if( ( devices = libhal_get_all_devices( p_sys->p_ctx, &i_devices, NULL ) ) )
    {
        device_t *p_device;
        for( i = 0; i < i_devices; i++ )
        {
            b_exists = VLC_FALSE;
            p_dev = ParseDevice( p_sd, devices[ i ] );

            for ( j = 0 ; j < p_sys->i_devices; j++ )
            {
                if( !strcmp( p_sys->pp_devices[j]->psz_uri,
                             p_dev->psz_uri ) )
                {
                    b_exists = VLC_TRUE;
                    p_dev->b_seen = VLC_TRUE;
                    break;
                }
                if( !b_exists )
                    AddDevice( p_intf, p_dev );
            }
        }
    }
    /// \todo Remove unseen devices
}


static void AddDevice( intf_thread_t * p_intf, device_t *p_dev )
{
    INSERT_ELEM( p_intf->p_sys->pp_devices,
                 p_intf->p_sys->i_devices,
                 p_intf->p_sys->i_devices,
                 p_dev );
    /// \todo : emit variable
}




static device_t * ParseDevice( intf_thread_t *p_intf,  char *psz_device )
{
    char *psz_disc_type;
    intf_sys_t *p_sys = p_intf->p_sys;
    /* FIXME: The following code provides media detection, not device */
 /*
    if( libhal_device_property_exists( p_sys->p_ctx, psz_device,
                                       "volume.disc.type", NULL ) )
    {
        psz_disc_type = libhal_device_get_property_string( p_sys->p_ctx,
                                                        psz_device,
                                                        "volume.disc.type",
                                                        NULL );
        if( !strcmp( psz_disc_type, "dvd_rom" ) )
        {
            /// \todo This is a DVD
            //psz_name = libhal_device_get_property_string( p_sd->p_sys->p_ctx,
            ///                          psz_device, "volume.label", NULL );
            // psz_blockdevice = libhal_device_get_property_string( p_sd->p_sys->p_ctx,
            //                            psz_device, "block.device", NULL );
            //    libhal_free_string( psz_device );
}
        }
        else if( !strcmp( psz_disc_type, "cd_rom" ) )
        {
            if( libhal_device_get_property_bool( p_sys->p_ctx, psz_device,
                                         "volume.disc.has_audio" , NULL ) )
            {
                /// \todo This is a CDDA
            }
        }
        libhal_free_string( psz_disc_type );
    }
    */
    return NULL;
}
