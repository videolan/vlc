/*****************************************************************************
 * io.c: network I/O functions
 *****************************************************************************
 * Copyright (C) 2004-2005 the VideoLAN team
 * Copyright © 2005-2006 Rémi Denis-Courmont
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@videolan.org>
 *          Rémi Denis-Courmont <rem # videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <vlc/vlc.h>

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include <errno.h>
#include <assert.h>

#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif
#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif
#ifdef HAVE_POLL
#   include <poll.h>
#endif

#include "network.h"

#ifndef INADDR_ANY
#   define INADDR_ANY  0x00000000
#endif
#ifndef INADDR_NONE
#   define INADDR_NONE 0xFFFFFFFF
#endif

#if defined(WIN32) || defined(UNDER_CE)
# undef EAFNOSUPPORT
# define EAFNOSUPPORT WSAEAFNOSUPPORT
#endif

extern int rootwrap_bind (int family, int socktype, int protocol,
                          const struct sockaddr *addr, size_t alen);

int net_Socket (vlc_object_t *p_this, int family, int socktype,
                int protocol)
{
    int fd = socket (family, socktype, protocol);
    if (fd == -1)
    {
        if (net_errno != EAFNOSUPPORT)
            msg_Err (p_this, "cannot create socket: %s",
                     net_strerror (net_errno));
        return -1;
    }

#if defined (WIN32) || defined (UNDER_CE)
    ioctlsocket (fd, FIONBIO, &(unsigned long){ 1 });
#else
    fcntl (fd, F_SETFD, FD_CLOEXEC);
    fcntl (fd, F_SETFL, fcntl (fd, F_GETFL, 0) | O_NONBLOCK);
#endif

    setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof (int));

#ifdef IPV6_V6ONLY
    /*
     * Accepts only IPv6 connections on IPv6 sockets
     * (and open an IPv4 socket later as well if needed).
     * Only Linux and FreeBSD can map IPv4 connections on IPv6 sockets,
     * so this allows for more uniform handling across platforms. Besides,
     * it makes sure that IPv4 addresses will be printed as w.x.y.z rather
     * than ::ffff:w.x.y.z
     */
    if (family == AF_INET6)
        setsockopt (fd, IPPROTO_IPV6, IPV6_V6ONLY, &(int){ 1 }, sizeof (int));
#endif

#if defined (WIN32) || defined (UNDER_CE)
# ifndef IPV6_PROTECTION_LEVEL
#  warning Please update your C library headers.
#  define IPV6_PROTECTION_LEVEL 23
#  define PROTECTION_LEVEL_UNRESTRICTED 10
# endif
    if (family == AF_INET6)
        setsockopt (fd, IPPROTO_IPV6, IPV6_PROTECTION_LEVEL,
                    &(int){ PROTECTION_LEVEL_UNRESTRICTED }, sizeof (int));
#endif

    return fd;
}


int *net_Listen (vlc_object_t *p_this, const char *psz_host,
                 int i_port, int family, int socktype, int protocol)
{
    struct addrinfo hints, *res;

    memset (&hints, 0, sizeof( hints ));
    hints.ai_family = family;
    hints.ai_socktype = socktype;
    hints.ai_protocol = protocol;
    hints.ai_flags = AI_PASSIVE;

    msg_Dbg (p_this, "net: listening to %s port %d", psz_host, i_port);

    int i_val = vlc_getaddrinfo (p_this, psz_host, i_port, &hints, &res);
    if (i_val)
    {
        msg_Err (p_this, "Cannot resolve %s port %d : %s", psz_host, i_port,
                 vlc_gai_strerror (i_val));
        return NULL;
    }

    int *sockv = NULL;
    unsigned sockc = 0;

    for (struct addrinfo *ptr = res; ptr != NULL; ptr = ptr->ai_next)
    {
        int fd = net_Socket (p_this, ptr->ai_family, ptr->ai_socktype,
                             ptr->ai_protocol);
        if (fd == -1)
        {
            msg_Dbg (p_this, "socket error: %s", net_strerror (net_errno));
            continue;
        }

        /* Bind the socket */
#if defined (WIN32) || defined (UNDER_CE)
        /*
         * Under Win32 and for multicasting, we bind to INADDR_ANY.
         * This is of course a severe bug, since the socket would logically
         * receive unicast traffic, and multicast traffic of groups subscribed
         * to via other sockets.
         */
        if (net_SockAddrIsMulticast (ptr->ai_addr, ptr->ai_addrlen)
         && (sizeof (struct sockaddr_storage) >= ptr->ai_addrlen))
        {
            // This works for IPv4 too - don't worry!
            struct sockaddr_in6 dumb =
            {
                .sin6_family = ptr->ai_addr->sa_family,
                .sin6_port =  ((struct sockaddr_in *)(ptr->ai_addr))->sin_port
            };

            bind (fd, (struct sockaddr *)&dumb, ptr->ai_addrlen);
        }
        else
#endif
        if (bind (fd, ptr->ai_addr, ptr->ai_addrlen))
        {
            int saved_errno = net_errno;

            net_Close (fd);
#if !defined(WIN32) && !defined(UNDER_CE)
            fd = rootwrap_bind (ptr->ai_family, ptr->ai_socktype,
                                ptr->ai_protocol, ptr->ai_addr,
                                ptr->ai_addrlen);
            if (fd != -1)
            {
                msg_Dbg (p_this, "got socket %d from rootwrap", fd);
            }
            else
#endif
            {
                msg_Err (p_this, "socket bind error (%s)",
                         net_strerror( saved_errno ) );
                continue;
            }
        }

        if (net_SockAddrIsMulticast (ptr->ai_addr, ptr->ai_addrlen))
        {
            if (net_Subscribe (p_this, fd, ptr->ai_addr, ptr->ai_addrlen))
            {
                net_Close (fd);
                continue;
            }
        }

        /* Listen */
        switch (ptr->ai_socktype)
        {
            case SOCK_STREAM:
            case SOCK_RDM:
            case SOCK_SEQPACKET:
                if (listen (fd, INT_MAX))
                {
                    msg_Err (p_this, "socket listen error (%s)",
                            net_strerror (net_errno));
                    net_Close (fd);
                    continue;
                }
        }

        int *nsockv = (int *)realloc (sockv, (sockc + 2) * sizeof (int));
        if (nsockv != NULL)
        {
            nsockv[sockc++] = fd;
            sockv = nsockv;
        }
        else
            net_Close (fd);
    }

    vlc_freeaddrinfo (res);

    if (sockv != NULL)
        sockv[sockc] = -1;

    return sockv;
}


