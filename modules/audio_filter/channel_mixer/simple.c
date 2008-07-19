/*****************************************************************************
 * simple.c : simple channel mixer plug-in (only 7/7.1/5/5.1 -> Stereo for now)
 *****************************************************************************
 * Copyright (C) 2002, 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
 * Module descriptor
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static int  OpenFilter( vlc_object_t * );

vlc_module_begin();
    set_description( N_("Audio filter for simple channel mixing") );
    set_capability( "audio filter", 10 );
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_MISC );
    set_callbacks( Create, NULL );

    add_submodule();
    set_description( N_("audio filter for simple channel mixing") );
    set_capability( "audio filter2", 10 );
    set_callbacks( OpenFilter, NULL );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#define AOUT_CHANS_STEREO_FRONT  ( AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT )
#define AOUT_CHANS_STEREO_REAR   ( AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT )
#define AOUT_CHANS_STEREO_MIDDLE (AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT )

#define AOUT_CHANS_2_0 AOUT_CHANS_STEREO_FRONT
#define AOUT_CHANS_4_0 (AOUT_CHANS_STEREO_FRONT | AOUT_CHANS_STEREO_REAR )
#define AOUT_CHANS_5_0 ( AOUT_CHANS_4_0 | AOUT_CHAN_CENTER )
#define AOUT_CHANS_6_0 (AOUT_CHANS_STEREO_FRONT | AOUT_CHANS_STEREO_REAR | AOUT_CHANS_STEREO_MIDDLE )
#define AOUT_CHANS_7_0 ( AOUT_CHANS_6_0 | AOUT_CHAN_CENTER )

static bool IsSupported( const audio_format_t *p_input, const audio_format_t *p_output );

static void DoWork    ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                        aout_buffer_t * );

static block_t *Filter( filter_t *, block_t * );

/*****************************************************************************
 * Create: allocate trivial channel mixer
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if( !IsSupported( &p_filter->input, &p_filter->output ) )
        return -1;

    p_filter->pf_do_work = DoWork;
    p_filter->b_in_place = 0;

    return 0;
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
    float *p_dest = (float *)p_out_buf->p_buffer;
    const float *p_src = (const float *)p_in_buf->p_buffer;
    int i;

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes * i_output_nb / i_input_nb;

    if( p_filter->output.i_physical_channels == AOUT_CHANS_2_0 )
    {
        if( p_filter->input.i_physical_channels & AOUT_CHAN_MIDDLELEFT )
        for( i = p_in_buf->i_nb_samples; i--; )
        {
            *p_dest = p_src[6] + 0.5 * p_src[0] + p_src[2] / 4 + p_src[4] / 4;
            p_dest++;
            *p_dest = p_src[6] + 0.5 * p_src[1] + p_src[3] / 4 + p_src[5] / 4;
            p_dest++;

            p_src += 7;

            if( p_filter->input.i_physical_channels & AOUT_CHAN_LFE ) p_src++;
        }
        else
        for( i = p_in_buf->i_nb_samples; i--; )
        {
            *p_dest = p_src[4] + 0.5 * p_src[0] + 0.33 * p_src[2];
            p_dest++;
            *p_dest = p_src[4] + 0.5 * p_src[1] + 0.33 * p_src[3];
            p_dest++;

            p_src += 5;

            if( p_filter->input.i_physical_channels & AOUT_CHAN_LFE ) p_src++;
        }
    }
    else if( p_filter->output.i_physical_channels == AOUT_CHAN_CENTER )
    {
        if( p_filter->input.i_physical_channels & AOUT_CHAN_MIDDLELEFT )
        for( i = p_in_buf->i_nb_samples; i--; )
        {
            *p_dest = p_src[6] + p_src[0] / 4 + p_src[1] / 4 + p_src[2] / 8 + p_src[3] / 8 + p_src[4] / 8 + p_src[5] / 8;
            p_dest++;

            p_src += 7;

            if( p_filter->input.i_physical_channels & AOUT_CHAN_LFE ) p_src++;
        }
        else if( p_filter->input.i_physical_channels & AOUT_CHAN_REARLEFT )
        for( i = p_in_buf->i_nb_samples; i--; )
        {
            *p_dest = p_src[4] + p_src[0] / 4 + p_src[1] / 4 + p_src[2] / 6 + p_src[3] / 6;
            p_dest++;

            p_src += 5;

            if( p_filter->input.i_physical_channels & AOUT_CHAN_LFE ) p_src++;
        }
        else
        for( i = p_in_buf->i_nb_samples; i--; )
        {
            *p_dest = p_src[0] / 2 + p_src[1] / 2;
            p_dest++;

            p_src += 2;
        }
    }
    else
    {
        if( p_filter->input.i_physical_channels & AOUT_CHAN_MIDDLELEFT )
        for( i = p_in_buf->i_nb_samples; i--; )
        {
            *p_dest = p_src[6] + 0.5 * p_src[0] + p_src[2] / 6;
            p_dest++;
            *p_dest = p_src[6] + 0.5 * p_src[1] + p_src[3] / 6;
            p_dest++;
            *p_dest = p_src[2] / 6 +  p_src[4];
            p_dest++;
            *p_dest = p_src[3] / 6 +  p_src[5];
            p_dest++;

            p_src += 7;

            if( p_filter->input.i_physical_channels & AOUT_CHAN_LFE ) p_src++;
        }
        else
        for( i = p_in_buf->i_nb_samples; i--; )
        {
            *p_dest = p_src[4] + 0.5 * p_src[0];
            p_dest++;
            *p_dest = p_src[4] + 0.5 * p_src[1];
            p_dest++;
            *p_dest = p_src[2];
            p_dest++;
            *p_dest = p_src[3];
            p_dest++;

            p_src += 5;

            if( p_filter->input.i_physical_channels & AOUT_CHAN_LFE ) p_src++;
        }
    }
}

/*****************************************************************************
 * OpenFilter:
 *****************************************************************************/
