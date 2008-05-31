/*****************************************************************************
 * trivial.c : trivial resampler (skips samples or pads with zeroes)
 *****************************************************************************
 * Copyright (C) 2002, 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
    set_description( N_("Audio filter for trivial resampling") );
    set_capability( "audio filter", 1 );
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_MISC );
    set_callbacks( Create, NULL );
vlc_module_end();

/*****************************************************************************
 * Create: allocate trivial resampler
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
        return -1;
    }

    p_filter->pf_do_work = DoWork;
    p_filter->b_in_place = true;

    return 0;
}

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    int i_in_nb = p_in_buf->i_nb_samples;
    int i_out_nb = i_in_nb * p_filter->output.i_rate
                    / p_filter->input.i_rate;
    int i_sample_bytes = aout_FormatNbChannels( &p_filter->input )
                          * sizeof(int32_t);

    /* Check if we really need to run the resampler */
    if( p_aout->mixer.mixer.i_rate == p_filter->input.i_rate )
    {
        return;
    }

    if ( p_out_buf != p_in_buf )
    {
        /* For whatever reason the buffer allocator decided to allocate
         * a new buffer. Currently, this never happens. */
        vlc_memcpy( p_out_buf->p_buffer, p_in_buf->p_buffer,
                    __MIN(i_out_nb, i_in_nb) * i_sample_bytes );
    }

    if ( i_out_nb > i_in_nb )
    {
        /* Pad with zeroes. */
        memset( p_out_buf->p_buffer + i_in_nb * i_sample_bytes,
                0, (i_out_nb - i_in_nb) * i_sample_bytes );
    }

    p_out_buf->i_nb_samples = i_out_nb;
    p_out_buf->i_nb_bytes = i_out_nb * i_sample_bytes;
    p_out_buf->start_date = p_in_buf->start_date;
    p_out_buf->end_date = p_out_buf->start_date + p_out_buf->i_nb_samples *
        1000000 / p_filter->output.i_rate;
}
