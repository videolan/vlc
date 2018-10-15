/*****************************************************************************
 * srt.c: SRT (Secure Reliable Transport) input module
 *****************************************************************************
 * Copyright (C) 2017-2018, Collabora Ltd.
 * Copyright (C) 2018, Haivision Systems Inc.
 *
 * Authors: Justin Kim <justin.kim@collabora.com>
 *          Roman Diouskine <rdiouskine@haivision.com>
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
/* Minimum/Maximum chunks to allow reading at a time from libsrt */
#define SRT_MIN_CHUNKS_TRYREAD 10
#define SRT_MAX_CHUNKS_TRYREAD 100
/* The default timeout is -1 (infinite) */
#define SRT_DEFAULT_POLL_TIMEOUT -1
/* The default latency is 125
 * which uses srt library internally */
#define SRT_DEFAULT_LATENCY 125
/* Crypto key length in bytes. */
#define SRT_KEY_LENGTH_TEXT N_("Crypto key length in bytes")
#define SRT_DEFAULT_KEY_LENGTH 16
static const int srt_key_lengths[] = {
    16, 24, 32,
};

static const char *const srt_key_length_names[] = {
    N_("16 bytes"), N_("24 bytes"), N_("32 bytes"),
};

typedef struct
{
    SRTSOCKET   sock;
    int         i_poll_id;
    vlc_mutex_t lock;
    bool        b_interrupted;
    char       *psz_host;
    int         i_port;
    int         i_chunks; /* Number of chunks to allocate in the next read */
} stream_sys_t;

static void srt_wait_interrupted(void *p_data)
{
    stream_t *p_stream = p_data;
    stream_sys_t *p_sys = p_stream->p_sys;

    vlc_mutex_lock( &p_sys->lock );
    if ( p_sys->i_poll_id >= 0 &&  p_sys->sock != SRT_INVALID_SOCK )
    {
        p_sys->b_interrupted = true;

        msg_Dbg( p_stream, "Waking up srt_epoll_wait");

        /* Removing all socket descriptors from the monitoring list
         * wakes up SRT's threads. We only have one to remove. */
        srt_epoll_remove_usock( p_sys->i_poll_id, p_sys->sock );
    }
    vlc_mutex_unlock( &p_sys->lock );
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
            *va_arg( args, vlc_tick_t * ) = VLC_TICK_FROM_MS(
                   var_InheritInteger(p_stream, "network-caching") );
            break;
        default:
            i_ret = VLC_EGENERIC;
            break;
    }

    return i_ret;
}

