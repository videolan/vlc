/*****************************************************************************
 * audio.c: audio decoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: audio.c,v 1.14 2003/02/07 01:22:55 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/aout.h>
#include <vlc/decoder.h>
#include <vlc/input.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>                                              /* getpid() */
#endif

#include <errno.h>
#include <string.h>

#ifdef HAVE_SYS_TIMES_H
#   include <sys/times.h>
#endif
#include "codecs.h"
#include "aout_internal.h"

#include "avcodec.h"                                            /* ffmpeg */

//#include "postprocessing/postprocessing.h"
#include "ffmpeg.h"
#include "audio.h"

/*
 * Local prototypes
 */
int      E_( InitThread_Audio )   ( adec_thread_t * );
void     E_( EndThread_Audio )    ( adec_thread_t * );
void     E_( DecodeThread_Audio ) ( adec_thread_t * );

static unsigned int pi_channels_maps[6] =
{
    0,
    AOUT_CHAN_CENTER,   AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
};

/*****************************************************************************
 * locales Functions
 *****************************************************************************/

/*****************************************************************************
 *
 * Functions that initialize, decode and end the decoding process
 *
 * Functions exported for ffmpeg.c
 *   * E_( InitThread_Audio )
 *   * E_( DecodeThread_Audio )
 *   * E_( EndThread_Video_Audio )
 *****************************************************************************/

/*****************************************************************************
 * InitThread: initialize vdec output thread
 *****************************************************************************
 * This function is called from decoder_Run and performs the second step
 * of the initialization. It returns 0 on success. Note that the thread's
 * flag are not modified inside this function.
 *
 * ffmpeg codec will be open, some memory allocated.
 *****************************************************************************/
int E_( InitThread_Audio )( adec_thread_t *p_adec )
{
    WAVEFORMATEX wf, *p_wf;

    if( ( p_wf = p_adec->p_fifo->p_waveformatex ) == NULL )
    {
        msg_Warn( p_adec->p_fifo, "audio informations missing" );
        p_wf = &wf;
        memset( p_wf, 0, sizeof( WAVEFORMATEX ) );
    }

    /* ***** Fill p_context with init values ***** */
    p_adec->p_context->sample_rate = p_wf->nSamplesPerSec;
    p_adec->p_context->channels = p_wf->nChannels;
#if LIBAVCODEC_BUILD >= 4618
    p_adec->p_context->block_align = p_wf->nBlockAlign;
#endif
    p_adec->p_context->bit_rate = p_wf->nAvgBytesPerSec * 8;

    if( ( p_adec->p_context->extradata_size = p_wf->cbSize ) > 0 )
    {
        p_adec->p_context->extradata =
            malloc( p_wf->cbSize );

        memcpy( p_adec->p_context->extradata,
                &p_wf[1],
                p_wf->cbSize);
    }

    /* ***** Open the codec ***** */
    if (avcodec_open(p_adec->p_context, p_adec->p_codec) < 0)
    {
        msg_Err( p_adec->p_fifo,
                 "cannot open codec (%s)",
                 p_adec->psz_namecodec );
        return( -1 );
    }
    else
    {
        msg_Dbg( p_adec->p_fifo,
                 "ffmpeg codec (%s) started",
                 p_adec->psz_namecodec );
    }

    p_adec->p_output = malloc( 3 * AVCODEC_MAX_AUDIO_FRAME_SIZE );


    p_adec->output_format.i_format = AOUT_FMT_S16_NE;
    p_adec->output_format.i_rate = p_wf->nSamplesPerSec;
    p_adec->output_format.i_physical_channels
        = p_adec->output_format.i_original_channels
        = pi_channels_maps[p_wf->nChannels];

    p_adec->p_aout = NULL;
    p_adec->p_aout_input = NULL;

    return( 0 );
}


/*****************************************************************************
 * DecodeThread: Called for decode one frame
 *****************************************************************************/
