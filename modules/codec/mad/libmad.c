/***************************************************************************
             libmad.c  -  description
               -------------------
    Functions that are called by libmad to communicate with vlc decoder
    infrastructure.

    begin                : Mon Nov 5 2001
    copyright            : (C) 2001 by Jean-Paul Saman
    email                : jpsaman@wxs.nl
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/decoder.h>

/*****************************************************************************
 * Libmad includes files
 *****************************************************************************/
#include <mad.h>
#include "decoder.h"
#include "libmad.h"

static void PrintFrameInfo(struct mad_header *Header);

/*****************************************************************************
 * libmad_input: this function is called by libmad when the input buffer needs
 * to be filled.
 *****************************************************************************/
enum mad_flow libmad_input(void *data, struct mad_stream *p_libmad_stream)
{
    mad_adec_thread_t *p_mad_adec = (mad_adec_thread_t *) data;
    size_t 	       ReadSize, Remaining;
    unsigned char     *ReadStart;

    if ( p_mad_adec->p_fifo->b_die == 1 ) {
        msg_Dbg( p_mad_adec->p_fifo, "libmad_input stopping libmad decoder" );
        return MAD_FLOW_STOP;
    }

    if ( p_mad_adec->p_fifo->b_error == 1 ) {
        msg_Warn( p_mad_adec->p_fifo, "libmad_input ignoring current audio frame" );
        return MAD_FLOW_IGNORE;
    }

    /* libmad_stream_buffer does not consume the total buffer, it consumes only data
     * for one frame only. So all data left in the buffer should be put back in front.
     */
    if ((p_libmad_stream->buffer==NULL) || (p_libmad_stream->error==MAD_ERROR_BUFLEN))
    {
        /* libmad does not consume all the buffer it's given. Some
         * datas, part of a truncated frame, is left unused at the
         * end of the buffer. Those datas must be put back at the
         * beginning of the buffer and taken in account for
         * refilling the buffer. This means that the input buffer
         * must be large enough to hold a complete frame at the
         * highest observable bit-rate (currently 448 kb/s). XXX=XXX
         * Is 2016 bytes the size of the largest frame?
         * (448000*(1152/32000))/8
         */
        if(p_libmad_stream->next_frame!=NULL)
        {
            Remaining=p_libmad_stream->bufend-p_libmad_stream->next_frame;
            if( p_mad_adec->buffer != p_libmad_stream->next_frame )
            {
                memcpy( p_mad_adec->buffer,
                        p_libmad_stream->next_frame, Remaining );
            }
            ReadStart=p_mad_adec->buffer+Remaining;
            ReadSize=(MAD_BUFFER_MDLEN)-Remaining;

            /* Store time stamp of next frame */
            p_mad_adec->i_current_pts = p_mad_adec->i_next_pts;
            p_mad_adec->i_next_pts = p_mad_adec->p_fifo->p_first->i_pts;
        }
        else
        {
            ReadSize=(MAD_BUFFER_MDLEN);
            ReadStart=p_mad_adec->buffer;
            Remaining=0;
            p_mad_adec->i_next_pts = 0;
            p_mad_adec->i_current_pts = p_mad_adec->p_fifo->p_first->i_pts;
        }

        /* Fill-in the buffer. If an error occurs print a message
         * and leave the decoding loop. If the end of stream is
         * reached we also leave the loop but the return status is
         * left untouched.
         */
        if( ReadSize > p_mad_adec->p_data->p_payload_end
                        - p_mad_adec->p_data->p_payload_start )
        {
            ReadSize = p_mad_adec->p_data->p_payload_end
                        - p_mad_adec->p_data->p_payload_start;
            memcpy( ReadStart, p_mad_adec->p_data->p_payload_start, ReadSize );
            NextDataPacket( p_mad_adec->p_fifo, &p_mad_adec->p_data );
        }
        else
        {
            memcpy( ReadStart, p_mad_adec->p_data->p_payload_start, ReadSize );
            p_mad_adec->p_data->p_payload_start += ReadSize;
        }

        if ( p_mad_adec->p_fifo->b_die == 1 )
        {
            msg_Dbg( p_mad_adec->p_fifo, "libmad_input stopping libmad decoder" );
            return MAD_FLOW_STOP;
        }

        if ( p_mad_adec->p_fifo->b_error == 1 )
        {
            msg_Warn( p_mad_adec->p_fifo, "libmad_input ignoring current audio frame" );    
            return MAD_FLOW_IGNORE;
        }

        /* Pipe the new buffer content to libmad's stream decoder facility.
         * Libmad never copies the buffer, but just references it. So keep it in
         * mad_adec_thread_t structure.
         */
        mad_stream_buffer(p_libmad_stream,(unsigned char*) &p_mad_adec->buffer,
                          Remaining + ReadSize);
        p_libmad_stream->error=0;
    }

