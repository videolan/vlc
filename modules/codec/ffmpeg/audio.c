/*****************************************************************************
 * audio.c: audio decoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2003 VideoLAN
 * $Id: audio.c,v 1.23 2003/11/17 02:52:39 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include <vlc/input.h>

#include "codecs.h"
#include "aout_internal.h"

/* ffmpeg header */
#ifdef HAVE_FFMPEG_AVCODEC_H
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

#include "ffmpeg.h"

static unsigned int pi_channels_maps[6] =
{
    0,
    AOUT_CHAN_CENTER,   AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
};

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /* Common part between video and audio decoder */
    int i_cat;
    int i_codec_id;
    char *psz_namecodec;
    AVCodecContext      *p_context;
    AVCodec             *p_codec;

    /* Temporary buffer for libavcodec */
    uint8_t *p_output;

    /*
     * Output properties
     */
    audio_sample_format_t aout_format;
    audio_date_t          end_date;
};

/*****************************************************************************
 * InitAudioDec: initialize audio decoder
 *****************************************************************************
 * The ffmpeg codec will be opened, some memory allocated.
 *****************************************************************************/
int E_(InitAudioDec)( decoder_t *p_dec, AVCodecContext *p_context,
                      AVCodec *p_codec, int i_codec_id, char *psz_namecodec )
{
    decoder_sys_t *p_sys;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
    {
        msg_Err( p_dec, "out of memory" );
        return VLC_EGENERIC;
    }

    p_dec->p_sys->p_context = p_context;
    p_dec->p_sys->p_codec = p_codec;
    p_dec->p_sys->i_codec_id = i_codec_id;
    p_dec->p_sys->psz_namecodec = psz_namecodec;

    /* ***** Fill p_context with init values ***** */
    p_sys->p_context->sample_rate = p_dec->fmt_in.audio.i_rate;
    p_sys->p_context->channels = p_dec->fmt_in.audio.i_channels;
    p_sys->p_context->block_align = p_dec->fmt_in.audio.i_blockalign;
    p_sys->p_context->bit_rate = p_dec->fmt_in.i_bitrate;

    if( ( p_sys->p_context->extradata_size = p_dec->fmt_in.i_extra ) > 0 )
    {
        p_sys->p_context->extradata =
            malloc( p_dec->fmt_in.i_extra + FF_INPUT_BUFFER_PADDING_SIZE );
        memcpy( p_sys->p_context->extradata,
                p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra );
        memset( p_sys->p_context->extradata + p_dec->fmt_in.i_extra, 0,
                FF_INPUT_BUFFER_PADDING_SIZE );
    }

    /* ***** Open the codec ***** */
    if (avcodec_open( p_sys->p_context, p_sys->p_codec ) < 0)
    {
        msg_Err( p_dec, "cannot open codec (%s)", p_sys->psz_namecodec );
        return VLC_EGENERIC;
    }
    else
    {
        msg_Dbg( p_dec, "ffmpeg codec (%s) started", p_sys->psz_namecodec );
    }

    p_sys->p_output = malloc( 3 * AVCODEC_MAX_AUDIO_FRAME_SIZE );

    aout_DateSet( &p_sys->end_date, 0 );

    /* Set output properties */
    p_dec->fmt_out.i_cat = AUDIO_ES;
    p_dec->fmt_out.i_codec = AOUT_FMT_S16_NE;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DecodeAudio: Called to decode one frame
 *****************************************************************************/
aout_buffer_t *E_( DecodeAudio )( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_used, i_output, i_samples;
    uint8_t *p_samples;
    aout_buffer_t *p_buffer;
    block_t *p_block;

    if( !pp_block || !*pp_block ) return NULL;

    p_block = *pp_block;

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

    i_used = avcodec_decode_audio( p_sys->p_context,
                                   (int16_t*)p_sys->p_output, &i_output,
                                   p_block->p_buffer, p_block->i_buffer );

    if( i_used < 0 )//|| i_output == 0 )
    {
        if( i_used < 0 )
            msg_Warn( p_dec, "cannot decode one frame (%d bytes)",
                      p_block->i_buffer );

        block_Release( p_block );
        return NULL;
    }
    else if( i_used > p_block->i_buffer )
    {
        i_used = p_block->i_buffer;
    }

    p_block->i_buffer -= i_used;
    p_block->p_buffer += i_used;

    if( p_sys->p_context->channels <= 0 || p_sys->p_context->channels > 6 )
    {
        msg_Warn( p_dec, "invalid channels count %d",
                  p_sys->p_context->channels );
        block_Release( p_block );
        return NULL;
    }

    if( p_dec->fmt_out.audio.i_rate != p_sys->p_context->sample_rate )
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
        p_block->i_pts = 0;
    }

    /* **** Now we can output these samples **** */
    i_samples = i_output / 2 / p_sys->p_context->channels;
    p_samples = p_sys->p_output;

    p_buffer = p_dec->pf_aout_buffer_new( p_dec, i_samples );
    if( !p_buffer )
    {
        msg_Err( p_dec, "cannot get aout buffer" );
        block_Release( p_block );
        return NULL;
    }

    p_buffer->start_date = aout_DateGet( &p_sys->end_date );
    p_buffer->end_date = aout_DateIncrement( &p_sys->end_date, i_samples );

    memcpy( p_buffer->p_buffer, p_samples, p_buffer->i_nb_bytes );

    return p_buffer;
}

/*****************************************************************************
 * EndAudioDec: audio decoder destruction
 *****************************************************************************/
void E_(EndAudioDec)( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_output ) free( p_sys->p_output );
}
