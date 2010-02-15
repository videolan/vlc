/*****************************************************************************
 * freebox.c:  Shoutcast services discovery module
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

static int  Open( vlc_object_t * );
static void Close( vlc_object_t * );

static int vlc_sd_probe_Open( vlc_object_t * );

vlc_module_begin ()
    set_category( CAT_PLAYLIST )
    set_subcategory( SUBCAT_PLAYLIST_SD )

    set_shortname( "Freebox")
    set_description( N_("Freebox TV") )
    set_capability( "services_discovery", 0 )
    set_callbacks( Open, Close )
    add_shortcut( "freebox" )

    VLC_SD_PROBE_SUBMODULE
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static void *Run( void * );
struct services_discovery_sys_t
{
    vlc_thread_t thread;
};

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open(vlc_object_t *this)
{
    services_discovery_t *sd = (services_discovery_t *)this;
    sd->p_sys = malloc(sizeof(*(sd->p_sys)));
    if (sd->p_sys == NULL)
        return VLC_ENOMEM;

    if (vlc_clone(&sd->p_sys->thread, Run, sd, VLC_THREAD_PRIORITY_LOW))
    {
        free(sd->p_sys);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * ItemAdded:
 *****************************************************************************/
static void ItemAdded(const vlc_event_t * event, void *user_data)
{
    services_discovery_t *sd = user_data;
    input_item_t *child = event->u.input_item_subitem_added.p_new_child;
    input_item_AddOption(child, "deinterlace=1", VLC_INPUT_OPTION_TRUSTED);
    services_discovery_AddItem(sd, child, NULL);
}


/*****************************************************************************
 * Run:
 *****************************************************************************/
static void *Run(void *data)
{
    services_discovery_t *sd = data;
    int canc = vlc_savecancel();

    const char * const name = "Freebox TV";
    const char * const url = "http://mafreebox.freebox.fr/freeboxtv/playlist.m3u";
    input_item_t *input = input_item_New(sd, url, vlc_gettext(name));
    input_item_AddOption(input, "no-playlist-autostart", VLC_INPUT_OPTION_TRUSTED);

    /* Read every subitems, and add them in ItemAdded */
    vlc_event_manager_t *em = &input->event_manager;
    vlc_event_attach(em, vlc_InputItemSubItemAdded, ItemAdded, sd);
    input_Read(sd, input);
    vlc_event_detach(em, vlc_InputItemSubItemAdded, ItemAdded, sd);

    vlc_gc_decref(input);

    vlc_restorecancel(canc);
    return NULL;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close(vlc_object_t *this)
{
    services_discovery_t *sd = (services_discovery_t *)this;
    services_discovery_sys_t *sys = sd->p_sys;
    vlc_join(sys->thread, NULL);
    free(sys);
}

static int vlc_sd_probe_Open(vlc_object_t *obj)
{
    VLC_UNUSED(obj);
    return VLC_PROBE_CONTINUE;
}
