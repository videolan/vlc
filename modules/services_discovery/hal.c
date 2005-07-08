/*****************************************************************************
 * sap.c :  SAP interface module
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN (Centrale RÃ©seaux) and its contributors
 * $Id: sap.c 9217 2004-11-07 11:02:59Z courmisch $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Includes
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */

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

/************************************************************************
 * Macros and definitions
 ************************************************************************/

#define MAX_LINE_LENGTH 256


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

/* Callbacks */
    static int  Open ( vlc_object_t * );
    static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("HAL device detection") );
    set_category( CAT_PLAYLIST );
    set_subcategory( SUBCAT_PLAYLIST_SD );

    set_capability( "services_discovery", 0 );
    set_callbacks( Open, Close );

vlc_module_end();


/*****************************************************************************
 * Local structures
 *****************************************************************************/

struct services_discovery_sys_t
{
    LibHalContext *p_ctx;

    /* playlist node */
    playlist_item_t *p_node;

};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* Main functions */
    static void Run    ( services_discovery_t *p_intf );

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
    playlist_view_t     *p_view;

    p_sd->pf_run = Run;
    p_sd->p_sys  = p_sys;

    if( !( p_sys->p_ctx = hal_initialize( NULL, FALSE ) ) )
    {
        free( p_sys );
        msg_Err( p_sd, "hal not available" );
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

    p_view = playlist_ViewFind( p_playlist, VIEW_CATEGORY );
    p_sys->p_node = playlist_NodeCreate( p_playlist, VIEW_CATEGORY,
                                         _("Devices"), p_view->p_root );

    p_sys->p_node->i_flags |= PLAYLIST_RO_FLAG;
    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-change", val );

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
        playlist_NodeDelete( p_playlist, p_sys->p_node, VLC_TRUE, VLC_TRUE );
        vlc_object_release( p_playlist );
    }
    free( p_sys );
}

static void AddDvd( services_discovery_t *p_sd, char *psz_device )
{
    char *psz_name;
    char *psz_uri;
    char *psz_blockdevice;
    services_discovery_sys_t    *p_sys  = p_sd->p_sys;
    playlist_t          *p_playlist;
    playlist_item_t     *p_item;
    psz_name = hal_device_get_property_string( p_sd->p_sys->p_ctx,
                                               psz_device, "volume.label" );
    psz_blockdevice = hal_device_get_property_string( p_sd->p_sys->p_ctx,
                                                 psz_device, "block.device" );
    asprintf( &psz_uri, "dvd://%s", psz_blockdevice );
    /* Create the playlist item here */
    p_item = playlist_ItemNew( p_sd, psz_uri,
                               psz_name );
    free( psz_uri );
    hal_free_string( psz_device );
    if( !p_item )
    {
        return;
    }
    p_item->i_flags &= ~PLAYLIST_SKIP_FLAG;
    p_playlist = (playlist_t *)vlc_object_find( p_sd, VLC_OBJECT_PLAYLIST,
                                                FIND_ANYWHERE );
    if( !p_playlist )
    {
        msg_Err( p_sd, "playlist not found" );
        return;
    }

    playlist_NodeAddItem( p_playlist, p_item, VIEW_CATEGORY, p_sys->p_node,
                          PLAYLIST_APPEND, PLAYLIST_END );

    vlc_object_release( p_playlist );
}

static void AddCdda( services_discovery_t *p_sd, char *psz_device )
{
    char *psz_name = "Audio CD";
    char *psz_uri;
    char *psz_blockdevice;
    services_discovery_sys_t    *p_sys  = p_sd->p_sys;
    playlist_t          *p_playlist;
    playlist_item_t     *p_item;
    psz_blockdevice = hal_device_get_property_string( p_sd->p_sys->p_ctx,
                                                 psz_device, "block.device" );
    asprintf( &psz_uri, "cdda://%s", psz_blockdevice );
    /* Create the playlist item here */
    p_item = playlist_ItemNew( p_sd, psz_uri,
                               psz_name );
    free( psz_uri );
    hal_free_string( psz_device );
    if( !p_item )
    {
        return;
    }
    p_item->i_flags &= ~PLAYLIST_SKIP_FLAG;
    p_playlist = (playlist_t *)vlc_object_find( p_sd, VLC_OBJECT_PLAYLIST,
                                                FIND_ANYWHERE );
    if( !p_playlist )
    {
        msg_Err( p_sd, "playlist not found" );
        return;
    }

    playlist_NodeAddItem( p_playlist, p_item, VIEW_CATEGORY, p_sys->p_node,
                          PLAYLIST_APPEND, PLAYLIST_END );

    vlc_object_release( p_playlist );

}

static void ParseDevice( services_discovery_t *p_sd, char *psz_device )
{
    char *psz_disc_type;
    services_discovery_sys_t    *p_sys  = p_sd->p_sys;
    if( hal_device_property_exists( p_sys->p_ctx, psz_device,
                                    "volume.disc.type" ) )
    {
        psz_disc_type = hal_device_get_property_string( p_sys->p_ctx,
                                                        psz_device,
                                                        "volume.disc.type" );
        if( !strcmp( psz_disc_type, "dvd_rom" ) )
        {
            AddDvd( p_sd, psz_device );
        }
        else if( !strcmp( psz_disc_type, "cd_rom" ) )
        {
            if( hal_device_get_property_bool( p_sys->p_ctx, psz_device, "volume.disc.has_audio" ) )
            {
                AddCdda( p_sd, psz_device );
            }
        }
        hal_free_string( psz_disc_type );
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
    if( ( devices = hal_get_all_devices( p_sys->p_ctx, &i_devices ) ) )
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
