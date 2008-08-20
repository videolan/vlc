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

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Close     ( vlc_object_t * );
static void DoWork    ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                        aout_buffer_t * );

static int  OpenFilter ( vlc_object_t * );
static void CloseFilter( vlc_object_t * );
static block_t *Resample( filter_t *, block_t * );

/*****************************************************************************
 * Local structures
 *****************************************************************************/
struct filter_sys_t
{
    int32_t *p_prev_sample;       /* this filter introduces a 1 sample delay */

    unsigned int i_remainder;                /* remainder of previous sample */

    audio_date_t end_date;
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( N_("Audio filter for linear interpolation resampling") );
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_MISC );
    set_capability( "audio filter", 5 );
    set_callbacks( Create, Close );

    add_submodule();
    set_description( N_("Audio filter for linear interpolation resampling") );
    set_capability( "audio filter2", 5 );
    set_callbacks( OpenFilter, CloseFilter );
vlc_module_end();

/*****************************************************************************
 * Create: allocate linear resampler
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;
    struct filter_sys_t * p_sys;
 
    if ( p_filter->input.i_rate == p_filter->output.i_rate
          || p_filter->input.i_format != p_filter->output.i_format
          || p_filter->input.i_physical_channels
              != p_filter->output.i_physical_channels
          || p_filter->input.i_original_channels
              != p_filter->output.i_original_channels
          || p_filter->input.i_format != VLC_FOURCC('f','l','3','2') )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the module's structure */
    p_sys = malloc( sizeof(filter_sys_t) );
    p_filter->p_sys = (struct aout_filter_sys_t *)p_sys;
    if( p_sys == NULL )
        return VLC_ENOMEM;
    p_sys->p_prev_sample = malloc(
        p_filter->input.i_channels * sizeof(int32_t) );
    if( p_sys->p_prev_sample == NULL )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }
    aout_DateInit( &p_sys->end_date, p_filter->output.i_rate );

    p_filter->pf_do_work = DoWork;

    /* We don't want a new buffer to be created because we're not sure we'll
     * actually need to resample anything. */
    p_filter->b_in_place = true;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: free our resources
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;
    filter_sys_t *p_sys = (filter_sys_t *)p_filter->p_sys;
 
    free( p_sys->p_prev_sample );
    free( p_sys );
}

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    filter_sys_t *p_sys = (filter_sys_t *)p_filter->p_sys;
#ifndef HAVE_ALLOCA
    float *p_in_orig;
#endif
    float *p_in, *p_out = (float *)p_out_buf->p_buffer;
    float *p_prev_sample = (float *)p_sys->p_prev_sample;

    int i_nb_channels = p_filter->input.i_channels;
    int i_in_nb = p_in_buf->i_nb_samples;
    int i_chan, i_in, i_out = 0;

    /* Check if we really need to run the resampler */
    if( p_aout->mixer.mixer.i_rate == p_filter->input.i_rate )
    {
        if( p_filter->b_continuity &&
            p_in_buf->i_size >=
              p_in_buf->i_nb_bytes + sizeof(float) * i_nb_channels )
        {
            /* output the whole thing with the last sample from last time */
            memmove( ((float *)(p_in_buf->p_buffer)) + i_nb_channels,
                     p_in_buf->p_buffer, p_in_buf->i_nb_bytes );
            memcpy( p_in_buf->p_buffer, p_prev_sample,
                    i_nb_channels * sizeof(float) );
        }
        p_filter->b_continuity = false;
        return;
    }

#ifdef HAVE_ALLOCA
    p_in = (float *)alloca( p_in_buf->i_nb_bytes );
#else
    p_in_orig = p_in = (float *)malloc( p_in_buf->i_nb_bytes );
