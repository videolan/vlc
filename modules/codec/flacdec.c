/*****************************************************************************
 * flac.c: flac decoder module making use of libflac
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: flacdec.c,v 1.1 2003/02/23 16:31:48 sigmunau Exp $
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
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
#include <string.h>                                    /* memcpy(), memset() */
#include <errno.h>

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/decoder.h>
#include <input_ext-dec.h>

#include <vlc/input.h>

#include <FLAC/stream_decoder.h>

/*****************************************************************************
 * dec_thread_t : flac decoder thread descriptor
 *****************************************************************************/
typedef struct dec_thread_t
{
    /*
     * Thread properties
     */
    vlc_thread_t        thread_id;                /* id for thread functions */

    /*
     * Input properties
     */
    decoder_fifo_t         *p_fifo;            /* stores the PES stream data */
    pes_packet_t           *p_pes;            /* current PES we are decoding */
    int                     i_last_pes_pos;             /* possition into pes*/

    int i_tot;
    /*
     * libflac decoder struct
     */
    FLAC__StreamDecoder *p_decoder;
    
    /*
     * Output properties
     */
    aout_instance_t        *p_aout;
    aout_input_t           *p_aout_input;
    audio_sample_format_t   output_format;
    audio_date_t            end_date;
    mtime_t                 pts;

} dec_thread_t;

static int pi_channels_maps[6] =
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
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder  ( vlc_object_t * );
static int  RunDecoder   ( decoder_fifo_t * );
static void CloseDecoder ( dec_thread_t * );

static FLAC__StreamDecoderReadStatus DecoderReadCallback (const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], unsigned *bytes, void *client_data);

static FLAC__StreamDecoderWriteStatus DecoderWriteCallback (const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data);

static void DecoderMetadataCallback (const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
static void DecoderErrorCallback (const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);
static void Interleave32( int32_t *p_out, const int32_t * const *pp_in,
                        int i_nb_channels, int i_samples );
static void Interleave16( int16_t *p_out, const int32_t * const *pp_in,
                        int i_nb_channels, int i_samples );
static void decoder_state_error( dec_thread_t *p_dec, FLAC__StreamDecoderState state );
/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("flac decoder module") );
    set_capability( "decoder", 100 );
    set_callbacks( OpenDecoder, NULL );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;

    if( p_fifo->i_fourcc != VLC_FOURCC('f','l','a','c') )
    {
        return VLC_EGENERIC;
    }

    p_fifo->pf_run = RunDecoder;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * RunDecoder: the vorbis decoder
 *****************************************************************************/
static int RunDecoder( decoder_fifo_t * p_fifo )
{
    dec_thread_t *p_dec;
    FLAC__StreamDecoderState state;
    /* Allocate the memory needed to store the thread's structure */
    if( (p_dec = (dec_thread_t *)malloc (sizeof(dec_thread_t)) )
            == NULL)
    {
        msg_Err( p_fifo, "out of memory" );
        goto error;
    }

    /* Initialize the thread properties */
    memset( p_dec, 0, sizeof(dec_thread_t) );
    p_dec->p_fifo = p_fifo;
    p_dec->p_pes  = NULL;
    p_dec->p_decoder = FLAC__stream_decoder_new();
    if( p_dec->p_decoder == NULL )
    {
        msg_Err( p_fifo, "FLAC__stream_decoder_new() failed" );
        goto error;
    }
    FLAC__stream_decoder_set_read_callback( p_dec->p_decoder,
                                               DecoderReadCallback );
    FLAC__stream_decoder_set_write_callback( p_dec->p_decoder,
                                               DecoderWriteCallback );
    FLAC__stream_decoder_set_metadata_callback( p_dec->p_decoder,
                                               DecoderMetadataCallback );
    FLAC__stream_decoder_set_error_callback( p_dec->p_decoder,
                                               DecoderErrorCallback );
    FLAC__stream_decoder_set_client_data( p_dec->p_decoder,
                                             p_dec );


    FLAC__stream_decoder_init( p_dec->p_decoder );
    if ( !FLAC__stream_decoder_process_until_end_of_metadata( p_dec->p_decoder ) )
    {
        state = FLAC__stream_decoder_get_state( p_dec->p_decoder );
        decoder_state_error( p_dec, state );                
        goto error;
    }

    aout_DateInit( &p_dec->end_date, p_dec->output_format.i_rate );
    p_dec->p_aout = NULL;
    p_dec->p_aout_input = aout_DecNew( p_dec->p_fifo,
                                       &p_dec->p_aout,
                                       &p_dec->output_format );

    if( p_dec->p_aout_input == NULL )
    {
        msg_Err( p_dec->p_fifo, "failed to create aout fifo" );
        goto error;
    }
    
    /* vorbis decoder thread's main loop */
    while( (!p_dec->p_fifo->b_die) && (!p_dec->p_fifo->b_error) )
    { 
        if ( !FLAC__stream_decoder_process_single( p_dec->p_decoder ) )
        {
            state = FLAC__stream_decoder_get_state( p_dec->p_decoder );
            decoder_state_error( p_dec, state );
        }
    }

    /* If b_error is set, the vorbis decoder thread enters the error loop */
    if( p_dec->p_fifo->b_error )
    {
        DecoderError( p_dec->p_fifo );
    }

    /* End of the vorbis decoder thread */
    CloseDecoder( p_dec );

    return 0;

 error:
    DecoderError( p_fifo );
    if( p_dec )
    {
        if( p_dec->p_fifo )
            p_dec->p_fifo->b_error = 1;

        /* End of the vorbis decoder thread */
        CloseDecoder( p_dec );
    }

    return -1;
}

