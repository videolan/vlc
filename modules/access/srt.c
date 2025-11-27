/*****************************************************************************
 * srt.c: SRT (Secure Reliable Transport) access module
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
#include "srt_common.h"

#include <vlc_fs.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_interrupt.h>

#include <vlc_network.h>
#include <vlc_url.h>



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
    vlc_object_t *strm_obj = VLC_OBJECT(p_stream);
    int i_latency=var_InheritInteger( p_stream, SRT_PARAM_LATENCY );
    int stat;
    char *psz_passphrase = var_InheritString( p_stream, SRT_PARAM_PASSPHRASE );
    bool passphrase_needs_free = true;
    char *psz_streamid = var_InheritString( p_stream, SRT_PARAM_STREAMID );
    bool streamid_needs_free = true;
    char *url = NULL;
    srt_params_t params = {
        .latency = -1,
        .passphrase = NULL,
        .key_length = SRT_DEFAULT_KEY_LENGTH,
        .payload_size = SRT_DEFAULT_PAYLOAD_SIZE,
        .bandwidth_overhead_limit = SRT_DEFAULT_BANDWIDTH_OVERHEAD_LIMIT,
        .streamid = NULL,
        .mode = SRT_DEFAULT_MODE, /* default = caller */
    };
    struct addrinfo hints = {
        .ai_socktype = SOCK_DGRAM,
    }, *res = NULL;

    stream_sys_t *p_sys = p_stream->p_sys;
    bool failed = false;

    /* Parse URL */
    if (p_stream->psz_url) {
        url = strdup( p_stream->psz_url );
        if ( !url ) {
            failed = true;
            goto out;
        }

        if (srt_parse_url( url, &params )) {
            if (params.latency != -1)
                i_latency = params.latency;
            if (params.passphrase != NULL) {
                free(psz_passphrase);
                passphrase_needs_free = false;
                psz_passphrase = (char *) params.passphrase;
            }
            if (params.streamid != NULL) {
                free(psz_streamid);
                streamid_needs_free = false;
                psz_streamid = (char *) params.streamid;
            }
        }
    }

    if (params.mode != SRT_MODE_LISTENER)
    {
        stat = vlc_getaddrinfo( p_sys->psz_host, p_sys->i_port, &hints, &res);
        if ( stat )
        {
            msg_Err( p_stream, "Cannot resolve [%s]:%d (reason: %s)",
                    p_sys->psz_host,
                    p_sys->i_port,
                    gai_strerror( stat ) );

            failed = true;
            goto out;
        }
    }

    /* Always start with a fresh socket */
    if (p_sys->sock != SRT_INVALID_SOCK)
    {
        srt_epoll_remove_usock( p_sys->i_poll_id, p_sys->sock );
        srt_close( p_sys->sock );
    }
    
    p_sys->sock = srt_create_socket( );
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
    srt_set_socket_option( strm_obj, SRT_PARAM_LATENCY, p_sys->sock,
            SRTO_LATENCY, &i_latency, sizeof(i_latency) );

    /* set passphrase */
    if (psz_passphrase != NULL && psz_passphrase[0] != '\0') {
        int i_key_length = var_InheritInteger( p_stream, SRT_PARAM_KEY_LENGTH );

        srt_set_socket_option( strm_obj, SRT_PARAM_KEY_LENGTH, p_sys->sock,
                SRTO_PBKEYLEN, &i_key_length, sizeof(i_key_length) );

        srt_set_socket_option( strm_obj, SRT_PARAM_PASSPHRASE, p_sys->sock,
                SRTO_PASSPHRASE, psz_passphrase, strlen(psz_passphrase) );
    }

    /* set stream id */
    if (psz_streamid != NULL && psz_streamid[0] != '\0') {
        srt_set_socket_option( strm_obj, SRT_PARAM_STREAMID, p_sys->sock,
                SRTO_STREAMID, psz_streamid, strlen(psz_streamid) );
    }

    msg_Dbg(p_stream, "SRT mode=%d host='%s' port=%d", params.mode,
            p_sys->psz_host ? p_sys->psz_host : "(null)", p_sys->i_port);

    if (params.mode == SRT_MODE_CALLER)
    {
        /* Schedule a connect */
        msg_Dbg( p_stream, "Schedule SRT connect (dest address: %s, port: %d).",
            p_sys->psz_host, p_sys->i_port);
    
        stat = srt_connect( p_sys->sock, res->ai_addr, res->ai_addrlen );
        if (stat == SRT_ERROR) {
            msg_Err( p_stream, "Failed to connect to server (reason: %s)",
                    srt_getlasterror_str() );
            failed = true;
            goto out;
        }

        srt_epoll_add_usock( p_sys->i_poll_id, p_sys->sock,
            &(int) { SRT_EPOLL_ERR | SRT_EPOLL_IN });
    }
    else if (params.mode == SRT_MODE_LISTENER)
    {
        msg_Dbg(p_stream, "Binding for SRT listener.");

        struct addrinfo hints = {
            .ai_family   = AF_UNSPEC,
            .ai_socktype = SOCK_DGRAM,
            .ai_flags    = AI_PASSIVE
        }, *res_local = NULL;

        /* Use the specific NIC or NULL for wildcard */
        char *node = NULL;
        if (p_sys->psz_host && *p_sys->psz_host)
            node = p_sys->psz_host;
        
        stat = vlc_getaddrinfo(node, p_sys->i_port, &hints, &res_local);
        if (stat) {
            msg_Err(p_stream, "Cannot resolve local address (reason: %s)", gai_strerror(stat));
            failed = true; goto out;
        }

        /* If binding IPv6 configure the socket */
        if (res_local->ai_family == AF_INET6) {
            srt_setsockopt(p_sys->sock, 0, SRTO_IPV6ONLY, &(int) { 0 }, sizeof(int));
        }

        srt_setsockopt(p_sys->sock, 0, SRTO_REUSEADDR, &(int) { 1 }, sizeof(int));

        stat = srt_bind(p_sys->sock, res_local->ai_addr, res_local->ai_addrlen);
        if ( stat ) {
            msg_Err(p_stream, "Failed to bind socket (reason: %s)", srt_getlasterror_str());
            freeaddrinfo(res_local);
            failed = true;
            goto out;
        }

        freeaddrinfo(res_local);

        stat = srt_listen(p_sys->sock, 1);
        if ( stat ) {
            msg_Err(p_stream, "Failed to listen on socket (reason: %s)", srt_getlasterror_str());
            failed = true;
            goto out;
        }

        if (srt_epoll_add_usock(p_sys->i_poll_id, p_sys->sock,
            &(int) { SRT_EPOLL_ERR | SRT_EPOLL_IN }) < 0) {
            msg_Err(p_stream, "epoll add failed: %s", srt_getlasterror_str());
            failed = true;
            goto out;
        }

        /* Try to accept a caller (non-blocking). */
        SRTSOCKET accepted = srt_accept(p_sys->sock, NULL, NULL);
        if (accepted == SRT_INVALID_SOCK) {
            int serr;
            srt_getlasterror(&serr);
            if (serr != SRT_EASYNCRCV && serr != SRT_EASYNCSND && serr != SRT_ETIMEOUT) {
                msg_Warn(p_stream, "srt_accept() temporary error: %s", srt_getlasterror_str());
            }
        } else {
            srt_epoll_remove_usock(p_sys->i_poll_id, p_sys->sock);
            srt_close(p_sys->sock);
            p_sys->sock = accepted;
            srt_epoll_add_usock(p_sys->i_poll_id, p_sys->sock,
                &(int) { SRT_EPOLL_ERR | SRT_EPOLL_IN });
        }
    }
    else
    {
        msg_Err(p_stream, "Unknown SRT mode %d", params.mode);
        failed = true;
        goto out;
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

    if (passphrase_needs_free)
        free( psz_passphrase );
    if (streamid_needs_free)
        free( psz_streamid );
    if (res)
        freeaddrinfo( res );
    free( url );

    return !failed;
}

