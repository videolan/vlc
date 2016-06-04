/*****************************************************************************
 * udp.c: raw UDP input module
 *****************************************************************************
 * Copyright (C) 2001-2005 VLC authors and VideoLAN
 * Copyright (C) 2007 Remi Denis-Courmont
 * $Id$
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
#include <fcntl.h>

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
    add_integer( "udp-buffer", 0x400000, BUFFER_TEXT, BUFFER_LONGTEXT, true )
    add_integer( "udp-timeout", -1, TIMEOUT_TEXT, NULL, true )

    set_capability( "access", 0 )
    add_shortcut( "udp", "udpstream", "udp4", "udp6" )

    set_callbacks( Open, Close )
vlc_module_end ()

struct access_sys_t
{
    int fd;
    int timeout;
    size_t mtu;
    size_t fifo_size;
    block_fifo_t *fifo;
    vlc_sem_t semaphore;
    vlc_thread_t thread;
    bool timeout_reached;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static block_t *BlockUDP( access_t * );
static int Control( access_t *, int, va_list );
static void* ThreadRead( void *data );

/*****************************************************************************
 * Open: open the socket
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *sys;

    if( p_access->b_preparsing )
        return VLC_EGENERIC;

    sys = malloc( sizeof( *sys ) );
    if( unlikely( sys == NULL ) )
        return VLC_ENOMEM;

    p_access->p_sys = sys;

    /* Set up p_access */
    access_InitFields( p_access );
    ACCESS_SET_CALLBACKS( NULL, BlockUDP, Control, NULL );

    char *psz_name = strdup( p_access->psz_location );
    char *psz_parser;
    const char *psz_server_addr, *psz_bind_addr = "";
    int  i_bind_port = 1234, i_server_port = 0;

    if( unlikely(psz_name == NULL) )
        goto error;

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
        goto error;
    }

    /* Revert to blocking I/O */
#ifndef _WIN32
    fcntl(sys->fd, F_SETFL, fcntl(sys->fd, F_GETFL) & ~O_NONBLOCK);
#else
    ioctlsocket(sys->fd, FIONBIO, &(unsigned long){ 0 });
#endif

    /* FIXME: There are no particular reasons to create a FIFO and thread here.
     * Those are just working around bugs in the stream cache. */
    sys->fifo = block_FifoNew();
    if( unlikely( sys->fifo == NULL ) )
    {
        net_Close( sys->fd );
        goto error;
    }

    sys->mtu = 7 * 188;
    sys->fifo_size = var_InheritInteger( p_access, "udp-buffer");
    vlc_sem_init( &sys->semaphore, 0 );

    sys->timeout = var_InheritInteger( p_access, "udp-timeout");
    sys->timeout_reached = false;
    if( sys->timeout > 0)
        sys->timeout *= 1000;

    if( vlc_clone( &sys->thread, ThreadRead, p_access,
                   VLC_THREAD_PRIORITY_INPUT ) )
    {
        vlc_sem_destroy( &sys->semaphore );
        block_FifoRelease( sys->fifo );
        net_Close( sys->fd );
error:
        free( sys );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *sys = p_access->p_sys;

    vlc_cancel( sys->thread );
    vlc_join( sys->thread, NULL );
    vlc_sem_destroy( &sys->semaphore );
    block_FifoRelease( sys->fifo );
    net_Close( sys->fd );
    free( sys );
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
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = false;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = INT64_C(1000)
                   * var_InheritInteger(p_access, "network-caching");
            break;

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * BlockUDP:
 *****************************************************************************/
static block_t *BlockUDP( access_t *p_access )
{
    access_sys_t *sys = p_access->p_sys;
    block_t *block;

    if (p_access->info.b_eof)
        return NULL;

    vlc_sem_wait_i11e(&sys->semaphore);
    vlc_fifo_Lock(sys->fifo);

    block = vlc_fifo_DequeueAllUnlocked(sys->fifo);

    if (unlikely(sys->timeout_reached == true))
        p_access->info.b_eof=true;

    vlc_fifo_Unlock(sys->fifo);

    return block;
}

/*****************************************************************************
 * ThreadRead: Pull packets from socket as soon as possible.
 *****************************************************************************/
static void* ThreadRead( void *data )
{
    access_t *access = data;
    access_sys_t *sys = access->p_sys;

    for(;;)
    {
        block_t *pkt = block_Alloc(sys->mtu);
        if (unlikely(pkt == NULL))
        {   /* OOM - dequeue and discard one packet */
            char dummy;
            recv(sys->fd, &dummy, 1, 0);
            continue;
        }

        struct iovec iov = {
            .iov_base = pkt->p_buffer,
            .iov_len = sys->mtu,
        };
        struct msghdr msg = {
            .msg_iov = &iov,
            .msg_iovlen = 1,
#ifdef __linux__
            .msg_flags = MSG_TRUNC,
#endif
        };
        ssize_t len;

        block_cleanup_push(pkt);
        do
        {
            int poll_return=0;
            struct pollfd ufd[1];
            ufd[0].fd = sys->fd;
            ufd[0].events = POLLIN;

            while ((poll_return = poll(ufd, 1, sys->timeout)) < 0); /* cancellation point */
            if (unlikely( poll_return == 0))
            {
                msg_Err( access, "Timeout on receiving, timeout %d seconds", sys->timeout/1000 );
                vlc_fifo_Lock(sys->fifo);
                sys->timeout_reached=true;
                vlc_fifo_Unlock(sys->fifo);
                vlc_sem_post(&sys->semaphore);
                len=0;
                break;
            }
            len = recvmsg(sys->fd, &msg, 0);
        }
        while (len == -1);
        vlc_cleanup_pop();

#ifdef MSG_TRUNC
        if (msg.msg_flags & MSG_TRUNC)
        {
            msg_Err(access, "%zd bytes packet truncated (MTU was %zu)",
                    len, sys->mtu);
            pkt->i_flags |= BLOCK_FLAG_CORRUPTED;
            sys->mtu = len;
        }
        else
#endif
            pkt->i_buffer = len;

        vlc_fifo_Lock(sys->fifo);
        /* Discard old buffers on overflow */
        while (vlc_fifo_GetBytes(sys->fifo) + len > sys->fifo_size)
        {
            int canc = vlc_savecancel();
            block_Release(vlc_fifo_DequeueUnlocked(sys->fifo));
            vlc_restorecancel(canc);
        }

        vlc_fifo_QueueUnlocked(sys->fifo, pkt);
        vlc_fifo_Unlock(sys->fifo);
        vlc_sem_post(&sys->semaphore);
    }

    return NULL;
}
