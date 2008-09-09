/*****************************************************************************
 * audio.c: audio decoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2003 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <vlc_aout.h>
#include <vlc_codec.h>
#include <vlc_input.h>

/* ffmpeg header */
#ifdef HAVE_LIBAVCODEC_AVCODEC_H
#   include <libavcodec/avcodec.h>
#elif defined(HAVE_FFMPEG_AVCODEC_H)
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

#include "avcodec.h"

static const unsigned int pi_channels_maps[7] =
{
    0,
    AOUT_CHAN_CENTER,   AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE
};

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    FFMPEG_COMMON_MEMBERS

    /* Temporary buffer for libavcodec */
    uint8_t *p_output;

    /*
     * Output properties
     */
    audio_sample_format_t aout_format;
    audio_date_t          end_date;

    /*
     *
     */
    uint8_t *p_samples;
    int     i_samples;

    /* */
    int     i_reject_count;

    int i_input_rate;
};

/*****************************************************************************
 * InitAudioDec: initialize audio decoder
 *****************************************************************************
 * The ffmpeg codec will be opened, some memory allocated.
 *****************************************************************************/
int InitAudioDec( decoder_t *p_dec, AVCodecContext *p_context,
                      AVCodec *p_codec, int i_codec_id, const char *psz_namecodec )
{
    decoder_sys_t *p_sys;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
    {
        return VLC_ENOMEM;
    }

    p_sys->p_context = p_context;
    p_sys->p_codec = p_codec;
    p_sys->i_codec_id = i_codec_id;
    p_sys->psz_namecodec = psz_namecodec;

    /* ***** Fill p_context with init values ***** */
    p_sys->p_context->sample_rate = p_dec->fmt_in.audio.i_rate;
    p_sys->p_context->channels = p_dec->fmt_in.audio.i_channels;
    if( !p_dec->fmt_in.audio.i_physical_channels )
    {
        msg_Warn( p_dec, "Physical channel configuration not set : guessing" );
        p_dec->fmt_in.audio.i_original_channels =
            p_dec->fmt_in.audio.i_physical_channels =
                pi_channels_maps[p_sys->p_context->channels];
    }

    p_dec->fmt_out.audio.i_physical_channels =
        p_dec->fmt_out.audio.i_original_channels =
        p_dec->fmt_in.audio.i_physical_channels;

    p_sys->p_context->block_align = p_dec->fmt_in.audio.i_blockalign;
    p_sys->p_context->bit_rate = p_dec->fmt_in.i_bitrate;
#if LIBAVCODEC_VERSION_INT < ((52<<16)+(0<<8)+0)
    p_sys->p_context->bits_per_sample = p_dec->fmt_in.audio.i_bitspersample;
#else
    p_sys->p_context->bits_per_coded_sample = p_dec->fmt_in.audio.i_bitspersample;
#endif

    if( p_dec->fmt_in.i_extra > 0 )
    {
        const uint8_t * const p_src = p_dec->fmt_in.p_extra;
        int i_offset;
        int i_size;

        if( p_dec->fmt_in.i_codec == VLC_FOURCC( 'f', 'l', 'a', 'c' ) )
        {
            i_offset = 8;
            i_size = p_dec->fmt_in.i_extra - 8;
        }
        else if( p_dec->fmt_in.i_codec == VLC_FOURCC( 'a', 'l', 'a', 'c' ) )
        {
            static const uint8_t p_pattern[] = { 0, 0, 0, 36, 'a', 'l', 'a', 'c' };
            /* Find alac atom XXX it is a bit ugly */
            for( i_offset = 0; i_offset < p_dec->fmt_in.i_extra - sizeof(p_pattern); i_offset++ )
            {
                if( !memcmp( &p_src[i_offset], p_pattern, sizeof(p_pattern) ) )
                    break;
            }
            i_size = __MIN( p_dec->fmt_in.i_extra - i_offset, 36 );
            if( i_size < 36 )
                i_size = 0;
        }
        else
        {
            i_offset = 0;
            i_size = p_dec->fmt_in.i_extra;
        }

        if( i_size > 0 )
        {
            p_sys->p_context->extradata =
                malloc( i_size + FF_INPUT_BUFFER_PADDING_SIZE );
            if( p_sys->p_context->extradata )
            {
                uint8_t *p_dst = p_sys->p_context->extradata;

                p_sys->p_context->extradata_size = i_size;

                memcpy( &p_dst[0],            &p_src[i_offset], i_size );
                memset( &p_dst[i_size], 0, FF_INPUT_BUFFER_PADDING_SIZE );
            }
        }
    }
    else
    {
        p_sys->p_context->extradata_size = 0;
        p_sys->p_context->extradata = NULL;
    }

    /* ***** Open the codec ***** */
    vlc_mutex_t *lock = var_AcquireMutex( "avcodec" );
    if( lock == NULL )
    {
        free( p_sys->p_context->extradata );
        free( p_sys );
        return VLC_ENOMEM;
    }

    if (avcodec_open( p_sys->p_context, p_sys->p_codec ) < 0)
    {
        vlc_mutex_unlock( lock );
        msg_Err( p_dec, "cannot open codec (%s)", p_sys->psz_namecodec );
        free( p_sys->p_context->extradata );
        free( p_sys );
        return VLC_EGENERIC;
    }
    vlc_mutex_unlock( lock );

    msg_Dbg( p_dec, "ffmpeg codec (%s) started", p_sys->psz_namecodec );

    p_sys->p_output = malloc( AVCODEC_MAX_AUDIO_FRAME_SIZE );
    p_sys->p_samples = NULL;
    p_sys->i_samples = 0;
    p_sys->i_reject_count = 0;
    p_sys->i_input_rate = INPUT_RATE_DEFAULT;

    aout_DateSet( &p_sys->end_date, 0 );
    if( p_dec->fmt_in.audio.i_rate )
        aout_DateInit( &p_sys->end_date, p_dec->fmt_in.audio.i_rate );

    /* Set output properties */
    p_dec->fmt_out.i_cat = AUDIO_ES;
    p_dec->fmt_out.i_codec = AOUT_FMT_S16_NE;
    p_dec->fmt_out.audio.i_bitspersample = 16;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SplitBuffer: Needed because aout really doesn't like big audio chunk and
 * wma produces easily > 30000 samples...
 *****************************************************************************/
static aout_buffer_t *SplitBuffer( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_samples = __MIN( p_sys->i_samples, 4096 );
    aout_buffer_t *p_buffer;

    if( i_samples == 0 ) return NULL;

    if( ( p_buffer = p_dec->pf_aout_buffer_new( p_dec, i_samples ) ) == NULL )
    {
        msg_Err( p_dec, "cannot get aout buffer" );
        return NULL;
    }

    p_buffer->start_date = aout_DateGet( &p_sys->end_date );
    p_buffer->end_date = aout_DateIncrement( &p_sys->end_date,
                                             i_samples * p_sys->i_input_rate / INPUT_RATE_DEFAULT );

    memcpy( p_buffer->p_buffer, p_sys->p_samples, p_buffer->i_nb_bytes );

    p_sys->p_samples += p_buffer->i_nb_bytes;
    p_sys->i_samples -= i_samples;

    return p_buffer;
}

/*****************************************************************************
 * DecodeAudio: Called to decode one frame
 *****************************************************************************/
aout_buffer_t * DecodeAudio ( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_used, i_output;
    aout_buffer_t *p_buffer;
    block_t *p_block;

    if( !pp_block || !*pp_block ) return NULL;

    p_block = *pp_block;

    if( p_block->i_rate > 0 )
        p_sys->i_input_rate = p_block->i_rate;

    if( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
        block_Release( p_block );
        avcodec_flush_buffers( p_sys->p_context );

        if( p_sys->i_codec_id == CODEC_ID_MP2 || p_sys->i_codec_id == CODEC_ID_MP3 )
            p_sys->i_reject_count = 3;
        return NULL;
    }

    if( p_sys->i_samples > 0 )
    {
        /* More data */
        p_buffer = SplitBuffer( p_dec );
        if( !p_buffer ) block_Release( p_block );
        return p_buffer;
    }

    if( !aout_DateGet( &p_sys->end_date ) && !p_block->i_pts )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( p_block );
        return NULL;
    }

    if( p_block->i_buffer <= 0 )
    {
        block_Release( p_block );
        return NULL;
    }
    if( p_block->i_buffer > AVCODEC_MAX_AUDIO_FRAME_SIZE )
    {
        /* Grow output buffer if necessary (eg. for PCM data) */
        p_sys->p_output = realloc(p_sys->p_output, p_block->i_buffer);
    }

    *pp_block = p_block = block_Realloc( p_block, 0, p_block->i_buffer + FF_INPUT_BUFFER_PADDING_SIZE );
    if( !p_block )
        return NULL;
    p_block->i_buffer -= FF_INPUT_BUFFER_PADDING_SIZE;
    memset( &p_block->p_buffer[p_block->i_buffer], 0, FF_INPUT_BUFFER_PADDING_SIZE );

#if LIBAVCODEC_VERSION_INT >= ((52<<16)+(0<<8)+0)
    i_output = __MAX( AVCODEC_MAX_AUDIO_FRAME_SIZE, p_block->i_buffer );
    i_used = avcodec_decode_audio2( p_sys->p_context,
                                   (int16_t*)p_sys->p_output, &i_output,
                                   p_block->p_buffer, p_block->i_buffer );
#else
    i_used = avcodec_decode_audio( p_sys->p_context,
                                   (int16_t*)p_sys->p_output, &i_output,
                                   p_block->p_buffer, p_block->i_buffer );
#endif

    if( i_used < 0 || i_output < 0 )
    {
        if( i_used < 0 )
            msg_Warn( p_dec, "cannot decode one frame (%zu bytes)",
                      p_block->i_buffer );

        block_Release( p_block );
        return NULL;
    }
    else if( (size_t)i_used > p_block->i_buffer )
    {
        i_used = p_block->i_buffer;
    }

    p_block->i_buffer -= i_used;
    p_block->p_buffer += i_used;

    if( p_sys->p_context->channels <= 0 || p_sys->p_context->channels > 6 ||
        p_sys->p_context->sample_rate <= 0 )
    {
        msg_Warn( p_dec, "invalid audio properties channels count %d, sample rate %d",
                  p_sys->p_context->channels, p_sys->p_context->sample_rate );
        block_Release( p_block );
        return NULL;
    }

    if( p_dec->fmt_out.audio.i_rate != (unsigned int)p_sys->p_context->sample_rate )
    {
        aout_DateInit( &p_sys->end_date, p_sys->p_context->sample_rate );
        aout_DateSet( &p_sys->end_date, p_block->i_pts );
    }

    /* **** Set audio output parameters **** */
    p_dec->fmt_out.audio.i_rate     = p_sys->p_context->sample_rate;
    p_dec->fmt_out.audio.i_channels = p_sys->p_context->channels;
    p_dec->fmt_out.audio.i_original_channels =
        p_dec->fmt_out.audio.i_physical_channels =
            pi_channels_maps[p_sys->p_context->channels];

    if( p_block->i_pts != 0 &&
        p_block->i_pts != aout_DateGet( &p_sys->end_date ) )
    {
        aout_DateSet( &p_sys->end_date, p_block->i_pts );
    }
    p_block->i_pts = 0;

    /* **** Now we can output these samples **** */
    p_sys->i_samples = i_output / sizeof(int16_t) / p_sys->p_context->channels;
    p_sys->p_samples = p_sys->p_output;

    /* Silent unwanted samples */
    if( p_sys->i_reject_count > 0 )
    {
        memset( p_sys->p_output, 0, i_output );
        p_sys->i_reject_count--;
    }

    p_buffer = SplitBuffer( p_dec );
    if( !p_buffer ) block_Release( p_block );
    return p_buffer;
}

/*****************************************************************************
 * EndAudioDec: audio decoder destruction
 *****************************************************************************/
void EndAudioDec( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    free( p_sys->p_output );
}
