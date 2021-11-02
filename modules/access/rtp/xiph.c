/**
 * @file xiph.c
 * @brief Real-Time Protocol (RTP) Xiph payloads receival
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
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_es.h>
#include <vlc_plugin.h>

#include "../../demux/xiph.h"
#include "rtp.h"

struct rtp_xiph {
    vlc_object_t *obj;
    enum es_format_category_e cat;
    vlc_fourcc_t fourcc;
};

struct rtp_xiph_source {
    struct vlc_rtp_es *id;
    block_t     *block;
    uint32_t     ident;
};

static void *xiph_init(struct vlc_rtp_pt *pt)
{
    struct rtp_xiph_source *self = malloc(sizeof (*self));

    if (self == NULL)
        return NULL;

    self->id = NULL;
    self->block = NULL;
    self->ident = 0xffffffff; /* impossible value on the wire */
    (void) pt;
    return self;
}

static void xiph_destroy(struct vlc_rtp_pt *pt, void *data)
{
    struct rtp_xiph_source *self = data;

    if (!data)
        return;
    if (self->block)
    {
        self->block->i_flags |= BLOCK_FLAG_CORRUPTED;
        vlc_rtp_es_send(self->id, self->block);
    }
    vlc_rtp_es_destroy(self->id);
    free (self);
    (void) pt;
}

/* Convert configuration from RTP to VLC format */
static ssize_t xiph_header (void **pextra, const uint8_t *buf, size_t len)
{
    /* Headers number */
    if (len == 0)
          return -1; /* Invalid */
    unsigned hcount = 1 + *buf++;
    len--;
    if (hcount != 3)
          return -1; /* Invalid */

    /* Header lengths */
    uint16_t idlen = 0, cmtlen = 0, setuplen = 0;
    do
    {
        if (len == 0)
            return -1;
        idlen = (idlen << 7) | (*buf & 0x7f);
        len--;
    }
    while (*buf++ & 0x80);
    do
    {
        if (len == 0)
            return -1;
        cmtlen = (cmtlen << 7) | (*buf & 0x7f);
        len--;
    }
    while (*buf++ & 0x80);
    if (len < idlen + cmtlen)
        return -1;
    setuplen = len - (idlen + cmtlen);

    /* Create the VLC extra format header */
    unsigned sizes[3] = {
        idlen, cmtlen, setuplen
    };
    const void *payloads[3] = {
        buf + 0,
        buf + idlen,
        buf + idlen + cmtlen
    };
    void *extra;
    int  extra_size;
    if (xiph_PackHeaders (&extra_size, &extra, sizes, payloads, 3))
        return -1;;
    *pextra = extra;
    return extra_size;
}

