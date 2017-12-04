/*****************************************************************************
 * srt.c: SRT (Secure Reliable Transport) input module
 *****************************************************************************
 * Copyright (C) 2017, Collabora Ltd.
 *
 * Authors: Justin Kim <justin.kim@collabora.com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <fcntl.h>

#include <vlc_common.h>
#include <vlc_interrupt.h>
#include <vlc_fs.h>
#include <vlc_plugin.h>
#include <vlc_access.h>

#include <vlc_network.h>
#include <vlc_url.h>

#include <srt/srt.h>

/* libsrt defines default packet size as 1316 internally
 * so srt module takes same value. */
#define SRT_DEFAULT_CHUNK_SIZE 1316
/* The default timeout is -1 (infinite) */
#define SRT_DEFAULT_POLL_TIMEOUT -1
/* The default latency is 125
 * which uses srt library internally */
#define SRT_DEFAULT_LATENCY 125

struct stream_sys_t
{
    SRTSOCKET   sock;
    int         i_poll_id;
    int         i_poll_timeout;
    int         i_latency;
    size_t      i_chunk_size;
    int         i_pipe_fds[2];
};

static void srt_wait_interrupted(void *p_data)
{
    stream_t *p_stream = p_data;
    stream_sys_t *p_sys = p_stream->p_sys;
    msg_Dbg( p_stream, "Waking up srt_epoll_wait");
    if ( write( p_sys->i_pipe_fds[1], &( bool ) { true }, sizeof( bool ) ) < 0 )
    {
        msg_Err( p_stream, "Failed to send data to pipe");
    }
}

static int Control(stream_t *p_stream, int i_query, va_list args)
{
    int i_ret = VLC_SUCCESS;

    switch( i_query )
    {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            *va_arg( args, bool * ) = false;
            break;
        case STREAM_GET_PTS_DELAY:
            *va_arg( args, int64_t * ) = INT64_C(1000)
                   * var_InheritInteger(p_stream, "network-caching");
            break;
        default:
            i_ret = VLC_EGENERIC;
            break;
    }

    return i_ret;
}

static block_t *BlockSRT(stream_t *p_stream, bool *restrict eof)
{
    stream_sys_t *p_sys = p_stream->p_sys;

    block_t *pkt = block_Alloc( p_sys->i_chunk_size );

    if ( unlikely( pkt == NULL ) )
    {
        return NULL;
    }

    vlc_interrupt_register( srt_wait_interrupted, p_stream);

    SRTSOCKET ready[2];

    if ( srt_epoll_wait( p_sys->i_poll_id,
        ready, &(int) { 2 }, NULL, 0, p_sys->i_poll_timeout,
        &(int) { p_sys->i_pipe_fds[0] }, &(int) { 1 }, NULL, 0 ) == -1 )
    {
        int srt_err = srt_getlasterror( NULL );

        /* Assuming that timeout error is normal when SRT socket is connected. */
        if ( srt_err == SRT_ETIMEOUT && srt_getsockstate( p_sys->sock ) == SRTS_CONNECTED )
        {
            goto skip;
        }

        msg_Err( p_stream, "released poll wait (reason : %s)", srt_getlasterror_str() );
        goto endofstream;
    }

    bool cancel = 0;
    int ret = read( p_sys->i_pipe_fds[0], &cancel, sizeof( bool ) );
    if ( ret > 0 && cancel )
    {
        msg_Dbg( p_stream, "Cancelled running" );
        goto endofstream;
    }

    int stat = srt_recvmsg( p_sys->sock, (char *)pkt->p_buffer, p_sys->i_chunk_size );

    if ( stat == SRT_ERROR )
    {
        msg_Err( p_stream, "failed to recevie SRT packet (reason: %s)", srt_getlasterror_str() );
        goto endofstream;
    }

    pkt->i_buffer = stat;
    vlc_interrupt_unregister();
    return pkt;

endofstream:
    msg_Dbg( p_stream, "EOS");
   *eof = true;
skip:
    block_Release(pkt);
    srt_clearlasterror();
    vlc_interrupt_unregister();

    return NULL;
}

