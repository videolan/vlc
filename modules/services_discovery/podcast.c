/*****************************************************************************
 * podcast.c:  Podcast services discovery module
 *****************************************************************************
 * Copyright (C) 2005-2009 the VideoLAN team
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

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_services_discovery.h>
#include <vlc_network.h>

#include <assert.h>
#include <stdnoreturn.h>
#include <unistd.h>

/************************************************************************
 * Macros and definitions
 ************************************************************************/

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

/* Callbacks */
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

VLC_SD_PROBE_HELPER("podcast", N_("Podcasts"), SD_CAT_INTERNET)

#define URLS_TEXT N_("Podcast URLs list")
#define URLS_LONGTEXT N_("Enter the list of podcasts to retrieve, " \
                         "separated by '|' (pipe)." )

vlc_module_begin ()
    set_shortname( "Podcast")
    set_description( N_("Podcasts") )
    set_category( CAT_PLAYLIST )
    set_subcategory( SUBCAT_PLAYLIST_SD )

    add_string( "podcast-urls", NULL,
                URLS_TEXT, URLS_LONGTEXT, false )

    set_capability( "services_discovery", 0 )
    set_callbacks( Open, Close )

    VLC_SD_PROBE_SUBMODULE

vlc_module_end ()


/*****************************************************************************
 * Local structures
 *****************************************************************************/

enum {
  UPDATE_URLS,
  UPDATE_REQUEST
}; /* FIXME Temporary. Updating by compound urls string to be removed later. */

typedef struct
{
    /* playlist node */
    input_thread_t **pp_input;
    int i_input;

    char **ppsz_urls;
    int i_urls;

    input_item_t **pp_items;
    int i_items;

    vlc_thread_t thread;
    vlc_mutex_t lock;
    vlc_cond_t  wait;
    bool b_update;
    bool b_savedurls_loaded;
    char *psz_request;
    int update_type;
} services_discovery_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void *Run( void * );
static int UrlsChange( vlc_object_t *, char const *, vlc_value_t,
                       vlc_value_t, void * );
static int Request( vlc_object_t *, char const *, vlc_value_t,
                       vlc_value_t, void * );
