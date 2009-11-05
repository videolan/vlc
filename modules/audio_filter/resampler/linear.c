/*****************************************************************************
 * linear.c : linear interpolation resampler
 *****************************************************************************
 * Copyright (C) 2002, 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *          Sigmund Augdal Helberg <dnumgis@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_filter.h>
#include <vlc_block.h>
#include <vlc_cpu.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenFilter ( vlc_object_t * );
static void CloseFilter( vlc_object_t * );
static block_t *Resample( filter_t *, block_t * );

#if HAVE_FPU
typedef float sample_t;
# define VLC_CODEC_NATIVE VLC_CODEC_FL32
#else
typedef int32_t sample_t;
# define VLC_CODEC_NATIVE VLC_CODEC_FI32
#endif

/*****************************************************************************
 * Local structures
 *****************************************************************************/
struct filter_sys_t
{
    sample_t *p_prev_sample;      /* this filter introduces a 1 sample delay */

    unsigned int i_remainder;                /* remainder of previous sample */

    date_t       end_date;
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Audio filter for linear interpolation resampling") )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_MISC )
    set_capability( "audio filter", 5 )
    set_callbacks( OpenFilter, CloseFilter )
vlc_module_end ()

/*****************************************************************************
 * Resample: convert a buffer
 *****************************************************************************/
static block_t *Resample( filter_t *p_filter, block_t *p_in_buf )
{
    if( !p_in_buf || !p_in_buf->i_nb_samples )
    {
        if( p_in_buf )
            block_Release( p_in_buf );
        return NULL;
    }

    filter_sys_t *p_sys = p_filter->p_sys;
    unsigned i_nb_channels = p_filter->fmt_in.audio.i_channels;
    sample_t *p_prev_sample = p_sys->p_prev_sample;

    /* Check if we really need to run the resampler */
    if( p_filter->fmt_out.audio.i_rate == p_filter->fmt_in.audio.i_rate )
    {
        if( !(p_in_buf->i_flags & BLOCK_FLAG_DISCONTINUITY) )
        {
            p_in_buf = block_Realloc( p_in_buf,
                                      sizeof(sample_t) * i_nb_channels,
                                      p_in_buf->i_buffer );
            if( !p_in_buf )
                return NULL;

            memcpy( p_in_buf->p_buffer, p_prev_sample,
                    i_nb_channels * sizeof(sample_t) );
        }
        return p_in_buf;
    }

    unsigned i_bytes_per_frame = p_filter->fmt_out.audio.i_channels *
                                 p_filter->fmt_out.audio.i_bitspersample / 8;

    size_t i_out_size = i_bytes_per_frame * (1 + (p_in_buf->i_nb_samples *
              p_filter->fmt_out.audio.i_rate / p_filter->fmt_in.audio.i_rate));
    block_t *p_out_buf = filter_NewAudioBuffer( p_filter, i_out_size );
    if( !p_out_buf )
        goto out;

    sample_t *p_out = (sample_t *)p_out_buf->p_buffer;

    unsigned i_in_nb = p_in_buf->i_nb_samples;
    unsigned i_out = 0;
    const sample_t *p_in = (sample_t *)p_in_buf->p_buffer;

    /* Take care of the previous input sample (if any) */
    if( p_in_buf->i_flags & BLOCK_FLAG_DISCONTINUITY )
    {
        p_out_buf->i_flags |= BLOCK_FLAG_DISCONTINUITY;
        p_sys->i_remainder = 0;
        date_Init( &p_sys->end_date, p_filter->fmt_out.audio.i_rate, 1 );
    }
    else
    {
        while( p_sys->i_remainder < p_filter->fmt_out.audio.i_rate )
        {
            for( unsigned i = 0; i < i_nb_channels ; i++ )
            {
                p_out[i] = p_prev_sample[i];
#if HAVE_FPU
                p_out[i] += (p_in[i] - p_prev_sample[i])
#else
                p_out[i] += (int64_t)(p_in[i] - p_prev_sample[i])
#endif
                    * p_sys->i_remainder / p_filter->fmt_out.audio.i_rate;
            }
            p_out += i_nb_channels;
            i_out++;

            p_sys->i_remainder += p_filter->fmt_in.audio.i_rate;
        }
        p_sys->i_remainder -= p_filter->fmt_out.audio.i_rate;
    }

    /* Take care of the current input samples (minus last one) */
    for( unsigned i_in = 0; i_in < i_in_nb - 1; i_in++ )
    {
        while( p_sys->i_remainder < p_filter->fmt_out.audio.i_rate )
        {
            for( unsigned i = 0; i < i_nb_channels ; i++ )
            {
                p_out[i] = p_in[i];
#if HAVE_FPU
                p_out[i] += (p_in[i + i_nb_channels] - p_in[i])
#else
                p_out[i] += (int64_t)(p_in[i + i_nb_channels] - p_in[i])
#endif
                    * p_sys->i_remainder / p_filter->fmt_out.audio.i_rate;
            }
            p_out += i_nb_channels;
            i_out++;

            p_sys->i_remainder += p_filter->fmt_in.audio.i_rate;
        }

        p_in += i_nb_channels;
        p_sys->i_remainder -= p_filter->fmt_out.audio.i_rate;
    }

    /* Backup the last input sample for next time */
    memcpy( p_prev_sample, p_in, i_nb_channels * sizeof(sample_t) );

    p_out_buf->i_nb_samples = i_out;
    p_out_buf->i_pts = p_in_buf->i_pts;

    if( p_in_buf->i_pts !=
        date_Get( &p_sys->end_date ) )
    {
        date_Set( &p_sys->end_date, p_in_buf->i_pts );
    }

    p_out_buf->i_length = date_Increment( &p_sys->end_date,
                                  p_out_buf->i_nb_samples ) - p_out_buf->i_pts;

    p_out_buf->i_buffer = p_out_buf->i_nb_samples *
        i_nb_channels * sizeof(sample_t);
out:
    block_Release( p_in_buf );
    return p_out_buf;
}

/*****************************************************************************
 * OpenFilter:
 *****************************************************************************/
static int OpenFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;
    int i_out_rate  = p_filter->fmt_out.audio.i_rate;

