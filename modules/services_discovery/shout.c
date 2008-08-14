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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_services_discovery.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

enum type_e { ShoutRadio = 0, ShoutTV = 1, Freebox = 2, FrenchTV = 3 };

static int  Open( vlc_object_t *, enum type_e );
static void Close( vlc_object_t * );

struct shout_item_t
{
    const char *psz_url;
    const char *psz_name;
    const char *ppsz_options[2];
    const struct shout_item_t * p_children;
};

#define endItem( ) { NULL, NULL, { NULL }, NULL }
#define item( title, url ) { url, title, { NULL }, NULL }
#define itemWithOption( title, url, option ) { url, title, { option, NULL }, NULL }
#define itemWithChildren( title, children ) { "vlc://nop", title, { NULL }, children }

/* WARN: We support only two levels */

static const struct shout_item_t p_frenchtv_canalplus[] = {
    itemWithOption( N_("Les Guignols"), "http://www.canalplus.fr/index.php?pid=1784", "http-forward-cookies" ),
    endItem()
};
    
static const struct shout_item_t p_frenchtv[] = {
    itemWithChildren( N_("Canal +"),  p_frenchtv_canalplus ),
    endItem()
};

static const struct shout_item_t p_items[] = {
    item(            N_("Shoutcast Radio"), "http/shout-winamp://www.shoutcast.com/sbin/newxml.phtml" ),
    item(            N_("Shoutcast TV"),    "http/shout-winamp://www.shoutcast.com/sbin/newtvlister.phtml?alltv=1" ),
    item(            N_("Freebox TV"),      "http://mafreebox.freebox.fr/freeboxtv/playlist.m3u" ),
    itemWithChildren(N_("French TV"),        p_frenchtv ),
    endItem()
};

#undef endItem
#undef item
#undef itemWithOptions
#undef itemWithChildren

struct shout_category_t {
    services_discovery_t * p_sd;
    const char * psz_category;
};

/* Main functions */
#define OPEN( type )                                \
static int Open ## type ( vlc_object_t *p_this )    \
{                                                   \
    msg_Dbg( p_this, "Starting " #type );           \
    return Open( p_this, type );                    \
}

OPEN( ShoutRadio )
OPEN( ShoutTV )
OPEN( Freebox )
OPEN( FrenchTV )

vlc_module_begin();
    set_category( CAT_PLAYLIST );
    set_subcategory( SUBCAT_PLAYLIST_SD );

    add_obsolete_integer( "shoutcast-limit" );

        set_shortname( "Shoutcast");
        set_description( N_("Shoutcast radio listings") );
        set_capability( "services_discovery", 0 );
        set_callbacks( OpenShoutRadio, Close );
        add_shortcut( "shoutcast" );

    add_submodule();
        set_shortname( "ShoutcastTV" );
        set_description( N_("Shoutcast TV listings") );
        set_capability( "services_discovery", 0 );
        set_callbacks( OpenShoutTV, Close );
        add_shortcut( "shoutcasttv" );

    add_submodule();
        set_shortname( "frenchtv");
        set_description( N_("French TV") );
        set_capability( "services_discovery", 0 );
        set_callbacks( OpenFrenchTV, Close );
        add_shortcut( "frenchtv" );

    add_submodule();
        set_shortname( "Freebox");
        set_description( N_("Freebox TV listing (French ISP free.fr services)") );
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
    struct shout_category_t * params = user_data;
    services_discovery_AddItem( params->p_sd,
            p_event->u.input_item_subitem_added.p_new_child,
            params->psz_category );
}

/*****************************************************************************
 * CreateInputItemFromShoutItem:
 *****************************************************************************/
static input_item_t * CreateInputItemFromShoutItem( services_discovery_t *p_sd,
                                         const struct shout_item_t * p_item )
{
    int i;
    /* Create the item */
    input_item_t *p_input = input_item_NewExt( p_sd,
                    p_item->psz_url, _(p_item->psz_name),
                    0, NULL, -1 );

    /* Copy options */
    for( i = 0; p_item->ppsz_options[i] != NULL; i++ )
        input_item_AddOption( p_input, p_item->ppsz_options[i] );
    input_item_AddOption( p_input, "no-playlist-autostart" );

    return p_input;
}

/*****************************************************************************
 * AddSubitemsOfShoutItemURL:
 *****************************************************************************/
static void AddSubitemsOfShoutItemURL( services_discovery_t *p_sd,
                                       const struct shout_item_t * p_item,
                                       const char * psz_category )
{
    struct shout_category_t category = { p_sd, psz_category };

    /* Create the item */
    input_item_t *p_input = CreateInputItemFromShoutItem( p_sd, p_item );

    /* Read every subitems, and add them in ItemAdded */
    vlc_event_attach( &p_input->event_manager, vlc_InputItemSubItemAdded,
                      ItemAdded, &category );
    input_Read( p_sd, p_input, true );
    vlc_event_detach( &p_input->event_manager, vlc_InputItemSubItemAdded,
                      ItemAdded, &category );

    vlc_gc_decref( p_input );
}

/*****************************************************************************
 * Run:
 *****************************************************************************/
static void Run( services_discovery_t *p_sd )
{
    enum type_e i_type = (enum type_e)p_sd->p_sys;
    int i, j;
    
    if( !p_items[i_type].p_children )
    {
        AddSubitemsOfShoutItemURL( p_sd, &p_items[i_type], NULL );
        return;
    }
    for( i = 0; p_items[i_type].p_children[i].psz_name; i++ )
    {
        const struct shout_item_t * p_subitem = &p_items[i_type].p_children[i];
        if( !p_subitem->p_children )
        {
            AddSubitemsOfShoutItemURL( p_sd, p_subitem, p_subitem->psz_name );
            continue;
        }
        for( j = 0; p_subitem->p_children[j].psz_name; j++ )
        {
            input_item_t *p_input = CreateInputItemFromShoutItem( p_sd, &p_subitem->p_children[j] );
            services_discovery_AddItem( p_sd,
                p_input,
                p_subitem->psz_name );
            vlc_gc_decref( p_input );
        }
    }
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    VLC_UNUSED(p_this);
}
