/*****************************************************************************
 * audio.c: audio decoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2003 VideoLAN
 * $Id: audio.c,v 1.20 2003/10/27 01:04:38 gbazin Exp $
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
#include <vlc/vout.h>
#include <vlc/aout.h>
#include <vlc/decoder.h>
#include <vlc/input.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>                                              /* getpid() */
#endif

#ifdef HAVE_SYS_TIMES_H
#   include <sys/times.h>
#endif

#include "codecs.h"
#include "aout_internal.h"

/* ffmpeg header */
#ifdef HAVE_FFMPEG_AVCODEC_H
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

#include "ffmpeg.h"
#include "audio.h"

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
    aout_instance_t       *p_aout;
    aout_input_t          *p_aout_input;
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
    WAVEFORMATEX wf, *p_wf;

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

    if( ( p_wf = p_dec->p_fifo->p_waveformatex ) == NULL )
    {
        msg_Warn( p_dec, "audio informations missing" );
        p_wf = &wf;
        memset( p_wf, 0, sizeof( WAVEFORMATEX ) );
    }

    /* ***** Fill p_context with init values ***** */
    p_sys->p_context->sample_rate = p_wf->nSamplesPerSec;
    p_sys->p_context->channels = p_wf->nChannels;
    p_sys->p_context->block_align = p_wf->nBlockAlign;
    p_sys->p_context->bit_rate = p_wf->nAvgBytesPerSec * 8;

    if( ( p_sys->p_context->extradata_size = p_wf->cbSize ) > 0 )
    {
        p_sys->p_context->extradata =
            malloc( p_wf->cbSize + FF_INPUT_BUFFER_PADDING_SIZE );
        memcpy( p_sys->p_context->extradata, &p_wf[1], p_wf->cbSize);
        memset( &((uint8_t*)p_sys->p_context->extradata)[p_wf->cbSize], 0,
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

    p_sys->p_aout = NULL;
    p_sys->p_aout_input = NULL;

    aout_DateSet( &p_sys->end_date, 0 );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DecodeAudio: Called to decode one frame
 *****************************************************************************/
int E_( DecodeAudio )( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    aout_buffer_t *p_aout_buffer;
    mtime_t i_pts;

    uint8_t *p_buffer, *p_samples;
    int i_buffer, i_samples;

    if( !aout_DateGet( &p_sys->end_date ) && !p_block->i_pts )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( p_block );
        return VLC_SUCCESS;
    }

    i_pts = p_block->i_pts;
    i_buffer = p_block->i_buffer;
    p_buffer = p_block->p_buffer;

    while( i_buffer )
    {
        int i_used, i_output;

        i_used = avcodec_decode_audio( p_sys->p_context,
                                       (int16_t*)p_sys->p_output, &i_output,
                                       p_buffer, i_buffer );
        if( i_used < 0 )
        {
            msg_Warn( p_dec, "cannot decode one frame (%d bytes)", i_buffer );
            break;
        }

        i_buffer -= i_used;
        p_buffer += i_used;

        if( p_sys->p_context->channels <= 0 || p_sys->p_context->channels > 6 )
        {
            msg_Warn( p_dec, "invalid channels count %d",
                      p_sys->p_context->channels );
            break;
        }

        /* **** First check if we have a valid output **** */
        if( p_sys->p_aout_input == NULL ||
            p_sys->aout_format.i_original_channels !=
            pi_channels_maps[p_sys->p_context->channels] )
        {
            if( p_sys->p_aout_input != NULL )
            {
                /* **** Delete the old **** */
                aout_DecDelete( p_sys->p_aout, p_sys->p_aout_input );
            }

            /* **** Create a new audio output **** */
            p_sys->aout_format.i_format = AOUT_FMT_S16_NE;
            p_sys->aout_format.i_rate = p_sys->p_context->sample_rate;
            p_sys->aout_format.i_physical_channels =
                p_sys->aout_format.i_original_channels =
                    pi_channels_maps[p_sys->p_context->channels];

            aout_DateInit( &p_sys->end_date, p_sys->aout_format.i_rate );
            p_sys->p_aout_input = aout_DecNew( p_dec, &p_sys->p_aout,
                                               &p_sys->aout_format );
        }

        if( !p_sys->p_aout_input )
        {
            msg_Err( p_dec, "cannot create audio output" );
            block_Release( p_block );
            return VLC_EGENERIC;
        }

        if( i_pts != 0 && i_pts != aout_DateGet( &p_sys->end_date ) )
        {
            aout_DateSet( &p_sys->end_date, i_pts );
            i_pts = 0;
        }

        /* **** Now we can output these samples **** */
        i_samples = i_output / 2 / p_sys->p_context->channels;

        p_samples = p_sys->p_output;
        while( i_samples > 0 )
        {
            int i_smaller_samples;

            i_smaller_samples = __MIN( 8000, i_samples );

            p_aout_buffer = aout_DecNewBuffer( p_sys->p_aout,
                                               p_sys->p_aout_input,
                                               i_smaller_samples );
            if( !p_aout_buffer )
            {
                msg_Err( p_dec, "cannot get aout buffer" );
                block_Release( p_block );
                return VLC_EGENERIC;
            }

            p_aout_buffer->start_date = aout_DateGet( &p_sys->end_date );
            p_aout_buffer->end_date = aout_DateIncrement( &p_sys->end_date,
                                                          i_smaller_samples );
            memcpy( p_aout_buffer->p_buffer, p_samples,
                    p_aout_buffer->i_nb_bytes );

            aout_DecPlay( p_sys->p_aout, p_sys->p_aout_input, p_aout_buffer );

            p_samples += i_smaller_samples * 2 * p_sys->p_context->channels;
            i_samples -= i_smaller_samples;
        }
    }

    block_Release( p_block );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * EndAudioDec: audio decoder destruction
 *****************************************************************************/
void E_(EndAudioDec)( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_output ) free( p_sys->p_output );

    if( p_sys->p_aout_input )
    {
        aout_DecDelete( p_sys->p_aout, p_sys->p_aout_input );
    }
}
