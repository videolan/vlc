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
enum mad_flow libmad_input( void *p_data, struct mad_stream *p_stream )
{
    mad_adec_thread_t * p_dec = (mad_adec_thread_t *) p_data;
    size_t   i_wanted, i_left;

    if ( p_dec->p_fifo->b_die )
    {
        msg_Dbg( p_dec->p_fifo, "stopping libmad decoder" );
        return MAD_FLOW_STOP;
    }

    if ( p_dec->p_fifo->b_error )
    {
        msg_Warn( p_dec->p_fifo, "ignoring current audio frame" );
        return MAD_FLOW_IGNORE;
    }

    /* libmad_stream_buffer does not consume the total buffer, it consumes
     * only data for one frame only. So all data left in the buffer should
     * be put back in front. */
    if ( !p_stream->buffer || p_stream->error == MAD_ERROR_BUFLEN )
    {
       /* libmad does not consume all the buffer it's given. Some data,
        * part of a truncated frame, is left unused at the end of the
        * buffer. Those datas must be put back at the beginning of the
        * buffer and taken in account for refilling the buffer. This
        * means that the input buffer must be large enough to hold a
        * complete frame at the highest observable bit-rate (currently
        * 448 kb/s). XXX=XXX Is 2016 bytes the size of the largest frame?
        * (448000*(1152/32000))/8 */
        if( p_stream->next_frame )
        {
            i_left = p_stream->bufend - p_stream->next_frame;
            if( p_dec->buffer != p_stream->next_frame )
            {
                memcpy( p_dec->buffer, p_stream->next_frame, i_left );
            }
            i_wanted = MAD_BUFFER_MDLEN - i_left;

            /* Store timestamp for next frame */
            p_dec->i_next_pts = p_dec->bit_stream.p_pes->i_pts;
        }
        else
        {
            i_wanted = MAD_BUFFER_MDLEN;
            i_left = 0;

            /* Store timestamp for this frame */
            p_dec->i_current_pts = p_dec->bit_stream.p_pes->i_pts;
        }

        /* Fill-in the buffer. If an error occurs print a message and leave
         * the decoding loop. If the end of stream is reached we also leave
         * the loop but the return status is left untouched. */
        if( i_wanted > (size_t)(p_dec->bit_stream.p_data->p_payload_end
                                 - p_dec->bit_stream.p_data->p_payload_start) )
        {
            i_wanted = p_dec->bit_stream.p_data->p_payload_end
                        - p_dec->bit_stream.p_data->p_payload_start;
            memcpy( p_dec->buffer + i_left,
                    p_dec->bit_stream.p_data->p_payload_start, i_wanted );
            NextDataPacket( p_dec->p_fifo, &p_dec->bit_stream );
            /* No need to check that p_dec->bit_stream->p_data is valid
             * since we check later on for b_die and b_error */
        }
        else
        {
            memcpy( p_dec->buffer + i_left,
                    p_dec->bit_stream.p_data->p_payload_start, i_wanted );
            p_dec->bit_stream.p_data->p_payload_start += i_wanted;
        }

        if ( p_dec->p_fifo->b_die )
        {
            msg_Dbg( p_dec->p_fifo, "stopping libmad decoder" );
            return MAD_FLOW_STOP;
        }

        if ( p_dec->p_fifo->b_error )
        {
            msg_Warn( p_dec->p_fifo, "ignoring current audio frame" );    
            return MAD_FLOW_IGNORE;
        }

        /* Pipe the new buffer content to libmad's stream decoder facility.
         * Libmad never copies the buffer, but just references it. So keep
         * it in mad_adec_thread_t structure. */
        mad_stream_buffer( p_stream, (unsigned char*) &p_dec->buffer,
                           i_left + i_wanted );
        p_stream->error = 0;
    }

    return MAD_FLOW_CONTINUE;
}

/*****************************************************************************
 * libmad_output: this function is called just after the frame is decoded
 *****************************************************************************/
