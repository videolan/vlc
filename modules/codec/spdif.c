/*****************************************************************************
 * spdif.c: S/PDIF pass-though decoder
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
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
#include <vlc_codec.h>
#include <vlc_modules.h>

static int OpenDecoder(vlc_object_t *);

vlc_module_begin()
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_ACODEC)
    set_description(N_("S/PDIF pass-through decoder"))
    set_capability("audio decoder", 120)
    set_callback(OpenDecoder)
vlc_module_end()

static int
DecodeBlock(decoder_t *p_dec, block_t *p_block)
{
    if (p_block != NULL)
        decoder_QueueAudio( p_dec, p_block );
    return VLCDEC_SUCCESS;
}

static int
OpenDecoder(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t*)p_this;

    switch (p_dec->fmt_in.i_codec)
    {
    case VLC_CODEC_MPGA:
    case VLC_CODEC_MP3:
        /* Disabled by default */
        if (!p_dec->obj.force)
            return VLC_EGENERIC;
        break;
    case VLC_CODEC_A52:
    case VLC_CODEC_EAC3:
    case VLC_CODEC_MLP:
    case VLC_CODEC_TRUEHD:
    case VLC_CODEC_DTS:
    case VLC_CODEC_SPDIFL:
    case VLC_CODEC_SPDIFB:
        /* Enabled by default */
        break;
    default:
        return VLC_EGENERIC;
    }

    /* Set output properties */
    p_dec->fmt_out.i_codec = p_dec->fmt_in.i_codec;
    p_dec->fmt_out.audio = p_dec->fmt_in.audio;
    p_dec->fmt_out.i_profile = p_dec->fmt_in.i_profile;
    p_dec->fmt_out.audio.i_format = p_dec->fmt_out.i_codec;

    if (decoder_UpdateAudioFormat(p_dec))
        return VLC_EGENERIC;

    p_dec->pf_decode = DecodeBlock;
    p_dec->pf_flush  = NULL;

    return VLC_SUCCESS;
}
