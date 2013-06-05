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

#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_block.h>
#include <vlc_network.h>

#include <limits.h>
#include <unistd.h>
#ifdef HAVE_POLL
# include <poll.h>
#endif

#include "rtp.h"
#ifdef HAVE_SRTP
# include <srtp.h>
#endif

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

static int rtp_timeout (mtime_t deadline)
{
    if (deadline == VLC_TS_INVALID)
        return -1; /* infinite */

    mtime_t t = mdate ();
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
    mtime_t deadline = VLC_TS_INVALID;
    int rtp_fd = sys->fd;

    struct pollfd ufd[1];
    ufd[0].fd = rtp_fd;
    ufd[0].events = POLLIN;

    for (;;)
    {
        int n = poll (ufd, 1, rtp_timeout (deadline));
        if (n == -1)
            continue;

        int canc = vlc_savecancel ();
        if (n == 0)
            goto dequeue;

        if (ufd[0].revents)
        {
            n--;
            if (unlikely(ufd[0].revents & POLLHUP))
                break; /* RTP socket dead (DCCP only) */

            block_t *block = block_Alloc (0xffff); /* TODO: p_sys->mru */
            if (unlikely(block == NULL))
                break; /* we are totallly screwed */

            ssize_t len = recv (rtp_fd, block->p_buffer, block->i_buffer, 0);
            if (len != -1)
            {
                block->i_buffer = len;
                rtp_process (demux, block);
            }
            else
            {
                msg_Warn (demux, "RTP network error: %m");
                block_Release (block);
            }
        }

    dequeue:
        if (!rtp_dequeue (demux, sys->session, &deadline))
            deadline = VLC_TS_INVALID;
        vlc_restorecancel (canc);
    }
    return NULL;
}

/**
 * RTP/RTCP session thread for stream sockets (framed RTP)
 */
void *rtp_stream_thread (void *opaque)
{
#ifndef _WIN32
    demux_t *demux = opaque;
    demux_sys_t *sys = demux->p_sys;
    int fd = sys->fd;

    for (;;)
    {
        /* There is no reordering on stream sockets, so no timeout. */
        ssize_t val;

        uint16_t frame_len;
        if (recv (fd, &frame_len, 2, MSG_WAITALL) != 2)
            break;

        block_t *block = block_Alloc (ntohs (frame_len));
        if (unlikely(block == NULL))
            break;

        block_cleanup_push (block);
        val = recv (fd, block->p_buffer, block->i_buffer, MSG_WAITALL);
        vlc_cleanup_pop ();

        if (val != (ssize_t)block->i_buffer)
        {
            block_Release (block);
            break;
        }

        int canc = vlc_savecancel ();
        rtp_process (demux, block);
        rtp_dequeue_force (demux, sys->session);
        vlc_restorecancel (canc);
    }
#else
    (void) opaque;
#endif
    return NULL;
}
