/*****************************************************************************
 * speex.c: speex decoder/packetizer/encoder module making use of libspeex.
 *****************************************************************************
 * Copyright (C) 2003-2009 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input_item.h>
#include <vlc_codec.h>
#include "../demux/xiph.h"

#include <ogg/ogg.h>
#include <speex/speex.h>
#include <speex/speex_header.h>
#include <speex/speex_stereo.h>
#include <speex/speex_callbacks.h>

#include <assert.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static int  OpenPacketizer( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

#ifdef ENABLE_SOUT
static int OpenEncoder   ( vlc_object_t * );
static void CloseEncoder ( vlc_object_t * );
#endif

#define ENC_CFG_PREFIX "sout-speex-"

#define ENC_MODE_TEXT N_("Mode" )
#define ENC_MODE_LONGTEXT N_( \
    "Enforce the mode of the encoder." )

#define ENC_QUALITY_TEXT N_("Encoding quality")
#define ENC_QUALITY_LONGTEXT N_( \
    "Enforce a quality between 0 (low) and 10 (high)." )

#define ENC_COMPLEXITY_TEXT N_("Encoding complexity" )
#define ENC_COMPLEXITY_LONGTEXT N_( \
    "Enforce the complexity of the encoder." )

#define ENC_MAXBITRATE_TEXT N_( "Maximal bitrate" )
#define ENC_MAXBITRATE_LONGTEXT N_( \
    "Enforce the maximal VBR bitrate" )

#define ENC_CBR_TEXT N_( "CBR encoding" )
#define ENC_CBR_LONGTEXT N_( \
    "Enforce a constant bitrate encoding (CBR) instead of default " \
    "variable bitrate encoding (VBR)." )

#define ENC_VAD_TEXT N_( "Voice activity detection" )
#define ENC_VAD_LONGTEXT N_( \
    "Enable voice activity detection (VAD). It is automatically " \
    "activated in VBR mode." )

#define ENC_DTX_TEXT N_( "Discontinuous Transmission" )
#define ENC_DTX_LONGTEXT N_( \
    "Enable discontinuous transmission (DTX)." )

static const int pi_enc_mode_values[] = { 0, 1, 2 };
static const char * const ppsz_enc_mode_descriptions[] = {
    N_("Narrow-band (8kHz)"), N_("Wide-band (16kHz)"), N_("Ultra-wideband (32kHz)"), NULL
};

vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )

    set_description( N_("Speex audio decoder") )
    set_capability( "audio decoder", 100 )
    set_shortname( N_("Speex") )
    set_callbacks( OpenDecoder, CloseDecoder )

    add_submodule ()
    set_description( N_("Speex audio packetizer") )
    set_capability( "packetizer", 100 )
    set_callbacks( OpenPacketizer, CloseDecoder )

#ifdef ENABLE_SOUT
    add_submodule ()
    set_description( N_("Speex audio encoder") )
    set_capability( "encoder", 100 )
    set_callbacks( OpenEncoder, CloseEncoder )

    add_integer( ENC_CFG_PREFIX "mode", 0, ENC_MODE_TEXT,
                 ENC_MODE_LONGTEXT, false )
        change_integer_list( pi_enc_mode_values, ppsz_enc_mode_descriptions )

    add_integer( ENC_CFG_PREFIX "complexity", 3, ENC_COMPLEXITY_TEXT,
                 ENC_COMPLEXITY_LONGTEXT, false )
        change_integer_range( 1, 10 )

    add_bool( ENC_CFG_PREFIX "cbr", false, ENC_CBR_TEXT,
                 ENC_CBR_LONGTEXT, false )

    add_float( ENC_CFG_PREFIX "quality", 8.0, ENC_QUALITY_TEXT,
               ENC_QUALITY_LONGTEXT, false )
        change_float_range( 0.0, 10.0 )

    add_integer( ENC_CFG_PREFIX "max-bitrate", 0, ENC_MAXBITRATE_TEXT,
                 ENC_MAXBITRATE_LONGTEXT, false )

    add_bool( ENC_CFG_PREFIX "vad", true, ENC_VAD_TEXT,
                 ENC_VAD_LONGTEXT, false )

    add_bool( ENC_CFG_PREFIX "dtx", false, ENC_DTX_TEXT,
                 ENC_DTX_LONGTEXT, false )

    /* TODO agc, noise suppression, */
#endif

vlc_module_end ()

static const char *const ppsz_enc_options[] = {
    "mode", "complexity", "cbr", "quality", "max-bitrate", "vad", "dtx", NULL
};

/*****************************************************************************
 * decoder_sys_t : speex decoder descriptor
 *****************************************************************************/
