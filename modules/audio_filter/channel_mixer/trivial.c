/*****************************************************************************
 * trivial.c : trivial channel mixer plug-in (drops unwanted channels)
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
    set_description( N_("Audio filter for trivial channel mixing") );
    set_capability( "audio filter", 1 );
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_MISC );
    set_callbacks( Create, NULL );
vlc_module_end();

/*****************************************************************************
 * Create: allocate trivial channel mixer
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if ( (p_filter->input.i_physical_channels
           == p_filter->output.i_physical_channels
           && p_filter->input.i_original_channels
               == p_filter->output.i_original_channels)
          || p_filter->input.i_format != p_filter->output.i_format
          || p_filter->input.i_rate != p_filter->output.i_rate
          || (p_filter->input.i_format != VLC_FOURCC('f','l','3','2')
               && p_filter->input.i_format != VLC_FOURCC('f','i','3','2')) )
    {
        return -1;
    }

    p_filter->pf_do_work = DoWork;
    if ( aout_FormatNbChannels( &p_filter->input )
           > aout_FormatNbChannels( &p_filter->output ) )
    {
        /* Downmixing */
        p_filter->b_in_place = 1;
    }
    else
    {
        /* Upmixing */
        p_filter->b_in_place = 0;
    }

    return 0;
}

/*****************************************************************************
 * SparseCopy: trivially downmix or upmix a buffer
 *****************************************************************************/
static void SparseCopy( int32_t * p_dest, const int32_t * p_src, size_t i_len,
                        int i_output_stride, int i_input_stride )
{
    int i;
    for ( i = i_len; i--; )
    {
        int j;
        for ( j = 0; j < i_output_stride; j++ )
        {
            p_dest[j] = p_src[j % i_input_stride];
        }
        p_src += i_input_stride;
        p_dest += i_output_stride;
    }
}

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    VLC_UNUSED(p_aout);
    int i_input_nb = aout_FormatNbChannels( &p_filter->input );
    int i_output_nb = aout_FormatNbChannels( &p_filter->output );
    int32_t * p_dest = (int32_t *)p_out_buf->p_buffer;
    int32_t * p_src = (int32_t *)p_in_buf->p_buffer;

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes * i_output_nb / i_input_nb;

    if ( (p_filter->output.i_original_channels & AOUT_CHAN_PHYSMASK)
                != (p_filter->input.i_original_channels & AOUT_CHAN_PHYSMASK)
           && (p_filter->input.i_original_channels & AOUT_CHAN_PHYSMASK)
                == (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT) )
    {
        int i;
        /* This is a bit special. */
        if ( !(p_filter->output.i_original_channels & AOUT_CHAN_LEFT) )
        {
            p_src++;
        }
        if ( p_filter->output.i_physical_channels == AOUT_CHAN_CENTER )
        {
            /* Mono mode */
            for ( i = p_in_buf->i_nb_samples; i--; )
            {
                *p_dest = *p_src;
                p_dest++;
                p_src += 2;
            }
        }
        else
        {
            /* Fake-stereo mode */
            for ( i = p_in_buf->i_nb_samples; i--; )
            {
                *p_dest = *p_src;
                p_dest++;
                *p_dest = *p_src;
                p_dest++;
                p_src += 2;
            }
        }
    }
    else if ( p_filter->output.i_original_channels
                                    & AOUT_CHAN_REVERSESTEREO )
    {
        /* Reverse-stereo mode */
        int i;
        for ( i = p_in_buf->i_nb_samples; i--; )
        {
            *p_dest = p_src[1];
            p_dest++;
            *p_dest = p_src[0];
            p_dest++;
            p_src += 2;
        }
    }
    else
    {
        SparseCopy( p_dest, p_src, p_in_buf->i_nb_samples, i_output_nb,
                    i_input_nb );
    }
}

