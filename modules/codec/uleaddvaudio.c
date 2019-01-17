/*****************************************************************************
 * uleaddvaudio.c
 *****************************************************************************
 * Copyright (C) 2012 Laurent Aimar
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
#include <vlc_codec.h>

#include "../demux/rawdv.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_description(N_("Ulead DV audio decoder"))
    set_capability("audio decoder", 50)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_ACODEC)
    set_callbacks(Open, Close)
vlc_module_end()

typedef struct
{
    date_t end_date;

    bool     is_pal;
    bool     is_12bit;
    uint16_t shuffle[2000];
} decoder_sys_t;

static void Flush(decoder_t *dec)
{
    decoder_sys_t *sys = dec->p_sys;

    date_Set(&sys->end_date, VLC_TICK_INVALID);
}

static block_t *DecodeBlock(decoder_t *dec, block_t **block_ptr)
{
    decoder_sys_t *sys  = dec->p_sys;

    if (!*block_ptr)
        return NULL;

    block_t *block = *block_ptr;
    if (block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED)) {
        Flush(dec);
        if (block->i_flags & BLOCK_FLAG_CORRUPTED) {
            block_Release(block);
            *block_ptr = NULL;
            return NULL;
        }
    }

    if (block->i_pts != VLC_TICK_INVALID &&
        block->i_pts != date_Get(&sys->end_date))
        date_Set(&sys->end_date, block->i_pts);
    block->i_pts = VLC_TICK_INVALID;
    if (date_Get(&sys->end_date) == VLC_TICK_INVALID) {
        /* We've just started the stream, wait for the first PTS. */
        block_Release(block);
        return NULL;
    }

    const unsigned int block_size = sys->is_pal ? 8640 : 7200;
    if (block->i_buffer >= block_size) {
        uint8_t *src = block->p_buffer;

        block->i_buffer -= block_size;
        block->p_buffer += block_size;

        int sample_count = dv_get_audio_sample_count(&src[244], sys->is_pal);

        if( decoder_UpdateAudioFormat(dec))
            return NULL;
        block_t *output = decoder_NewAudioBuffer(dec, sample_count);
        if (!output)
            return NULL;
        output->i_pts    = date_Get(&sys->end_date);
        output->i_length = date_Increment(&sys->end_date, sample_count) - output->i_pts;

        int16_t *dst = (int16_t*)output->p_buffer;
        for (int i = 0; i < sample_count; i++) {
          const uint8_t *v = &src[sys->shuffle[i]];
          if (sys->is_12bit) {
              *dst++ = dv_audio_12to16((v[0] << 4) | ((v[2] >> 4) & 0x0f));
              *dst++ = dv_audio_12to16((v[1] << 4) | ((v[2] >> 0) & 0x0f));
          } else {
              *dst++ = GetWBE(&v[0]);
              *dst++ = GetWBE(&v[sys->is_pal ? 4320 : 3600]);
          }
        }
        return output;
    }
    block_Release(block);
    return NULL;
}

static int DecodeAudio(decoder_t *dec, block_t *block)
{
    if (block == NULL) /* No Drain */
        return VLCDEC_SUCCESS;

    block_t **block_ptr = &block, *out;
    while ((out = DecodeBlock(dec, block_ptr)) != NULL)
        decoder_QueueAudio(dec,out);
    return VLCDEC_SUCCESS;
}

static int Open(vlc_object_t *object)
{
    decoder_t *dec = (decoder_t*)object;

    if (dec->fmt_in.i_codec != VLC_CODEC_ULEAD_DV_AUDIO_NTSC &&
        dec->fmt_in.i_codec != VLC_CODEC_ULEAD_DV_AUDIO_PAL)
        return VLC_EGENERIC;
    if (dec->fmt_in.audio.i_bitspersample != 12 && dec->fmt_in.audio.i_bitspersample != 16)
        return VLC_EGENERIC;
    if (dec->fmt_in.audio.i_channels != 2)
        return VLC_EGENERIC;
    if (dec->fmt_in.audio.i_rate <= 0)
        return VLC_EGENERIC;

    decoder_sys_t *sys = dec->p_sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    sys->is_pal = dec->fmt_in.i_codec == VLC_CODEC_ULEAD_DV_AUDIO_PAL;
    sys->is_12bit = dec->fmt_in.audio.i_bitspersample == 12;

    date_Init(&sys->end_date, dec->fmt_in.audio.i_rate, 1);

    for (unsigned i = 0; i < sizeof(sys->shuffle) / sizeof(*sys->shuffle); i++) {
        const unsigned a = sys->is_pal ? 18 : 15;
        const unsigned b = 3 * a;
        sys->shuffle[i] = 80 * ((21 * (i % 3) + 9 * (i / 3) + ((i / a) % 3)) % b) +
                          (2 + sys->is_12bit) * (i / b) + 8;
    }

    dec->fmt_out.i_codec = VLC_CODEC_S16N;
    dec->fmt_out.audio.i_rate = dec->fmt_in.audio.i_rate;
    dec->fmt_out.audio.i_channels = 2;
    dec->fmt_out.audio.i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;

    dec->pf_decode = DecodeAudio;
    dec->pf_flush  = Flush;

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *object)
{
    decoder_t *dec = (decoder_t *)object;

    free(dec->p_sys);
}

