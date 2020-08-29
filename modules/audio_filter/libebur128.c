/*****************************************************************************
 * libebur128.c : libebur128 filter
 *****************************************************************************
 * Copyright © 2020 Videolabs
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
#include <vlc_modules.h>
#include <vlc_plugin.h>

#include <ebur128.h>

#define UPDATE_INTERVAL VLC_TICK_FROM_MS(400)

#define CFG_PREFIX "ebur128-"

struct filter_sys
{
    int mode;
    ebur128_state *state;
    vlc_tick_t last_update;
    bool new_frames;
};

static ebur128_state *
CreateEbuR128State(filter_t *filter, int mode)
{
    ebur128_state *state =
        ebur128_init(filter->fmt_in.audio.i_channels, filter->fmt_in.audio.i_rate, mode);
    if (state == NULL)
        return NULL;

    /* TODO: improve */
    unsigned channels_set = 2;
    int error;
    if (filter->fmt_in.audio.i_physical_channels == AOUT_CHANS_5_1
     || filter->fmt_in.audio.i_physical_channels == AOUT_CHANS_7_1)
    {
        error = ebur128_set_channel(state, 2, EBUR128_LEFT_SURROUND);
        if (error != EBUR128_SUCCESS)
            goto error;
        ebur128_set_channel(state, 3, EBUR128_RIGHT_SURROUND);
        if (error != EBUR128_SUCCESS)
            goto error;
        ebur128_set_channel(state, 4, EBUR128_CENTER);
        if (error != EBUR128_SUCCESS)
            goto error;

        channels_set += 3;
    }

    for (unsigned i = channels_set; i < filter->fmt_in.audio.i_channels; ++i)
    {
        error = ebur128_set_channel(state, i, EBUR128_UNUSED);
        if (error != EBUR128_SUCCESS)
            goto error;
    }

    return state;
error:
    ebur128_destroy(&state);
    return NULL;
}

static int
SendLoudnessMeter(filter_t *filter)
{
    struct filter_sys *sys = filter->p_sys;

    int error;
    struct vlc_audio_loudness loudness = { 0, 0, 0, 0, 0 };

    error = ebur128_loudness_momentary(sys->state, &loudness.loudness_momentary);
    if (error != EBUR128_SUCCESS)
        return error;

    if ((sys->state->mode & EBUR128_MODE_S) == EBUR128_MODE_S)
    {
        error = ebur128_loudness_shortterm(sys->state, &loudness.loudness_shortterm);
        if (error != EBUR128_SUCCESS)
            return error;
    }
    if ((sys->state->mode & EBUR128_MODE_I) == EBUR128_MODE_I)
    {
        error = ebur128_loudness_global(sys->state, &loudness.loudness_integrated);
        if (error != EBUR128_SUCCESS)
            return error;

    }
    if ((sys->state->mode & EBUR128_MODE_LRA) == EBUR128_MODE_LRA)
    {
        error = ebur128_loudness_range(sys->state, &loudness.loudness_range);
        if (error != EBUR128_SUCCESS)
            return error;
    }
    if ((sys->state->mode & EBUR128_MODE_TRUE_PEAK) == EBUR128_MODE_TRUE_PEAK)
    {
        for (unsigned i = 0; i < filter->fmt_in.audio.i_channels; ++i)
        {
            double truepeak;
            error = ebur128_true_peak(sys->state, 0, &truepeak);
            if (error != EBUR128_SUCCESS)
                return error;
            if (truepeak > loudness.truepeak)
                loudness.truepeak = truepeak;
        }
    }

    filter_SendAudioLoudness(filter, &loudness);

    return EBUR128_SUCCESS;
}