int net_ListenSingle (vlc_object_t *obj, const char *host, int port,
                      int family, int socktype, int protocol)
{
    int *fdv = net_Listen (obj, host, port, family, socktype, protocol);
    if (fdv == NULL)
        return -1;

    for (unsigned i = 1; fdv[i] != -1; i++)
    {
        msg_Warn (obj, "A socket has been dropped!");
        net_Close (fdv[i]);
    }

    int fd = fdv[0];
    assert (fd != -1);

    free (fdv);
    return fd;
}



/*****************************************************************************
 * __net_Close:
 *****************************************************************************
 * Close a network handle
 *****************************************************************************/
void net_Close (int fd)
{
#ifdef UNDER_CE
    CloseHandle ((HANDLE)fd);
#elif defined (WIN32)
    closesocket (fd);
#else
    (void)close (fd);
#endif
}


static ssize_t
net_ReadInner( vlc_object_t *restrict p_this, unsigned fdc, const int *fdv,
               const v_socket_t *const *restrict vsv,
               uint8_t *restrict p_buf, size_t i_buflen,
               int wait_ms, vlc_bool_t waitall )
{
    size_t i_total = 0;

    while (i_buflen > 0)
    {
        unsigned i;
        ssize_t n;
#ifdef HAVE_POLL
        struct pollfd ufd[fdc];
#else
        int maxfd = -1;
        fd_set set;
#endif

        int delay_ms = 500;
        if ((wait_ms != -1) && (wait_ms < 500))
            delay_ms = wait_ms;

        if (p_this->b_die)
        {
            errno = EINTR;
            goto error;
        }

#ifdef HAVE_POLL
        memset (ufd, 0, sizeof (ufd));

        for( i = 0; i < fdc; i++ )
        {
            ufd[i].fd = fdv[i];
            ufd[i].events = POLLIN;
        }

        n = poll( ufd, fdc, delay_ms );
#else
        FD_ZERO (&set);

        for( i = 0; i < fdc; i++ )
        {
#if !defined(WIN32) && !defined(UNDER_CE)
            if( fdv[i] >= FD_SETSIZE )
            {
                /* We don't want to overflow select() fd_set */
                msg_Err( p_this, "select set overflow" );
                return -1;
            }
#endif
            FD_SET( fdv[i], &set );
            if( fdv[i] > maxfd )
                maxfd = fdv[i];
        }

        n = select( maxfd + 1, &set, NULL, NULL,
                    (wait_ms == -1) ? NULL
                                  : &(struct timeval){ 0, delay_ms * 1000 } );
#endif
        if( n == -1 )
            goto error;

        assert ((unsigned)n <= fdc);

        if (n == 0) // timeout
            continue;

        for (i = 0;; i++)
        {
#ifdef HAVE_POLL
            if ((i_total > 0) && (ufd[i].revents & POLLERR))
                return i_total; // error will be dequeued on next run

            if ((ufd[i].revents & POLLIN) == 0)
#else
            if (!FD_ISSET (fdv[i], &set))
                continue;
#endif
            fdc = 1;
            fdv += i;
            vsv += i;
            break;
        }

        if( (*vsv) != NULL )
        {
            n = (*vsv)->pf_recv( (*vsv)->p_sys, p_buf, i_buflen );
        }
        else
        {
#if defined(WIN32) || defined(UNDER_CE)
            n = recv( *fdv, p_buf, i_buflen, 0 );
#else
            n = read( *fdv, p_buf, i_buflen );
#endif
        }

        if( n == -1 )
        {
#if defined(WIN32) || defined(UNDER_CE)
            switch( WSAGetLastError() )
            {
                case WSAEWOULDBLOCK:
                /* only happens with vs != NULL (SSL) - not really an error */
                    continue;

                case WSAEMSGSIZE:
                /* For UDP only */
                /* On Win32, recv() fails if the datagram doesn't fit inside
                 * the passed buffer, even though the buffer will be filled
                 * with the first part of the datagram. */
                    msg_Err( p_this, "Receive error: "
                                     "Increase the mtu size (--mtu option)" );
                    i_total += i_buflen;
                    return i_total;
            }
#else
            if( errno == EAGAIN ) /* spurious wake-up (sucks if fdc > 1) */
                continue;
#endif
            goto error;
        }

        if (n == 0) // EOF
            return i_total;

        i_total += n;
        p_buf += n;
        i_buflen -= n;

        if (!waitall)
            return i_total;

        if (wait_ms != -1)
        {
            wait_ms -= delay_ms;
            if (wait_ms == 0)
                return i_total; // time's up!
        }
    }
    return i_total;

error:
    if( errno != EINTR )
        msg_Err( p_this, "Read error: %s", net_strerror (net_errno) );
    return i_total ? (ssize_t)i_total : -1;
}


