/*****************************************************************************
 * rtp.c : Real-Time Protocol (RTP) demux module for VLC media player
 *****************************************************************************
 * Copyright (C) 2001-2005 the VideoLAN team
 * Copyright © 2007 Rémi Denis-Courmont
 * $Id$
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
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <stdarg.h>
#include <assert.h>

#include <vlc_demux.h>
#include <vlc_aout.h>

#include <vlc_codecs.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define RTP_MAX_DROPOUT_TEXT N_("RTP maximum sequence number dropout")
#define RTP_MAX_DROPOUT_LONGTEXT N_( \
    "RTP packets will be discarded if they are too much ahead (i.e. in the " \
    "future) by this many packets from the last received packet." )

#define RTP_MAX_MISORDER_TEXT N_("RTP maximum sequence number misordering")
#define RTP_MAX_MISORDER_LONGTEXT N_( \
    "RTP packets will be discarded if they are too far behind (i.e. in the " \
    "past) by this many packets from the last received packet." )

#define RTP_MIN_SEQUENTIAL_TEXT N_("RTP minimum sequential packets count")
#define RTP_MIN_SEQUENTIAL_LONGTEXT N_( \
    "VLC will wait until it has received this many sequential RTP packets " \
    "before it considers the RTP stream synchronized." )


static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin ();
    set_shortname (_("RTP"));
    set_description (_("(Experimental) Real-Time Protocol demuxer"));
    set_category (CAT_INPUT);
    set_subcategory (SUBCAT_INPUT_DEMUX);
    set_capability ("access_demux", 10);
    set_callbacks (Open, Close);

    add_integer( "rtp-max-dropout", 3000, NULL, RTP_MAX_DROPOUT_TEXT,
                 RTP_MAX_DROPOUT_LONGTEXT, VLC_TRUE );
    add_integer( "rtp-max-misorder", 100, NULL, RTP_MAX_MISORDER_TEXT,
                 RTP_MAX_MISORDER_LONGTEXT, VLC_TRUE );
    add_integer( "rtp-min-seq", 2, NULL, RTP_MIN_SEQUENTIAL_TEXT,
                 RTP_MIN_SEQUENTIAL_LONGTEXT, VLC_TRUE );

    add_shortcut ("rtp");
vlc_module_end ();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux (demux_t *);
static int Control (demux_t *, int i_query, va_list args);
static block_t *ParseRTP (demux_t *obj, block_t *p_block, int8_t *pt,
                          uint16_t *seq);

#define RTP_PACKET_SIZE 0xffff

typedef int (*rtp_pt_cb) (demux_t *, block_t *, void *);

/* State for a RTP source */
typedef struct rtp_source_t
{
    uint32_t ssrc; /* current synchronization source */
    int8_t   pt;   /* current payload type, -1 if none */

    uint8_t  probation; /* how many packets left before resync */
    uint16_t max_seq; /* next expected sequence */
    uint16_t bad_seq; /* tentatively next expected sequence for resync */
} rtp_source_t;

/* State for a RTP session */
typedef struct rtp_session_t
{
    /*stream_t *feed;*/ /* where data comes from */
    /* TODO: keep values to sync multiple sessions. */
    /* We'd need to parse RTCP SR to do that though... */
    rtp_source_t src[1];
} rtp_session_t;


struct demux_sys_t
{
    uint16_t max_dropout;
    uint16_t max_misorder;
    uint16_t min_sequential;

    rtp_session_t session;
};


/*****************************************************************************
 * Open: check stream and initializes structures
 *****************************************************************************/
static int Open (vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *p_sys;

    assert (demux->s == NULL);
    msg_Dbg (pbj, "access = %s", obj->psz_access);

    /* Initializes demux */
    p_sys = calloc (1, sizeof (*p_sys));
    if (p_sys == NULL)
        return VLC_ENOMEM;

    p_sys->max_dropout = var_CreateGetInteger (obj, "rtp-max-dropout");
    p_sys->max_misorder = var_CreateGetInteger (obj, "rtp-max-misorder");
    p_sys->min_sequential = var_CreateGetInteger (obj, "rtp-min-seq");

    demux->pf_demux   = Demux;
    demux->pf_control = Control;
    demux->p_sys      = p_sys;

    return VLC_SUCCESS;
}