    return MAD_FLOW_CONTINUE;
}

/*****************************************************************************
 * libmad_header: this function is called just after the header of a frame is
 * decoded
 *****************************************************************************/
/*
 *enum mad_flow libmad_header(void *data, struct mad_header const *p_libmad_header)
 *{
 *   mad_adec_thread_t *p_mad_adec = (mad_adec_thread_t *) data;
 *
 *   msg_Err( p_mad_adec->p_fifo, "libmad_header samplerate %d", p_libmad_header->samplerate);
 *
 *   PrintFrameInfo(p_limad_mad_header)
 *   return MAD_FLOW_CONTINUE;
 *}
 */

/*****************************************************************************
 * lib_mad_filter: this function is called to filter data of a frame
 *****************************************************************************/
/* enum mad_flow libmad_filter(void *data, struct mad_stream const *p_libmad_stream, struct mad_frame *p_libmad_frame)
 * {
 *	return MAD_FLOW_CONTINUE;
 * }
 */

/*****************************************************************************
 * support routines borrowed from mpg321 (file: mad.c), which is distributed
 * under GPL license
 *
 * mpg321 was written by Joe Drew <drew@debian.org>, and based upon 'plaympeg'
 * from the smpeg sources, which was written by various people from Loki Software
 * (http://www.lokigames.com).
 *
 * It also incorporates some source from mad, written by Robert Leslie
 *****************************************************************************/

/* The following two routines and data structure are from the ever-brilliant
     Rob Leslie.
*/

struct audio_dither {
    mad_fixed_t error[3];
    mad_fixed_t random;
};

/*
* NAME:                prng()
* DESCRIPTION: 32-bit pseudo-random number generator
*/
static inline unsigned long prng(unsigned long state)
{
    return (state * 0x0019660dL + 0x3c6ef35fL) & 0xffffffffL;
}

/*
* NAME:        mpg321_s24_to_s16_pcm()
* DESCRIPTION: generic linear sample quantize and dither routine
*/
static inline signed int mpg321_s24_to_s16_pcm(unsigned int bits, mad_fixed_t sample,
                                    struct audio_dither *dither)
{
    unsigned int scalebits;
    mad_fixed_t output, mask, random;

    enum {
        MIN = -MAD_F_ONE,
        MAX = MAD_F_ONE - 1
    };

    /* noise shape */
    sample += dither->error[0] - dither->error[1] + dither->error[2];

    dither->error[2] = dither->error[1];
    dither->error[1] = dither->error[0] / 2;

    /* bias */
    output = sample + (1L << (MAD_F_FRACBITS + 1 - bits - 1));

    scalebits = MAD_F_FRACBITS + 1 - bits;
    mask = (1L << scalebits) - 1;

    /* dither */
    random    = prng(dither->random);
    output += (random & mask) - (dither->random & mask);

    dither->random = random;

    /* clip */
    if (output > MAX) {
        output = MAX;

        if (sample > MAX)
            sample = MAX;
    }
    else if (output < MIN) {
        output = MIN;

        if (sample < MIN)
            sample = MIN;
    }

    /* quantize */
    output &= ~mask;

    /* error feedback */
    dither->error[0] = sample - output;

    /* scale */
    return output >> scalebits;
}

