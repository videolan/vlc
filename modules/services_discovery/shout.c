/*****************************************************************************
 * shout.c:  Shoutcast services discovery module
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
 *          Antoine Cellerier <dionoea -@T- videolan -d.t- org>
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
 * Includes
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc_interaction.h>

#include <vlc/input.h>

#include "network.h"

#include <errno.h>                                                 /* ENOMEM */

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif

/************************************************************************
 * Macros and definitions
 ************************************************************************/

#define MAX_LINE_LENGTH 256
#define SHOUTCAST_BASE_URL "http/shout-winamp://www.shoutcast.com/sbin/newxml.phtml"
#define SHOUTCAST_TV_BASE_URL "http/shout-winamp://www.shoutcast.com/sbin/newtvlister.phtml?alltv=1"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

/* Callbacks */
    static int  Open ( vlc_object_t *, int );
    static int  OpenRadio ( vlc_object_t * );
    static int  OpenTV ( vlc_object_t * );
    static void Close( vlc_object_t * );

vlc_module_begin();
    set_shortname( "Shoutcast");
    set_description( _("Shoutcast radio listings") );
    add_shortcut( "shoutcast" );
    set_category( CAT_PLAYLIST );
    set_subcategory( SUBCAT_PLAYLIST_SD );

    add_suppressed_integer( "shoutcast-limit" );

    set_capability( "services_discovery", 0 );
    set_callbacks( OpenRadio, Close );

    add_submodule();
        set_shortname( "ShoutcastTV" );
        set_description( _("Shoutcast TV listings") );
        set_capability( "services_discovery", 0 );
        set_callbacks( OpenTV, Close );
        add_shortcut( "shoutcasttv" );

vlc_module_end();


/*****************************************************************************
 * Local structures
 *****************************************************************************/

struct services_discovery_sys_t
{
    playlist_item_t *p_node_cat,*p_node_one;
    input_item_t *p_input;
    vlc_bool_t b_dialog;
};

#define RADIO 0
#define TV 1

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* Main functions */
static int OpenRadio( vlc_object_t *p_this )
{
    return Open( p_this, RADIO );
}

static int OpenTV( vlc_object_t *p_this )
{
    return Open( p_this, TV );
}

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this, int i_type )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    playlist_t          *p_playlist = pl_Yield( p_this );
    DECMALLOC_ERR( p_sys, services_discovery_sys_t );
    p_sd->p_sys  = p_sys;

    switch( i_type )
    {
        case TV:
            p_sys->p_input = input_ItemNewExt( p_playlist,
                                SHOUTCAST_TV_BASE_URL, _("Shoutcast TV"),
                                0, NULL, -1 );
            break;
        case RADIO:
        default:
            p_sys->p_input = input_ItemNewExt( p_playlist,
                                SHOUTCAST_BASE_URL, _("Shoutcast"),
                                0, NULL, -1 );
            break;
    }
    input_ItemAddOption( p_sys->p_input, "no-playlist-autostart" );
    p_sys->p_input->b_prefers_tree = VLC_TRUE;
    p_sys->p_node_cat = playlist_NodeAddInput( p_playlist, p_sys->p_input,
                           p_playlist->p_root_category,
                           PLAYLIST_APPEND, PLAYLIST_END );
    p_sys->p_node_one = playlist_NodeAddInput( p_playlist, p_sys->p_input,
                           p_playlist->p_root_onelevel,
                           PLAYLIST_APPEND, PLAYLIST_END );
    p_sys->p_node_cat->i_flags |= PLAYLIST_RO_FLAG;
    p_sys->p_node_cat->i_flags |= PLAYLIST_SKIP_FLAG;
    p_sys->p_node_one->i_flags |= PLAYLIST_RO_FLAG;
    p_sys->p_node_one->i_flags |= PLAYLIST_SKIP_FLAG;
    p_sys->p_node_one->p_input->i_id = p_sys->p_node_cat->p_input->i_id;

    var_SetVoid( p_playlist, "intf-change" );

    pl_Release( p_this );

    input_Read( p_sd, p_sys->p_input, VLC_FALSE );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys  = p_sd->p_sys;
    playlist_t *p_playlist =  pl_Yield( p_sd );
    playlist_NodeDelete( p_playlist, p_sys->p_node_cat, VLC_TRUE, VLC_FALSE );
    playlist_NodeDelete( p_playlist, p_sys->p_node_one, VLC_TRUE, VLC_FALSE );
    pl_Release( p_sd );
    free( p_sys );
}
