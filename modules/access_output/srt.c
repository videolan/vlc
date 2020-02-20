/*****************************************************************************
 * srt.c: SRT (Secure Reliable Transport) access_output module
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

#include <srt_common.h>

#include <vlc_interrupt.h>
#include <vlc_fs.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>
#include <vlc_network.h>

typedef struct
{
    SRTSOCKET     sock;
    int           i_poll_id;
    bool          b_interrupted;
    vlc_mutex_t   lock;
} sout_access_out_sys_t;

static void srt_wait_interrupted(void *p_data)
{
    sout_access_out_t *p_access = p_data;
    sout_access_out_sys_t *p_sys = p_access->p_sys;

    vlc_mutex_lock( &p_sys->lock );
    if ( p_sys->i_poll_id >= 0 &&  p_sys->sock != SRT_INVALID_SOCK )
    {
        p_sys->b_interrupted = true;

        msg_Dbg( p_access, "Waking up srt_epoll_wait");

        /* Removing all socket descriptors from the monitoring list
         * wakes up SRT's threads. We only have one to remove. */
        srt_epoll_remove_usock( p_sys->i_poll_id, p_sys->sock );
    }
    vlc_mutex_unlock( &p_sys->lock );
}

static bool srt_schedule_reconnect(sout_access_out_t *p_access)
{
    vlc_object_t *access_obj = (vlc_object_t *) p_access;
    int stat;
    char *psz_dst_addr = NULL;
    int i_dst_port;
    int i_latency=var_InheritInteger( p_access, SRT_PARAM_LATENCY );
    int i_payload_size = var_InheritInteger( p_access, SRT_PARAM_PAYLOAD_SIZE );
    char *psz_passphrase = var_InheritString( p_access, SRT_PARAM_PASSPHRASE );
    bool passphrase_needs_free = true;
    int i_max_bandwidth_limit =
    var_InheritInteger( p_access, SRT_PARAM_BANDWIDTH_OVERHEAD_LIMIT );
    char *url = NULL;
    srt_params_t params;
    struct addrinfo hints = {
        .ai_socktype = SOCK_DGRAM,
    }, *res = NULL;

    sout_access_out_sys_t *p_sys = p_access->p_sys;
    bool failed = false;

    i_dst_port = SRT_DEFAULT_PORT;
    char *psz_parser = psz_dst_addr = strdup( p_access->psz_path );
    if( !psz_dst_addr )
    {
        failed = true;
        goto out;
    }

    if ( psz_parser[0] == '[' )
        psz_parser = strchr( psz_parser, ']' );

    psz_parser = strchr( psz_parser ? psz_parser : psz_dst_addr, ':' );
    if ( psz_parser != NULL )
    {
        *psz_parser++ = '\0';
        i_dst_port = atoi( psz_parser );
    }

    stat = vlc_getaddrinfo( psz_dst_addr, i_dst_port, &hints, &res );
    if ( stat )
    {
        msg_Err( p_access, "Cannot resolve [%s]:%d (reason: %s)",
                 psz_dst_addr,
                 i_dst_port,
                 gai_strerror( stat ) );

        failed = true;
        goto out;
    }

    /* Always start with a fresh socket */
    if ( p_sys->sock != SRT_INVALID_SOCK )
    {
        srt_epoll_remove_usock( p_sys->i_poll_id, p_sys->sock );
        srt_close( p_sys->sock );
    }

    p_sys->sock = srt_socket( res->ai_family, SOCK_DGRAM, 0 );
    if ( p_sys->sock == SRT_INVALID_SOCK )
    {
        msg_Err( p_access, "Failed to open socket." );
        failed = true;
        goto out;
    }

    if (psz_dst_addr) {
        url = strdup( psz_dst_addr );
        if (srt_parse_url( url, &params )) {
            if (params.latency != -1)
                i_latency = params.latency;
            if (params.payload_size != -1)
                i_payload_size = params.payload_size;
            if (params.passphrase != NULL) {
                free( psz_passphrase );
                passphrase_needs_free = false;
                psz_passphrase = (char *) params.passphrase;
            }
        }
    }

    /* Make SRT non-blocking */
    srt_setsockopt( p_sys->sock, 0, SRTO_SNDSYN,
        &(bool) { false }, sizeof( bool ) );
    srt_setsockopt( p_sys->sock, 0, SRTO_RCVSYN,
        &(bool) { false }, sizeof( bool ) );

    /* Make sure TSBPD mode is enable (SRT mode) */
    srt_setsockopt( p_sys->sock, 0, SRTO_TSBPDMODE,
        &(int) { 1 }, sizeof( int ) );

    /* This is an access_out so it is always a sender */
    srt_setsockopt( p_sys->sock, 0, SRTO_SENDER,
        &(int) { 1 }, sizeof( int ) );

    /* Set latency */
    srt_set_socket_option( access_obj, SRT_PARAM_LATENCY, p_sys->sock,
            SRTO_TSBPDDELAY, &i_latency, sizeof(i_latency) );

    /* set passphrase */
    if (psz_passphrase != NULL && psz_passphrase[0] != '\0') {
        int i_key_length = var_InheritInteger( access_obj, SRT_PARAM_KEY_LENGTH );

        srt_set_socket_option( access_obj, SRT_PARAM_KEY_LENGTH, p_sys->sock,
                SRTO_PBKEYLEN, &i_key_length, sizeof(i_key_length) );

        srt_set_socket_option( access_obj, SRT_PARAM_PASSPHRASE, p_sys->sock,
                SRTO_PASSPHRASE, psz_passphrase, strlen(psz_passphrase) );
    }

    /* set maximumu payload size */
    srt_set_socket_option( access_obj, SRT_PARAM_PAYLOAD_SIZE, p_sys->sock,
            SRTO_PAYLOADSIZE, &i_payload_size, sizeof(i_payload_size) );

    /* set maximum bandwidth limit*/
    srt_set_socket_option( access_obj, SRT_PARAM_BANDWIDTH_OVERHEAD_LIMIT,
            p_sys->sock, SRTO_OHEADBW, &i_max_bandwidth_limit,
            sizeof(i_max_bandwidth_limit) );

    srt_setsockopt( p_sys->sock, 0, SRTO_SENDER, &(int) { 1 }, sizeof(int) );

    srt_epoll_add_usock( p_sys->i_poll_id, p_sys->sock,
        &(int) { SRT_EPOLL_ERR | SRT_EPOLL_OUT });

    /* Schedule a connect */
    msg_Dbg( p_access, "Schedule SRT connect (dest addresss: %s, port: %d).",
        psz_dst_addr, i_dst_port );

    stat = srt_connect( p_sys->sock, res->ai_addr, res->ai_addrlen );
    if ( stat == SRT_ERROR )
    {
        msg_Err( p_access, "Failed to connect to server (reason: %s)",
                 srt_getlasterror_str() );
        failed = true;
    }

out:
    if (failed && p_sys->sock != SRT_INVALID_SOCK)
    {
        srt_epoll_remove_usock( p_sys->i_poll_id, p_sys->sock );
        srt_close(p_sys->sock);
        p_sys->sock = SRT_INVALID_SOCK;
    }

    if (passphrase_needs_free)
        free( psz_passphrase );
    free( psz_dst_addr );
    free( url );
    freeaddrinfo( res );

    return !failed;
}

