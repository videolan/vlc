/**
 * @file session.c
 * @brief RTP session handling
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

#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include <vlc_common.h>
#include <vlc_demux.h>

#include "rtp.h"

typedef struct rtp_source_t rtp_source_t;

/** State for a RTP session: */
struct rtp_session_t
{
    rtp_source_t **srcv;
    unsigned       srcc;
    uint8_t        ptc;
    rtp_pt_t      *ptv;
};

static rtp_source_t *
rtp_source_create (demux_t *, const rtp_session_t *, uint32_t, uint16_t);
static void
rtp_source_destroy (demux_t *, const rtp_session_t *, rtp_source_t *);

static void rtp_decode (demux_t *, const rtp_session_t *, rtp_source_t *);

/**
 * Creates a new RTP session.
 */
rtp_session_t *
rtp_session_create (demux_t *demux)
{
    rtp_session_t *session = malloc (sizeof (*session));
    if (session == NULL)
        return NULL;

    session->srcv = NULL;
    session->srcc = 0;
    session->ptc = 0;
    session->ptv = NULL;

    (void)demux;
    return session;
}


/**
 * Destroys an RTP session.
 */
void rtp_session_destroy (demux_t *demux, rtp_session_t *session)
{
    for (unsigned i = 0; i < session->srcc; i++)
        rtp_source_destroy (demux, session, session->srcv[i]);

    free (session->srcv);
    free (session->ptv);
    free (session);
    (void)demux;
}

static void *no_init (demux_t *demux)
{
    (void)demux;
    return NULL;
}

static void no_destroy (demux_t *demux, void *opaque)
{
    (void)demux; (void)opaque;
}

static void no_decode (demux_t *demux, void *opaque, block_t *block)
{
    (void)demux; (void)opaque;
    block_Release (block);
}

/**
 * Adds a payload type to an RTP session.
 */
int rtp_add_type (demux_t *demux, rtp_session_t *ses, const rtp_pt_t *pt)
{
    if (ses->srcc > 0)
    {
        msg_Err (demux, "cannot change RTP payload formats during session");
        return EINVAL;
    }

    rtp_pt_t *ppt = realloc (ses->ptv, (ses->ptc + 1) * sizeof (rtp_pt_t));
    if (ppt == NULL)
        return ENOMEM;

    ses->ptv = ppt;
    ppt += ses->ptc++;

    ppt->init = pt->init ? pt->init : no_init;
    ppt->destroy = pt->destroy ? pt->destroy : no_destroy;
    ppt->decode = pt->decode ? pt->decode : no_decode;
    ppt->header = NULL;
    ppt->frequency = pt->frequency;
    ppt->number = pt->number;
    msg_Dbg (demux, "added payload type %"PRIu8" (f = %"PRIu32" Hz)",
             ppt->number, ppt->frequency);

    assert (ppt->frequency > 0); /* SIGFPE! */
    (void)demux;
    return 0;
}

/** State for an RTP source */
struct rtp_source_t
{
    uint32_t ssrc;
    uint32_t jitter;  /* interarrival delay jitter estimate */
    mtime_t  last_rx; /* last received packet local timestamp */
    uint32_t last_ts; /* last received packet RTP timestamp */

    uint32_t ref_rtp; /* sender RTP timestamp reference */
    mtime_t  ref_ntp; /* sender NTP timestamp reference */

    uint16_t bad_seq; /* tentatively next expected sequence for resync */
    uint16_t max_seq; /* next expected sequence */

    uint16_t last_seq; /* sequence of the next dequeued packet */
    block_t *blocks; /* re-ordered blocks queue */
    void    *opaque[]; /* Per-source private payload data */
};

/**
 * Initializes a new RTP source within an RTP session.
 */
