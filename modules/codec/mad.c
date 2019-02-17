/*****************************************************************************
 * mad.c: MPEG-1 & 2 audio layer I, II, III + MPEG 2.5 decoder,
 * using MAD (MPEG Audio Decoder)
 *****************************************************************************
 * Copyright (C) 2001-2016 VLC authors and VideoLAN
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman _at_ videolan _dot_ org>
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
 * NOTA BENE: this module requires the linking against a library which is
 * known to require licensing under the GNU General Public License version 2
 * (or later). Therefore, the result of compiling this module will normally
 * be subject to the terms of that later license.
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
#include <vlc_block.h>
#include <vlc_aout.h>
#include <vlc_codec.h>

#define MAD_BUFFER_GUARD 8

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

/*****************************************************************************
 * Local structures
 *****************************************************************************/
typedef struct
{
    struct mad_stream mad_stream;
    struct mad_frame  mad_frame;
    struct mad_synth  mad_synth;

    int               i_reject_count;
    block_t          *p_last_buf;
} decoder_sys_t;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )
    set_description( N_("MPEG audio layer I/II/III decoder") )
    set_capability( "audio decoder", 99 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * DecodeBlock: decode an MPEG audio frame.
 *****************************************************************************/
static block_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_out_buf = NULL, *p_last_buf = NULL;

    if( !pp_block )
    {
        /* Drain */
        p_last_buf = p_sys->p_last_buf;
        p_sys->p_last_buf = NULL;
        if( !p_last_buf )
            return NULL;
    }
    else
    {
        if( !*pp_block )
            return NULL;
        block_t *p_in_buf = *pp_block;
        *pp_block = NULL;

        if( p_in_buf->i_buffer < MAD_BUFFER_GUARD )
        {
            block_Release( p_in_buf );
            return NULL;
        }

        /* Buffers passed to the mad_stream_buffer() function need to ends with
         * the header (MAD_BUFFER_GUARD) of the following block. Therefore,
         * this DecodeBlock() function will always return the output buffer
         * corresponding to the last input buffer. */
        if( !p_sys->p_last_buf )
        {
            /* Wait for the next block */
            p_sys->p_last_buf = p_in_buf;
            return NULL;
        }
        p_last_buf = p_sys->p_last_buf;
        p_sys->p_last_buf = p_in_buf;

        /* Put the header of the current buffer at the end of the last one.
         * Normally, this won't do a real realloc() since VLC blocks are
         * allocated with pre and post padding */
        p_last_buf = block_Realloc( p_last_buf, 0,
                                    p_last_buf->i_buffer + MAD_BUFFER_GUARD );
        if( !p_last_buf )
            return NULL;
        memcpy( &p_last_buf->p_buffer[p_last_buf->i_buffer - MAD_BUFFER_GUARD],
                p_in_buf->p_buffer, MAD_BUFFER_GUARD);
    }

    mad_stream_buffer( &p_sys->mad_stream, p_last_buf->p_buffer,
                       p_last_buf->i_buffer );
    /* Do the actual decoding now (ignore EOF error when draining). */
    if ( mad_frame_decode( &p_sys->mad_frame, &p_sys->mad_stream ) == -1
     && ( pp_block != NULL || p_sys->mad_stream.error != MAD_ERROR_BUFLEN ) )
    {
        msg_Err( p_dec, "libmad error: %s",
                  mad_stream_errorstr( &p_sys->mad_stream ) );
        if( !MAD_RECOVERABLE( p_sys->mad_stream.error ) )
            p_sys->i_reject_count = 3;
    }
    else if( p_last_buf->i_flags & BLOCK_FLAG_DISCONTINUITY )
    {
        p_sys->i_reject_count = 3;
    }

    if( p_sys->i_reject_count > 0 )
        goto reject;

    mad_synth_frame( &p_sys->mad_synth, &p_sys->mad_frame );

    struct mad_pcm * p_pcm = &p_sys->mad_synth.pcm;
    unsigned int i_samples = p_pcm->length;
    mad_fixed_t const * p_left = p_pcm->samples[0];
    mad_fixed_t const * p_right = p_pcm->samples[1];
    p_out_buf = decoder_NewAudioBuffer( p_dec, i_samples );
    if( !p_out_buf )
        goto end;
    p_out_buf->i_dts = p_last_buf->i_dts;
    p_out_buf->i_pts = p_last_buf->i_pts;
    p_out_buf->i_length = p_last_buf->i_length;
    float *p_samples = (float *)p_out_buf->p_buffer;

    if (p_pcm->channels > p_dec->fmt_out.audio.i_channels)
    {
        msg_Err( p_dec, "wrong channels count (corrupt stream?): %u > %u",
                 p_pcm->channels, p_dec->fmt_out.audio.i_channels);
        p_sys->i_reject_count = 3;
        goto reject;
    }

    if( i_samples != p_out_buf->i_nb_samples )
    {
        msg_Err( p_dec, "unexpected samples count (corrupt stream?): "
                 "%u / %u", i_samples, p_out_buf->i_nb_samples );
        p_sys->i_reject_count = 3;
        goto reject;
    }

    /* Interleave and keep buffers in mad_fixed_t format */
    if ( p_pcm->channels == 2 )
    {
        while ( i_samples-- )
        {
            //assert( *p_left < MAD_F_ONE );
            //assert( *p_left >= -MAD_F_ONE );
            //assert( *p_right < MAD_F_ONE );
            //assert( *p_right >= -MAD_F_ONE );
            *p_samples++ = (float)*p_left++ / (float)MAD_F_ONE;
            *p_samples++ = (float)*p_right++ / (float)MAD_F_ONE;
        }
    }
    else
    {
        assert( p_pcm->channels == 1 );
        while ( i_samples-- )
        {
            //assert( *p_left < MAD_F_ONE );
            //assert( *p_left >= -MAD_F_ONE );
            *p_samples++ = (float)*p_left++ / (float)MAD_F_ONE;
        }
    }

end:
    block_Release( p_last_buf );
    return p_out_buf;
reject:
    p_sys->i_reject_count--;
    if( p_out_buf )
    {
        block_Release( p_out_buf );
        p_out_buf = NULL;
    }
    goto end;
}

