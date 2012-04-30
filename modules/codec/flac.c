/*****************************************************************************
 * flac.c: flac decoder/encoder module making use of libflac
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

/* workaround libflac overriding assert.h system header */
#define assert(x) do {} while(0)

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>

#include <stream_decoder.h>
#include <stream_encoder.h>

#include <vlc_block_helper.h>
#include <vlc_bits.h>

#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT >= 8
#   define USE_NEW_FLAC_API
#endif

/*****************************************************************************
 * decoder_sys_t : FLAC decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /*
     * Input/Output properties
     */
    block_t *p_block;
    aout_buffer_t *p_aout_buffer;
    date_t        end_date;

    /*
     * FLAC properties
     */
    FLAC__StreamDecoder *p_flac;
    FLAC__StreamMetadata_StreamInfo stream_info;
    bool b_stream_info;
};

static const int pi_channels_maps[9] =
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
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT
     | AOUT_CHAN_MIDDLERIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT
     | AOUT_CHAN_LFE
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

static int OpenEncoder   ( vlc_object_t * );
static void CloseEncoder ( vlc_object_t * );

static aout_buffer_t *DecodeBlock( decoder_t *, block_t ** );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()

    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )
    add_shortcut( "flac" )

    set_description( N_("Flac audio decoder") )
    set_capability( "decoder", 100 )
    set_callbacks( OpenDecoder, CloseDecoder )

    add_submodule ()
    add_shortcut( "flac" )
    set_description( N_("Flac audio encoder") )
    set_capability( "encoder", 100 )
    set_callbacks( OpenEncoder, CloseEncoder )

vlc_module_end ()

/*****************************************************************************
 * Interleave: helper function to interleave channels
 *****************************************************************************/
static void Interleave32( int32_t *p_out, const int32_t * const *pp_in,
                          const int pi_index[],
                          int i_nb_channels, int i_samples )
{
    int i, j;
    for ( j = 0; j < i_samples; j++ )
    {
        for ( i = 0; i < i_nb_channels; i++ )
        {
            p_out[j * i_nb_channels + i] = pp_in[pi_index[i]][j];
        }
    }
}

static void Interleave24( int8_t *p_out, const int32_t * const *pp_in,
                          const int pi_index[],
                          int i_nb_channels, int i_samples )
{
    int i, j;
    for ( j = 0; j < i_samples; j++ )
    {
        for ( i = 0; i < i_nb_channels; i++ )
        {
            const int i_index = pi_index[i];
#ifdef WORDS_BIGENDIAN
            p_out[3*(j * i_nb_channels + i)+0] = (pp_in[i_index][j] >> 16) & 0xff;
            p_out[3*(j * i_nb_channels + i)+1] = (pp_in[i_index][j] >> 8 ) & 0xff;
            p_out[3*(j * i_nb_channels + i)+2] = (pp_in[i_index][j] >> 0 ) & 0xff;
#else
            p_out[3*(j * i_nb_channels + i)+2] = (pp_in[i_index][j] >> 16) & 0xff;
            p_out[3*(j * i_nb_channels + i)+1] = (pp_in[i_index][j] >> 8 ) & 0xff;
            p_out[3*(j * i_nb_channels + i)+0] = (pp_in[i_index][j] >> 0 ) & 0xff;
#endif
        }
    }
}

static void Interleave16( int16_t *p_out, const int32_t * const *pp_in,
                          const int pi_index[],
                          int i_nb_channels, int i_samples )
{
    int i, j;
    for ( j = 0; j < i_samples; j++ )
    {
        for ( i = 0; i < i_nb_channels; i++ )
        {
            p_out[j * i_nb_channels + i] = (int32_t)(pp_in[pi_index[i]][j]);
        }
    }
}

/*****************************************************************************
 * DecoderWriteCallback: called by libflac to output decoded samples
 *****************************************************************************/
