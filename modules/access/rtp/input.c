/**
 * @file input.c
 * @brief RTP packet input
 */
/*****************************************************************************
 * Copyright © 2008 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <limits.h>
#include <errno.h>
#ifdef HAVE_POLL_H
# include <poll.h>
#endif
#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif
#ifdef _WIN32
# include <winsock2.h>
#endif

#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_block.h>
#include <vlc_dtls.h>

#include "rtp.h"
#ifdef HAVE_SRTP
# include "srtp.h"
#endif

#define DEFAULT_MRU (1500u - (20 + 8))

/**
 * Processes a packet received from the RTP socket.
 */
static void rtp_process (demux_t *demux, block_t *block)
{
    demux_sys_t *sys = demux->p_sys;

    if (block->i_buffer < 2)
        goto drop;
    const uint8_t ptype = rtp_ptype (block);
    if (ptype >= 72 && ptype <= 76)
        goto drop; /* Muxed RTCP, ignore for now FIXME */

#ifdef HAVE_SRTP
    if (sys->srtp != NULL)
    {
        size_t len = block->i_buffer;
        if (srtp_recv (sys->srtp, block->p_buffer, &len))
        {
            msg_Dbg (demux, "SRTP authentication/decryption failed");
            goto drop;
        }
        block->i_buffer = len;
    }
#endif

    /* TODO: use SDP and get rid of this hack */
    if (unlikely(sys->autodetect))
    {   /* Autodetect payload type, _before_ rtp_queue() */
        rtp_autodetect (demux, sys->session, block);
        sys->autodetect = false;
    }

    rtp_queue (demux, sys->session, block);
    return;
drop:
    block_Release (block);
}

static int rtp_timeout (vlc_tick_t deadline)
{
    if (deadline == VLC_TICK_INVALID)
        return -1; /* infinite */

    vlc_tick_t t = vlc_tick_now ();
    if (t >= deadline)
        return 0;

    t = (deadline - t) / (CLOCK_FREQ / INT64_C(1000));
    if (unlikely(t > INT_MAX))
        return INT_MAX;
    return t;
}

/**
 * RTP/RTCP session thread for datagram sockets
 */
void *rtp_dgram_thread (void *opaque)
{
    demux_t *demux = opaque;
    demux_sys_t *sys = demux->p_sys;
    vlc_tick_t deadline = VLC_TICK_INVALID;
    struct vlc_dtls *rtp_sock = sys->rtp_sock;

    for (;;)
    {
        struct pollfd ufd[1];

        ufd[0].events = POLLIN;
        ufd[0].fd = vlc_dtls_GetPollFD(rtp_sock, &ufd[0].events);

        int n = poll (ufd, 1, rtp_timeout (deadline));
        if (n == -1)
            continue;

        int canc = vlc_savecancel ();
        if (n == 0)
            goto dequeue;

        if (ufd[0].revents)
        {
            block_t *block = block_Alloc(DEFAULT_MRU);
            if (unlikely(block == NULL))
                break; /* we are totallly screwed */

            bool truncated;
            ssize_t len = vlc_dtls_Recv(rtp_sock, block->p_buffer,
                                       block->i_buffer, &truncated);
            if (len >= 0) {
                if (truncated) {
                    msg_Err(demux, "packet truncated (MRU was %zu)",
                            block->i_buffer);
                    block->i_flags |= BLOCK_FLAG_CORRUPTED;
                }
                else
                    block->i_buffer = len;

                rtp_process (demux, block);
            }
            else
            {
                if (errno == EPIPE)
                    break; /* connection terminated */
                msg_Warn (demux, "RTP network error: %s",
                          vlc_strerror_c(errno));
                block_Release (block);
            }

            n--;
        }

    dequeue:
        if (!rtp_dequeue (demux, sys->session, &deadline))
            deadline = VLC_TICK_INVALID;
        vlc_restorecancel (canc);
    }
    return NULL;
}