static bool srt_schedule_reconnect(stream_t *p_stream)
{
    int         i_latency;
    int         stat;
    char        *psz_passphrase = NULL;

    struct addrinfo hints = {
        .ai_socktype = SOCK_DGRAM,
    }, *res = NULL;

    stream_sys_t *p_sys = p_stream->p_sys;
    bool failed = false;

    stat = vlc_getaddrinfo( p_sys->psz_host, p_sys->i_port, &hints, &res );
    if ( stat )
    {
        msg_Err( p_stream, "Cannot resolve [%s]:%d (reason: %s)",
                 p_sys->psz_host,
                 p_sys->i_port,
                 gai_strerror( stat ) );

        failed = true;
        goto out;
    }

    /* Always start with a fresh socket */
    if (p_sys->sock != SRT_INVALID_SOCK)
    {
        srt_epoll_remove_usock( p_sys->i_poll_id, p_sys->sock );
        srt_close( p_sys->sock );
    }

    p_sys->sock = srt_socket( res->ai_family, SOCK_DGRAM, 0 );
    if ( p_sys->sock == SRT_INVALID_SOCK )
    {
        msg_Err( p_stream, "Failed to open socket." );
        failed = true;
        goto out;
    }

    /* Make SRT non-blocking */
    srt_setsockopt( p_sys->sock, 0, SRTO_SNDSYN,
        &(bool) { false }, sizeof( bool ) );
    srt_setsockopt( p_sys->sock, 0, SRTO_RCVSYN,
        &(bool) { false }, sizeof( bool ) );

    /* Make sure TSBPD mode is enable (SRT mode) */
    srt_setsockopt( p_sys->sock, 0, SRTO_TSBPDMODE,
        &(int) { 1 }, sizeof( int ) );

    /* This is an access module so it is always a receiver */
    srt_setsockopt( p_sys->sock, 0, SRTO_SENDER,
        &(int) { 0 }, sizeof( int ) );

    /* Set latency */
    i_latency = var_InheritInteger( p_stream, "latency" );
    srt_setsockopt( p_sys->sock, 0, SRTO_TSBPDDELAY,
        &i_latency, sizeof( int ) );

    psz_passphrase = var_InheritString( p_stream, "passphrase" );
    if ( psz_passphrase != NULL && psz_passphrase[0] != '\0')
    {
        int i_key_length = var_InheritInteger( p_stream, "key-length" );
        srt_setsockopt( p_sys->sock, 0, SRTO_PASSPHRASE,
            psz_passphrase, strlen( psz_passphrase ) );
        srt_setsockopt( p_sys->sock, 0, SRTO_PBKEYLEN,
            &i_key_length, sizeof( int ) );
    }

    srt_epoll_add_usock( p_sys->i_poll_id, p_sys->sock,
        &(int) { SRT_EPOLL_ERR | SRT_EPOLL_IN });

    /* Schedule a connect */
    msg_Dbg( p_stream, "Schedule SRT connect (dest addresss: %s, port: %d).",
        p_sys->psz_host, p_sys->i_port);

    stat = srt_connect( p_sys->sock, res->ai_addr, res->ai_addrlen);
    if ( stat == SRT_ERROR )
    {
        msg_Err( p_stream, "Failed to connect to server (reason: %s)",
                 srt_getlasterror_str() );
        failed = true;
    }

    /* Reset the number of chunks to allocate as the bitrate of
     * the stream may have changed.
     */
    p_sys->i_chunks = SRT_MIN_CHUNKS_TRYREAD;

out:
    if (failed && p_sys->sock != SRT_INVALID_SOCK)
    {
        srt_epoll_remove_usock( p_sys->i_poll_id, p_sys->sock );
        srt_close(p_sys->sock);
        p_sys->sock = SRT_INVALID_SOCK;
    }

    freeaddrinfo( res );
    free( psz_passphrase );

    return !failed;
}

static block_t *BlockSRT(stream_t *p_stream, bool *restrict eof)
{
    stream_sys_t *p_sys = p_stream->p_sys;
    int i_chunk_size = var_InheritInteger( p_stream, "chunk-size" );
    int i_poll_timeout = var_InheritInteger( p_stream, "poll-timeout" );
    /* SRT doesn't have a concept of EOF for live streams. */
    VLC_UNUSED(eof);

    if ( vlc_killed() )
    {
        /* We are told to stop. Stop. */
        return NULL;
    }

    if ( p_sys->i_chunks == 0 )
        p_sys->i_chunks = SRT_MIN_CHUNKS_TRYREAD;

    size_t i_chunk_size_actual = ( i_chunk_size > 0 )
        ? i_chunk_size : SRT_DEFAULT_CHUNK_SIZE;
    size_t bufsize = i_chunk_size_actual * p_sys->i_chunks;
    block_t *pkt = block_Alloc( bufsize );
    if ( unlikely( pkt == NULL ) )
    {
        return NULL;
    }

    vlc_interrupt_register( srt_wait_interrupted, p_stream);

    SRTSOCKET ready[1];
    int readycnt = 1;
    while ( srt_epoll_wait( p_sys->i_poll_id,
        ready, &readycnt, 0, 0,
        i_poll_timeout, NULL, 0, NULL, 0 ) >= 0)
    {
        if ( readycnt < 0  || ready[0] != p_sys->sock )
        {
            /* should never happen, force recovery */
            srt_close(p_sys->sock);
            p_sys->sock = SRT_INVALID_SOCK;
        }

        switch( srt_getsockstate( p_sys->sock ) )
        {
            case SRTS_CONNECTED:
                /* Good to go */
                break;
            case SRTS_BROKEN:
            case SRTS_NONEXIST:
            case SRTS_CLOSED:
                /* Failed. Schedule recovery. */
                if ( !srt_schedule_reconnect( p_stream ) )
                    msg_Err( p_stream, "Failed to schedule connect" );
                /* Fall-through */
            default:
                /* Not ready */
                continue;
        }

        /* Try to get as much data as possible out of the lib, if there
         * is still some left, increase the number of chunks to read so that
         * it will read faster on the next iteration. This way the buffer will
         * grow until it reads fast enough to keep the library empty after
         * each iteration.
         */
        pkt->i_buffer = 0;
        while ( ( bufsize - pkt->i_buffer ) >= i_chunk_size_actual )
        {
            int stat = srt_recvmsg( p_sys->sock,
                (char *)( pkt->p_buffer + pkt->i_buffer ),
                bufsize - pkt->i_buffer );
            if ( stat <= 0 )
            {
                break;
            }
            pkt->i_buffer += (size_t)stat;
        }

        msg_Dbg ( p_stream, "Read %zu bytes out of a max of %zu"
            " (%d chunks of %zu bytes)", pkt->i_buffer,
            p_sys->i_chunks * i_chunk_size_actual, p_sys->i_chunks,
                 i_chunk_size_actual );


        /* Gradually adjust number of chunks we read at a time
        * up to a predefined maximum. The actual number we might
        * settle on depends on stream's bit rate.
        */
        size_t rem = bufsize - pkt->i_buffer;
        if ( rem < i_chunk_size_actual )
        {
            if ( p_sys->i_chunks < SRT_MAX_CHUNKS_TRYREAD )
            {
                p_sys->i_chunks++;
            }
        }

        goto out;
    }

    /* if the poll reports errors for any reason at all,
     * including a timeout, we skip the turn.
     */
    pkt->i_buffer = 0;

out:
    if (pkt->i_buffer == 0) {
      block_Release(pkt);
      pkt = NULL;
    }

    vlc_interrupt_unregister();

    /* Re-add the socket to the poll if we were interrupted */
    vlc_mutex_lock( &p_sys->lock );
    if ( p_sys->b_interrupted )
    {
        srt_epoll_add_usock( p_sys->i_poll_id, p_sys->sock,
            &(int) { SRT_EPOLL_ERR | SRT_EPOLL_IN } );
        p_sys->b_interrupted = false;
    }
    vlc_mutex_unlock( &p_sys->lock );

    return pkt;
}

