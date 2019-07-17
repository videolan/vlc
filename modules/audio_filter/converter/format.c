/*****************************************************************************
 * format.c : PCM format converter
 *****************************************************************************
 * Copyright (C) 2002-2005 VLC authors and VideoLAN
 * Copyright (C) 2010 Laurent Aimar
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
#include <math.h>
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

vlc_module_begin()
    set_description(N_("Audio filter for PCM format conversion"))
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_MISC)
    set_capability("audio converter", 1)
    set_callback(Open)
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef block_t *(*cvt_t)(filter_t *, block_t *);
static cvt_t FindConversion(vlc_fourcc_t src, vlc_fourcc_t dst);

static int Open(vlc_object_t *object)
{
    filter_t     *filter = (filter_t *)object;

    const es_format_t *src = &filter->fmt_in;
    es_format_t       *dst = &filter->fmt_out;

    if (!AOUT_FMTS_SIMILAR(&src->audio, &dst->audio))
        return VLC_EGENERIC;
    if (src->i_codec == dst->i_codec)
        return VLC_EGENERIC;

    filter->pf_audio_filter = FindConversion(src->i_codec, dst->i_codec);
    if (filter->pf_audio_filter == NULL)
        return VLC_EGENERIC;

    msg_Dbg(filter, "%4.4s->%4.4s, bits per sample: %i->%i",
            (char *)&src->i_codec, (char *)&dst->i_codec,
            src->audio.i_bitspersample, dst->audio.i_bitspersample);
    return VLC_SUCCESS;
}


/*** from U8 ***/
static block_t *U8toS16(filter_t *filter, block_t *bsrc)
{
    block_t *bdst = block_Alloc(bsrc->i_buffer * 2);
    if (unlikely(bdst == NULL))
        goto out;

    block_CopyProperties(bdst, bsrc);
    uint8_t *src = (uint8_t *)bsrc->p_buffer;
    int16_t *dst = (int16_t *)bdst->p_buffer;
    for (size_t i = bsrc->i_buffer; i--;)
        *dst++ = ((*src++) << 8) - 0x8000;
out:
    block_Release(bsrc);
    VLC_UNUSED(filter);
    return bdst;
}

static block_t *U8toFl32(filter_t *filter, block_t *bsrc)
{
    block_t *bdst = block_Alloc(bsrc->i_buffer * 4);
    if (unlikely(bdst == NULL))
        goto out;

    block_CopyProperties(bdst, bsrc);
    uint8_t *src = (uint8_t *)bsrc->p_buffer;
    float   *dst = (float *)bdst->p_buffer;
    for (size_t i = bsrc->i_buffer; i--;)
        *dst++ = ((float)((*src++) - 128)) / 128.f;
out:
    block_Release(bsrc);
    VLC_UNUSED(filter);
    return bdst;
}

static block_t *U8toS32(filter_t *filter, block_t *bsrc)
{
    block_t *bdst = block_Alloc(bsrc->i_buffer * 4);
    if (unlikely(bdst == NULL))
        goto out;

    block_CopyProperties(bdst, bsrc);
    uint8_t *src = (uint8_t *)bsrc->p_buffer;
    int32_t *dst = (int32_t *)bdst->p_buffer;
    for (size_t i = bsrc->i_buffer; i--;)
        *dst++ = ((*src++) << 24) - 0x80000000;
out:
    block_Release(bsrc);
    VLC_UNUSED(filter);
    return bdst;
}

static block_t *U8toFl64(filter_t *filter, block_t *bsrc)
{
    block_t *bdst = block_Alloc(bsrc->i_buffer * 8);
    if (unlikely(bdst == NULL))
        goto out;

    block_CopyProperties(bdst, bsrc);
    uint8_t *src = (uint8_t *)bsrc->p_buffer;
    double  *dst = (double *)bdst->p_buffer;
    for (size_t i = bsrc->i_buffer; i--;)
        *dst++ = ((double)((*src++) - 128)) / 128.;
out:
    block_Release(bsrc);
    VLC_UNUSED(filter);
    return bdst;
}


/*** from S16N ***/
static block_t *S16toU8(filter_t *filter, block_t *b)
{
    VLC_UNUSED(filter);
    int16_t *src = (int16_t *)b->p_buffer;
    uint8_t *dst = (uint8_t *)src;
    for (size_t i = b->i_buffer / 2; i--;)
        *dst++ = ((*src++) + 32768) >> 8;

    b->i_buffer /= 2;
    return b;
}