/*****************************************************************************
 * s24_to_s16_pcm: Scale a 24 bit pcm sample to a 16 bit pcm sample.
 *****************************************************************************/
static inline mad_fixed_t s24_to_s16_pcm(mad_fixed_t sample)
{
  /* round */
  sample += (1L << (MAD_F_FRACBITS - 16));

  /* clip */
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  /* quantize */
  return sample >> (MAD_F_FRACBITS + 1 - 16);
}

/*****************************************************************************
 * libmad_ouput: this function is called just after the frame is decoded
 *****************************************************************************/
//enum mad_flow libmad_output(void *data, struct mad_header const *p_libmad_header, struct mad_pcm *p_libmad_pcm)
//{
//    mad_adec_thread_t *p_mad_adec= (mad_adec_thread_t *) data;
//    byte_t *buffer=NULL;
//    mad_fixed_t const *left_ch = p_libmad_pcm->samples[0], *right_ch = p_libmad_pcm->samples[1];
//    register int nsamples = p_libmad_pcm->length;
//    mad_fixed_t sample;
//    static struct audio_dither dither;
//
//    /* Creating the audio output fifo.
//     * Assume the samplerate and nr of channels from the first decoded frame is right for the entire audio track.
//     */
//    if (p_mad_adec->p_aout_fifo==NULL)
//    {
//    	p_mad_adec->p_aout_fifo = aout_CreateFifo(
//                p_mad_adec->p_fifo,
//                AOUT_FIFO_PCM,              /* fifo type */
//                2, /*p_libmad_pcm->channels,*/     /* nr. of channels */
//                p_libmad_pcm->samplerate,   /* frame rate in Hz ?*/
//                p_libmad_pcm->length*2,     /* length of output buffer *2 channels*/
//                NULL  );                    /* buffer */
//
//    	if ( p_mad_adec->p_aout_fifo == NULL )
//    	{
//        	return MAD_FLOW_BREAK;
//    	}
//
//        msg_Dbg( p_mad_adec->p_fifo, "aout fifo created");
//    }
//
//    if (p_mad_adec->p_aout_fifo->i_rate != p_libmad_pcm->samplerate)
//    {
//	msg_Warn( p_mad_adec->p_fifo, "samplerate is changing from [%d] Hz "
//                  "to [%d] Hz, sample size [%d], error_code [%0x]",
//                  p_mad_adec->p_aout_fifo->i_rate, p_libmad_pcm->samplerate,
//                  p_libmad_pcm->length,
//                  p_mad_adec->libmad_decoder->sync->stream.error );
//	p_mad_adec->p_aout_fifo->i_rate = p_libmad_pcm->samplerate;
//    }
//
//    if( p_mad_adec->i_current_pts )
//    {
//        p_mad_adec->p_aout_fifo->date[p_mad_adec->p_aout_fifo->i_end_frame]
//                = p_mad_adec->i_current_pts;
//    }
//    else
//    {
//        p_mad_adec->p_aout_fifo->date[p_mad_adec->p_aout_fifo->i_end_frame]
//                = LAST_MDATE;
//    }
////    mad_timer_add(&p_mad_adec->libmad_timer,p_libmad_header->duration);
//
//    buffer = ((byte_t *)p_mad_adec->p_aout_fifo->buffer) + (p_mad_adec->p_aout_fifo->i_end_frame * (p_libmad_pcm->length*4));
//
//    while (nsamples--)
//    {
//        switch (p_mad_adec->audio_scaling)
//        {
//          case MPG321_SCALING:
//               sample = mpg321_s24_to_s16_pcm(16, *left_ch++, &dither);
//          break;
//          case FAST_SCALING: /* intended fall through */
//          default:
//               sample = s24_to_s16_pcm(*left_ch++);
//          break;
//			  }
//
//        /* left audio channel */
//#ifndef WORDS_BIGENDIAN
//        *buffer++ = (byte_t) (sample >> 0);
//        *buffer++ = (byte_t) (sample >> 8);
//#else
//     	  *buffer++ = (byte_t) (sample >> 8);
//    	  *buffer++ = (byte_t) (sample >> 0);
//#endif
//      	if (p_libmad_pcm->channels == 2)
//        {
//       	    /* right audio channel */
//            switch (p_mad_adec->audio_scaling)
//            {
//              case MPG321_SCALING:
//                   sample = mpg321_s24_to_s16_pcm(16, *right_ch++, &dither);
//              break;
//              case FAST_SCALING: /* intended fall through */
//              default:
//                   sample = s24_to_s16_pcm(*right_ch++);
//              break;
//    			  }
//        }
//        /* else reuse left_ch */
//#ifndef WORDS_BIGENDIAN
//        *buffer++ = (byte_t) (sample >> 0);
//        *buffer++ = (byte_t) (sample >> 8);
//#else
//        *buffer++ = (byte_t) (sample >> 8);
//        *buffer++ = (byte_t) (sample >> 0);
//#endif						
//    }
//
//    /* DEBUG */
//    /*
//    if (p_libmad_pcm->channels == 1) {
//       msg_Dbg( p_mad_adec->p_fifo, "libmad_output channels [%d]", p_libmad_pcm->channels);
//    }
//    */
//
//    vlc_mutex_lock (&p_mad_adec->p_aout_fifo->data_lock);
//    p_mad_adec->p_aout_fifo->i_end_frame = (p_mad_adec->p_aout_fifo->i_end_frame + 1) & AOUT_FIFO_SIZE;
//    vlc_cond_signal (&p_mad_adec->p_aout_fifo->data_wait);
//    vlc_mutex_unlock (&p_mad_adec->p_aout_fifo->data_lock);
//
//    return MAD_FLOW_CONTINUE;
//}

