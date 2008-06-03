/**
 * @file rtp.c
 * @brief Real-Time Protocol (RTP) demux module for VLC media player
 */
/*****************************************************************************
 * Copyright (C) 2001-2005 the VideoLAN team
 * Copyright © 2007-2008 Rémi Denis-Courmont
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
#include <stdarg.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_aout.h>
#include <vlc_network.h>
#include <vlc_plugin.h>

#include <vlc_codecs.h>

#include "rtp.h"

#define RTP_CACHING_TEXT N_("RTP de-jitter buffer length (msec)")
#define RTP_CACHING_LONGTEXT N_( \
    "How long to wait for late RTP packets (and delay the performance)." )

#define RTP_MAX_SRC_TEXT N_("Maximum RTP sources")
#define RTP_MAX_SRC_LONGTEXT N_( \
    "How many distinct active RTP sources are allowed at a time." )

#define RTP_TIMEOUT_TEXT N_("RTP source timeout (sec)")
#define RTP_TIMEOUT_LONGTEXT N_( \
    "How long to wait for any packet before a source is expired.")

#define RTP_MAX_DROPOUT_TEXT N_("Maximum RTP sequence number dropout")
#define RTP_MAX_DROPOUT_LONGTEXT N_( \
    "RTP packets will be discarded if they are too much ahead (i.e. in the " \
    "future) by this many packets from the last received packet." )

#define RTP_MAX_MISORDER_TEXT N_("Maximum RTP sequence number misordering")
#define RTP_MAX_MISORDER_LONGTEXT N_( \
    "RTP packets will be discarded if they are too far behind (i.e. in the " \
    "past) by this many packets from the last received packet." )

static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);

/*
 * Module descriptor
 */
vlc_module_begin ();
    set_shortname (_("RTP"));
    set_description (_("(Experimental) Real-Time Protocol demuxer"));
    set_category (CAT_INPUT);
    set_subcategory (SUBCAT_INPUT_DEMUX);
    set_capability ("access_demux", 10);
    set_callbacks (Open, Close);

    add_integer ("rtp-caching", 1000, NULL, RTP_CACHING_TEXT,
                 RTP_CACHING_LONGTEXT, true);
        change_integer_range (0, 65535);
    add_integer ("rtp-max-src", 1, NULL, RTP_MAX_SRC_TEXT,
                 RTP_MAX_SRC_LONGTEXT, true);
        change_integer_range (1, 255);
    add_integer ("rtp-timeout", 5, NULL, RTP_TIMEOUT_TEXT,
                 RTP_TIMEOUT_LONGTEXT, true);
    add_integer ("rtp-max-dropout", 3000, NULL, RTP_MAX_DROPOUT_TEXT,
                 RTP_MAX_DROPOUT_LONGTEXT, true);
        change_integer_range (0, 32767);
    add_integer ("rtp-max-misorder", 100, NULL, RTP_MAX_MISORDER_TEXT,
                 RTP_MAX_MISORDER_LONGTEXT, true);
        change_integer_range (0, 32767);

    add_shortcut ("rtp");
vlc_module_end ();

/*
 * TODO: so much stuff
 * - send RTCP-RR and RTCP-BYE
 * - dynamic payload types (need SDP parser)
 * - multiple medias (need SDP parser, and RTCP-SR parser for lip-sync)
 * - support for access_filter in case of stream_Demux (MPEG-TS)
 */

/*
 * Local prototypes
 */
static int Demux (demux_t *);
static int Control (demux_t *, int i_query, va_list args);
static int extract_port (char **phost);

/**
 * Probes and initializes.
 */