static int Open(vlc_object_t *p_this)
{
    stream_t     *p_stream = (stream_t*)p_this;
    stream_sys_t *p_sys = NULL;
    vlc_url_t     parsed_url = { 0 };

    p_sys = vlc_obj_calloc( p_this, 1, sizeof( *p_sys ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    srt_startup();

    vlc_mutex_init( &p_sys->lock );

    p_stream->p_sys = p_sys;

    if ( vlc_UrlParse( &parsed_url, p_stream->psz_url ) == -1 )
    {
        msg_Err( p_stream, "Failed to parse input URL (%s)",
            p_stream->psz_url );
        goto failed;
    }

    p_sys->psz_host = vlc_obj_strdup( p_this, parsed_url.psz_host );
    p_sys->i_port = parsed_url.i_port;

    vlc_UrlClean( &parsed_url );

    p_sys->i_poll_id = srt_epoll_create();
    if ( p_sys->i_poll_id == -1 )
    {
        msg_Err( p_stream, "Failed to create poll id for SRT socket." );
        goto failed;
    }

    if ( !srt_schedule_reconnect( p_stream ) )
    {
        msg_Err( p_stream, "Failed to schedule connect");

        goto failed;
    }

    p_stream->pf_block = BlockSRT;
    p_stream->pf_control = Control;

    return VLC_SUCCESS;

failed:
    vlc_mutex_destroy( &p_sys->lock );

    if ( p_sys->sock != -1 ) srt_close( p_sys->sock );
    if ( p_sys->i_poll_id != -1 ) srt_epoll_release( p_sys->i_poll_id );

    return VLC_EGENERIC;
}

static void Close(vlc_object_t *p_this)
{
    stream_t     *p_stream = (stream_t*)p_this;
    stream_sys_t *p_sys = p_stream->p_sys;

    vlc_mutex_destroy( &p_sys->lock );

    srt_epoll_remove_usock( p_sys->i_poll_id, p_sys->sock );
    srt_close( p_sys->sock );
    srt_epoll_release( p_sys->i_poll_id );

    srt_cleanup();
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
    add_password("passphrase", "", N_("Password for stream encryption"), NULL)
    add_integer( "key-length", SRT_DEFAULT_KEY_LENGTH,
            SRT_KEY_LENGTH_TEXT, SRT_KEY_LENGTH_TEXT, false )
        change_integer_list( srt_key_lengths, srt_key_length_names )

    set_capability( "access", 0 )
    add_shortcut( "srt" )

    set_callbacks( Open, Close )
vlc_module_end ()
