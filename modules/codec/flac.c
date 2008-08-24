/*****************************************************************************
 * flac.c: flac decoder/packetizer/encoder module making use of libflac
 *****************************************************************************
 * Copyright (C) 1999-2001 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Sigmund Augdal Helberg <dnumgis@videolan.org>
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
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_aout.h>

#ifdef HAVE_FLAC_STREAM_DECODER_H
#   include <FLAC/stream_decoder.h>
#   include <FLAC/stream_encoder.h>
#   define USE_LIBFLAC
#endif

#include <vlc_block_helper.h>
#include <vlc_bits.h>

#define MAX_FLAC_HEADER_SIZE 16

#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT >= 8
#   define USE_NEW_FLAC_API
#endif

/*****************************************************************************
 * decoder_sys_t : FLAC decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /*
     * Input properties
     */
    int i_state;

    block_bytestream_t bytestream;

    /*
     * Input/Output properties
     */
    block_t *p_block;
    aout_buffer_t *p_aout_buffer;

    /*
     * FLAC properties
     */
#ifdef USE_LIBFLAC
    FLAC__StreamDecoder *p_flac;
    FLAC__StreamMetadata_StreamInfo stream_info;
#else
    struct
    {
        unsigned min_blocksize, max_blocksize;
        unsigned min_framesize, max_framesize;
        unsigned sample_rate;
        unsigned channels;
        unsigned bits_per_sample;

    } stream_info;
#endif
    bool b_stream_info;

    /*
     * Common properties
     */
    audio_date_t end_date;
    mtime_t i_pts;

    int i_frame_size, i_frame_length, i_bits_per_sample;
    unsigned int i_rate, i_channels, i_channels_conf;
};

enum {

    STATE_NOSYNC,
    STATE_SYNC,
    STATE_HEADER,
    STATE_NEXT_SYNC,
    STATE_GET_DATA,
    STATE_SEND_DATA
};

static const int pi_channels_maps[7] =
{
    0,
    AOUT_CHAN_CENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static int  OpenPacketizer( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

#ifdef USE_LIBFLAC
static int OpenEncoder   ( vlc_object_t * );
static void CloseEncoder ( vlc_object_t * );
#endif

#ifdef USE_LIBFLAC
static aout_buffer_t *DecodeBlock( decoder_t *, block_t ** );
#endif
static block_t *PacketizeBlock( decoder_t *, block_t ** );

static int SyncInfo( decoder_t *, uint8_t *, unsigned int *, unsigned int *,
                     unsigned int *,int * );


#ifdef USE_LIBFLAC
static FLAC__StreamDecoderReadStatus
DecoderReadCallback( const FLAC__StreamDecoder *decoder,
                     FLAC__byte buffer[], unsigned *bytes, void *client_data );

static FLAC__StreamDecoderWriteStatus
DecoderWriteCallback( const FLAC__StreamDecoder *decoder,
                      const FLAC__Frame *frame,
                      const FLAC__int32 *const buffer[], void *client_data );

static void DecoderMetadataCallback( const FLAC__StreamDecoder *decoder,
                                     const FLAC__StreamMetadata *metadata,
                                     void *client_data );
static void DecoderErrorCallback( const FLAC__StreamDecoder *decoder,
                                  FLAC__StreamDecoderErrorStatus status,
                                  void *client_data);

static void Interleave32( int32_t *p_out, const int32_t * const *pp_in,
                          int i_nb_channels, int i_samples );
static void Interleave24( int8_t *p_out, const int32_t * const *pp_in,
                          int i_nb_channels, int i_samples );
static void Interleave16( int16_t *p_out, const int32_t * const *pp_in,
                          int i_nb_channels, int i_samples );

static void decoder_state_error( decoder_t *p_dec,
                                 FLAC__StreamDecoderState state );
#endif

static uint64_t read_utf8( const uint8_t *p_buf, int *pi_read );
static uint8_t flac_crc8( const uint8_t *data, unsigned len );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();

    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACODEC );
    add_shortcut( "flac" );

#ifdef USE_LIBFLAC
    set_description( N_("Flac audio decoder") );
    set_capability( "decoder", 100 );
    set_callbacks( OpenDecoder, CloseDecoder );

    add_submodule();
    set_description( N_("Flac audio encoder") );
    set_capability( "encoder", 100 );
    set_callbacks( OpenEncoder, CloseEncoder );

    add_submodule();
#endif
    set_description( N_("Flac audio packetizer") );
    set_capability( "packetizer", 100 );
    set_callbacks( OpenPacketizer, CloseDecoder );

vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('f','l','a','c') )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;

    /* Misc init */
    aout_DateSet( &p_sys->end_date, 0 );
    p_sys->i_state = STATE_NOSYNC;
    p_sys->b_stream_info = false;
    p_sys->p_block=NULL;
    p_sys->bytestream = block_BytestreamInit();

#ifdef USE_LIBFLAC
    /* Take care of flac init */
    if( !(p_sys->p_flac = FLAC__stream_decoder_new()) )
    {
        msg_Err( p_dec, "FLAC__stream_decoder_new() failed" );
        free( p_sys );
        return VLC_EGENERIC;
    }

