/*****************************************************************************
 * ugly.c : ugly resampler (changes pitch)
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: ugly.c,v 1.5 2002/11/11 22:27:01 gbazin Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include <errno.h>
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include "audio_output.h"
#include "aout_internal.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );

static void DoWork    ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                        aout_buffer_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("audio filter for ugly resampling") );
    set_capability( "audio filter", 5 );
    set_callbacks( Create, NULL );
vlc_module_end();

/*****************************************************************************
 * Create: allocate ugly resampler
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if ( p_filter->input.i_rate == p_filter->output.i_rate
          || p_filter->input.i_format != p_filter->output.i_format
          || p_filter->input.i_channels != p_filter->output.i_channels
          || (p_filter->input.i_format != VLC_FOURCC('f','l','3','2')
               && p_filter->input.i_format != VLC_FOURCC('f','i','3','2')) )
    {
        return VLC_EGENERIC;
    }

    p_filter->pf_do_work = DoWork;
    p_filter->b_in_place = VLC_FALSE;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    int32_t* p_in = (int32_t*)p_in_buf->p_buffer;
    int32_t* p_out = (int32_t*)p_out_buf->p_buffer;

    int i_nb_channels = aout_FormatNbChannels( &p_filter->input );
    int i_in_nb = p_in_buf->i_nb_samples;
    int i_out_nb = i_in_nb * p_filter->output.i_rate
                    / p_filter->input.i_rate;
    int i_sample_bytes = i_nb_channels * sizeof(int32_t);
    int i_out, i_chan, i_remainder = 0;

    for( i_out = i_out_nb ; i_out-- ; )
    {
        for( i_chan = i_nb_channels ; i_chan ; )
        {
            i_chan--;
            p_out[i_chan] = p_in[i_chan];
        }
        p_out += i_nb_channels;

        i_remainder += p_filter->input.i_rate;
        while( i_remainder >= p_filter->output.i_rate )
        {
            p_in += i_nb_channels;
            i_remainder -= p_filter->output.i_rate;
        }
    }

    p_out_buf->i_nb_samples = i_out_nb;
    p_out_buf->i_nb_bytes = i_out_nb * i_sample_bytes;
    p_out_buf->start_date = p_in_buf->start_date;
    p_out_buf->end_date = p_out_buf->start_date + p_out_buf->i_nb_samples *
        1000000 / p_filter->output.i_rate;
}
