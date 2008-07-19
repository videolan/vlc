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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_block.h>
#include "vlc_filter.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );
static void DoWork    ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                        aout_buffer_t * );

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
vlc_module_begin();
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACODEC );
    set_description( N_("MPEG audio decoder") );
    set_capability( "audio filter", 100 );
    set_callbacks( Create, Destroy );

    add_submodule();
    set_description( N_("MPEG audio decoder") );
    set_capability( "audio filter2", 100 );
    set_callbacks( OpenFilter, CloseFilter );
vlc_module_end();

/*****************************************************************************
 * Create:
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_filter_t *p_filter = (aout_filter_t *)p_this;
    struct filter_sys_t *p_sys;

    if ( (p_filter->input.i_format != VLC_FOURCC('m','p','g','a')
           && p_filter->input.i_format != VLC_FOURCC('m','p','g','3'))
            || (p_filter->output.i_format != VLC_FOURCC('f','l','3','2')
                 && p_filter->output.i_format != VLC_FOURCC('f','i','3','2')) )
    {
        return -1;
    }

    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
    {
        return -1;
    }

    /* Allocate the memory needed to store the module's structure */
    p_sys = malloc( sizeof(filter_sys_t) );
    p_filter->p_sys = (struct aout_filter_sys_t *)p_sys;
    if( p_sys == NULL )
        return -1;

    /* Initialize libmad */
    mad_stream_init( &p_sys->mad_stream );
    mad_frame_init( &p_sys->mad_frame );
    mad_synth_init( &p_sys->mad_synth );
    mad_stream_options( &p_sys->mad_stream, MAD_OPTION_IGNORECRC );
    p_sys->i_reject_count = 0;

    p_filter->pf_do_work = DoWork;
    p_filter->b_in_place = 0;

    return 0;
}

