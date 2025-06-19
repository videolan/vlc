/*****************************************************************************
 * gate.c : noise gate audio filter
 *****************************************************************************
 * Copyright © 2025 VLC authors and VideoLAN
 * 
 * Authors: Paschalis Melissas <melissaspaschalis@gmail.com>
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

#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_aout_volume.h>
#include <vlc_filter.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>
#include <math.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static int      Open        ( vlc_object_t * );
static void     Close       ( filter_t * );
static block_t  *Process    ( filter_t *, block_t * );

typedef struct
{
    float time;		        /* Time to estimate power [ms] */
    float thresh;	        /* Threshold [dB] */
    float attack;	        /* Attack time [ms] */
    float release;	        /* Release time [ms] */

    float *pow;		        /* Estimated power level [dB] - per channel */
    float *gain;            /* Current gain per channel (for smooth transitions) */

    /* additional data - filter state */
    int channels;
    float alpha;            /* smoothing factor - this is actually the forgetting factor for power estimate of the Anders Johansson approach. */
    float thresh_linear;    /* Threshold in linear scale */
    float attack_factor;    /* Attack factor for smoothing */
    float release_factor;   /* Release factor for smoothing */
} filter_sys_t;

static float db_to_linear(float db)
{
    return powf(10.0f, db / 20.0f);
}

static float set_alpha(int sample_rate, float smoothing_time)
{
    /* reference : https://en.wikipedia.org/wiki/Exponential_smoothing#Time_constant */

    /* Calculate smoothing factor (alpha) from time constant */
    float time_constant_sec = smoothing_time / 1000.0f;  /* τ in seconds */
    float delta_t = 1.0f / sample_rate;  /* Time between samples (ΔT) in seconds */
    float tau_normalizer = -delta_t / time_constant_sec;  /* -ΔT/τ */
    float alpha = 1.0f - expf(tau_normalizer);  /* α = 1 - e^(-ΔT/τ) */
    return alpha;
}

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define THRESHOLD_TEXT N_("Threshold")
#define THRESHOLD_LONGTEXT N_("Threshold level in dB (negative values)")

#define ATTACK_TEXT N_("Attack time")
#define ATTACK_LONGTEXT N_("Attack time in milliseconds")

#define RELEASE_TEXT N_("Release time")
#define RELEASE_LONGTEXT N_("Release time in milliseconds")

#define SMOOTHING_TEXT N_("Smoothing time")
#define SMOOTHING_LONGTEXT N_("Amount of time for the smoothed response in milliseconds")

