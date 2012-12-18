/*****************************************************************************
 * format.c : PCM format converter
 *****************************************************************************
 * Copyright (C) 2002-2005 VLC authors and VideoLAN
 * Copyright (C) 2010 Laurent Aimar
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_block.h>
#include <vlc_filter.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_description(N_("Audio filter for PCM format conversion"))
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_MISC)
    set_capability("audio converter", 1)
    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static block_t *Filter(filter_t *, block_t *);

typedef block_t *(*cvt_direct_t)(filter_t *, block_t *);
typedef void (*cvt_indirect_t)(block_t *, const block_t *);

struct filter_sys_t {
    cvt_direct_t   directs[2];
    cvt_indirect_t indirects[2];
    unsigned       indirects_ratio[2][2];
};

static cvt_direct_t FindDirect(vlc_fourcc_t src, vlc_fourcc_t dst);
static cvt_indirect_t FindIndirect(vlc_fourcc_t src, vlc_fourcc_t dst);

/* */
static int Open(vlc_object_t *object)
{
    filter_t     *filter = (filter_t *)object;

    const es_format_t *src = &filter->fmt_in;
    es_format_t       *dst = &filter->fmt_out;

    if (!AOUT_FMTS_SIMILAR(&src->audio, &dst->audio))
        return VLC_EGENERIC;
    if (src->i_codec == dst->i_codec)
        return VLC_EGENERIC;

    cvt_direct_t direct = FindDirect(src->i_codec, dst->i_codec);
    if (direct) {
        filter->pf_audio_filter = direct;
        filter->p_sys = NULL;
        goto end;
    }

    /* */
    filter_sys_t *sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    /* Find the cost minimal conversion */
    for (unsigned mask = 0; mask <= 0x01; mask ++) {
        memset(sys, 0, sizeof(*sys));

        vlc_fourcc_t fsrc = src->i_codec;
        vlc_fourcc_t fdst = dst->i_codec;

        const bool has_middle = mask & 0x01;
        for (int i = 0; fsrc != fdst && i < 1 + has_middle; i++) {
            /* XXX Hardcoded middle format: native 16 bits */
            vlc_fourcc_t ftarget = has_middle && i == 0 ? VLC_CODEC_S16N : fdst;
            sys->directs[i] = FindDirect(fsrc, ftarget);
            if (!sys->directs[i]) {
                sys->indirects[i] = FindIndirect(fsrc, ftarget);
                if (!sys->indirects[i])
                    break;
                sys->indirects_ratio[i][0] = aout_BitsPerSample(fsrc) / 8;
                sys->indirects_ratio[i][1] = aout_BitsPerSample(ftarget) / 8;
            }
            fsrc = ftarget;
        }
        if (fsrc != fdst)
            continue;

        /* We have a full conversion */
        filter->pf_audio_filter = Filter;
        filter->p_sys = sys;
        goto end;
    }
    free(sys);
    return VLC_EGENERIC;

end:
    dst->audio = src->audio;
    dst->audio.i_format = dst->i_codec;
    aout_FormatPrepare(&dst->audio);

    msg_Dbg(filter, "%4.4s->%4.4s, bits per sample: %i->%i",
            (char *)&src->i_codec, (char *)&dst->i_codec,
            src->audio.i_bitspersample, dst->audio.i_bitspersample);
    return VLC_SUCCESS;
}

/* */
static void Close(vlc_object_t *object)
{
    filter_t *filter = (filter_t *)object;
    free(filter->p_sys);
}

/* */
static block_t *Filter(filter_t *filter, block_t *block)
{
    filter_sys_t *sys = filter->p_sys;

    for (int i = 0; i < 2; i++) {
        if (sys->directs[i]) {
            block = sys->directs[i](filter, block);
        } else if (sys->indirects[i]) {
            int dst_size = sys->indirects_ratio[i][1] *
                           (block->i_buffer / sys->indirects_ratio[i][0]);
            block_t *out = filter_NewAudioBuffer(filter, dst_size);
            if (!out) {
                block_Release(block);
                return NULL;
            }
            out->i_nb_samples = block->i_nb_samples;
            out->i_dts        = block->i_dts;
            out->i_pts        = block->i_pts;
            out->i_length     = block->i_length;

            sys->indirects[i](out, block);

            block_Release(block);
            block = out;
        }
    }

    return block;
}

/* */
static block_t *S16toU8(filter_t *filter, block_t *b)
{
    VLC_UNUSED(filter);
    int16_t *src = (int16_t *)b->p_buffer;
    uint8_t *dst = (uint8_t *)src;
    for (int i = b->i_buffer / 2; i--;)
        *dst++ = ((*src++) + 32768) >> 8;

    b->i_buffer /= 2;
    return b;
}

static block_t *U16toU8(filter_t *filter, block_t *b)
{
    VLC_UNUSED(filter);
    uint16_t *src = (uint16_t *)b->p_buffer;
    uint8_t  *dst = (uint8_t *)src;
    for (int i = b->i_buffer / 2; i--;)
        *dst++ = (*src++) >> 8;

    b->i_buffer /= 2;
    return b;
}