#ifdef USE_NEW_FLAC_API
    if( FLAC__stream_decoder_init_stream( p_sys->p_flac,
                                          DecoderReadCallback,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          DecoderWriteCallback,
                                          DecoderMetadataCallback,
                                          DecoderErrorCallback,
                                          p_dec )
        != FLAC__STREAM_DECODER_INIT_STATUS_OK )
    {
        msg_Err( p_dec, "FLAC__stream_decoder_init_stream() failed" );
        FLAC__stream_decoder_delete( p_sys->p_flac );
        free( p_sys );
        return VLC_EGENERIC;
    }
#else
    FLAC__stream_decoder_set_read_callback( p_sys->p_flac,
                                            DecoderReadCallback );
    FLAC__stream_decoder_set_write_callback( p_sys->p_flac,
                                             DecoderWriteCallback );
    FLAC__stream_decoder_set_metadata_callback( p_sys->p_flac,
                                                DecoderMetadataCallback );
    FLAC__stream_decoder_set_error_callback( p_sys->p_flac,
                                             DecoderErrorCallback );
    FLAC__stream_decoder_set_client_data( p_sys->p_flac, p_dec );

    FLAC__stream_decoder_init( p_sys->p_flac );
#endif
#endif

    /* Set output properties */
    p_dec->fmt_out.i_cat = AUDIO_ES;
    p_dec->fmt_out.i_codec = VLC_FOURCC('f','l','3','2');

    /* Set callbacks */
#ifdef USE_LIBFLAC
    p_dec->pf_decode_audio = DecodeBlock;
#endif

    return VLC_SUCCESS;
}

static int OpenPacketizer( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    es_format_t es_save = p_dec->fmt_out;
    int i_ret;

    /* */
    es_format_Copy( &p_dec->fmt_out, &p_dec->fmt_in );

    i_ret = OpenDecoder( p_this );
    p_dec->pf_decode_audio = NULL;
    p_dec->pf_packetize    = PacketizeBlock;

    /* Set output properties */
    p_dec->fmt_out.i_codec = VLC_FOURCC('f','l','a','c');

    if( i_ret != VLC_SUCCESS )
    {
        es_format_Clean( &p_dec->fmt_out );
        p_dec->fmt_out = es_save;
    }
    return i_ret;
}

/*****************************************************************************
 * CloseDecoder: flac decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

#ifdef USE_LIBFLAC
    FLAC__stream_decoder_finish( p_sys->p_flac );
    FLAC__stream_decoder_delete( p_sys->p_flac );
#endif

    free( p_sys->p_block );
    free( p_sys );
}

/*****************************************************************************
 * ProcessHeader: process Flac header.
 *****************************************************************************/
static void ProcessHeader( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

#ifdef USE_LIBFLAC
    if( !p_dec->fmt_in.i_extra ) return;

    /* Decode STREAMINFO */
    msg_Dbg( p_dec, "decode STREAMINFO" );
    p_sys->p_block = block_New( p_dec, p_dec->fmt_in.i_extra );
    memcpy( p_sys->p_block->p_buffer, p_dec->fmt_in.p_extra,
            p_dec->fmt_in.i_extra );
    FLAC__stream_decoder_process_until_end_of_metadata( p_sys->p_flac );
    msg_Dbg( p_dec, "STREAMINFO decoded" );

#else
    bs_t bs;

    if( !p_dec->fmt_in.i_extra ) return;

    bs_init( &bs, p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra );

    p_sys->stream_info.min_blocksize = bs_read( &bs, 16 );
    p_sys->stream_info.max_blocksize = bs_read( &bs, 16 );

    p_sys->stream_info.min_framesize = bs_read( &bs, 24 );
    p_sys->stream_info.max_framesize = bs_read( &bs, 24 );

    p_sys->stream_info.sample_rate = bs_read( &bs, 20 );
    p_sys->stream_info.channels = bs_read( &bs, 3 ) + 1;
    p_sys->stream_info.bits_per_sample = bs_read( &bs, 5 ) + 1;
#endif

    if( !p_sys->b_stream_info ) return;

    if( p_dec->fmt_out.i_codec == VLC_FOURCC('f','l','a','c') )
    {
        p_dec->fmt_out.i_extra = p_dec->fmt_in.i_extra;
        p_dec->fmt_out.p_extra =
            realloc( p_dec->fmt_out.p_extra, p_dec->fmt_out.i_extra );
        memcpy( p_dec->fmt_out.p_extra,
                p_dec->fmt_in.p_extra, p_dec->fmt_out.i_extra );
    }
}

/****************************************************************************
 * PacketizeBlock: the whole thing
 ****************************************************************************
 * This function is called just after the thread is launched.
 ****************************************************************************/
