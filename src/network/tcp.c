/*****************************************************************************
 * tcp.c:
 *****************************************************************************
 * Copyright (C) 2004-2005 VLC authors and VideoLAN
 * Copyright (C) 2005-2006 Rémi Denis-Courmont
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@videolan.org>
 *          Rémi Denis-Courmont
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

#include <vlc_common.h>

#include <errno.h>
#include <assert.h>
#include <limits.h>
#include <unistd.h>
#ifdef HAVE_POLL
# include <poll.h>
#endif

#include <vlc_network.h>
#if defined (_WIN32)
#   undef EINPROGRESS
#   define EINPROGRESS WSAEWOULDBLOCK
#   undef EWOULDBLOCK
#   define EWOULDBLOCK WSAEWOULDBLOCK
#   undef EAGAIN
#   define EAGAIN WSAEWOULDBLOCK
#endif
#include <vlc_interrupt.h>

static int SocksNegotiate( vlc_object_t *, int fd, int i_socks_version,
                           const char *psz_user, const char *psz_passwd );
static int SocksHandshakeTCP( vlc_object_t *,
                              int fd, int i_socks_version,
                              const char *psz_user, const char *psz_passwd,
                              const char *psz_host, int i_port );
extern int net_Socket( vlc_object_t *p_this, int i_family, int i_socktype,
                       int i_protocol );

#undef net_Connect
/*****************************************************************************
 * net_Connect:
 *****************************************************************************
 * Open a network connection.
 * @return socket handler or -1 on error.
 *****************************************************************************/
