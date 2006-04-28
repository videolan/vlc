/*****************************************************************************
 * io.c: network I/O functions
 *****************************************************************************
 * Copyright (C) 2004-2005 the VideoLAN team
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
#include <stdlib.h>
#include <vlc/vlc.h>

#include <errno.h>

#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif
#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include "network.h"

#ifndef INADDR_ANY
#   define INADDR_ANY  0x00000000
#endif
#ifndef INADDR_NONE
#   define INADDR_NONE 0xFFFFFFFF
#endif

int net_Socket( vlc_object_t *p_this, int i_family, int i_socktype,
                int i_protocol )
{
    int fd, i_val;

    fd = socket( i_family, i_socktype, i_protocol );
    if( fd == -1 )
    {
#if defined(WIN32) || defined(UNDER_CE)
        if( WSAGetLastError ( ) != WSAEAFNOSUPPORT )
            msg_Warn( p_this, "cannot create socket (%i)",
                      WSAGetLastError() );
#else
        if( errno != EAFNOSUPPORT )
            msg_Warn( p_this, "cannot create socket (%s)",
                      strerror( errno ) );
#endif
        return -1;
    }

#if defined( WIN32 ) || defined( UNDER_CE )
    {
        unsigned long i_dummy = 1;
        if( ioctlsocket( fd, FIONBIO, &i_dummy ) != 0 )
            msg_Err( p_this, "cannot set socket to non-blocking mode" );
    }
#else
    if( fd >= FD_SETSIZE )
    {
        /* We don't want to overflow select() fd_set */
        msg_Err( p_this, "cannot create socket (too many already in use)" );
        net_Close( fd );
        return -1;
    }

    fcntl( fd, F_SETFD, FD_CLOEXEC );
    i_val = fcntl( fd, F_GETFL, 0 );
    fcntl( fd, F_SETFL, ((i_val != -1) ? i_val : 0) | O_NONBLOCK );
#endif

    i_val = 1;
    setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, (void *)&i_val,
                sizeof( i_val ) );

#ifdef IPV6_V6ONLY
    /*
     * Accepts only IPv6 connections on IPv6 sockets
     * (and open an IPv4 socket later as well if needed).
     * Only Linux and FreeBSD can map IPv4 connections on IPv6 sockets,
     * so this allows for more uniform handling across platforms. Besides,
     * it makes sure that IPv4 addresses will be printed as w.x.y.z rather
     * than ::ffff:w.x.y.z
     */
    if( i_family == AF_INET6 )
        setsockopt( fd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&i_val,
                    sizeof( i_val ) );
#endif

#if defined( WIN32 ) || defined( UNDER_CE )
# ifndef IPV6_PROTECTION_LEVEL
#  define IPV6_PROTECTION_LEVEL 23
# endif
    if( i_family == AF_INET6 )
    {
        i_val = 30 /*PROTECTION_LEVEL_UNRESTRICTED*/;
        setsockopt( fd, IPPROTO_IPV6, IPV6_PROTECTION_LEVEL,
                   (const char*)&i_val, sizeof( i_val ) );
    }
#endif
    return fd;
}


/*****************************************************************************
 * __net_Close:
 *****************************************************************************
 * Close a network handle
 *****************************************************************************/
void net_Close( int fd )
{
#ifdef UNDER_CE
    CloseHandle( (HANDLE)fd );
#elif defined( WIN32 )
    closesocket( fd );
#else
    close( fd );
#endif
}

/*****************************************************************************
 * __net_Read:
 *****************************************************************************
 * Read from a network socket
 * If b_retry is true, then we repeat until we have read the right amount of
 * data
 *****************************************************************************/
