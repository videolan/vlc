/*****************************************************************************
 * tcp.c: TCP input module
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_network.h>
#include <vlc_interrupt.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("TCP") )
    set_description( N_("TCP input") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )

    set_capability( "access", 0 )
    add_shortcut( "tcp" )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct access_sys_t
{
    int        fd;
};

static ssize_t Read( access_t *, void *, size_t );
static int Control( access_t *, int, va_list );

/*****************************************************************************
 * Open: open the socket
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t *)p_this;
    access_sys_t *p_sys;

    char         *psz_dup = strdup(p_access->psz_location);
    char         *psz_parser = psz_dup;

    /* Parse server:port */
    if( *psz_parser == '[' )
    {
        psz_parser = strchr( psz_parser, ']' );
        if( psz_parser == NULL )
            psz_parser = psz_dup;
    }
    psz_parser = strchr( psz_parser, ':' );

    if( psz_parser == NULL )
    {
        msg_Err( p_access, "missing port number : %s", psz_dup );
        free( psz_dup );
        return VLC_EGENERIC;
    }

    *psz_parser++ = '\0';

    /* Init p_access */
    ACCESS_SET_CALLBACKS( Read, NULL, Control, NULL );
    p_sys = p_access->p_sys = calloc( 1, sizeof( access_sys_t ) );
    if( !p_sys )
    {
        free( psz_dup );
        return VLC_ENOMEM;
    }

    p_sys->fd = net_ConnectTCP( p_access, psz_dup, atoi( psz_parser ) );
    free( psz_dup );

    if( p_sys->fd < 0 )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t *)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    net_Close( p_sys->fd );
    free( p_sys );
}

/*****************************************************************************
 * Read: read on a file descriptor
 *****************************************************************************/
static ssize_t Read( access_t *p_access, void *p_buffer, size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_read;

    i_read = vlc_recv_i11e( p_sys->fd, p_buffer, i_len, 0 );
    if( i_read < 0 )
    {
        if( errno != EINTR && errno != EAGAIN )
        {
            msg_Err( p_access, "receive error: %s", vlc_strerror_c(errno) );
            i_read = 0;
        }
    }

    return i_read;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    bool    *pb_bool;
    int64_t *pi_64;

    switch( i_query )
    {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = false;
            break;
        case STREAM_CAN_PAUSE:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = true;    /* FIXME */
            break;
        case STREAM_CAN_CONTROL_PACE:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = true;    /* FIXME */
            break;

        case STREAM_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = INT64_C(1000)
                   * var_InheritInteger( p_access, "network-caching" );
            break;

        case STREAM_SET_PAUSE_STATE:
            /* Nothing to do */
            break;

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