/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close (vlc_object_t *obj)
{
    demux_sys_t *p_sys = ((demux_t *)obj)->p_sys;
    free (p_sys);
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control (demux_t *p_demux, int i_query, va_list args)
{
    /*demux_sys_t *p_sys  = p_demux->p_sys;*/

    switch (i_query)
    {
        case DEMUX_GET_POSITION:
            return VLC_EGENERIC;

        case DEMUX_GET_TIME:
        case DEMUX_GET_LENGTH:
        {
            int64_t *v = va_arg (args, int64_t *);
            *v = 0;
            return 0;
        }
    }

    return VLC_EGENERIC;
}


static int Demux (demux_t *demux)
{
    //demux_sys_t *p_sys = demux->p_sys;
    block_t *block = stream_Block (demux->s, RTP_PACKET_SIZE);
    uint16_t seq;
    int8_t pt;

    if (block == NULL)
        return 0;

    block = ParseRTP (demux, block, &pt, &seq);
    if (block == NULL)
        return 1;

    msg_Dbg (demux, "got len = %5u; PT = %3d, seq = %5u", block->i_buffer, pt, seq);

    return 1;
}


static block_t *ParseRTP (demux_t *obj, block_t *p_block, int8_t *pt,
                          uint16_t *seq)
{
    size_t i_skip = 12;

    /* RTP header sanity checks (see RFC 3550) */
    if (p_block->i_buffer < 12)
        goto trash;

    if (p_block->i_buffer > RTP_PACKET_SIZE)
    {
        msg_Err (obj, "RTP packet too big! wrong demux?");
        goto trash;
    }

    // Version number:
    if ((p_block->p_buffer[0] >> 6 ) != 2)
    {
        // STUN/ICE anyone ?
        msg_Dbg (obj, "RTP version is %u instead of 2", p_block->p_buffer[0] >> 6);
        goto trash;
    }

    // Padding bit:
    uint8_t pad = (p_block->p_buffer[0] & 0x20)
                 ? p_block->p_buffer[p_block->i_buffer - 1] : 0;

    // CSRC count:
    i_skip += (p_block->p_buffer[0] & 0x0F) * 4;

    // Extension header:
    if (p_block->p_buffer[0] & 0x10) /* Extension header */
    {
        i_skip += 4;
        if ((size_t)p_block->i_buffer < i_skip)
            goto trash;

        i_skip += 4 * GetWBE (p_block->p_buffer + i_skip - 2);
    }

    if ((size_t)p_block->i_buffer < (i_skip + pad))
        goto trash;

    *pt = p_block->p_buffer[1] & 0x7F;
    *seq = GetWBE (p_block->p_buffer + 2);

    /* This is the place for deciphering and authentication */

    /* Remove the RTP header */
    p_block->i_buffer -= i_skip;
    p_block->p_buffer += i_skip;

    /* Remove padding (at the end) */
    p_block->i_buffer -= pad;

    return p_block;

trash:
    block_Release (p_block);
    msg_Dbg (obj, "ignored non-RTP packet");
    return NULL;
}


/**
 * Initializes a source before any packet has been received
 */
static
void PreinitSource (const demux_sys_t *p_sys, rtp_source_t *src)
{
    src->pt = -1;
    src->probation = p_sys->min_sequential;
}


/**
 * Reinitializes a source (resynchronization)
 */
static
void InitSource (const demux_sys_t *p_sys, rtp_source_t *src, uint32_t ssrc,
                 uint16_t seq)
{
    src->ssrc = ssrc;
    (void)src->pt;

    src->probation = p_sys->min_sequential - 1;
    src->bad_seq = src->max_seq = seq;
}


#if 0
/*
 * Generic packet handlers
 */

/* Ignore a packet */
static int pt_ignore (demux_t *obj, block_t *block, rtp_pt_t *self)
{
    (void)self;
    msg_Dbg (obj, "ignoring unknown payload type");
    block_Release (block);
    return 0;
}


/* Send a packet to decoder */
#if 0
static void pt_decode (demux_t *obj, block_t *block, rtp_pt_t *self)
{
    p_block->i_pts = p_block->i_dts = date_... (...);
    es_out_Control (obj->out, ES_OUT_SET_PCR, p_block->i_pts);
    es_out_Send (obj->out, (es_out_id_t *)*p_id, block);
    return 0;
}
#endif


/* Send a packet to a chained demuxer */
static
int pt_demux (demux_t *obj, block_t *block, rtp_pt_t *self, const char *demux)
{
    stream_t *stream = self->data.demux.stream;

    if (stream == NULL)
    {
        stream = stream_DemuxNew (obj, demux, obj->out);
        if (stream == NULL)
            return VLC_EGENERIC;
        self->data.demux.stream = stream;
    }

    stream_DemuxSend (stream, block);
    return 0;
}


/*
 * Static payload types handler
 */

/* PT=14
 * MPA: MPEG Audio (RFC2250, §3.4)
 */
static int pt_mpa (demux_t *obj, block_t *block, rtp_pt_t *self)
{
    if (block->i_buffer < 4)
        return VLC_EGENERIC;

    block->i_buffer -= 4; // 32 bits RTP/MPA header
    block->p_buffer += 4;

    return pt_demux (obj, block, self, "mpga");
}


/* PT=32
 * MPV: MPEG Video (RFC2250, §3.5)
 */
static int pt_mpv (demux_t *obj, block_t *block, rtp_pt_t *self)
{
    if (block->i_buffer < 4)
        return VLC_EGENERIC;

    block->i_buffer -= 4; // 32 bits RTP/MPV header
    block->p_buffer += 4;

    if (block->p_buffer[-3] & 0x4)
    {
        /* MPEG2 Video extension header */
        /* TODO: shouldn't we skip this too ? */
    }

    return pt_demux (obj, block, self, "mpgv");
}


/* PT=33
 * MP2: MPEG TS (RFC2250, §2)
 */
static int pt_ts (demux_t *obj, block_t *block, rtp_pt_t *self)
{
    return pt_demux (obj, block, self, "ts");
}


/*
 * Dynamic payload type handlers
 * Hmm, none implemented yet.
 */
#endif