static void xiph_decode(struct vlc_rtp_pt *pt, void *data, block_t *block,
                        const struct vlc_rtp_pktinfo *restrict info)
{
    struct rtp_xiph_source *self = data;
    struct rtp_xiph *sys = pt->opaque;

    if (!data || block->i_buffer < 4)
        goto drop;

    /* 32-bits RTP header (§2.2) */
    uint32_t ident = GetDWBE (block->p_buffer);
    block->i_buffer -= 4;
    block->p_buffer += 4;

    unsigned fragtype = (ident >> 6) & 3;
    unsigned datatype = (ident >> 4) & 3;
    unsigned pkts = (ident) & 15;
    ident >>= 8;

    /* RTP defragmentation */
    if (self->block && (block->i_flags & BLOCK_FLAG_DISCONTINUITY))
    {   /* Screwed! discontinuity within a fragmented packet */
        msg_Warn(sys->obj, "discontinuity in fragmented Xiph packet");
        block_Release (self->block);
        self->block = NULL;
    }

    if (fragtype <= 1)
    {
        if (self->block) /* Invalid first fragment */
        {
            block_Release (self->block);
            self->block = NULL;
        }
    }
    else
    {
        if (!self->block)
            goto drop; /* Invalid non-first fragment */
    }

    if (fragtype > 0)
    {   /* Fragment */
        if (pkts > 0 || block->i_buffer < 2)
            goto drop;

        size_t fraglen = GetWBE (block->p_buffer);
        if (block->i_buffer < (fraglen + 2))
            goto drop; /* Invalid payload length */
        block->i_buffer = fraglen;
        if (fragtype == 1)/* Keep first fragment */
        {
            block->i_buffer += 2;
            self->block = block;
        }
        else
        {   /* Append non-first fragment */
            size_t len = self->block->i_buffer;
            self->block = block_Realloc (self->block, 0, len + fraglen);
            if (!self->block)
            {
                block_Release (block);
                return;
            }
            memcpy (self->block->p_buffer + len, block->p_buffer + 2,
                    fraglen);
            block_Release (block);
        }
        if (fragtype < 3)
            return; /* Non-last fragment */

        /* Last fragment reached, process it */
        block = self->block;
        self->block = NULL;
        SetWBE (block->p_buffer, block->i_buffer - 2);
        pkts = 1;
    }

    /* RTP payload packets processing */
    while (pkts > 0)
    {
        if (block->i_buffer < 2)
            goto drop;

        size_t len = GetWBE (block->p_buffer);
        block->i_buffer -= 2;
        block->p_buffer += 2;
        if (block->i_buffer < len)
            goto drop;

        switch (datatype)
        {
            case 0: /* Raw payload */
            {
                if (self->ident != ident)
                {
                    msg_Warn(sys->obj,
                             "ignoring raw payload without configuration");
                    break;
                }
                block_t *raw = block_Alloc (len);
                memcpy (raw->p_buffer, block->p_buffer, len);
                raw->i_pts = block->i_pts; /* FIXME: what about pkts > 1 */
                block->i_dts = VLC_TICK_INVALID;
                vlc_rtp_es_send(self->id, raw);
                break;
            }

            case 1: /* Packed configuration frame (§3.1.1) */
            {
                if (self->ident == ident)
                    break; /* Ignore config retransmission */

                void *extv;
                ssize_t extc = xiph_header (&extv, block->p_buffer, len);
                if (extc < 0)
                    break;

                es_format_t fmt;
                es_format_Init(&fmt, sys->cat, sys->fourcc);
                fmt.p_extra = extv;
                fmt.i_extra = extc;
                vlc_rtp_es_destroy(self->id);
                msg_Dbg(sys->obj,
                        "packed configuration received (%06"PRIx32")", ident);
                self->ident = ident;
                self->id = vlc_rtp_pt_request_es(pt, &fmt);
                break;
            }
        }

        block->i_buffer -= len;
        block->p_buffer += len;
        pkts--;
    }

    (void) info;
drop:
    block_Release (block);
}

static void xiph_release(struct vlc_rtp_pt *pt)
{
    free(pt->opaque);
}

static const struct vlc_rtp_pt_operations rtp_xiph_ops = {
    xiph_release, xiph_init, xiph_destroy, xiph_decode,
};

static int xiph_open(vlc_object_t *obj, struct vlc_rtp_pt *pt,
                     const struct vlc_sdp_pt *desc,
                     int cat, vlc_fourcc_t fourcc)
{
    struct rtp_xiph *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->obj = obj;
    sys->cat = cat;
    sys->fourcc = fourcc;
    pt->opaque = sys;
    pt->ops = &rtp_xiph_ops;
    (void) desc;
    return VLC_SUCCESS;
}

/* Xiph Vorbis audio (RFC 5215) */
static int vorbis_open(vlc_object_t *obj, struct vlc_rtp_pt *pt,
                       const struct vlc_sdp_pt *desc)
{
    return xiph_open(obj, pt, desc, AUDIO_ES, VLC_CODEC_VORBIS);
}

/* Xiph Theora video (I-D draft-barbato-avt-rtp-theora-01) */
static int theora_open(vlc_object_t *obj, struct vlc_rtp_pt *pt,
                       const struct vlc_sdp_pt *desc)
{
    return xiph_open(obj, pt, desc, VIDEO_ES, VLC_CODEC_THEORA);
}

vlc_module_begin()
    set_shortname(N_("RTP Xiph"))
    set_description(N_("RTP Xiph payload parser"))
    set_subcategory(SUBCAT_INPUT_DEMUX)
    set_capability("rtp audio parser", 0)
    set_callback(vorbis_open)
    add_shortcut("vorbis")

    add_submodule()
    set_capability("rtp video parser", 0)
    set_callback(theora_open)
    add_shortcut("theora")
vlc_module_end()