static block_t *
Process(filter_t *filter, block_t *block)
{
    struct filter_sys *sys = filter->p_sys;
    int error;
    block_t *out = block;

    if (unlikely(sys->state == NULL))
    {
        /* Can happen after a flush */
        sys->state = CreateEbuR128State(filter, sys->mode);
        if (sys->state == NULL)
            return out;
    }

    switch (filter->fmt_in.i_codec)
    {
        case VLC_CODEC_U8:
        {
            /* Convert to S16N */
            short *data_s16 = malloc(block->i_buffer * 2);
            if (unlikely(data_s16 == NULL))
                return out;

            uint8_t *src = (uint8_t *)block->p_buffer;
            short *dst = data_s16;
            for (size_t i = block->i_buffer; i--;)
                *dst++ = ((*src++) << 8) - 0x8000;

            error = ebur128_add_frames_short(sys->state, data_s16,
                                             block->i_nb_samples);
            free(data_s16);
            break;
        }
        case VLC_CODEC_S16N:
            error = ebur128_add_frames_short(sys->state,
                                             (const short *)block->p_buffer,
                                             block->i_nb_samples);
            break;
        case VLC_CODEC_S32N:
            error = ebur128_add_frames_int(sys->state,
                                           (const int *) block->p_buffer,
                                           block->i_nb_samples);
            break;
        case VLC_CODEC_FL32:
            error = ebur128_add_frames_float(sys->state,
                                             (const float *) block->p_buffer,
                                             block->i_nb_samples);
            break;
        case VLC_CODEC_FL64:
            error = ebur128_add_frames_double(sys->state,
                                              (const double *) block->p_buffer,
                                              block->i_nb_samples);
            break;
        default: vlc_assert_unreachable();
    }

    if (error != EBUR128_SUCCESS)
    {
        msg_Warn(filter, "ebur128_add_frames_*() failed: %d\n", error);
        return out;
    }

    if (sys->last_update == VLC_TICK_INVALID)
        sys->last_update = out->i_pts;

    if (out->i_pts + out->i_length - sys->last_update >= UPDATE_INTERVAL)
    {
        error = SendLoudnessMeter(filter);
        if (error == EBUR128_SUCCESS)
        {
            sys->last_update = out->i_pts + out->i_length;
            sys->new_frames = false;
        }
    }
    else
        sys->new_frames = true;

    return out;
}

static void
Flush(filter_t *filter)
{
    struct filter_sys *sys = filter->p_sys;

    if (sys->state != NULL)
    {
        if (sys->new_frames)
        {
            SendLoudnessMeter(filter);
            sys->new_frames = false;
        }
        sys->last_update = VLC_TICK_INVALID;

        ebur128_destroy(&sys->state);
    }
}

static int Open(vlc_object_t *this)
{
    filter_t *filter = (filter_t *) this;

    switch (filter->fmt_in.i_codec)
    {
        case VLC_CODEC_U8:
        case VLC_CODEC_S16N:
        case VLC_CODEC_S32N:
        case VLC_CODEC_FL32:
        case VLC_CODEC_FL64:
            break;
        default:
            return VLC_EGENERIC;
    }

    static const char *const options[] = {
        "mode", NULL
    };
    config_ChainParse(filter, CFG_PREFIX, options, filter->p_cfg);

    struct filter_sys *sys = malloc(sizeof(*sys));
    if (sys == NULL)
        return VLC_ENOMEM;

    int plugin_mode = var_InheritInteger(filter, CFG_PREFIX "mode");
    sys->mode = EBUR128_MODE_M;
    switch (plugin_mode)
    {
        case 4: sys->mode |= EBUR128_MODE_TRUE_PEAK;/* fall-through */
        case 3: sys->mode |= EBUR128_MODE_LRA;      /* fall-through */
        case 2: sys->mode |= EBUR128_MODE_I;        /* fall-through */
        case 1: sys->mode |= EBUR128_MODE_S;        /* fall-through */
        case 0: break;
        default: vlc_assert_unreachable();
    }


    sys->last_update = VLC_TICK_INVALID;
    sys->new_frames = false;
    sys->state = CreateEbuR128State(filter, sys->mode);
    if (sys->state == NULL)
    {
        free(sys);
        return VLC_EGENERIC;
    }

    filter->p_sys = sys;
    filter->fmt_out.audio = filter->fmt_in.audio;
    filter->pf_audio_filter = Process;
    filter->pf_flush = Flush;
    return VLC_SUCCESS;
}

static void
Close(vlc_object_t *this)
{
    filter_t *filter = (filter_t*) this;
    struct filter_sys *sys = filter->p_sys;

    if (sys->state != NULL)
        ebur128_destroy(&sys->state);
    free(filter->p_sys);
}

vlc_module_begin()
    set_shortname("EBU R 128")
    set_description("EBU R128 standard for loudness normalisation")
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_AFILTER)
    add_integer_with_range(CFG_PREFIX "mode", 0, 0, 4, NULL, NULL, false)
    set_capability("audio meter", 0)
    set_callbacks(Open, Close)
vlc_module_end()
