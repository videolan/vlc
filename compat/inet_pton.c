/*****************************************************************************
 * inet_pton.c: POSIX/IETF inet_pton() and inet_ntop() replacements
 *****************************************************************************
 * Copyright © 2011 Rémi Denis-Courmont
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
# include <config.h>
#endif

#include <stdio.h>
#include <errno.h>

#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#elif defined(_WIN32)
# include <winsock2.h>
# include <ws2tcpip.h>
# undef EAFNOSUPPORT
# define EAFNOSUPPORT WSAEAFNOSUPPORT
#endif

int inet_pton (int af, const char *src, void *dst)
{
    unsigned char *b = dst;

    switch (af)
    {
        case AF_INET:
            return sscanf (src, "%hhu.%hhu.%hhu.%hhu",
                           b + 0, b + 1, b + 2, b + 3) == 4;
    }
    errno = EAFNOSUPPORT;
    return -1;
}

const char *inet_ntop (int af, const void *src, char *dst, socklen_t len)
{
    const unsigned char *b = src;

    switch (af)
    {
        case AF_INET:
            if (snprintf (dst, len, "%hhu.%hhu.%hhu.%hhu",
                          b[0], b[1], b[2], b[3]) >= len)
            {
                errno = ENOSPC;
                return NULL;
            }
            return dst;
    }
    errno = EAFNOSUPPORT;
    return NULL;
}
