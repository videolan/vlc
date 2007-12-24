/*****************************************************************************
 * shout.c:  Shoutcast services discovery module
 *****************************************************************************
 * Copyright (C) 2005-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
 *          Antoine Cellerier <dionoea -@T- videolan -d.t- org>
 *          Pierre d'Herbemont <pdherbemont # videolan.org>
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

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

enum type_e { ShoutRadio = 0, ShoutTV = 1, Freebox = 2 };

static int  Open( vlc_object_t *, enum type_e );
static void Close( vlc_object_t * );

static const struct
{
    const char *psz_url;
    const char *psz_name;
    const char *ppsz_options[2];
} p_items[] = {
    { "http/shout-winamp://www.shoutcast.com/sbin/newxml.phtml",
      N_("Shoutcast Radio"), { NULL } },
    { "http/shout-winamp://www.shoutcast.com/sbin/newtvlister.phtml?alltv=1",
      N_("Shoutcast TV"), { NULL } },
    { "http://mafreebox.freebox.fr/freeboxtv/playlist.m3u",
      N_("Freebox TV"), { "m3u-extvlcopt=1", NULL } },
};

/* Main functions */
#define OPEN( type )                                \
static int Open ## type ( vlc_object_t *p_this )    \
{                                                   \
    return Open( p_this, type );                    \
}

OPEN( ShoutRadio )
OPEN( ShoutTV )
OPEN( Freebox )

vlc_module_begin();
    set_category( CAT_PLAYLIST );
    set_subcategory( SUBCAT_PLAYLIST_SD );

    add_obsolete_integer( "shoutcast-limit" );

        set_shortname( "Shoutcast");
        set_description( _("Shoutcast radio listings") );
        set_capability( "services_discovery", 0 );
        set_callbacks( OpenShoutRadio, Close );
        add_shortcut( "shoutcast" );

    add_submodule();
        set_shortname( "ShoutcastTV" );
        set_description( _("Shoutcast TV listings") );
        set_capability( "services_discovery", 0 );
        set_callbacks( OpenShoutTV, Close );
        add_shortcut( "shoutcasttv" );

    add_submodule();
        set_shortname( "Freebox");
        set_description( _("Freebox TV listing (French ISP free.fr services)") );
        set_capability( "services_discovery", 0 );
        set_callbacks( OpenFreebox, Close );
        add_shortcut( "freebox" );

vlc_module_end();


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static void Run( services_discovery_t *p_sd );


/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this, enum type_e i_type )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_SetLocalizedName( p_sd, _(p_items[i_type].psz_name) );
    p_sd->pf_run = Run;
    p_sd->p_sys = (void *)i_type;
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
    enum type_e i_type = (enum type_e)p_sd->p_sys;
    int i;
    input_item_t *p_input = input_ItemNewExt( p_sd,
                        p_items[i_type].psz_url, _(p_items[i_type].psz_name),
                        0, NULL, -1 );
    for( i = 0; p_items[i_type].ppsz_options[i] != NULL; i++ )
        input_ItemAddOption( p_input, p_items[i_type].ppsz_options[i] );
    input_ItemAddOption( p_input, "no-playlist-autostart" );

    vlc_event_attach( &p_input->event_manager, vlc_InputItemSubItemAdded,
                      ItemAdded, p_sd );
    input_Read( p_sd, p_input, VLC_TRUE );
    vlc_event_detach( &p_input->event_manager, vlc_InputItemSubItemAdded,
                      ItemAdded, p_sd );
    vlc_gc_decref( p_input );
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
}
