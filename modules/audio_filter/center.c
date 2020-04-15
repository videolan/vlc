/*****************************************************************************
 * center.c : Central channel filter
 *****************************************************************************
 * Copyright © 2020 VLC authors and VideoLAN
 *
 * Authors: Vedanta Nayak <vedantnayak2@gmail.com>
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
#include <vlc_aout.h>
#include <vlc_filter.h>
#include <vlc_plugin.h>

static block_t *Process ( filter_t *filter, block_t *in_buf )
{
#define NB_CHANNELS 3

    (void) filter;
    float *in = (float*)in_buf->p_buffer;
    size_t i_nb_samples = in_buf->i_nb_samples;
    block_t *out_buf = block_Alloc(sizeof(float) * i_nb_samples * NB_CHANNELS);
    if ( !out_buf )
    {
        block_Release(in_buf);
        return out_buf;
    }
    float * p_out = (float*)out_buf->p_buffer;
    out_buf->i_nb_samples = i_nb_samples;
    out_buf->i_dts        = in_buf->i_dts;
    out_buf->i_pts        = in_buf->i_pts;
    out_buf->i_length     = sizeof(float) * i_nb_samples;
    const float factor = .70710678;
    for ( size_t i = 0 ; i < i_nb_samples ; ++i)
    {
        float left = in[i*2];
        float right = in[i*2+1];
        float center = ( left + right ) * factor / 2;
        p_out[i * NB_CHANNELS   ] = left;
        p_out[i * NB_CHANNELS + 1 ] = right;
        p_out[i * NB_CHANNELS + 2 ] = center;
    }
    block_Release(in_buf);
    return out_buf;
}

static int Open (vlc_object_t *in)
{
    filter_t *filter = (filter_t *)in;
    if (filter->fmt_in.audio.i_physical_channels != AOUT_CHANS_STEREO)
        return VLC_EGENERIC;

    static_assert(AOUT_CHANIDX_CENTER > AOUT_CHANIDX_RIGHT &&
        AOUT_CHANIDX_RIGHT > AOUT_CHANIDX_LEFT, "Change in channel order.");

    filter->fmt_out.audio.i_format = VLC_CODEC_FL32;
    filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
    filter->fmt_out.audio.i_physical_channels = AOUT_CHANS_STEREO | AOUT_CHAN_CENTER;
    filter->fmt_out.audio.i_rate = filter->fmt_in.audio.i_rate;
    aout_FormatPrepare(&filter->fmt_in.audio);
    aout_FormatPrepare(&filter->fmt_out.audio);
    filter->pf_audio_filter = Process;
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname (N_("Center"))
    set_description (N_("Create a central channel"))
    set_category (CAT_AUDIO)
    set_subcategory (SUBCAT_AUDIO_AFILTER)
    set_capability ("audio filter",0)
    set_callback (Open)
vlc_module_end ()