static block_t *S16toFl32(filter_t *filter, block_t *bsrc)
{
    block_t *bdst = block_Alloc(bsrc->i_buffer * 2);
    if (unlikely(bdst == NULL))
        goto out;

    block_CopyProperties(bdst, bsrc);
    int16_t *src = (int16_t *)bsrc->p_buffer;
    float   *dst = (float *)bdst->p_buffer;
    for (size_t i = bsrc->i_buffer / 2; i--;)
#if 0
        /* Slow version */
        *dst++ = (float)*src++ / 32768.f;
#else
    {   /* This is Walken's trick based on IEEE float format. On my PIII
         * this takes 16 seconds to perform one billion conversions, instead
         * of 19 seconds for the above division. */
        union { float f; int32_t i; } u;
        u.i = *src++ + 0x43c00000;
        *dst++ = u.f - 384.f;
    }
#endif
out:
    block_Release(bsrc);
    VLC_UNUSED(filter);
    return bdst;
}

static block_t *S16toS32(filter_t *filter, block_t *bsrc)
{
    block_t *bdst = block_Alloc(bsrc->i_buffer * 2);
    if (unlikely(bdst == NULL))
        goto out;

    block_CopyProperties(bdst, bsrc);
    int16_t *src = (int16_t *)bsrc->p_buffer;
    int32_t *dst = (int32_t *)bdst->p_buffer;
    for (int i = bsrc->i_buffer / 2; i--;)
        *dst++ = *src++ << 16;
out:
    block_Release(bsrc);
    VLC_UNUSED(filter);
    return bdst;
}

static block_t *S16toFl64(filter_t *filter, block_t *bsrc)
{
    block_t *bdst = block_Alloc(bsrc->i_buffer * 4);
    if (unlikely(bdst == NULL))
        goto out;

    block_CopyProperties(bdst, bsrc);
    int16_t *src = (int16_t *)bsrc->p_buffer;
    float   *dst = (float *)bdst->p_buffer;
    for (size_t i = bsrc->i_buffer / 2; i--;)
        *dst++ = (double)*src++ / 32768.;
out:
    block_Release(bsrc);
    VLC_UNUSED(filter);
    return bdst;
}