typedef struct
{
    /* Module mode */
    bool b_packetizer;

    /*
     * Input properties
     */
    bool b_has_headers;
    int i_frame_in_packet;

    /*
     * Speex properties
     */
    SpeexBits bits;
    SpeexHeader *p_header;
    SpeexStereoState stereo;
    void *p_state;
    unsigned int rtp_rate;

    /*
     * Common properties
     */
    date_t end_date;

} decoder_sys_t;

/****************************************************************************
 * Local prototypes
 ****************************************************************************/

static block_t *Packetize  ( decoder_t *, block_t ** );
static int      DecodeAudio  ( decoder_t *, block_t * );
static int      DecodeRtpSpeexPacket( decoder_t *, block_t *);
static int      ProcessHeaders( decoder_t * );
static int      ProcessInitialHeader ( decoder_t *, ogg_packet * );
static block_t *ProcessPacket( decoder_t *, ogg_packet *, block_t ** );
static void Flush( decoder_t * );

static block_t *DecodePacket( decoder_t *, ogg_packet * );
static block_t *SendPacket( decoder_t *, block_t * );

static void ParseSpeexComments( decoder_t *, ogg_packet * );

static int OpenCommon( vlc_object_t *p_this, bool b_packetizer )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_SPEEX )
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = malloc(sizeof(decoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;
    p_sys->bits.buf_size = 0;
    p_sys->b_packetizer = b_packetizer;
    p_sys->rtp_rate = p_dec->fmt_in.audio.i_rate;
    p_sys->b_has_headers = false;

    date_Set( &p_sys->end_date, VLC_TICK_INVALID );

    if( b_packetizer )
    {
        p_dec->fmt_out.i_codec = VLC_CODEC_SPEEX;
        p_dec->pf_packetize    = Packetize;
    }
    else
    {
        /* Set output properties */
        p_dec->fmt_out.i_codec = VLC_CODEC_S16N;

        /*
          Set callbacks
          If the codec is spxr then this decoder is
          being invoked on a Speex stream arriving via RTP.
          A special decoder callback is used.
        */
        if (p_dec->fmt_in.i_original_fourcc == VLC_FOURCC('s', 'p', 'x', 'r'))
        {
            msg_Dbg( p_dec, "Using RTP version of Speex decoder @ rate %d.",
            p_dec->fmt_in.audio.i_rate );
            p_dec->pf_decode = DecodeRtpSpeexPacket;
        }
        else
        {
            p_dec->pf_decode = DecodeAudio;
        }
    }
    p_dec->pf_flush        = Flush;

    p_sys->p_state = NULL;
    p_sys->p_header = NULL;
    p_sys->i_frame_in_packet = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    return OpenCommon( p_this, false );
}

static int OpenPacketizer( vlc_object_t *p_this )
{
    return OpenCommon( p_this, true );
}

static int CreateDefaultHeader( decoder_t *p_dec )
{
    ogg_packet oggpacket;
    SpeexHeader *p_header = malloc( sizeof(SpeexHeader) );
    if( !p_header )
        return VLC_ENOMEM;

    const int rate = p_dec->fmt_in.audio.i_rate;
    const unsigned i_mode = (rate / 8000) >> 1;

    const SpeexMode *mode;
    int ret = VLC_SUCCESS;
    oggpacket.packet = NULL;

    switch( rate )
    {
        case 8000:
        case 16000:
        case 32000:
            mode = speex_lib_get_mode( i_mode );
            break;
        default:
            msg_Err( p_dec, "Unexpected rate %d", rate );
            ret = VLC_EGENERIC;
            goto cleanup;
    }

    speex_init_header( p_header, rate, p_dec->fmt_in.audio.i_channels, mode );
    p_header->frames_per_packet = 160 << i_mode;

    oggpacket.packet = (unsigned char *) speex_header_to_packet( p_header,
            (int *) &oggpacket.bytes );
    if( !oggpacket.packet )
    {
        ret = VLC_ENOMEM;
        goto cleanup;
    }

    oggpacket.b_o_s = 1;
    oggpacket.e_o_s = 0;
    oggpacket.granulepos = -1;
    oggpacket.packetno = 0;

    ret = ProcessInitialHeader( p_dec, &oggpacket );

    if( ret != VLC_SUCCESS )
    {
        msg_Err( p_dec, "default Speex header is corrupted" );
    }

cleanup:
    free( oggpacket.packet );
    free( p_header );

    return ret;
}


/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with ogg packets.
 ****************************************************************************/
static block_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    ogg_packet oggpacket;

    block_t *block = *pp_block;

    if( block != NULL )
    {
        if( block->i_flags & (BLOCK_FLAG_CORRUPTED|BLOCK_FLAG_DISCONTINUITY) )
        {
            Flush( p_dec );
            if( block->i_flags & BLOCK_FLAG_CORRUPTED )
            {
                block_Release( block );
                *pp_block = NULL;
                return NULL;
            }
        }
        /* Block to Ogg packet */
        oggpacket.packet = block->p_buffer;
        oggpacket.bytes = block->i_buffer;
    }
    else
    {
        if( p_sys->b_packetizer ) return NULL;

        /* Block to Ogg packet */
        oggpacket.packet = NULL;
        oggpacket.bytes = 0;
    }

    oggpacket.granulepos = -1;
    oggpacket.b_o_s = 0;
    oggpacket.e_o_s = 0;
    oggpacket.packetno = 0;

    /* Check for headers */
    if( !p_sys->b_has_headers )
    {
        if( !p_dec->fmt_in.p_extra )
        {
            msg_Warn( p_dec, "Header missing, using default settings" );

            if( CreateDefaultHeader( p_dec ) )
            {
                if( block != NULL )
                    block_Release( block );
                return NULL;
            }
        }
        else if( ProcessHeaders( p_dec ) )
        {
            if( block != NULL )
                block_Release( block );
            return NULL;
        }
        p_sys->b_has_headers = true;
    }

    return ProcessPacket( p_dec, &oggpacket, pp_block );
}

