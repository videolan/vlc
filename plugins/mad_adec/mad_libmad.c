/***************************************************************************
           mad_libmad.c  -  description
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

#include "defs.h"

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */

#include "config.h"
#include "common.h"                                     /* boolean_t, byte_t */
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"
#include "intf_msg.h"

#include "audio_output.h"

#include "modules.h"
#include "modules_export.h"

#include "stream_control.h"
#include "input_ext-dec.h"

#include "debug.h"

/*****************************************************************************
 * Libmad includes files
 *****************************************************************************/

#include <mad.h>
#include "mad_adec.h"
#include "mad_libmad.h"

/*****************************************************************************
 * libmad_input: this function is called by libmad when the input buffer needs
 * to be filled.
 *****************************************************************************/
enum mad_flow libmad_input(void *data, struct mad_stream *p_libmad_stream)
{
    mad_adec_thread_t *p_mad_adec = (mad_adec_thread_t *) data;
    byte_t buffer[ADEC_FRAME_SIZE];

    /* Store time stamp of current frame */
    if ( DECODER_FIFO_START(*p_mad_adec->p_fifo)->i_pts ) {
         p_mad_adec->i_pts_save = DECODER_FIFO_START(*p_mad_adec->p_fifo)->i_pts;
         DECODER_FIFO_START(*p_mad_adec->p_fifo)->i_pts = 0;
    }
    else {
         p_mad_adec->i_pts_save = LAST_MDATE;
    }

    GetChunk( &p_mad_adec->bit_stream, buffer, ADEC_FRAME_SIZE );

    if ( p_mad_adec->p_fifo->b_die == 1 ) {
        intf_ErrMsg( "mad_adec error: libmad_input stopping libmad decoder" );
        return MAD_FLOW_STOP;
    }

    if ( p_mad_adec->p_fifo->b_error == 1 ) {
        intf_ErrMsg( "mad_adec error: libmad_input ignoring current audio frame" );	
        return MAD_FLOW_IGNORE;
    }

    /* the length meant to be in bytes */
    mad_stream_buffer(p_libmad_stream, (unsigned char*) &buffer, ADEC_FRAME_SIZE );

    return MAD_FLOW_CONTINUE;
}

/*****************************************************************************
 * libmad_header: this function is called just after the header of a frame is
 * decoded
 *****************************************************************************/
enum mad_flow libmad_header(void *data, struct mad_header const *p_libmad_header)
{
    mad_adec_thread_t *p_mad_adec = (mad_adec_thread_t *) data;

    vlc_mutex_lock (&p_mad_adec->p_aout_fifo->data_lock);
/*
    intf_ErrMsg( "mad_adec: libmad_header samplerate %d", p_libmad_header->samplerate);
	intf_DbgMsg( "mad_adec: libmad_header bitrate %d", p_libmad_header->bitrate);	
*/
    p_mad_adec->p_aout_fifo->l_rate = p_libmad_header->samplerate;
    vlc_cond_signal (&p_mad_adec->p_aout_fifo->data_wait);
    vlc_mutex_unlock (&p_mad_adec->p_aout_fifo->data_lock);

    return MAD_FLOW_CONTINUE;
}

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
static __inline__ unsigned long prng(unsigned long state)
{
    return (state * 0x0019660dL + 0x3c6ef35fL) & 0xffffffffL;
}

/*
* NAME:                audio_linear_dither()
* DESCRIPTION: generic linear sample quantize and dither routine
*/
static __inline__ signed long audio_linear_dither(unsigned int bits, mad_fixed_t sample,
                                    struct audio_dither *dither)
{
    unsigned int scalebits;
    mad_fixed_t output, mask, random;

    enum {
        MIN = -MAD_F_ONE,
        MAX =    MAD_F_ONE - 1
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
 * libmad_ouput: this function is called just after the frame is decoded
 *****************************************************************************/
enum mad_flow libmad_output(void *data, struct mad_header const *p_libmad_header, struct mad_pcm *p_libmad_pcm)
{
    mad_adec_thread_t *p_mad_adec= (mad_adec_thread_t *) data;
    byte_t *buffer=NULL;

    mad_fixed_t const *left_ch = p_libmad_pcm->samples[0], *right_ch = p_libmad_pcm->samples[1];
    /*
     * 1152 because that's what mad has as a max; *4 because
     * there are 4 distinct bytes per sample (in 2 channel case)
     */
    static unsigned char stream[ADEC_FRAME_SIZE];
    register int nsamples = p_libmad_pcm->length;
    static struct audio_dither dither;

    register char * ptr = stream;
    register signed int sample;

    /* Set timestamp to synchronize audio and video decoder fifo's */
    vlc_mutex_lock (&p_mad_adec->p_aout_fifo->data_lock);
    p_mad_adec->p_aout_fifo->date[p_mad_adec->p_aout_fifo->l_end_frame] = p_mad_adec->i_pts_save;

    buffer = ((byte_t *)p_mad_adec->p_aout_fifo->buffer) + (p_mad_adec->p_aout_fifo->l_end_frame * ADEC_FRAME_SIZE);

