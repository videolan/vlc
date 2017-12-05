/*****************************************************************************
 * srt.c: SRT (Secure Reliable Transport) output module
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
#include <vlc_sout.h>
#include <vlc_block.h>
#include <vlc_network.h>

#include <srt/srt.h>

/* libsrt defines default packet size as 1316 internally
 * so srt module takes same value. */
#define SRT_DEFAULT_CHUNK_SIZE 1316
/* libsrt tutorial uses 9000 as a default binding port */
#define SRT_DEFAULT_PORT 9000
/* The default timeout is -1 (infinite) */
#define SRT_DEFAULT_POLL_TIMEOUT 100
/* The default latency is 125
 * which uses srt library internally */
#define SRT_DEFAULT_LATENCY 125

struct sout_access_out_sys_t
{
    SRTSOCKET     sock;
    int           i_poll_id;
    int           i_poll_timeout;
    int           i_latency;
    size_t        i_chunk_size;
    int           i_pipe_fds[2];
};

static void srt_wait_interrupted(void *p_data)
{
    sout_access_out_t *p_access = p_data;
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    msg_Dbg( p_access, "Waking up srt_epoll_wait");
    if ( write( p_sys->i_pipe_fds[1], &( bool ) { true }, sizeof( bool ) ) < 0 )
    {
        msg_Err( p_access, "Failed to send data to event fd");
    }
}

static ssize_t Write( sout_access_out_t *p_access, block_t *p_buffer )
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    int i_len = 0;

    vlc_interrupt_register( srt_wait_interrupted, p_access);

    while( p_buffer )
    {
        block_t *p_next;

        i_len += p_buffer->i_buffer;

        while( p_buffer->i_buffer )
        {
            size_t i_write = __MIN( p_buffer->i_buffer, p_sys->i_chunk_size );
            SRTSOCKET ready[2];

retry:
            if ( srt_epoll_wait( p_sys->i_poll_id,
                0, 0, ready, &(int){ 2 }, p_sys->i_poll_timeout,
                &(int) { p_sys->i_pipe_fds[0] }, &(int) { 1 }, NULL, 0 ) == -1 )
            {
                /* Assuming that timeout error is normal when SRT socket is connected. */
                if ( srt_getlasterror( NULL ) == SRT_ETIMEOUT &&
                     srt_getsockstate( p_sys->sock ) == SRTS_CONNECTED )
                {
                    srt_clearlasterror();
                    goto retry;
                }

                i_len = VLC_EGENERIC;
                goto out;
            }

            bool cancel = 0;
            int ret = read( p_sys->i_pipe_fds[0], &cancel, sizeof( bool ) );
            if ( ret > 0 && cancel )
            {
                msg_Dbg( p_access, "Cancelled running" );
                i_len = 0;
                goto out;
            }

            if ( srt_sendmsg2( p_sys->sock, (char *)p_buffer->p_buffer, i_write, 0 ) == SRT_ERROR )
                msg_Warn( p_access, "send error: %s", srt_getlasterror_str() );

            p_buffer->p_buffer += i_write;
            p_buffer->i_buffer -= i_write;
        }

        p_next = p_buffer->p_next;
        block_Release( p_buffer );
        p_buffer = p_next;
    }

out:
    vlc_interrupt_unregister();
    if ( i_len <= 0 ) block_ChainRelease( p_buffer );
    return i_len;
}

