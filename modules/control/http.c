/*****************************************************************************
 * http.c :  http remote control plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: http.c,v 1.7 2003/05/22 15:34:02 hartman Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <errno.h>                                                 /* ENOMEM */
#include <stdio.h>
#include <ctype.h>
#include <signal.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif

#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif
#include <sys/types.h>

#include "httpd.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate     ( vlc_object_t * );
static void Close        ( vlc_object_t * );
static void Run          ( intf_thread_t *p_intf );

static int  httpd_page_interface_get( httpd_file_callback_args_t *p_args,
                                      uint8_t *p_request, int i_request,
                                      uint8_t **pp_data, int *pi_data );

/*****************************************************************************
 * intf_sys_t: description and status of interface
 *****************************************************************************/
struct intf_sys_t
{
    httpd_t             *p_httpd;
    httpd_host_t        *p_httpd_host;

    input_thread_t *    p_input;
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define PORT_TEXT N_( "HTTP interface bind port" )
#define PORT_LONGTEXT N_( \
    "You can set the port on which the http interface will accept connections" )
#define ADDR_TEXT N_( "HTTP interface bind address" )
#define ADDR_LONGTEXT N_( \
    "You can set the address on which the http interface will bind" )

vlc_module_begin();
    add_category_hint( N_("HTTP remote control"), NULL, VLC_TRUE );
    add_string( "http-addr", NULL, NULL, ADDR_TEXT, ADDR_LONGTEXT, VLC_TRUE );
    add_integer( "http-port", 8080, NULL, PORT_TEXT, PORT_LONGTEXT, VLC_TRUE );
    set_description( _("HTTP remote control interface") );
    set_capability( "interface", 10 );
    set_callbacks( Activate, Close );
vlc_module_end();

/*****************************************************************************
 * Activate: initialize and create stuff
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        return VLC_EGENERIC;
    };

    p_intf->p_sys->p_httpd = NULL;
    p_intf->p_sys->p_httpd_host = NULL;

    p_intf->pf_run = Run;

    CONSOLE_INTRO_MSG;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseIntf: destroy interface
 *****************************************************************************/
void Close ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    if( p_intf->p_sys->p_httpd_host )
        p_intf->p_sys->p_httpd->pf_unregister_host( p_intf->p_sys->p_httpd,
                                                 p_intf->p_sys->p_httpd_host );

    if( p_intf->p_sys->p_httpd )
        httpd_Release( p_intf->p_sys->p_httpd );

    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: http interface thread
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    input_thread_t *p_input = NULL;
    playlist_t     *p_playlist = NULL;
    httpd_file_t   *p_page_intf;

#if 0
    input_info_category_t * p_category;
    input_info_t * p_info;

    off_t      i_oldpos = 0;
    off_t      i_newpos;
    double     f_ratio = 1.0;
#endif

    /* Get bind address and port */
    char *psz_bind_addr = config_GetPsz( p_intf, "http-addr" );
    int i_bind_port = config_GetInt( p_intf, "http-port" );
    if( !psz_bind_addr ) psz_bind_addr = strdup( "" );

    p_intf->p_sys->p_httpd = httpd_Find( VLC_OBJECT(p_intf), VLC_TRUE );
    if( !p_intf->p_sys->p_httpd )
    {
        msg_Err( p_intf, "cannot start httpd daemon" );
        free( p_intf->p_sys );
        return;
    }

    p_intf->p_sys->p_httpd_host =
        p_intf->p_sys->p_httpd->pf_register_host( p_intf->p_sys->p_httpd,
                                                  psz_bind_addr, i_bind_port );

    if( !p_intf->p_sys->p_httpd_host )
    {
        msg_Err( p_intf, "cannot listen on %s:%d", psz_bind_addr, i_bind_port);
        httpd_Release( p_intf->p_sys->p_httpd );
        free( p_intf->p_sys );
        return;
    }

    msg_Info( p_intf, "http interface started" );

    /*
     * Register our interface page with the httpd daemon
     */
    p_page_intf = p_intf->p_sys->p_httpd->pf_register_file(
                      p_intf->p_sys->p_httpd, "/", "text/html",
                      NULL, NULL, httpd_page_interface_get,
                      httpd_page_interface_get,
                      (httpd_file_callback_args_t*)p_intf );

    while( !p_intf->b_die )
    {

        /* Manage the input part */
        if( p_input == NULL )
        {
            if( p_playlist )
            {
                p_input = vlc_object_find( p_playlist, VLC_OBJECT_INPUT,
                                                       FIND_CHILD );
            }
            else
            {
                p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                   FIND_ANYWHERE );
                if( p_input )
                {
                    p_playlist = vlc_object_find( p_input, VLC_OBJECT_PLAYLIST,
                                                           FIND_PARENT );
                }
            }
        }
        else if( p_input->b_dead )
        {
            vlc_object_release( p_input );
            p_input = NULL;
        }

        if( p_input )
        {
            /* Get position */
            vlc_mutex_lock( &p_input->stream.stream_lock );
            if( !p_input->b_die && p_input->stream.i_mux_rate )
            {
#if 0
#define A p_input->stream.p_selected_area
                f_ratio = 1.0 / ( 50 * p_input->stream.i_mux_rate );
                i_newpos = A->i_tell * f_ratio;

                if( i_oldpos != i_newpos )
                {
                    i_oldpos = i_newpos;
                    printf( "pos: %li s / %li s\n", (long int)i_newpos,
                            (long int)(f_ratio * A->i_size) );
                }
#undef S
#endif
            }
            vlc_mutex_unlock( &p_input->stream.stream_lock );
        }

        /* Wait a bit */
        msleep( INTF_IDLE_SLEEP );
    }

    p_intf->p_sys->p_httpd->pf_unregister_file( p_intf->p_sys->p_httpd,
                                                p_page_intf );

    if( p_input )
    {
        vlc_object_release( p_input );
        p_input = NULL;
    }

    if( p_playlist )
    {
        vlc_object_release( p_playlist );
        p_playlist = NULL;
    }
}