static rtp_source_t *
rtp_source_create (demux_t *demux, const rtp_session_t *session,
                   uint32_t ssrc, uint16_t init_seq)
{
    rtp_source_t *source;

    source = malloc (sizeof (*source) + (sizeof (void *) * session->ptc));
    if (source == NULL)
        return NULL;

    source->ssrc = ssrc;
    source->jitter = 0;
    source->ref_rtp = 0;
    /* TODO: use VLC_TS_0, but VLC does not like negative PTS at the moment */
    source->ref_ntp = UINT64_C (1) << 62;
    source->max_seq = source->bad_seq = init_seq;
    source->last_seq = init_seq - 1;
    source->blocks = NULL;

    /* Initializes all payload */
    for (unsigned i = 0; i < session->ptc; i++)
        source->opaque[i] = session->ptv[i].init (demux);

    msg_Dbg (demux, "added RTP source (%08x)", ssrc);
    return source;
}


/**
 * Destroys an RTP source and its associated streams.
 */
static void
rtp_source_destroy (demux_t *demux, const rtp_session_t *session,
                    rtp_source_t *source)
{
    msg_Dbg (demux, "removing RTP source (%08x)", source->ssrc);

    for (unsigned i = 0; i < session->ptc; i++)
        session->ptv[i].destroy (demux, source->opaque[i]);
    block_ChainRelease (source->blocks);
    free (source);
}

static inline uint16_t rtp_seq (const block_t *block)
{
    assert (block->i_buffer >= 4);
    return GetWBE (block->p_buffer + 2);
}

static inline uint32_t rtp_timestamp (const block_t *block)
{
    assert (block->i_buffer >= 12);
    return GetDWBE (block->p_buffer + 4);
}

static const struct rtp_pt_t *
rtp_find_ptype (const rtp_session_t *session, rtp_source_t *source,
                const block_t *block, void **pt_data)
{
    uint8_t ptype = rtp_ptype (block);

    for (unsigned i = 0; i < session->ptc; i++)
    {
        if (session->ptv[i].number == ptype)
        {
            if (pt_data != NULL)
                *pt_data = source->opaque[i];
            return &session->ptv[i];
        }
    }
    return NULL;
}

/**
 * Receives an RTP packet and queues it. Not a cancellation point.
 *
 * @param demux VLC demux object
 * @param session RTP session receiving the packet
 * @param block RTP packet including the RTP header
 */
void
rtp_queue (demux_t *demux, rtp_session_t *session, block_t *block)
{
    demux_sys_t *p_sys = demux->p_sys;

    /* RTP header sanity checks (see RFC 3550) */
    if (block->i_buffer < 12)
        goto drop;
    if ((block->p_buffer[0] >> 6 ) != 2) /* RTP version number */
        goto drop;

    /* Remove padding if present */
    if (block->p_buffer[0] & 0x20)
    {
        uint8_t padding = block->p_buffer[block->i_buffer - 1];
        if ((padding == 0) || (block->i_buffer < (12u + padding)))
            goto drop; /* illegal value */

        block->i_buffer -= padding;
    }

    mtime_t        now = mdate ();
    rtp_source_t  *src  = NULL;
    const uint16_t seq  = rtp_seq (block);
    const uint32_t ssrc = GetDWBE (block->p_buffer + 8);

    /* In most case, we know this source already */
    for (unsigned i = 0, max = session->srcc; i < max; i++)
    {
        rtp_source_t *tmp = session->srcv[i];
        if (tmp->ssrc == ssrc)
        {
            src = tmp;
            break;
        }

        /* RTP source garbage collection */
        if ((tmp->last_rx + p_sys->timeout) < now)
        {
            rtp_source_destroy (demux, session, tmp);
            if (--session->srcc > 0)
                session->srcv[i] = session->srcv[session->srcc - 1];
        }
    }

    if (src == NULL)
    {
        /* New source */
        if (session->srcc >= p_sys->max_src)
        {
            msg_Warn (demux, "too many RTP sessions");
            goto drop;
        }

        rtp_source_t **tab;
        tab = realloc (session->srcv, (session->srcc + 1) * sizeof (*tab));
        if (tab == NULL)
            goto drop;
        session->srcv = tab;

        src = rtp_source_create (demux, session, ssrc, seq);
        if (src == NULL)
            goto drop;

        tab[session->srcc++] = src;
        /* Cannot compute jitter yet */
    }
    else
    {
        const rtp_pt_t *pt = rtp_find_ptype (session, src, block, NULL);

        if (pt != NULL)
        {
            /* Recompute jitter estimate.
             * That is computed from the RTP timestamps and the system clock.
             * It is independent of RTP sequence. */
            uint32_t freq = pt->frequency;
            int64_t ts = rtp_timestamp (block);
            int64_t d = ((now - src->last_rx) * freq) / CLOCK_FREQ;
            d        -=    ts - src->last_ts;
            if (d < 0) d = -d;
            src->jitter += ((d - src->jitter) + 8) >> 4;
        }
    }
    src->last_rx = now;
    block->i_pts = now; /* store reception time until dequeued */
    src->last_ts = rtp_timestamp (block);

    /* Check sequence number */
    /* NOTE: the sequence number is per-source,
     * but is independent from the payload type. */
    int16_t delta_seq = seq - src->max_seq;
    if ((delta_seq > 0) ? (delta_seq > p_sys->max_dropout)
                        : (-delta_seq > p_sys->max_misorder))
    {
        msg_Dbg (demux, "sequence discontinuity"
                 " (got: %"PRIu16", expected: %"PRIu16")", seq, src->max_seq);
        if (seq == src->bad_seq)
        {
            src->max_seq = src->bad_seq = seq + 1;
            src->last_seq = seq - 0x7fffe; /* hack for rtp_decode() */
            msg_Warn (demux, "sequence resynchronized");
            block_ChainRelease (src->blocks);
            src->blocks = NULL;
        }
        else
        {
            src->bad_seq = seq + 1;
            goto drop;
        }
    }
    else
    if (delta_seq >= 0)
        src->max_seq = seq + 1;

    /* Queues the block in sequence order,
     * hence there is a single queue for all payload types. */
    block_t **pp = &src->blocks;
    for (block_t *prev = *pp; prev != NULL; prev = *pp)
    {
        delta_seq = seq - rtp_seq (prev);
        if (delta_seq < 0)
            break;
        if (delta_seq == 0)
        {
            msg_Dbg (demux, "duplicate packet (sequence: %"PRIu16")", seq);
            goto drop; /* duplicate */
        }
        pp = &prev->p_next;
    }
    block->p_next = *pp;
    *pp = block;

    /*rtp_decode (demux, session, src);*/
    return;

drop:
    block_Release (block);
}


