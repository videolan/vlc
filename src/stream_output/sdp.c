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


static
char *sdp_Start (const char *name, const char *description, const char *url,
                 const char *email, const char *phone,
                 const struct sockaddr *src, size_t srclen,
                 const struct sockaddr *addr, size_t addrlen)
{
    uint64_t now = NTPtime64 ();
    char *sdp;
    char connection[MAXSDPADDRESS], hostname[256],
         sfilter[MAXSDPADDRESS + sizeof ("\r\na=source-filter: incl * ")];
    const char *preurl = "\r\nu=", *premail = "\r\ne=", *prephone = "\r\np=";

    gethostname (hostname, sizeof (hostname));

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
     || (AddressToSDP (addr, addrlen, connection) == NULL))
        return NULL;

    strcpy (sfilter, "");
    if (srclen > 0)
    {
        char machine[MAXSDPADDRESS];

        if (AddressToSDP (src, srclen, machine) != NULL)
            sprintf (sfilter, "\r\na=source-filter: incl IN IP%c * %s",
                     machine[5], machine + 7);
    }

    if (asprintf (&sdp, "v=0"
                    "\r\no=- %"PRIu64" %"PRIu64" IN IP%c %s"
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
               /* o= */ now, now, connection[5], hostname,
               /* s= */ name,
               /* i= */ description,
               /* u= */ preurl, url,
               /* e= */ premail, email,
               /* p= */ prephone, phone,
               /* c= */ connection,
    /* source-filter */ sfilter) == -1)
        return NULL;
    return sdp;
}


static char *
vsdp_AddAttribute (char **sdp, const char *name, const char *fmt, va_list ap)
{
    size_t oldlen = strlen (*sdp);
    size_t addlen = sizeof ("a=\r\n") + strlen (name);

    if (fmt != NULL)
    {
        va_list aq;

        va_copy (aq, ap);
        addlen += 1 + vsnprintf (NULL, 0, fmt, aq);
        va_end (aq);
    }

    char *ret = realloc (*sdp, oldlen + addlen);
    if (ret == NULL)
        return NULL;

    oldlen += sprintf (ret + oldlen, "a=%s", name);
    if (fmt != NULL)
    {
        ret[oldlen++] = ':';
        oldlen += vsprintf (ret + oldlen, fmt, ap);
    }

    strcpy (ret + oldlen, "\r\n");
    return *sdp = ret;
}


char *sdp_AddAttribute (char **sdp, const char *name, const char *fmt, ...)
{
    char *ret;
    va_list ap;

    va_start (ap, fmt);
    ret = vsdp_AddAttribute (sdp, name, fmt, ap);
    va_end (ap);

    return ret;
}


char *sdp_AddMedia (char **sdp,
                    const char *type, const char *protocol, int dport,
                    unsigned pt, bool bw_indep, unsigned bw,
                    const char *ptname, unsigned clock, unsigned chans,
                    const char *fmtp)
{
    char *newsdp, *ptr;
    size_t inlen = strlen (*sdp), outlen = inlen;

    /* Some default values */
    if (type == NULL)
        type = "video";
    if (protocol == NULL)
        protocol = "RTP/AVP";
    assert (pt < 128u);

    outlen += snprintf (NULL, 0,
                        "m=%s %u %s %d\r\n"
                        "b=TIAS:%u\r\n"
                        "b=RR:0\r\n",
                        type, dport, protocol, pt, bw);

    newsdp = realloc (*sdp, outlen + 1);
    if (newsdp == NULL)
        return NULL;

    *sdp = newsdp;
    ptr = newsdp + inlen;

    ptr += sprintf (ptr, "m=%s %u %s %u\r\n",
                         type, dport, protocol, pt);
    if (bw > 0)
        ptr += sprintf (ptr, "b=%s:%u\r\n", bw_indep ? "TIAS" : "AS", bw);
    ptr += sprintf (ptr, "b=RR:0\r\n");

    /* RTP payload type map */
    if (ptname != NULL)
    {
        if ((strcmp (type, "audio") == 0) && (chans != 1))
            sdp_AddAttribute (sdp, "rtpmap", "%u %s/%u/%u", pt, ptname, clock,
                              chans);
        else
            sdp_AddAttribute (sdp, "rtpmap", "%u %s/%u", pt, ptname, clock);
    }
    /* Format parameters */
    if (fmtp != NULL)
        sdp_AddAttribute (sdp, "fmtp", "%u %s", pt, fmtp);

    return newsdp;
}


char *vlc_sdp_Start (vlc_object_t *obj, const char *cfgpref,
                     const struct sockaddr *src, size_t srclen,
                     const struct sockaddr *addr, size_t addrlen)
{
    size_t cfglen = strlen (cfgpref);
    if (cfglen > 100)
        return NULL;

    char varname[cfglen + sizeof ("description")], *subvar = varname + cfglen;
    strcpy (varname, cfgpref);

    strcpy (subvar, "name");
    char *name = var_GetNonEmptyString (obj, varname);
    strcpy (subvar, "description");
    char *description = var_GetNonEmptyString (obj, varname);
    strcpy (subvar, "url");
    char *url = var_GetNonEmptyString (obj, varname);
    strcpy (subvar, "email");
    char *email = var_GetNonEmptyString (obj, varname);
    strcpy (subvar, "phone");
    char *phone = var_GetNonEmptyString (obj, varname);

    char *sdp = sdp_Start (name, description, url, email, phone,
                           src, srclen, addr, addrlen);
    free (name);
    free (description);
    free (url);
    free (email);
    free (phone);

    if (sdp == NULL)
        return NULL;

    /* Totally non-standard */
    strcpy (subvar, "group");
    char *group = var_GetNonEmptyString (obj, varname);
    if (group != NULL)
    {
        sdp_AddAttribute (&sdp, "x-plgroup", "%s", group);
        free (group);
    }

    return sdp;
}