/*****************************************************************************
 * Local functions
 *****************************************************************************/
static int httpd_page_interface_update( intf_thread_t *p_intf,
                                        playlist_t *p_playlist,
                                        uint8_t **pp_data, int *pi_data, vlc_bool_t b_redirect );

static void uri_extract_value( char *psz_uri, char *psz_name,
                               char *psz_value, int i_value_max )
{
    char *p;

    p = strstr( psz_uri, psz_name );
    if( p )
    {
        int i_len;

        p += strlen( psz_name );
        if( *p == '=' ) p++;

        if( strchr( p, '&' ) )
        {
            i_len = strchr( p, '&' ) - p;
        }
        else
        {
            /* for POST method */
            if( strchr( p, '\n' ) )
            {
                i_len = strchr( p, '\n' ) - p;
                if( i_len && *(p+i_len-1) == '\r' ) i_len--;
            }
            else
            {
                i_len = strlen( p );
            }
        }
        i_len = __MIN( i_value_max - 1, i_len );
        if( i_len > 0 )
        {
            strncpy( psz_value, p, i_len );
            psz_value[i_len] = '\0';
        }
        else
        {
            strncpy( psz_value, "", i_value_max );
        }
    }
    else
    {
        strncpy( psz_value, "", i_value_max );
    }
}

static void uri_decode_url_encoded( char *psz )
{
    char *dup = strdup( psz );
    char *p = dup;

    while( *p )
    {
        if( *p == '%' )
        {
            char val[3];
            p++;
            if( !*p )
            {
                break;
            }

            val[0] = *p++;
            val[1] = *p++;
            val[2] = '\0';

            *psz++ = strtol( val, NULL, 16 );
        }
        else if( *p == '+' )
        {
            *psz++ = ' ';
            p++;
        }
        else
        {
            *psz++ = *p++;
        }
    }
    *psz++  ='\0';
    free( dup );
}

