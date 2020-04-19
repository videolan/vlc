/**
 * @file rtpfmt.c
 */
/*****************************************************************************
 * Copyright (C) 2001-2005 VLC authors and VideoLAN
 * Copyright © 2007-2009 Rémi Denis-Courmont
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

#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_dialog.h>
#include <vlc_aout.h> /* aout_FormatPrepare() */

#include "rtp.h"

/*
 * Generic packet handlers
 */

void *codec_init (demux_t *demux, es_format_t *fmt)
{
    if (fmt->i_cat == AUDIO_ES)
        aout_FormatPrepare (&fmt->audio);
    return es_out_Add (demux->out, fmt);
}

void codec_destroy (demux_t *demux, void *data)
{
    if (data)
        es_out_Del (demux->out, (es_out_id_t *)data);
}

/* Send a packet to decoder */
void codec_decode (demux_t *demux, void *data, block_t *block)
{
    if (data)
    {
        block->i_dts = VLC_TICK_INVALID; /* RTP does not specify this */
        es_out_SetPCR(demux->out, block->i_pts);
        es_out_Send (demux->out, (es_out_id_t *)data, block);
    }
    else
        block_Release (block);
}

static void *stream_init (demux_t *demux, const char *name)
{
    demux_sys_t *p_sys = demux->p_sys;

    if (p_sys->chained_demux != NULL)
        return NULL;
    p_sys->chained_demux = vlc_demux_chained_New(VLC_OBJECT(demux), name,
                                                 demux->out);
    return p_sys->chained_demux;
}

static void stream_destroy (demux_t *demux, void *data)
{
    demux_sys_t *p_sys = demux->p_sys;

    if (data)
    {
        vlc_demux_chained_Delete(data);
        p_sys->chained_demux = NULL;
    }
}

static void stream_header (demux_t *demux, void *data, block_t *block)
{
    VLC_UNUSED(demux);
    VLC_UNUSED(data);
    if(block->p_buffer[1] & 0x80) /* TS M-bit == discontinuity (RFC 2250, 2.1) */
    {
        block->i_flags |= BLOCK_FLAG_DISCONTINUITY;
    }
}

/* Send a packet to a chained demuxer */
static void stream_decode (demux_t *demux, void *data, block_t *block)
{
    if (data)
        vlc_demux_chained_Send(data, block);
    else
        block_Release (block);
    (void)demux;
}

/*
 * Static payload types handler
 */

/* PT=0
 * PCMU: G.711 µ-law (RFC3551)
 */
static void *pcmu_init (demux_t *demux)
{
    es_format_t fmt;

    es_format_Init (&fmt, AUDIO_ES, VLC_CODEC_MULAW);
    fmt.audio.i_rate = 8000;
    fmt.audio.i_physical_channels = AOUT_CHAN_CENTER;
    return codec_init (demux, &fmt);
}

/* PT=3
 * GSM
 */
static void *gsm_init (demux_t *demux)
{
    es_format_t fmt;

    es_format_Init (&fmt, AUDIO_ES, VLC_CODEC_GSM);
    fmt.audio.i_rate = 8000;
    fmt.audio.i_physical_channels = AOUT_CHAN_CENTER;
    return codec_init (demux, &fmt);
}

/* PT=8
 * PCMA: G.711 A-law (RFC3551)
 */
static void *pcma_init (demux_t *demux)
{
    es_format_t fmt;

    es_format_Init (&fmt, AUDIO_ES, VLC_CODEC_ALAW);
    fmt.audio.i_rate = 8000;
    fmt.audio.i_physical_channels = AOUT_CHAN_CENTER;
    return codec_init (demux, &fmt);
}

/* PT=10,11
 * L16: 16-bits (network byte order) PCM
 */
static void *l16s_init (demux_t *demux)
{
    es_format_t fmt;

    es_format_Init (&fmt, AUDIO_ES, VLC_CODEC_S16B);
    fmt.audio.i_rate = 44100;
    fmt.audio.i_physical_channels = AOUT_CHANS_STEREO;
    return codec_init (demux, &fmt);
}

static void *l16m_init (demux_t *demux)
{
    es_format_t fmt;

    es_format_Init (&fmt, AUDIO_ES, VLC_CODEC_S16B);
    fmt.audio.i_rate = 44100;
    fmt.audio.i_physical_channels = AOUT_CHAN_CENTER;
    return codec_init (demux, &fmt);
}

/* PT=12
 * QCELP
 */
static void *qcelp_init (demux_t *demux)
{
    es_format_t fmt;

    es_format_Init (&fmt, AUDIO_ES, VLC_CODEC_QCELP);
    fmt.audio.i_rate = 8000;
    fmt.audio.i_physical_channels = AOUT_CHAN_CENTER;
    return codec_init (demux, &fmt);
}

/* PT=14
 * MPA: MPEG Audio (RFC2250, §3.4)
 */
