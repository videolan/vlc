/*****************************************************************************
 * simple.c : simple channel mixer plug-in
 *****************************************************************************
 * Copyright (C) 2002, 2004, 2006-2009 VLC authors and VideoLAN
 * $Id$
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
#include <assert.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenFilter( vlc_object_t * );
static void CloseFilter( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("Audio filter for simple channel mixing") )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_MISC )
    set_capability( "audio converter", 10 )
    set_callbacks( OpenFilter, CloseFilter );
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct filter_sys_t
{
    void (*pf_dowork)(filter_t *, block_t *, block_t * );
};

/*****************************************************************************
 * IsSupported: can we downmix?
 *****************************************************************************/
static bool IsSupported( const audio_format_t *p_input, const audio_format_t *p_output )
{
    if( p_input->i_format != VLC_CODEC_FL32 ||
        p_input->i_format != p_output->i_format ||
        p_input->i_rate != p_output->i_rate )
    {
        return false;
    }

    if( p_input->i_physical_channels == p_output->i_physical_channels &&
        p_input->i_original_channels == p_output->i_original_channels )
    {
        return false;
    }

    /* Only conversion to Mono, Stereo, 4.0 and 5.1 */
    if( p_output->i_physical_channels != AOUT_CHAN_CENTER &&
        p_output->i_physical_channels != AOUT_CHANS_2_0 &&
        p_output->i_physical_channels != AOUT_CHANS_4_0 &&
        p_output->i_physical_channels != AOUT_CHANS_5_1 )
    {
        return false;
    }

    /* Only from 7.x/5.x/4.0/3.x/2.0
     * NB 5.X rear and middle are handled the same way
     * We don't support 2.1 -> 2.0 (trivial can do it)
     * TODO: We don't support any 8.1 input
     * TODO: We don't support any 6.x input
     * TODO: We don't support 4.0 rear and 4.0 middle
     * */
    if( (p_input->i_physical_channels & ~AOUT_CHAN_LFE) != AOUT_CHANS_7_0 &&
        (p_input->i_physical_channels)                  != AOUT_CHANS_6_1_MIDDLE &&
        (p_input->i_physical_channels & ~AOUT_CHAN_LFE) != AOUT_CHANS_5_0 &&
        (p_input->i_physical_channels & ~AOUT_CHAN_LFE) != AOUT_CHANS_5_0_MIDDLE &&
        (p_input->i_physical_channels & ~AOUT_CHAN_LFE) != AOUT_CHANS_4_CENTER_REAR &&
        (p_input->i_physical_channels & ~AOUT_CHAN_LFE) != AOUT_CHANS_3_0 &&
         p_input->i_physical_channels != AOUT_CHANS_2_0 )
    {
        return false;
    }

    /* Only downmixing */
    if( aout_FormatNbChannels( p_input ) <= aout_FormatNbChannels( p_output ) )
        return false;

    return true;
}

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


/*****************************************************************************
 * OpenFilter:
 *****************************************************************************/
static int OpenFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    audio_format_t fmt_in  = p_filter->fmt_in.audio;
    audio_format_t fmt_out = p_filter->fmt_out.audio;

    fmt_in.i_format = p_filter->fmt_in.i_codec;
    fmt_out.i_format = p_filter->fmt_out.i_codec;

    if( !IsSupported( &fmt_in, &fmt_out ) )
        return VLC_EGENERIC;

    p_filter->p_sys = malloc( sizeof(*p_sys) );
    if( unlikely(!p_filter->p_sys) )
        return VLC_ENOMEM;

    p_filter->pf_audio_filter = Filter;

    const unsigned i_input_physical = p_filter->fmt_in.audio.i_physical_channels;
    const bool b_input_7_0 = (i_input_physical & ~AOUT_CHAN_LFE) == AOUT_CHANS_7_0;
    const bool b_input_6_1 = !b_input_7_0 &&
                             i_input_physical == AOUT_CHANS_6_1_MIDDLE;
    const bool b_input_5_0 = !b_input_7_0 && !b_input_6_1 &&
                             ( (i_input_physical & AOUT_CHANS_5_0) == AOUT_CHANS_5_0 ||
                               (i_input_physical & AOUT_CHANS_5_0_MIDDLE) == AOUT_CHANS_5_0_MIDDLE );
    const bool b_input_4_center_rear =  !b_input_7_0 && !b_input_5_0 &&
                             (i_input_physical & ~AOUT_CHAN_LFE) == AOUT_CHANS_4_CENTER_REAR;
    const bool b_input_3_0 = !b_input_7_0 && !b_input_5_0 && !b_input_4_center_rear &&
                             (i_input_physical & ~AOUT_CHAN_LFE) == AOUT_CHANS_3_0;

    if( p_filter->fmt_out.audio.i_physical_channels == AOUT_CHANS_2_0 )
    {
        if( b_input_7_0 )
            p_filter->p_sys->pf_dowork = DoWork_7_x_to_2_0;
        else if( b_input_6_1 )
            p_filter->p_sys->pf_dowork = DoWork_6_1_to_2_0;
        else if( b_input_5_0 )
            p_filter->p_sys->pf_dowork = DoWork_5_x_to_2_0;
        else if( b_input_4_center_rear )
            p_filter->p_sys->pf_dowork = DoWork_4_0_to_2_0;
        else if( b_input_3_0 )
            p_filter->p_sys->pf_dowork = DoWork_3_x_to_2_0;
    }
    else if( p_filter->fmt_out.audio.i_physical_channels == AOUT_CHAN_CENTER )
    {
        if( b_input_7_0 )
            p_filter->p_sys->pf_dowork = DoWork_7_x_to_1_0;
        else if( b_input_5_0 )
            p_filter->p_sys->pf_dowork = DoWork_5_x_to_1_0;
        else if( b_input_4_center_rear )
            p_filter->p_sys->pf_dowork = DoWork_4_0_to_1_0;
        else if( b_input_3_0 )
            p_filter->p_sys->pf_dowork = DoWork_3_x_to_1_0;
        else
            p_filter->p_sys->pf_dowork = DoWork_2_x_to_1_0;
    }
    else if(p_filter->fmt_out.audio.i_physical_channels == AOUT_CHANS_4_0)
    {
        if( b_input_7_0 )
            p_filter->p_sys->pf_dowork = DoWork_7_x_to_4_0;
        else
            p_filter->p_sys->pf_dowork = DoWork_5_x_to_4_0;
    }
    else
    {
        assert( b_input_7_0 || b_input_6_1 );
        if( b_input_7_0 )
            p_filter->p_sys->pf_dowork = DoWork_7_x_to_5_x;
        else
            p_filter->p_sys->pf_dowork = DoWork_6_1_to_5_x;
    }

    return VLC_SUCCESS;
}

static void CloseFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *) p_this;
    filter_sys_t *p_sys = p_filter->p_sys;
    free( p_sys );
}

/*****************************************************************************
 * Filter:
 *****************************************************************************/
static block_t *Filter( filter_t *p_filter, block_t *p_block )
{
    filter_sys_t *p_sys = (filter_sys_t *)p_filter->p_sys;
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

    p_sys->pf_dowork( p_filter, p_block, p_out );

    block_Release( p_block );

    return p_out;
}

