/*****************************************************************************
 * tcp.c: TCP input module
 *****************************************************************************
 * Copyright (C) 2003-2004 VideoLAN
 * $Id: tcp.c,v 1.4 2004/01/25 17:31:22 gbazin Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "network.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for udp streams. This " \
    "value should be set in miliseconds units." )

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("TCP input") );

    add_integer( "tcp-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT,
                 CACHING_LONGTEXT, VLC_TRUE );

    set_capability( "access", 0 );
    add_shortcut( "tcp" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct access_sys_t
{
    int fd;
};

static ssize_t Read ( input_thread_t *, byte_t *, size_t );

/*****************************************************************************
 * Open: open the socket
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    access_sys_t   *p_sys;

    char           *psz_dup = strdup(p_input->psz_name);
    char           *psz_parser = psz_dup;

    vlc_value_t    val;

    /* Parse server:port */
    while( *psz_parser && *psz_parser != ':' )
    {
        if( *psz_parser == '[' )
        {
            /* IPV6 */
            while( *psz_parser && *psz_parser  != ']' )
            {
                psz_parser++;
            }
        }
        psz_parser++;
    }
    if( *psz_parser != ':' || psz_parser == psz_dup )
    {
        msg_Err( p_input, "you have to provide server:port addresse" );
        free( psz_dup );
        return VLC_EGENERIC;
    }
    *psz_parser++ = '\0';

    if( atoi( psz_parser ) <= 0 )
    {
        msg_Err( p_input, "invalid port number (%d)", atoi( psz_parser ) );
        free( psz_dup );
        return VLC_EGENERIC;
    }

    /* Connect */
    p_input->p_access_data = p_sys = malloc( sizeof( access_sys_t ) );
    p_sys->fd = net_OpenTCP( p_input, psz_dup, atoi( psz_parser ) );
    free( psz_dup );

    if( p_sys->fd < 0 )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_input->pf_read = Read;
    p_input->pf_set_program = input_SetProgram;
    p_input->pf_set_area = NULL;
    p_input->pf_seek = NULL;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = VLC_TRUE;  /* FIXME ? */
    p_input->stream.b_seekable = 0;
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.i_method = INPUT_METHOD_NETWORK;
    p_input->i_mtu = 0;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* Update default_pts to a suitable value for udp access */
    var_Create( p_input, "tcp-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_input, "tcp-caching", &val );
    p_input->i_pts_delay = val.i_int * 1000;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    access_sys_t   *p_sys = p_input->p_access_data;

    msg_Info( p_input, "closing TCP target `%s'", p_input->psz_source );

    net_Close( p_sys->fd );
    free( p_sys );
}

/*****************************************************************************
 * Read: read on a file descriptor, checking b_die periodically
 *****************************************************************************/
static ssize_t Read( input_thread_t * p_input, byte_t * p_buffer, size_t i_len )
{
    access_sys_t   *p_sys = p_input->p_access_data;

    return net_Read( p_input, p_sys->fd, p_buffer, i_len, VLC_FALSE );
}