    if( p_filter->fmt_in.audio.i_rate == p_filter->fmt_out.audio.i_rate ||
        p_filter->fmt_in.i_codec != VLC_CODEC_NATIVE )
    {
        return VLC_EGENERIC;
    }
 
    /* Allocate the memory needed to store the module's structure */
    p_filter->p_sys = p_sys = malloc( sizeof(struct filter_sys_t) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    p_sys->p_prev_sample = malloc(
        p_filter->fmt_in.audio.i_channels * sizeof(sample_t) );
    if( p_sys->p_prev_sample == NULL )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }
    date_Init( &p_sys->end_date, p_filter->fmt_in.audio.i_rate, 1 );
    p_sys->i_remainder = 0;

    p_filter->pf_audio_filter = Resample;

    msg_Dbg( p_this, "%4.4s/%iKHz/%i->%4.4s/%iKHz/%i",
             (char *)&p_filter->fmt_in.i_codec,
             p_filter->fmt_in.audio.i_rate,
             p_filter->fmt_in.audio.i_channels,
             (char *)&p_filter->fmt_out.i_codec,
             p_filter->fmt_out.audio.i_rate,
             p_filter->fmt_out.audio.i_channels);

    p_filter->fmt_out = p_filter->fmt_in;
    p_filter->fmt_out.audio.i_rate = i_out_rate;

    return 0;
}

/*****************************************************************************
 * CloseFilter : deallocate data structures
 *****************************************************************************/
static void CloseFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    free( p_filter->p_sys->p_prev_sample );
    free( p_filter->p_sys );
}