static int DecodeAudio( decoder_t *p_dec, block_t *p_block )
{
    if( p_block == NULL ) /* No Drain */
        return VLCDEC_SUCCESS;

    block_t **pp_block = &p_block, *p_out;
    while( ( p_out = DecodeBlock( p_dec, pp_block ) ) != NULL )
        decoder_QueueAudio( p_dec, p_out );
    return VLCDEC_SUCCESS;
}

static block_t *Packetize( decoder_t *p_dec, block_t **pp_block )
{
    if( pp_block == NULL ) /* No Drain */
        return NULL;
    return DecodeBlock( p_dec, pp_block );
}

/*****************************************************************************
 * ProcessHeaders: process Speex headers.
 *****************************************************************************/
static int ProcessHeaders( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    ogg_packet oggpacket;

    unsigned pi_size[XIPH_MAX_HEADER_COUNT];
    const void *pp_data[XIPH_MAX_HEADER_COUNT];
    unsigned i_count;
    if( xiph_SplitHeaders( pi_size, pp_data, &i_count,
                           p_dec->fmt_in.i_extra, p_dec->fmt_in.p_extra) )
        return VLC_EGENERIC;
    if( i_count < 2 )
        return VLC_EGENERIC;;

    oggpacket.granulepos = -1;
    oggpacket.e_o_s = 0;
    oggpacket.packetno = 0;

    /* Take care of the initial Vorbis header */
    oggpacket.b_o_s = 1; /* yes this actually is a b_o_s packet :) */
    oggpacket.bytes  = pi_size[0];
    oggpacket.packet = (void *)pp_data[0];
    if( ProcessInitialHeader( p_dec, &oggpacket ) != VLC_SUCCESS )
    {
        msg_Err( p_dec, "initial Speex header is corrupted" );
        return VLC_EGENERIC;;
    }

    /* The next packet in order is the comments header */
    oggpacket.b_o_s = 0;
    oggpacket.bytes  = pi_size[1];
    oggpacket.packet = (void *)pp_data[1];
    ParseSpeexComments( p_dec, &oggpacket );

    if( p_sys->b_packetizer )
    {
        void* p_extra = realloc( p_dec->fmt_out.p_extra,
                                 p_dec->fmt_in.i_extra );
        if( unlikely( p_extra == NULL ) )
        {
            return VLC_ENOMEM;
        }
        p_dec->fmt_out.p_extra = p_extra;
        p_dec->fmt_out.i_extra = p_dec->fmt_in.i_extra;
        memcpy( p_dec->fmt_out.p_extra,
                p_dec->fmt_in.p_extra, p_dec->fmt_out.i_extra );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ProcessInitialHeader: processes the inital Speex header packet.
 *****************************************************************************/
static int ProcessInitialHeader( decoder_t *p_dec, ogg_packet *p_oggpacket )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    void *p_state;
    SpeexHeader *p_header;
    const SpeexMode *p_mode;
    SpeexCallback callback;

    p_sys->p_header = p_header =
        speex_packet_to_header( (char *)p_oggpacket->packet,
                                p_oggpacket->bytes );
    if( !p_header )
    {
        msg_Err( p_dec, "cannot read Speex header" );
        return VLC_EGENERIC;
    }
    if( p_header->mode >= SPEEX_NB_MODES || p_header->mode < 0 )
    {
        msg_Err( p_dec, "mode number %d does not (yet/any longer) exist in "
                 "this version of libspeex.", p_header->mode );
        return VLC_EGENERIC;
    }

    p_mode = speex_mode_list[p_header->mode];
    if( p_mode == NULL )
        return VLC_EGENERIC;

    if( p_header->speex_version_id > 1 )
    {
        msg_Err( p_dec, "this file was encoded with Speex bit-stream "
                 "version %d which is not supported by this decoder.",
                 p_header->speex_version_id );
        return VLC_EGENERIC;
    }

    if( p_mode->bitstream_version < p_header->mode_bitstream_version )
    {
        msg_Err( p_dec, "file encoded with a newer version of Speex." );
        return VLC_EGENERIC;
    }
    if( p_mode->bitstream_version > p_header->mode_bitstream_version )
    {
        msg_Err( p_dec, "file encoded with an older version of Speex." );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_dec, "Speex %d Hz audio using %s mode %s%s",
             p_header->rate, p_mode->modeName,
             ( p_header->nb_channels == 1 ) ? " (mono" : " (stereo",
             p_header->vbr ? ", VBR)" : ")" );

    /* Take care of speex decoder init */
    speex_bits_init( &p_sys->bits );
    p_sys->p_state = p_state = speex_decoder_init( p_mode );
    if( !p_state )
    {
        msg_Err( p_dec, "decoder initialization failed" );
        return VLC_EGENERIC;
    }

    if( p_header->nb_channels == 2 )
    {
        SpeexStereoState stereo = SPEEX_STEREO_STATE_INIT;
        p_sys->stereo = stereo;
        callback.callback_id = SPEEX_INBAND_STEREO;
        callback.func = speex_std_stereo_request_handler;
        callback.data = &p_sys->stereo;
        speex_decoder_ctl( p_state, SPEEX_SET_HANDLER, &callback );
    }
    if( p_header->nb_channels <= 0 ||
        p_header->nb_channels > 5 )
    {
        msg_Err( p_dec, "invalid number of channels (not between 1 and 5): %i",
                 p_header->nb_channels );
        return VLC_EGENERIC;
    }

    /* Setup the format */
    p_dec->fmt_out.audio.i_physical_channels =
        vlc_chan_maps[p_header->nb_channels];
    p_dec->fmt_out.audio.i_channels = p_header->nb_channels;
    p_dec->fmt_out.audio.i_rate = p_header->rate;

    date_Init( &p_sys->end_date, p_header->rate, 1 );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Flush:
 *****************************************************************************/
static void Flush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    date_Set( &p_sys->end_date, VLC_TICK_INVALID );
}

/*****************************************************************************
 * ProcessPacket: processes a Speex packet.
 *****************************************************************************/
static block_t *ProcessPacket( decoder_t *p_dec, ogg_packet *p_oggpacket,
                               block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block = *pp_block;

    /* Date management */
    if( p_block && p_block->i_pts != VLC_TICK_INVALID &&
        p_block->i_pts != date_Get( &p_sys->end_date ) )
    {
        date_Set( &p_sys->end_date, p_block->i_pts );
    }

    if( date_Get( &p_sys->end_date ) == VLC_TICK_INVALID )
    {
        /* We've just started the stream, wait for the first PTS. */
        if( p_block ) block_Release( p_block );
        return NULL;
    }

    *pp_block = NULL; /* To avoid being fed the same packet again */

    if( p_sys->b_packetizer )
    {
        if ( p_sys->p_header->frames_per_packet > 1 )
        {
            short *p_frame_holder = NULL;
            int i_bits_before = 0, i_bits_after = 0, i_bytes_in_speex_frame = 0,
                i_pcm_output_size = 0, i_bits_in_speex_frame = 0;
            block_t *p_new_block = NULL;

            i_pcm_output_size = p_sys->p_header->frame_size;
            p_frame_holder = (short*)xmalloc( sizeof(short)*i_pcm_output_size );

            speex_bits_read_from( &p_sys->bits, (char*)p_oggpacket->packet,
                p_oggpacket->bytes);
            i_bits_before = speex_bits_remaining( &p_sys->bits );
            speex_decode_int(p_sys->p_state, &p_sys->bits, p_frame_holder);
            i_bits_after = speex_bits_remaining( &p_sys->bits );

            i_bits_in_speex_frame = i_bits_before - i_bits_after;
            i_bytes_in_speex_frame = ( i_bits_in_speex_frame +
                (8 - (i_bits_in_speex_frame % 8)) )
                / 8;

            p_new_block = block_Alloc( i_bytes_in_speex_frame );
            memset( p_new_block->p_buffer, 0xff, i_bytes_in_speex_frame );

            /*
             * Copy the first frame in this packet to a new packet.
             */
            speex_bits_rewind( &p_sys->bits );
            speex_bits_write( &p_sys->bits,
                (char*)p_new_block->p_buffer,
                (int)i_bytes_in_speex_frame );

            /*
             * Move the remaining part of the original packet (subsequent
             * frames, if there are any) into the beginning
             * of the original packet so
             * they are preserved following the realloc.
             * Note: Any bits that
             * remain in the initial packet
             * are "filler" if they do not constitute
             * an entire byte.
             */
            if ( i_bits_after > 7 )
            {
                /* round-down since we rounded-up earlier (to include
             * the speex terminator code.
             */
                i_bytes_in_speex_frame--;
                speex_bits_write( &p_sys->bits,
                    (char*)p_block->p_buffer,
                    p_block->i_buffer - i_bytes_in_speex_frame );
                p_block = block_Realloc( p_block,
                    0,
                    p_block->i_buffer-i_bytes_in_speex_frame );
                *pp_block = p_block;
            }
            else
            {
                speex_bits_reset( &p_sys->bits );
            }

            free( p_frame_holder );
            return SendPacket( p_dec, p_new_block);
        }
        else
        {
                return SendPacket( p_dec, p_block );
        }
    }
    else
    {
        block_t *p_aout_buffer = DecodePacket( p_dec, p_oggpacket );

        if( p_block )
            block_Release( p_block );
        return p_aout_buffer;
    }
}

static int DecodeRtpSpeexPacket( decoder_t *p_dec, block_t *p_speex_bit_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_aout_buffer;
    int i_decode_ret;
    unsigned int i_speex_frame_size;

    if ( !p_speex_bit_block || p_speex_bit_block->i_pts == VLC_TICK_INVALID )
        return VLCDEC_SUCCESS;

    /*
      If the SpeexBits buffer size is 0 (a default value),
      we know that a proper initialization has not yet been done.
    */
    if ( p_sys->bits.buf_size==0 )
    {
        p_sys->p_header = malloc(sizeof(SpeexHeader));
        if ( !p_sys->p_header )
        {
            msg_Err( p_dec, "Could not allocate a Speex header.");
            return VLCDEC_SUCCESS;
        }

        const SpeexMode *mode = speex_lib_get_mode((p_sys->rtp_rate / 8000) >> 1);

        speex_init_header( p_sys->p_header,p_sys->rtp_rate, 1, mode );
        speex_bits_init( &p_sys->bits );
        p_sys->p_state = speex_decoder_init( mode );
        if ( !p_sys->p_state )
        {
            msg_Err( p_dec, "Could not allocate a Speex decoder." );
            free( p_sys->p_header );
            return VLCDEC_SUCCESS;
        }

        /*
          Assume that variable bit rate is enabled. Also assume
          that there is only one frame per packet.
        */
        p_sys->p_header->vbr = 1;
        p_sys->p_header->frames_per_packet = 1;

        p_dec->fmt_out.audio.i_channels = p_sys->p_header->nb_channels;
        p_dec->fmt_out.audio.i_physical_channels =
            vlc_chan_maps[p_sys->p_header->nb_channels];
        p_dec->fmt_out.audio.i_rate = p_sys->p_header->rate;

        if ( speex_mode_query( &speex_nb_mode,
                               SPEEX_MODE_FRAME_SIZE,
                               &i_speex_frame_size ) )
        {
            msg_Err( p_dec, "Could not determine the frame size." );
            speex_decoder_destroy( p_sys->p_state );
            free( p_sys->p_header );
            return VLCDEC_SUCCESS;
        }
        p_dec->fmt_out.audio.i_bytes_per_frame = i_speex_frame_size;

        date_Init(&p_sys->end_date, p_sys->p_header->rate, 1);
    }

    /*
      If the SpeexBits are initialized but there is
      still no header, an error must be thrown.
    */
    if ( !p_sys->p_header )
    {
        msg_Err( p_dec, "There is no valid Speex header found." );
        return VLCDEC_SUCCESS;
    }

    if ( date_Get( &p_sys->end_date ) == VLC_TICK_INVALID )
        date_Set( &p_sys->end_date, p_speex_bit_block->i_dts );

    /*
      Ask for a new audio output buffer and make sure
      we get one.
    */
    if( decoder_UpdateAudioFormat( p_dec ) )
        p_aout_buffer = NULL;
    else
        p_aout_buffer = decoder_NewAudioBuffer( p_dec,
            p_sys->p_header->frame_size );
    if ( !p_aout_buffer || p_aout_buffer->i_buffer == 0 )
    {
        msg_Err(p_dec, "Oops: No new buffer was returned!");
        return VLCDEC_SUCCESS;
    }

    /*
      Read the Speex payload into the SpeexBits buffer.
    */
    speex_bits_read_from( &p_sys->bits,
        (char*)p_speex_bit_block->p_buffer,
        p_speex_bit_block->i_buffer );

    /*
      Decode the input and ensure that no errors
      were encountered.
    */
    i_decode_ret = speex_decode_int( p_sys->p_state, &p_sys->bits,
            (int16_t*)p_aout_buffer->p_buffer );
    if ( i_decode_ret < 0 )
    {
        msg_Err( p_dec, "Decoding failed. Perhaps we have a bad stream?" );
        return VLCDEC_SUCCESS;
    }

    /*
      Handle date management on the audio output buffer.
    */
    p_aout_buffer->i_pts = date_Get( &p_sys->end_date );
    p_aout_buffer->i_length = date_Increment( &p_sys->end_date,
        p_sys->p_header->frame_size ) - p_aout_buffer->i_pts;


    p_sys->i_frame_in_packet++;
    block_Release( p_speex_bit_block );
    decoder_QueueAudio( p_dec, p_aout_buffer );
    return VLCDEC_SUCCESS;
}

/*****************************************************************************
 * DecodePacket: decodes a Speex packet.
 *****************************************************************************/
static block_t *DecodePacket( decoder_t *p_dec, ogg_packet *p_oggpacket )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_oggpacket->bytes )
    {
        /* Copy Ogg packet to Speex bitstream */
        speex_bits_read_from( &p_sys->bits, (char *)p_oggpacket->packet,
                              p_oggpacket->bytes );
        p_sys->i_frame_in_packet = 0;
    }

    /* Decode one frame at a time */
    if( p_sys->i_frame_in_packet < p_sys->p_header->frames_per_packet )
    {
        block_t *p_aout_buffer;
        if( p_sys->p_header->frame_size == 0 )
            return NULL;

        if( decoder_UpdateAudioFormat( p_dec ) )
            return NULL;
        p_aout_buffer =
            decoder_NewAudioBuffer( p_dec, p_sys->p_header->frame_size );
        if( !p_aout_buffer )
        {
            return NULL;
        }

        switch( speex_decode_int( p_sys->p_state, &p_sys->bits,
                                  (int16_t *)p_aout_buffer->p_buffer ) )
        {
            case -2:
                msg_Err( p_dec, "decoding error: corrupted stream?" );
                /* fall through */
            case -1: /* End of stream */
                return NULL;
        }

        if( speex_bits_remaining( &p_sys->bits ) < 0 )
        {
            msg_Err( p_dec, "decoding overflow: corrupted stream?" );
        }

        if( p_sys->p_header->nb_channels == 2 )
            speex_decode_stereo_int( (int16_t *)p_aout_buffer->p_buffer,
                                     p_sys->p_header->frame_size,
                                     &p_sys->stereo );

        /* Date management */
        p_aout_buffer->i_pts = date_Get( &p_sys->end_date );
        p_aout_buffer->i_length =
            date_Increment( &p_sys->end_date, p_sys->p_header->frame_size )
            - p_aout_buffer->i_pts;

        p_sys->i_frame_in_packet++;

        return p_aout_buffer;
    }
    else
    {
        return NULL;
    }
}

