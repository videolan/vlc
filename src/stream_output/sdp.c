/*****************************************************************************
 * sdp.c : SDP creation helpers
 *****************************************************************************
 * Copyright © 2007 Rémi Denis-Courmont
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

#include <vlc_common.h>

#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <vlc_network.h>
#include <vlc_charset.h>
#include <vlc_memstream.h>

#include "stream_output.h"

#define MAXSDPADDRESS 47

static
char *AddressToSDP (const struct sockaddr *addr, size_t addrlen, char *buf)
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


static bool IsSDPString (const char *str)
{
    if (strchr (str, '\r') != NULL)
        return false;
    if (strchr (str, '\n') != NULL)
        return false;
    if (!IsUTF8 (str))
        return false;
    return true;
}

static void vsdp_AddAttribute(struct vlc_memstream *restrict stream,
                              const char *name, const char *fmt, va_list ap)
{
    if (fmt == NULL)
    {
        vlc_memstream_printf(stream, "a=%s\r\n", name);
        return;
    }
    vlc_memstream_printf(stream, "a=%s:", name);
    vlc_memstream_vprintf(stream, fmt, ap);
    vlc_memstream_puts(stream, "\r\n");
}

void sdp_AddAttribute(struct vlc_memstream *restrict stream, const char *name,
                      const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsdp_AddAttribute(stream, name, fmt, ap);
    va_end(ap);
}

void sdp_AddMedia(struct vlc_memstream *restrict stream,
                  const char *type, const char *proto, int dport,
                  unsigned pt, bool bw_indep, unsigned bw,
                  const char *ptname, unsigned clock, unsigned chans,
                  const char *fmtp)
{
    /* Some default values */
    if (type == NULL)
        type = "video";
    if (proto == NULL)
        proto = "RTP/AVP";
    assert (pt < 128u);

    vlc_memstream_printf(stream, "m=%s %u %s %u\r\n", type, dport, proto, pt);

    if (bw > 0)
        vlc_memstream_printf(stream, "b=%s:%u\r\n",
                             bw_indep ? "TIAS" : "AS", bw);
    vlc_memstream_printf(stream, "b=%s:%u\r\n", "RR", 0);

    /* RTP payload type map */
    if (ptname != NULL)
    {
        vlc_memstream_printf(stream, "a=rtpmap:%u %s/%u", pt, ptname, clock);
        if ((strcmp(type, "audio") == 0) && (chans != 1))
            vlc_memstream_printf(stream, "/%u", chans);
        vlc_memstream_puts(stream, "\r\n");
    }

    /* Format parameters */
    if (fmtp != NULL)
        vlc_memstream_printf(stream, "a=fmtp:%u %s\r\n", pt, fmtp);
}

int vlc_sdp_Start(struct vlc_memstream *restrict stream,
                  vlc_object_t *obj, const char *cfgpref,
                  const struct sockaddr *src, size_t srclen,
                  const struct sockaddr *addr, size_t addrlen)
{
    char connection[MAXSDPADDRESS];
    char *str = NULL;

    size_t cfglen = strlen(cfgpref);
    if (cfglen >= 128)
        return -1;

    char varname[cfglen + sizeof ("description")];
    char *subvar = varname + cfglen;

    strcpy(varname, cfgpref);

    vlc_memstream_open(stream);
    vlc_memstream_puts(stream, "v=0\r\n");

    if (AddressToSDP(addr, addrlen, connection) == NULL)
        goto error;
    {
        const uint_fast64_t now = NTPtime64();
        char hostname[256];

        gethostname(hostname, sizeof (hostname));

        vlc_memstream_printf(stream, "o=- %"PRIu64" %"PRIu64" IN IP%c %s\r\n",
                             now, now, connection[5], hostname);
    }

    strcpy(subvar, "name");
    str = var_GetNonEmptyString(obj, varname);
    if (str != NULL)
    {
        if (!IsSDPString(str))
            goto error;

        vlc_memstream_printf(stream, "s=%s\r\n", str);
        free(str);
    }
    else
        vlc_memstream_printf(stream, "s=%s\r\n", "Unnamed");

    strcpy(subvar, "description");
    str = var_GetNonEmptyString(obj, varname);
    if (str != NULL)
    {
        if (!IsSDPString(str))
            goto error;

        vlc_memstream_printf(stream, "i=%s\r\n", str);
        free(str);
    }
    else
        vlc_memstream_printf(stream, "i=%s\r\n", "N/A");

    strcpy(subvar, "url");
    str = var_GetNonEmptyString(obj, varname);
    if (str != NULL)
    {
        if (!IsSDPString(str))
            goto error;

        vlc_memstream_printf(stream, "u=%s\r\n", str);
        free(str);
    }

    strcpy(subvar, "email");
    str = var_GetNonEmptyString(obj, varname);
    if (str != NULL)
    {
        if (!IsSDPString(str))
            goto error;

        vlc_memstream_printf(stream, "e=%s\r\n", str);
        free(str);
    }

    // no phone (useless)

    vlc_memstream_printf(stream, "c=%s\r\n", connection);
    // bandwidth not specified
    vlc_memstream_puts(stream, "t=0 0\r\n"); // one dummy time span
    // no repeating
    // no time zone adjustment (silly idea anyway)
    // no encryption key (deprecated)

    vlc_memstream_printf(stream, "a=tool:%s\r\n", PACKAGE_STRING);
    vlc_memstream_puts(stream, "a=recvonly\r\n");
    vlc_memstream_puts(stream, "a=type:broadcast\r\n");
    vlc_memstream_puts(stream, "a=charset:UTF-8\r\n");

    if (srclen > 0)
    {
        char machine[MAXSDPADDRESS];

        if (AddressToSDP(src, srclen, machine) != NULL)
            vlc_memstream_printf(stream,
                                 "a=source-filter: incl IN IP%c * %s\r\n",
                                 machine[5], machine + 7);
    }

    strcpy(subvar, "cat");
    str = var_GetNonEmptyString(obj, varname);
    if (str != NULL)
    {
        if (IsSDPString(str))
            goto error;

        vlc_memstream_printf(stream, "a=cat:%s\r\n", str);
        vlc_memstream_printf(stream, "a=x-plgroup:%s\r\n", str);
        free(str);
    }
    return 0;
error:
    free(str);
    if (vlc_memstream_close(stream) == 0)
        free(stream->ptr);
    return -1;
}