static void ParseRequest( services_discovery_t *p_sd );
static void ParseUrls( services_discovery_t *p_sd, char *psz_urls );
static void SaveUrls( services_discovery_t *p_sd );

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    if( strcmp( vlc_object_typename(vlc_object_parent(p_this)), "playlist" ) )
        return VLC_EGENERIC; /* FIXME: support LibVLC SD too! */

    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->i_urls = 0;
    p_sys->ppsz_urls = NULL;
    p_sys->i_input = 0;
    p_sys->pp_input = NULL;
    p_sys->pp_items = NULL;
    p_sys->i_items = 0;
    vlc_mutex_init( &p_sys->lock );
    vlc_cond_init( &p_sys->wait );
    p_sys->b_update = true;
    p_sys->b_savedurls_loaded = false;
    p_sys->psz_request = NULL;
    p_sys->update_type = UPDATE_URLS;

    p_sd->p_sys  = p_sys;
    p_sd->description = _("Podcasts");

    /* Launch the callback associated with this variable */
    vlc_object_t *pl = vlc_object_parent(p_sd);
    var_Create( pl, "podcast-urls", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_AddCallback( pl, "podcast-urls", UrlsChange, p_sys );

    var_Create( pl, "podcast-request", VLC_VAR_STRING );
    var_AddCallback( pl, "podcast-request", Request, p_sys );

    if (vlc_clone (&p_sys->thread, Run, p_sd, VLC_THREAD_PRIORITY_LOW))
    {
        var_DelCallback( pl, "podcast-request", Request, p_sys );
        var_DelCallback( pl, "podcast-urls", UrlsChange, p_sys );
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
    services_discovery_sys_t *p_sys = p_sd->p_sys;
    vlc_object_t *pl = vlc_object_parent(p_sd);

    vlc_cancel (p_sys->thread);
    vlc_join (p_sys->thread, NULL);

    var_DelCallback( pl, "podcast-urls", UrlsChange, p_sys );
    var_DelCallback( pl, "podcast-request", Request, p_sys );
    vlc_cond_destroy( &p_sys->wait );
    vlc_mutex_destroy( &p_sys->lock );

    for( int i = 0; i < p_sys->i_input; i++ )
    {
        input_thread_t *p_input = p_sys->pp_input[i];
        if( !p_input )
            continue;

        input_Stop( p_input );
        input_Close( p_input );

        p_sys->pp_input[i] = NULL;
    }
    free( p_sys->pp_input );

    for( int i = 0; i < p_sys->i_urls; i++ )
         free( p_sys->ppsz_urls[i] );
    free( p_sys->ppsz_urls );

    for( int i = 0; i < p_sys->i_items; i++ )
         input_item_Release( p_sys->pp_items[i] );
    free( p_sys->pp_items );

    free( p_sys->psz_request );
    free( p_sys );
}

/*****************************************************************************
 * Run: main thread
 *****************************************************************************/
static input_thread_t *InputCreateAndStart( services_discovery_t *sd,
                                            input_item_t *item )
{
    input_thread_t *input = input_Create( sd, input_LegacyEvents, NULL, item, NULL, NULL );
    if( input != NULL && input_Start( input ) )
    {
        input_LegacyVarInit( input );
        vlc_object_release( input );
        input = NULL;
    }
    return input;
}

noreturn static void *Run( void *data )
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

        if( p_sys->update_type == UPDATE_URLS )
        {
            char *psz_urls = var_GetNonEmptyString( vlc_object_parent(p_sd),
                                                    "podcast-urls" );
            ParseUrls( p_sd, psz_urls );
            free( psz_urls );
        }
        else if( p_sys->update_type == UPDATE_REQUEST )
            ParseRequest( p_sd );

        p_sys->b_update = false;

        for( int i = 0; i < p_sys->i_input; i++ )
        {
            input_thread_t *p_input = p_sys->pp_input[i];
            int state = var_GetInteger( p_input, "state" );

            if( state == END_S || state == ERROR_S )
            {
                input_Stop( p_input );
                input_Close( p_input );

                p_sys->pp_input[i] = NULL;
                TAB_ERASE(p_sys->i_input, p_sys->pp_input, i);
                i--;
            }
        }
        vlc_restorecancel (canc);
    }
    vlc_cleanup_pop();
    vlc_assert_unreachable(); /* dead code */
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
    p_sys->update_type = UPDATE_URLS;
    vlc_cond_signal( &p_sys->wait );
    vlc_mutex_unlock( &p_sys->lock );
    return VLC_SUCCESS;
}

static int Request( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval,
                       void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(psz_var); VLC_UNUSED(oldval);
    services_discovery_sys_t *p_sys = (services_discovery_sys_t *)p_data;

    vlc_mutex_lock( &p_sys->lock );
    free( p_sys->psz_request );
    p_sys->psz_request = NULL;
    if( newval.psz_string && *newval.psz_string ) {
      p_sys->psz_request = strdup( newval.psz_string );
      p_sys->b_update = true;
      p_sys->update_type = UPDATE_REQUEST;
      vlc_cond_signal( &p_sys->wait );
    }
    vlc_mutex_unlock( &p_sys->lock );
    return VLC_SUCCESS;
}

