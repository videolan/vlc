/*****************************************************************************
 * endian.c : PCM endian converter
 *****************************************************************************
 * Copyright (C) 2002-2005 VLC authors and VideoLAN
 * Copyright (C) 2010 Laurent Aimar
 * Copyright (C) 2012 Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_block.h>
#include <vlc_filter.h>

static int  Open(vlc_object_t *);

vlc_module_begin()
    set_description(N_("Audio filter for endian conversion"))
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_MISC)
    set_capability("audio converter", 2)
    set_callbacks(Open, NULL)
vlc_module_end()

static block_t *Filter16(filter_t *filter, block_t *block)
{
    uint16_t *data = (uint16_t *)block->p_buffer;

    for (size_t i = 0; i < block->i_buffer / 2; i++)
        data[i] = bswap16 (data[i]);

    (void) filter;
    return block;
}

static block_t *Filter24(filter_t *filter, block_t *block)
{
    uint8_t *data = (uint8_t *)block->p_buffer;

    for (size_t i = 0; i < block->i_buffer; i += 3) {
        uint8_t buf = data[i];
        data[i] = data[i + 2];
        data[i + 2] = buf;
    }

    (void) filter;
    return block;
}

static block_t *Filter32(filter_t *filter, block_t *block)
{
    uint32_t *data = (uint32_t *)block->p_buffer;

    for (size_t i = 0; i < block->i_buffer / 4; i++)
        data[i] = bswap32 (data[i]);

    (void) filter;
    return block;
}

static block_t *Filter64(filter_t *filter, block_t *block)
{
    uint64_t *data = (uint64_t *)block->p_buffer;

    for (size_t i = 0; i < block->i_buffer / 8; i++)
        data[i] = bswap64 (data[i]);

    (void) filter;
    return block;
}

static const vlc_fourcc_t list[][2] = {
    { VLC_CODEC_F64B, VLC_CODEC_F64L },
    { VLC_CODEC_F32B, VLC_CODEC_F32L },
    { VLC_CODEC_S16B, VLC_CODEC_S16L },
    { VLC_CODEC_S24B, VLC_CODEC_S24L },
    { VLC_CODEC_S32B, VLC_CODEC_S32L },
    { VLC_CODEC_S16B, VLC_CODEC_S16L },
    { VLC_CODEC_S24B, VLC_CODEC_S24L },
    { VLC_CODEC_S32B, VLC_CODEC_S32L },
};

static int Open(vlc_object_t *object)
{
    filter_t *filter = (filter_t *)object;

    const audio_sample_format_t *src = &filter->fmt_in.audio;
    const audio_sample_format_t *dst = &filter->fmt_out.audio;

    if (!AOUT_FMTS_SIMILAR(src, dst))
        return VLC_EGENERIC;

    for (size_t i = 0; i < sizeof (list) / sizeof (list[0]); i++) {
        if (src->i_format == list[i][0]) {
            if (dst->i_format != list[i][1])
                goto ok;
            break;
        }
        if (src->i_format == list[i][1]) {
            if (dst->i_format == list[i][0])
                goto ok;
            break;
        }
    }
    return VLC_EGENERIC;

ok:
    switch (src->i_bitspersample) {
        case 16:
            filter->pf_audio_filter = Filter16;
            break;
        case 24:
            filter->pf_audio_filter = Filter24;
            break;
        case 32:
            filter->pf_audio_filter = Filter32;
            break;
        case 64:
            filter->pf_audio_filter = Filter64;
            break;
    }

    return VLC_SUCCESS;
}