static int OpenFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    audio_format_t fmt_in  = p_filter->fmt_in.audio;
    audio_format_t fmt_out = p_filter->fmt_out.audio;

    fmt_in.i_format = p_filter->fmt_in.i_codec;
    fmt_out.i_format = p_filter->fmt_out.i_codec;

    if( !IsSupported( &fmt_in, &fmt_out ) )
        return -1;

    p_filter->pf_audio_filter = Filter;

    return 0;
}

/*****************************************************************************
 * Filter:
 *****************************************************************************/
static block_t *Filter( filter_t *p_filter, block_t *p_block )
{
    aout_filter_t aout_filter;
    aout_buffer_t in_buf, out_buf;
    block_t *p_out;
    int i_out_size;

    if( !p_block || !p_block->i_samples )
    {
        if( p_block )
            block_Release( p_block );
        return NULL;
    }

    i_out_size = p_block->i_samples *
      p_filter->fmt_out.audio.i_bitspersample *
        p_filter->fmt_out.audio.i_channels / 8;

    p_out = p_filter->pf_audio_buffer_new( p_filter, i_out_size );
    if( !p_out )
    {
        msg_Warn( p_filter, "can't get output buffer" );
        block_Release( p_block );
        return NULL;
    }

    p_out->i_samples = p_block->i_samples;
    p_out->i_dts = p_block->i_dts;
    p_out->i_pts = p_block->i_pts;
    p_out->i_length = p_block->i_length;

    aout_filter.p_sys = (struct aout_filter_sys_t *)p_filter->p_sys;
    aout_filter.input = p_filter->fmt_in.audio;
    aout_filter.input.i_format = p_filter->fmt_in.i_codec;
    aout_filter.output = p_filter->fmt_out.audio;
    aout_filter.output.i_format = p_filter->fmt_out.i_codec;

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

/*****************************************************************************
 * Helpers:
 *****************************************************************************/
static bool IsSupported( const audio_format_t *p_input, const audio_format_t *p_output )
{
    if( p_input->i_format != VLC_FOURCC('f','l','3','2') ||
          p_input->i_format != p_output->i_format ||
          p_input->i_rate != p_output->i_rate )
        return false;

    if( p_input->i_physical_channels == p_output->i_physical_channels &&
        p_input->i_original_channels == p_output->i_original_channels )
    {
        return false;
    }

    /* Only conversion to Mono, Stereo and 4.0 right now */
    if( p_output->i_physical_channels != AOUT_CHAN_CENTER &&
        p_output->i_physical_channels != AOUT_CHANS_2_0 &&
        p_output->i_physical_channels != AOUT_CHANS_4_0 )
    {
        return false;
    }

    /* Only from 7/7.1/5/5.1/2.0 */
    if( (p_input->i_physical_channels & ~AOUT_CHAN_LFE) != AOUT_CHANS_7_0 &&
        (p_input->i_physical_channels & ~AOUT_CHAN_LFE) != AOUT_CHANS_5_0 &&
         p_input->i_physical_channels != AOUT_CHANS_2_0 )
    {
        return false;
    }

    /* Only if we downmix */
    if( aout_FormatNbChannels( p_input ) <= aout_FormatNbChannels( p_output ) )
        return false;

    return true;
}