/*****************************************************************************
 * SendPacket: send an ogg packet to the stream output.
 *****************************************************************************/
static block_t *SendPacket( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* Date management */
    p_block->i_dts = p_block->i_pts = date_Get( &p_sys->end_date );

    p_block->i_length =
        date_Increment( &p_sys->end_date,
                            p_sys->p_header->frame_size ) -
        p_block->i_pts;

    return p_block;
}

/*****************************************************************************
 * ParseSpeexComments:
 *****************************************************************************/

static void ParseSpeexComments( decoder_t *p_dec, ogg_packet *p_oggpacket )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    const SpeexMode *p_mode;

    assert( p_sys->p_header->mode < SPEEX_NB_MODES );

    p_mode = speex_mode_list[p_sys->p_header->mode];
    assert( p_mode != NULL );

    if( !p_dec->p_description )
    {
        p_dec->p_description = vlc_meta_New();
        if( !p_dec->p_description )
            return;
    }

    /* */
    char *psz_mode;
    if( asprintf( &psz_mode, "%s%s", p_mode->modeName, p_sys->p_header->vbr ? " VBR" : "" ) >= 0 )
    {
        vlc_meta_AddExtra( p_dec->p_description, _("Mode"), psz_mode );
        free( psz_mode );
    }

    /* TODO: finish comments parsing */
    VLC_UNUSED( p_oggpacket );
}

