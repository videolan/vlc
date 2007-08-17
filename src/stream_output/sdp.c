/*****************************************************************************
 * sdp.c : SDP creation helpers
 *****************************************************************************
 * Copyright © 2007 Rémi Denis-Courmont
 * $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <vlc/vlc.h>

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <vlc_network.h>
#include <vlc_charset.h>

#include "stream_output.h"

#define MAXSDPADDRESS 47

static
char *AddressToSDP (const struct sockaddr *addr, socklen_t addrlen, char *buf)
{
    if (addrlen < offsetof (struct sockaddr, sa_family)
                 + sizeof (addr->sa_family))
        return NULL;

    strcpy (buf, "IN IP* ");

    if (vlc_getnameinfo (addr, addrlen, buf + 7, MAXSDPADDRESS - 7, NULL,
                         NI_NUMERICHOST))
        return NULL;

    switch (addr->sa_family)
    {
        case AF_INET:
        {
            if (net_SockAddrIsMulticast (addr, addrlen))
                strcat (buf, "/255"); // obsolete in RFC4566, dummy value
            buf[5] = '4';
            break;
        }

#ifdef AF_INET6
        case AF_INET6:
        {
            char *ptr = strchr (buf, '%');
            if (ptr != NULL)
                *ptr = '\0'; // remove scope ID
            buf[5] = '6';
            break;
        }
#endif

        default:
            return NULL;
    }

    return buf;
}


static vlc_bool_t IsSDPString (const char *str)
{
    if (strchr (str, '\r') != NULL)
        return VLC_FALSE;
    if (strchr (str, '\n') != NULL)
        return VLC_FALSE;
    if (!IsUTF8 (str))
        return VLC_FALSE;
    return VLC_TRUE;
}


char *StartSDP (const char *name, const char *description, const char *url,
                const char *email, const char *phone, vlc_bool_t ssm,
                const struct sockaddr *orig, socklen_t origlen,
                const struct sockaddr *addr, socklen_t addrlen)
{
    uint64_t t = NTPtime64 ();
    char *sdp, machine[MAXSDPADDRESS], conn[MAXSDPADDRESS],
         sfilter[MAXSDPADDRESS + sizeof ("\r\na=source-filter: incl * ")];
    const char *preurl = "\r\nu=", *premail = "\r\ne=", *prephone = "\r\np=";

    if (name == NULL)
        name = "Unnamed";
    if (description == NULL)
        description = "N/A";
    if (url == NULL)
        preurl = url = "";
    if (email == NULL)
        premail = email = "";
    if (phone == NULL)
        prephone = phone = "";

    if (!IsSDPString (name) || !IsSDPString (description)
     || !IsSDPString (url) || !IsSDPString (email) || !IsSDPString (phone)
     || (AddressToSDP (orig, origlen, machine) == NULL)
     || (AddressToSDP (addr, addrlen, conn) == NULL))
        return NULL;

    if (ssm)
        sprintf (sfilter, "\r\na=source-filter: incl IN IP%c * %s",
                 machine[5], machine + 7);
    else
        *sfilter = '\0';

    if (asprintf (&sdp, "v=0"
                    "\r\no=- "I64Fu" "I64Fu" %s"
                    "\r\ns=%s"
                    "\r\ni=%s"
                    "%s%s" // optional URL
                    "%s%s" // optional email
                    "%s%s" // optional phone number
                    "\r\nc=%s"
                        // bandwidth not specified
                    "\r\nt=0 0" // one dummy time span
                        // no repeating
                        // no time zone adjustment (silly idea anyway)
                        // no encryption key (deprecated)
                    "\r\na=tool:"PACKAGE_STRING
                    "\r\na=recvonly"
                    "\r\na=type:broadcast"
                    "\r\na=charset:UTF-8"
                    "%s" // optional source filter
                    "\r\n",
               /* o= */ t, t, machine,
               /* s= */ name,
               /* i= */ description,
               /* u= */ preurl, url,
               /* e= */ premail, email,
               /* p= */ prephone, phone,
               /* c= */ conn,
    /* source-filter */ sfilter) == -1)
        return NULL;
    return sdp;
}


char *vAddSDPMedia (const char *type, int dport, const char *protocol,
                    unsigned pt, const char *rtpmap,
                    const char *fmtpfmt, va_list ap)
{
    char *sdp_media = NULL;

    /* Some default values */
    if (type == NULL)
        type = "video";
    if (dport == 0)
        dport = 9;
    if (protocol == NULL)
        protocol = "RTP/AVP";
    assert (pt < 128u);

    /* RTP payload type map */
    char sdp_rtpmap[rtpmap ? (sizeof ("a=rtpmap:123 *\r\n") + strlen (rtpmap)) : 1];
    if (rtpmap != NULL)
        sprintf (sdp_rtpmap, "a=rtpmap:%u %s\r\n", pt, rtpmap);
    else
        *sdp_rtpmap = '\0';

    /* Format parameters */
    char *fmtp = NULL;
    if ((fmtpfmt != NULL)
     && (vasprintf (&fmtp, fmtpfmt, ap) == -1))
        return NULL;

    char sdp_fmtp[fmtp ? (sizeof ("a=fmtp:123 *\r\n") + strlen (fmtp)) : 1];
    if (fmtp != NULL)
    {
        sprintf (sdp_fmtp, "a=fmtp:%u %s\r\n", pt, fmtp);
        free (fmtp);
    }
    else
        *sdp_fmtp = '\0';

    if (asprintf (&sdp_media, "m=%s %u %s %d\r\n" "b=RR:0\r\n" "%s" "%s",
                  type, dport, protocol, pt,
                  sdp_rtpmap, sdp_fmtp) == -1)
        return NULL;

    return sdp_media;
}


char *AddSDPMedia (const char *type, int dport, const char *protocol,
                    unsigned pt, const char *rtpmap, const char *fmtpfmt, ...)
{
    va_list ap;
    char *ret;
    va_start (ap, fmtpfmt);
    ret = vAddSDPMedia (type, dport, protocol, pt, rtpmap, fmtpfmt, ap);
    va_end (ap);
    return ret;
}