static block_t *S16toU16(filter_t *filter, block_t *b)
{
    VLC_UNUSED(filter);
    int16_t *src = (int16_t *)b->p_buffer;
    uint16_t *dst = (uint16_t *)src;
    for (int i = b->i_buffer / 2; i--;)
        *dst++ = (*src++) + 32768;

    return b;
}

static block_t *U16toS16(filter_t *filter, block_t *b)
{
    VLC_UNUSED(filter);
    uint16_t *src = (uint16_t *)b->p_buffer;
    int16_t  *dst = (int16_t *)src;
    for (int i = b->i_buffer / 2; i--;)
        *dst++ = (int)(*src++) - 32768;

    return b;
}

static block_t *S24toS16(filter_t *filter, block_t *b)
{
    VLC_UNUSED(filter);
    uint8_t *src = (uint8_t *)b->p_buffer;
    uint8_t *dst = (uint8_t *)src;
    for (int i = b->i_buffer / 3; i--;) {
#ifdef WORDS_BIGENDIAN
        *dst++ = *src++;
        *dst++ = *src++;
        src++;
#else
        src++;
        *dst++ = *src++;
        *dst++ = *src++;
#endif
    }

    b->i_buffer = b->i_buffer * 2 / 3;
    return b;
}
static block_t *S32toS16(filter_t *filter, block_t *b)
{
    VLC_UNUSED(filter);
    int32_t *src = (int32_t *)b->p_buffer;
    int16_t *dst = (int16_t *)src;
    for (int i = b->i_buffer / 4; i--;)
        *dst++ = (*src++) >> 16;

    b->i_buffer /= 2;
    return b;
}
static block_t *Fl32toS16(filter_t *filter, block_t *b)
{
    VLC_UNUSED(filter);
    float   *src = (float *)b->p_buffer;
    int16_t *dst = (int16_t *)src;
    for (int i = b->i_buffer / 4; i--;) {
#if 0
        /* Slow version. */
        if (*src >= 1.0) *dst = 32767;
        else if (*src < -1.0) *dst = -32768;
        else *dst = *src * 32768.0;
        src++; dst++;
#else
        /* This is walken's trick based on IEEE float format. */
        union { float f; int32_t i; } u;
        u.f = *src++ + 384.0;
        if (u.i > 0x43c07fff)
            *dst++ = 32767;
        else if (u.i < 0x43bf8000)
            *dst++ = -32768;
        else
            *dst++ = u.i - 0x43c00000;
#endif
    }

    b->i_buffer /= 2;
    return b;
}
static block_t *Fl64toS16(filter_t *filter, block_t *b)
{
    VLC_UNUSED(filter);
    double  *src = (double *)b->p_buffer;
    int16_t *dst = (int16_t *)src;
    for (int i = b->i_buffer / 8; i--;) {
        const double v = *src++;
        /* Slow version. */
        if (v >= 1.0)
            *dst++ = 32767;
        else if (v < -1.0)
            *dst++ = -32768;
        else
            *dst++ = v * 32768.0;
    }

    b->i_buffer /= 4;
    return b;
}
static block_t *S32toFl32(filter_t *filter, block_t *b)
{
    VLC_UNUSED(filter);
    int32_t *src = (int32_t*)b->p_buffer;
    float   *dst = (float *)src;
    for (int i = b->i_buffer / 4; i--;)
        *dst++ = (float)(*src++) / 2147483648.0;
    return b;
}
static block_t *Fi32toFl32(filter_t *filter, block_t *b)
{
    VLC_UNUSED(filter);
    vlc_fixed_t *src = (vlc_fixed_t *)b->p_buffer;
    float       *dst = (float *)src;
    for (int i = b->i_buffer / 4; i--;)
        *dst++ = *src++ / (float)FIXED32_ONE;
    return b;
}
static block_t *Fi32toS16(filter_t *filter, block_t *b)
{
    VLC_UNUSED(filter);
    vlc_fixed_t *src = (vlc_fixed_t *)b->p_buffer;
    int16_t     *dst = (int16_t *)src;
    for (int i = b->i_buffer / 4; i--;) {
        const vlc_fixed_t v = *src++;
        if (v >= FIXED32_ONE)
            *dst++ = INT16_MAX;
        else if (v <= -FIXED32_ONE)
            *dst++ = INT16_MIN;
        else
            *dst++ = v >> (32 - FIXED32_FRACBITS);
    }
    b->i_buffer /= 2;
    return b;
}

/* */
static void X8toX16(block_t *bdst, const block_t *bsrc)
{
    uint8_t  *src = (uint8_t *)bsrc->p_buffer;
    uint16_t *dst = (uint16_t *)bdst->p_buffer;
    for (int i = bsrc->i_buffer; i--;)
        *dst++ = (*src++) << 8;
}

