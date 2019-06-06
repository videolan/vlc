/*****************************************************************************
 * rtcp.c: RTCP stream output support
 *****************************************************************************
 * Copyright © 2007 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_block.h>

#include <vlc_network.h>
#include <vlc_sout.h>
#include <vlc_fs.h>
#include "rtp.h"

#include <assert.h>

#ifndef SOL_IP
# define SOL_IP IPPROTO_IP
#endif

/*
 * NOTE on RTCP implementation:
 * - there is a single sender (us), no conferencing here! => n = sender = 1,
 * - as such we need not bother to include Receiver Reports,
 * - in unicast case, there is a single receiver => members = 1 + 1 = 2,
 *   and obviously n > 25% of members,
 * - in multicast case, we do not want to maintain the number of receivers
 *   and we assume it is big (i.e. than 3) because that's what broadcasting is
 *   all about,
 * - it is assumed we_sent = true (could be wrong), since we are THE sender,
 * - we always send SR + SDES, while running,
 * - FIXME: we do not implement separate rate limiting for SDES,
 * - we do not implement any profile-specific extensions for the time being.
 */
struct rtcp_sender_t
{
    size_t   length;  /* RTCP packet length */
    uint8_t  payload[28 + 8 + (2 * 257) + 8];
    int      handle;  /* RTCP socket handler */

    uint32_t packets; /* RTP packets sent */
    uint32_t bytes;   /* RTP bytes sent */
    unsigned counter; /* RTP packets sent since last RTCP packet */
};


rtcp_sender_t *OpenRTCP (vlc_object_t *obj, int rtp_fd, int proto,
                         bool mux)
{
    rtcp_sender_t *rtcp;
    uint8_t *ptr;
    int fd;
    char src[NI_MAXNUMERICHOST];
    int sport;

    if (net_GetSockAddress (rtp_fd, src, &sport))
        return NULL;

    if (mux)
    {
        /* RTP/RTCP mux: duplicate the socket */
#ifndef _WIN32
        fd = vlc_dup (rtp_fd);
#else
        WSAPROTOCOL_INFO info;
        WSADuplicateSocket (rtp_fd, GetCurrentProcessId (), &info);
        fd = WSASocket (info.iAddressFamily, info.iSocketType, info.iProtocol,
                        &info, 0, 0);
#endif
    }
    else
    {
        /* RTCP on a separate port */
        char dst[NI_MAXNUMERICHOST];
        int dport;

        if (net_GetPeerAddress (rtp_fd, dst, &dport))
            return NULL;

        sport++;
        dport++;

        fd = net_OpenDgram (obj, src, sport, dst, dport, proto);
        if (fd != -1)
        {
            /* Copy the multicast IPv4 TTL value (useless for IPv6) */
            int ttl;
            socklen_t len = sizeof (ttl);

            if (!getsockopt (rtp_fd, SOL_IP, IP_MULTICAST_TTL, &ttl, &len))
                setsockopt (fd, SOL_IP, IP_MULTICAST_TTL, &ttl, len);

            /* Ignore all incoming RTCP-RR packets */
            setsockopt (fd, SOL_SOCKET, SO_RCVBUF, &(int){ 0 }, sizeof (int));
        }
    }

    if (fd == -1)
        return NULL;

    rtcp = malloc (sizeof (*rtcp));
    if (rtcp == NULL)
    {
        net_Close (fd);
        return NULL;
    }

    rtcp->handle = fd;
    rtcp->bytes = rtcp->packets = rtcp->counter = 0;

    ptr = (uint8_t *)strchr (src, '%');
    if (ptr != NULL)
        *ptr = '\0'; /* remove scope ID frop IPv6 addresses */

    ptr = rtcp->payload;

    /* Sender report */
    ptr[0] = 2 << 6; /* V = 2, P = RC = 0 */
    ptr[1] = 200; /* payload type: Sender Report */
    SetWBE (ptr + 2, 6); /* length = 6 (7 double words) */
    memset (ptr + 4, 0, 4); /* SSRC unknown yet */
    SetQWBE (ptr + 8, NTPtime64 ());
    memset (ptr + 16, 0, 12); /* timestamp and counters */
    ptr += 28;

    /* Source description */
    uint8_t *sdes = ptr;
    ptr[0] = (2 << 6) | 1; /* V = 2, P = 0, SC = 1 */
    ptr[1] = 202; /* payload type: Source Description */
    uint8_t *lenptr = ptr + 2;
    memset (ptr + 4, 0, 4); /* SSRC unknown yet */
    ptr += 8;

    ptr[0] = 1; /* CNAME - mandatory */
    assert (NI_MAXNUMERICHOST <= 256);
    ptr[1] = strlen (src);
    memcpy (ptr + 2, src, ptr[1]);
    ptr += ptr[1] + 2;

    static const char tool[] = PACKAGE_STRING;
    ptr[0] = 6; /* TOOL */
    ptr[1] = (sizeof (tool) > 256) ? 255 : (sizeof (tool) - 1);
    memcpy (ptr + 2, tool, ptr[1]);
    ptr += ptr[1] + 2;

    while ((ptr - sdes) & 3) /* 32-bits padding */
        *ptr++ = 0;
    SetWBE (lenptr, (ptr - sdes - 1) >> 2);

    rtcp->length = ptr - rtcp->payload;
    return rtcp;
}


void CloseRTCP (rtcp_sender_t *rtcp)
{
    if (rtcp == NULL)
        return;

    uint8_t *ptr = rtcp->payload;
    uint64_t now64 = NTPtime64 ();
    SetQWBE (ptr + 8, now64); /* Update the Sender Report timestamp */

    /* Bye */
    ptr += rtcp->length;
    ptr[0] = (2 << 6) | 1; /* V = 2, P = 0, SC = 1 */
    ptr[1] = 203; /* payload type: Bye */
    SetWBE (ptr + 2, 1);
    memcpy (ptr + 4, rtcp->payload + 4, 4); /* Copy SSRC from Sender Report */
    rtcp->length += 8;

    /* We are THE sender, so we are more important than anybody else, so
     * we can afford not to check bandwidth constraints here. */
    send (rtcp->handle, rtcp->payload, rtcp->length, 0);
    net_Close (rtcp->handle);
    free (rtcp);
}


void SendRTCP (rtcp_sender_t *restrict rtcp, const block_t *rtp)
{
    if ((rtcp == NULL) /* RTCP sender off */
     || (rtp->i_buffer < 12)) /* too short RTP packet */
        return;

    /* Updates statistics */
    rtcp->packets++;
    rtcp->bytes += rtp->i_buffer;
    rtcp->counter += rtp->i_buffer;

    /* 1.25% rate limit */
    if ((rtcp->counter / 80) < rtcp->length)
        return;

    uint8_t *ptr = rtcp->payload;
    uint32_t last = GetDWBE (ptr + 8); // last RTCP SR send time
    uint64_t now64 = NTPtime64 ();
    if ((now64 >> 32) < (last + 5))
        return; // no more than one SR every 5 seconds

    memcpy (ptr + 4, rtp->p_buffer + 8, 4); /* SR SSRC */
    SetQWBE (ptr + 8, now64);
    memcpy (ptr + 16, rtp->p_buffer + 4, 4); /* RTP timestamp */
    SetDWBE (ptr + 20, rtcp->packets);
    SetDWBE (ptr + 24, rtcp->bytes);
    memcpy (ptr + 28 + 4, rtp->p_buffer + 8, 4); /* SDES SSRC */

    if (send (rtcp->handle, ptr, rtcp->length, 0) == (ssize_t)rtcp->length)
        rtcp->counter = 0;
}