static int Open(vlc_object_t *p_this)
{
    stream_t     *p_stream = (stream_t*)p_this;
    stream_sys_t *p_sys = NULL;
    vlc_url_t     parsed_url = { 0 };
    struct addrinfo hints = {
        .ai_socktype = SOCK_DGRAM,
    }, *res = NULL;
    int stat;

    p_sys = vlc_obj_malloc( p_this, sizeof( *p_sys ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    if ( vlc_UrlParse( &parsed_url, p_stream->psz_url ) == -1 )
    {
        msg_Err( p_stream, "Failed to parse a given URL (%s)", p_stream->psz_url );
        goto failed;
    }

    p_sys->i_chunk_size = var_InheritInteger( p_stream, "chunk-size" );
    p_sys->i_poll_timeout = var_InheritInteger( p_stream, "poll-timeout" );
    p_sys->i_latency = var_InheritInteger( p_stream, "latency" );
    p_sys->i_poll_id = -1;
    p_stream->p_sys = p_sys;
    p_stream->pf_block = BlockSRT;
    p_stream->pf_control = Control;

    stat = vlc_getaddrinfo( parsed_url.psz_host, parsed_url.i_port, &hints, &res );
    if ( stat )
    {
        msg_Err( p_stream, "Cannot resolve [%s]:%d (reason: %s)",
                 parsed_url.psz_host,
                 parsed_url.i_port,
                 gai_strerror( stat ) );

        goto failed;
    }

    p_sys->sock = srt_socket( res->ai_family, SOCK_DGRAM, 0 );
    if ( p_sys->sock == SRT_ERROR )
    {
        msg_Err( p_stream, "Failed to open socket." );
        goto failed;
    }

    /* Make SRT non-blocking */
    srt_setsockopt( p_sys->sock, 0, SRTO_SNDSYN, &(bool) { false }, sizeof( bool ) );

    /* Make sure TSBPD mode is enable (SRT mode) */
    srt_setsockopt( p_sys->sock, 0, SRTO_TSBPDMODE, &(int) { 1 }, sizeof( int ) );

    /* Set latency */
    srt_setsockopt( p_sys->sock, 0, SRTO_TSBPDDELAY, &p_sys->i_latency, sizeof( int ) );

    p_sys->i_poll_id = srt_epoll_create();
    if ( p_sys->i_poll_id == -1 )
    {
        msg_Err( p_stream, "Failed to create poll id for SRT socket." );
        goto failed;
    }

    if ( vlc_pipe( p_sys->i_pipe_fds ) != 0 )
    {
        msg_Err( p_stream, "Failed to create pipe fds." );
        goto failed;
    }

    fcntl( p_sys->i_pipe_fds[0], F_SETFL, O_NONBLOCK | fcntl( p_sys->i_pipe_fds[0], F_GETFL ) );

    srt_epoll_add_usock( p_sys->i_poll_id, p_sys->sock, &(int) { SRT_EPOLL_IN } );
    srt_epoll_add_ssock( p_sys->i_poll_id, p_sys->i_pipe_fds[0], &(int) { SRT_EPOLL_IN } );

    stat = srt_connect( p_sys->sock, res->ai_addr, sizeof (struct sockaddr) );

    if ( stat == SRT_ERROR )
    {
        msg_Err( p_stream, "Failed to connect to server." );
        goto failed;
    }

    vlc_UrlClean( &parsed_url );
    freeaddrinfo( res );

    return VLC_SUCCESS;

failed:

    if ( parsed_url.psz_host != NULL
      && parsed_url.psz_buffer != NULL)
    {
        vlc_UrlClean( &parsed_url );
    }

    if ( res != NULL )
    {
        freeaddrinfo( res );
    }

    vlc_close( p_sys->i_pipe_fds[0] );
    vlc_close( p_sys->i_pipe_fds[1] );

    if ( p_sys->i_poll_id != -1 )
    {
        srt_epoll_release( p_sys->i_poll_id );
        p_sys->i_poll_id = -1;
    }
    srt_close( p_sys->sock );

    return VLC_EGENERIC;
}

static void Close(vlc_object_t *p_this)
{
    stream_t     *p_stream = (stream_t*)p_this;
    stream_sys_t *p_sys = p_stream->p_sys;

    if ( p_sys->i_poll_id != -1 )
    {
        srt_epoll_release( p_sys->i_poll_id );
        p_sys->i_poll_id = -1;
    }
    msg_Dbg( p_stream, "closing server" );
    srt_close( p_sys->sock );

    vlc_close( p_sys->i_pipe_fds[0] );
    vlc_close( p_sys->i_pipe_fds[1] );
}

/* Module descriptor */
vlc_module_begin ()
    set_shortname( N_("SRT") )
    set_description( N_("SRT input") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )

    add_integer( "chunk-size", SRT_DEFAULT_CHUNK_SIZE,
            N_("SRT chunk size (bytes)"), NULL, true )
    add_integer( "poll-timeout", SRT_DEFAULT_POLL_TIMEOUT,
            N_("Return poll wait after timeout milliseconds (-1 = infinite)"), NULL, true )
    add_integer( "latency", SRT_DEFAULT_LATENCY, N_("SRT latency (ms)"), NULL, true )

    set_capability( "access", 0 )
    add_shortcut( "srt" )

    set_callbacks( Open, Close )
vlc_module_end ()