/*****************************************************************************
 * CloseDecoder: closes the decoder
 *****************************************************************************/
static void CloseDecoder ( dec_thread_t *p_dec )
{
    if( p_dec->p_aout_input != NULL )
    {
        aout_DecDelete( p_dec->p_aout, p_dec->p_aout_input );
    }

    if( p_dec )
    {
        if( p_dec->p_pes )
            input_DeletePES( p_dec->p_fifo->p_packets_mgt, p_dec->p_pes );
        FLAC__stream_decoder_finish( p_dec->p_decoder );
        FLAC__stream_decoder_delete( p_dec->p_decoder );
        free( p_dec );
    }

}


    
/*****************************************************************************
 * DecoderReadCallback: called by libflac when it needs more data
 *****************************************************************************/
static FLAC__StreamDecoderReadStatus DecoderReadCallback (const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], unsigned *bytes, void *client_data)
{
    dec_thread_t *p_dec = (dec_thread_t *)client_data;
    if( !p_dec->i_last_pes_pos )
    {
        input_DeletePES( p_dec->p_fifo->p_packets_mgt,
                         p_dec->p_pes );
        input_ExtractPES( p_dec->p_fifo, &p_dec->p_pes );
        if( !p_dec->p_pes )
        {
            return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
        }
    }
    p_dec->pts = p_dec->p_pes->i_pts;
    if( ( p_dec->p_pes->i_pes_size - p_dec->i_last_pes_pos ) > *bytes )
    {
        p_dec->p_fifo->p_vlc->pf_memcpy( buffer,
                                         p_dec->p_pes->p_first->p_payload_start
                                         + p_dec->i_last_pes_pos,
                                         *bytes );
        p_dec->i_last_pes_pos += *bytes;
    }
    else
    {
        p_dec->p_fifo->p_vlc->pf_memcpy( buffer,
                                         p_dec->p_pes->p_first->p_payload_start
                                         + p_dec->i_last_pes_pos,
                                         p_dec->p_pes->i_pes_size
                                         - p_dec->i_last_pes_pos );
        *bytes = p_dec->p_pes->i_pes_size - p_dec->i_last_pes_pos ;
        p_dec->i_last_pes_pos = 0;
    }
    p_dec->i_tot += *bytes;

    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

/*****************************************************************************
 * DecoderWriteCallback: called by libflac to output decoded samples
 *****************************************************************************/
static FLAC__StreamDecoderWriteStatus DecoderWriteCallback (
    const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame,
    const FLAC__int32 *const buffer[], void *client_data )
{
    dec_thread_t *p_dec = (dec_thread_t *)client_data;
    int i_samples = frame->header.blocksize;
    aout_buffer_t *p_aout_buffer;
    p_aout_buffer = aout_DecNewBuffer( p_dec->p_aout, p_dec->p_aout_input,
                                       i_samples );
    if( !p_aout_buffer )
    {
        msg_Err( p_dec->p_fifo, "cannot get aout buffer" );
        p_dec->p_fifo->b_error = 1;
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }
    switch ( frame->header.bits_per_sample )
    {
    case 16:
        Interleave16( (int16_t *)p_aout_buffer->p_buffer, buffer,
                frame->header.channels, i_samples );
        break;
    default:
        Interleave32( (int32_t *)p_aout_buffer->p_buffer, buffer,
                frame->header.channels, i_samples );
    }
        
    if( p_dec->pts != 0 && p_dec->pts != aout_DateGet( &p_dec->end_date ) )
    {
        aout_DateSet( &p_dec->end_date, p_dec->pts );
        p_dec->pts = 0;
    }
    else if( !aout_DateGet( &p_dec->end_date ) )
    {
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    /* Date management */
    p_aout_buffer->start_date = aout_DateGet( &p_dec->end_date );
    p_aout_buffer->end_date = aout_DateIncrement( &p_dec->end_date,
                                                  i_samples );
    aout_DecPlay( p_dec->p_aout, p_dec->p_aout_input, p_aout_buffer );
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

/*****************************************************************************
' * DecoderMetadataCallback: called by libflac to when it encounters metadata
 *****************************************************************************/
static void DecoderMetadataCallback (const FLAC__StreamDecoder *decoder,
                                     const FLAC__StreamMetadata *metadata,
                                     void *client_data)
{
    dec_thread_t *p_dec = (dec_thread_t *)client_data;
    switch ( metadata->data.stream_info.bits_per_sample )
    {
    case 8:
        p_dec->output_format.i_format = VLC_FOURCC('s','8',' ',' ');
        break;
    case 16:
        p_dec->output_format.i_format = AOUT_FMT_S16_NE;
        break;
    default:
        msg_Dbg( p_dec->p_fifo, "strange bps %d",
                 metadata->data.stream_info.bits_per_sample );
        p_dec->output_format.i_format = VLC_FOURCC('f','i','3','2');
        break;
    }
    p_dec->output_format.i_physical_channels =
        p_dec->output_format.i_original_channels =
            pi_channels_maps[metadata->data.stream_info.channels];
    p_dec->output_format.i_rate = metadata->data.stream_info.sample_rate;

    return;
}

/*****************************************************************************
 * DecoderErrorCallback: called when the libflac decoder encounters an error
 *****************************************************************************/
static void DecoderErrorCallback (const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
    dec_thread_t *p_dec = (dec_thread_t *)client_data;
    switch ( status )
    {
    case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC :
        msg_Err( p_dec->p_fifo, "An error in the stream caused the decoder to lose synchronization.");
        break;

    case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER :
        msg_Err( p_dec->p_fifo, "The decoder encountered a corrupted frame header.");
        break;

    case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH :
        msg_Err( p_dec->p_fifo, "The frame's data did not match the CRC in the footer.");
        break;
    default:
        msg_Err( p_dec->p_fifo, "got decoder error: %d", status );
    }
    return;
}

/*****************************************************************************
 * Interleave: helper function to interleave channels
 *****************************************************************************/
static void Interleave32( int32_t *p_out, const int32_t * const *pp_in,
                        int i_nb_channels, int i_samples )
{
    int i, j;

    for ( j = 0; j < i_samples; j++ )
    {
        for ( i = 0; i < i_nb_channels; i++ )
        {
            p_out[j * i_nb_channels + i] = pp_in[i][j];
        }
    }
}
static void Interleave16( int16_t *p_out, const int32_t * const *pp_in,
                        int i_nb_channels, int i_samples )
{
    int i, j;

    for ( j = 0; j < i_samples; j++ )
    {
        for ( i = 0; i < i_nb_channels; i++ )
        {
            p_out[j * i_nb_channels + i] = (int32_t)(pp_in[i][j]);
        }
    }
}


static void decoder_state_error( dec_thread_t *p_dec, FLAC__StreamDecoderState state )
{
    switch ( state )
    {
    case FLAC__STREAM_DECODER_SEARCH_FOR_METADATA  : 
        msg_Err( p_dec->p_fifo, "The decoder is ready to search for metadata.");
        break;
    case FLAC__STREAM_DECODER_READ_METADATA  : 
        msg_Err( p_dec->p_fifo, "The decoder is ready to or is in the process of reading metadata.");
        break;
    case FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC  : 
        msg_Err( p_dec->p_fifo, "The decoder is ready to or is in the process of searching for the frame sync code.");
        break;
    case FLAC__STREAM_DECODER_READ_FRAME  : 
        msg_Err( p_dec->p_fifo, "The decoder is ready to or is in the process of reading a frame.");
        break;
    case FLAC__STREAM_DECODER_END_OF_STREAM  : 
        msg_Err( p_dec->p_fifo, "The decoder has reached the end of the stream.");
        break;
    case FLAC__STREAM_DECODER_ABORTED  : 
        msg_Err( p_dec->p_fifo, "The decoder was aborted by the read callback.");
        break;
    case FLAC__STREAM_DECODER_UNPARSEABLE_STREAM  : 
        msg_Err( p_dec->p_fifo, "The decoder encountered reserved fields in use in the stream.");
        break;
    case FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR  : 
        msg_Err( p_dec->p_fifo, "An error occurred allocating memory.");
        break;
    case FLAC__STREAM_DECODER_ALREADY_INITIALIZED  : 
        msg_Err( p_dec->p_fifo, "FLAC__stream_decoder_init() was called when the decoder was already initialized, usually because FLAC__stream_decoder_finish() was not called.");
        break;
    case FLAC__STREAM_DECODER_INVALID_CALLBACK  : 
        msg_Err( p_dec->p_fifo, "FLAC__stream_decoder_init() was called without all callbacks being set.");
        break;
    case FLAC__STREAM_DECODER_UNINITIALIZED  : 
        msg_Err( p_dec->p_fifo, "The decoder is in the uninitialized state.");
        break;
    default:
        msg_Err(p_dec->p_fifo, "unknown error" );
    }
}
