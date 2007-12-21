/*****************************************************************************
 * freebox.c :  Freebox interface module
 *****************************************************************************
 * Copyright (C) 2004-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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

/************************************************************************
 * definitions
 ************************************************************************/
static const char kpsz_freebox_playlist_url[] = "http://mafreebox.freebox.fr/freeboxtv/playlist.m3u";

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

/* Callbacks */
static int  Open ( vlc_object_t *, int );
static void Close( vlc_object_t * );
static void ItemAdded( const vlc_event_t * p_event, void * user_data );
static void Run( services_discovery_t *p_sd );

vlc_module_begin();
    set_shortname( "Freebox");
    set_description( _("Freebox TV listing (French ISP free.fr services)") );
    add_shortcut( "freebox" );
    set_category( CAT_PLAYLIST );
    set_subcategory( SUBCAT_PLAYLIST_SD );

    set_capability( "services_discovery", 0 );
    set_callbacks( Open, Close );
vlc_module_end();


/*****************************************************************************
 * Open: initialize
 *****************************************************************************/
static int Open( vlc_object_t *p_this, int i_type )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    p_sd->pf_run = Run;
    services_discovery_SetLocalizedName( p_sd, _("Freebox TV") );
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
    input_item_t * p_input = input_ItemNewExt( p_sd, kpsz_freebox_playlist_url,
                                _("Freebox TV"), 0, NULL, -1 );
    input_ItemAddOption( p_input, "no-playlist-autostart" );
    vlc_gc_incref( p_input );

    vlc_event_attach( &p_input->event_manager, vlc_InputItemSubItemAdded, ItemAdded, p_sd );
    input_Read( p_sd, p_input, VLC_TRUE );
    vlc_event_detach( &p_input->event_manager, vlc_InputItemSubItemAdded, ItemAdded, p_sd );
    vlc_gc_decref( p_input );
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
}