/*****************************************************************************
 * CloseDecoder: speex decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t * p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_state )
    {
        speex_decoder_destroy( p_sys->p_state );
        speex_bits_destroy( &p_sys->bits );
    }

    free( p_sys->p_header );
    free( p_sys );
}

#ifdef ENABLE_SOUT
/*****************************************************************************
 * encoder_sys_t: encoder descriptor
 *****************************************************************************/
#define MAX_FRAME_BYTES 2000

typedef struct
{
    /*
     * Input properties
     */
    char *p_buffer;
    char p_buffer_out[MAX_FRAME_BYTES];

    /*
     * Speex properties
     */
    SpeexBits bits;
    SpeexHeader header;
    SpeexStereoState stereo;
    void *p_state;

    int i_frames_per_packet;
    int i_frames_in_packet;

    int i_frame_length;
    int i_samples_delay;
    int i_frame_size;
} encoder_sys_t;

static block_t *Encode   ( encoder_t *, block_t * );

/*****************************************************************************
 * OpenEncoder: probe the encoder and return score
 *****************************************************************************/
static int OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;
    const SpeexMode *p_speex_mode = &speex_nb_mode;
    int i_tmp, i;
    const char *pp_header[2];
    int pi_header[2];
    uint8_t *p_extra;

    if( p_enc->fmt_out.i_codec != VLC_CODEC_SPEEX &&
        !p_enc->obj.force )
    {
        return VLC_EGENERIC;
    }

    config_ChainParse( p_enc, ENC_CFG_PREFIX, ppsz_enc_options, p_enc->p_cfg );
    switch( var_GetInteger( p_enc, ENC_CFG_PREFIX "mode" ) )
    {
    case 1:
        msg_Dbg( p_enc, "Using wideband" );
        p_speex_mode = &speex_wb_mode;
        break;
    case 2:
        msg_Dbg( p_enc, "Using ultra-wideband" );
        p_speex_mode = &speex_uwb_mode;
        break;
    default:
        msg_Dbg( p_enc, "Using narrowband" );
        p_speex_mode = &speex_nb_mode;
        break;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_sys = (encoder_sys_t *)malloc(sizeof(encoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;
    p_enc->p_sys = p_sys;
    p_enc->pf_encode_audio = Encode;
    p_enc->fmt_in.i_codec = VLC_CODEC_S16N;
    p_enc->fmt_out.i_codec = VLC_CODEC_SPEEX;

    speex_init_header( &p_sys->header, p_enc->fmt_in.audio.i_rate,
                       1, p_speex_mode );

    p_sys->header.frames_per_packet = 1;
    p_sys->header.vbr = var_GetBool( p_enc, ENC_CFG_PREFIX "cbr" ) ? 0 : 1;
    p_sys->header.nb_channels = p_enc->fmt_in.audio.i_channels;

    /* Create a new encoder state in narrowband mode */
    p_sys->p_state = speex_encoder_init( p_speex_mode );

    /* Parameters */
    i_tmp = var_GetInteger( p_enc, ENC_CFG_PREFIX "complexity" );
    speex_encoder_ctl( p_sys->p_state, SPEEX_SET_COMPLEXITY, &i_tmp );

    i_tmp = var_GetBool( p_enc, ENC_CFG_PREFIX "cbr" ) ? 0 : 1;
    speex_encoder_ctl( p_sys->p_state, SPEEX_SET_VBR, &i_tmp );

    if( i_tmp == 0 ) /* CBR */
    {
        i_tmp = var_GetFloat( p_enc, ENC_CFG_PREFIX "quality" );
        speex_encoder_ctl( p_sys->p_state, SPEEX_SET_QUALITY, &i_tmp );

        i_tmp = var_GetBool( p_enc, ENC_CFG_PREFIX "vad" ) ? 1 : 0;
        speex_encoder_ctl( p_sys->p_state, SPEEX_SET_VAD, &i_tmp );
    }
    else
    {
        float f_tmp;

        f_tmp = var_GetFloat( p_enc, ENC_CFG_PREFIX "quality" );
        speex_encoder_ctl( p_sys->p_state, SPEEX_SET_VBR_QUALITY, &f_tmp );

        i_tmp = var_GetInteger( p_enc, ENC_CFG_PREFIX "max-bitrate" );
        if( i_tmp > 0 )
#ifdef SPEEX_SET_VBR_MAX_BITRATE
            speex_encoder_ctl( p_sys->p_state, SPEEX_SET_VBR_MAX_BITRATE, &i_tmp );
#else
            msg_Dbg( p_enc, "max-bitrate cannot be set in this version of libspeex");
#endif
    }

    i_tmp = var_GetBool( p_enc, ENC_CFG_PREFIX "dtx" ) ? 1 : 0;
    speex_encoder_ctl( p_sys->p_state, SPEEX_SET_DTX, &i_tmp );


    /*Initialization of the structure that holds the bits*/
    speex_bits_init( &p_sys->bits );

    p_sys->i_frames_in_packet = 0;
    p_sys->i_samples_delay = 0;

    speex_encoder_ctl( p_sys->p_state, SPEEX_GET_FRAME_SIZE,
                       &p_sys->i_frame_length );

    p_sys->i_frame_size = p_sys->i_frame_length *
        sizeof(int16_t) * p_enc->fmt_in.audio.i_channels;
    p_sys->p_buffer = xmalloc( p_sys->i_frame_size );

    /* Create and store headers */
    pp_header[0] = speex_header_to_packet( &p_sys->header, &pi_header[0] );
    pp_header[1] = "ENCODER=VLC media player";
    pi_header[1] = sizeof("ENCODER=VLC media player");

    p_enc->fmt_out.i_extra = 3 * 2 + pi_header[0] + pi_header[1];
    p_extra = p_enc->fmt_out.p_extra = xmalloc( p_enc->fmt_out.i_extra );
    for( i = 0; i < 2; i++ )
    {
        *(p_extra++) = pi_header[i] >> 8;
        *(p_extra++) = pi_header[i] & 0xFF;
        memcpy( p_extra, pp_header[i], pi_header[i] );
        p_extra += pi_header[i];
    }

    msg_Dbg( p_enc, "encoding: frame size:%d, channels:%d, samplerate:%d",
             p_sys->i_frame_size, p_enc->fmt_in.audio.i_channels,
             p_enc->fmt_in.audio.i_rate );

    return VLC_SUCCESS;
}

/****************************************************************************
 * Encode: the whole thing
 ****************************************************************************
 * This function spits out ogg packets.
 ****************************************************************************/
static block_t *Encode( encoder_t *p_enc, block_t *p_aout_buf )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    block_t *p_block, *p_chain = NULL;

    /* Encoder gets NULL when it's time to flush */
    if( unlikely( !p_aout_buf ) ) return NULL;

    unsigned char *p_buffer = p_aout_buf->p_buffer;
    unsigned i_samples = p_aout_buf->i_nb_samples;
    int i_samples_delay = p_sys->i_samples_delay;

    vlc_tick_t i_pts = p_aout_buf->i_pts -
                vlc_tick_from_samples( p_sys->i_samples_delay,
                            p_enc->fmt_in.audio.i_rate );

    p_sys->i_samples_delay += i_samples;

    while( p_sys->i_samples_delay >= p_sys->i_frame_length )
    {
        int16_t *p_samples;
        int i_out;

        if( i_samples_delay )
        {
            /* Take care of the left-over from last time */
            int i_delay_size = i_samples_delay * 2 *
                                 p_enc->fmt_in.audio.i_channels;
            int i_size = p_sys->i_frame_size - i_delay_size;

            p_samples = (int16_t *)p_sys->p_buffer;
            memcpy( p_sys->p_buffer + i_delay_size, p_buffer, i_size );
            p_buffer -= i_delay_size;
            i_samples += i_samples_delay;
            i_samples_delay = 0;
        }
        else
        {
            p_samples = (int16_t *)p_buffer;
        }

        /* Encode current frame */
        if( p_enc->fmt_in.audio.i_channels == 2 )
            speex_encode_stereo_int( p_samples, p_sys->i_frame_length,
                                     &p_sys->bits );

#if 0
        if( p_sys->preprocess )
            speex_preprocess( p_sys->preprocess, p_samples, NULL );
#endif

        speex_encode_int( p_sys->p_state, p_samples, &p_sys->bits );

        p_buffer += p_sys->i_frame_size;
        p_sys->i_samples_delay -= p_sys->i_frame_length;
        i_samples -= p_sys->i_frame_length;

        p_sys->i_frames_in_packet++;

        if( p_sys->i_frames_in_packet < p_sys->header.frames_per_packet )
            continue;

        p_sys->i_frames_in_packet = 0;

        speex_bits_insert_terminator( &p_sys->bits );
        i_out = speex_bits_write( &p_sys->bits, p_sys->p_buffer_out,
                                  MAX_FRAME_BYTES );
        speex_bits_reset( &p_sys->bits );

        p_block = block_Alloc( i_out );
        memcpy( p_block->p_buffer, p_sys->p_buffer_out, i_out );

        p_block->i_length = vlc_tick_from_samples(
            p_sys->i_frame_length * p_sys->header.frames_per_packet,
            p_enc->fmt_in.audio.i_rate );

        p_block->i_dts = p_block->i_pts = i_pts;

        /* Update pts */
        i_pts += p_block->i_length;
        block_ChainAppend( &p_chain, p_block );

    }

    /* Backup the remaining raw samples */
    if( i_samples )
    {
        memcpy( p_sys->p_buffer + i_samples_delay * 2 *
                p_enc->fmt_in.audio.i_channels, p_buffer,
                i_samples * 2 * p_enc->fmt_in.audio.i_channels );
    }

    return p_chain;
}

/*****************************************************************************
 * CloseEncoder: encoder destruction
 *****************************************************************************/
static void CloseEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    speex_encoder_destroy( p_sys->p_state );
    speex_bits_destroy( &p_sys->bits );

    free( p_sys->p_buffer );
    free( p_sys );
}
#endif