void  E_( DecodeThread_Audio )( adec_thread_t *p_adec )
{
    pes_packet_t    *p_pes;
    aout_buffer_t   *p_aout_buffer;

    int     i_samplesperchannel;
    int     i_output_size;
    int     i_frame_size;
    int     i_used;

    do
    {
        input_ExtractPES( p_adec->p_fifo, &p_pes );
        if( !p_pes )
        {
            p_adec->p_fifo->b_error = 1;
            return;
        }
        p_adec->pts = p_pes->i_pts;
        i_frame_size = p_pes->i_pes_size;

        if( i_frame_size > 0 )
        {
            uint8_t *p_last;
            int     i_need;


            i_need = i_frame_size + 16 + p_adec->i_buffer;
            if( p_adec->i_buffer_size < i_need )
            {
                p_last = p_adec->p_buffer;
                p_adec->p_buffer = malloc( i_need );
                p_adec->i_buffer_size = i_need;
                if( p_adec->i_buffer > 0 )
                {
                    memcpy( p_adec->p_buffer, p_last, p_adec->i_buffer );
                }
                FREE( p_last );
            }
            i_frame_size =
                E_( GetPESData )( p_adec->p_buffer + p_adec->i_buffer,
                                  i_frame_size,
                                  p_pes );
            /* make ffmpeg happier but I'm not sure it's needed for audio */
            memset( p_adec->p_buffer + p_adec->i_buffer + i_frame_size,
                    0,
                    16 );
        }
        input_DeletePES( p_adec->p_fifo->p_packets_mgt, p_pes );
    } while( i_frame_size <= 0 );


    i_frame_size += p_adec->i_buffer;

usenextdata:
    i_used = avcodec_decode_audio( p_adec->p_context,
                                   (int16_t*)p_adec->p_output,
                                   &i_output_size,
                                   p_adec->p_buffer,
                                   i_frame_size );
    if( i_used < 0 )
    {
        msg_Warn( p_adec->p_fifo,
                  "cannot decode one frame (%d bytes)",
                  i_frame_size );
        p_adec->i_buffer = 0;
        return;
    }
    else if( i_used < i_frame_size )
    {
        memmove( p_adec->p_buffer,
                 p_adec->p_buffer + i_used,
                 p_adec->i_buffer_size - i_used );

        p_adec->i_buffer = i_frame_size - i_used;
    }
    else
    {
        p_adec->i_buffer = 0;
    }

    i_frame_size -= i_used;

//    msg_Dbg( p_adec->p_fifo, "frame size:%d buffer used:%d", i_frame_size, i_used );
    if( i_output_size <= 0 )
    {
         msg_Warn( p_adec->p_fifo, 
                  "decoded %d samples bytes",
                  i_output_size );
    }

    if( p_adec->p_context->channels <= 0 ||
        p_adec->p_context->channels > 5 )
    {
        msg_Warn( p_adec->p_fifo,
                  "invalid channels count %d",
                  p_adec->p_context->channels );
    }

    /* **** Now we can output these samples **** */
    i_samplesperchannel = i_output_size / 2
                           / aout_FormatNbChannels( &p_adec->output_format );
    /* **** First check if we have a valid output **** */
    if( ( p_adec->p_aout_input == NULL )||
        ( p_adec->output_format.i_original_channels !=
                    pi_channels_maps[p_adec->p_context->channels] ) )
    {
        if( p_adec->p_aout_input != NULL )
        {
            /* **** Delete the old **** */
            aout_DecDelete( p_adec->p_aout, p_adec->p_aout_input );
        }

        /* **** Create a new audio output **** */
        p_adec->output_format.i_physical_channels =
            p_adec->output_format.i_original_channels =
                pi_channels_maps[p_adec->p_context->channels];

        aout_DateInit( &p_adec->date, p_adec->output_format.i_rate );
        p_adec->p_aout_input = aout_DecNew( p_adec->p_fifo,
                                            &p_adec->p_aout,
                                            &p_adec->output_format );
    }

    if( !p_adec->p_aout_input )
    {
        msg_Err( p_adec->p_fifo, "cannot create aout" );
        return;
    }

    if( p_adec->pts != 0 && p_adec->pts != aout_DateGet( &p_adec->date ) )
    {
        aout_DateSet( &p_adec->date, p_adec->pts );
    }
    else if( !aout_DateGet( &p_adec->date ) )
    {
        return;
    }

    p_aout_buffer = aout_DecNewBuffer( p_adec->p_aout,
                                       p_adec->p_aout_input,
                                       i_samplesperchannel );
    if( !p_aout_buffer )
    {
        msg_Err( p_adec->p_fifo, "cannot get aout buffer" );
        p_adec->p_fifo->b_error = 1;
        return;
    }

    p_aout_buffer->start_date = aout_DateGet( &p_adec->date );
    p_aout_buffer->end_date = aout_DateIncrement( &p_adec->date,
                                                  i_samplesperchannel );
    memcpy( p_aout_buffer->p_buffer,
            p_adec->p_output,
            p_aout_buffer->i_nb_bytes );

    aout_DecPlay( p_adec->p_aout, p_adec->p_aout_input, p_aout_buffer );

    if( i_frame_size > 0 )
    {
        goto usenextdata;
    }

    return;
}


/*****************************************************************************
 * EndThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization.
 *****************************************************************************/
void E_( EndThread_Audio )( adec_thread_t *p_adec )
{
//    FREE( p_adec->format.p_data );
    FREE( p_adec->p_output );

    if( p_adec->p_aout_input )
    {
        aout_DecDelete( p_adec->p_aout, p_adec->p_aout_input );
    }

}