static FLAC__StreamDecoderWriteStatus
DecoderWriteCallback( const FLAC__StreamDecoder *decoder,
                      const FLAC__Frame *frame,
                      const FLAC__int32 *const buffer[], void *client_data )
{
    /* XXX it supposes our internal format is WG4 */
    static const int ppi_reorder[1+8][8] = {
        {-1},
        { 0, },
        { 0, 1 },
        { 0, 1, 2 },
        { 0, 1, 2, 3 },
        { 0, 1, 3, 4, 2 },
        { 0, 1, 4, 5, 2, 3 },

        { 0, 1, 6, 4, 5, 2, 3 },    /* 7.0 Unspecified by flac, but following SMPTE */
        { 0, 1, 6, 7, 4, 5, 2, 3 }, /* 7.1 Unspecified by flac, but following SMPTE */
    };

    VLC_UNUSED(decoder);
    decoder_t *p_dec = (decoder_t *)client_data;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_dec->fmt_out.audio.i_channels <= 0 ||
        p_dec->fmt_out.audio.i_channels > 8 )
        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
    if( date_Get( &p_sys->end_date ) <= VLC_TS_INVALID )
        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;

    const int * const pi_reorder = ppi_reorder[p_dec->fmt_out.audio.i_channels];

    p_sys->p_aout_buffer =
        decoder_NewAudioBuffer( p_dec, frame->header.blocksize );

    if( p_sys->p_aout_buffer == NULL )
        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;

    switch( frame->header.bits_per_sample )
    {
    case 16:
        Interleave16( (int16_t *)p_sys->p_aout_buffer->p_buffer, buffer, pi_reorder,
                      frame->header.channels, frame->header.blocksize );
        break;
    case 24:
        Interleave24( (int8_t *)p_sys->p_aout_buffer->p_buffer, buffer, pi_reorder,
                      frame->header.channels, frame->header.blocksize );
        break;
    default:
        Interleave32( (int32_t *)p_sys->p_aout_buffer->p_buffer, buffer, pi_reorder,
                      frame->header.channels, frame->header.blocksize );
    }

    /* Date management (already done by packetizer) */
    p_sys->p_aout_buffer->i_pts = date_Get( &p_sys->end_date );
    p_sys->p_aout_buffer->i_length =
        date_Increment( &p_sys->end_date, frame->header.blocksize ) -
        p_sys->p_aout_buffer->i_pts;

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

/*****************************************************************************
 * DecoderReadCallback: called by libflac when it needs more data
 *****************************************************************************/
static FLAC__StreamDecoderReadStatus
DecoderReadCallback( const FLAC__StreamDecoder *decoder, FLAC__byte buffer[],
                     size_t *bytes, void *client_data )
{
    VLC_UNUSED(decoder);
    decoder_t *p_dec = (decoder_t *)client_data;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_block && p_sys->p_block->i_buffer )
    {
        *bytes = __MIN(*bytes, p_sys->p_block->i_buffer);
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
            p_dec->fmt_out.i_codec = VLC_CODEC_S8;
            break;
        case 16:
            p_dec->fmt_out.i_codec = VLC_CODEC_S16N;
            break;
        case 24:
            p_dec->fmt_out.i_codec = VLC_CODEC_S24N;
            break;
        default:
            msg_Dbg( p_dec, "strange bit/sample value: %d",
                     metadata->data.stream_info.bits_per_sample );
            p_dec->fmt_out.i_codec = VLC_CODEC_FI32;
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

    msg_Dbg( p_dec, "channels:%d samplerate:%d bitspersamples:%d",
             p_dec->fmt_out.audio.i_channels, p_dec->fmt_out.audio.i_rate,
             p_dec->fmt_out.audio.i_bitspersample );

    p_sys->b_stream_info = true;
    p_sys->stream_info = metadata->data.stream_info;

    date_Init( &p_sys->end_date, p_dec->fmt_out.audio.i_rate, 1 );
    date_Set( &p_sys->end_date, VLC_TS_INVALID );
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
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_FLAC )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = malloc(sizeof(*p_sys)) ) == NULL )
        return VLC_ENOMEM;

    /* Misc init */
    p_sys->b_stream_info = false;
    p_sys->p_block = NULL;

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

    /* Set output properties */
    p_dec->fmt_out.i_cat = AUDIO_ES;
    p_dec->fmt_out.i_codec = VLC_CODEC_FL32;

    /* Set callbacks */
    p_dec->pf_decode_audio = DecodeBlock;

    /* */
    p_dec->b_need_packetized = true;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseDecoder: flac decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    FLAC__stream_decoder_finish( p_sys->p_flac );
    FLAC__stream_decoder_delete( p_sys->p_flac );

    if( p_sys->p_block )
        block_Release( p_sys->p_block );
    free( p_sys );
}