static ssize_t Write( sout_access_out_t *p_access, block_t *p_buffer )
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    int i_len = 0;
    size_t i_chunk_size = var_InheritInteger( p_access, SRT_PARAM_CHUNK_SIZE);
    int i_poll_timeout = var_InheritInteger( p_access, SRT_PARAM_POLL_TIMEOUT );
    bool b_interrupted = false;

    vlc_interrupt_register( srt_wait_interrupted, p_access);

    while( p_buffer )
    {
        block_t *p_next;

        i_len += p_buffer->i_buffer;

        while( p_buffer->i_buffer )
        {
            if ( vlc_killed() )
            {
                /* We are told to stop. Stop. */
                i_len = VLC_EGENERIC;
                goto out;
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
                    if ( !srt_schedule_reconnect( p_access ) )
                        msg_Err( p_access, "Failed to schedule connect");
                    /* Fall-through */
                default:
                    /* Not ready */
                    i_len = VLC_EGENERIC;
                    goto out;
            }

            SRTSOCKET ready[1];
            int readycnt = 1;
            if ( srt_epoll_wait( p_sys->i_poll_id,
                0, 0, &ready[0], &readycnt,
                i_poll_timeout, NULL, 0, NULL, 0 ) < 0)
            {
                if ( vlc_killed() )
                {
                    /* We are told to stop. Stop. */
                    i_len = VLC_EGENERIC;
                    goto out;
                }

                /* if 'srt_epoll_wait' is interrupted, we still need to
                *  finish sending current block or it may be sent only
                *  partially. TODO: this delay can be prevented,
                *  possibly with a FIFO and an additional thread.
                */
                vlc_mutex_lock( &p_sys->lock );
                if ( p_sys->b_interrupted )
                {
                    srt_epoll_add_usock( p_sys->i_poll_id, p_sys->sock,
                        &(int) { SRT_EPOLL_ERR | SRT_EPOLL_OUT });
                    p_sys->b_interrupted = false;
                    b_interrupted = true;
                }
                vlc_mutex_unlock( &p_sys->lock );

                if ( !b_interrupted )
                {
                    continue;
                }
                else if ( (true) )
                {
                    msg_Dbg( p_access, "srt_epoll_wait was interrupted");
                }
            }

            if ( readycnt > 0  && ready[0] == p_sys->sock
                && srt_getsockstate( p_sys->sock ) == SRTS_CONNECTED)
            {
                size_t i_write = __MIN( p_buffer->i_buffer, i_chunk_size );
                if (srt_sendmsg2( p_sys->sock,
                    (char *)p_buffer->p_buffer, i_write, 0 ) == SRT_ERROR )
                {
                    msg_Warn( p_access, "send error: %s", srt_getlasterror_str() );
                    i_len = VLC_EGENERIC;
                    goto out;
                }

                p_buffer->p_buffer += i_write;
                p_buffer->i_buffer -= i_write;
            }
        }

        p_next = p_buffer->p_next;
        block_Release( p_buffer );
        p_buffer = p_next;

        if ( b_interrupted )
        {
            goto out;
        }
    }