static block_t *BlockSRT(stream_t *p_stream, bool *restrict eof)
{
    stream_sys_t *p_sys = p_stream->p_sys;
    int i_poll_timeout = var_InheritInteger( p_stream, SRT_PARAM_POLL_TIMEOUT );
    /* SRT doesn't have a concept of EOF for live streams. */
    VLC_UNUSED(eof);

    if ( vlc_killed() )
    {
        /* We are told to stop. Stop. */
        return NULL;
    }

    if ( p_sys->i_chunks == 0 )
        p_sys->i_chunks = SRT_MIN_CHUNKS_TRYREAD;

    const size_t i_chunk_size = SRT_LIVE_MAX_PLSIZE;
    const size_t bufsize = i_chunk_size * p_sys->i_chunks;
    block_t *pkt = block_Alloc( bufsize );
    if ( unlikely( pkt == NULL ) )
    {
        return NULL;
    }

    vlc_interrupt_register( srt_wait_interrupted, p_stream);

    SRTSOCKET ready[1];
    int readycnt = 1;
    while ( (readycnt = 1,
             srt_epoll_wait(p_sys->i_poll_id, ready, &readycnt,
                            0, 0, i_poll_timeout, NULL, 0, NULL, 0)) >= 0)
    {
        if ( readycnt < 0  || ready[0] != p_sys->sock )
        {
            /* should never happen, force recovery */
            srt_close(p_sys->sock);
            p_sys->sock = SRT_INVALID_SOCK;
        }

        switch( srt_getsockstate( p_sys->sock ) )
        {
            case SRTS_LISTENING: {
                /* Try to accept a caller (non-blocking). */
                SRTSOCKET accepted = srt_accept(p_sys->sock, NULL, NULL);
                if (accepted == SRT_INVALID_SOCK) {
                    int serr; srt_getlasterror(&serr);
                    /* No pending connection yet: keep waiting. */
                    continue;
                }
                /* Swap listening socket -> connected socket */
                srt_epoll_remove_usock(p_sys->i_poll_id, p_sys->sock);
                srt_close(p_sys->sock);
                p_sys->sock = accepted;
                srt_epoll_add_usock(p_sys->i_poll_id, p_sys->sock,
                    &(int){ SRT_EPOLL_ERR | SRT_EPOLL_IN });
                /* Now loop back; next iteration will see CONNECTED. */
                continue;
            }
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
        while ( ( bufsize - pkt->i_buffer ) >= i_chunk_size )
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

#if 0
        msg_Dbg ( p_stream, "Read %zu bytes out of a max of %zu"
            " (%d chunks of %zu bytes)", pkt->i_buffer,
            p_sys->i_chunks * i_chunk_size, p_sys->i_chunks,
                i_chunk_size );
#endif

        /* Gradually adjust number of chunks we read at a time
        * up to a predefined maximum. The actual number we might
        * settle on depends on stream's bit rate.
        */
        size_t rem = bufsize - pkt->i_buffer;
        if ( rem < i_chunk_size )
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
    vlc_url_t     parsed_url;

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
        vlc_UrlClean( &parsed_url );
        goto failed;
    }

    p_sys->psz_host = vlc_obj_strdup( p_this, parsed_url.psz_host );

    if ( parsed_url.i_port != 0 )
        p_sys->i_port = parsed_url.i_port;
    else
        p_sys->i_port = SRT_DEFAULT_PORT;

    vlc_UrlClean( &parsed_url );

    p_sys->i_poll_id = srt_epoll_create();
    if ( p_sys->i_poll_id == -1 )
    {
        msg_Err( p_stream, "Failed to create poll id for SRT socket." );
        goto failed;
    }
    p_sys->sock = SRT_INVALID_SOCK;

    if ( !srt_schedule_reconnect( p_stream ) )
    {
        msg_Err( p_stream, "Failed to schedule connect");

        goto failed;
    }

    p_stream->pf_block = BlockSRT;
    p_stream->pf_control = Control;

    return VLC_SUCCESS;

failed:
    if ( p_sys->sock != SRT_INVALID_SOCK ) srt_close( p_sys->sock );
    if ( p_sys->i_poll_id != -1 ) srt_epoll_release( p_sys->i_poll_id );
    srt_cleanup();

    return VLC_EGENERIC;
}

static void Close(vlc_object_t *p_this)
{
    stream_t     *p_stream = (stream_t*)p_this;
    stream_sys_t *p_sys = p_stream->p_sys;

    srt_epoll_remove_usock( p_sys->i_poll_id, p_sys->sock );
    srt_close( p_sys->sock );
    srt_epoll_release( p_sys->i_poll_id );

    srt_cleanup();
}

/* Module descriptor */
vlc_module_begin ()
    set_shortname( N_( "SRT" ) )
    set_description( N_( "SRT input" ) )
    set_subcategory( SUBCAT_INPUT_ACCESS )

    add_obsolete_integer( SRT_PARAM_CHUNK_SIZE )
    add_integer( SRT_PARAM_POLL_TIMEOUT, SRT_DEFAULT_POLL_TIMEOUT,
            N_( "Return poll wait after timeout milliseconds (-1 = infinite)" ),
            NULL )
    add_integer( SRT_PARAM_LATENCY, SRT_DEFAULT_LATENCY,
            N_( "SRT latency (ms)" ), NULL )
    add_password( SRT_PARAM_PASSPHRASE, "",
            N_( "Password for stream encryption" ), NULL )
    add_obsolete_integer( SRT_PARAM_PAYLOAD_SIZE )
    add_integer( SRT_PARAM_KEY_LENGTH, SRT_DEFAULT_KEY_LENGTH,
            SRT_KEY_LENGTH_TEXT, NULL )
    change_integer_list( srt_key_lengths, srt_key_length_names )
    add_string(SRT_PARAM_STREAMID, "",
            N_( "SRT Stream ID"), NULL)
    add_integer( SRT_PARAM_MODE, SRT_DEFAULT_MODE,
            SRT_MODE_TEXT, NULL )
    change_integer_list( srt_mode_values, srt_mode_names )
    change_safe()

    set_capability("access", 0)
    add_shortcut("srt")

    set_callbacks(Open, Close)
vlc_module_end ()
