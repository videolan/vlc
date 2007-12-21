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

#include <vlc/vlc.h>
#include <vlc_services_discovery.h>
#include <vlc_interface.h>

#include <vlc_network.h>

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

    add_obsolete_integer( "shoutcast-limit" );

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
    input_item_t *p_input;
    vlc_bool_t b_dialog;
};

#define RADIO 0
#define TV 1

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static void Run( services_discovery_t *p_sd );

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
    DECMALLOC_ERR( p_sys, services_discovery_sys_t );
    p_sd->p_sys  = p_sys;
    p_sd->pf_run = Run;

    switch( i_type )
    {
        case TV:
            services_discovery_SetLocalizedName( p_sd, _("Shoutcast TV") );

            p_sys->p_input = input_ItemNewExt( p_sd,
                                SHOUTCAST_TV_BASE_URL, _("Shoutcast TV"),
                                0, NULL, -1 );
            break;
        case RADIO:
        default:
            services_discovery_SetLocalizedName( p_sd, _("Shoutcast Radio") );

            p_sys->p_input = input_ItemNewExt( p_sd,
                                SHOUTCAST_BASE_URL, _("Shoutcast Radio"),
                                0, NULL, -1 );
            break;
    }
    vlc_gc_decref( p_sys->p_input ); /* Refcount to 1, so we can release it
                                      * in Close() */

    input_ItemAddOption( p_sys->p_input, "no-playlist-autostart" );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * ItemAdded:
 *****************************************************************************/
static void ItemAdded( const vlc_event_t * p_event, void * user_data )
{
    services_discovery_t *p_sd = user_data;
    services_discovery_AddItem( p_sd,
            p_event->u.input_item_subitem_added.p_new_child,
            NULL /* no category */ );
}

/*****************************************************************************
 * Run:
 *****************************************************************************/
static void Run( services_discovery_t *p_sd )
{
    vlc_event_attach( &p_sd->p_sys->p_input->event_manager, vlc_InputItemSubItemAdded, ItemAdded, p_sd );
    input_Read( p_sd, p_sd->p_sys->p_input, VLC_TRUE );
    vlc_event_detach( &p_sd->p_sys->p_input->event_manager, vlc_InputItemSubItemAdded, ItemAdded, p_sd );
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys  = p_sd->p_sys;
    vlc_gc_decref( p_sys->p_input );
    free( p_sys );
}
