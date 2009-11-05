/*****************************************************************************
 * trivial.c : trivial resampler (skips samples or pads with zeroes)
 *****************************************************************************
 * Copyright (C) 2002-2009 the VideoLAN team
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
#include <vlc_filter.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );

static block_t *DoWork( filter_t *, block_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Audio filter for trivial resampling") )
    set_capability( "audio filter", 1 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_MISC )
    set_callbacks( Create, NULL )
vlc_module_end ()

/*****************************************************************************
 * Create: allocate trivial resampler
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t * p_filter = (filter_t *)p_this;

    if ( p_filter->fmt_in.audio.i_rate == p_filter->fmt_out.audio.i_rate
          || p_filter->fmt_in.audio.i_format != p_filter->fmt_out.audio.i_format
          || p_filter->fmt_in.audio.i_physical_channels
              != p_filter->fmt_out.audio.i_physical_channels
          || p_filter->fmt_in.audio.i_original_channels
              != p_filter->fmt_out.audio.i_original_channels
          || (p_filter->fmt_in.audio.i_format != VLC_CODEC_FL32
               && p_filter->fmt_in.audio.i_format != VLC_CODEC_FI32) )
    {
        return VLC_EGENERIC;
    }

    p_filter->pf_audio_filter = DoWork;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************/
static block_t *DoWork( filter_t * p_filter, block_t * p_block )
{
    /* Check if we really need to run the resampler */
    if( p_filter->fmt_out.audio.i_rate == p_filter->fmt_in.audio.i_rate )
        return p_block;

    int i_in_nb = p_block->i_nb_samples;
    int i_out_nb = i_in_nb * p_filter->fmt_out.audio.i_rate
                    / p_filter->fmt_in.audio.i_rate;
    int i_sample_bytes = aout_FormatNbChannels( &p_filter->fmt_in.audio )
                          * sizeof(int32_t);

    p_block = block_Realloc( p_block, 0, i_out_nb * i_sample_bytes );
    if( !p_block )
        return NULL;

    if( i_out_nb > i_in_nb )
    {
        /* Pad with zeroes. */
        memset( p_block->p_buffer + i_in_nb * i_sample_bytes,
                0, (i_out_nb - i_in_nb) * i_sample_bytes );
    }

    p_block->i_nb_samples = i_out_nb;
    p_block->i_length = p_block->i_nb_samples *
        CLOCK_FREQ / p_filter->fmt_out.audio.i_rate;
    return p_block;
}
