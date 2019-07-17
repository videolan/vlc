/*****************************************************************************
 * simple.c : simple channel mixer plug-in
 *****************************************************************************
 * Copyright (C) 2002, 2004, 2006-2009 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
#include <vlc_block.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenFilter( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("Audio filter for simple channel mixing") )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_MISC )
    set_capability( "audio converter", 10 )
    set_callback( OpenFilter );
vlc_module_end ()

static block_t *Filter( filter_t *, block_t * );

static void DoWork_7_x_to_2_0( filter_t * p_filter,  block_t * p_in_buf, block_t * p_out_buf ) {
    float *p_dest = (float *)p_out_buf->p_buffer;
    const float *p_src = (const float *)p_in_buf->p_buffer;
    for( int i = p_in_buf->i_nb_samples; i--; )
    {
        float ctr = p_src[6] * 0.7071f;
        *p_dest++ = ctr + p_src[0] + p_src[2] / 4 + p_src[4] / 4;
        *p_dest++ = ctr + p_src[1] + p_src[3] / 4 + p_src[5] / 4;

        p_src += 7;

        if( p_filter->fmt_in.audio.i_physical_channels & AOUT_CHAN_LFE ) p_src++;
    }
}

static void DoWork_6_1_to_2_0( filter_t *p_filter, block_t *p_in_buf,
                               block_t *p_out_buf )
{
    VLC_UNUSED(p_filter);
    float *p_dest = (float *)p_out_buf->p_buffer;
    const float *p_src = (const float *)p_in_buf->p_buffer;
    for( int i = p_in_buf->i_nb_samples; i--; )
    {
        float ctr = (p_src[2] + p_src[5]) * 0.7071f;
        *p_dest++ = p_src[0] + p_src[3] + ctr;
        *p_dest++ = p_src[1] + p_src[4] + ctr;

        p_src += 6;

        /* We always have LFE here */
        p_src++;
    }
}

static void DoWork_5_x_to_2_0( filter_t * p_filter,  block_t * p_in_buf, block_t * p_out_buf ) {
    float *p_dest = (float *)p_out_buf->p_buffer;
    const float *p_src = (const float *)p_in_buf->p_buffer;
    for( int i = p_in_buf->i_nb_samples; i--; )
    {
        *p_dest++ = p_src[0] + 0.7071f * (p_src[4] + p_src[2]);
        *p_dest++ = p_src[1] + 0.7071f * (p_src[4] + p_src[3]);

        p_src += 5;

        if( p_filter->fmt_in.audio.i_physical_channels & AOUT_CHAN_LFE ) p_src++;
    }
}

static void DoWork_4_0_to_2_0( filter_t * p_filter,  block_t * p_in_buf, block_t * p_out_buf ) {
    VLC_UNUSED(p_filter);
    float *p_dest = (float *)p_out_buf->p_buffer;
    const float *p_src = (const float *)p_in_buf->p_buffer;
    for( int i = p_in_buf->i_nb_samples; i--; )
    {
        *p_dest++ = p_src[2] + p_src[3] + 0.5f * p_src[0];
        *p_dest++ = p_src[2] + p_src[3] + 0.5f * p_src[1];
        p_src += 4;
    }
}

static void DoWork_3_x_to_2_0( filter_t * p_filter,  block_t * p_in_buf, block_t * p_out_buf ) {
    float *p_dest = (float *)p_out_buf->p_buffer;
    const float *p_src = (const float *)p_in_buf->p_buffer;
    for( int i = p_in_buf->i_nb_samples; i--; )
    {
        *p_dest++ = p_src[2] + 0.5f * p_src[0];
        *p_dest++ = p_src[2] + 0.5f * p_src[1];

        p_src += 3;

        if( p_filter->fmt_in.audio.i_physical_channels & AOUT_CHAN_LFE ) p_src++;
    }
}

static void DoWork_7_x_to_1_0( filter_t * p_filter,  block_t * p_in_buf, block_t * p_out_buf ) {
    float *p_dest = (float *)p_out_buf->p_buffer;
    const float *p_src = (const float *)p_in_buf->p_buffer;
    for( int i = p_in_buf->i_nb_samples; i--; )
    {
        *p_dest++ = p_src[6] + p_src[0] / 4 + p_src[1] / 4 + p_src[2] / 8 + p_src[3] / 8 + p_src[4] / 8 + p_src[5] / 8;

        p_src += 7;

        if( p_filter->fmt_in.audio.i_physical_channels & AOUT_CHAN_LFE ) p_src++;
    }
}

static void DoWork_5_x_to_1_0( filter_t * p_filter,  block_t * p_in_buf, block_t * p_out_buf ) {
    float *p_dest = (float *)p_out_buf->p_buffer;
    const float *p_src = (const float *)p_in_buf->p_buffer;
    for( int i = p_in_buf->i_nb_samples; i--; )
    {
        *p_dest++ = 0.7071f * (p_src[0] + p_src[1]) + p_src[4]
                     + 0.5f * (p_src[2] + p_src[3]);

        p_src += 5;

        if( p_filter->fmt_in.audio.i_physical_channels & AOUT_CHAN_LFE ) p_src++;
    }
}

