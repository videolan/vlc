/*****************************************************************************
 * ugly.c : linear interpolation resampler
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: linear.c,v 1.1 2002/11/07 21:09:59 sigmunau Exp $
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
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
    set_description( _("audio filter for linear interpolation resampling") );
    set_capability( "audio filter", 10 );
    set_callbacks( Create, NULL );
vlc_module_end();

/*****************************************************************************
 * Create: allocate ugly resampler
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;
    msg_Dbg( p_this, " trying the linear resampler");
    if ( p_filter->input.i_rate == p_filter->output.i_rate
          || p_filter->input.i_format != p_filter->output.i_format
          || p_filter->input.i_channels != p_filter->output.i_channels
          || p_filter->input.i_format != VLC_FOURCC('f','l','3','2') )
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
    float* p_in = (float*)p_in_buf->p_buffer;
    float* p_out = (float*)p_out_buf->p_buffer;

    int i_nb_channels = aout_FormatNbChannels( &p_filter->input );
    int i_in_nb = p_in_buf->i_nb_samples;
    int i_out_nb = i_in_nb * p_filter->output.i_rate
                    / p_filter->input.i_rate;
    int i_frame_bytes = i_nb_channels * sizeof(s32);
    int i_in, i_chan, i_out = 0;
    double f_step = (float)i_in_nb/i_out_nb;
    float f_pos = 1;
    for( i_in = 0 ; i_in < i_in_nb - 1; i_in++ )
    {
        f_pos--;
        while( f_pos < 1 )
        {
            for( i_chan = i_nb_channels ; i_chan ; )
            {
                i_chan--;
                p_out[i_chan] = p_in[i_chan] +
                    ( p_in[i_chan + i_nb_channels] - p_in[i_chan] ) * f_pos;
                i_out++;
            }
            f_pos += f_step;
            p_out += i_nb_channels;
        }
        p_in += i_nb_channels;
    }
    if ( f_step < 1 ) {
        for( i_chan = i_nb_channels ; i_chan ; )
        {
            i_chan--;
            p_out[i_chan] = p_in[i_chan];
            i_out++;
        }
    }
    if ( i_out != i_out_nb * i_nb_channels ) {
        msg_Warn( p_aout, "mismatch in sample nubers: %d requested, %d generated", i_out_nb* i_nb_channels, i_out);
    }
                                              
    p_out_buf->i_nb_samples = i_out_nb;
    p_out_buf->i_nb_bytes = i_out_nb * i_frame_bytes;
}