static block_t *PacketizeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t p_header[MAX_FLAC_HEADER_SIZE];
    block_t *p_sout_block;

    if( !pp_block || !*pp_block ) return NULL;

    if( (*pp_block)->i_flags&(BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
        if( (*pp_block)->i_flags&BLOCK_FLAG_CORRUPTED )
        {
            p_sys->i_state = STATE_NOSYNC;
            block_BytestreamFlush( &p_sys->bytestream );
        }
//        aout_DateSet( &p_sys->end_date, 0 );
        block_Release( *pp_block );
        return NULL;
    }

    if( !p_sys->b_stream_info ) ProcessHeader( p_dec );

    if( p_sys->stream_info.channels > 6 )
    {
        msg_Err( p_dec, "This stream uses too many audio channels" );
        return NULL;
    }

    if( !aout_DateGet( &p_sys->end_date ) && !(*pp_block)->i_pts )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( *pp_block );
        return NULL;
    }
    else if( !aout_DateGet( &p_sys->end_date ) )
    {
        /* The first PTS is as good as anything else. */
        p_sys->i_rate = p_dec->fmt_out.audio.i_rate;
        aout_DateInit( &p_sys->end_date, p_sys->i_rate );
        aout_DateSet( &p_sys->end_date, (*pp_block)->i_pts );
    }

    block_BytestreamPush( &p_sys->bytestream, *pp_block );

    while( 1 )
    {
        switch( p_sys->i_state )
        {
        case STATE_NOSYNC:
            while( block_PeekBytes( &p_sys->bytestream, p_header, 2 )
                   == VLC_SUCCESS )
            {
                if( p_header[0] == 0xFF && p_header[1] == 0xF8 )
                {
                    p_sys->i_state = STATE_SYNC;
                    break;
                }
                block_SkipByte( &p_sys->bytestream );
            }
            if( p_sys->i_state != STATE_SYNC )
            {
                block_BytestreamFlush( &p_sys->bytestream );

                /* Need more data */
                return NULL;
            }

        case STATE_SYNC:
            /* New frame, set the Presentation Time Stamp */
            p_sys->i_pts = p_sys->bytestream.p_block->i_pts;
            if( p_sys->i_pts != 0 &&
                p_sys->i_pts != aout_DateGet( &p_sys->end_date ) )
            {
                aout_DateSet( &p_sys->end_date, p_sys->i_pts );
            }
            p_sys->i_state = STATE_HEADER;

        case STATE_HEADER:
            /* Get FLAC frame header (MAX_FLAC_HEADER_SIZE bytes) */
            if( block_PeekBytes( &p_sys->bytestream, p_header,
                                 MAX_FLAC_HEADER_SIZE ) != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }

            /* Check if frame is valid and get frame info */
            p_sys->i_frame_length = SyncInfo( p_dec, p_header,
                                              &p_sys->i_channels,
                                              &p_sys->i_channels_conf,
                                              &p_sys->i_rate,
                                              &p_sys->i_bits_per_sample );
            if( !p_sys->i_frame_length )
            {
                msg_Dbg( p_dec, "emulated sync word" );
                block_SkipByte( &p_sys->bytestream );
                p_sys->i_state = STATE_NOSYNC;
                break;
            }
            if( p_sys->i_rate != p_dec->fmt_out.audio.i_rate )
            {
                p_dec->fmt_out.audio.i_rate = p_sys->i_rate;
                aout_DateInit( &p_sys->end_date, p_sys->i_rate );
            }
            p_sys->i_state = STATE_NEXT_SYNC;
            p_sys->i_frame_size = 1;

        case STATE_NEXT_SYNC:
            /* TODO: If pp_block == NULL, flush the buffer without checking the
             * next sync word */

            /* Check if next expected frame contains the sync word */
            while( block_PeekOffsetBytes( &p_sys->bytestream,
                                          p_sys->i_frame_size, p_header,
                                          MAX_FLAC_HEADER_SIZE )
                   == VLC_SUCCESS )
            {
                if( p_header[0] == 0xFF && p_header[1] == 0xF8 )
                {
                    /* Check if frame is valid and get frame info */
                    int i_frame_length =
                        SyncInfo( p_dec, p_header,
                                  &p_sys->i_channels,
                                  &p_sys->i_channels_conf,
                                  &p_sys->i_rate,
                                  &p_sys->i_bits_per_sample );

                    if( i_frame_length )
                    {
                        p_sys->i_state = STATE_SEND_DATA;
                        break;
                    }
                }
                p_sys->i_frame_size++;
            }

            if( p_sys->i_state != STATE_SEND_DATA )
            {
                /* Need more data */
                return NULL;
            }

        case STATE_SEND_DATA:
            p_sout_block = block_New( p_dec, p_sys->i_frame_size );

            /* Copy the whole frame into the buffer. When we reach this point
             * we already know we have enough data available. */
            block_GetBytes( &p_sys->bytestream, p_sout_block->p_buffer,
                            p_sys->i_frame_size );

            /* Make sure we don't reuse the same pts twice */
            if( p_sys->i_pts == p_sys->bytestream.p_block->i_pts )
                p_sys->i_pts = p_sys->bytestream.p_block->i_pts = 0;

            /* So p_block doesn't get re-added several times */
            *pp_block = block_BytestreamPop( &p_sys->bytestream );

            p_sys->i_state = STATE_NOSYNC;

            /* Date management */
            p_sout_block->i_pts =
                p_sout_block->i_dts = aout_DateGet( &p_sys->end_date );
            aout_DateIncrement( &p_sys->end_date, p_sys->i_frame_length );
            p_sout_block->i_length =
                aout_DateGet( &p_sys->end_date ) - p_sout_block->i_pts;

            return p_sout_block;
        }
    }

    return NULL;
}

