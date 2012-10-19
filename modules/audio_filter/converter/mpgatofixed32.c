/*****************************************************************************
 * mpgatofixed32.c: MPEG-1 & 2 audio layer I, II, III + MPEG 2.5 decoder,
 * using MAD (MPEG Audio Decoder)
 *****************************************************************************
 * Copyright (C) 2001-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman _at_ videolan _dot_ org>
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

#include <mad.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_block.h>
#include <vlc_filter.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenFilter ( vlc_object_t * );
static void CloseFilter( vlc_object_t * );
static block_t *Convert( filter_t *, block_t * );

/*****************************************************************************
 * Local structures
 *****************************************************************************/
struct filter_sys_t
{
    struct mad_stream mad_stream;
    struct mad_frame  mad_frame;
    struct mad_synth  mad_synth;

    int               i_reject_count;
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )
    set_description( N_("MPEG audio decoder") )
    set_capability( "audio converter", 100 )
    set_callbacks( OpenFilter, CloseFilter )
vlc_module_end ()

/*****************************************************************************
 * DoWork: decode an MPEG audio frame.
 *****************************************************************************/
static void DoWork( filter_t * p_filter,
                    block_t * p_in_buf, block_t * p_out_buf )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_buffer = p_in_buf->i_nb_samples * sizeof(vlc_fixed_t) *
                               aout_FormatNbChannels( &p_filter->fmt_out.audio );

    /* Do the actual decoding now. */
    mad_stream_buffer( &p_sys->mad_stream, p_in_buf->p_buffer,
                       p_in_buf->i_buffer );
    if ( mad_frame_decode( &p_sys->mad_frame, &p_sys->mad_stream ) == -1 )
    {
        msg_Dbg( p_filter, "libmad error: %s",
                  mad_stream_errorstr( &p_sys->mad_stream ) );
        p_sys->i_reject_count = 3;
    }
    else if( p_in_buf->i_flags & BLOCK_FLAG_DISCONTINUITY )
    {
        p_sys->i_reject_count = 3;
    }

    if( p_sys->i_reject_count > 0 )
    {
        memset( p_out_buf->p_buffer, 0, p_out_buf->i_buffer );
        p_sys->i_reject_count--;
        return;
    }


    mad_synth_frame( &p_sys->mad_synth, &p_sys->mad_frame );

    struct mad_pcm * p_pcm = &p_sys->mad_synth.pcm;
    unsigned int i_samples = p_pcm->length;
    mad_fixed_t const * p_left = p_pcm->samples[0];
    mad_fixed_t const * p_right = p_pcm->samples[1];
    mad_fixed_t * p_samples = (mad_fixed_t *)p_out_buf->p_buffer;

    assert( i_samples == p_out_buf->i_nb_samples );
    /* Interleave and keep buffers in mad_fixed_t format */
    if ( p_pcm->channels == 2 )
    {
        while ( i_samples-- )
        {
            *p_samples++ = *p_left++;
            *p_samples++ = *p_right++;
        }
    }
    else
    {
        assert( p_pcm->channels == 1 );
        memcpy( p_samples, p_left, i_samples * sizeof(mad_fixed_t) );
    }
}

/*****************************************************************************
 * OpenFilter:
 *****************************************************************************/
static int OpenFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    if( p_filter->fmt_in.audio.i_format != VLC_CODEC_MPGA &&
        p_filter->fmt_in.audio.i_format != VLC_FOURCC('m','p','g','3') )
        return VLC_EGENERIC;

    if( p_filter->fmt_out.audio.i_format != VLC_CODEC_FI32 )
        return VLC_EGENERIC;

    if( !AOUT_FMTS_SIMILAR( &p_filter->fmt_in.audio, &p_filter->fmt_out.audio ) )
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the module's structure */
    p_sys = p_filter->p_sys = malloc( sizeof(filter_sys_t) );
    if( p_sys == NULL )
        return -1;
    p_sys->i_reject_count = 0;

    p_filter->pf_audio_filter = Convert;

    /* Initialize libmad */
    mad_stream_init( &p_sys->mad_stream );
    mad_frame_init( &p_sys->mad_frame );
    mad_synth_init( &p_sys->mad_synth );
    mad_stream_options( &p_sys->mad_stream, MAD_OPTION_IGNORECRC );

    msg_Dbg( p_this, "%4.4s->%4.4s, bits per sample: %i",
             (char *)&p_filter->fmt_in.audio.i_format,
             (char *)&p_filter->fmt_out.audio.i_format,
             p_filter->fmt_out.audio.i_bitspersample );

    return 0;
}

/*****************************************************************************
 * CloseFilter : deallocate data structures
 *****************************************************************************/
static void CloseFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    mad_synth_finish( &p_sys->mad_synth );
    mad_frame_finish( &p_sys->mad_frame );
    mad_stream_finish( &p_sys->mad_stream );
    free( p_sys );
}

static block_t *Convert( filter_t *p_filter, block_t *p_block )
{
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

    DoWork( p_filter, p_block, p_out );

    block_Release( p_block );

    return p_out;
}
