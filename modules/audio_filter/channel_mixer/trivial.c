/*****************************************************************************
 * trivial.c : trivial channel mixer plug-in (drops unwanted channels)
 *****************************************************************************
 * Copyright (C) 2002, 2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
    set_description( N_("Audio filter for trivial channel mixing") )
    set_capability( "audio converter", 1 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_MISC )
    set_callbacks( Create, NULL )
vlc_module_end ()

/*****************************************************************************
 * Create: allocate trivial channel mixer
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t * p_filter = (filter_t *)p_this;

    if ( (p_filter->fmt_in.audio.i_physical_channels
           == p_filter->fmt_out.audio.i_physical_channels
           && p_filter->fmt_in.audio.i_original_channels
               == p_filter->fmt_out.audio.i_original_channels)
          || p_filter->fmt_in.audio.i_format != p_filter->fmt_out.audio.i_format
          || p_filter->fmt_in.audio.i_rate != p_filter->fmt_out.audio.i_rate
          || p_filter->fmt_in.audio.i_format != VLC_CODEC_FL32 )
    {
        return VLC_EGENERIC;
    }

    p_filter->pf_audio_filter = DoWork;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * SparseCopy: trivially downmix or upmix a buffer
 *****************************************************************************/
static void SparseCopy( float * p_dest, const float * p_src, size_t i_len,
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
static block_t *DoWork( filter_t * p_filter, block_t * p_in_buf )
{
    int i_input_nb = aout_FormatNbChannels( &p_filter->fmt_in.audio );
    int i_output_nb = aout_FormatNbChannels( &p_filter->fmt_out.audio );

    block_t *p_out_buf;
    if( i_input_nb >= i_output_nb )
    {
        p_out_buf = p_in_buf; /* mix in place */
        p_out_buf->i_buffer = p_in_buf->i_buffer / i_input_nb * i_output_nb;
    }
    else
    {
        p_out_buf = block_Alloc(
                              p_in_buf->i_buffer / i_input_nb * i_output_nb );
        if( !p_out_buf )
            goto out;
        p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
        p_out_buf->i_dts        = p_in_buf->i_dts;
        p_out_buf->i_pts        = p_in_buf->i_pts;
        p_out_buf->i_length     = p_in_buf->i_length;
    }

    float * p_dest = (float *)p_out_buf->p_buffer;
    const float * p_src = (float *)p_in_buf->p_buffer;

    if ( (p_filter->fmt_out.audio.i_original_channels & AOUT_CHAN_PHYSMASK)
                != (p_filter->fmt_in.audio.i_original_channels & AOUT_CHAN_PHYSMASK)
           && (p_filter->fmt_in.audio.i_original_channels & AOUT_CHAN_PHYSMASK)
                == (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT) )
    {
        int i;
        /* This is a bit special. */
        if ( !(p_filter->fmt_out.audio.i_original_channels & AOUT_CHAN_LEFT) )
        {
            p_src++;
        }
        if ( p_filter->fmt_out.audio.i_physical_channels == AOUT_CHAN_CENTER )
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
    else if ( p_filter->fmt_out.audio.i_original_channels
                                    & AOUT_CHAN_REVERSESTEREO )
    {
        /* Reverse-stereo mode */
        int i;
        for ( i = p_in_buf->i_nb_samples; i--; )
        {
            float i_tmp = p_src[0];
            p_dest[0] = p_src[1];
            p_dest[1] = i_tmp;

            p_dest += 2;
            p_src += 2;
        }
    }
    else
    {
        SparseCopy( p_dest, p_src, p_in_buf->i_nb_samples, i_output_nb,
                    i_input_nb );
    }
out:
    if( p_in_buf != p_out_buf )
        block_Release( p_in_buf );
    return p_out_buf;
}

