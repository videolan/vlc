/*****************************************************************************
 * sdp.c : SDP creation helpers
 *****************************************************************************
 * Copyright © 2007 Rémi Denis-Courmont
 * $Id$
 *
 * Authors: Rémi Denis-Courmont
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

# include <vlc/vlc.h>

# include <string.h>
# include <vlc_network.h>
# include <vlc_charset.h>

# include "stream_output.h"

# define MAXSDPADDRESS 47

static
char *AddressToSDP (const struct sockaddr *addr, socklen_t addrlen, char *buf)
{
    const char *ttl = NULL;
    strcpy (buf, "IN IP* ");

    switch (addr->sa_family)
    {
        case AF_INET:
        {
            if (net_SockAddrIsMulticast (addr, addrlen))
                ttl = "/255"; // obsolete in RFC4566, dummy value
            buf[5] = '4';
            break;
        }

#ifdef AF_INET6
        case AF_INET6:
            buf[5] = '6';
            break;
#endif

        default:
            return NULL;
    }

    if (vlc_getnameinfo (addr, addrlen, buf + 4, MAXSDPADDRESS - 4, NULL,
                         NI_NUMERICHOST))
        return NULL;

    if (ttl != NULL)
        strcat (buf, ttl);

    return buf;
}


char *StartSDP (const char *name,
                const struct sockaddr *orig, socklen_t origlen,
                const struct sockaddr *addr, socklen_t addrlen)
{
    uint64_t t = NTPtime64 ();
    char *sdp, machine[MAXSDPADDRESS], conn[MAXSDPADDRESS];

    if (strchr (name, '\r') || strchr (name, '\n') || !IsUTF8 (name)
     || (AddressToSDP ((struct sockaddr *)&orig, origlen, machine) == NULL)
     || (AddressToSDP ((struct sockaddr *)&addr, addrlen, conn) == NULL))
        return NULL;

    if (asprintf (&sdp, "v=0\r\n"
                        "o=- "I64Fu" "I64Fu" %s\r\n"
                        "s=%s\r\n"
                        "i=N/A\r\n" // must be there even if useless
                        // no URL, email and phone here */
                        "c=%s\r\n"
                        // bandwidth not specified
                        "t= 0 0" // one dummy time span
                        // no repeating
                        // no time zone adjustment (silly idea anyway)
                        // no encryption key (deprecated)
                        "a=tool:"PACKAGE_STRING"\r\n"
                        "a=recvonly\r\n"
                        "a=type:broadcast\r\n"
                        "a=charset:UTF-8\r\n",
               /* o= */ t, t, machine,
               /* s= */ name,
               /* c= */ conn) == -1)
        return NULL;
    return sdp;
}