/*****************************************************************************
 * libmad_ouput3: this function is called just after the frame is decoded
 *****************************************************************************/
enum mad_flow libmad_output3(void *data, struct mad_header const *p_libmad_header, struct mad_pcm *p_libmad_pcm)
{
    mad_adec_thread_t *p_mad_adec= (mad_adec_thread_t *) data;
    aout_buffer_t * p_buffer;
    mad_fixed_t const *left_ch = p_libmad_pcm->samples[0], *right_ch = p_libmad_pcm->samples[1];
    register int nsamples = p_libmad_pcm->length;
    mad_fixed_t sample;

    /* Creating the audio output fifo.
     * Assume the samplerate and nr of channels from the first decoded frame
     * is right for the entire audio track.
     */
    if( (p_mad_adec->p_aout_input != NULL) &&
        ( (p_mad_adec->output_format.i_rate != p_libmad_pcm->samplerate)
    {
        /* Parameters changed - this should not happen. */
        aout_InputDelete( p_mad_adec->p_aout, p_mad_adec->p_aout_input );
        p_mad_adec->p_aout_input = NULL;
    }

    /* Creating the audio input if not created yet. */
    if( p_mad_adec->p_aout_input == NULL )
    {
        p_mad_adec->output_format.i_rate = p_libmad_pcm->samplerate;
        /* p_mad_adec->output_format.i_channels = p_libmad_pcm->channels; */
        p_mad_adec->p_aout_input = aout_InputNew( p_mad_adec->p_fifo,
                                               &p_mad_adec->p_aout,
                                               &p_mad_adec->output_format );

        if ( p_mad_adec->p_aout_input == NULL )
        {
           p_mad_adec->p_fifo->b_error = 1;
           return MAD_FLOW_BREAK;
        }
        msg_Dbg( p_mad_adec->p_fifo, "aout3 buffer created");
    }

    if (p_mad_adec->output_format->i_rate != p_libmad_pcm->samplerate)
    {
        msg_Warn( p_mad_adec->p_fifo, "samplerate is changing from [%d] Hz "
                  "to [%d] Hz, sample size [%d], error_code [%0x]",
                  p_mad_adec->p_aout_fifo->i_rate, p_libmad_pcm->samplerate,
                  p_libmad_pcm->length,
                  p_mad_adec->libmad_decoder->sync->stream.error );
        p_mad_adec->output_format.i_rate = p_libmad_pcm->samplerate;
    }

    /* Set the Presentation Time Stamp */
    p_buffer = aout_BufferNew( p_mad_adec->p_aout, p_mad_adec->p_aout_input,
                               (p_libmad_pcm->length*4) );
    if ( p_buffer == NULL )
    {
        return MAD_FLOW_BREAK;
    }
    /* Add accurate PTS to buffer. */
    if ( p_mad_adec->i_current_pts )
    {
        p_buffer->start_date = p_mad_adec->i_current_pts;
    }
    else
    {
        p_buffer->start_date = LAST_MDATE;
    }
    p_mad_adec->last_date += (mtime_t)(p_libmad_pcm->length*4)
                             / p_mad_adec->output_format.i_rate;
    p_buffer->end_date = p_mad_adec->last_date;

    /* Interleave and keep buffers in mad_fixed_t format */
    while (nsamples--)
    {
        /* left audio channel */
        sample = *left_ch++;
#ifndef WORDS_BIGENDIAN
        p_buffer->p_buffer++ = (mad_fixed_t) (sample >> 0);
        p_buffer->p_buffer++ = (mad_fixed_t) (sample >> 8);
#else
        p_buffer->p_buffer++ = (mad_fixed_t) (sample >> 8);
        p_buffer->p_buffer++ = (mad_fixed_t) (sample >> 0);
#endif
        /* right audio channel */
        if (p_libmad_pcm->channels == 2)
        {
           sample = *right_ch++;
        } /* else reuse left audio channel */
#ifndef WORDS_BIGENDIAN
        p_buffer->p_buffer++ = (mad_fixed_t) (sample >> 0);
        p_buffer->p_buffer++ = (mad_fixed_t) (sample >> 8);
#else
        p_buffer->p_buffer++ = (mad_fixed_t) (sample >> 8);
        p_buffer->p_buffer++ = (mad_fixed_t) (sample >> 0);
#endif
    }

    aout_BufferPlay( p_mad_adec->p_aout, p_mad_adec->p_aout_input, p_buffer );

    return MAD_FLOW_CONTINUE;
}

/*****************************************************************************
 * libmad_error: this function is called when an error occurs during decoding
 *****************************************************************************/
enum mad_flow libmad_error(void *data, struct mad_stream *p_libmad_stream, struct mad_frame *p_libmad_frame)
{
    enum mad_flow result = MAD_FLOW_CONTINUE;

    switch (p_libmad_stream->error)
    {             
    case MAD_ERROR_BUFLEN:                /* input buffer too small (or EOF) */
//X        msg_Err("libmad error: input buffer too small (or EOF)");
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BUFPTR:                /* invalid (null) buffer pointer */
//X        msg_Err("libmad error: invalid (null) buffer pointer");
        result = MAD_FLOW_STOP;
        break;
    case MAD_ERROR_NOMEM:                 /* not enough memory */
//X        msg_Err("libmad error: invalid (null) buffer pointer");
        result = MAD_FLOW_STOP;
        break;
    case MAD_ERROR_LOSTSYNC:            /* lost synchronization */
//X        msg_Err("libmad error: lost synchronization");
        mad_stream_sync(p_libmad_stream);
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADLAYER:            /* reserved header layer value */
//X        msg_Err("libmad error: reserved header layer value");
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADBITRATE:        /* forbidden bitrate value */
//X        msg_Err("libmad error: forbidden bitrate value");
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADSAMPLERATE: /* reserved sample frequency value */
//X        msg_Err("libmad error: reserved sample frequency value");
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADEMPHASIS:     /* reserved emphasis value */
//X        msg_Err("libmad error: reserverd emphasis value");
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADCRC:                /* CRC check failed */
//X        msg_Err("libmad error: CRC check failed");
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADBITALLOC:     /* forbidden bit allocation value */
//X        msg_Err("libmad error: forbidden bit allocation value");
        result = MAD_FLOW_IGNORE;
        break;
    case MAD_ERROR_BADSCALEFACTOR:/* bad scalefactor index */
//X        msg_Err("libmad error: bad scalefactor index");
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADFRAMELEN:     /* bad frame length */
//X        msg_Err("libmad error: bad frame length");
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADBIGVALUES:    /* bad big_values count */
//X        msg_Err("libmad error: bad big values count");
        result = MAD_FLOW_IGNORE;
        break;
    case MAD_ERROR_BADBLOCKTYPE:    /* reserved block_type */
//X        msg_Err("libmad error: reserverd block_type");
        result = MAD_FLOW_IGNORE;
        break;
    case MAD_ERROR_BADSCFSI:            /* bad scalefactor selection info */
//X        msg_Err("libmad error: bad scalefactor selection info");
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADDATAPTR:        /* bad main_data_begin pointer */
//X        msg_Err("libmad error: bad main_data_begin pointer");
        result = MAD_FLOW_STOP;
        break;
    case MAD_ERROR_BADPART3LEN:     /* bad audio data length */
//X        msg_Err("libmad error: bad audio data length");
        result = MAD_FLOW_IGNORE;
        break;
    case MAD_ERROR_BADHUFFTABLE:    /* bad Huffman table select */
//X        msg_Err("libmad error: bad Huffman table select");
        result = MAD_FLOW_IGNORE;
        break;
    case MAD_ERROR_BADHUFFDATA:     /* Huffman data overrun */
//X        msg_Err("libmad error: Huffman data overrun");
        result = MAD_FLOW_IGNORE;
        break;
    case MAD_ERROR_BADSTEREO:         /* incompatible block_type for JS */
//X        msg_Err("libmad error: incompatible block_type for JS");
        result = MAD_FLOW_IGNORE;
        break;
    default:
//X        msg_Err("libmad error: unknown error occured stopping decoder");
        result = MAD_FLOW_STOP;
        break;
    }
    
    return (MAD_RECOVERABLE(p_libmad_stream->error)? result: MAD_FLOW_STOP);
    //return (MAD_FLOW_CONTINUE);
}

/*****************************************************************************
 * libmad_message: this function is called to send a message
 *****************************************************************************/
/* enum mad_flow libmad_message(void *, void*, unsigned int*)
 * {
 *     return MAD_FLOW_CONTINUE;
 * }
 */



/****************************************************************************
 * Print human readable informations about an audio MPEG frame.				*
 ****************************************************************************/
static void PrintFrameInfo(struct mad_header *Header)
{
	const char	*Layer,
			*Mode,
			*Emphasis;

	/* Convert the layer number to it's printed representation. */
	switch(Header->layer)
	{
		case MAD_LAYER_I:
			Layer="I";
			break;
		case MAD_LAYER_II:
			Layer="II";
			break;
		case MAD_LAYER_III:
			Layer="III";
			break;
		default:
			Layer="(unexpected layer value)";
			break;
	}

	/* Convert the audio mode to it's printed representation. */
	switch(Header->mode)
	{
		case MAD_MODE_SINGLE_CHANNEL:
			Mode="single channel";
			break;
		case MAD_MODE_DUAL_CHANNEL:
			Mode="dual channel";
			break;
		case MAD_MODE_JOINT_STEREO:
			Mode="joint (MS/intensity) stereo";
			break;
		case MAD_MODE_STEREO:
			Mode="normal LR stereo";
			break;
		default:
			Mode="(unexpected mode value)";
			break;
	}

	/* Convert the emphasis to it's printed representation. */
	switch(Header->emphasis)
	{
		case MAD_EMPHASIS_NONE:
			Emphasis="no";
			break;
		case MAD_EMPHASIS_50_15_US:
			Emphasis="50/15 us";
			break;
		case MAD_EMPHASIS_CCITT_J_17:
			Emphasis="CCITT J.17";
			break;
		default:
			Emphasis="(unexpected emphasis value)";
			break;
	}

//X	msg_Err("statistics: %lu kb/s audio mpeg layer %s stream %s crc, "
//X			"%s with %s emphasis at %d Hz sample rate\n",
//X			Header->bitrate,Layer,
//X			Header->flags&MAD_FLAG_PROTECTION?"with":"without",
//X			Mode,Emphasis,Header->samplerate);
}