int net_Connect( vlc_object_t *p_this, const char *psz_host, int i_port,
                 int type, int proto )
{
    const char      *psz_realhost;
    char            *psz_socks;
    int             i_realport, i_handle = -1;

    psz_socks = var_InheritString( p_this, "socks" );
    if( psz_socks != NULL )
    {
        char *psz = strchr( psz_socks, ':' );

        if( psz )
            *psz++ = '\0';

        psz_realhost = psz_socks;
        i_realport = ( psz != NULL ) ? atoi( psz ) : 1080;

        msg_Dbg( p_this, "net: connecting to %s port %d (SOCKS) "
                 "for %s port %d", psz_realhost, i_realport,
                 psz_host, i_port );

        /* We only implement TCP with SOCKS */
        switch( type )
        {
            case 0:
                type = SOCK_STREAM;
            case SOCK_STREAM:
                break;
            default:
                msg_Err( p_this, "Socket type not supported through SOCKS" );
                free( psz_socks );
                return -1;
        }
        switch( proto )
        {
            case 0:
                proto = IPPROTO_TCP;
            case IPPROTO_TCP:
                break;
            default:
                msg_Err( p_this, "Transport not supported through SOCKS" );
                free( psz_socks );
                return -1;
        }
    }
    else
    {
        psz_realhost = psz_host;
        i_realport = i_port;

        msg_Dbg( p_this, "net: connecting to %s port %d", psz_realhost,
                 i_realport );
    }

    struct addrinfo hints = {
        .ai_socktype = type,
        .ai_protocol = proto,
        .ai_flags = AI_NUMERICSERV | AI_IDN,
    }, *res;

    int val = vlc_getaddrinfo_i11e(psz_realhost, i_realport, &hints, &res);
    if (val)
    {
        msg_Err (p_this, "cannot resolve %s port %d : %s", psz_realhost,
                 i_realport, gai_strerror (val));
        free( psz_socks );
        return -1;
    }
    free( psz_socks );

    vlc_tick_t timeout = var_InheritInteger(p_this, "ipv4-timeout")
                      * (CLOCK_FREQ / 1000);

    for (struct addrinfo *ptr = res; ptr != NULL; ptr = ptr->ai_next)
    {
        int fd = net_Socket( p_this, ptr->ai_family,
                             ptr->ai_socktype, ptr->ai_protocol );
        if( fd == -1 )
        {
            msg_Dbg( p_this, "socket error: %s", vlc_strerror_c(net_errno) );
            continue;
        }

        if( connect( fd, ptr->ai_addr, ptr->ai_addrlen ) )
        {
            if( net_errno != EINPROGRESS && errno != EINTR )
            {
                msg_Err( p_this, "connection failed: %s",
                         vlc_strerror_c(net_errno) );
                goto next_ai;
            }

            struct pollfd ufd;
            vlc_tick_t deadline = VLC_TS_INVALID;

            ufd.fd = fd;
            ufd.events = POLLOUT;
            deadline = mdate() + timeout;

            do
            {
                vlc_tick_t now = mdate();

                if (vlc_killed())
                    goto next_ai;

                if (now > deadline)
                    now = deadline;

                val = vlc_poll_i11e(&ufd, 1, (deadline - now) / 1000);
            }
            while (val == -1 && errno == EINTR);

            switch (val)
            {
                 case -1: /* error */
                     msg_Err (p_this, "polling error: %s",
                              vlc_strerror_c(net_errno));
                     goto next_ai;

                 case 0: /* timeout */
                     msg_Warn (p_this, "connection timed out");
                     goto next_ai;
            }

            /* There is NO WAY around checking SO_ERROR.
             * Don't ifdef it out!!! */
            if (getsockopt (fd, SOL_SOCKET, SO_ERROR, &val,
                            &(socklen_t){ sizeof (val) }) || val)
            {
                msg_Err (p_this, "connection failed: %s",
                         vlc_strerror_c(val));
                goto next_ai;
            }
        }

        msg_Dbg( p_this, "connection succeeded (socket = %d)", fd );
        i_handle = fd; /* success! */
        break;

next_ai: /* failure */
        net_Close( fd );
    }

    freeaddrinfo( res );

    if( i_handle == -1 )
        return -1;

    if( psz_socks != NULL )
    {
        /* NOTE: psz_socks already free'd! */
        char *psz_user = var_InheritString( p_this, "socks-user" );
        char *psz_pwd  = var_InheritString( p_this, "socks-pwd" );

        if( SocksHandshakeTCP( p_this, i_handle, 5, psz_user, psz_pwd,
                               psz_host, i_port ) )
        {
            msg_Err( p_this, "SOCKS handshake failed" );
            net_Close( i_handle );
            i_handle = -1;
        }

        free( psz_user );
        free( psz_pwd );
    }

    return i_handle;
}


int net_AcceptSingle (vlc_object_t *obj, int lfd)
{
    int fd = vlc_accept (lfd, NULL, NULL, true);
    if (fd == -1)
    {
        if (net_errno != EAGAIN)
#if (EAGAIN != EWOULDBLOCK)
          if (net_errno != EWOULDBLOCK)
#endif
            msg_Err (obj, "accept failed (from socket %d): %s", lfd,
                     vlc_strerror_c(net_errno));
        return -1;
    }

    msg_Dbg (obj, "accepted socket %d (from socket %d)", fd, lfd);
    setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));
    return fd;
}


#undef net_Accept
/**
 * Accepts an new connection on a set of listening sockets.
 * If there are no pending connections, this function will wait.
 * @note If the thread needs to handle events other than incoming connections,
 * you need to use poll() and net_AcceptSingle() instead.
 *
 * @param p_this VLC object for logging and object kill signal
 * @param pi_fd listening socket set
 * @return -1 on error (may be transient error due to network issues),
 * a new socket descriptor on success.
 */