static int Open (vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;

    if (strcmp (demux->psz_access, "rtp"))
        return VLC_EGENERIC;

    char *tmp = strdup (demux->psz_path);
    char *shost = tmp;
    if (shost == NULL)
        return VLC_ENOMEM;

    char *dhost = strchr (shost, '@');
    if (dhost)
        *dhost++ = '\0';

    /* Parses the port numbers */
    int sport = 0, dport = 0;
    sport = extract_port (&shost);
    if (dhost != NULL)
        dport = extract_port (&dhost);
    if (dport == 0)
        dport = 5004; /* avt-profile-1 port */

    /* Try to connect */
    int fd = net_OpenDgram (obj, dhost, dport, shost, sport,
                            AF_UNSPEC, IPPROTO_UDP);
    free (tmp);
    if (fd == -1)
        return VLC_EGENERIC;

    /* Initializes demux */
    demux_sys_t *p_sys = malloc (sizeof (*p_sys));
    if (p_sys == NULL)
        goto error;

    p_sys->caching      = var_CreateGetInteger (obj, "rtp-caching");
    p_sys->max_src      = var_CreateGetInteger (obj, "rtp-max-src");
    p_sys->timeout      = var_CreateGetInteger (obj, "rtp-timeout");
    p_sys->max_dropout  = var_CreateGetInteger (obj, "rtp-max-dropout");
    p_sys->max_misorder = var_CreateGetInteger (obj, "rtp-max-misorder");
    p_sys->autodetect   = true;

    demux->pf_demux   = Demux;
    demux->pf_control = Control;
    demux->p_sys      = p_sys;

    p_sys->session = rtp_session_create (demux);
    if (p_sys->session == NULL)
        goto error;

    p_sys->fd = fd;
    return VLC_SUCCESS;

error:
    net_Close (fd);
    free (p_sys);
    return VLC_EGENERIC;
}


/**
 * Releases resources
 */
static void Close (vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *p_sys = demux->p_sys;

    rtp_session_destroy (demux, p_sys->session);
    net_Close (p_sys->fd);
    free (p_sys);
}


/**
 * Extracts port number from "[host]:port" or "host:port" strings,
 * and remove brackets from the host name.
 * @param phost pointer to the string upon entry,
 * pointer to the hostname upon return.
 * @return port number, 0 if missing.
 */
static int extract_port (char **phost)
{
    char *host = *phost, *port;

    if (host[0] == '[')
    {
        host = *++phost; /* skip '[' */
        port = strchr (host, ']');
        if (port)
            *port++ = '\0'; /* skip ']' */
    }
    else
        port = strchr (host, ':');

    if (port == NULL)
        return 0;
    *port++ = '\0'; /* skip ':' */
    return atoi (port);
}


/**
 * Control callback
 */
static int Control (demux_t *demux, int i_query, va_list args)
{
    demux_sys_t *p_sys = demux->p_sys;

    switch (i_query)
    {
        case DEMUX_GET_POSITION:
        {
            float *v = va_arg (args, float *);
            *v = 0.;
            return 0;
        }

        case DEMUX_GET_LENGTH:
        case DEMUX_GET_TIME:
        {
            int64_t *v = va_arg (args, int64_t *);
            *v = 0;
            return 0;
        }

        case DEMUX_GET_PTS_DELAY:
        {
            int64_t *v = va_arg (args, int64_t *);
            *v = p_sys->caching;
            return 0;
        }
    }

    return VLC_EGENERIC;
}


/**
 * Gets a datagram from the network
 */
static block_t *rtp_dgram_recv (demux_t *demux, int fd)
{
    block_t *block = block_Alloc (0xffff);

    ssize_t len = net_Read (VLC_OBJECT (demux), fd, NULL,
                            block->p_buffer, block->i_buffer, false);
    if (len == -1)
    {
        block_Release (block);
        return NULL;
    }
    return block_Realloc (block, 0, len);
}


/*
 * Generic packet handlers
 */

static void *codec_init (demux_t *demux, es_format_t *fmt)
{
    return es_out_Add (demux->out, fmt);
}

static void codec_destroy (demux_t *demux, void *data)
{
    if (data)
        es_out_Del (demux->out, (es_out_id_t *)data);
}