static int Control( sout_access_out_t *p_access, int i_query, va_list args )
{
    VLC_UNUSED (p_access);

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

    char                    *psz_dst_addr = NULL;
    int                      i_dst_port;
    int                      stat;

    struct addrinfo hints = {
        .ai_socktype = SOCK_DGRAM,
    }, *res = NULL;

    if (var_Create ( p_access, "dst-port", VLC_VAR_INTEGER )
     || var_Create ( p_access, "src-port", VLC_VAR_INTEGER )
     || var_Create ( p_access, "dst-addr", VLC_VAR_STRING )
     || var_Create ( p_access, "src-addr", VLC_VAR_STRING ) )
    {
         msg_Err( p_access, "Valid network information is required." );
        return VLC_ENOMEM;
    }

    if( !( p_sys = malloc ( sizeof( *p_sys ) ) ) )
        return VLC_ENOMEM;

    p_sys->i_chunk_size = var_InheritInteger( p_access, "chunk-size" );
    p_sys->i_poll_timeout = var_InheritInteger( p_access, "poll-timeout" );
    p_sys->i_latency = var_InheritInteger( p_access, "latency" );
    p_sys->i_poll_id = -1;

    p_access->p_sys = p_sys;

    i_dst_port = SRT_DEFAULT_PORT;
    char *psz_parser = psz_dst_addr = strdup( p_access->psz_path );
    if( !psz_dst_addr )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }

    if (psz_parser[0] == '[')
        psz_parser = strchr (psz_parser, ']');

    psz_parser = strchr (psz_parser ? psz_parser : psz_dst_addr, ':');
    if (psz_parser != NULL)
    {
        *psz_parser++ = '\0';
        i_dst_port = atoi (psz_parser);
    }

    msg_Dbg( p_access, "Setting SRT socket (dest addresss: %s, port: %d).",
             psz_dst_addr, i_dst_port );

    stat = vlc_getaddrinfo( psz_dst_addr, i_dst_port, &hints, &res );
    if ( stat )
    {
        msg_Err( p_access, "Cannot resolve [%s]:%d (reason: %s)",
                 psz_dst_addr,
                 i_dst_port,
                 gai_strerror( stat ) );

        goto failed;
    }

    p_sys->sock = srt_socket( res->ai_family, SOCK_DGRAM, 0 );
    if ( p_sys->sock == SRT_ERROR )
    {
        msg_Err( p_access, "Failed to open socket." );
        goto failed;
    }

    /* Make SRT non-blocking */
    srt_setsockopt( p_sys->sock, 0, SRTO_SNDSYN, &(bool) { false }, sizeof( bool ) );

    /* Make sure TSBPD mode is enable (SRT mode) */
    srt_setsockopt( p_sys->sock, 0, SRTO_TSBPDMODE, &(int) { 1 }, sizeof( int ) );

    /* This is an access_out so it is always a sender */
    srt_setsockopt( p_sys->sock, 0, SRTO_SENDER, &(int) { 1 }, sizeof( int ) );

    /* Set latency */
    srt_setsockopt( p_sys->sock, 0, SRTO_TSBPDDELAY, &p_sys->i_latency, sizeof( int ) );

    p_sys->i_poll_id = srt_epoll_create();
    if ( p_sys->i_poll_id == -1 )
    {
        msg_Err( p_access, "Failed to create poll id for SRT socket (reason: %s)",
                 srt_getlasterror_str() );

        goto failed;
    }

    if ( vlc_pipe( p_sys->i_pipe_fds ) != 0 )
    {
        msg_Err( p_access, "Failed to create pipe fds." );
        goto failed;
    }
    fcntl( p_sys->i_pipe_fds[0], F_SETFL, O_NONBLOCK | fcntl( p_sys->i_pipe_fds[0], F_GETFL ) );

    srt_epoll_add_usock( p_sys->i_poll_id, p_sys->sock, &(int) { SRT_EPOLL_OUT });
    srt_setsockopt( p_sys->sock, 0, SRTO_SENDER, &(int) { 1 }, sizeof(int) );

    srt_epoll_add_ssock( p_sys->i_poll_id, p_sys->i_pipe_fds[0], &(int) { SRT_EPOLL_IN } );

    stat = srt_connect( p_sys->sock, res->ai_addr, sizeof (struct sockaddr));
    if ( stat == SRT_ERROR )
    {
        msg_Err( p_access, "Failed to connect to server (reason: %s)",
                 srt_getlasterror_str() );
        goto failed;
    }

    p_access->pf_write = Write;
    p_access->pf_control = Control;

    free( psz_dst_addr );
    freeaddrinfo( res );

    return VLC_SUCCESS;

failed:
    if ( psz_dst_addr != NULL)
        free( psz_dst_addr );

    if ( res != NULL )
        freeaddrinfo( res );

    vlc_close( p_sys->i_pipe_fds[0] );
    vlc_close( p_sys->i_pipe_fds[1] );

    if ( p_sys != NULL )
    {
        if ( p_sys->i_poll_id != -1 ) srt_epoll_release( p_sys->i_poll_id );
        if ( p_sys->sock != -1 ) srt_close( p_sys->sock );

        free( p_sys );
    }

    return VLC_EGENERIC;
}

static void Close( vlc_object_t * p_this )
{
    sout_access_out_t     *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t *p_sys = p_access->p_sys;

    srt_epoll_release( p_sys->i_poll_id );
    srt_close( p_sys->sock );

    vlc_close( p_sys->i_pipe_fds[0] );
    vlc_close( p_sys->i_pipe_fds[1] );

    free( p_sys );
}

/* Module descriptor */
vlc_module_begin()
    set_shortname( N_("SRT") )
    set_description( N_("SRT stream output") )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_ACO )

    add_integer( "chunk-size", SRT_DEFAULT_CHUNK_SIZE,
            N_("SRT chunk size (bytes)"), NULL, true )
    add_integer( "poll-timeout", SRT_DEFAULT_POLL_TIMEOUT,
            N_("Return poll wait after timeout milliseconds (-1 = infinite)"), NULL, true )
    add_integer( "latency", SRT_DEFAULT_LATENCY, N_("SRT latency (ms)"), NULL, true )

    set_capability( "sout access", 0 )
    add_shortcut( "srt" )

    set_callbacks( Open, Close )
vlc_module_end ()
