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

#include <vlc/vlc.h>
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
    set_description( _("Podcasts") );
    set_category( CAT_PLAYLIST );
    set_subcategory( SUBCAT_PLAYLIST_SD );

    add_string( "podcast-urls", NULL, NULL,
                URLS_TEXT, URLS_LONGTEXT, VLC_FALSE );
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

    char **ppsz_urls;
    int i_urls;
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
    
    p_sys->i_urls = 0;
    p_sys->ppsz_urls = NULL;
    p_sys->pp_input = NULL;
    
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
    for( i = 0; i < p_sys->i_urls; i++ )
    {
        if( p_sd->p_sys->pp_input[i] )
        {
            input_StopThread( p_sd->p_sys->pp_input[i] );
            input_DestroyThread( p_sd->p_sys->pp_input[i] );
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

    int i, j;
    char *psz_buf;
    char *psz_tmp = psz_buf = var_CreateGetString( p_sd, "podcast-urls" );

    i = 0;
    p_sys->i_urls = psz_buf[0] ? 1 : 0;
    while( psz_buf[i] != 0 )
        if( psz_buf[i++] == '|' )
            p_sys->i_urls++;

    p_sys->ppsz_urls = (char **)malloc( p_sys->i_urls * sizeof( char * ) );

    i = 0;
    j = 0;
    while( psz_buf[i] != 0 )
    {
        if( psz_buf[i] == '|' )
        {
            psz_buf[i] = 0;
            p_sys->ppsz_urls[j] = strdup( psz_tmp );
            i++;
            j++;
            psz_tmp = psz_buf+i;
        }
        else
            i++;
    }
    p_sys->ppsz_urls[j] = strdup( psz_tmp );
    free( psz_buf );

    p_sys->pp_input = malloc( p_sys->i_urls * sizeof( input_thread_t * ) );
    for( i = 0; i < p_sys->i_urls; i++ )
    {
        input_item_t *p_input;
        asprintf( &psz_buf, "%s", p_sys->ppsz_urls[i] );
        p_input = input_ItemNewExt( p_sd, psz_buf,
                                    p_sys->ppsz_urls[i], 0, NULL, -1 );
        input_ItemAddOption( p_input, "demux=podcast" );
        services_discovery_AddItem( p_sd, p_input, NULL /* no cat */ );
        p_sys->pp_input[i] = input_CreateThread( p_sd, p_input );
    }

    while( !p_sd->b_die )
    {
        int i;
        for( i = 0; i < p_sd->p_sys->i_urls; i++ )
        {
            if( p_sd->p_sys->pp_input[i] &&
                ( p_sd->p_sys->pp_input[i]->b_eof
                  || p_sd->p_sys->pp_input[i]->b_error ) )
            {
                input_StopThread( p_sd->p_sys->pp_input[i] );
                input_DestroyThread( p_sd->p_sys->pp_input[i] );
                p_sd->p_sys->pp_input[i] = NULL;
            }
        }
        msleep( 100000 );
    }
}
