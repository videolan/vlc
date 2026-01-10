/*****************************************************************************
 * introskip.c: Automatic intro/credits skip detector
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * Authors: Ashutosh Mishra
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
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_filter.h>
#include <vlc_player.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static int  Open( vlc_object_t * );
static void Close( filter_t * );
static block_t *Process( filter_t *, block_t * );

typedef struct
{
    vlc_tick_t  i_last_msg_time;      /* Time of last message */
    vlc_tick_t  i_silence_start;      /* Start of current silence */
    vlc_tick_t  i_intro_start;        /* Detected intro start time */
    vlc_tick_t  i_intro_end;          /* Detected intro end time */
    float       f_silence_threshold;   /* Silence detection threshold */
    vlc_tick_t  i_min_silence;        /* Minimum silence duration */
    bool        b_intro_detected;      /* Intro has been detected */
    bool        b_in_silence;          /* Currently in silence period */
    int         i_silent_buffers;      /* Count of consecutive silent buffers */
} filter_sys_t;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define THRESHOLD_TEXT N_("Silence detection threshold")
#define THRESHOLD_LONGTEXT N_("Audio level threshold for silence detection. " \
    "Lower values detect quieter sounds as silence. (0.0-1.0, default: 0.01)")

#define MIN_SILENCE_TEXT N_("Minimum silence duration (ms)")
#define MIN_SILENCE_LONGTEXT N_("Minimum duration of silence to consider " \
    "as potential intro/credits boundary (in milliseconds, default: 2000)")

#define ENABLED_TEXT N_("Enable intro skip detection")
#define ENABLED_LONGTEXT N_("Enable automatic detection of intro/credits " \
    "based on silence patterns")

vlc_module_begin()
    set_description( N_("Intro/Credits skip detector") )
    set_shortname( N_("Intro Skip") )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )
    set_callback( Open )
    add_shortcut( "introskip" )
    
    add_bool( "introskip-enabled", true, ENABLED_TEXT, ENABLED_LONGTEXT )
    add_float_with_range( "introskip-threshold", 0.01, 0.0, 1.0,
                          THRESHOLD_TEXT, THRESHOLD_LONGTEXT )
    add_integer( "introskip-min-silence", 2000, MIN_SILENCE_TEXT,
                 MIN_SILENCE_LONGTEXT )
vlc_module_end()

/*****************************************************************************
 * Open: Initialize module
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys;

    if( !var_InheritBool( p_filter, "introskip-enabled" ) )
        return VLC_EGENERIC;

    p_sys = p_filter->p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    /* Initialize parameters */
    p_sys->i_last_msg_time = 0;
    p_sys->i_silence_start = 0;
    p_sys->i_intro_start = VLC_TICK_INVALID;
    p_sys->i_intro_end = VLC_TICK_INVALID;
    p_sys->b_intro_detected = false;
    p_sys->b_in_silence = false;
    p_sys->i_silent_buffers = 0;
    
    p_sys->f_silence_threshold = var_InheritFloat( p_filter, "introskip-threshold" );
    p_sys->i_min_silence = VLC_TICK_FROM_MS( var_InheritInteger( p_filter, "introskip-min-silence" ) );

    p_filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
    p_filter->fmt_out.audio = p_filter->fmt_in.audio;
    p_filter->pf_audio_filter = Process;

    msg_Dbg( p_filter, "Intro skip detector initialized (threshold: %.3f, min silence: %d ms)",
             p_sys->f_silence_threshold,
             (int)(p_sys->i_min_silence / 1000) );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: Cleanup
 *****************************************************************************/
static void Close( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    
    if( p_sys->b_intro_detected )
    {
        msg_Info( p_filter, "Intro detected: %d to %d seconds",
                  (int)(p_sys->i_intro_start / CLOCK_FREQ),
                  (int)(p_sys->i_intro_end / CLOCK_FREQ) );
    }
    
    free( p_sys );
}

/*****************************************************************************
 * Process: Analyze audio for silence patterns
 *****************************************************************************/
static block_t *Process( filter_t *p_filter, block_t *p_block )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    
    if( !p_block )
        return NULL;

    /* Calculate RMS (Root Mean Square) of the audio block */
    float *p_samples = (float *)p_block->p_buffer;
    unsigned i_samples = p_block->i_nb_samples * aout_FormatNbChannels( &p_filter->fmt_in.audio );
    
    float f_sum = 0.0f;
    for( unsigned i = 0; i < i_samples; i++ )
    {
        f_sum += p_samples[i] * p_samples[i];
    }
    float f_rms = sqrtf( f_sum / i_samples );
    
    /* Check if audio is silent */
    bool b_is_silent = ( f_rms < p_sys->f_silence_threshold );
    
    vlc_tick_t i_current_time = p_block->i_pts;
    
    if( b_is_silent )
    {
        if( !p_sys->b_in_silence )
        {
            /* Start of silence */
            p_sys->i_silence_start = i_current_time;
            p_sys->b_in_silence = true;
            p_sys->i_silent_buffers = 1;
        }
        else
        {
            p_sys->i_silent_buffers++;
        }
    }
    else
    {
        if( p_sys->b_in_silence )
        {
            /* End of silence */
            vlc_tick_t i_silence_duration = i_current_time - p_sys->i_silence_start;
            
            /* If silence was long enough and occurred early in playback */
            if( i_silence_duration >= p_sys->i_min_silence )
            {
                /* Potential intro detected */
                if( !p_sys->b_intro_detected && i_current_time < VLC_TICK_FROM_SEC(180) )
                {
                    p_sys->i_intro_start = 0;
                    p_sys->i_intro_end = i_current_time;
                    p_sys->b_intro_detected = true;
                    
                    msg_Warn( p_filter, "*** INTRO DETECTED: 0s to %ds - Press 'I' to skip! ***",
                             (int)(p_sys->i_intro_end / CLOCK_FREQ) );
                }
            }
            
            p_sys->b_in_silence = false;
            p_sys->i_silent_buffers = 0;
        }
    }
    
    /* Log status every 10 seconds for debugging */
    if( i_current_time - p_sys->i_last_msg_time > VLC_TICK_FROM_SEC(10) )
    {
        msg_Dbg( p_filter, "Introskip: time=%ds, RMS=%.4f, silent=%d, detected=%d",
                 (int)(i_current_time / CLOCK_FREQ), f_rms,
                 b_is_silent, p_sys->b_intro_detected );
        p_sys->i_last_msg_time = i_current_time;
    }
    
    return p_block;
}
