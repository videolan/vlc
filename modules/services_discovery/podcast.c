/*****************************************************************************
 * podcast.c:  Podcast services discovery module
 *****************************************************************************
 * Copyright (C) 2005-2009 the VideoLAN team
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
#include <vlc_services_discovery.h>

#include <vlc_network.h>
#include <assert.h>

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

vlc_module_begin ()
    set_shortname( "Podcast")
    set_description( N_("Podcasts") )
    set_category( CAT_PLAYLIST )
    set_subcategory( SUBCAT_PLAYLIST_SD )

    add_string( "podcast-urls", NULL, NULL,
                URLS_TEXT, URLS_LONGTEXT, false )
        change_autosave ()

    set_capability( "services_discovery", 0 )
    set_callbacks( Open, Close )

vlc_module_end ()


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

    vlc_thread_t thread;
    vlc_mutex_t lock;
    vlc_cond_t  wait;
    bool b_update;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void *Run( void * );
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
    vlc_mutex_init( &p_sys->lock );
    vlc_cond_init( &p_sys->wait );
    p_sys->b_update = true;

    p_sd->p_sys  = p_sys;

    /* Launch the callback associated with this variable */
    var_Create( p_sd, "podcast-urls", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_AddCallback( p_sd, "podcast-urls", UrlsChange, p_sys );

    if (vlc_clone (&p_sys->thread, Run, p_sd, VLC_THREAD_PRIORITY_LOW))
    {
        var_DelCallback( p_sd, "podcast-urls", UrlsChange, p_sys );
        vlc_cond_destroy( &p_sys->wait );
        vlc_mutex_destroy( &p_sys->lock );
        free (p_sys);
        return VLC_EGENERIC;
    }
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

    vlc_cancel (p_sys->thread);
    vlc_join (p_sys->thread, NULL);

    var_DelCallback( p_sd, "podcast-urls", UrlsChange, p_sys );
    vlc_cond_destroy( &p_sys->wait );
    vlc_mutex_destroy( &p_sys->lock );

    for( i = 0; i < p_sys->i_input; i++ )
    {
        input_thread_t *p_input = p_sd->p_sys->pp_input[i];
        if( !p_input )
            continue;

        input_Stop( p_input, true );
        vlc_thread_join( p_input );
        vlc_object_release( p_input );

        p_sd->p_sys->pp_input[i] = NULL;
    }
    free( p_sd->p_sys->pp_input );
    for( i = 0; i < p_sys->i_urls; i++ ) free( p_sys->ppsz_urls[i] );
    free( p_sys->ppsz_urls );
    free( p_sys );
}

/*****************************************************************************
 * Run: main thread
 *****************************************************************************/
static void *Run( void *data )
{
    services_discovery_t *p_sd = data;
    services_discovery_sys_t *p_sys  = p_sd->p_sys;

    vlc_mutex_lock( &p_sys->lock );
    mutex_cleanup_push( &p_sys->lock );
    for( ;; )
    {
        while( !p_sys->b_update )
            vlc_cond_wait( &p_sys->wait, &p_sys->lock );

        int canc = vlc_savecancel ();
        msg_Dbg( p_sd, "Update required" );
        char* psz_urls = var_GetNonEmptyString( p_sd, "podcast-urls" );
        if( psz_urls != NULL )
            ParseUrls( p_sd, psz_urls );
        free( psz_urls );
        p_sys->b_update = false;

        for( int i = 0; i < p_sd->p_sys->i_input; i++ )
        {
            input_thread_t *p_input = p_sd->p_sys->pp_input[i];

            if( p_input->b_eof || p_input->b_error )
            {
                input_Stop( p_input, false );
                vlc_thread_join( p_input );
                vlc_object_release( p_input );

                p_sd->p_sys->pp_input[i] = NULL;
                REMOVE_ELEM( p_sys->pp_input, p_sys->i_input, i );
                i--;
            }
        }
        vlc_restorecancel (canc);
    }
    vlc_cleanup_pop();
    assert(0); /* dead code */
}

static int UrlsChange( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval,
                       void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(psz_var); VLC_UNUSED(oldval);
    VLC_UNUSED(newval);
    services_discovery_sys_t *p_sys  = (services_discovery_sys_t *)p_data;

    vlc_mutex_lock( &p_sys->lock );
    p_sys->b_update = true;
    vlc_cond_signal( &p_sys->wait );
    vlc_mutex_unlock( &p_sys->lock );
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
            p_input = input_item_New( p_sd, psz_urls, psz_urls );
            input_item_AddOption( p_input, "demux=podcast", VLC_INPUT_OPTION_TRUSTED );
            services_discovery_AddItem( p_sd, p_input, NULL /* no cat */ );
            vlc_gc_decref( p_input );
            INSERT_ELEM( p_sys->pp_input, p_sys->i_input, p_sys->i_input,
                         input_CreateAndStart( p_sd, p_input, NULL ) );
        }
        if( psz_tok )  psz_urls = psz_tok+1;
        else           return;
    }
}
