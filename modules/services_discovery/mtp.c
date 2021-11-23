/*****************************************************************************
 * mtp.c :  MTP interface module
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 *
 * Authors: Fabio Ritrovato <exsephiroth87@gmail.com>
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

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_list.h>
#include <vlc_plugin.h>
#include <vlc_services_discovery.h>
#include <vlc_vector.h>

#include <libmtp.h>

typedef struct VLC_VECTOR(input_item_t *) vec_items_t;

typedef struct
{
    struct vlc_list node;
    input_item_t *root;
    vec_items_t items;
    char *psz_name;
    uint32_t i_bus;
    uint8_t i_dev;
    uint16_t i_product_id;
} mtp_device_t;

typedef struct
{
    struct vlc_list devices; /**< list of mtp_device_t */
    vlc_thread_t thread;
} services_discovery_sys_t;

static void vlc_libmtp_init(void *data)
{
    (void) data;
    LIBMTP_Init();
}

/* Takes ownership of the name */
static mtp_device_t *
DeviceNew( char *name )
{
    mtp_device_t *device = malloc( sizeof( *device ) );
    if ( !device )
        return NULL;

    device->root = input_item_NewExt( "vlc://nop", name,
                                      INPUT_DURATION_INDEFINITE,
                                      ITEM_TYPE_NODE, ITEM_LOCAL );
    if ( !device->root )
    {
        free( device );
        return NULL;
    }

    device->psz_name = name;

    vlc_vector_init( &device->items );

    return device;
}

static void
DeviceDelete( mtp_device_t *device )
{
    for (size_t i = 0; i < device->items.size; ++i )
        input_item_Release( device->items.data[i] );
    vlc_vector_destroy( &device->items );

    input_item_Release( device->root );
    free( device->psz_name );
    free( device );
}

static char *GetDeviceName( LIBMTP_mtpdevice_t *p_device )
{
    char *name = LIBMTP_Get_Friendlyname( p_device );
    if ( !EMPTY_STR( name ) )
        return name;

    name = LIBMTP_Get_Modelname( p_device );
    if ( !EMPTY_STR( name ) )
        return name;

    return strdup( "MTP Device" );
}

static void AddTrack( services_discovery_t *p_sd, mtp_device_t *device,
                      LIBMTP_track_t *p_track )
{
    input_item_t *p_input;
    char *psz_string;
    char *extension;

    extension = rindex( p_track->filename, '.' );
    if( asprintf( &psz_string, "mtp://%"PRIu32":%"PRIu8":%"PRIu16":%d%s",
                  device->i_bus, device->i_dev,
                  device->i_product_id, p_track->item_id,
                  extension ) == -1 )
    {
        msg_Err( p_sd, "Error adding %s, skipping it", p_track->filename );
        return;
    }
    if( ( p_input = input_item_New( psz_string, p_track->title ) ) == NULL )
    {
        msg_Err( p_sd, "Error adding %s, skipping it", p_track->filename );
        free( psz_string );
        return;
    }
    free( psz_string );

    input_item_SetArtist( p_input, p_track->artist );
    input_item_SetGenre( p_input, p_track->genre );
    input_item_SetAlbum( p_input, p_track->album );
    if( asprintf( &psz_string, "%d", p_track->tracknumber ) != -1 )
    {
        input_item_SetTrackNum( p_input, psz_string );
        free( psz_string );
    }
    if( asprintf( &psz_string, "%d", p_track->rating ) != -1 )
    {
        input_item_SetRating( p_input, psz_string );
        free( psz_string );
    }
    input_item_SetDate( p_input, p_track->date );
    p_input->i_duration = VLC_TICK_FROM_MS(p_track->duration);

    if ( !vlc_vector_push( &device->items, p_input ) )
    {
        msg_Err( p_sd, "Error adding %s, skipping it", p_track->filename );
        input_item_Release( p_input );
        return;
    }

    services_discovery_AddSubItem( p_sd, device->root, p_input );
}