int net_Accept (vlc_object_t *p_this, int *pi_fd)
{
    assert (pi_fd != NULL);

    unsigned n = 0;
    while (pi_fd[n] != -1)
        n++;

    struct pollfd ufd[n];
    /* Initialize file descriptor set */
    for (unsigned i = 0; i < n; i++)
    {
        ufd[i].fd = pi_fd[i];
        ufd[i].events = POLLIN;
    }

    for (;;)
    {
        while (poll (ufd, n, -1) == -1)
        {
            if (net_errno != EINTR)
            {
                msg_Err (p_this, "poll error: %s", vlc_strerror_c(net_errno));
                return -1;
            }
        }

        for (unsigned i = 0; i < n; i++)
        {
            if (ufd[i].revents == 0)
                continue;

            int sfd = ufd[i].fd;
            int fd = net_AcceptSingle (p_this, sfd);
            if (fd == -1)
                continue;

            /*
             * Move listening socket to the end to let the others in the
             * set a chance next time.
             */
            memmove (pi_fd + i, pi_fd + i + 1, n - (i + 1));
            pi_fd[n - 1] = sfd;
            return fd;
        }
    }
    return -1;
}


/*****************************************************************************
 * SocksNegotiate:
 *****************************************************************************
 * Negotiate authentication with a SOCKS server.
 *****************************************************************************/