/*****************************************************************************
 * __net_Read:
 *****************************************************************************
 * Read from a network socket
 * If b_retry is true, then we repeat until we have read the right amount of
 * data
 *****************************************************************************/
int __net_Read( vlc_object_t *restrict p_this, int fd,
                const v_socket_t *restrict p_vs,
                uint8_t *restrict p_data, int i_data, vlc_bool_t b_retry )
{
    return net_ReadInner( p_this, 1, &(int){ fd },
                          &(const v_socket_t *){ p_vs },
                          p_data, i_data, -1, b_retry );
}


/*****************************************************************************
 * __net_ReadNonBlock:
 *****************************************************************************
 * Read from a network socket, non blocking mode (with timeout)
 *****************************************************************************/
int __net_ReadNonBlock( vlc_object_t *restrict p_this, int fd,
                        const v_socket_t *restrict p_vs,
                        uint8_t *restrict p_data, int i_data, mtime_t i_wait)
{
    return net_ReadInner (p_this, 1, &(int){ fd },
                          &(const v_socket_t *){ p_vs },
                          p_data, i_data, i_wait / 1000, VLC_FALSE);
}


/*****************************************************************************
 * __net_Select:
 *****************************************************************************
 * Read from several sockets (with timeout). Takes data from the first socket
 * that has some.
 *****************************************************************************/
int __net_Select( vlc_object_t *restrict p_this, const int *restrict pi_fd,
                  const v_socket_t *const *restrict pp_vs,
                  int i_fd, uint8_t *restrict p_data, int i_data,
                  mtime_t i_wait )
{
    if( pp_vs == NULL )
    {
        const v_socket_t *vsv[i_fd];
        memset( vsv, 0, sizeof (vsv) );

        return net_ReadInner( p_this, i_fd, pi_fd, vsv, p_data, i_data,
                              i_wait / 1000, VLC_FALSE );
    }

    return net_ReadInner( p_this, i_fd, pi_fd, pp_vs, p_data, i_data,
                          i_wait / 1000, VLC_FALSE );
}


