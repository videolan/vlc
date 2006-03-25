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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/intf.h>

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

    set_capability( "services_discovery", 0 );
    set_callbacks( Open, Close );

vlc_module_end();


/*****************************************************************************
 * Local structures
 *****************************************************************************/

struct services_discovery_sys_t
{
    /* playlist node */
    playlist_item_t *p_node;
    input_thread_t *p_input;

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

    vlc_value_t         val;
    playlist_t          *p_playlist;
    playlist_view_t     *p_view;
    playlist_item_t     *p_item;

    int i, j;
    char *psz_buf;
    char *psz_tmp = psz_buf = var_CreateGetString( p_sd, "podcast-urls" );

    i = 0;
    p_sys->i_urls = 1;
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

    p_sd->pf_run = Run;
    p_sd->p_sys  = p_sys;

    /* Create our playlist node */
    p_playlist = (playlist_t *)vlc_object_find( p_sd, VLC_OBJECT_PLAYLIST,
                                                FIND_ANYWHERE );
    if( !p_playlist )
    {
        msg_Warn( p_sd, "unable to find playlist, cancelling");
        return VLC_EGENERIC;
    }

    p_view = playlist_ViewFind( p_playlist, VIEW_CATEGORY );
    p_sys->p_node = playlist_NodeCreate( p_playlist, VIEW_CATEGORY,
                                         _("Podcast"), p_view->p_root );
    for( i = 0; i < p_sys->i_urls; i++ )
    {
        asprintf( &psz_buf, "%s", p_sys->ppsz_urls[i] );
        p_item = playlist_ItemNew( p_playlist, psz_buf,
                                         p_sys->ppsz_urls[i] );
        free( psz_buf );
        playlist_ItemAddOption( p_item, "demux=podcast" );
        playlist_NodeAddItem( p_playlist, p_item,
                              p_sys->p_node->pp_parents[0]->i_view,
                              p_sys->p_node, PLAYLIST_APPEND,
                              PLAYLIST_END );

        /* We need to declare the parents of the node as the same of the
         * parent's ones */
        playlist_CopyParents( p_sys->p_node, p_item );
        p_sys->p_input = input_CreateThread( p_playlist, &p_item->input );
    }


    p_sys->p_node->i_flags |= PLAYLIST_RO_FLAG;
    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-change", val );

    vlc_object_release( p_playlist );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys  = p_sd->p_sys;
    playlist_t *p_playlist =  (playlist_t *) vlc_object_find( p_sd,
                                 VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    int i;
    if( p_sd->p_sys->p_input )
    {
        input_StopThread( p_sd->p_sys->p_input );
        input_DestroyThread( p_sd->p_sys->p_input );
        vlc_object_detach( p_sd->p_sys->p_input );
        vlc_object_destroy( p_sd->p_sys->p_input );
        p_sd->p_sys->p_input = NULL;
    }
    if( p_playlist )
    {
        playlist_NodeDelete( p_playlist, p_sys->p_node, VLC_TRUE, VLC_TRUE );
        vlc_object_release( p_playlist );
    }
    for( i = 0; i < p_sys->i_urls; i++ ) free( p_sys->ppsz_urls[i] );
    free( p_sys->ppsz_urls );
    free( p_sys );
}

/*****************************************************************************
 * Run: main thread
 *****************************************************************************/
static void Run( services_discovery_t *p_sd )
{
    while( !p_sd->b_die )
    {
        if( p_sd->p_sys->p_input &&
            ( p_sd->p_sys->p_input->b_eof || p_sd->p_sys->p_input->b_error ) )
        {
            input_StopThread( p_sd->p_sys->p_input );
            input_DestroyThread( p_sd->p_sys->p_input );
            vlc_object_detach( p_sd->p_sys->p_input );
            vlc_object_destroy( p_sd->p_sys->p_input );
            p_sd->p_sys->p_input = NULL;
        }
        msleep( 100000 );
    }
}