static int DecodeAudio( decoder_t *p_dec, block_t *p_block )
{
    block_t **pp_block = p_block ? &p_block : NULL, *p_out;
    while( ( p_out = DecodeBlock( p_dec, pp_block ) ) != NULL )
        decoder_QueueAudio( p_dec, p_out );
    return VLCDEC_SUCCESS;
}

static void DecodeFlush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_last_buf )
        block_Release( p_sys->p_last_buf );
    p_sys->p_last_buf = NULL;
}

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys;

    if( ( p_dec->fmt_in.i_codec != VLC_CODEC_MPGA
     && p_dec->fmt_in.i_codec != VLC_CODEC_MP3
     && p_dec->fmt_in.i_codec != VLC_FOURCC('m','p','g','3') )
     || p_dec->fmt_in.audio.i_rate == 0
     || p_dec->fmt_in.audio.i_physical_channels == 0
     || p_dec->fmt_in.audio.i_bytes_per_frame == 0
     || p_dec->fmt_in.audio.i_frame_length == 0 )
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the module's structure */
    p_sys = p_dec->p_sys = malloc( sizeof(decoder_sys_t) );
    if( p_sys == NULL )
        return VLC_ENOMEM;
    p_sys->i_reject_count = 0;
    p_sys->p_last_buf = NULL;

    /* Initialize libmad */
    mad_stream_init( &p_sys->mad_stream );
    mad_frame_init( &p_sys->mad_frame );
    mad_synth_init( &p_sys->mad_synth );
    mad_stream_options( &p_sys->mad_stream, MAD_OPTION_IGNORECRC );

    p_dec->fmt_out.audio = p_dec->fmt_in.audio;
    p_dec->fmt_out.audio.i_format = VLC_CODEC_FL32;
    p_dec->fmt_out.i_codec = p_dec->fmt_out.audio.i_format;

    aout_FormatPrepare( &p_dec->fmt_out.audio );

    if( decoder_UpdateAudioFormat( p_dec ) )
    {
        Close( p_this );
        return VLC_EGENERIC;
    }

    p_dec->pf_decode = DecodeAudio;
    p_dec->pf_flush  = DecodeFlush;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close : deallocate data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    mad_synth_finish( &p_sys->mad_synth );
    mad_frame_finish( &p_sys->mad_frame );
    mad_stream_finish( &p_sys->mad_stream );
    if( p_sys->p_last_buf )
        block_Release( p_sys->p_last_buf );
    free( p_sys );
}
