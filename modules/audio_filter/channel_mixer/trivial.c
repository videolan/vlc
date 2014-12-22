/*****************************************************************************
 * trivial.c : trivial channel mixer plug-in (drops unwanted channels)
 *****************************************************************************
 * Copyright (C) 2002, 2006, 2014 VLC authors and VideoLAN
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

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_filter.h>

static int Create( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("Audio filter for trivial channel mixing") )
    set_capability( "audio converter", 1 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_MISC )
    set_callbacks( Create, NULL )
vlc_module_end ()

/**
 * Trivially upmixes
 */
static block_t *Upmix( filter_t *p_filter, block_t *p_in_buf )
{
    unsigned i_input_nb = aout_FormatNbChannels( &p_filter->fmt_in.audio );
    unsigned i_output_nb = aout_FormatNbChannels( &p_filter->fmt_out.audio );

    assert( i_input_nb < i_output_nb );

    block_t *p_out_buf = block_Alloc(
                              p_in_buf->i_buffer * i_output_nb / i_input_nb );
    if( unlikely(p_out_buf == NULL) )
    {
        block_Release( p_in_buf );
        return NULL;
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_dts        = p_in_buf->i_dts;
    p_out_buf->i_pts        = p_in_buf->i_pts;
    p_out_buf->i_length     = p_in_buf->i_length;

    float *p_dest = (float *)p_out_buf->p_buffer;
    const float *p_src = (float *)p_in_buf->p_buffer;

    for( size_t i = 0; i < p_in_buf->i_nb_samples; i++ )
    {
        for( unsigned j = 0; j < i_output_nb; j++ )
            p_dest[j] = p_src[j % i_input_nb];

        p_src += i_input_nb;
        p_dest += i_output_nb;
    }

    block_Release( p_in_buf );
    return p_out_buf;
}

/**
 * Trivially downmixes (i.e. drop extra channels)
 */
static block_t *Downmix( filter_t *p_filter, block_t *p_buf )
{
    unsigned i_input_nb = aout_FormatNbChannels( &p_filter->fmt_in.audio );
    unsigned i_output_nb = aout_FormatNbChannels( &p_filter->fmt_out.audio );

    assert( i_input_nb >= i_output_nb );

    float *p_dest = (float *)p_buf->p_buffer;
    const float *p_src = p_dest;

    for( size_t i = 0; i < p_buf->i_nb_samples; i++ )
    {
        for( unsigned j = 0; j < i_output_nb; j++ )
            p_dest[j] = p_src[j];

        p_src += i_input_nb;
        p_dest += i_output_nb;
    }

    return p_buf;
}

static block_t *CopyLeft( filter_t *p_filter, block_t *p_buf )
{
    float *p = (float *)p_buf->p_buffer;

    for( unsigned i = 0; i < p_buf->i_nb_samples; i++ )
    {
        p[1] = p[0];
        p += 2;
    }
    (void) p_filter;
    return p_buf;
}

static block_t *CopyRight( filter_t *p_filter, block_t *p_buf )
{
    float *p = (float *)p_buf->p_buffer;

    for( unsigned i = 0; i < p_buf->i_nb_samples; i++ )
    {
        p[0] = p[1];
        p += 2;
    }
    (void) p_filter;
    return p_buf;
}

static block_t *ExtractLeft( filter_t *p_filter, block_t *p_buf )
{
    float *p_dest = (float *)p_buf->p_buffer;
    const float *p_src = p_dest;

    for( unsigned i = 0; i < p_buf->i_nb_samples; i++ )
    {
        *(p_dest++) = *p_src;
        p_src += 2;
    }
    (void) p_filter;
    return p_buf;
}

static block_t *ExtractRight( filter_t *p_filter, block_t *p_buf )
{
    float *p_dest = (float *)p_buf->p_buffer;
    const float *p_src = p_dest;

    for( unsigned i = 0; i < p_buf->i_nb_samples; i++ )
    {
        p_src++;
        *(p_dest++) = *(p_src++);
    }
    (void) p_filter;
    return p_buf;
}

static block_t *ReverseStereo( filter_t *p_filter, block_t *p_buf )
{
    float *p = (float *)p_buf->p_buffer;

    /* Reverse-stereo mode */
    for( unsigned i = 0; i < p_buf->i_nb_samples; i++ )
    {
        float f = p[0];
        p[0] = p[1];
        p[1] = f;
        p += 2;
    }
    (void) p_filter;
    return p_buf;
}

/**
 * Probes the trivial channel mixer
 */
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    const audio_format_t *infmt = &p_filter->fmt_in.audio;
    const audio_format_t *outfmt = &p_filter->fmt_out.audio;

    if( infmt->i_format != outfmt->i_format
     || infmt->i_rate != outfmt->i_rate
     || infmt->i_format != VLC_CODEC_FL32 )
        return VLC_EGENERIC;
    if( infmt->i_physical_channels == outfmt->i_physical_channels
     && infmt->i_original_channels == outfmt->i_original_channels )
        return VLC_EGENERIC;

    if( outfmt->i_physical_channels == AOUT_CHANS_STEREO )
    {
        bool swap = (outfmt->i_original_channels & AOUT_CHAN_REVERSESTEREO)
                  != (infmt->i_original_channels & AOUT_CHAN_REVERSESTEREO);

        if( (outfmt->i_original_channels & AOUT_CHAN_PHYSMASK)
                                                            == AOUT_CHAN_LEFT )
        {
            p_filter->pf_audio_filter = swap ? CopyRight : CopyLeft;
            return VLC_SUCCESS;
        }

        if( (outfmt->i_original_channels & AOUT_CHAN_PHYSMASK)
                                                           == AOUT_CHAN_RIGHT )
        {
            p_filter->pf_audio_filter = swap ? CopyLeft : CopyRight;
            return VLC_SUCCESS;
        }

        if( swap )
        {
            p_filter->pf_audio_filter = ReverseStereo;
            return VLC_SUCCESS;
        }
    }

    if ( aout_FormatNbChannels( outfmt ) == 1 )
    {
        bool mono = !!(infmt->i_original_channels & AOUT_CHAN_DUALMONO);

        if( mono && (infmt->i_original_channels & AOUT_CHAN_LEFT) )
        {
            p_filter->pf_audio_filter = ExtractLeft;
            return VLC_SUCCESS;
        }

        if( mono && (infmt->i_original_channels & AOUT_CHAN_RIGHT) )
        {
            p_filter->pf_audio_filter = ExtractRight;
            return VLC_SUCCESS;
        }
    }

    if( aout_FormatNbChannels( outfmt ) > aout_FormatNbChannels( infmt ) )
        p_filter->pf_audio_filter = Upmix;
    else
        p_filter->pf_audio_filter = Downmix;

    return VLC_SUCCESS;
}