enum mad_flow libmad_output( void *p_data, struct mad_header const *p_header,
                             struct mad_pcm *p_pcm )
{
    mad_adec_thread_t * p_dec = (mad_adec_thread_t *) p_data;
    aout_buffer_t *     p_buffer;
    mad_fixed_t const * p_left = p_pcm->samples[0];
    mad_fixed_t const * p_right = p_pcm->samples[1];
    unsigned int        i_samples = p_pcm->length;
    mad_fixed_t *       p_samples;
    unsigned int        i_channels = (p_pcm->channels == 2) ?
                                     AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT :
                                     AOUT_CHAN_CENTER;

    /* Creating the audio output fifo. Assume the samplerate and nr of channels
     * from the first decoded frame is right for the entire audio track. */
    if( (p_dec->p_aout_input != NULL) &&
        (p_dec->output_format.i_rate != p_pcm->samplerate
           || p_dec->output_format.i_physical_channels != i_channels) )
    {
        /* Parameters changed - this should not happen. */
        aout_DecDelete( p_dec->p_aout, p_dec->p_aout_input );
        p_dec->p_aout_input = NULL;
    }

    /* Creating the audio input if not created yet. */
    if( p_dec->p_aout_input == NULL )
    {
        p_dec->output_format.i_rate = p_pcm->samplerate;
        p_dec->output_format.i_physical_channels = i_channels;
        p_dec->output_format.i_original_channels = i_channels;
        aout_DateInit( &p_dec->end_date, p_pcm->samplerate );
        p_dec->p_aout_input = aout_DecNew( p_dec->p_fifo,
                                           &p_dec->p_aout,
                                           &p_dec->output_format );

        if ( p_dec->p_aout_input == NULL )
        {
            p_dec->p_fifo->b_error = VLC_TRUE;
            return MAD_FLOW_BREAK;
        }
    }

    if( p_dec->i_current_pts )
    {
        /* Set the Presentation Time Stamp */
        if( p_dec->i_current_pts != aout_DateGet( &p_dec->end_date ) )
        {
            aout_DateSet( &p_dec->end_date, p_dec->i_current_pts );
        }

        p_dec->i_current_pts = 0;
    }
    else if( p_dec->i_next_pts )
    {
        /* No PTS this time, but it'll be for next frame */
        p_dec->i_current_pts = p_dec->i_next_pts;
        p_dec->i_next_pts = 0;
    }

    if( !aout_DateGet( &p_dec->end_date ) )
    {
        /* No date available yet, wait for the first PTS. */
        return MAD_FLOW_CONTINUE;
    }

    p_buffer = aout_DecNewBuffer( p_dec->p_aout, p_dec->p_aout_input, i_samples );

    if ( p_buffer == NULL )
    {
        msg_Err( p_dec->p_fifo, "allocating new buffer failed" );
        return MAD_FLOW_BREAK;
    }

    p_buffer->start_date = aout_DateGet( &p_dec->end_date );
    p_buffer->end_date = aout_DateIncrement( &p_dec->end_date, i_samples );

    /* Interleave and keep buffers in mad_fixed_t format */
    p_samples = (mad_fixed_t *)p_buffer->p_buffer;

    switch( p_pcm->channels )
    {
    case 2:
        while( i_samples-- )
        {
            *p_samples++ = *p_left++;
            *p_samples++ = *p_right++;
        }
        break;

    case 1:
        p_dec->p_fifo->p_vlc->pf_memcpy( p_samples, p_left,
                                         i_samples * sizeof(mad_fixed_t) );
        break;

    default:
        msg_Err( p_dec->p_fifo, "cannot interleave %i channels",
                                p_pcm->channels );
    }

    aout_DecPlay( p_dec->p_aout, p_dec->p_aout_input, p_buffer );

    return MAD_FLOW_CONTINUE;
}

/*****************************************************************************
 * libmad_error: this function is called when an error occurs during decoding
 *****************************************************************************/
enum mad_flow libmad_error( void *data, struct mad_stream *p_libmad_stream,
                            struct mad_frame *p_libmad_frame )
{
    mad_adec_thread_t *p_dec = (mad_adec_thread_t *) data;
    enum mad_flow result = MAD_FLOW_CONTINUE;