/* Write exact amount requested */
int __net_Write( vlc_object_t *p_this, int fd, const v_socket_t *p_vs,
                 const uint8_t *p_data, int i_data )
{
    size_t i_total = 0;

    while( i_data > 0 )
    {
        if( p_this->b_die )
            return i_total;

#ifdef HAVE_POLL
        struct pollfd ufd[1];
        memset (ufd, 0, sizeof (ufd));
        ufd[0].fd = fd;
        ufd[0].events = POLLOUT;

        int val = poll (ufd, 1, 500);
        if ((val > 0) && (ufd[0].revents & POLLERR) && (i_total > 0))
            return i_total; // error will be dequeued separately on next call
#else
        fd_set set;
        FD_ZERO (&set);

#if !defined(WIN32) && !defined(UNDER_CE)
        if (fd >= FD_SETSIZE)
        {
            /* We don't want to overflow select() fd_set */
            msg_Err (p_this, "select set overflow");
            return -1;
        }
#endif
        FD_SET (fd, &set);

        int val = select (fd + 1, NULL, &set, NULL,
                          &(struct timeval){ 0, 500000 });
#endif
        switch (val)
        {
            case -1:
                if (errno != EINTR)
                {
                    msg_Err (p_this, "Write error: %s",
                             net_strerror (net_errno));
                    return i_total ? (int)i_total : -1;
                }

            case 0:
                continue;
        }

        if (p_vs != NULL)
            val = p_vs->pf_send (p_vs->p_sys, p_data, i_data);
        else
#if defined(WIN32) || defined(UNDER_CE)
            val = send (fd, p_data, i_data, 0);
#else
            val = write (fd, p_data, i_data);
#endif

        if (val == -1)
            return i_total ? (int)i_total : -1;
        if (val == 0)
            return i_total;

        p_data += val;
        i_data -= val;
        i_total += val;
    }

    return i_total;
}

char *__net_Gets( vlc_object_t *p_this, int fd, const v_socket_t *p_vs )
{
    char *psz_line = NULL, *ptr = NULL;
    size_t  i_line = 0, i_max = 0;


    for( ;; )
    {
        if( i_line == i_max )
        {
            i_max += 1024;
            psz_line = realloc( psz_line, i_max );
            ptr = psz_line + i_line;
        }

        if( net_Read( p_this, fd, p_vs, (uint8_t *)ptr, 1, VLC_TRUE ) != 1 )
        {
            if( i_line == 0 )
            {
                free( psz_line );
                return NULL;
            }
            break;
        }

        if ( *ptr == '\n' )
            break;

        i_line++;
        ptr++;
    }

    *ptr-- = '\0';

    if( ( ptr >= psz_line ) && ( *ptr == '\r' ) )
        *ptr = '\0';

    return psz_line;
}

int net_Printf( vlc_object_t *p_this, int fd, const v_socket_t *p_vs,
                const char *psz_fmt, ... )
{
    int i_ret;
    va_list args;
    va_start( args, psz_fmt );
    i_ret = net_vaPrintf( p_this, fd, p_vs, psz_fmt, args );
    va_end( args );

    return i_ret;
}

int __net_vaPrintf( vlc_object_t *p_this, int fd, const v_socket_t *p_vs,
                    const char *psz_fmt, va_list args )
{
    char    *psz;
    int     i_size, i_ret;

    i_size = vasprintf( &psz, psz_fmt, args );
    i_ret = __net_Write( p_this, fd, p_vs, (uint8_t *)psz, i_size ) < i_size
        ? -1 : i_size;
    free( psz );

    return i_ret;
}


/*****************************************************************************
 * inet_pton replacement for obsolete and/or crap operating systems
 *****************************************************************************/
#ifndef HAVE_INET_PTON
int inet_pton(int af, const char *src, void *dst)
{
# ifdef WIN32
    /* As we already know, Microsoft always go its own way, so even if they do
     * provide IPv6, they don't provide the API. */
    struct sockaddr_storage addr;
    int len = sizeof( addr );

    /* Damn it, they didn't even put LPCSTR for the firs parameter!!! */
#ifdef UNICODE
    wchar_t *workaround_for_ill_designed_api =
        malloc( MAX_PATH * sizeof(wchar_t) );
    mbstowcs( workaround_for_ill_designed_api, src, MAX_PATH );
    workaround_for_ill_designed_api[MAX_PATH-1] = 0;
#else
    char *workaround_for_ill_designed_api = strdup( src );
#endif

    if( !WSAStringToAddress( workaround_for_ill_designed_api, af, NULL,
                             (LPSOCKADDR)&addr, &len ) )
    {
        free( workaround_for_ill_designed_api );
        return -1;
    }
    free( workaround_for_ill_designed_api );

    switch( af )
    {
        case AF_INET6:
            memcpy( dst, &((struct sockaddr_in6 *)&addr)->sin6_addr, 16 );
            break;

        case AF_INET:
            memcpy( dst, &((struct sockaddr_in *)&addr)->sin_addr, 4 );
            break;

        default:
            WSASetLastError( WSAEAFNOSUPPORT );
            return -1;
    }
# else
    /* Assume IPv6 is not supported. */
    /* Would be safer and more simpler to use inet_aton() but it is most
     * likely not provided either. */
    uint32_t ipv4;

    if( af != AF_INET )
    {
        errno = EAFNOSUPPORT;
        return -1;
    }

    ipv4 = inet_addr( src );
    if( ipv4 == INADDR_NONE )
        return -1;

    memcpy( dst, &ipv4, 4 );
# endif /* WIN32 */
    return 0;
}
#endif /* HAVE_INET_PTON */