/* Send a packet to decoder */
static void codec_decode (demux_t *demux, void *data, block_t *block)
{
    if (data)
    {
        block->i_dts = 0; /* RTP does not specify this */
        es_out_Control (demux->out, ES_OUT_SET_PCR,
                        block->i_pts - demux->p_sys->caching * 1000);
        es_out_Send (demux->out, (es_out_id_t *)data, block);
    }
    else
        block_Release (block);
}


static void *stream_init (demux_t *demux, const char *name)
{
    return stream_DemuxNew (demux, name, demux->out);
}

static void stream_destroy (demux_t *demux, void *data)
{
    if (data)
        stream_DemuxDelete ((stream_t *)data);
    (void)demux;
}

/* Send a packet to a chained demuxer */
static void stream_decode (demux_t *demux, void *data, block_t *block)
{
    if (data)
        stream_DemuxSend ((stream_t *)data, block);
    else
        block_Release (block);
    (void)demux;
}

/*
 * Static payload types handler
 */

/* PT=0
 * PCMU:
 */
static void *pcmu_init (demux_t *demux)
{
    es_format_t fmt;

    es_format_Init (&fmt, AUDIO_ES, VLC_FOURCC ('u', 'l', 'a', 'w'));
    fmt.audio.i_rate = 8000;
    fmt.audio.i_channels = 1;
    return codec_init (demux, &fmt);
}

/* PT=8
 * PCMA:
 */
static void *pcma_init (demux_t *demux)
{
    es_format_t fmt;

    es_format_Init (&fmt, AUDIO_ES, VLC_FOURCC ('a', 'l', 'a', 'w'));
    fmt.audio.i_rate = 8000;
    fmt.audio.i_channels = 1;
    return codec_init (demux, &fmt);
}

/* PT=14
 * MPA: MPEG Audio (RFC2250, §3.4)
 */
static void *mpa_init (demux_t *demux)
{
    es_format_t fmt;

    es_format_Init (&fmt, AUDIO_ES, VLC_FOURCC ('m', 'p', 'g', 'a'));
    fmt.audio.i_channels = 2;
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

    es_format_Init (&fmt, VIDEO_ES, VLC_FOURCC ('m', 'p', 'g', 'v'));
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


/*
 * Dynamic payload type handlers
 * Hmm, none implemented yet.
 */

/**
 * Processing callback
 */
static int Demux (demux_t *demux)
{
    demux_sys_t *p_sys = demux->p_sys;
    block_t     *block;

    block = rtp_dgram_recv (demux, p_sys->fd);
    if (block)
    {
        /* Not using SDP, we need to guess the payload format used */
        if (p_sys->autodetect && block->i_buffer >= 2)
        {
            rtp_pt_t pt = {
                .init = NULL,
                .destroy = codec_destroy,
                .decode = codec_decode,
                .frequency = 0,
                .number = block->p_buffer[1] & 0x7f,
            };

            switch (pt.number)
            {
              case 0:
                msg_Dbg (demux, "detected G.711 mu-law");
                pt.init = pcmu_init;
                pt.frequency = 8000;
                break;

              case 8:
                msg_Dbg (demux, "detected G.711 A-law");
                pt.init = pcma_init;
                pt.frequency = 8000;
                break;

              case 14:
                msg_Dbg (demux, "detected MPEG Audio");
                pt.init = mpa_init;
                pt.decode = mpa_decode;
                pt.frequency = 44100;
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
                pt.decode = stream_decode;
                pt.frequency = 90000;
                break;

              case 72: /* muxed SR */
              case 73: /* muxed RR */
              case 74: /* muxed SDES */
              case 75: /* muxed BYE */
              case 76: /* muxed APP */
              default:
                block_Release (block); /* ooh! ignoring RTCP is evil! */
                return 1;
            }
            rtp_add_type (demux, p_sys->session, &pt);
            p_sys->autodetect = false;
        }

        rtp_receive (demux, p_sys->session, block);
    }

    return 1;
}