    switch (p_libmad_stream->error)
    {             
    case MAD_ERROR_BUFLEN:                /* input buffer too small (or EOF) */
        msg_Err( p_dec->p_fifo, "input buffer too small (or EOF)" );
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BUFPTR:                /* invalid (null) buffer pointer */
        msg_Err( p_dec->p_fifo, "invalid (null) buffer pointer" );
        result = MAD_FLOW_STOP;
        break;
    case MAD_ERROR_NOMEM:                 /* not enough memory */
        msg_Err( p_dec->p_fifo, "invalid (null) buffer pointer" );
        result = MAD_FLOW_STOP;
        break;
    case MAD_ERROR_LOSTSYNC:            /* lost synchronization */
        msg_Err( p_dec->p_fifo, "lost synchronization" );
        mad_stream_sync(p_libmad_stream);
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADLAYER:            /* reserved header layer value */
        msg_Err( p_dec->p_fifo, "reserved header layer value" );
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADBITRATE:        /* forbidden bitrate value */
        msg_Err( p_dec->p_fifo, "forbidden bitrate value" );
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADSAMPLERATE: /* reserved sample frequency value */
        msg_Err( p_dec->p_fifo, "reserved sample frequency value" );
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADEMPHASIS:     /* reserved emphasis value */
        msg_Err( p_dec->p_fifo, "reserverd emphasis value" );
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADCRC:                /* CRC check failed */
        msg_Err( p_dec->p_fifo, "CRC check failed" );
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADBITALLOC:     /* forbidden bit allocation value */
        msg_Err( p_dec->p_fifo, "forbidden bit allocation value" );
        result = MAD_FLOW_IGNORE;
        break;
    case MAD_ERROR_BADSCALEFACTOR:/* bad scalefactor index */
        msg_Err( p_dec->p_fifo, "bad scalefactor index" );
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADFRAMELEN:     /* bad frame length */
        msg_Err( p_dec->p_fifo, "bad frame length" );
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADBIGVALUES:    /* bad big_values count */
        msg_Err( p_dec->p_fifo, "bad big values count" );
        result = MAD_FLOW_IGNORE;
        break;
    case MAD_ERROR_BADBLOCKTYPE:    /* reserved block_type */
        msg_Err( p_dec->p_fifo, "reserverd block_type" );
        result = MAD_FLOW_IGNORE;
        break;
    case MAD_ERROR_BADSCFSI:            /* bad scalefactor selection info */
        msg_Err( p_dec->p_fifo, "bad scalefactor selection info" );
        result = MAD_FLOW_CONTINUE;
        break;
    case MAD_ERROR_BADDATAPTR:        /* bad main_data_begin pointer */
        msg_Err( p_dec->p_fifo, "bad main_data_begin pointer" );
        result = MAD_FLOW_STOP;
        break;
    case MAD_ERROR_BADPART3LEN:     /* bad audio data length */
        msg_Err( p_dec->p_fifo, "bad audio data length" );
        result = MAD_FLOW_IGNORE;
        break;
    case MAD_ERROR_BADHUFFTABLE:    /* bad Huffman table select */
        msg_Err( p_dec->p_fifo, "bad Huffman table select" );
        result = MAD_FLOW_IGNORE;
        break;
    case MAD_ERROR_BADHUFFDATA:     /* Huffman data overrun */
        msg_Err( p_dec->p_fifo, "Huffman data overrun" );
        result = MAD_FLOW_IGNORE;
        break;
    case MAD_ERROR_BADSTEREO:         /* incompatible block_type for JS */
        msg_Err( p_dec->p_fifo, "incompatible block_type for JS" );
        result = MAD_FLOW_IGNORE;
        break;
    default:
        msg_Err( p_dec->p_fifo, "unknown error occured stopping decoder" );
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
 * Print human readable informations about an audio MPEG frame.
 ****************************************************************************/
static void PrintFrameInfo(struct mad_header *Header)
{
	const char	*Layer,
			*Mode,
			*Emphasis;

	/* Convert the layer number to its printed representation. */
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

	/* Convert the audio mode to its printed representation. */
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

	/* Convert the emphasis to its printed representation. */
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