static void ParseUrls( services_discovery_t *p_sd, char *psz_urls )
{
    services_discovery_sys_t *p_sys = p_sd->p_sys;
    int i_new_items = 0;
    input_item_t **pp_new_items = NULL;

    int i_new_urls = 0;
    char **ppsz_new_urls = NULL;
    p_sys->b_savedurls_loaded = true;

    int i, j;

    for( ;; )
    {
        if( !psz_urls )
            break;

        char *psz_tok = strchr( psz_urls, '|' );
        if( psz_tok ) *psz_tok = '\0';

        for( i = 0; i < p_sys->i_urls; i++ )
            if( !strcmp( psz_urls, p_sys->ppsz_urls[i] ) )
                break;
        if( i == p_sys->i_urls )
        {
            TAB_APPEND( i_new_urls, ppsz_new_urls, strdup( psz_urls ) );

            input_item_t *p_input;
            p_input = input_item_New( psz_urls, psz_urls );
            input_item_AddOption( p_input, "demux=directory", VLC_INPUT_OPTION_TRUSTED );

            TAB_APPEND( i_new_items, pp_new_items, p_input );
            services_discovery_AddItem( p_sd, p_input );

            TAB_APPEND( p_sys->i_input, p_sys->pp_input,
                         InputCreateAndStart( p_sd, p_input ) );
        }
        else
        {
            TAB_APPEND( i_new_urls, ppsz_new_urls,
                         strdup( p_sys->ppsz_urls[i]) );
            TAB_APPEND( i_new_items, pp_new_items, p_sys->pp_items[i] );
        }
        if( psz_tok )
            psz_urls = psz_tok+1;
        else
            break;
    }

    /* delete removed items and signal the removal */
    for( i = 0; i<p_sys->i_items; ++i )
    {
        for( j = 0; j < i_new_items; ++j )
            if( pp_new_items[j] == p_sys->pp_items[i] ) break;
        if( j == i_new_items )
        {
            services_discovery_RemoveItem( p_sd, p_sys->pp_items[i] );
            input_item_Release( p_sys->pp_items[i] );
        }
    }
    free( p_sys->pp_items );
    for( int i = 0; i < p_sys->i_urls; i++ )
        free( p_sys->ppsz_urls[i] );
    free( p_sys->ppsz_urls );

    p_sys->ppsz_urls = ppsz_new_urls;
    p_sys->i_urls = i_new_urls;
    p_sys->pp_items = pp_new_items;
    p_sys->i_items = i_new_items;
}

static void ParseRequest( services_discovery_t *p_sd )
{
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    char *psz_request = p_sys->psz_request;

    int i;

    char *psz_tok = strchr( psz_request, ':' );
    if( !psz_tok ) return;
    *psz_tok = '\0';

    if ( ! p_sys->b_savedurls_loaded )
    {
        char *psz_urls = var_GetNonEmptyString( vlc_object_parent(p_sd),
                                                "podcast-urls" );
        ParseUrls( p_sd, psz_urls );
        free( psz_urls );
    }

    if( !strcmp( psz_request, "ADD" ) )
    {
        psz_request = psz_tok + 1;
        for( i = 0; i<p_sys->i_urls; i++ )
            if( !strcmp(p_sys->ppsz_urls[i],psz_request) )
              break;
        if( i == p_sys->i_urls )
        {
            TAB_APPEND( p_sys->i_urls, p_sys->ppsz_urls,
                        strdup( psz_request ) );

            input_item_t *p_input;
            p_input = input_item_New( psz_request, psz_request );
            input_item_AddOption( p_input, "demux=directory", VLC_INPUT_OPTION_TRUSTED );

            TAB_APPEND( p_sys->i_items, p_sys->pp_items, p_input );
            services_discovery_AddItem( p_sd, p_input );

            TAB_APPEND( p_sys->i_input, p_sys->pp_input,
                        InputCreateAndStart( p_sd, p_input ) );
            SaveUrls( p_sd );
        }
    }
    else if ( !strcmp( psz_request, "RM" ) )
    {
        psz_request = psz_tok + 1;
        for( i = 0; i < p_sys->i_urls; i++ )
            if( !strcmp(p_sys->ppsz_urls[i], psz_request) )
            {
                services_discovery_RemoveItem( p_sd, p_sys->pp_items[i] );
                input_item_Release( p_sys->pp_items[i] );
                TAB_ERASE(p_sys->i_urls, p_sys->ppsz_urls, i );
                TAB_ERASE(p_sys->i_items, p_sys->pp_items, i );
                break;
            }
        SaveUrls( p_sd );
    }

    free( p_sys->psz_request );
    p_sys->psz_request = NULL;
}

static void SaveUrls( services_discovery_t *p_sd )
{
    services_discovery_sys_t *p_sys = p_sd->p_sys;
    int i;
    char *psz_urls;
    int len = 0;

    for( i=0; i < p_sys->i_urls; i++ )
        len += strlen( p_sys->ppsz_urls[i] ) + 1;

    psz_urls = (char*) calloc( len, sizeof(char) );

    for( i=0; i < p_sys->i_urls; i++ )
    {
        strcat( psz_urls, p_sys->ppsz_urls[i] );
        if( i < p_sys->i_urls - 1 ) strcat( psz_urls, "|" );
    }

    config_PutPsz( "podcast-urls", psz_urls );

    free( psz_urls );
}
