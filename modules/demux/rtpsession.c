/**
 * @file session.c
 * @brief RTP session handling
 */
/*****************************************************************************
 * Copyright © 2008 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2.0
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

#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include <vlc/vlc.h>
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
    mtime_t  expiry;  /* inactivation date */
    uint32_t ssrc;
    uint16_t bad_seq; /* tentatively next expected sequence for resync */
    uint16_t max_seq; /* next expected sequence */

    uint16_t last_seq; /* sequence of the last dequeued packet */
    block_t *blocks; /* re-ordered blocks queue */
    void    *opaque[0]; /* Per-source private payload data */
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

/**
 * Receives an RTP packet and queues it.
 * @param demux VLC demux object
 * @param session RTP session receiving the packet
 * @param block RTP packet including the RTP header
 */
void
rtp_receive (demux_t *demux, rtp_session_t *session, block_t *block)
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
    const uint16_t seq  = GetWBE (block->p_buffer + 2);
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
        if (tmp->expiry < now)
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
    }

    /* Be optimistic for the first packet. Certain codec, such as Vorbis
     * do not like loosing the first packet(s), so we cannot just wait
     * for proper sequence synchronization. And we don't want to assume that
     * the sender starts at seq=0 either. */
    if (src->blocks == NULL)
        src->max_seq = seq - p_sys->max_dropout;

    /* Check sequence number */
    /* NOTE: the sequence number is per-source,
     * but is independent from the payload type. */
    uint16_t delta_seq = seq - (src->max_seq + 1);
    if ((delta_seq < 0x8000) ? (delta_seq > p_sys->max_dropout)
                             : ((65535 - delta_seq) > p_sys->max_misorder))
    {
        msg_Dbg (demux, "sequence discontinuity (got: %u, expected: %u)",
                 seq, (src->max_seq + 1) & 0xffff);
        if (seq == ((src->bad_seq + 1) & 0xffff))
        {
            src->max_seq = src->bad_seq = seq;
            msg_Warn (demux, "sequence resynchronized");
            block_ChainRelease (src->blocks);
            src->blocks = NULL;
        }
        else
        {
            src->bad_seq = seq;
            goto drop;
        }
    }
    else
    if (delta_seq < 0x8000)
        src->max_seq = seq;

    /* Queues the block in sequence order,
     * hence there is a single queue for all payload types. */
    block_t **pp = &src->blocks;
    for (block_t *prev = *pp; prev != NULL; prev = *pp)
    {
        int16_t delta_seq = seq - rtp_seq (prev);
        if (delta_seq < 0)
            break;
        if (delta_seq == 0)
            goto drop; /* duplicate */
        pp = &prev->p_next;
    }
    block->p_next = *pp;
    *pp = block;

    rtp_decode (demux, session, src);
    return;

drop:
    block_Release (block);
}


static void
rtp_decode (demux_t *demux, const rtp_session_t *session, rtp_source_t *src)
{
    block_t *block = src->blocks;

    /* Buffer underflow? */
    if (!block || !block->p_next || !block->p_next->p_next)
        return;
    /* TODO: use time rather than packet counts for buffer measurement */
    src->blocks = block->p_next;
    block->p_next = NULL;

    /* Discontinuity detection */
    if (((src->last_seq + 1) & 0xffff) != rtp_seq (block))
        block->i_flags |= BLOCK_FLAG_DISCONTINUITY;
    src->last_seq = rtp_seq (block);

    /* Match the payload type */
    const rtp_pt_t *pt = NULL;
    void *pt_data = NULL;
    const uint8_t   ptype = block->p_buffer[1] & 0x7F;

    for (unsigned i = 0; i < session->ptc; i++)
    {
        if (session->ptv[i].number == ptype)
        {
            pt = &session->ptv[i];
            pt_data = src->opaque[i];
            break;
        }
    }

    if (pt == NULL)
    {
        msg_Dbg (demux, "ignoring unknown payload (%"PRIu8")", ptype);
        goto drop;
    }

    /* Computes the PTS from the RTP timestamp and payload RTP frequency.
     * DTS is unknown. Also, while the clock frequency depends on the payload
     * format, a single source MUST only use payloads of a chosen frequency.
     * Otherwise it would be impossible to compute consistent timestamps. */
    /* FIXME: handle timestamp wrap properly */
    /* TODO: sync multiple sources sanely... */
    const uint32_t timestamp = GetDWBE (block->p_buffer + 4);
    block->i_pts = UINT64_C(1) * CLOCK_FREQ * timestamp / pt->frequency;

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