static int httpd_page_interface_get( httpd_file_callback_args_t *p_args,
                                     uint8_t *p_request, int i_request,
                                     uint8_t **pp_data, int *pi_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_args;
    playlist_t *p_playlist;
    int i_ret;

    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( !p_playlist )
    {
        return VLC_EGENERIC;
    }

    if( i_request > 0)
    {
        char action[512];

        uri_extract_value( p_request, "action", action, 512 );

        if( !strcmp( action, "play" ) )
        {
            int i_item;
            uri_extract_value( p_request, "item", action, 512 );
            i_item = atol( action );
            playlist_Command( p_playlist, PLAYLIST_GOTO, i_item );
            msg_Dbg( p_intf, "requested playlist item: %i", i_item );
        }
        else if( !strcmp( action, "stop" ) )
        {
            playlist_Command( p_playlist, PLAYLIST_STOP, 0 );
            msg_Dbg( p_intf, "requested playlist stop" );
        }
        else if( !strcmp( action, "pause" ) )
        {
            playlist_Command( p_playlist, PLAYLIST_PAUSE, 0 );
            msg_Dbg( p_intf, "requested playlist pause" );
        }
        else if( !strcmp( action, "next" ) )
        {
            playlist_Command( p_playlist, PLAYLIST_GOTO,
                              p_playlist->i_index + 1 );
            msg_Dbg( p_intf, "requested playlist next" );
        }
        else if( !strcmp( action, "previous" ) )
        {
            playlist_Command( p_playlist, PLAYLIST_GOTO,
                              p_playlist->i_index - 1 );
            msg_Dbg( p_intf, "requested playlist previous" );
        }
        else if( !strcmp( action, "add" ) )
        {
            uri_extract_value( p_request, "mrl", action, 512 );
            uri_decode_url_encoded( action );
            playlist_Add( p_playlist, action,
                          PLAYLIST_APPEND, PLAYLIST_END );
            msg_Dbg( p_intf, "requested playlist add: %s", action );
        }
    }

    i_ret = httpd_page_interface_update( p_intf, p_playlist, pp_data, pi_data, i_request ? VLC_TRUE : VLC_FALSE );

    vlc_object_release( p_playlist );

    return i_ret;
}

static int httpd_page_interface_update( intf_thread_t *p_intf,
                                        playlist_t *p_playlist,
                                        uint8_t **pp_data, int *pi_data, vlc_bool_t b_redirect )
{
    int i, i_size = 0;
    char *p;

    vlc_mutex_lock( &p_playlist->object_lock );

    /*
     * Count playlist items for memory allocation
     */
    for ( i = 0; i < p_playlist->i_size; i++ )
    {
        i_size += sizeof("<a href=?action=play&item=?>? - </a><br />\n" );
        i_size += strlen( p_playlist->pp_items[i]->psz_name );
    }
    /* add something for all the static strings below */
    i_size += 8192;

    p = *pp_data = malloc( i_size );

    p += sprintf( p, "<html>\n" );
    p += sprintf( p, "<head>\n" );
    p += sprintf( p, "<title>VLC Media Player</title>\n" );
    if( b_redirect )
    {
        p += sprintf( p, "<meta http-equiv=\"refresh\" content=\"0;URL=/\"\n" );
    }
    /* p += sprintf( p, "<link rel=\"shortcut icon\" href=\"http://www.videolan.org/favicon.ico\">\n" ); */
    p += sprintf( p, "</head>\n" );
    p += sprintf( p, "<body>\n" );
    p += sprintf( p, "<h2><center><a href=\"http://www.videolan.org\">"
                     "VLC Media Player</a> (http interface)</center></h2>\n" );

    /*
     * Display the controls
     */
    p += sprintf( p, "<hr />\n" );

    p += sprintf( p, "<td><form method=\"get\" action=\"\">"
        "<input type=\"submit\" name=\"action\" value=\"stop\" />"
        "<input type=\"submit\" name=\"action\" value=\"pause\" />"
        "<input type=\"submit\" name=\"action\" value=\"previous\" />"
        "<input type=\"submit\" name=\"action\" value=\"next\" />"
        "</form></td><br />\n" );

    p += sprintf( p, "<td><form method=\"get\" action=\"\" "
                     "enctype=\"text/plain\" >"
        "Media Resource Locator: "
        "<input type=\"text\" name=\"mrl\" size=\"40\" />"
        "<input type=\"submit\" name=\"action\" value=\"add\" />"
        "</form></td>\n" );

    p += sprintf( p, "<hr />\n" );

    /*
     * Display the playlist items
     */
    for ( i = 0; i < p_playlist->i_size; i++ )
    {
        if( i == p_playlist->i_index ) p += sprintf( p, "<b>" );

        p += sprintf( p, "<a href=?action=play&item=%i>", i );
        p += sprintf( p, "%i - %s", i,
                      p_playlist->pp_items[i]->psz_name );
        p += sprintf( p, "</a>" );

        if( i == p_playlist->i_index ) p += sprintf( p, "</b>" );
        p += sprintf( p, "<br />\n" );
    }
    if ( i == 0 )
    {
        p += sprintf( p, "no entries\n" );
    }

    p += sprintf( p, "</body>\n" );
    p += sprintf( p, "</html>\n" );

    *pi_data = strlen( *pp_data ) + 1;

    vlc_mutex_unlock( &p_playlist->object_lock );

    return VLC_SUCCESS;
}
