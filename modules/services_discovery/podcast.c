/*****************************************************************************
 * podcast.c:  Podcast services discovery module
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
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
#include <vlc_playlist.h>
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

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

/* Callbacks */
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define URLS_TEXT N_("Podcast URLs list")
#define URLS_LONGTEXT N_("Enter the list of podcasts to retrieve, " \
                         "separated by '|' (pipe)." )

vlc_module_begin();
    set_shortname( "Podcast");
    set_description( N_("Podcasts") );
    set_category( CAT_PLAYLIST );
    set_subcategory( SUBCAT_PLAYLIST_SD );

    add_string( "podcast-urls", NULL, NULL,
                URLS_TEXT, URLS_LONGTEXT, false );
        change_autosave();

    set_capability( "services_discovery", 0 );
    set_callbacks( Open, Close );

vlc_module_end();


/*****************************************************************************
 * Local structures
 *****************************************************************************/

struct services_discovery_sys_t
{
    /* playlist node */
    input_thread_t **pp_input;
    int i_input;

    char **ppsz_urls;
    int i_urls;

    bool b_update;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void Run( services_discovery_t *p_intf );
static int UrlsChange( vlc_object_t *, char const *, vlc_value_t,
                       vlc_value_t, void * );
static void ParseUrls( services_discovery_t *p_sd, char *psz_urls );

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys  = malloc(
                                    sizeof( services_discovery_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->i_urls = 0;
    p_sys->ppsz_urls = NULL;
    p_sys->i_input = 0;
    p_sys->pp_input = NULL;
    p_sys->b_update = true;

    p_sd->pf_run = Run;
    p_sd->p_sys  = p_sys;

    /* Give us a name */
    services_discovery_SetLocalizedName( p_sd, _("Podcasts") );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys  = p_sd->p_sys;
    int i;
    for( i = 0; i < p_sys->i_input; i++ )
    {
        if( p_sd->p_sys->pp_input[i] )
        {
            input_StopThread( p_sd->p_sys->pp_input[i] );
            vlc_object_release( p_sd->p_sys->pp_input[i] );
            p_sd->p_sys->pp_input[i] = NULL;
        }
    }
    free( p_sd->p_sys->pp_input );
    for( i = 0; i < p_sys->i_urls; i++ ) free( p_sys->ppsz_urls[i] );
    free( p_sys->ppsz_urls );
    free( p_sys );
}

/*****************************************************************************
 * Run: main thread
 *****************************************************************************/
static void Run( services_discovery_t *p_sd )
{
    services_discovery_sys_t *p_sys  = p_sd->p_sys;

    /* Launch the callback associated with this variable */
    var_Create( p_sd, "podcast-urls", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_AddCallback( p_sd, "podcast-urls", UrlsChange, p_sys );

    while( vlc_object_alive (p_sd) )
    {
        int i;
        if( p_sys->b_update == true )
        {
            msg_Dbg( p_sd, "Update required" );
            char* psz_urls = var_GetNonEmptyString( p_sd, "podcast-urls" );
            if( psz_urls != NULL )
                ParseUrls( p_sd, psz_urls );
            free( psz_urls );
            p_sys->b_update = false;
        }

        for( i = 0; i < p_sd->p_sys->i_input; i++ )
        {
            if( p_sd->p_sys->pp_input[i]->b_eof
                || p_sd->p_sys->pp_input[i]->b_error )
            {
                input_StopThread( p_sd->p_sys->pp_input[i] );
                vlc_object_release( p_sd->p_sys->pp_input[i] );
                p_sd->p_sys->pp_input[i] = NULL;
                REMOVE_ELEM( p_sys->pp_input, p_sys->i_input, i );
                i--;
            }
        }
        msleep( 500 );
    }
}

static int UrlsChange( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval,
                       void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(psz_var); VLC_UNUSED(oldval);
    VLC_UNUSED(newval);
    services_discovery_sys_t *p_sys  = (services_discovery_sys_t *)p_data;
    p_sys->b_update = true;
    return VLC_SUCCESS;
}

static void ParseUrls( services_discovery_t *p_sd, char *psz_urls )
{
    services_discovery_sys_t *p_sys = p_sd->p_sys;
    for( ;; )
    {
        int i;
        char *psz_tok = strchr( psz_urls, '|' );
        if( psz_tok ) *psz_tok = '\0';
        for( i = 0; i < p_sys->i_urls; i++ )
            if( !strcmp( psz_urls, p_sys->ppsz_urls[i] ) )
                break;
        if( i == p_sys->i_urls )
        {
            /* Only add new urls.
             * FIXME: We don't delete urls which have been removed from
             * the config since we don't have a way to know which inputs
             * they spawned */
            input_item_t *p_input;
            INSERT_ELEM( p_sys->ppsz_urls, p_sys->i_urls, p_sys->i_urls,
                         strdup( psz_urls ) );
            p_input = input_item_NewExt( p_sd, psz_urls,
                                        psz_urls, 0, NULL, -1 );
            input_item_AddOption( p_input, "demux=podcast" );
            services_discovery_AddItem( p_sd, p_input, NULL /* no cat */ );
            vlc_gc_decref( p_input );
            INSERT_ELEM( p_sys->pp_input, p_sys->i_input, p_sys->i_input,
                         input_CreateThread( p_sd, p_input ) );
        }
        if( psz_tok )  psz_urls = psz_tok+1;
        else           return;
    }
}