/*****************************************************************************
 * ProcessHeader: process Flac header.
 *****************************************************************************/
static void ProcessHeader( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( !p_dec->fmt_in.i_extra )
        return;

    /* Decode STREAMINFO */
    msg_Dbg( p_dec, "decode STREAMINFO" );
    p_sys->p_block = block_New( p_dec, p_dec->fmt_in.i_extra );
    memcpy( p_sys->p_block->p_buffer, p_dec->fmt_in.p_extra,
            p_dec->fmt_in.i_extra );
    FLAC__stream_decoder_process_until_end_of_metadata( p_sys->p_flac );
    msg_Dbg( p_dec, "STREAMINFO decoded" );
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

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************/
static aout_buffer_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( !pp_block || !*pp_block )
        return NULL;
    if( (*pp_block)->i_flags&(BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
        block_Release( *pp_block );
        return NULL;
    }

    if( !p_sys->b_stream_info )
        ProcessHeader( p_dec );

    p_sys->p_block = *pp_block;
    *pp_block = NULL;

    if( p_sys->p_block->i_pts > VLC_TS_INVALID &&
        p_sys->p_block->i_pts != date_Get( &p_sys->end_date ) )
        date_Set( &p_sys->end_date, p_sys->p_block->i_pts );

    p_sys->p_aout_buffer = 0;

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

    return p_sys->p_aout_buffer;
}

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

/*****************************************************************************
 * EncoderWriteCallback: called by libflac to output encoded samples
 *****************************************************************************/
static FLAC__StreamEncoderWriteStatus
EncoderWriteCallback( const FLAC__StreamEncoder *encoder,
                      const FLAC__byte buffer[],
                      size_t bytes, unsigned samples,
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
            msg_Dbg( p_enc, "Writing STREAMINFO: %zu", bytes );

            /* Backup the STREAMINFO metadata block */
            p_enc->fmt_out.i_extra = STREAMINFO_SIZE + 4;
            p_enc->fmt_out.p_extra = xmalloc( STREAMINFO_SIZE + 4 );
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
 * OpenEncoder: probe the encoder and return score
 *****************************************************************************/
static int OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;

    if( p_enc->fmt_out.i_codec != VLC_CODEC_FLAC &&
        !p_enc->b_force )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_sys = (encoder_sys_t *)malloc(sizeof(encoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;
    p_enc->p_sys = p_sys;
    p_enc->pf_encode_audio = Encode;
    p_enc->fmt_out.i_codec = VLC_CODEC_FLAC;

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
    p_enc->fmt_in.i_codec = VLC_CODEC_S16N;

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

    p_sys->i_pts = p_aout_buf->i_pts -
                (mtime_t)1000000 * (mtime_t)p_sys->i_samples_delay /
                (mtime_t)p_enc->fmt_in.audio.i_rate;

    p_sys->i_samples_delay += p_aout_buf->i_nb_samples;

    /* Convert samples to FLAC__int32 */
    if( p_sys->i_buffer < p_aout_buf->i_buffer * 2 )
    {
        p_sys->p_buffer =
            xrealloc( p_sys->p_buffer, p_aout_buf->i_buffer * 2 );
        p_sys->i_buffer = p_aout_buf->i_buffer * 2;
    }

    for( i = 0 ; i < p_aout_buf->i_buffer / 2 ; i++ )
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
