/*****************************************************************************
 * linear.c : linear interpolation resampler
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: linear.c,v 1.10 2003/03/04 03:27:40 gbazin Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *          Sigmund Augdal <sigmunau@idi.ntnu.no>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include "audio_output.h"
#include "aout_internal.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Close     ( vlc_object_t * );
static void DoWork    ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                        aout_buffer_t * );

/*****************************************************************************
 * Local structures
 *****************************************************************************/
struct aout_filter_sys_t
{
    int32_t *p_prev_sample;       /* this filter introduces a 1 sample delay */

    unsigned int i_remainder;                /* remainder of previous sample */

    audio_date_t end_date;
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("audio filter for linear interpolation resampling") );
    set_capability( "audio filter", 2 );
    set_callbacks( Create, Close );
vlc_module_end();

/*****************************************************************************
 * Create: allocate linear resampler
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;
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
    p_filter->p_sys = malloc( sizeof(struct aout_filter_sys_t) );
    if( p_filter->p_sys == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }
    p_filter->p_sys->p_prev_sample = malloc(
        aout_FormatNbChannels( &p_filter->input ) * sizeof(int32_t) );
    if( p_filter->p_sys->p_prev_sample == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }

    p_filter->pf_do_work = DoWork;

    /* We don't want a new buffer to be created because we're not sure we'll
     * actually need to resample anything. */
    p_filter->b_in_place = VLC_TRUE;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: free our resources
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;
    free( p_filter->p_sys->p_prev_sample );
    free( p_filter->p_sys );
}

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    float *p_in, *p_out = (float *)p_out_buf->p_buffer;
    float *p_prev_sample = (float *)p_filter->p_sys->p_prev_sample;

    int i_nb_channels = aout_FormatNbChannels( &p_filter->input );
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
        p_filter->b_continuity = VLC_FALSE;
        return;
    }

#ifdef HAVE_ALLOCA
    p_in = (float *)alloca( p_in_buf->i_nb_bytes );
#else
    p_in = (float *)malloc( p_in_buf->i_nb_bytes );
#endif
    if( p_in == NULL )
    {
        return;
    }

    p_aout->p_vlc->pf_memcpy( p_in, p_in_buf->p_buffer, p_in_buf->i_nb_bytes );

    /* Take care of the previous input sample (if any) */
    if( !p_filter->b_continuity )
    {
        p_filter->b_continuity = VLC_TRUE;
	p_filter->p_sys->i_remainder = 0;
        aout_DateInit( &p_filter->p_sys->end_date, p_filter->output.i_rate );
    }
    else
    {
        while( p_filter->p_sys->i_remainder < p_filter->output.i_rate )
        {
            for( i_chan = i_nb_channels ; i_chan ; )
            {
                i_chan--;
                p_out[i_chan] = p_prev_sample[i_chan];
	        p_out[i_chan] += ( (p_prev_sample[i_chan] - p_in[i_chan])
				   * p_filter->p_sys->i_remainder
				   / p_filter->output.i_rate );
            }
            p_out += i_nb_channels;
  	    i_out++;

            p_filter->p_sys->i_remainder += p_filter->input.i_rate;
        }
        p_filter->p_sys->i_remainder -= p_filter->output.i_rate;
    }

    /* Take care of the current input samples (minus last one) */
    for( i_in = 0; i_in < i_in_nb - 1; i_in++ )
    {
        while( p_filter->p_sys->i_remainder < p_filter->output.i_rate )
        {
            for( i_chan = i_nb_channels ; i_chan ; )
            {
                i_chan--;
                p_out[i_chan] = p_in[i_chan];
	        p_out[i_chan] += ( (p_in[i_chan] -
	            p_in[i_chan + i_nb_channels])
		    * p_filter->p_sys->i_remainder / p_filter->output.i_rate );
            }
            p_out += i_nb_channels;
  	    i_out++;

            p_filter->p_sys->i_remainder += p_filter->input.i_rate;
        }

        p_in += i_nb_channels;
        p_filter->p_sys->i_remainder -= p_filter->output.i_rate;
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
	aout_DateGet( &p_filter->p_sys->end_date ) )
    {
        aout_DateSet( &p_filter->p_sys->end_date, p_in_buf->start_date );
    }

    p_out_buf->end_date = aout_DateIncrement( &p_filter->p_sys->end_date,
                                              p_out_buf->i_nb_samples );

    p_out_buf->i_nb_bytes = p_out_buf->i_nb_samples *
        i_nb_channels * sizeof(int32_t);

}
