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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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

#include <unistd.h>
#ifdef HAVE_POLL
# include <poll.h>
#endif

#include "rtp.h"
#ifdef HAVE_SRTP
# include <srtp.h>
#endif

static bool fd_dead (int fd)
{
    struct pollfd ufd = { .fd = fd, };
    return (poll (&ufd, 1, 0) > 0) && (ufd.revents & POLLHUP);
}

/**
 * Gets a datagram from the network.
 * @param fd datagram file descriptor
 * @return a block or NULL on fatal error (socket dead)
 */
static block_t *rtp_dgram_recv (vlc_object_t *obj, int fd)
{
    block_t *block = block_Alloc (0xffff);
    ssize_t len;

    block_cleanup_push (block);
    do
    {
        len = net_Read (obj, fd, NULL,
                        block->p_buffer, block->i_buffer, false);

        if (((len <= 0) && fd_dead (fd)) || !vlc_object_alive (obj))
        {   /* POLLHUP -> permanent (DCCP) socket error */
            block_Release (block);
            block = NULL;
            break;
        }
    }
    while (len == -1);
    vlc_cleanup_pop ();

    return block ? block_Realloc (block, 0, len) : NULL;
}


/**
 * Gets a framed RTP packet.
 * @param fd stream file descriptor
 * @return a block or NULL in case of fatal error
 */
static block_t *rtp_stream_recv (vlc_object_t *obj, int fd)
{
    ssize_t len = 0;
    uint8_t hdr[2]; /* frame header */

    /* Receives the RTP frame header */
    do
    {
        ssize_t val = net_Read (obj, fd, NULL, hdr + len, 2 - len, false);
        if (val <= 0)
            return NULL;
        len += val;
    }
    while (len < 2);

    block_t *block = block_Alloc (GetWBE (hdr));

    /* Receives the RTP packet */
    for (ssize_t i = 0; i < len;)
    {
        ssize_t val;

        block_cleanup_push (block);
        val = net_Read (obj, fd, NULL,
                        block->p_buffer + i, block->i_buffer - i, false);
        vlc_cleanup_pop ();

        if (val <= 0)
        {
            block_Release (block);
            return NULL;
        }
        i += val;
    }

    return block;
}


static block_t *rtp_recv (demux_t *demux)
{
    demux_sys_t *p_sys = demux->p_sys;

    for (block_t *block;; block_Release (block))
    {
        block = p_sys->framed_rtp
                ? rtp_stream_recv (VLC_OBJECT (demux), p_sys->fd)
                : rtp_dgram_recv (VLC_OBJECT (demux), p_sys->fd);
        if (block == NULL)
        {
            msg_Err (demux, "RTP flow stopped");
            break; /* fatal error */
        }

        if (block->i_buffer < 2)
            continue;

        /* FIXME */
        const uint8_t ptype = rtp_ptype (block);
        if (ptype >= 72 && ptype <= 76)
            continue; /* Muxed RTCP, ignore for now */
#ifdef HAVE_SRTP
        if (p_sys->srtp)
        {
            size_t len = block->i_buffer;
            int canc, err;

            canc = vlc_savecancel ();
            err = srtp_recv (p_sys->srtp, block->p_buffer, &len);
            vlc_restorecancel (canc);
            if (err)
            {
                msg_Dbg (demux, "SRTP authentication/decryption failed");
                continue;
            }
            block->i_buffer = len;
        }
#endif
        return block; /* success! */
    }
    return NULL;
}


static void timer_cleanup (void *timer)
{
    vlc_timer_destroy ((vlc_timer_t)timer);
}

static void rtp_process (void *data);

void *rtp_thread (void *data)
{
    demux_t *demux = data;
    demux_sys_t *p_sys = demux->p_sys;
    bool autodetect = true;

    if (vlc_timer_create (&p_sys->timer, rtp_process, data))
        return NULL;
    vlc_cleanup_push (timer_cleanup, (void *)p_sys->timer);

    for (;;)
    {
        block_t *block = rtp_recv (demux);
        if (block == NULL)
            break;

        if (autodetect)
        {   /* Autodetect payload type, _before_ rtp_queue() */
            /* No need for lock - the queue is empty. */
            if (rtp_autodetect (demux, p_sys->session, block))
            {
                block_Release (block);
                continue;
            }
            autodetect = false;
        }

        int canc = vlc_savecancel ();
        vlc_mutex_lock (&p_sys->lock);
        rtp_queue (demux, p_sys->session, block);
        vlc_mutex_unlock (&p_sys->lock);
        vlc_restorecancel (canc);

        rtp_process (demux);
    }
    vlc_cleanup_run ();
    return NULL;
}


/**
 * Process one RTP packet from the de-jitter queue.
 */
static void rtp_process (void *data)
{
    demux_t *demux = data;
    demux_sys_t *p_sys = demux->p_sys;
    mtime_t deadline;

    vlc_mutex_lock (&p_sys->lock);
    if (rtp_dequeue (demux, p_sys->session, &deadline))
        vlc_timer_schedule (p_sys->timer, true, deadline, 0);
    vlc_mutex_unlock (&p_sys->lock);
}
