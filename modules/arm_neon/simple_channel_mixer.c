/*****************************************************************************
 * simple_channel_mixer.c : simple channel mixer plug-in using NEON assembly
 *****************************************************************************
 * Copyright (C) 2002, 2004, 2006-2009, 2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          David Geldreich <david.geldreich@free.fr>
 *          Sébastien Toque
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
#include <vlc_cpu.h>
#include <assert.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenFilter( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("Audio filter for simple channel mixing using NEON assembly") )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_MISC )
    set_capability( "audio converter", 20 )
    set_callbacks( OpenFilter, NULL )
vlc_module_end ()

#define FILTER_WRAPPER(in, out)                                                  \
    void convert_##in##to##out##_neon_asm(float *dst, const float *src, int num, bool lfeChannel); \
    static block_t *Filter_##in##to##out (filter_t *p_filter, block_t *p_block)  \
    {                                                                            \
        block_t *p_out;                                                          \
        if (!FilterInit( p_filter, p_block, &p_out ))                            \
            return NULL;                                                         \
        const float *p_src = (const float *)p_block->p_buffer;                   \
        float *p_dest = (float *)p_out->p_buffer;                                \
        convert_##in##to##out##_neon_asm( p_dest, p_src, p_block->i_nb_samples,  \
                  p_filter->fmt_in.audio.i_physical_channels & AOUT_CHAN_LFE );  \
        block_Release( p_block );                                                \
        return p_out;                                                            \
    }

#define TRY_FILTER(in, out)                                \
    if ( b_input_##in && b_output_##out )                  \
    {                                                      \
        p_filter->pf_audio_filter = Filter_##in##to##out ; \
        return VLC_SUCCESS;                                \
    }

/*****************************************************************************
 * Filter:
 *****************************************************************************/
static bool FilterInit( filter_t *p_filter, block_t *p_block, block_t **pp_out )
{
    if( !p_block || !p_block->i_nb_samples )
    {
        if( p_block )
            block_Release( p_block );
        return false;
    }

    size_t i_out_size = p_block->i_nb_samples *
        p_filter->fmt_out.audio.i_bitspersample *
        p_filter->fmt_out.audio.i_channels / 8;

    block_t *p_out = block_Alloc( i_out_size );
    if( !p_out )
    {
        msg_Warn( p_filter, "can't get output buffer" );
        block_Release( p_block );
        return false;
    }

    p_out->i_nb_samples = p_block->i_nb_samples;
    p_out->i_dts = p_block->i_dts;
    p_out->i_pts = p_block->i_pts;
    p_out->i_length = p_block->i_length;

    int i_input_nb = aout_FormatNbChannels( &p_filter->fmt_in.audio );
    int i_output_nb = aout_FormatNbChannels( &p_filter->fmt_out.audio );
    p_out->i_buffer = p_block->i_buffer * i_output_nb / i_input_nb;

    *pp_out = p_out;
    return true;
}

FILTER_WRAPPER(7,2)
FILTER_WRAPPER(5,2)
FILTER_WRAPPER(4,2)
FILTER_WRAPPER(3,2)
FILTER_WRAPPER(7,1)
FILTER_WRAPPER(5,1)
FILTER_WRAPPER(7,4)
FILTER_WRAPPER(5,4)

/*****************************************************************************
 * OpenFilter:
 *****************************************************************************/
static int OpenFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    if (!vlc_CPU_ARM_NEON())
        return VLC_EGENERIC;

    audio_format_t fmt_in  = p_filter->fmt_in.audio;
    audio_format_t fmt_out = p_filter->fmt_out.audio;

    fmt_in.i_format = p_filter->fmt_in.i_codec;
    fmt_out.i_format = p_filter->fmt_out.i_codec;

    if( fmt_in.i_format != VLC_CODEC_FL32 ||
        fmt_in.i_format != fmt_out.i_format ||
        fmt_in.i_rate != fmt_out.i_rate )
    {
        return VLC_EGENERIC;
    }

    if( fmt_in.i_physical_channels == fmt_out.i_physical_channels &&
        fmt_in.i_original_channels == fmt_out.i_original_channels )
    {
        return VLC_EGENERIC;
    }

    const bool b_input_7 = (fmt_in.i_physical_channels & ~AOUT_CHAN_LFE) == AOUT_CHANS_7_0;
    const bool b_input_5 = ( (fmt_in.i_physical_channels & AOUT_CHANS_5_0) == AOUT_CHANS_5_0 ||
                             (fmt_in.i_physical_channels & AOUT_CHANS_5_0_MIDDLE) == AOUT_CHANS_5_0_MIDDLE );
    const bool b_input_4 =  (fmt_in.i_physical_channels & ~AOUT_CHAN_LFE) == AOUT_CHANS_4_CENTER_REAR;
    const bool b_input_3 = (fmt_in.i_physical_channels & ~AOUT_CHAN_LFE) == AOUT_CHANS_3_0;

    const bool b_output_1 = fmt_out.i_physical_channels == AOUT_CHAN_CENTER;
    const bool b_output_2 = fmt_out.i_physical_channels == AOUT_CHANS_2_0;
    const bool b_output_4 = fmt_out.i_physical_channels == AOUT_CHANS_4_0;

    /* Only conversion to Mono, Stereo and 4.0 right now */
    /* Only from 7/7.1/5/5.1/3/3.1/2.0
     * XXX 5.X rear and middle are handled the same way */

    TRY_FILTER(7,2)
    TRY_FILTER(5,2)
    TRY_FILTER(4,2)
    TRY_FILTER(3,2)
    TRY_FILTER(7,1)
    TRY_FILTER(5,1)
    TRY_FILTER(7,4)
    TRY_FILTER(5,4)

    return VLC_EGENERIC;
}