static void U8toS16(block_t *bdst, const block_t *bsrc)
{
    uint8_t *src = (uint8_t *)bsrc->p_buffer;
    int16_t *dst = (int16_t *)bdst->p_buffer;
    for (int i = bsrc->i_buffer; i--;)
        *dst++ = ((*src++) - 128) << 8;
}

static void S16toS24(block_t *bdst, const block_t *bsrc)
{
    uint8_t *src = (uint8_t *)bsrc->p_buffer;
    uint8_t *dst = (uint8_t *)bdst->p_buffer;

    for (int i = bsrc->i_buffer / 2; i--;) {
#ifdef WORDS_BIGENDIAN
        *dst++ = *src++;
        *dst++ = *src++;
        *dst++ = 0;
#else
        *dst++ = 0;
        *dst++ = *src++;
        *dst++ = *src++;
#endif
    }
}
static void S16toS32(block_t *bdst, const block_t *bsrc)
{
    int16_t *src = (int16_t *)bsrc->p_buffer;
    int32_t *dst = (int32_t *)bdst->p_buffer;
    for (int i = bsrc->i_buffer / 2; i--;)
        *dst++ = *src++ << 16;
}
static void S16toFl32(block_t *bdst, const block_t *bsrc)
{
    int16_t *src = (int16_t *)bsrc->p_buffer;
    float *dst = (float *)bdst->p_buffer;
    for (int i = bsrc->i_buffer / 2; i--;) {
#if 0
        /* Slow version */
        *dst++ = (float)*src++ / 32768.0;
#else
        /* This is walken's trick based on IEEE float format. On my PIII
         * this takes 16 seconds to perform one billion conversions, instead
         * of 19 seconds for the above division. */
        union { float f; int32_t i; } u;
        u.i = *src++ + 0x43c00000;
        *dst++ = u.f - 384.0;
#endif
    }
}
static void S24toFl32(block_t *bdst, const block_t *bsrc)
{
    uint8_t *src = bsrc->p_buffer;
    float   *dst = (float *)bdst->p_buffer;
    for (int i = bsrc->i_buffer / 3; i--;) {
#ifdef WORDS_BIGENDIAN
        int32_t v = (src[0] << 24) | (src[1] << 16) | (src[2] <<  8);
#else
        int32_t v = (src[0] <<  8) | (src[1] << 16) | (src[2] << 24);
#endif
        src += 3;
        *dst++ = v / 2147483648.0;
    }
}

/* */
static const struct {
    vlc_fourcc_t src;
    vlc_fourcc_t dst;
    cvt_direct_t convert;
} cvt_directs[] = {
    { VLC_CODEC_FL64, VLC_CODEC_S16N,   Fl64toS16 },
    { VLC_CODEC_FI32, VLC_CODEC_FL32,   Fi32toFl32 },
    { VLC_CODEC_FI32, VLC_CODEC_S16N,   Fi32toS16 },
    { VLC_CODEC_S32N, VLC_CODEC_FL32,   S32toFl32 },

    { VLC_CODEC_S24N, VLC_CODEC_S16N,   S24toS16 },
    { VLC_CODEC_S32N, VLC_CODEC_S32N,   S32toS16 },
    { VLC_CODEC_FL32, VLC_CODEC_S16N,   Fl32toS16 },

    { VLC_CODEC_S16N, VLC_CODEC_U8,     S16toU8 },
    { VLC_CODEC_S16N, VLC_CODEC_U16N,   S16toU16 },

    { VLC_CODEC_U16N, VLC_CODEC_U8,     U16toU8 },
    { VLC_CODEC_U16N, VLC_CODEC_S16N,   U16toS16 },

    { 0, 0, NULL }
};

static const struct {
    vlc_fourcc_t   src;
    vlc_fourcc_t   dst;
    cvt_indirect_t convert;
} cvt_indirects[] = {
    { VLC_CODEC_S24N, VLC_CODEC_FL32, S24toFl32 },

    { VLC_CODEC_S16N, VLC_CODEC_S24N, S16toS24 },
    { VLC_CODEC_S16N, VLC_CODEC_S32N, S16toS32 },
    { VLC_CODEC_S16N, VLC_CODEC_FL32, S16toFl32 },

    { VLC_CODEC_U8,   VLC_CODEC_U16N, X8toX16 },
    { VLC_CODEC_U8,   VLC_CODEC_S16N, U8toS16 },
    { 0, 0, NULL }
};

static cvt_direct_t FindDirect(vlc_fourcc_t src, vlc_fourcc_t dst)
{
    for (int i = 0; cvt_directs[i].convert; i++) {
        if (cvt_directs[i].src == src &&
            cvt_directs[i].dst == dst)
            return cvt_directs[i].convert;
    }
    return NULL;
}
static cvt_indirect_t FindIndirect(vlc_fourcc_t src, vlc_fourcc_t dst)
{
    for (int i = 0; cvt_indirects[i].convert; i++) {
        if (cvt_indirects[i].src == src &&
            cvt_indirects[i].dst == dst)
            return cvt_indirects[i].convert;
    }
    return NULL;
}