int __net_Read( vlc_object_t *p_this, int fd, v_socket_t *p_vs,
                uint8_t *p_data, int i_data, vlc_bool_t b_retry )
{
    struct timeval  timeout;
    fd_set          fds_r, fds_e;
    int             i_recv;
    int             i_total = 0;
    int             i_ret;
    vlc_bool_t      b_die = p_this->b_die;

    while( i_data > 0 )
    {
        do
        {
            if( p_this->b_die != b_die )
            {
                return 0;
            }

            /* Initialize file descriptor set */
            FD_ZERO( &fds_r );
            FD_SET( fd, &fds_r );
            FD_ZERO( &fds_e );
            FD_SET( fd, &fds_e );

            /* We'll wait 0.5 second if nothing happens */
            timeout.tv_sec = 0;
            timeout.tv_usec = 500000;

        } while( (i_ret = select(fd + 1, &fds_r, NULL, &fds_e, &timeout)) == 0
                 || ( i_ret < 0 && errno == EINTR ) );

        if( i_ret < 0 )
        {
#if defined(WIN32) || defined(UNDER_CE)
            msg_Err( p_this, "network select error (%d)", WSAGetLastError() );
#else
            msg_Err( p_this, "network select error (%s)", strerror(errno) );
#endif
            return i_total > 0 ? i_total : -1;
        }

        if( ( i_recv = (p_vs != NULL)
              ? p_vs->pf_recv( p_vs->p_sys, p_data, i_data )
              : recv( fd, p_data, i_data, 0 ) ) < 0 )
        {
#if defined(WIN32) || defined(UNDER_CE)
            if( WSAGetLastError() == WSAEWOULDBLOCK )
            {
                /* only happens with p_vs (SSL) - not really an error */
            }
            else
            /* For udp only */
            /* On win32 recv() will fail if the datagram doesn't fit inside
             * the passed buffer, even though the buffer will be filled with
             * the first part of the datagram. */
            if( WSAGetLastError() == WSAEMSGSIZE )
            {
                msg_Err( p_this, "recv() failed. "
                         "Increase the mtu size (--mtu option)" );
                i_total += i_data;
            }
            else if( WSAGetLastError() == WSAEINTR ) continue;
            else msg_Err( p_this, "recv failed (%i)", WSAGetLastError() );
#else
            /* EAGAIN only happens with p_vs (TLS) and it's not an error */
            if( errno != EAGAIN )
                msg_Err( p_this, "recv failed (%s)", strerror(errno) );
#endif
            return i_total > 0 ? i_total : -1;
        }
        else if( i_recv == 0 )
        {
            /* Connection closed */
            b_retry = VLC_FALSE;
        }

        p_data += i_recv;
        i_data -= i_recv;
        i_total+= i_recv;
        if( !b_retry )
        {
            break;
        }
    }
    return i_total;
}

/*****************************************************************************
 * __net_ReadNonBlock:
 *****************************************************************************
 * Read from a network socket, non blocking mode (with timeout)
 *****************************************************************************/
int __net_ReadNonBlock( vlc_object_t *p_this, int fd, v_socket_t *p_vs,
                        uint8_t *p_data, int i_data, mtime_t i_wait)
{
    struct timeval  timeout;
    fd_set          fds_r, fds_e;
    int             i_recv;
    int             i_ret;

    /* Initialize file descriptor set */
    FD_ZERO( &fds_r );
    FD_SET( fd, &fds_r );
    FD_ZERO( &fds_e );
    FD_SET( fd, &fds_e );

    timeout.tv_sec = 0;
    timeout.tv_usec = i_wait;

    i_ret = select(fd + 1, &fds_r, NULL, &fds_e, &timeout);

    if( i_ret < 0 && errno == EINTR )
    {
        return 0;
    }
    else if( i_ret < 0 )
    {
#if defined(WIN32) || defined(UNDER_CE)
        msg_Err( p_this, "network select error (%d)", WSAGetLastError() );
#else
        msg_Err( p_this, "network select error (%s)", strerror(errno) );
#endif
        return -1;
    }
    else if( i_ret == 0)
    {
        return 0;
    }
    else
    {
#if !defined(UNDER_CE)
        if( fd == 0/*STDIN_FILENO*/ ) i_recv = read( fd, p_data, i_data ); else
#endif
        if( ( i_recv = (p_vs != NULL)
              ? p_vs->pf_recv( p_vs->p_sys, p_data, i_data )
              : recv( fd, p_data, i_data, 0 ) ) < 0 )
        {
#if defined(WIN32) || defined(UNDER_CE)
            /* For udp only */
            /* On win32 recv() will fail if the datagram doesn't fit inside
             * the passed buffer, even though the buffer will be filled with
             * the first part of the datagram. */
            if( WSAGetLastError() == WSAEMSGSIZE )
            {
                msg_Err( p_this, "recv() failed. "
                         "Increase the mtu size (--mtu option)" );
            }
            else msg_Err( p_this, "recv failed (%i)", WSAGetLastError() );
#else
            msg_Err( p_this, "recv failed (%s)", strerror(errno) );
#endif
            return -1;
        }

        return i_recv ? i_recv : -1;  /* !i_recv -> connection closed if tcp */
    }

    /* We will never be here */
    return -1;
}

/*****************************************************************************
 * __net_Select:
 *****************************************************************************
 * Read from several sockets (with timeout). Takes data from the first socket
 * that has some.
 *****************************************************************************/