static int SocksNegotiate( vlc_object_t *p_obj,
                           int fd, int i_socks_version,
                           const char *psz_socks_user,
                           const char *psz_socks_passwd )
{
    uint8_t buffer[128+2*256];
    int i_len;
    bool b_auth = false;

    if( i_socks_version != 5 )
        return VLC_SUCCESS;

    /* We negotiate authentication */
    buffer[0] = i_socks_version;    /* SOCKS version */
    if( psz_socks_user != NULL && psz_socks_passwd != NULL )
    {
        buffer[1] = 2;                  /* Number of methods */
        buffer[2] = 0x00;               /* - No auth required */
        buffer[3] = 0x02;               /* - USer/Password */
        i_len = 4;
        b_auth = true;
    }
    else
    {
        buffer[1] = 1;                  /* Number of methods */
        buffer[2] = 0x00;               /* - No auth required */
        i_len = 3;
    }

    if( net_Write( p_obj, fd, buffer, i_len ) != i_len )
        return VLC_EGENERIC;
    if( net_Read( p_obj, fd, buffer, 2) != 2 )
        return VLC_EGENERIC;

    msg_Dbg( p_obj, "socks: v=%d method=%x", buffer[0], buffer[1] );

    if( buffer[1] == 0x00 )
    {
        msg_Dbg( p_obj, "socks: no authentication required" );
    }
    else if( buffer[1] == 0x02 )
    {
        if( psz_socks_user == NULL || psz_socks_passwd == NULL )
        {
            msg_Err( p_obj, "socks: server mandates authentication but "
                            "a username and/or password was not supplied" );
            return VLC_EGENERIC;
        }

        int const i_user = strlen( psz_socks_user );
        int const i_pasw = strlen( psz_socks_passwd );

        if( i_user > 255 || i_pasw > 255 )
        {
            msg_Err( p_obj, "socks: rejecting username and/or password due to "
                            "violation of RFC1929 (longer than 255 bytes)" );
            return VLC_EGENERIC;
        }

        msg_Dbg( p_obj, "socks: username/password authentication" );

        buffer[0] = i_socks_version;        /* Version */
        buffer[1] = i_user;                 /* User length */
        memcpy( &buffer[2], psz_socks_user, i_user );
        buffer[2+i_user] = i_pasw;          /* Password length */
        memcpy( &buffer[2+i_user+1], psz_socks_passwd, i_pasw );

        i_len = 3 + i_user + i_pasw;

        if( net_Write( p_obj, fd, buffer, i_len ) != i_len )
            return VLC_EGENERIC;

        if( net_Read( p_obj, fd, buffer, 2 ) != 2 )
            return VLC_EGENERIC;

        msg_Dbg( p_obj, "socks: v=%d status=%x", buffer[0], buffer[1] );
        if( buffer[1] != 0x00 )
        {
            msg_Err( p_obj, "socks: authentication rejected" );
            return VLC_EGENERIC;
        }
    }
    else
    {
        if( b_auth )
            msg_Err( p_obj, "socks: unsupported authentication method %x",
                     buffer[0] );
        else
            msg_Err( p_obj, "socks: authentication needed" );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SocksHandshakeTCP:
 *****************************************************************************
 * Open a TCP connection using a SOCKS server and return a handle (RFC 1928)
 *****************************************************************************/
static int SocksHandshakeTCP( vlc_object_t *p_obj,
                              int fd,
                              int i_socks_version,
                              const char *psz_user, const char *psz_passwd,
                              const char *psz_host, int i_port )
{
    uint8_t buffer[128+2*256];

    if( i_socks_version != 4 && i_socks_version != 5 )
    {
        msg_Warn( p_obj, "invalid socks protocol version %d", i_socks_version );
        i_socks_version = 5;
    }

    if( i_socks_version == 5 &&
        SocksNegotiate( p_obj, fd, i_socks_version,
                        psz_user, psz_passwd ) )
        return VLC_EGENERIC;

    if( i_socks_version == 4 )
    {
        /* v4 only support ipv4 */
        static const struct addrinfo hints = {
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM,
            .ai_protocol = IPPROTO_TCP,
            .ai_flags = AI_IDN,
        };
        struct addrinfo *res;

        if (vlc_getaddrinfo_i11e(psz_host, 0, &hints, &res))
            return VLC_EGENERIC;

        buffer[0] = i_socks_version;
        buffer[1] = 0x01;               /* CONNECT */
        SetWBE( &buffer[2], i_port );   /* Port */
        memcpy (&buffer[4],             /* Address */
                &((struct sockaddr_in *)(res->ai_addr))->sin_addr, 4);
        freeaddrinfo (res);

        buffer[8] = 0;                  /* Empty user id */

        if( net_Write( p_obj, fd, buffer, 9 ) != 9 )
            return VLC_EGENERIC;
        if( net_Read( p_obj, fd, buffer, 8 ) != 8 )
            return VLC_EGENERIC;

        msg_Dbg( p_obj, "socks: v=%d cd=%d",
                 buffer[0], buffer[1] );

        if( buffer[1] != 90 )
            return VLC_EGENERIC;
    }
    else if( i_socks_version == 5 )
    {
        int i_hlen = __MIN(strlen( psz_host ), 255);
        int i_len;

        buffer[0] = i_socks_version;    /* Version */
        buffer[1] = 0x01;               /* Cmd: connect */
        buffer[2] = 0x00;               /* Reserved */
        buffer[3] = 3;                  /* ATYP: for now domainname */

        buffer[4] = i_hlen;
        memcpy( &buffer[5], psz_host, i_hlen );
        SetWBE( &buffer[5+i_hlen], i_port );

        i_len = 5 + i_hlen + 2;


        if( net_Write( p_obj, fd, buffer, i_len ) != i_len )
            return VLC_EGENERIC;

        /* Read the header */
        if( net_Read( p_obj, fd, buffer, 5 ) != 5 )
            return VLC_EGENERIC;

        msg_Dbg( p_obj, "socks: v=%d rep=%d atyp=%d",
                 buffer[0], buffer[1], buffer[3] );

        if( buffer[1] != 0x00 )
        {
            msg_Err( p_obj, "socks: CONNECT request failed" );
            return VLC_EGENERIC;
        }

        /* Read the remaining bytes */
        if( buffer[3] == 0x01 )
            i_len = 4-1 + 2;
        else if( buffer[3] == 0x03 )
            i_len = buffer[4] + 2;
        else if( buffer[3] == 0x04 )
            i_len = 16-1+2;
        else
            return VLC_EGENERIC;

        if( net_Read( p_obj, fd, buffer, i_len ) != i_len )
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

void net_ListenClose( int *pi_fd )
{
    if( pi_fd != NULL )
    {
        int *pi;

        for( pi = pi_fd; *pi != -1; pi++ )
            net_Close( *pi );
        free( pi_fd );
    }
}