static void *mpa_init (demux_t *demux)
{
    es_format_t fmt;

    es_format_Init (&fmt, AUDIO_ES, VLC_CODEC_MPGA);
    fmt.audio.i_physical_channels = AOUT_CHANS_STEREO;
    fmt.b_packetized = false;
    return codec_init (demux, &fmt);
}

static void mpa_decode (demux_t *demux, void *data, block_t *block)
{
    if (block->i_buffer < 4)
    {
        block_Release (block);
        return;
    }

    block->i_buffer -= 4; /* 32-bits RTP/MPA header */
    block->p_buffer += 4;

    codec_decode (demux, data, block);
}


/* PT=32
 * MPV: MPEG Video (RFC2250, §3.5)
 */
static void *mpv_init (demux_t *demux)
{
    es_format_t fmt;

    es_format_Init (&fmt, VIDEO_ES, VLC_CODEC_MPGV);
    fmt.b_packetized = false;
    return codec_init (demux, &fmt);
}

static void mpv_decode (demux_t *demux, void *data, block_t *block)
{
    if (block->i_buffer < 4)
    {
        block_Release (block);
        return;
    }

    block->i_buffer -= 4; /* 32-bits RTP/MPV header */
    block->p_buffer += 4;
#if 0
    if (block->p_buffer[-3] & 0x4)
    {
        /* MPEG2 Video extension header */
        /* TODO: shouldn't we skip this too ? */
    }
#endif
    codec_decode (demux, data, block);
}


/* PT=33
 * MP2: MPEG TS (RFC2250, §2)
 */
static void *ts_init (demux_t *demux)
{
    return stream_init (demux, "ts");
}


/* Not using SDP, we need to guess the payload format used */
/* see http://www.iana.org/assignments/rtp-parameters */
void rtp_autodetect (demux_t *demux, rtp_session_t *session,
                     const block_t *block)
{
    uint8_t ptype = rtp_ptype (block);
    rtp_pt_t pt = {
        .init = NULL,
        .destroy = codec_destroy,
        .header = NULL,
        .decode = codec_decode,
        .frequency = 0,
        .number = ptype,
    };

    /* Remember to keep this in sync with modules/services_discovery/sap.c */
    switch (ptype)
    {
      case 0:
        msg_Dbg (demux, "detected G.711 mu-law");
        pt.init = pcmu_init;
        pt.frequency = 8000;
        break;

      case 3:
        msg_Dbg (demux, "detected GSM");
        pt.init = gsm_init;
        pt.frequency = 8000;
        break;

      case 8:
        msg_Dbg (demux, "detected G.711 A-law");
        pt.init = pcma_init;
        pt.frequency = 8000;
        break;

      case 10:
        msg_Dbg (demux, "detected stereo PCM");
        pt.init = l16s_init;
        pt.frequency = 44100;
        break;

      case 11:
        msg_Dbg (demux, "detected mono PCM");
        pt.init = l16m_init;
        pt.frequency = 44100;
        break;

      case 12:
        msg_Dbg (demux, "detected QCELP");
        pt.init = qcelp_init;
        pt.frequency = 8000;
        break;

      case 14:
        msg_Dbg (demux, "detected MPEG Audio");
        pt.init = mpa_init;
        pt.decode = mpa_decode;
        pt.frequency = 90000;
        break;

      case 32:
        msg_Dbg (demux, "detected MPEG Video");
        pt.init = mpv_init;
        pt.decode = mpv_decode;
        pt.frequency = 90000;
        break;

      case 33:
        msg_Dbg (demux, "detected MPEG2 TS");
        pt.init = ts_init;
        pt.destroy = stream_destroy;
        pt.header = stream_header;
        pt.decode = stream_decode;
        pt.frequency = 90000;
        break;

      default:
        if (ptype >= 96)
        {
            char *dynamic = var_InheritString(demux, "rtp-dynamic-pt");
            if (dynamic == NULL)
                ;
            else if (!strcmp(dynamic, "theora"))
            {
                msg_Dbg (demux, "assuming Theora Encoded Video");
                pt.init = theora_init;
                pt.destroy = xiph_destroy;
                pt.decode = xiph_decode;
                pt.frequency = 90000;

                free (dynamic);
                break;
            }
            else
                msg_Err (demux, "unknown dynamic payload format `%s' "
                                "specified", dynamic);
            free (dynamic);
        }

        msg_Err (demux, "unspecified payload format (type %"PRIu8")", ptype);
        msg_Info (demux, "A valid SDP is needed to parse this RTP stream.");
        vlc_dialog_display_error (demux, N_("SDP required"),
             N_("A description in SDP format is required to receive the RTP "
                "stream. Note that rtp:// URIs cannot work with dynamic "
                "RTP payload format (%"PRIu8")."), ptype);
        return;
    }
    rtp_add_type (demux, session, &pt);
}

/*
 * Dynamic payload type handlers
 * Hmm, none implemented yet apart from Xiph ones.
 */