int __net_Select( vlc_object_t *p_this, int *pi_fd, v_socket_t **pp_vs,
                  int i_fd, uint8_t *p_data, int i_data, mtime_t i_wait )
{
    struct timeval  timeout;
    fd_set          fds_r, fds_e;
    int             i_recv;
    int             i_ret;
    int             i;
    int             i_max_fd = 0;

    /* Initialize file descriptor set */
    FD_ZERO( &fds_r );
    FD_ZERO( &fds_e );

    for( i = 0 ; i < i_fd ; i++)
    {
        if( pi_fd[i] > i_max_fd ) i_max_fd = pi_fd[i];
        FD_SET( pi_fd[i], &fds_r );
        FD_SET( pi_fd[i], &fds_e );
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = i_wait;

    i_ret = select( i_max_fd + 1, &fds_r, NULL, &fds_e, &timeout );

    if( i_ret < 0 && errno == EINTR )
    {
        return 0;
    }
    else if( i_ret < 0 )
    {
        msg_Err( p_this, "network selection error (%s)", strerror(errno) );
        return -1;
    }
    else if( i_ret == 0 )
    {
        return 0;
    }
    else
    {
        for( i = 0 ; i < i_fd ; i++)
        {
            if( FD_ISSET( pi_fd[i], &fds_r ) )
            {
                i_recv = ((pp_vs != NULL) && (pp_vs[i] != NULL))
                         ? pp_vs[i]->pf_recv( pp_vs[i]->p_sys, p_data, i_data )
                         : recv( pi_fd[i], p_data, i_data, 0 );
                if( i_recv < 0 )
                {
#ifdef WIN32
                    /* For udp only */
                    /* On win32 recv() will fail if the datagram doesn't
                     * fit inside the passed buffer, even though the buffer
                     *  will be filled with the first part of the datagram. */
                    if( WSAGetLastError() == WSAEMSGSIZE )
                    {
                        msg_Err( p_this, "recv() failed. "
                             "Increase the mtu size (--mtu option)" );
                    }
                    else msg_Err( p_this, "recv failed (%i)",
                                  WSAGetLastError() );
#else
                    msg_Err( p_this, "recv failed (%s)", strerror(errno) );
#endif
                    return VLC_EGENERIC;
                }

                return i_recv;
            }
        }
    }

    /* We will never be here */
    return -1;
}


/* Write exact amount requested */
int __net_Write( vlc_object_t *p_this, int fd, v_socket_t *p_vs,
                 const uint8_t *p_data, int i_data )
{
    struct timeval  timeout;
    fd_set          fds_w, fds_e;
    int             i_send;
    int             i_total = 0;
    int             i_ret;

    vlc_bool_t      b_die = p_this->b_die;

    while( i_data > 0 )
    {
        do
        {
            if( p_this->b_die != b_die )
            {
                return 0;
            }

            /* Initialize file descriptor set */
            FD_ZERO( &fds_w );
            FD_SET( fd, &fds_w );
            FD_ZERO( &fds_e );
            FD_SET( fd, &fds_e );

            /* We'll wait 0.5 second if nothing happens */
            timeout.tv_sec = 0;
            timeout.tv_usec = 500000;

        } while( (i_ret = select(fd + 1, NULL, &fds_w, &fds_e, &timeout)) == 0
                 || ( i_ret < 0 && errno == EINTR ) );

        if( i_ret < 0 )
        {
#if defined(WIN32) || defined(UNDER_CE)
            msg_Err( p_this, "network selection error (%d)", WSAGetLastError() );
#else
            msg_Err( p_this, "network selection error (%s)", strerror(errno) );
#endif
            return i_total > 0 ? i_total : -1;
        }

        if( ( i_send = (p_vs != NULL)
                       ? p_vs->pf_send( p_vs->p_sys, p_data, i_data )
                       : send( fd, p_data, i_data, 0 ) ) < 0 )
        {
            /* XXX With udp for example, it will issue a message if the host
             * isn't listening */
            /* msg_Err( p_this, "send failed (%s)", strerror(errno) ); */
            return i_total > 0 ? i_total : -1;
        }

        p_data += i_send;
        i_data -= i_send;
        i_total+= i_send;
    }
    return i_total;
}

char *__net_Gets( vlc_object_t *p_this, int fd, v_socket_t *p_vs )
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

int net_Printf( vlc_object_t *p_this, int fd, v_socket_t *p_vs,
                const char *psz_fmt, ... )
{
    int i_ret;
    va_list args;
    va_start( args, psz_fmt );
    i_ret = net_vaPrintf( p_this, fd, p_vs, psz_fmt, args );
    va_end( args );

    return i_ret;
}

int __net_vaPrintf( vlc_object_t *p_this, int fd, v_socket_t *p_vs,
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