static void rtp_decode (demux_t *, const rtp_session_t *, rtp_source_t *);

/**
 * Dequeues RTP packets and pass them to decoder. Not cancellation-safe(?).
 * A packet is decoded if it is the next in sequence order, or if we have
 * given up waiting on the missing packets (time out) from the last one
 * already decoded.
 *
 * @param demux VLC demux object
 * @param session RTP session receiving the packet
 * @param deadlinep pointer to deadline to call rtp_dequeue() again
 * @return true if the buffer is not empty, false otherwise.
 * In the later case, *deadlinep is undefined.
 */
bool rtp_dequeue (demux_t *demux, const rtp_session_t *session,
                  mtime_t *restrict deadlinep)
{
    mtime_t now = mdate ();
    bool pending = false;

    *deadlinep = INT64_MAX;

    for (unsigned i = 0, max = session->srcc; i < max; i++)
    {
        rtp_source_t *src = session->srcv[i];
        block_t *block;

        /* Because of IP packet delay variation (IPDV), we need to guesstimate
         * how long to wait for a missing packet in the RTP sequence
         * (see RFC3393 for background on IPDV).
         *
         * This situation occurs if a packet got lost, or if the network has
         * re-ordered packets. Unfortunately, the MSL is 2 minutes, orders of
         * magnitude too long for multimedia. We need a trade-off.
         * If we underestimated IPDV, we may have to discard valid but late
         * packets. If we overestimate it, we will either cause too much
         * delay, or worse, underflow our downstream buffers, as we wait for
         * definitely a lost packets.
         *
         * The rest of the "de-jitter buffer" work is done by the internal
         * LibVLC E/S-out clock synchronization. Here, we need to bother about
         * re-ordering packets, as decoders can't cope with mis-ordered data.
         */
        while (((block = src->blocks)) != NULL)
        {
            if ((int16_t)(rtp_seq (block) - (src->last_seq + 1)) <= 0)
            {   /* Next (or earlier) block ready, no need to wait */
                rtp_decode (demux, session, src);
                continue;
            }

            /* Wait for 3 times the inter-arrival delay variance (about 99.7%
             * match for random gaussian jitter).
             */
            mtime_t deadline;
            const rtp_pt_t *pt = rtp_find_ptype (session, src, block, NULL);
            if (pt)
                deadline = CLOCK_FREQ * 3 * src->jitter / pt->frequency;
            else
                deadline = 0; /* no jitter estimate with no frequency :( */

            /* Make sure we wait at least for 25 msec */
            if (deadline < (CLOCK_FREQ / 40))
                deadline = CLOCK_FREQ / 40;

            /* Additionnaly, we implicitly wait for the packetization time
             * multiplied by the number of missing packets. block is the first
             * non-missing packet (lowest sequence number). We have no better
             * estimated time of arrival, as we do not know the RTP timestamp
             * of not yet received packets. */
            deadline += block->i_pts;
            if (now >= deadline)
            {
                rtp_decode (demux, session, src);
                continue;
            }
            if (*deadlinep > deadline)
                *deadlinep = deadline;
            pending = true; /* packet pending in buffer */
            break;
        }
    }
    return pending;
}