static int AddDevice( services_discovery_t *p_sd,
                      LIBMTP_raw_device_t *p_raw_device )
{
    services_discovery_sys_t *p_sys = p_sd->p_sys;
    char *psz_name = NULL;
    LIBMTP_mtpdevice_t *p_device;
    LIBMTP_track_t *p_track, *p_tmp;

    if( ( p_device = LIBMTP_Open_Raw_Device( p_raw_device ) ) != NULL )
    {
        psz_name = GetDeviceName( p_device );
        if ( !psz_name )
            return VLC_ENOMEM;
        msg_Info( p_sd, "Found device: %s", psz_name );

        /* The device takes ownership of the name */
        mtp_device_t *mtp_device = DeviceNew( psz_name );
        if ( !mtp_device )
        {
            free( psz_name );
            return VLC_ENOMEM;
        }

        mtp_device->i_bus = p_raw_device->bus_location;
        mtp_device->i_dev = p_raw_device->devnum;
        mtp_device->i_product_id = p_raw_device->device_entry.product_id;

        vlc_list_append( &mtp_device->node, &p_sys->devices );

        services_discovery_AddItem( p_sd, mtp_device->root );

        if( ( p_track = LIBMTP_Get_Tracklisting_With_Callback( p_device,
                            NULL, NULL ) ) == NULL )
        {
            msg_Warn( p_sd, "No tracks on the device" );
        }
        else
        {
            while( p_track != NULL )
            {
                msg_Dbg( p_sd, "Track found: %s - %s", p_track->artist,
                         p_track->title );
                AddTrack( p_sd, mtp_device, p_track );
                p_tmp = p_track;
                p_track = p_track->next;
                LIBMTP_destroy_track_t( p_tmp );
            }
        }
        LIBMTP_Release_Device( p_device );

        return VLC_SUCCESS;
    }
    else
    {
        msg_Info( p_sd, "The device seems to be mounted, unmount it first" );
        return VLC_EGENERIC;
    }
}

static void CloseDevice( services_discovery_t *p_sd, mtp_device_t *device )
{
    /* Notify the removal of the whole node and its children */
    services_discovery_RemoveItem( p_sd, device->root );

    vlc_list_remove( &device->node );
    DeviceDelete( device );
}

/*****************************************************************************
 * Run: main thread
 *****************************************************************************/
static void *Run( void *data )
{
    LIBMTP_raw_device_t *p_rawdevices;
    int i_numrawdevices;
    int i_ret;
    int i_status = 0;
    services_discovery_t *p_sd = data;
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    for(;;)
    {
        int canc = vlc_savecancel();
        i_ret = LIBMTP_Detect_Raw_Devices( &p_rawdevices, &i_numrawdevices );
        if ( i_ret == 0 && i_numrawdevices > 0 && p_rawdevices != NULL &&
             i_status == 0 )
        {
            /* Found a new device, add it */
            msg_Dbg( p_sd, "New device found" );
            if( AddDevice( p_sd, &p_rawdevices[0] ) == VLC_SUCCESS )
                i_status = 1;
            else
                i_status = 2;
        }
        else
        {
            if ( ( i_ret != 0 || i_numrawdevices == 0 || p_rawdevices == NULL )
                 && i_status == 1)
            {
                /* The device is not connected anymore, delete it */
                msg_Info( p_sd, "Device disconnected" );
                mtp_device_t *device =
                    vlc_list_first_entry_or_null( &p_sys->devices, mtp_device_t,
                                                  node );
                /* Exactly 1 device */
                assert( device );
                assert( vlc_list_is_last( &device->node, &p_sys->devices ) );
                CloseDevice( p_sd, device );
                i_status = 0;
            }
        }
        free( p_rawdevices );
        vlc_restorecancel(canc);
        if( i_status == 2 )
        {
            vlc_tick_sleep( VLC_TICK_FROM_SEC(5) );
            i_status = 0;
        }
        else
            vlc_tick_sleep( VLC_TICK_FROM_MS(500) );
    }
    return NULL;
}

static int Open( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t * )p_this;
    services_discovery_sys_t *p_sys;

    if( !( p_sys = malloc( sizeof( services_discovery_sys_t ) ) ) )
        return VLC_ENOMEM;
    p_sd->p_sys = p_sys;
    p_sd->description = _("MTP devices");

    vlc_list_init( &p_sys->devices );

    static vlc_once_t mtp_init_once = VLC_STATIC_ONCE;

    vlc_once(&mtp_init_once, vlc_libmtp_init, NULL);

    if (vlc_clone (&p_sys->thread, Run, p_sd, VLC_THREAD_PRIORITY_LOW))
    {
        free (p_sys);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static void Close( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t * )p_this;
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    vlc_cancel( p_sys->thread );
    vlc_join( p_sys->thread, NULL );

    mtp_device_t *device;
    vlc_list_foreach( device, &p_sys->devices, node )
        DeviceDelete( device );

    free( p_sys );
}

VLC_SD_PROBE_HELPER("mtp", N_("MTP devices"), SD_CAT_DEVICES)

vlc_module_begin()
    set_shortname( "MTP" )
    set_description( N_( "MTP devices" ) )
    set_category( CAT_PLAYLIST )
    set_subcategory( SUBCAT_PLAYLIST_SD )
    set_capability( "services_discovery", 0 )
    set_callbacks( Open, Close )
    cannot_unload_broken_library()

    VLC_SD_PROBE_SUBMODULE
vlc_module_end()