static void DoWork_4_0_to_1_0( filter_t * p_filter,  block_t * p_in_buf, block_t * p_out_buf ) {
    VLC_UNUSED(p_filter);
    float *p_dest = (float *)p_out_buf->p_buffer;
    const float *p_src = (const float *)p_in_buf->p_buffer;
    for( int i = p_in_buf->i_nb_samples; i--; )
    {
        *p_dest++ = p_src[2] + p_src[3] + p_src[0] / 4 + p_src[1] / 4;
        p_src += 4;
    }
}

static void DoWork_3_x_to_1_0( filter_t * p_filter,  block_t * p_in_buf, block_t * p_out_buf ) {
    float *p_dest = (float *)p_out_buf->p_buffer;
    const float *p_src = (const float *)p_in_buf->p_buffer;
    for( int i = p_in_buf->i_nb_samples; i--; )
    {
        *p_dest++ = p_src[2] + p_src[0] / 4 + p_src[1] / 4;

        p_src += 3;

        if( p_filter->fmt_in.audio.i_physical_channels & AOUT_CHAN_LFE ) p_src++;
    }
}

static void DoWork_2_x_to_1_0( filter_t * p_filter,  block_t * p_in_buf, block_t * p_out_buf ) {
    VLC_UNUSED(p_filter);
    float *p_dest = (float *)p_out_buf->p_buffer;
    const float *p_src = (const float *)p_in_buf->p_buffer;
    for( int i = p_in_buf->i_nb_samples; i--; )
    {
        *p_dest++ = p_src[0] / 2 + p_src[1] / 2;

        p_src += 2;
    }
}

static void DoWork_7_x_to_4_0( filter_t * p_filter,  block_t * p_in_buf, block_t * p_out_buf ) {
    float *p_dest = (float *)p_out_buf->p_buffer;
    const float *p_src = (const float *)p_in_buf->p_buffer;
    for( int i = p_in_buf->i_nb_samples; i--; )
    {
        *p_dest++ = p_src[6] + 0.5f * p_src[0] + p_src[2] / 6;
        *p_dest++ = p_src[6] + 0.5f * p_src[1] + p_src[3] / 6;
        *p_dest++ = p_src[2] / 6 +  p_src[4];
        *p_dest++ = p_src[3] / 6 +  p_src[5];

        p_src += 7;

        if( p_filter->fmt_in.audio.i_physical_channels & AOUT_CHAN_LFE ) p_src++;
    }
}

static void DoWork_5_x_to_4_0( filter_t * p_filter,  block_t * p_in_buf, block_t * p_out_buf ) {
    float *p_dest = (float *)p_out_buf->p_buffer;
    const float *p_src = (const float *)p_in_buf->p_buffer;
    for( int i = p_in_buf->i_nb_samples; i--; )
    {
        float ctr = p_src[4] * 0.7071f;
        *p_dest++ = p_src[0] + ctr;
        *p_dest++ = p_src[1] + ctr;
        *p_dest++ = p_src[2];
        *p_dest++ = p_src[3];

        p_src += 5;

        if( p_filter->fmt_in.audio.i_physical_channels & AOUT_CHAN_LFE ) p_src++;
    }
}

static void DoWork_7_x_to_5_x( filter_t * p_filter,  block_t * p_in_buf, block_t * p_out_buf ) {
    float *p_dest = (float *)p_out_buf->p_buffer;
    const float *p_src = (const float *)p_in_buf->p_buffer;
    for( int i = p_in_buf->i_nb_samples; i--; )
    {
        *p_dest++ = p_src[0];
        *p_dest++ = p_src[1];
        *p_dest++ = (p_src[2] + p_src[4]) * 0.5f;
        *p_dest++ = (p_src[3] + p_src[5]) * 0.5f;
        *p_dest++ = p_src[6];

        p_src += 7;

        if( p_filter->fmt_in.audio.i_physical_channels & AOUT_CHAN_LFE &&
            p_filter->fmt_out.audio.i_physical_channels & AOUT_CHAN_LFE )
            *p_dest++ = *p_src++;
        else if( p_filter->fmt_in.audio.i_physical_channels & AOUT_CHAN_LFE ) p_src++;
    }
}

static void DoWork_6_1_to_5_x( filter_t * p_filter,  block_t * p_in_buf, block_t * p_out_buf ) {
    VLC_UNUSED(p_filter);
    float *p_dest = (float *)p_out_buf->p_buffer;
    const float *p_src = (const float *)p_in_buf->p_buffer;
    for( int i = p_in_buf->i_nb_samples; i--; )
    {
        *p_dest++ = p_src[0];
        *p_dest++ = p_src[1];
        *p_dest++ = (p_src[2] + p_src[4]) * 0.5f;
        *p_dest++ = (p_src[3] + p_src[4]) * 0.5f;
        *p_dest++ = p_src[5];

        p_src += 6;

        /* We always have LFE here */
        *p_dest++ = *p_src++;
    }
}