/**
 * Dequeues all RTP packets and pass them to decoder. Not cancellation-safe(?).
 * This function can be used when the packet source is known not to reorder.
 */
void rtp_dequeue_force (demux_t *demux, const rtp_session_t *session)
{
    for (unsigned i = 0, max = session->srcc; i < max; i++)
    {
        rtp_source_t *src = session->srcv[i];
        block_t *block;

        while (((block = src->blocks)) != NULL)
            rtp_decode (demux, session, src);
    }
}

/**
 * Decodes one RTP packet.
 */
static void
rtp_decode (demux_t *demux, const rtp_session_t *session, rtp_source_t *src)
{
    block_t *block = src->blocks;

    assert (block);
    src->blocks = block->p_next;
    block->p_next = NULL;

    /* Discontinuity detection */
    uint16_t delta_seq = rtp_seq (block) - (src->last_seq + 1);
    if (delta_seq != 0)
    {
        if (delta_seq >= 0x8000)
        {   /* Trash too late packets (and PIM Assert duplicates) */
            msg_Dbg (demux, "ignoring late packet (sequence: %"PRIu16")",
                      rtp_seq (block));
            goto drop;
        }
        msg_Warn (demux, "%"PRIu16" packet(s) lost", delta_seq);
        block->i_flags |= BLOCK_FLAG_DISCONTINUITY;
    }
    src->last_seq = rtp_seq (block);

    /* Match the payload type */
    void *pt_data;
    const rtp_pt_t *pt = rtp_find_ptype (session, src, block, &pt_data);
    if (pt == NULL)
    {
        msg_Dbg (demux, "unknown payload (%"PRIu8")",
                 rtp_ptype (block));
        goto drop;
    }

    if(pt->header)
        pt->header(demux, pt_data, block);

    /* Computes the PTS from the RTP timestamp and payload RTP frequency.
     * DTS is unknown. Also, while the clock frequency depends on the payload
     * format, a single source MUST only use payloads of a chosen frequency.
     * Otherwise it would be impossible to compute consistent timestamps. */
    const uint32_t timestamp = rtp_timestamp (block);
    block->i_pts = src->ref_ntp
       + CLOCK_FREQ * (int32_t)(timestamp - src->ref_rtp) / pt->frequency;
    /* TODO: proper inter-medias/sessions sync (using RTCP-SR) */
    src->ref_ntp = block->i_pts;
    src->ref_rtp = timestamp;

    /* CSRC count */
    size_t skip = 12u + (block->p_buffer[0] & 0x0F) * 4;

    /* Extension header (ignored for now) */
    if (block->p_buffer[0] & 0x10)
    {
        skip += 4;
        if (block->i_buffer < skip)
            goto drop;

        skip += 4 * GetWBE (block->p_buffer + skip - 2);
    }

    if (block->i_buffer < skip)
        goto drop;

    block->p_buffer += skip;
    block->i_buffer -= skip;

    pt->decode (demux, pt_data, block);
    return;

drop:
    block_Release (block);
}