out:
    vlc_interrupt_unregister();

    /* Re-add the socket to the poll if we were interrupted */
    vlc_mutex_lock( &p_sys->lock );
    if ( p_sys->b_interrupted )
    {
        srt_epoll_add_usock( p_sys->i_poll_id, p_sys->sock,
            &(int) { SRT_EPOLL_ERR | SRT_EPOLL_OUT } );
        p_sys->b_interrupted = false;
    }
    vlc_mutex_unlock( &p_sys->lock );

    if ( i_len <= 0 ) block_ChainRelease( p_buffer );
    return i_len;
}

static int Control( sout_access_out_t *p_access, int i_query, va_list args )
{
    VLC_UNUSED( p_access );

    int i_ret = VLC_SUCCESS;

    switch( i_query )
    {
        case ACCESS_OUT_CONTROLS_PACE:
            *va_arg( args, bool * ) = false;
            break;

        default:
            i_ret = VLC_EGENERIC;
            break;
    }

    return i_ret;
}

static int Open( vlc_object_t *p_this )
{
    sout_access_out_t       *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t   *p_sys = NULL;

    if (var_Create ( p_access, "dst-port", VLC_VAR_INTEGER )
     || var_Create ( p_access, "src-port", VLC_VAR_INTEGER )
     || var_Create ( p_access, "dst-addr", VLC_VAR_STRING )
     || var_Create ( p_access, "src-addr", VLC_VAR_STRING ) )
    {
         msg_Err( p_access, "Valid network information is required." );
        return VLC_ENOMEM;
    }

    p_sys = vlc_obj_calloc( p_this, 1, sizeof( *p_sys ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    srt_startup();

    vlc_mutex_init( &p_sys->lock );

    p_access->p_sys = p_sys;

    p_sys->i_poll_id = srt_epoll_create();
    if ( p_sys->i_poll_id == -1 )
    {
        msg_Err( p_access, "Failed to create poll id for SRT socket (reason: %s)",
                 srt_getlasterror_str() );

        goto failed;
    }

    if ( !srt_schedule_reconnect( p_access ) )
    {
        msg_Err( p_access, "Failed to schedule connect");

        goto failed;
    }

    p_access->pf_write = Write;
    p_access->pf_control = Control;

    return VLC_SUCCESS;

failed:
    if ( p_sys->sock != -1 ) srt_close( p_sys->sock );
    if ( p_sys->i_poll_id != -1 ) srt_epoll_release( p_sys->i_poll_id );

    return VLC_EGENERIC;
}

static void Close( vlc_object_t * p_this )
{
    sout_access_out_t     *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t *p_sys = p_access->p_sys;

    srt_epoll_remove_usock( p_sys->i_poll_id, p_sys->sock );
    srt_close( p_sys->sock );
    srt_epoll_release( p_sys->i_poll_id );

    srt_cleanup();
}

/* Module descriptor */
vlc_module_begin()
    set_shortname( N_( "SRT" ) )
    set_description( N_( "SRT stream output" ) )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_ACO )

    add_integer( SRT_PARAM_CHUNK_SIZE, SRT_DEFAULT_CHUNK_SIZE,
            N_( "SRT chunk size (bytes)" ), NULL, true )
    add_integer( SRT_PARAM_POLL_TIMEOUT, SRT_DEFAULT_POLL_TIMEOUT,
            N_( "Return poll wait after timeout milliseconds (-1 = infinite)" ),
            NULL, true )
    add_integer( SRT_PARAM_LATENCY, SRT_DEFAULT_LATENCY, N_( "SRT latency (ms)" ),
            NULL, true )
    add_password( SRT_PARAM_PASSPHRASE, "", N_( "Password for stream encryption" ),
            NULL )
    add_integer( SRT_PARAM_PAYLOAD_SIZE, SRT_DEFAULT_PAYLOAD_SIZE,
            N_( "SRT maximum payload size (bytes)" ), NULL, true )
    add_integer( SRT_PARAM_BANDWIDTH_OVERHEAD_LIMIT,
            SRT_DEFAULT_BANDWIDTH_OVERHEAD_LIMIT,
            N_( "SRT maximum bandwidth ceiling (bytes)" ), NULL, true )
    add_integer( SRT_PARAM_KEY_LENGTH, SRT_DEFAULT_KEY_LENGTH, SRT_KEY_LENGTH_TEXT,
            SRT_KEY_LENGTH_TEXT, false )
    change_integer_list( srt_key_lengths, srt_key_length_names )

    set_capability( "sout access", 0 )
    add_shortcut( "srt" )

    set_callbacks( Open, Close )
vlc_module_end ()
