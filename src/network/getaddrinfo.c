/*****************************************************************************
 * getaddrinfo.c: getaddrinfo/getnameinfo replacement functions
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * Copyright (C) 2002-2007 Rémi Denis-Courmont
 * $Id$
 *
 * Author: Rémi Denis-Courmont <rem # videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_charset.h>

#include <stddef.h> /* size_t */
#include <string.h> /* strlen(), memcpy(), memset(), strchr() */
#include <stdlib.h> /* malloc(), free(), strtoul() */
#include <errno.h>
#include <assert.h>

#include <sys/types.h>
#include <vlc_network.h>

#ifndef AF_UNSPEC
#   define AF_UNSPEC   0
#endif

int vlc_getnameinfo( const struct sockaddr *sa, int salen,
                     char *host, int hostlen, int *portnum, int flags )
{
    char psz_servbuf[6], *psz_serv;
    int i_servlen, i_val;

    flags |= NI_NUMERICSERV;
    if( portnum != NULL )
    {
        psz_serv = psz_servbuf;
        i_servlen = sizeof( psz_servbuf );
    }
    else
    {
        psz_serv = NULL;
        i_servlen = 0;
    }

    i_val = getnameinfo(sa, salen, host, hostlen, psz_serv, i_servlen, flags);

    if( portnum != NULL )
        *portnum = atoi( psz_serv );

    return i_val;
}


/**
 * Resolves a host name to a list of socket addresses (like getaddrinfo()).
 *
 * @param p_this a VLC object
 * @param node host name to resolve (encoded as UTF-8), or NULL
 * @param i_port port number for the socket addresses
 * @param p_hints parameters (see getaddrinfo() manual page)
 * @param res pointer set to the resulting chained list.
 * @return 0 on success, a getaddrinfo() error otherwise.
 * On failure, *res is undefined. On success, it must be freed with
 * freeaddrinfo().
 */
int vlc_getaddrinfo( vlc_object_t *p_this, const char *node,
                     int i_port, const struct addrinfo *p_hints,
                     struct addrinfo **res )
{
    struct addrinfo hints;
    char psz_buf[NI_MAXHOST], psz_service[6];

    /*
     * In VLC, we always use port number as integer rather than strings
     * for historical reasons (and portability).
     */
    if( ( i_port > 65535 ) || ( i_port < 0 ) )
    {
        msg_Err( p_this, "invalid port number %d specified", i_port );
        return EAI_SERVICE;
    }

    /* cannot overflow */
    snprintf( psz_service, 6, "%d", i_port );

    /* Check if we have to force ipv4 or ipv6 */
    memset (&hints, 0, sizeof (hints));
    if (p_hints != NULL)
    {
        const int safe_flags =
            AI_PASSIVE |
            AI_CANONNAME |
            AI_NUMERICHOST |
            AI_NUMERICSERV |
#ifdef AI_ALL
            AI_ALL |
#endif
#ifdef AI_ADDRCONFIG
            AI_ADDRCONFIG |
#endif
#ifdef AI_V4MAPPED
            AI_V4MAPPED |
#endif
            0;

        hints.ai_family = p_hints->ai_family;
        hints.ai_socktype = p_hints->ai_socktype;
        hints.ai_protocol = p_hints->ai_protocol;
        /* Unfortunately, some flags chang the layout of struct addrinfo, so
         * they cannot be copied blindly from p_hints to &hints. Therefore, we
         * only copy flags that we know for sure are "safe".
         */
        hints.ai_flags = p_hints->ai_flags & safe_flags;
    }

    /* We only ever use port *numbers* */
    hints.ai_flags |= AI_NUMERICSERV;

    /*
     * VLC extensions :
     * - accept "" as NULL
     * - ignore square brackets
     */
    if (node != NULL)
    {
        if (node[0] == '[')
        {
            size_t len = strlen (node + 1);
            if ((len <= sizeof (psz_buf)) && (node[len] == ']'))
            {
                assert (len > 0);
                memcpy (psz_buf, node + 1, len - 1);
                psz_buf[len - 1] = '\0';
                node = psz_buf;
            }
        }
        if (node[0] == '\0')
            node = NULL;
    }

    int ret;
    node = ToLocale (node);
#ifdef WIN32
    /*
     * Winsock tries to resolve numerical IPv4 addresses as AAAA
     * and IPv6 addresses as A... There comes the bug-to-bug fix.
     */
    if ((hints.ai_flags & AI_NUMERICHOST) == 0)
    {
        hints.ai_flags |= AI_NUMERICHOST;
        ret = getaddrinfo (node, psz_service, &hints, res);
        if (ret == 0)
            goto out;
        hints.ai_flags &= ~AI_NUMERICHOST;
    }
#endif
#ifdef AI_IDN
    /* Run-time I18n Domain Names support */
    hints.ai_flags |= AI_IDN;
    ret = getaddrinfo (node, psz_service, &hints, res);
    if (ret != EAI_BADFLAGS)
        goto out;
    /* IDN not available: disable and retry without it */
    hints.ai_flags &= ~AI_IDN;
#endif
    ret = getaddrinfo (node, psz_service, &hints, res);

#if defined(AI_IDN) || defined(WIN32)
out:
#endif
    LocaleFree (node);
    return ret;
}
