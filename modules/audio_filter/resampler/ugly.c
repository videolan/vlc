/*****************************************************************************
 * ugly.c : ugly resampler (changes pitch)
 *****************************************************************************
 * Copyright (C) 2002, 2006 the VideoLAN team
 * $Id$
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
    set_description( N_("Audio filter for ugly resampling") );
    set_capability( "audio filter", 2 );
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_MISC );
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
          || p_filter->input.i_physical_channels
              != p_filter->output.i_physical_channels
          || p_filter->input.i_original_channels
              != p_filter->output.i_original_channels
          || (p_filter->input.i_format != VLC_FOURCC('f','l','3','2')
               && p_filter->input.i_format != VLC_FOURCC('f','i','3','2')) )
    {
        return VLC_EGENERIC;
    }

    p_filter->pf_do_work = DoWork;

    /* We don't want a new buffer to be created because we're not sure we'll
     * actually need to resample anything. */
    p_filter->b_in_place = true;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    int32_t *p_in, *p_out = (int32_t*)p_out_buf->p_buffer;
#ifndef HAVE_ALLOCA
    int32_t *p_in_orig;
#endif

    unsigned int i_nb_channels = aout_FormatNbChannels( &p_filter->input );
    unsigned int i_in_nb = p_in_buf->i_nb_samples;
    unsigned int i_out_nb = i_in_nb * p_filter->output.i_rate
                                    / p_filter->input.i_rate;
    unsigned int i_sample_bytes = i_nb_channels * sizeof(int32_t);
    unsigned int i_out, i_chan, i_remainder = 0;

    /* Check if we really need to run the resampler */
    if( p_aout->mixer.mixer.i_rate == p_filter->input.i_rate )
    {
        return;
    }

#ifdef HAVE_ALLOCA
    p_in = (int32_t *)alloca( p_in_buf->i_nb_bytes );
#else
    p_in_orig = p_in = (int32_t *)malloc( p_in_buf->i_nb_bytes );
#endif
    if( p_in == NULL )
    {
        return;
    }

    vlc_memcpy( p_in, p_in_buf->p_buffer, p_in_buf->i_nb_bytes );

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

#ifndef HAVE_ALLOCA
    free( p_in_orig );
#endif

}
