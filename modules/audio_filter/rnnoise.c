/*****************************************************************************
 * rnnoise.c : Recurrent neural network for audio noise reduction
 *****************************************************************************
 * Copyright © 2019 Tristan Matthews
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

#include <rnnoise.h>

typedef struct
{
    DenoiseState **p_sts;
    float *p_scratch_buffer;
    bool b_first;
} filter_sys_t;

static void
Flush(filter_t *p_filter)
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if (p_sys->p_sts) {
        int i_channels = p_filter->fmt_in.audio.i_channels;
        for (int i = 0; i < i_channels; i++) {
            rnnoise_destroy( p_sys->p_sts[i] );
        }
        free(p_sys->p_sts);
        p_sys->p_sts = NULL;
        p_sys->b_first = true;
    }
}

static DenoiseState **init_denoise_state(unsigned i_channels)
{
    DenoiseState **p_sts = malloc(i_channels * sizeof(DenoiseState *));
    if (unlikely(!p_sts))
        return NULL;

    for (unsigned i = 0; i < i_channels; i++)
    {
        p_sts[i] = rnnoise_create(NULL);
        if (unlikely(!p_sts[i]))
        {
            for (unsigned j = 0; j < i; j++)
                rnnoise_destroy( p_sts[j] );

            return NULL;
        }
    }

    return p_sts;
}


#define FRAME_SIZE 480

static block_t *Process(filter_t *p_filter, block_t *p_block)
{
    filter_sys_t *p_sys = p_filter->p_sys;
    float *p_buffer = (float *)p_block->p_buffer;
    const int i_channels = p_filter->fmt_in.audio.i_channels;

    float *tmp = p_sys->p_scratch_buffer;

    if (unlikely(p_sys->p_sts == NULL))
    {
        /* Can happen after a flush */
        p_sys->p_sts = init_denoise_state(i_channels);
        if (p_sys->p_sts == NULL)
            return p_block;
    }

    for (int i_nb_samples = p_block->i_nb_samples; i_nb_samples > 0; i_nb_samples -= FRAME_SIZE)
    {
        /* handle case where we have fewer than FRAME_SIZE samples left to process */
        const unsigned frame_size = __MIN(FRAME_SIZE, i_nb_samples);

        /* rnnoise processes blocks of 480 samples, and expects input to be in the 32768 scale. */
        for (unsigned i = 0; i < frame_size; i++) {
            for (int j = 0; j < i_channels; j++) {
                tmp[i + j * frame_size] = p_buffer[i * i_channels + j] * 32768.f;
            }
        }

        for (int i = 0; i < i_channels; i++) {
            rnnoise_process_frame(p_sys->p_sts[i], tmp + i * frame_size , tmp + i * frame_size);
        }

        /* Skip writing first frame to output (as per the examples) I guess to prime rnnoise? */
        if(!p_sys->b_first)
        {
            for (unsigned i = 0; i < frame_size; i++) {
                for (int j = 0; j < i_channels; j++) {
                    p_buffer[i * i_channels + j] = tmp[i + j * frame_size] / 32768.f;
                }
            }
        }
        else
        {
            p_sys->b_first = false;
        }
        p_buffer += frame_size * i_channels;
    }

    return p_block;
}

static void Close( filter_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;
    filter_sys_t *p_sys = p_filter->p_sys;
    Flush(p_filter);
    free(p_sys->p_scratch_buffer);
}

static int Open (vlc_object_t *obj)
{
    filter_t *p_filter = (filter_t *)obj;

    filter_sys_t *p_sys = p_filter->p_sys = vlc_obj_malloc(VLC_OBJECT(p_filter), sizeof(*p_sys));
    if(unlikely(!p_sys))
        return VLC_ENOMEM;

    p_sys->b_first = true;

    int i_channels = p_filter->fmt_in.audio.i_channels;
    p_sys->p_sts = init_denoise_state(i_channels);
    if (unlikely(!p_sys->p_sts)) {
        vlc_obj_free(VLC_OBJECT(p_filter), p_sys);
        return VLC_ENOMEM;
    }

    p_sys->p_scratch_buffer = malloc(FRAME_SIZE * i_channels * sizeof(*p_sys->p_scratch_buffer));
    if (unlikely(!p_sys->p_scratch_buffer)) {
        Flush(p_filter);
        vlc_obj_free(VLC_OBJECT(p_filter), p_sys);
        return VLC_ENOMEM;
    }

    p_filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
    aout_FormatPrepare(&p_filter->fmt_in.audio);
    p_filter->fmt_out.audio = p_filter->fmt_in.audio;

    static const struct vlc_filter_operations filter_ops =
    {
        .filter_audio = Process, .flush = Flush, .close = Close,
    };
    p_filter->ops = &filter_ops;

    return VLC_SUCCESS;
}

vlc_module_begin ()
    set_shortname (N_("RNNoise"))
    set_description (N_("RNNoise filter"))
    set_category (CAT_AUDIO)
    set_subcategory (SUBCAT_AUDIO_AFILTER)
    set_capability ("audio filter", 0)
    set_callback( Open )
vlc_module_end ()