/*** from FL32 ***/
static block_t *Fl32toU8(filter_t *filter, block_t *b)
{
    float   *src = (float *)b->p_buffer;
    uint8_t *dst = (uint8_t *)src;
    for (size_t i = b->i_buffer / 4; i--;)
    {
        float s = *(src++) * 128.f;
        if (s >= 127.f)
            *(dst++) = 255;
        else
        if (s <= -128.f)
            *(dst++) = 0;
        else
            *(dst++) = lroundf(s) + 128;
    }
    b->i_buffer /= 4;
    VLC_UNUSED(filter);
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
        else *dst = lroundf(*src * 32768.f);
        src++; dst++;
#else
        /* This is Walken's trick based on IEEE float format. */
        union { float f; int32_t i; } u;
        u.f = *src++ + 384.f;
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

static block_t *Fl32toS32(filter_t *filter, block_t *b)
{
    float   *src = (float *)b->p_buffer;
    int32_t *dst = (int32_t *)src;
    for (size_t i = b->i_buffer / 4; i--;)
    {
        float s = *(src++) * 2147483648.f;
        if (s >= 2147483647.f)
            *(dst++) = 2147483647;
        else
        if (s <= -2147483648.f)
            *(dst++) = -2147483648;
        else
            *(dst++) = lroundf(s);
    }
    VLC_UNUSED(filter);
    return b;
}

static block_t *Fl32toFl64(filter_t *filter, block_t *bsrc)
{
    block_t *bdst = block_Alloc(bsrc->i_buffer * 2);
    if (unlikely(bdst == NULL))
        goto out;

    block_CopyProperties(bdst, bsrc);
    float  *src = (float *)bsrc->p_buffer;
    double *dst = (double *)bdst->p_buffer;
    for (size_t i = bsrc->i_buffer / 4; i--;)
        *(dst++) = *(src++);
out:
    block_Release(bsrc);
    VLC_UNUSED(filter);
    return bdst;
}


/*** from S32N ***/
static block_t *S32toU8(filter_t *filter, block_t *b)
{
    VLC_UNUSED(filter);
    int32_t *src = (int32_t *)b->p_buffer;
    uint8_t *dst = (uint8_t *)src;
    for (size_t i = b->i_buffer / 4; i--;)
        *dst++ = ((*src++) >> 24) + 128;

    b->i_buffer /= 4;
    return b;
}

static block_t *S32toS16(filter_t *filter, block_t *b)
{
    VLC_UNUSED(filter);
    int32_t *src = (int32_t *)b->p_buffer;
    int16_t *dst = (int16_t *)src;
    for (size_t i = b->i_buffer / 4; i--;)
        *dst++ = (*src++) >> 16;

    b->i_buffer /= 2;
    return b;
}

static block_t *S32toFl32(filter_t *filter, block_t *b)
{
    VLC_UNUSED(filter);
    int32_t *src = (int32_t*)b->p_buffer;
    float   *dst = (float *)src;
    for (int i = b->i_buffer / 4; i--;)
        *dst++ = (float)(*src++) / 2147483648.f;
    return b;
}

static block_t *S32toFl64(filter_t *filter, block_t *bsrc)
{
    block_t *bdst = block_Alloc(bsrc->i_buffer * 2);
    if (unlikely(bdst == NULL))
        goto out;

    block_CopyProperties(bdst, bsrc);
    int32_t *src = (int32_t*)bsrc->p_buffer;
    double  *dst = (double *)bdst->p_buffer;
    for (size_t i = bsrc->i_buffer / 4; i--;)
        *dst++ = (double)(*src++) / 2147483648.;
out:
    VLC_UNUSED(filter);
    block_Release(bsrc);
    return bdst;
}


/*** from FL64 ***/
static block_t *Fl64toU8(filter_t *filter, block_t *b)
{
    double  *src = (double *)b->p_buffer;
    uint8_t *dst = (uint8_t *)src;
    for (size_t i = b->i_buffer / 8; i--;)
    {
        float s = *(src++) * 128.;
        if (s >= 127.f)
            *(dst++) = 255;
        else
        if (s <= -128.f)
            *(dst++) = 0;
        else
            *(dst++) = lround(s) + 128;
    }
    b->i_buffer /= 8;
    VLC_UNUSED(filter);
    return b;
}

static block_t *Fl64toS16(filter_t *filter, block_t *b)
{
    VLC_UNUSED(filter);
    double  *src = (double *)b->p_buffer;
    int16_t *dst = (int16_t *)src;
    for (size_t i = b->i_buffer / 8; i--;) {
        const double v = *src++ * 32768.;
        /* Slow version. */
        if (v >= 32767.)
            *dst++ = 32767;
        else if (v < -32768.)
            *dst++ = -32768;
        else
            *dst++ = lround(v);
    }
    b->i_buffer /= 4;
    return b;
}

static block_t *Fl64toFl32(filter_t *filter, block_t *b)
{
    double *src = (double *)b->p_buffer;
    float  *dst = (float *)src;
    for (size_t i = b->i_buffer / 8; i--;)
        *(dst++) = *(src++);

    VLC_UNUSED(filter);
    return b;
}

static block_t *Fl64toS32(filter_t *filter, block_t *b)
{
    double  *src = (double *)b->p_buffer;
    int32_t *dst = (int32_t *)src;
    for (size_t i = b->i_buffer / 8; i--;)
    {
        float s = *(src++) * 2147483648.;
        if (s >= 2147483647.f)
            *(dst++) = 2147483647;
        else
        if (s <= -2147483648.f)
            *(dst++) = -2147483648;
        else
            *(dst++) = lround(s);
    }
    VLC_UNUSED(filter);
    return b;
}


/* */
/* */
static const struct {
    vlc_fourcc_t src;
    vlc_fourcc_t dst;
    cvt_t convert;
} cvt_directs[] = {
    { VLC_CODEC_U8,   VLC_CODEC_S16N, U8toS16    },
    { VLC_CODEC_U8,   VLC_CODEC_FL32, U8toFl32   },
    { VLC_CODEC_U8,   VLC_CODEC_S32N, U8toS32    },
    { VLC_CODEC_U8,   VLC_CODEC_FL64, U8toFl64   },

    { VLC_CODEC_S16N, VLC_CODEC_U8,   S16toU8    },
    { VLC_CODEC_S16N, VLC_CODEC_FL32, S16toFl32  },
    { VLC_CODEC_S16N, VLC_CODEC_S32N, S16toS32   },
    { VLC_CODEC_S16N, VLC_CODEC_FL64, S16toFl64  },

    { VLC_CODEC_FL32, VLC_CODEC_U8,   Fl32toU8   },
    { VLC_CODEC_FL32, VLC_CODEC_S16N, Fl32toS16  },
    { VLC_CODEC_FL32, VLC_CODEC_S32N, Fl32toS32  },
    { VLC_CODEC_FL32, VLC_CODEC_FL64, Fl32toFl64 },

    { VLC_CODEC_S32N, VLC_CODEC_U8,   S32toU8    },
    { VLC_CODEC_S32N, VLC_CODEC_S16N, S32toS16   },
    { VLC_CODEC_S32N, VLC_CODEC_FL32, S32toFl32  },
    { VLC_CODEC_S32N, VLC_CODEC_FL64, S32toFl64  },

    { VLC_CODEC_FL64, VLC_CODEC_U8,   Fl64toU8   },
    { VLC_CODEC_FL64, VLC_CODEC_S16N, Fl64toS16  },
    { VLC_CODEC_FL64, VLC_CODEC_FL32, Fl64toFl32 },
    { VLC_CODEC_FL64, VLC_CODEC_S32N, Fl64toS32  },

    { 0, 0, NULL }
};

static cvt_t FindConversion(vlc_fourcc_t src, vlc_fourcc_t dst)
{
    for (int i = 0; cvt_directs[i].convert; i++) {
        if (cvt_directs[i].src == src &&
            cvt_directs[i].dst == dst)
            return cvt_directs[i].convert;
    }
    return NULL;
}