vlc_module_begin()
    set_shortname( N_("Noise Gate") )
    set_description( N_("Noise gate audio filter") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    add_float_with_range( "gate-threshold", -30.0, -60.0, 0.0, 
                        THRESHOLD_TEXT, THRESHOLD_LONGTEXT)
    add_float_with_range( "gate-attack", 50, 10, 500, 
                            ATTACK_TEXT, ATTACK_LONGTEXT )
    add_float_with_range( "gate-release", 300, 50, 1000, 
                            RELEASE_TEXT, RELEASE_LONGTEXT )
    add_float_with_range( "smoothing-time", 10, 1, 30, 
                            SMOOTHING_TEXT, SMOOTHING_LONGTEXT )
    set_capability( "audio filter", 0 )
    add_shortcut( "gate" )
    set_callback( Open )
vlc_module_end()

/*****************************************************************************
 * Open: initialize filter
 *****************************************************************************/

static int Open(vlc_object_t *p_this)
{
    filter_t *p_filter = (filter_t *)p_this;
    
    if (p_filter->fmt_in.audio.i_format != VLC_CODEC_FL32) 
    {
        msg_Warn( p_filter, "unsupported format" );
        return VLC_EGENERIC;
    }

    filter_sys_t *p_sys = p_filter->p_sys = malloc( sizeof( *p_sys ) );
    if (!p_sys)
        return VLC_ENOMEM;

    /* Initialize parameters */
    p_sys->channels = p_filter->fmt_in.audio.i_channels;
    unsigned int sample_rate = p_filter->fmt_in.audio.i_rate;
    if (sample_rate == 0) 
    {
        msg_Err(p_filter, "sample rate is zero");
        free(p_sys);
        return VLC_EINVAL;
    }
    p_sys->thresh = var_InheritFloat(p_filter, "gate-threshold");
    float thresh_lin = db_to_linear(p_sys->thresh); /* Convert parameters to linear scale */
    p_sys->thresh_linear = thresh_lin * thresh_lin; /*Convert to power domain */
    p_sys->attack = var_InheritFloat(p_filter, "gate-attack");
    p_sys->release = var_InheritFloat(p_filter, "gate-release");
    float attack_samples = (sample_rate * p_sys->attack) / 1000;
    float release_samples = (sample_rate * p_sys->release) / 1000;
    /* Calculate smoothing factors */
    p_sys->attack_factor = attack_samples > 0 ? 1.0f / attack_samples : 1.0f;
    p_sys->release_factor = release_samples > 0 ? 1.0f / release_samples : 1.0f;
    float smoothing_time = var_InheritFloat(p_filter, "smoothing-time");
    if(smoothing_time <= 0.0f)
    {
        msg_Err(p_filter, "Invalid smoothing time (%f), smoothing time must be between 1 and 30 ms",smoothing_time);
        free(p_sys);
        return VLC_EINVAL;
    }
    p_sys->alpha = set_alpha(sample_rate,smoothing_time);
    p_sys->pow = (float*)calloc( p_sys->channels,sizeof(float));
    p_sys->gain = (float*)calloc(p_sys->channels, sizeof(float));

    if (!p_sys->pow || !p_sys->gain) 
    {
        free(p_sys->pow);
        free(p_sys->gain);
        free(p_sys);
        return VLC_ENOMEM;
    }
    for (int i = 0; i < p_sys->channels; i++) 
    {
        p_sys->pow[i] = 0.0f;
        p_sys->gain[i] = 1.0f; /* Start with full gain */
    }
        
    p_filter->fmt_out.audio = p_filter->fmt_in.audio;

    static const struct vlc_filter_operations filter_ops = 
    {
        .filter_audio = Process, .close = Close,
    };
    p_filter->ops = &filter_ops;
    
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Process: apply noise gate to samples
 *****************************************************************************/

static block_t *Process(filter_t *p_filter, block_t *p_block)
{

    filter_sys_t *p_sys = p_filter->p_sys;

    if (!p_block || !p_block->i_nb_samples)
        return p_block;

    float *p_samples = (float *)p_block->p_buffer;
    int i_nb_samples = p_block->i_nb_samples;
    int channels = p_sys->channels;

    /* iterate over interleaved data */
    for (int ch = 0; ch < channels; ch++) {
        for (int i = ch; i < i_nb_samples * channels; i += channels) {
            float x = p_samples[i];
            float pow = x * x;

            /* Update power estimate with low-pass filter */
            p_sys->pow[ch] = (1.0f - p_sys->alpha) * p_sys->pow[ch] + pow * p_sys->alpha;
            
            /* Determine target gain */
            float target_gain = (p_sys->pow[ch] < p_sys->thresh_linear) ? 0.0f : 1.0f;
            
            /* Calculate gain delta */
            float gain_delta = target_gain - p_sys->gain[ch];

            /* Apply smoothing to gain changes */
            float gain_change_factor = ( gain_delta > 0.0f ) ? 
                                                p_sys->attack_factor : p_sys->release_factor;

            /* Smoothly adjust gain */
            p_sys->gain[ch] += gain_change_factor * gain_delta;

            /* Apply gain to sample */
            p_samples[i] = x * p_sys->gain[ch];
        }
    }
    return p_block;
}

/*****************************************************************************
 * Close: close filter
 *****************************************************************************/

static void Close( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    free( p_sys->pow );
    free( p_sys->gain );
    free( p_sys );
}