#endif
    if( p_in == NULL )
    {
        return;
    }

    vlc_memcpy( p_in, p_in_buf->p_buffer, p_in_buf->i_nb_bytes );

    /* Take care of the previous input sample (if any) */
    if( !p_filter->b_continuity )
    {
        p_filter->b_continuity = true;
        p_sys->i_remainder = 0;
        aout_DateInit( &p_sys->end_date, p_filter->output.i_rate );
    }
    else
    {
        while( p_sys->i_remainder < p_filter->output.i_rate )
        {
            for( i_chan = i_nb_channels ; i_chan ; )
            {
                i_chan--;
                p_out[i_chan] = p_prev_sample[i_chan];
                p_out[i_chan] += ( ( p_in[i_chan] - p_prev_sample[i_chan] )
                                   * p_sys->i_remainder
                                   / p_filter->output.i_rate );
            }
            p_out += i_nb_channels;
              i_out++;

            p_sys->i_remainder += p_filter->input.i_rate;
        }
        p_sys->i_remainder -= p_filter->output.i_rate;
    }

    /* Take care of the current input samples (minus last one) */
    for( i_in = 0; i_in < i_in_nb - 1; i_in++ )
    {
        while( p_sys->i_remainder < p_filter->output.i_rate )
        {
            for( i_chan = i_nb_channels ; i_chan ; )
            {
                i_chan--;
                p_out[i_chan] = p_in[i_chan];
                p_out[i_chan] += ( ( p_in[i_chan + i_nb_channels]
                    - p_in[i_chan] )
                    * p_sys->i_remainder / p_filter->output.i_rate );
            }
            p_out += i_nb_channels;
              i_out++;

            p_sys->i_remainder += p_filter->input.i_rate;
        }

        p_in += i_nb_channels;
        p_sys->i_remainder -= p_filter->output.i_rate;
    }

    /* Backup the last input sample for next time */
    for( i_chan = i_nb_channels ; i_chan ; )
    {
        i_chan--;
        p_prev_sample[i_chan] = p_in[i_chan];
    }

    p_out_buf->i_nb_samples = i_out;
    p_out_buf->start_date = p_in_buf->start_date;

    if( p_in_buf->start_date !=
        aout_DateGet( &p_sys->end_date ) )
    {
        aout_DateSet( &p_sys->end_date, p_in_buf->start_date );
    }

    p_out_buf->end_date = aout_DateIncrement( &p_sys->end_date,
                                              p_out_buf->i_nb_samples );

    p_out_buf->i_nb_bytes = p_out_buf->i_nb_samples *
        i_nb_channels * sizeof(int32_t);

#ifndef HAVE_ALLOCA
    free( p_in_orig );
#endif

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
        p_filter->fmt_in.i_codec != VLC_FOURCC('f','l','3','2') )
    {
        return VLC_EGENERIC;
    }
 
    /* Allocate the memory needed to store the module's structure */
    p_filter->p_sys = p_sys = malloc( sizeof(struct filter_sys_t) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    p_sys->p_prev_sample = malloc(
        p_filter->fmt_in.audio.i_channels * sizeof(int32_t) );
    if( p_sys->p_prev_sample == NULL )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }
    aout_DateInit( &p_sys->end_date, p_filter->fmt_in.audio.i_rate );

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

/*****************************************************************************
 * Resample
 *****************************************************************************/
static block_t *Resample( filter_t *p_filter, block_t *p_block )
{
    aout_filter_t aout_filter;
    aout_buffer_t in_buf, out_buf;
    block_t *p_out;
    int i_out_size;
    int i_bytes_per_frame;

    if( !p_block || !p_block->i_samples )
    {
        if( p_block )
            block_Release( p_block );
        return NULL;
    }
 
    i_bytes_per_frame = p_filter->fmt_out.audio.i_channels *
                  p_filter->fmt_out.audio.i_bitspersample / 8;
 
    i_out_size = i_bytes_per_frame * ( 1 + (p_block->i_samples *
        p_filter->fmt_out.audio.i_rate / p_filter->fmt_in.audio.i_rate));

    p_out = p_filter->pf_audio_buffer_new( p_filter, i_out_size );
    if( !p_out )
    {
        msg_Warn( p_filter, "can't get output buffer" );
        block_Release( p_block );
        return NULL;
    }

    p_out->i_samples = i_out_size / i_bytes_per_frame;
    p_out->i_dts = p_block->i_dts;
    p_out->i_pts = p_block->i_pts;
    p_out->i_length = p_block->i_length;

    aout_filter.p_sys = (struct aout_filter_sys_t *)p_filter->p_sys;
    aout_filter.input = p_filter->fmt_in.audio;
    aout_filter.output = p_filter->fmt_out.audio;
    aout_filter.b_continuity = false;

    in_buf.p_buffer = p_block->p_buffer;
    in_buf.i_nb_bytes = p_block->i_buffer;
    in_buf.i_nb_samples = p_block->i_samples;
    out_buf.p_buffer = p_out->p_buffer;
    out_buf.i_nb_bytes = p_out->i_buffer;
    out_buf.i_nb_samples = p_out->i_samples;

    DoWork( (aout_instance_t *)p_filter, &aout_filter, &in_buf, &out_buf );

    block_Release( p_block );
 
    p_out->i_buffer = out_buf.i_nb_bytes;
    p_out->i_samples = out_buf.i_nb_samples;

    return p_out;
}
