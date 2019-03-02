/*****************************************************************************
 * udp.c: raw UDP input module
 *****************************************************************************
 * Copyright (C) 2001-2005 VLC authors and VideoLAN
 * Copyright (C) 2007 Remi Denis-Courmont
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Tristan Leteurtre <tooney@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
 *          Remi Denis-Courmont
 *
 * Reviewed: 23 October 2003, Jean-Paul Saman <jpsaman _at_ videolan _dot_ org>
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
#include <vlc_block.h>
#include <vlc_interrupt.h>
#ifdef HAVE_POLL
# include <poll.h>
#endif
#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open( vlc_object_t * );
static void Close( vlc_object_t * );

#define BUFFER_TEXT N_("Receive buffer")
#define BUFFER_LONGTEXT N_("UDP receive buffer size (bytes)" )
#define TIMEOUT_TEXT N_("UDP Source timeout (sec)")

vlc_module_begin ()
    set_shortname( N_("UDP" ) )
    set_description( N_("UDP input") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )

    add_obsolete_integer( "server-port" ) /* since 2.0.0 */
    add_obsolete_integer( "udp-buffer" ) /* since 3.0.0 */
    add_integer( "udp-timeout", -1, TIMEOUT_TEXT, NULL, true )

    set_capability( "access", 0 )
    add_shortcut( "udp", "udpstream", "udp4", "udp6" )

    set_callbacks( Open, Close )
vlc_module_end ()

typedef struct
{
    int fd;
    int timeout;
    size_t mtu;
    block_t *overflow_block;
} access_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static block_t *BlockUDP( stream_t *, bool * );
static int Control( stream_t *, int, va_list );

/*****************************************************************************
 * Open: open the socket
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    stream_t     *p_access = (stream_t*)p_this;
    access_sys_t *sys;

    if( p_access->b_preparsing )
        return VLC_EGENERIC;

    sys = vlc_obj_malloc( p_this, sizeof( *sys ) );
    if( unlikely( sys == NULL ) )
        return VLC_ENOMEM;

    sys->mtu = 7 * 188;

    /* Overflow can be max theoretical datagram content less anticipated MTU,
     *  IPv6 headers are larger than IPv4, ignore IPv6 jumbograms
     */
    sys->overflow_block = block_Alloc(65507 - sys->mtu);
    if( unlikely( sys->overflow_block == NULL ) )
        return VLC_ENOMEM;

    p_access->p_sys = sys;

    /* Set up p_access */
    ACCESS_SET_CALLBACKS( NULL, BlockUDP, Control, NULL );

    char *psz_name = strdup( p_access->psz_location );
    char *psz_parser;
    const char *psz_server_addr, *psz_bind_addr = "";
    int  i_bind_port = 1234, i_server_port = 0;

    if( unlikely(psz_name == NULL) )
        return VLC_ENOMEM;

    /* Parse psz_name syntax :
     * [serveraddr[:serverport]][@[bindaddr]:[bindport]] */
    psz_parser = strchr( psz_name, '@' );
    if( psz_parser != NULL )
    {
        /* Found bind address and/or bind port */
        *psz_parser++ = '\0';
        psz_bind_addr = psz_parser;

        if( psz_bind_addr[0] == '[' )
            /* skips bracket'd IPv6 address */
            psz_parser = strchr( psz_parser, ']' );

        if( psz_parser != NULL )
        {
            psz_parser = strchr( psz_parser, ':' );
            if( psz_parser != NULL )
            {
                *psz_parser++ = '\0';
                i_bind_port = atoi( psz_parser );
            }
        }
    }

    psz_server_addr = psz_name;
    psz_parser = ( psz_server_addr[0] == '[' )
        ? strchr( psz_name, ']' ) /* skips bracket'd IPv6 address */
        : psz_name;

    if( psz_parser != NULL )
    {
        psz_parser = strchr( psz_parser, ':' );
        if( psz_parser != NULL )
        {
            *psz_parser++ = '\0';
            i_server_port = atoi( psz_parser );
        }
    }

    msg_Dbg( p_access, "opening server=%s:%d local=%s:%d",
             psz_server_addr, i_server_port, psz_bind_addr, i_bind_port );

    sys->fd = net_OpenDgram( p_access, psz_bind_addr, i_bind_port,
                             psz_server_addr, i_server_port, IPPROTO_UDP );
    free( psz_name );
    if( sys->fd == -1 )
    {
        msg_Err( p_access, "cannot open socket" );
        return VLC_EGENERIC;
    }

    sys->timeout = var_InheritInteger( p_access, "udp-timeout");
    if( sys->timeout > 0)
        sys->timeout *= 1000;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    stream_t     *p_access = (stream_t*)p_this;
    access_sys_t *sys = p_access->p_sys;
    if( sys->overflow_block )
        block_Release( sys->overflow_block );

    net_Close( sys->fd );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( stream_t *p_access, int i_query, va_list args )
{
    bool    *pb_bool;

    switch( i_query )
    {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            pb_bool = va_arg( args, bool * );
            *pb_bool = false;
            break;

        case STREAM_GET_PTS_DELAY:
            *va_arg( args, vlc_tick_t * ) =
                VLC_TICK_FROM_MS(var_InheritInteger(p_access, "network-caching"));
            break;

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * BlockUDP:
 *****************************************************************************/
static block_t *BlockUDP(stream_t *access, bool *restrict eof)
{
    access_sys_t *sys = access->p_sys;

    block_t *pkt = block_Alloc(sys->mtu);
    if (unlikely(pkt == NULL))
    {   /* OOM - dequeue and discard one packet */
        char dummy;
        recv(sys->fd, &dummy, 1, 0);
        return NULL;
    }


    struct iovec iov[] = {{
        .iov_base = pkt->p_buffer,
        .iov_len = sys->mtu,
    },{
        .iov_base = sys->overflow_block->p_buffer,
        .iov_len = sys->overflow_block->i_buffer,
    }};
    struct msghdr msg = {
        .msg_iov = iov,
        .msg_iovlen = 2,
    };

    struct pollfd ufd[1];

    ufd[0].fd = sys->fd;
    ufd[0].events = POLLIN;

    switch (vlc_poll_i11e(ufd, 1, sys->timeout))
    {
        case 0:
            msg_Err(access, "receive time-out");
            *eof = true;
            /* fall through */
        case -1:
            goto skip;
     }

    ssize_t len = recvmsg(sys->fd, &msg, 0);

    if (len < 0)
    {
skip:
        block_Release(pkt);
        return NULL;
    }

    /* Received more than mtu amount,
     * we should gather blocks and increase mtu
     * and allocate new overflow block.  See Open()
     */
    if (unlikely((size_t)len > sys->mtu))
    {
        msg_Warn(access, "%zd bytes packet received (MTU was %zu), adjusting mtu",
                len, sys->mtu);
        block_t *gather_block = sys->overflow_block;

        sys->overflow_block = block_Alloc(65507 - len);

        gather_block->i_buffer = len - sys->mtu;
        pkt->p_next = gather_block;
        pkt = block_ChainGather( pkt );

        sys->mtu = len;
    }
    else
        pkt->i_buffer = len;

    return pkt;
}