#ifdef USE_LIBFLAC
/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************/
static aout_buffer_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( !pp_block || !*pp_block ) return NULL;

    p_sys->p_aout_buffer = 0;
    if( ( p_sys->p_block = PacketizeBlock( p_dec, pp_block ) ) )
    {
        if( !FLAC__stream_decoder_process_single( p_sys->p_flac ) )
        {
            decoder_state_error( p_dec,
                FLAC__stream_decoder_get_state( p_sys->p_flac ) );
            FLAC__stream_decoder_flush( p_dec->p_sys->p_flac );
        }

        /* If the decoder is in the "aborted" state,
         * FLAC__stream_decoder_process_single() won't return an error. */
        if( FLAC__stream_decoder_get_state(p_dec->p_sys->p_flac)
            == FLAC__STREAM_DECODER_ABORTED )
        {
            FLAC__stream_decoder_flush( p_dec->p_sys->p_flac );
        }

        block_Release( p_sys->p_block );
        p_sys->p_block = NULL;
    }

    return p_sys->p_aout_buffer;
}

/*****************************************************************************
 * DecoderReadCallback: called by libflac when it needs more data
 *****************************************************************************/
static FLAC__StreamDecoderReadStatus
DecoderReadCallback( const FLAC__StreamDecoder *decoder, FLAC__byte buffer[],
                     unsigned *bytes, void *client_data )
{
    VLC_UNUSED(decoder);
    decoder_t *p_dec = (decoder_t *)client_data;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_block && p_sys->p_block->i_buffer )
    {
        *bytes = __MIN(*bytes, (unsigned)p_sys->p_block->i_buffer);
        memcpy( buffer, p_sys->p_block->p_buffer, *bytes );
        p_sys->p_block->i_buffer -= *bytes;
        p_sys->p_block->p_buffer += *bytes;
    }
    else
    {
        *bytes = 0;
        return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    }

    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

/*****************************************************************************
 * DecoderWriteCallback: called by libflac to output decoded samples
 *****************************************************************************/
static FLAC__StreamDecoderWriteStatus
DecoderWriteCallback( const FLAC__StreamDecoder *decoder,
                      const FLAC__Frame *frame,
                      const FLAC__int32 *const buffer[], void *client_data )
{
    VLC_UNUSED(decoder);
    decoder_t *p_dec = (decoder_t *)client_data;
    decoder_sys_t *p_sys = p_dec->p_sys;

    p_sys->p_aout_buffer =
        p_dec->pf_aout_buffer_new( p_dec, frame->header.blocksize );

    if( p_sys->p_aout_buffer == NULL )
        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;

    switch( frame->header.bits_per_sample )
    {
    case 16:
        Interleave16( (int16_t *)p_sys->p_aout_buffer->p_buffer, buffer,
                      frame->header.channels, frame->header.blocksize );
        break;
    case 24:
        Interleave24( (int8_t *)p_sys->p_aout_buffer->p_buffer, buffer,
                      frame->header.channels, frame->header.blocksize );
        break;
    default:
        Interleave32( (int32_t *)p_sys->p_aout_buffer->p_buffer, buffer,
                      frame->header.channels, frame->header.blocksize );
    }

    /* Date management (already done by packetizer) */
    p_sys->p_aout_buffer->start_date = p_sys->p_block->i_pts;
    p_sys->p_aout_buffer->end_date =
        p_sys->p_block->i_pts + p_sys->p_block->i_length;

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

/*****************************************************************************
 * DecoderMetadataCallback: called by libflac to when it encounters metadata
 *****************************************************************************/
static void DecoderMetadataCallback( const FLAC__StreamDecoder *decoder,
                                     const FLAC__StreamMetadata *metadata,
                                     void *client_data )
{
    VLC_UNUSED(decoder);
    decoder_t *p_dec = (decoder_t *)client_data;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_dec->pf_decode_audio )
    {
        switch( metadata->data.stream_info.bits_per_sample )
        {
        case 8:
            p_dec->fmt_out.i_codec = VLC_FOURCC('s','8',' ',' ');
            break;
        case 16:
            p_dec->fmt_out.i_codec = AOUT_FMT_S16_NE;
            break;
        case 24:
            p_dec->fmt_out.i_codec = AOUT_FMT_S24_NE;
            break;
        default:
            msg_Dbg( p_dec, "strange bit/sample value: %d",
                     metadata->data.stream_info.bits_per_sample );
            p_dec->fmt_out.i_codec = VLC_FOURCC('f','i','3','2');
            break;
        }
    }

    /* Setup the format */
    p_dec->fmt_out.audio.i_rate     = metadata->data.stream_info.sample_rate;
    p_dec->fmt_out.audio.i_channels = metadata->data.stream_info.channels;
    p_dec->fmt_out.audio.i_physical_channels =
        p_dec->fmt_out.audio.i_original_channels =
            pi_channels_maps[metadata->data.stream_info.channels];
    p_dec->fmt_out.audio.i_bitspersample =
        metadata->data.stream_info.bits_per_sample;

    aout_DateInit( &p_sys->end_date, p_dec->fmt_out.audio.i_rate );

    msg_Dbg( p_dec, "channels:%d samplerate:%d bitspersamples:%d",
             p_dec->fmt_out.audio.i_channels, p_dec->fmt_out.audio.i_rate,
             p_dec->fmt_out.audio.i_bitspersample );

    p_sys->b_stream_info = true;
    p_sys->stream_info = metadata->data.stream_info;

    return;
}

/*****************************************************************************
 * DecoderErrorCallback: called when the libflac decoder encounters an error
 *****************************************************************************/