#if defined (CAN_COMPILE_NEON)
#include "simple_neon.h"
#define GET_WORK(in, out) GET_WORK_##in##_to_##out##_neon()
#else
#define GET_WORK(in, out) DoWork_##in##_to_##out
#endif

/*****************************************************************************
 * OpenFilter:
 *****************************************************************************/
static int OpenFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    void (*do_work)(filter_t *, block_t *, block_t *) = NULL;

    if( p_filter->fmt_in.audio.i_format != VLC_CODEC_FL32 ||
        p_filter->fmt_in.audio.i_format != p_filter->fmt_out.audio.i_format ||
        p_filter->fmt_in.audio.i_rate != p_filter->fmt_out.audio.i_rate ||
        aout_FormatNbChannels( &p_filter->fmt_in.audio) < 2 )
        return VLC_EGENERIC;

    uint32_t input = p_filter->fmt_in.audio.i_physical_channels;
    uint32_t output = p_filter->fmt_out.audio.i_physical_channels;

    /* Short circuit the common case of not remixing */
    if( input == output )
        return VLC_EGENERIC;

    const bool b_input_6_1 = input == AOUT_CHANS_6_1_MIDDLE;
    const bool b_input_4_center_rear = input == AOUT_CHANS_4_CENTER_REAR;

    input &= ~AOUT_CHAN_LFE;

    const bool b_input_7_x = input == AOUT_CHANS_7_0;
    const bool b_input_5_x = input == AOUT_CHANS_5_0
                          || input == AOUT_CHANS_5_0_MIDDLE;
    const bool b_input_3_x = input == AOUT_CHANS_3_0;

    /*
     * TODO: We don't support any 8.1 input
     * TODO: We don't support any 6.x input
     * TODO: We don't support 4.0 rear and 4.0 middle
     */
    if( output == AOUT_CHAN_CENTER )
    {
        if( b_input_7_x )
            do_work = GET_WORK(7_x,1_0);
        else if( b_input_5_x )
            do_work = GET_WORK(5_x,1_0);
        else if( b_input_4_center_rear )
            do_work = GET_WORK(4_0,1_0);
        else if( b_input_3_x )
            do_work = GET_WORK(3_x,1_0);
        else
            do_work = GET_WORK(2_x,1_0);
    }
    else if( output == AOUT_CHANS_2_0 )
    {
        if( b_input_7_x )
            do_work = GET_WORK(7_x,2_0);
        else if( b_input_6_1 )
            do_work = GET_WORK(6_1,2_0);
        else if( b_input_5_x )
            do_work = GET_WORK(5_x,2_0);
        else if( b_input_4_center_rear )
            do_work = GET_WORK(4_0,2_0);
        else if( b_input_3_x )
            do_work = GET_WORK(3_x,2_0);
    }
    else if( output == AOUT_CHANS_4_0 )
    {
        if( b_input_7_x )
            do_work = GET_WORK(7_x,4_0);
        else if( b_input_5_x )
            do_work = GET_WORK(5_x,4_0);
    }
    else if( (output & ~AOUT_CHAN_LFE) == AOUT_CHANS_5_0 ||
             (output & ~AOUT_CHAN_LFE) == AOUT_CHANS_5_0_MIDDLE )
    {
        if( b_input_7_x )
            do_work = GET_WORK(7_x,5_x);
        else if( b_input_6_1 )
            do_work = GET_WORK(6_1,5_x);
    }

    if( do_work == NULL )
        return VLC_EGENERIC;

    p_filter->pf_audio_filter = Filter;
    p_filter->p_sys = (void *)do_work;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Filter:
 *****************************************************************************/
static block_t *Filter( filter_t *p_filter, block_t *p_block )
{
    void (*work)(filter_t *, block_t *, block_t *) = (void *)p_filter->p_sys;

    if( !p_block || !p_block->i_nb_samples )
    {
        if( p_block )
            block_Release( p_block );
        return NULL;
    }

    size_t i_out_size = p_block->i_nb_samples *
      p_filter->fmt_out.audio.i_bitspersample *
        p_filter->fmt_out.audio.i_channels / 8;

    block_t *p_out = block_Alloc( i_out_size );
    if( !p_out )
    {
        msg_Warn( p_filter, "can't get output buffer" );
        block_Release( p_block );
        return NULL;
    }

    p_out->i_nb_samples = p_block->i_nb_samples;
    p_out->i_dts = p_block->i_dts;
    p_out->i_pts = p_block->i_pts;
    p_out->i_length = p_block->i_length;

    int i_input_nb = aout_FormatNbChannels( &p_filter->fmt_in.audio );
    int i_output_nb = aout_FormatNbChannels( &p_filter->fmt_out.audio );
    p_out->i_nb_samples = p_block->i_nb_samples;
    p_out->i_buffer = p_block->i_buffer * i_output_nb / i_input_nb;

    work( p_filter, p_block, p_out );

    block_Release( p_block );

    return p_out;
}

