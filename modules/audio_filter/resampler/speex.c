/*****************************************************************************
 * speex.c : libspeex DSP resampler
 *****************************************************************************
 * Copyright © 2011-2012 Rémi Denis-Courmont
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
# include <config.h>
#endif

#include <inttypes.h>

#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_filter.h>
#include <vlc_plugin.h>

#include <speex/speex_resampler.h>

#define QUALITY_TEXT N_("Resampling quality")
#define QUALITY_LONGTEXT N_( "Resampling quality, from worst to best" )

static int Open (vlc_object_t *);
static int OpenResampler (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin ()
    set_shortname (N_("Speex resampler"))
    set_description (N_("Speex resampler") )
    set_category (CAT_AUDIO)
    set_subcategory (SUBCAT_AUDIO_RESAMPLER)
    add_integer ("speex-resampler-quality", 4,
                 QUALITY_TEXT, QUALITY_LONGTEXT, true)
        change_integer_range (0, 10)
    set_capability ("audio converter", 0)
    set_callbacks (Open, Close)

    add_submodule ()
    set_capability ("audio resampler", 0)
    set_callbacks (OpenResampler, Close)
    add_shortcut ("speex")
vlc_module_end ()

static block_t *Resample (filter_t *, block_t *);

static int OpenResampler (vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;

    /* Cannot convert format */
    if (filter->fmt_in.audio.i_format != filter->fmt_out.audio.i_format
    /* Cannot remix */
     || filter->fmt_in.audio.i_channels != filter->fmt_out.audio.i_channels
     || filter->fmt_in.audio.i_physical_channels == 0 )
        return VLC_EGENERIC;

    switch (filter->fmt_in.audio.i_format)
    {
        case VLC_CODEC_FL32: break;
        case VLC_CODEC_S16N: break;
        default:             return VLC_EGENERIC;
    }

    SpeexResamplerState *st;

    unsigned q = var_InheritInteger (obj, "speex-resampler-quality");
    if (unlikely(q > 10))
        q = 3;

    int err;
    st = speex_resampler_init(filter->fmt_in.audio.i_channels,
                              filter->fmt_in.audio.i_rate,
                              filter->fmt_out.audio.i_rate, q, &err);
    if (unlikely(st == NULL))
    {
        msg_Err (obj, "cannot initialize resampler: %s",
                 speex_resampler_strerror (err));
        return VLC_ENOMEM;
    }

    filter->p_sys = (filter_sys_t *)st;
    filter->pf_audio_filter = Resample;
    return VLC_SUCCESS;
}

static int Open (vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;

    /* Will change rate */
    if (filter->fmt_in.audio.i_rate == filter->fmt_out.audio.i_rate)
        return VLC_EGENERIC;
    return OpenResampler (obj);
}

static void Close (vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    SpeexResamplerState *st = (SpeexResamplerState *)filter->p_sys;

    speex_resampler_destroy (st);
}

static block_t *Resample (filter_t *filter, block_t *in)
{
    SpeexResamplerState *st = (SpeexResamplerState *)filter->p_sys;

    const size_t framesize = filter->fmt_out.audio.i_bytes_per_frame;
    const unsigned irate = filter->fmt_in.audio.i_rate;
    const unsigned orate = filter->fmt_out.audio.i_rate;

    spx_uint32_t ilen = in->i_nb_samples;
    spx_uint32_t olen = ((ilen + 2) * orate * UINT64_C(11))
                      / (irate * UINT64_C(10));

    block_t *out = block_Alloc (olen * framesize);
    if (unlikely(out == NULL))
        goto error;

    speex_resampler_set_rate (st, irate, orate);

    int err;
    if (filter->fmt_in.audio.i_format == VLC_CODEC_FL32)
        err = speex_resampler_process_interleaved_float (st,
            (float *)in->p_buffer, &ilen, (float *)out->p_buffer, &olen);
    else
        err = speex_resampler_process_interleaved_int (st,
            (int16_t *)in->p_buffer, &ilen, (int16_t *)out->p_buffer, &olen);
    if (err != 0)
    {
        msg_Err (filter, "cannot resample: %s",
                 speex_resampler_strerror (err));
        block_Release (out);
        out = NULL;
        goto error;
    }

    if (ilen < in->i_nb_samples)
        msg_Err (filter, "lost %"PRIu32" of %u input frames",
                 in->i_nb_samples - ilen, in->i_nb_samples);

    out->i_buffer = olen * framesize;
    out->i_nb_samples = olen;
    out->i_pts = in->i_pts;
    out->i_length = olen * CLOCK_FREQ / filter->fmt_out.audio.i_rate;
error:
    block_Release (in);
    return out;
}