/*****************************************************************************
 * DoWork: decode an MPEG audio frame.
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    filter_sys_t *p_sys = (filter_sys_t *)p_filter->p_sys;

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_samples * sizeof(vlc_fixed_t) *
                               aout_FormatNbChannels( &p_filter->output );

    /* Do the actual decoding now. */
    mad_stream_buffer( &p_sys->mad_stream, p_in_buf->p_buffer,
                       p_in_buf->i_nb_bytes );
    if ( mad_frame_decode( &p_sys->mad_frame, &p_sys->mad_stream ) == -1 )
    {
        msg_Dbg( p_aout, "libmad error: %s",
                  mad_stream_errorstr( &p_sys->mad_stream ) );
        p_sys->i_reject_count = 3;
    }
    else if( p_in_buf->b_discontinuity )
    {
        p_sys->i_reject_count = 3;
    }

    if( p_sys->i_reject_count > 0 )
    {
        if( p_filter->output.i_format == VLC_FOURCC('f','l','3','2') )
        {
            int i;
            int i_size = p_out_buf->i_nb_bytes / sizeof(float);

            float * a = (float *)p_out_buf->p_buffer;
            for ( i = 0 ; i < i_size ; i++ )
                *a++ = 0.0;
        }
        else
        {
            memset( p_out_buf->p_buffer, 0, p_out_buf->i_nb_bytes );
        }
        p_sys->i_reject_count--;
        return;
    }


    mad_synth_frame( &p_sys->mad_synth, &p_sys->mad_frame );

    if ( p_filter->output.i_format == VLC_FOURCC('f','i','3','2') )
    {
        /* Interleave and keep buffers in mad_fixed_t format */
        mad_fixed_t * p_samples = (mad_fixed_t *)p_out_buf->p_buffer;
        struct mad_pcm * p_pcm = &p_sys->mad_synth.pcm;
        unsigned int i_samples = p_pcm->length;
        mad_fixed_t const * p_left = p_pcm->samples[0];
        mad_fixed_t const * p_right = p_pcm->samples[1];

        switch ( p_pcm->channels )
        {
        case 2:
            if ( p_filter->output.i_physical_channels == AOUT_CHAN_CENTER )
            {
                while ( i_samples-- )
                {
                    *p_samples++ = (*p_left++ >> 1) + (*p_right++ >> 1);
                }
            }
            else if ( p_filter->output.i_original_channels == AOUT_CHAN_LEFT )
            {
                while ( i_samples-- )
                {
                    *p_samples++ = *p_left;
                    *p_samples++ = *p_left++;
                }
            }
            else if ( p_filter->output.i_original_channels == AOUT_CHAN_RIGHT )
            {
                while ( i_samples-- )
                {
                    *p_samples++ = *p_right;
                    *p_samples++ = *p_right++;
                }
            }
            else
            {
                while ( i_samples-- )
                {
                    *p_samples++ = *p_left++;
                    *p_samples++ = *p_right++;
                }
            }
            break;

        case 1:
            vlc_memcpy( p_samples, p_left, i_samples * sizeof(mad_fixed_t) );
            break;

        default:
            msg_Err( p_aout, "cannot interleave %i channels",
                     p_pcm->channels );
        }
    }
    else
    {
        /* float32 */
        float * p_samples = (float *)p_out_buf->p_buffer;
        struct mad_pcm * p_pcm = &p_sys->mad_synth.pcm;
        unsigned int i_samples = p_pcm->length;
        mad_fixed_t const * p_left = p_pcm->samples[0];
        mad_fixed_t const * p_right = p_pcm->samples[1];
        float f_temp = (float)FIXED32_ONE;

        switch ( p_pcm->channels )
        {
        case 2:
            if ( p_filter->output.i_physical_channels == AOUT_CHAN_CENTER )
            {
                while ( i_samples-- )
                {
                    *p_samples++ = (float)*p_left++ / f_temp / 2 +
                                   (float)*p_right++ / f_temp / 2;
                }
            }
            else if ( p_filter->output.i_original_channels == AOUT_CHAN_LEFT )
            {
                while ( i_samples-- )
                {
                    *p_samples++ = (float)*p_left / f_temp;
                    *p_samples++ = (float)*p_left++ / f_temp;
                }
            }
            else if ( p_filter->output.i_original_channels == AOUT_CHAN_RIGHT )
            {
                while ( i_samples-- )
                {
                    *p_samples++ = (float)*p_right / f_temp;
                    *p_samples++ = (float)*p_right++ / f_temp;
                }
            }
            else
            {
                while ( i_samples-- )
                {
                    *p_samples++ = (float)*p_left++ / f_temp;
                    *p_samples++ = (float)*p_right++ / f_temp;
                }
            }
            break;

        case 1:
            while ( i_samples-- )
            {
                *p_samples++ = (float)*p_left++ / f_temp;
            }
            break;

        default:
            msg_Err( p_aout, "cannot interleave %i channels",
                     p_pcm->channels );
        }
    }
}

/*****************************************************************************
 * Destroy : deallocate data structures
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    aout_filter_t *p_filter = (aout_filter_t *)p_this;
    filter_sys_t *p_sys = (filter_sys_t *)p_filter->p_sys;

    mad_synth_finish( &p_sys->mad_synth );
    mad_frame_finish( &p_sys->mad_frame );
    mad_stream_finish( &p_sys->mad_stream );
    free( p_sys );
}

/*****************************************************************************
 * OpenFilter:
 *****************************************************************************/
static int OpenFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    if( p_filter->fmt_in.i_codec != VLC_FOURCC('m','p','g','a') &&
        p_filter->fmt_in.i_codec != VLC_FOURCC('m','p','g','3') )
    {
        return VLC_EGENERIC;
    }

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

    if( vlc_CPU() & CPU_CAPABILITY_FPU )
        p_filter->fmt_out.i_codec = VLC_FOURCC('f','l','3','2');
    else
        p_filter->fmt_out.i_codec = VLC_FOURCC('f','i','3','2');
    p_filter->fmt_out.audio.i_format = p_filter->fmt_out.i_codec;
    p_filter->fmt_out.audio.i_bitspersample =
        aout_BitsPerSample( p_filter->fmt_out.i_codec );

    p_filter->fmt_out.audio.i_rate = p_filter->fmt_in.audio.i_rate;

    msg_Dbg( p_this, "%4.4s->%4.4s, bits per sample: %i",
             (char *)&p_filter->fmt_in.i_codec,
             (char *)&p_filter->fmt_out.i_codec,
             p_filter->fmt_in.audio.i_bitspersample );

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
    in_buf.b_discontinuity = false;
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