    if (p_libmad_pcm->channels == 2)
    {
        while (nsamples--)
        {
            sample = (signed int) audio_linear_dither(16, *left_ch++, &dither);
#ifndef WORDS_BIGENDIAN
            *ptr++ = (unsigned char) (sample >> 0);
            *ptr++ = (unsigned char) (sample >> 8);
#else
            *ptr++ = (unsigned char) (sample >> 8);
            *ptr++ = (unsigned char) (sample >> 0);
#endif

            sample = (signed int) audio_linear_dither(16, *right_ch++, &dither);
#ifndef WORDS_BIGENDIAN
            *ptr++ = (unsigned char) (sample >> 0);
            *ptr++ = (unsigned char) (sample >> 8);
#else
            *ptr++ = (unsigned char) (sample >> 8);
            *ptr++ = (unsigned char) (sample >> 0);
#endif						
        }
        buffer = memcpy(buffer,stream,p_libmad_pcm->length*4);
        vlc_cond_signal (&p_mad_adec->p_aout_fifo->data_wait);
  }
  else
  {
        while (nsamples--)
        {
            sample = (signed int) audio_linear_dither(16, *left_ch++, &dither);

#ifndef WORDS_BIGENDIAN
            *ptr++ = (unsigned char) (sample >> 0);
            *ptr++ = (unsigned char) (sample >> 8);
#else
            *ptr++ = (unsigned char) (sample >> 8);
            *ptr++ = (unsigned char) (sample >> 0);
#endif					
        }
        buffer = memcpy(buffer,stream,p_libmad_pcm->length*2);
        vlc_cond_signal (&p_mad_adec->p_aout_fifo->data_wait);
    }
    vlc_mutex_unlock (&p_mad_adec->p_aout_fifo->data_lock);

    vlc_mutex_lock (&p_mad_adec->p_aout_fifo->data_lock);
    p_mad_adec->p_aout_fifo->l_end_frame = (p_mad_adec->p_aout_fifo->l_end_frame + 1) & AOUT_FIFO_SIZE;
    vlc_cond_signal (&p_mad_adec->p_aout_fifo->data_wait);
    vlc_mutex_unlock (&p_mad_adec->p_aout_fifo->data_lock);

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
        intf_ErrMsg("libmad error: input buffer too small (or EOF)");
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BUFPTR:                /* invalid (null) buffer pointer */
        intf_ErrMsg("libmad error: invalid (null) buffer pointer");
        result = MAD_FLOW_STOP;
        break;
    case MAD_ERROR_NOMEM:                 /* not enough memory */
        intf_ErrMsg("libmad error: invalid (null) buffer pointer");
        result = MAD_FLOW_STOP;
        break;
    case MAD_ERROR_LOSTSYNC:            /* lost synchronization */
        intf_ErrMsg("libmad error: lost synchronization");
        mad_stream_sync(p_libmad_stream);
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADLAYER:            /* reserved header layer value */
        intf_ErrMsg("libmad error: reserved header layer value");
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADBITRATE:        /* forbidden bitrate value */
        intf_ErrMsg("libmad error: forbidden bitrate value");
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADSAMPLERATE: /* reserved sample frequency value */
            intf_ErrMsg("libmad error: reserved sample frequency value");
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADEMPHASIS:     /* reserved emphasis value */
        intf_ErrMsg("libmad error: reserverd emphasis value");
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADCRC:                /* CRC check failed */
        intf_ErrMsg("libmad error: CRC check failed");
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADBITALLOC:     /* forbidden bit allocation value */
        intf_ErrMsg("libmad error: forbidden bit allocation value");
        result = MAD_FLOW_IGNORE;
        break;
    case MAD_ERROR_BADSCALEFACTOR:/* bad scalefactor index */
        intf_ErrMsg("libmad error: bad scalefactor index");
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADFRAMELEN:     /* bad frame length */
        intf_ErrMsg("libmad error: bad frame length");
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADBIGVALUES:    /* bad big_values count */
        intf_ErrMsg("libmad error: bad big values count");
        result = MAD_FLOW_IGNORE;
        break;
    case MAD_ERROR_BADBLOCKTYPE:    /* reserved block_type */
        intf_ErrMsg("libmad error: reserverd block_type");
        result = MAD_FLOW_IGNORE;
        break;
    case MAD_ERROR_BADSCFSI:            /* bad scalefactor selection info */
        intf_ErrMsg("libmad error: bad scalefactor selection info");
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADDATAPTR:        /* bad main_data_begin pointer */
        intf_ErrMsg("libmad error: bad main_data_begin pointer");
        result = MAD_FLOW_STOP;
        break;
    case MAD_ERROR_BADPART3LEN:     /* bad audio data length */
        intf_ErrMsg("libmad error: bad audio data length");
        result = MAD_FLOW_IGNORE;
        break;
    case MAD_ERROR_BADHUFFTABLE:    /* bad Huffman table select */
        intf_ErrMsg("libmad error: bad Huffman table select");
        result = MAD_FLOW_IGNORE;
        break;
    case MAD_ERROR_BADHUFFDATA:     /* Huffman data overrun */
        intf_ErrMsg("libmad error: Huffman data overrun");
        result = MAD_FLOW_IGNORE;
        break;
    case MAD_ERROR_BADSTEREO:         /* incompatible block_type for JS */
        intf_ErrMsg("libmad error: incompatible block_type for JS");
        result = MAD_FLOW_IGNORE;
        break;
    default:
        intf_ErrMsg("libmad error: unknown error occured stopping decoder");
        result = MAD_FLOW_STOP;
        break;
    }
    
    //return (MAD_RECOVERABLE(p_libmad_stream->error)? result: MAD_FLOW_STOP);
    return (MAD_FLOW_CONTINUE);
}

/*****************************************************************************
 * libmad_message: this function is called to send a message
 *****************************************************************************/
/* enum mad_flow libmad_message(void *, void*, unsigned int*)
 * {
 *     return MAD_FLOW_CONTINUE;
 * }
 */