static void DecoderErrorCallback( const FLAC__StreamDecoder *decoder,
                                  FLAC__StreamDecoderErrorStatus status,
                                  void *client_data )
{
    VLC_UNUSED(decoder);
    decoder_t *p_dec = (decoder_t *)client_data;

    switch( status )
    {
    case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC:
        msg_Warn( p_dec, "an error in the stream caused the decoder to "
                 "lose synchronization." );
        break;
    case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
        msg_Err( p_dec, "the decoder encountered a corrupted frame header." );
        break;
    case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
        msg_Err( p_dec, "frame's data did not match the CRC in the "
                 "footer." );
        break;
    default:
        msg_Err( p_dec, "got decoder error: %d", status );
    }

    FLAC__stream_decoder_flush( p_dec->p_sys->p_flac );
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

static void Interleave24( int8_t *p_out, const int32_t * const *pp_in,
                          int i_nb_channels, int i_samples )
{
    int i, j;
    for ( j = 0; j < i_samples; j++ )
    {
        for ( i = 0; i < i_nb_channels; i++ )
        {
#ifdef WORDS_BIGENDIAN
            p_out[3*(j * i_nb_channels + i)+0] = (pp_in[i][j] >> 16) & 0xff;
            p_out[3*(j * i_nb_channels + i)+1] = (pp_in[i][j] >> 8 ) & 0xff;
            p_out[3*(j * i_nb_channels + i)+2] = (pp_in[i][j] >> 0 ) & 0xff;
#else
            p_out[3*(j * i_nb_channels + i)+2] = (pp_in[i][j] >> 16) & 0xff;
            p_out[3*(j * i_nb_channels + i)+1] = (pp_in[i][j] >> 8 ) & 0xff;
            p_out[3*(j * i_nb_channels + i)+0] = (pp_in[i][j] >> 0 ) & 0xff;
#endif
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

/*****************************************************************************
 * decoder_state_error: print meaningful error messages
 *****************************************************************************/
static void decoder_state_error( decoder_t *p_dec,
                                 FLAC__StreamDecoderState state )
{
    switch ( state )
    {
    case FLAC__STREAM_DECODER_SEARCH_FOR_METADATA:
        msg_Dbg( p_dec, "the decoder is ready to search for metadata." );
        break;
    case FLAC__STREAM_DECODER_READ_METADATA:
        msg_Dbg( p_dec, "the decoder is ready to or is in the process of "
                 "reading metadata." );
        break;
    case FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC:
        msg_Dbg( p_dec, "the decoder is ready to or is in the process of "
                 "searching for the frame sync code." );
        break;
    case FLAC__STREAM_DECODER_READ_FRAME:
        msg_Dbg( p_dec, "the decoder is ready to or is in the process of "
                 "reading a frame." );
        break;
    case FLAC__STREAM_DECODER_END_OF_STREAM:
        msg_Dbg( p_dec, "the decoder has reached the end of the stream." );
        break;
#ifdef USE_NEW_FLAC_API
    case FLAC__STREAM_DECODER_OGG_ERROR:
        msg_Err( p_dec, "error occurred in the Ogg layer." );
        break;
    case FLAC__STREAM_DECODER_SEEK_ERROR:
        msg_Err( p_dec, "error occurred while seeking." );
        break;
#endif
    case FLAC__STREAM_DECODER_ABORTED:
        msg_Warn( p_dec, "the decoder was aborted by the read callback." );
        break;
#ifndef USE_NEW_FLAC_API
    case FLAC__STREAM_DECODER_UNPARSEABLE_STREAM:
        msg_Warn( p_dec, "the decoder encountered reserved fields in use "
                 "in the stream." );
        break;
#endif
    case FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
        msg_Err( p_dec, "error when allocating memory." );
        break;
#ifndef USE_NEW_FLAC_API
    case FLAC__STREAM_DECODER_ALREADY_INITIALIZED:
        msg_Err( p_dec, "FLAC__stream_decoder_init() was called when the "
                 "decoder was already initialized, usually because "
                 "FLAC__stream_decoder_finish() was not called." );
        break;
    case FLAC__STREAM_DECODER_INVALID_CALLBACK:
        msg_Err( p_dec, "FLAC__stream_decoder_init() was called without "
                 "all callbacks being set." );
        break;
#endif
    case FLAC__STREAM_DECODER_UNINITIALIZED:
        msg_Err( p_dec, "decoder in uninitialized state." );
        break;
    default:
        msg_Warn(p_dec, "unknown error" );
    }
}
#endif

/*****************************************************************************
 * SyncInfo: parse FLAC sync info
 *****************************************************************************/
static int SyncInfo( decoder_t *p_dec, uint8_t *p_buf,
                     unsigned int * pi_channels,
                     unsigned int * pi_channels_conf,
                     unsigned int * pi_sample_rate,
                     int * pi_bits_per_sample )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_header, i_temp, i_read;
    int i_blocksize = 0, i_blocksize_hint = 0, i_sample_rate_hint = 0;
    uint64_t i_sample_number = 0;

    bool b_variable_blocksize = ( p_sys->b_stream_info &&
        p_sys->stream_info.min_blocksize != p_sys->stream_info.max_blocksize );
    bool b_fixed_blocksize = ( p_sys->b_stream_info &&
        p_sys->stream_info.min_blocksize == p_sys->stream_info.max_blocksize );

    /* Check syncword */
    if( p_buf[0] != 0xFF || p_buf[1] != 0xF8 ) return 0;

    /* Check there is no emulated sync code in the rest of the header */
    if( p_buf[2] == 0xff || p_buf[3] == 0xFF ) return 0;

    /* Find blocksize (framelength) */
    switch( i_temp = p_buf[2] >> 4 )
    {
    case 0:
        if( b_fixed_blocksize )
            i_blocksize = p_sys->stream_info.min_blocksize;
        else return 0; /* We can't do anything with this */
        break;

    case 1:
        i_blocksize = 192;
        break;

    case 2:
    case 3:
    case 4:
    case 5:
        i_blocksize = 576 << (i_temp - 2);
        break;

    case 6:
    case 7:
        i_blocksize_hint = i_temp;
        break;

    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
        i_blocksize = 256 << (i_temp - 8);
        break;
    }

    /* Find samplerate */
    switch( i_temp = p_buf[2] & 0x0f )
    {
    case 0:
        if( p_sys->b_stream_info )
            *pi_sample_rate = p_sys->stream_info.sample_rate;
        else return 0; /* We can't do anything with this */
        break;

    case 1:
    case 2:
    case 3:
        return 0;
        break;

    case 4:
        *pi_sample_rate = 8000;
        break;

    case 5:
        *pi_sample_rate = 16000;
        break;

    case 6:
        *pi_sample_rate = 22050;
        break;

    case 7:
        *pi_sample_rate = 24000;
        break;

    case 8:
        *pi_sample_rate = 32000;
        break;

    case 9:
        *pi_sample_rate = 44100;
        break;

    case 10:
        *pi_sample_rate = 48000;
        break;

    case 11:
        *pi_sample_rate = 96000;
        break;

    case 12:
    case 13:
    case 14:
        i_sample_rate_hint = i_temp;
        break;

    case 15:
        return 0;
    }

    /* Find channels */
    i_temp = (unsigned)(p_buf[3] >> 4);
    if( i_temp & 8 )
    {
#ifdef USE_LIBFLAC
        int i_channel_assignment; /* ??? */

        switch( i_temp & 7 )
        {
        case 0:
            i_channel_assignment = FLAC__CHANNEL_ASSIGNMENT_LEFT_SIDE;
            break;
        case 1:
            i_channel_assignment = FLAC__CHANNEL_ASSIGNMENT_RIGHT_SIDE;
            break;
        case 2:
            i_channel_assignment = FLAC__CHANNEL_ASSIGNMENT_MID_SIDE;
            break;
        default:
            return 0;
            break;
        }
#endif

        *pi_channels = 2;
    }
    else
    {
        *pi_channels = i_temp + 1;
        *pi_channels_conf = pi_channels_maps[ *pi_channels ];
    }

    /* Find bits per sample */
    switch( i_temp = (unsigned)(p_buf[3] & 0x0e) >> 1 )
    {
    case 0:
        if( p_sys->b_stream_info )
            *pi_bits_per_sample = p_sys->stream_info.bits_per_sample;
        else
            return 0;
        break;

    case 1:
        *pi_bits_per_sample = 8;
        break;

    case 2:
        *pi_bits_per_sample = 12;
        break;

    case 4:
        *pi_bits_per_sample = 16;
        break;

    case 5:
        *pi_bits_per_sample = 20;
        break;

    case 6:
        *pi_bits_per_sample = 24;
        break;

    case 3:
    case 7:
        return 0;
        break;
    }

    /* Zero padding bit */
    if( p_buf[3] & 0x01 ) return 0;

    /* End of fixed size header */
    i_header = 4;

    /* Find Sample/Frame number */
    if( i_blocksize_hint && b_variable_blocksize )
    {
        i_sample_number = read_utf8( &p_buf[i_header++], &i_read );
        if( i_sample_number == INT64_C(0xffffffffffffffff) ) return 0;
    }
    else
    {
        i_sample_number = read_utf8( &p_buf[i_header++], &i_read );
        if( i_sample_number == INT64_C(0xffffffffffffffff) ) return 0;

        if( p_sys->b_stream_info )
            i_sample_number *= p_sys->stream_info.min_blocksize;
    }

    i_header += i_read;

    /* Read blocksize */
    if( i_blocksize_hint )
    {
        int i_val1 = p_buf[i_header++];
        if( i_blocksize_hint == 7 )
        {
            int i_val2 = p_buf[i_header++];
            i_val1 = (i_val1 << 8) | i_val2;
        }
        i_blocksize = i_val1 + 1;
    }

    /* Read sample rate */
    if( i_sample_rate_hint )
    {
        int i_val1 = p_buf[i_header++];
        if( i_sample_rate_hint != 12 )
        {
            int i_val2 = p_buf[i_header++];
            i_val1 = (i_val1 << 8) | i_val2;
        }
        if( i_sample_rate_hint == 12 ) *pi_sample_rate = i_val1 * 1000;
        else if( i_sample_rate_hint == 13 ) *pi_sample_rate = i_val1;
        else *pi_sample_rate = i_val1 * 10;
    }

    /* Check the CRC-8 byte */
    if( flac_crc8( p_buf, i_header ) != p_buf[i_header] )
    {
        return 0;
    }

    /* Sanity check using stream info header when possible */
    if( p_sys->b_stream_info )
    {
        if( i_blocksize < p_sys->stream_info.min_blocksize ||
            i_blocksize > p_sys->stream_info.max_blocksize )
            return 0;
    }
    return i_blocksize;
}

/* Will return 0xffffffffffffffff for an invalid utf-8 sequence */
static uint64_t read_utf8( const uint8_t *p_buf, int *pi_read )
{
    uint64_t i_result = 0;
    unsigned i, j;

    if( !(p_buf[0] & 0x80) ) /* 0xxxxxxx */
    {
        i_result = p_buf[0];
        i = 0;
    }
    else if( p_buf[0] & 0xC0 && !(p_buf[0] & 0x20) ) /* 110xxxxx */
    {
        i_result = p_buf[0] & 0x1F;
        i = 1;
    }
    else if( p_buf[0] & 0xE0 && !(p_buf[0] & 0x10) ) /* 1110xxxx */
    {
        i_result = p_buf[0] & 0x0F;
        i = 2;
    }
    else if( p_buf[0] & 0xF0 && !(p_buf[0] & 0x08) ) /* 11110xxx */
    {
        i_result = p_buf[0] & 0x07;
        i = 3;
    }
    else if( p_buf[0] & 0xF8 && !(p_buf[0] & 0x04) ) /* 111110xx */
    {
        i_result = p_buf[0] & 0x03;
        i = 4;
    }
    else if( p_buf[0] & 0xFC && !(p_buf[0] & 0x02) ) /* 1111110x */
    {
        i_result = p_buf[0] & 0x01;
        i = 5;
    }
    else if( p_buf[0] & 0xFE && !(p_buf[0] & 0x01) ) /* 11111110 */
    {
        i_result = 0;
        i = 6;
    }
    else {
        return INT64_C(0xffffffffffffffff);
    }

    for( j = 1; j <= i; j++ )
    {
        if( !(p_buf[j] & 0x80) || (p_buf[j] & 0x40) ) /* 10xxxxxx */
        {
            return INT64_C(0xffffffffffffffff);
        }
        i_result <<= 6;
        i_result |= (p_buf[j] & 0x3F);
    }

    *pi_read = i;
    return i_result;
}

/* CRC-8, poly = x^8 + x^2 + x^1 + x^0, init = 0 */
static const uint8_t flac_crc8_table[256] = {
        0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
        0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
        0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
        0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
        0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
        0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
        0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
        0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
        0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
        0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
        0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
        0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
        0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
        0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
        0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
        0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
        0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
        0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
        0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
        0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
        0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
        0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
        0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
        0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
        0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
        0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
        0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
        0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
        0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
        0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
        0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
        0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

static uint8_t flac_crc8( const uint8_t *data, unsigned len )
{
    uint8_t crc = 0;

    while(len--)
        crc = flac_crc8_table[crc ^ *data++];

    return crc;
}

#ifdef USE_LIBFLAC
/*****************************************************************************
 * encoder_sys_t : flac encoder descriptor
 *****************************************************************************/
struct encoder_sys_t
{
    /*
     * Input properties
     */
    int i_headers;

    int i_samples_delay;
    int i_channels;

    FLAC__int32 *p_buffer;
    unsigned int i_buffer;

    block_t *p_chain;

    /*
     * FLAC properties
     */
    FLAC__StreamEncoder *p_flac;
    FLAC__StreamMetadata_StreamInfo stream_info;

    /*
     * Common properties
     */
    mtime_t i_pts;
};

#define STREAMINFO_SIZE 38

static block_t *Encode( encoder_t *, aout_buffer_t * );

static FLAC__StreamEncoderWriteStatus
EncoderWriteCallback( const FLAC__StreamEncoder *encoder,
                      const FLAC__byte buffer[],
                      unsigned bytes, unsigned samples,
                      unsigned current_frame, void *client_data );

static void EncoderMetadataCallback( const FLAC__StreamEncoder *encoder,
                                     const FLAC__StreamMetadata *metadata,
                                     void *client_data );

/*****************************************************************************
 * OpenEncoder: probe the encoder and return score
 *****************************************************************************/
static int OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;

    if( p_enc->fmt_out.i_codec != VLC_FOURCC('f','l','a','c') &&
        !p_enc->b_force )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_sys = (encoder_sys_t *)malloc(sizeof(encoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;
    p_enc->p_sys = p_sys;
    p_enc->pf_encode_audio = Encode;
    p_enc->fmt_out.i_codec = VLC_FOURCC('f','l','a','c');

    p_sys->i_headers = 0;
    p_sys->p_buffer = 0;
    p_sys->i_buffer = 0;
    p_sys->i_samples_delay = 0;

    /* Create flac encoder */
    if( !(p_sys->p_flac = FLAC__stream_encoder_new()) )
    {
        msg_Err( p_enc, "FLAC__stream_encoder_new() failed" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    FLAC__stream_encoder_set_streamable_subset( p_sys->p_flac, 1 );
    FLAC__stream_encoder_set_channels( p_sys->p_flac,
                                       p_enc->fmt_in.audio.i_channels );
    FLAC__stream_encoder_set_sample_rate( p_sys->p_flac,
                                          p_enc->fmt_in.audio.i_rate );
    FLAC__stream_encoder_set_bits_per_sample( p_sys->p_flac, 16 );
    p_enc->fmt_in.i_codec = AOUT_FMT_S16_NE;

    /* Get and store the STREAMINFO metadata block as a p_extra */
    p_sys->p_chain = 0;

#ifdef USE_NEW_FLAC_API
    if( FLAC__stream_encoder_init_stream( p_sys->p_flac,
                                          EncoderWriteCallback,
                                          NULL,
                                          NULL,
                                          EncoderMetadataCallback,
                                          p_enc )
        != FLAC__STREAM_ENCODER_INIT_STATUS_OK )
    {
        msg_Err( p_enc, "FLAC__stream_encoder_init_stream() failed" );
        FLAC__stream_encoder_delete( p_sys->p_flac );
        free( p_sys );
        return VLC_EGENERIC;
    }
#else
    FLAC__stream_encoder_set_write_callback( p_sys->p_flac,
        EncoderWriteCallback );
    FLAC__stream_encoder_set_metadata_callback( p_sys->p_flac,
        EncoderMetadataCallback );
    FLAC__stream_encoder_set_client_data( p_sys->p_flac, p_enc );

    FLAC__stream_encoder_init( p_sys->p_flac );
#endif

    return VLC_SUCCESS;
}

/****************************************************************************
 * Encode: the whole thing
 ****************************************************************************
 * This function spits out ogg packets.
 ****************************************************************************/
static block_t *Encode( encoder_t *p_enc, aout_buffer_t *p_aout_buf )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    block_t *p_chain;
    unsigned int i;

    p_sys->i_pts = p_aout_buf->start_date -
                (mtime_t)1000000 * (mtime_t)p_sys->i_samples_delay /
                (mtime_t)p_enc->fmt_in.audio.i_rate;

    p_sys->i_samples_delay += p_aout_buf->i_nb_samples;

    /* Convert samples to FLAC__int32 */
    if( p_sys->i_buffer < p_aout_buf->i_nb_bytes * 2 )
    {
        p_sys->p_buffer =
            realloc( p_sys->p_buffer, p_aout_buf->i_nb_bytes * 2 );
        p_sys->i_buffer = p_aout_buf->i_nb_bytes * 2;
    }

    for( i = 0 ; i < p_aout_buf->i_nb_bytes / 2 ; i++ )
    {
        p_sys->p_buffer[i]= ((int16_t *)p_aout_buf->p_buffer)[i];
    }

    FLAC__stream_encoder_process_interleaved( p_sys->p_flac, p_sys->p_buffer,
                                              p_aout_buf->i_nb_samples );

    p_chain = p_sys->p_chain;
    p_sys->p_chain = 0;

    return p_chain;
}

/*****************************************************************************
 * CloseEncoder: encoder destruction
 *****************************************************************************/
static void CloseEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    FLAC__stream_encoder_delete( p_sys->p_flac );

    free( p_sys->p_buffer );
    free( p_sys );
}

/*****************************************************************************
 * EncoderMetadataCallback: called by libflac to output metadata
 *****************************************************************************/
static void EncoderMetadataCallback( const FLAC__StreamEncoder *encoder,
                                     const FLAC__StreamMetadata *metadata,
                                     void *client_data )
{
    VLC_UNUSED(encoder);
    encoder_t *p_enc = (encoder_t *)client_data;

    msg_Err( p_enc, "MetadataCallback: %i", metadata->type );
    return;
}

/*****************************************************************************
 * EncoderWriteCallback: called by libflac to output encoded samples
 *****************************************************************************/
static FLAC__StreamEncoderWriteStatus
EncoderWriteCallback( const FLAC__StreamEncoder *encoder,
                      const FLAC__byte buffer[],
                      unsigned bytes, unsigned samples,
                      unsigned current_frame, void *client_data )
{
    VLC_UNUSED(encoder); VLC_UNUSED(current_frame);
    encoder_t *p_enc = (encoder_t *)client_data;
    encoder_sys_t *p_sys = p_enc->p_sys;
    block_t *p_block;

    if( samples == 0 )
    {
        if( p_sys->i_headers == 1 )
        {
            msg_Dbg( p_enc, "Writing STREAMINFO: %i", bytes );

            /* Backup the STREAMINFO metadata block */
            p_enc->fmt_out.i_extra = STREAMINFO_SIZE + 4;
            p_enc->fmt_out.p_extra = malloc( STREAMINFO_SIZE + 4 );
            memcpy( p_enc->fmt_out.p_extra, "fLaC", 4 );
            memcpy( ((uint8_t *)p_enc->fmt_out.p_extra) + 4, buffer,
                    STREAMINFO_SIZE );

            /* Fake this as the last metadata block */
            ((uint8_t*)p_enc->fmt_out.p_extra)[4] |= 0x80;
        }
        p_sys->i_headers++;
        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
    }

    p_block = block_New( p_enc, bytes );
    memcpy( p_block->p_buffer, buffer, bytes );

    p_block->i_dts = p_block->i_pts = p_sys->i_pts;

    p_sys->i_samples_delay -= samples;

    p_block->i_length = (mtime_t)1000000 *
        (mtime_t)samples / (mtime_t)p_enc->fmt_in.audio.i_rate;

    /* Update pts */
    p_sys->i_pts += p_block->i_length;

    block_ChainAppend( &p_sys->p_chain, p_block );

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}
#endif
