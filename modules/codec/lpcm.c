/*****************************************************************************
 * lpcm.c: lpcm decoder/packetizer module
 *****************************************************************************
 * Copyright (C) 1999-2008 VLC authors and VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Henri Fallon <henri@videolan.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Lauren Aimar <fenrir _AT_ videolan _DOT_ org >
 *          Steinar H. Gunderson <steinar+vlc@gunderson.no>
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
#include <vlc_codec.h>
#include <vlc_aout.h>
#include <unistd.h>
#include <assert.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static int  OpenPacketizer( vlc_object_t * );
static void CloseCommon   ( vlc_object_t * );

#ifdef ENABLE_SOUT
static int  OpenEncoder   ( vlc_object_t * );
static void CloseEncoder  ( vlc_object_t * );
static block_t *EncodeFrames( encoder_t *, block_t * );
#endif

vlc_module_begin ()

    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )
    set_description( N_("Linear PCM audio decoder") )
    set_capability( "audio decoder", 100 )
    set_callbacks( OpenDecoder, CloseCommon )

    add_submodule ()
    set_description( N_("Linear PCM audio packetizer") )
    set_capability( "packetizer", 100 )
    set_callbacks( OpenPacketizer, CloseCommon )

#ifdef ENABLE_SOUT
    add_submodule ()
    set_description( N_("Linear PCM audio encoder") )
    set_capability( "encoder", 100 )
    set_callbacks( OpenEncoder, CloseEncoder )
    add_shortcut( "lpcm" )
#endif

vlc_module_end ()


/*****************************************************************************
 * decoder_sys_t : lpcm decoder descriptor
 *****************************************************************************/
typedef struct
{
    /* Module mode */
    bool b_packetizer;

    /*
     * Output properties
     */
    date_t   end_date;

    /* */
    unsigned i_header_size;
    int      i_type;
    uint8_t  i_chans_to_reorder;
    uint8_t  pi_chan_table[AOUT_CHAN_MAX];
} decoder_sys_t;

#ifdef ENABLE_SOUT
typedef struct
{
    int     i_channels;
    int     i_rate;

    int     i_frame_samples;
    uint8_t *p_buffer;
    int     i_buffer_used;
    int     i_frame_num;
} encoder_sys_t;
#endif

/*
 * LPCM DVD header :
 * - number of frames in this packet (8 bits)
 * - first access unit (16 bits) == 0x0003 ?
 * - emphasis (1 bit)
 * - mute (1 bit)
 * - reserved (1 bit)
 * - current frame (5 bits)
 * - quantisation (2 bits) 0 == 16bps, 1 == 20bps, 2 == 24bps, 3 == illegal
 * - frequency (2 bits) 0 == 48 kHz, 1 == 96 kHz, 2 == 44.1 kHz, 3 == 32 kHz
 * - reserved (1 bit)
 * - number of channels - 1 (3 bits) 1 == 2 channels
 * - dynamic range (8 bits) 0x80 == neutral
 *
 * LPCM DVD-A header (http://dvd-audio.sourceforge.net/spec/aob.shtml)
 * - continuity counter (8 bits, clipped to 0x00-0x1f)
 * - header size (16 bits)
 * - byte pointer to start of first audio frame.
 * - unknown (8bits, 0x10 for stereo, 0x00 for surround)
 * - sample size (4+4 bits)
 * - samplerate (4+4 bits)
 * - unknown (8 bits)
 * - group assignment (8 bits)
 * - unknown (8 bits)
 * - padding(variable)
 *
 * LPCM BD header :
 * - unknown (16 bits)
 * - number of channels (4 bits)
 * - frequency (4 bits)
 * - bits per sample (2 bits)
 * - unknown (6 bits)
 *
 * LPCM WIDI header
 * refers http://www.dvdforum.org/images/Guideline1394V10R0_20020911.pdf
  * - sub stream id (8 bits) = 0xa0
 * - frame header count (8 bits) = 0x06
 * [ 0b0000000 (7 bits)
 * - audio emphasis (1 bit) ] (8 bits)
 * [ qz word length (2 bits) 0x00 == 16bits
 * - sampling freq (3 bits) 0b001 == 44.1K, 0b010 == 48K Hz
 * - channels count(3 bits) ] (8 bits) 0b000 == dual mono, 0b001 == stereo
 * follows: LPCM data (15360 bits/1920 bytes)
 */

#define LPCM_VOB_HEADER_LEN (6)
#define LPCM_AOB_HEADER_LEN (11)
#define LPCM_BD_HEADER_LEN (4)
#define LPCM_WIDI_HEADER_LEN (4)

enum
{
    LPCM_VOB,
    LPCM_AOB,
    LPCM_BD,
    LPCM_WIDI,
};

typedef struct
{
    unsigned i_channels;
    unsigned i_bits;
    unsigned pi_position[6];
} aob_group_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int DecodeFrame    ( decoder_t *, block_t * );
static block_t *Packetize ( decoder_t *, block_t ** );
static void Flush( decoder_t * );

/* */
static int VobHeader( unsigned *pi_rate,
                      unsigned *pi_channels, unsigned *pi_original_channels,
                      unsigned *pi_bits,
                      const uint8_t *p_header );
static void VobExtract( block_t *, block_t *, unsigned i_bits );
/* */
static int AobHeader( unsigned *pi_rate,
                      unsigned *pi_channels, unsigned *pi_layout,
                      unsigned *pi_bits,
                      unsigned *pi_padding,
                      aob_group_t g[2],
                      const uint8_t *p_header );
static void AobExtract( block_t *, block_t *, unsigned i_bits, aob_group_t p_group[2] );
/* */
static int BdHeader( decoder_sys_t *p_sys,
                     unsigned *pi_rate,
                     unsigned *pi_channels,
                     unsigned *pi_channels_padding,
                     unsigned *pi_original_channels,
                     unsigned *pi_bits,
                     const uint8_t *p_header );
static void BdExtract( block_t *, block_t *, unsigned, unsigned, unsigned, unsigned );
/* */
static int WidiHeader( unsigned *pi_rate,
                       unsigned *pi_channels, unsigned *pi_original_channels,
                       unsigned *pi_bits,
                       const uint8_t *p_header );

/*****************************************************************************
 * OpenCommon:
 *****************************************************************************/
static int OpenCommon( decoder_t *p_dec, bool b_packetizer )
{
    decoder_sys_t *p_sys;
    int i_type;
    int i_header_size;

    switch( p_dec->fmt_in.i_codec )
    {
    /* DVD LPCM */
    case VLC_CODEC_DVD_LPCM:
        i_type = LPCM_VOB;
        i_header_size = LPCM_VOB_HEADER_LEN;
        break;
    /* DVD-Audio LPCM */
    case VLC_CODEC_DVDA_LPCM:
        i_type = LPCM_AOB;
        i_header_size = LPCM_AOB_HEADER_LEN;
        break;
    /* BD LPCM */
    case VLC_CODEC_BD_LPCM:
        i_type = LPCM_BD;
        i_header_size = LPCM_BD_HEADER_LEN;
        break;
    /* WIDI LPCM */
    case VLC_CODEC_WIDI_LPCM:
        i_type = LPCM_WIDI;
        i_header_size = LPCM_WIDI_HEADER_LEN;
        break;
    default:
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = malloc(sizeof(decoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;

    /* Misc init */
    p_sys->b_packetizer = b_packetizer;
    date_Set( &p_sys->end_date, VLC_TICK_INVALID );
    p_sys->i_type = i_type;
    p_sys->i_header_size = i_header_size;
    p_sys->i_chans_to_reorder = 0;

    /* Set output properties */
    if( b_packetizer )
    {
        switch( i_type )
        {
        case LPCM_VOB:
            p_dec->fmt_out.i_codec = VLC_CODEC_DVD_LPCM;
            break;
        case LPCM_AOB:
            p_dec->fmt_out.i_codec = VLC_CODEC_DVDA_LPCM;
            break;
        case LPCM_WIDI:
            p_dec->fmt_out.i_codec = VLC_CODEC_WIDI_LPCM;
            break;
        default:
            vlc_assert_unreachable();
        case LPCM_BD:
            p_dec->fmt_out.i_codec = VLC_CODEC_BD_LPCM;
            break;
        }
    }
    else
    {
        switch( p_dec->fmt_out.audio.i_bitspersample )
        {
        case 24:
        case 20:
            p_dec->fmt_out.i_codec = VLC_CODEC_S32N;
            p_dec->fmt_out.audio.i_bitspersample = 32;
            break;
        default:
            p_dec->fmt_out.i_codec = VLC_CODEC_S16N;
            p_dec->fmt_out.audio.i_bitspersample = 16;
            break;
        }
    }

    /* Set callback */
    if( !b_packetizer )
        p_dec->pf_decode    = DecodeFrame;
    else
        p_dec->pf_packetize = Packetize;
    p_dec->pf_flush     = Flush;

    return VLC_SUCCESS;
}
static int OpenDecoder( vlc_object_t *p_this )
{
    return OpenCommon( (decoder_t*) p_this, false );
}
static int OpenPacketizer( vlc_object_t *p_this )
{
    return OpenCommon( (decoder_t*) p_this, true );
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
 * DecodeFrame: decodes an lpcm frame.
 ****************************************************************************
 * Beware, this function must be fed with complete frames (PES packet).
 *****************************************************************************/
static block_t *Packetize( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t       *p_block;
    unsigned int  i_rate = 0, i_original_channels = 0, i_channels = 0, i_bits = 0;
    int           i_frame_length;

    if( !pp_block || !*pp_block ) return NULL;

    p_block = *pp_block;
    *pp_block = NULL; /* So the packet doesn't get re-sent */

    if( p_block->i_flags & (BLOCK_FLAG_CORRUPTED|BLOCK_FLAG_DISCONTINUITY) )
    {
        Flush( p_dec );
        if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
        {
            block_Release( p_block );
            *pp_block = NULL;
            return NULL;
        }
    }

    /* Date management */
    if( p_block->i_pts != VLC_TICK_INVALID &&
        p_block->i_pts != date_Get( &p_sys->end_date ) )
    {
        date_Set( &p_sys->end_date, p_block->i_pts );
    }

    if( date_Get( &p_sys->end_date ) == VLC_TICK_INVALID )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( p_block );
        return NULL;
    }

    if( p_block->i_buffer <= p_sys->i_header_size )
    {
        msg_Err(p_dec, "frame is too short");
        block_Release( p_block );
        return NULL;
    }

    int i_ret;
    unsigned i_channels_padding = 0;
    unsigned i_padding = 0; /* only for AOB */
    aob_group_t p_aob_group[2];

    switch( p_sys->i_type )
    {
    case LPCM_VOB:
        i_ret = VobHeader( &i_rate, &i_channels, &i_original_channels, &i_bits,
                           p_block->p_buffer );
        break;
    case LPCM_AOB:
        i_ret = AobHeader( &i_rate, &i_channels, &i_original_channels, &i_bits, &i_padding,
                           p_aob_group,
                           p_block->p_buffer );
        break;
    case LPCM_BD:
        i_ret = BdHeader( p_sys, &i_rate, &i_channels, &i_channels_padding, &i_original_channels, &i_bits,
                          p_block->p_buffer );
        break;
    case LPCM_WIDI:
        i_ret = WidiHeader( &i_rate, &i_channels, &i_original_channels, &i_bits,
                            p_block->p_buffer );
        break;
    default:
        abort();
    }

    if( i_ret || p_block->i_buffer <= p_sys->i_header_size + i_padding )
    {
        msg_Warn( p_dec, "no frame sync or too small frame" );
        block_Release( p_block );
        return NULL;
    }

    /* Set output properties */
    if( p_dec->fmt_out.audio.i_rate != i_rate )
    {
        date_Init( &p_sys->end_date, i_rate, 1 );
        date_Set( &p_sys->end_date, p_block->i_pts );
    }
    p_dec->fmt_out.audio.i_rate = i_rate;
    p_dec->fmt_out.audio.i_channels = i_channels;
    p_dec->fmt_out.audio.i_physical_channels = i_original_channels;

    if ( p_sys->i_type == LPCM_AOB )
    {
        i_frame_length = (p_block->i_buffer - p_sys->i_header_size - i_padding) /
                         (
                            ( (p_aob_group[0].i_bits / 8) * p_aob_group[0].i_channels ) +
                            ( (p_aob_group[1].i_bits / 8) * p_aob_group[1].i_channels )
                         );
    }
    else
    {
        i_frame_length = (p_block->i_buffer - p_sys->i_header_size - i_padding) /
                         (i_channels + i_channels_padding) * 8 / i_bits;
    }

    if( p_sys->b_packetizer )
    {
        p_block->i_pts = p_block->i_dts = date_Get( &p_sys->end_date );
        p_block->i_length =
            date_Increment( &p_sys->end_date, i_frame_length ) -
            p_block->i_pts;

        /* Just pass on the incoming frame */
        return p_block;
    }
    else
    {
        /* */
        if( i_bits == 16 )
        {
            p_dec->fmt_out.audio.i_format =
            p_dec->fmt_out.i_codec = VLC_CODEC_S16N;
            p_dec->fmt_out.audio.i_bitspersample = 16;
        }
        else
        {
            p_dec->fmt_out.audio.i_format =
            p_dec->fmt_out.i_codec = VLC_CODEC_S32N;
            p_dec->fmt_out.audio.i_bitspersample = 32;
        }
        aout_FormatPrepare(&p_dec->fmt_out.audio);

        /* */
        block_t *p_aout_buffer;
        if( decoder_UpdateAudioFormat( p_dec ) != VLC_SUCCESS ||
           !(p_aout_buffer = decoder_NewAudioBuffer( p_dec, i_frame_length )) )
        {
            block_Release( p_block );
            return NULL;
        }

        p_aout_buffer->i_pts = date_Get( &p_sys->end_date );
        p_aout_buffer->i_length =
            date_Increment( &p_sys->end_date, i_frame_length )
            - p_aout_buffer->i_pts;

        p_block->p_buffer += p_sys->i_header_size + i_padding;
        p_block->i_buffer -= p_sys->i_header_size + i_padding;

        switch( p_sys->i_type )
        {
        case LPCM_WIDI:
        case LPCM_VOB:
            VobExtract( p_aout_buffer, p_block, i_bits );
            break;
        case LPCM_AOB:
            AobExtract( p_aout_buffer, p_block, i_bits, p_aob_group );
            break;
        default:
            vlc_assert_unreachable();
        case LPCM_BD:
            BdExtract( p_aout_buffer, p_block, i_frame_length, i_channels, i_channels_padding, i_bits );
            break;
        }

        if( p_sys->i_chans_to_reorder )
        {
            aout_ChannelReorder( p_aout_buffer->p_buffer, p_aout_buffer->i_buffer,
                                 p_sys->i_chans_to_reorder, p_sys->pi_chan_table,
                                 p_dec->fmt_out.i_codec );
        }

        block_Release( p_block );
        return p_aout_buffer;
    }
}

static int DecodeFrame( decoder_t *p_dec, block_t *p_block )
{
    block_t *p_out = Packetize( p_dec, &p_block );
    if( p_out != NULL )
        decoder_QueueAudio( p_dec, p_out );
    return VLCDEC_SUCCESS;
}

/*****************************************************************************
 * CloseCommon : lpcm decoder destruction
 *****************************************************************************/
static void CloseCommon( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    free( p_dec->p_sys );
}

#ifdef ENABLE_SOUT
/*****************************************************************************
 * OpenEncoder: lpcm encoder construction
 *****************************************************************************/
static int OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;

    /* We only support DVD LPCM yet. */
    if( p_enc->fmt_out.i_codec != VLC_CODEC_DVD_LPCM )
        return VLC_EGENERIC;

    if( p_enc->fmt_in.audio.i_rate != 48000 &&
        p_enc->fmt_in.audio.i_rate != 96000 &&
        p_enc->fmt_in.audio.i_rate != 44100 &&
        p_enc->fmt_in.audio.i_rate != 32000 )
    {
        msg_Err( p_enc, "DVD LPCM supports only sample rates of 48, 96, 44.1 or 32 kHz" );
        return VLC_EGENERIC;
    }

    if( p_enc->fmt_in.audio.i_channels > 8 )
    {
        msg_Err( p_enc, "DVD LPCM supports a maximum of eight channels" );
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the encoder's structure */
    if( ( p_enc->p_sys = p_sys =
          (encoder_sys_t *)malloc(sizeof(encoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;

    /* In DVD LCPM, a frame is always 150 PTS ticks. */
    p_sys->i_frame_samples = p_enc->fmt_in.audio.i_rate * 150 / 90000;
    p_sys->p_buffer = xmalloc(p_sys->i_frame_samples
                            * p_enc->fmt_in.audio.i_channels * 16);
    p_sys->i_buffer_used = 0;
    p_sys->i_frame_num = 0;

    p_sys->i_channels = p_enc->fmt_in.audio.i_channels;
    p_sys->i_rate = p_enc->fmt_in.audio.i_rate;

    p_enc->pf_encode_audio = EncodeFrames;
    p_enc->fmt_in.i_codec = p_enc->fmt_out.i_codec;

    p_enc->fmt_in.audio.i_bitspersample = 16;
    p_enc->fmt_in.i_codec = VLC_CODEC_S16N;

    p_enc->fmt_out.i_bitrate =
        p_enc->fmt_in.audio.i_channels *
        p_enc->fmt_in.audio.i_rate *
        p_enc->fmt_in.audio.i_bitspersample *
        (p_sys->i_frame_samples + LPCM_VOB_HEADER_LEN) /
        p_sys->i_frame_samples;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseEncoder: lpcm encoder destruction
 *****************************************************************************/
static void CloseEncoder ( vlc_object_t *p_this )
{
    encoder_t     *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    free( p_sys->p_buffer );
    free( p_sys );
}

/*****************************************************************************
 * EncodeFrames: encode zero or more LCPM audio packets
 *****************************************************************************/
static block_t *EncodeFrames( encoder_t *p_enc, block_t *p_aout_buf )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    block_t *p_first_block = NULL, *p_last_block = NULL;

    if( !p_aout_buf || !p_aout_buf->i_buffer ) return NULL;

    const int i_num_frames = ( p_sys->i_buffer_used + p_aout_buf->i_nb_samples ) /
        p_sys->i_frame_samples;
    const int i_leftover_samples = ( p_sys->i_buffer_used + p_aout_buf->i_nb_samples ) %
        p_sys->i_frame_samples;
    const int i_frame_size = p_sys->i_frame_samples * p_sys->i_channels * 2 + LPCM_VOB_HEADER_LEN;
    const int i_start_offset = -p_sys->i_buffer_used;

    uint8_t i_freq_code = 0;

    switch( p_sys->i_rate ) {
    case 48000:
        i_freq_code = 0;
        break;
    case 96000:
        i_freq_code = 1;
        break;
    case 44100:
        i_freq_code = 2;
        break;
    case 32000:
        i_freq_code = 3;
        break;
    default:
        vlc_assert_unreachable();
    }

    int i_bytes_consumed = 0;

    for ( int i = 0; i < i_num_frames; ++i )
    {
        block_t *p_block = block_Alloc( i_frame_size );
        if( !p_block )
            return NULL;

        uint8_t *frame = (uint8_t *)p_block->p_buffer;
        frame[0] = 1;  /* one frame in packet */
        frame[1] = 0;
        frame[2] = 0;  /* no first access unit */
        frame[3] = (p_sys->i_frame_num + i) & 0x1f;  /* no emphasis, no mute */
        frame[4] = (i_freq_code << 4) | (p_sys->i_channels - 1);
        frame[5] = 0x80;  /* neutral dynamic range */

        const int i_consume_samples = p_sys->i_frame_samples - p_sys->i_buffer_used;
        const int i_kept_bytes = p_sys->i_buffer_used * p_sys->i_channels * 2;
        const int i_consume_bytes = i_consume_samples * p_sys->i_channels * 2;

#ifdef WORDS_BIGENDIAN
        memcpy( frame + 6, p_sys->p_buffer, i_kept_bytes );
        memcpy( frame + 6 + i_kept_bytes, p_aout_buf->p_buffer + i_bytes_consumed,
                i_consume_bytes );
#else
        swab( p_sys->p_buffer, frame + 6, i_kept_bytes );
        swab( p_aout_buf->p_buffer + i_bytes_consumed, frame + 6 + i_kept_bytes,
              i_consume_bytes );
#endif

        p_sys->i_frame_num++;
        p_sys->i_buffer_used = 0;
        i_bytes_consumed += i_consume_bytes;

        /* We need to find i_length by means of next_pts due to possible roundoff errors. */
        vlc_tick_t this_pts = p_aout_buf->i_pts +
            vlc_tick_from_samples(i * p_sys->i_frame_samples + i_start_offset, p_sys->i_rate);
        vlc_tick_t next_pts = p_aout_buf->i_pts +
            vlc_tick_from_samples((i + 1) * p_sys->i_frame_samples + i_start_offset, p_sys->i_rate);

        p_block->i_pts = p_block->i_dts = this_pts;
        p_block->i_length = next_pts - this_pts;

        if( !p_first_block )
            p_first_block = p_last_block = p_block;
        else
            p_last_block = p_last_block->p_next = p_block;
    }

    memcpy( p_sys->p_buffer,
            p_aout_buf->p_buffer + i_bytes_consumed,
            i_leftover_samples * p_sys->i_channels * 2 );
    p_sys->i_buffer_used = i_leftover_samples;

    return p_first_block;
}
#endif

/*****************************************************************************
 *
 *****************************************************************************/
static int VobHeader( unsigned *pi_rate,
                      unsigned *pi_channels, unsigned *pi_original_channels,
                      unsigned *pi_bits,
                      const uint8_t *p_header )
{
    const uint8_t i_header = p_header[4];

    switch( (i_header >> 4) & 0x3 )
    {
    case 0:
        *pi_rate = 48000;
        break;
    case 1:
        *pi_rate = 96000;
        break;
    case 2:
        *pi_rate = 44100;
        break;
    case 3:
        *pi_rate = 32000;
        break;
    }

    *pi_channels = (i_header & 0x7) + 1;
    switch( *pi_channels - 1 )
    {
    case 0:
        *pi_original_channels = AOUT_CHAN_CENTER;
        break;
    case 1:
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
        break;
    case 2:
        /* This is unsure. */
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_LFE;
        break;
    case 3:
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
        break;
    case 4:
        /* This is unsure. */
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                               | AOUT_CHAN_LFE;
        break;
    case 5:
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                               | AOUT_CHAN_CENTER | AOUT_CHAN_LFE;
        break;
    case 6:
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                               | AOUT_CHAN_CENTER | AOUT_CHAN_MIDDLELEFT
                               | AOUT_CHAN_MIDDLERIGHT;
        break;
    case 7:
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                               | AOUT_CHAN_CENTER | AOUT_CHAN_MIDDLELEFT
                               | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE;
        break;
    }

    switch( (i_header >> 6) & 0x3 )
    {
    case 2:
        *pi_bits = 24;
        break;
    case 1:
        *pi_bits = 20;
        break;
    case 0:
    default:
        *pi_bits = 16;
        break;
    }

    /* Check frame sync and drop it. */
    if( p_header[5] != 0x80 )
        return -1;
    return 0;
}

static const unsigned p_aob_group1[21][6] = {
    { AOUT_CHAN_CENTER, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, AOUT_CHAN_CENTER,   0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, AOUT_CHAN_CENTER,   0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, AOUT_CHAN_CENTER,   0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, AOUT_CHAN_CENTER,   0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, AOUT_CHAN_CENTER,   0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT, 0  },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT, 0  },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT, 0  },
};
static const unsigned p_aob_group2[21][6] = {
    { 0 },
    { 0 },
    { AOUT_CHAN_REARCENTER, 0 },
    { AOUT_CHAN_REARLEFT,   AOUT_CHAN_REARRIGHT,    0 },
    { AOUT_CHAN_LFE,        0 },
    { AOUT_CHAN_LFE,        AOUT_CHAN_REARCENTER,   0 },
    { AOUT_CHAN_LFE,        AOUT_CHAN_REARLEFT,     AOUT_CHAN_REARRIGHT,    0 },
    { AOUT_CHAN_CENTER,     0 },
    { AOUT_CHAN_CENTER,     AOUT_CHAN_REARCENTER, 0 },
    { AOUT_CHAN_CENTER,     AOUT_CHAN_REARLEFT,   AOUT_CHAN_REARRIGHT,    0 },
    { AOUT_CHAN_CENTER,     AOUT_CHAN_LFE,        0 },
    { AOUT_CHAN_CENTER,     AOUT_CHAN_LFE,        AOUT_CHAN_REARCENTER,   0 },
    { AOUT_CHAN_CENTER,     AOUT_CHAN_LFE,        AOUT_CHAN_REARLEFT,     AOUT_CHAN_REARRIGHT,    0 },
    { AOUT_CHAN_REARCENTER, 0 },
    { AOUT_CHAN_REARLEFT,   AOUT_CHAN_REARRIGHT,    0 },
    { AOUT_CHAN_LFE,        0 },
    { AOUT_CHAN_LFE,        AOUT_CHAN_REARCENTER,   0 },
    { AOUT_CHAN_LFE,        AOUT_CHAN_REARLEFT,     AOUT_CHAN_REARRIGHT,    0 },
    { AOUT_CHAN_LFE,        0 },
    { AOUT_CHAN_CENTER,     0 },
    { AOUT_CHAN_CENTER,     AOUT_CHAN_LFE,          0 },
};

static int AobHeader( unsigned *pi_rate,
                      unsigned *pi_channels, unsigned *pi_layout,
                      unsigned *pi_bits,
                      unsigned *pi_padding,
                      aob_group_t g[2],
                      const uint8_t *p_header )
{
    const unsigned i_header_size = GetWBE( &p_header[1] );
    if( i_header_size + 3 < LPCM_AOB_HEADER_LEN )
        return VLC_EGENERIC;

    /* Padding = Total header size - Normal AOB header
     *         + 3 bytes (1 for continuity counter + 2 for header_size ) */
    *pi_padding = 3 + i_header_size - LPCM_AOB_HEADER_LEN;

    const int i_index_size_g1 = (p_header[6] >> 4);
    const int i_index_size_g2 = (p_header[6]     ) & 0x0f;
    const int i_index_rate_g1 = (p_header[7] >> 4);
    const int i_index_rate_g2 = (p_header[7]     ) & 0x0f;
    const int i_assignment     = p_header[9];

    /* Validate */
    if( i_index_size_g1 > 0x02 ||
        ( i_index_size_g2 != 0x0f && i_index_size_g2 > 0x02 ) )
        return VLC_EGENERIC;
    if( (i_index_rate_g1 & 0x07) > 0x02 ||
        ( i_index_rate_g2 != 0x0f && (i_index_rate_g1 & 0x07) > 0x02 ) )
        return VLC_EGENERIC;
    if( i_assignment > 20 )
        return VLC_EGENERIC;

    /* */
    /* max is 0x2, 0xf == unused */
    g[0].i_bits = 16 + 4 * i_index_size_g1;
    g[1].i_bits = ( i_index_size_g2 != 0x0f ) ? 16 + 4 * i_index_size_g2 : 0;

    /* No info about interlacing of different sampling rate */
    if ( g[1].i_bits && ( i_index_rate_g1 != i_index_rate_g2 ) )
        return VLC_EGENERIC;

    /* only set 16bits if both are <= */
    if( g[0].i_bits )
    {
        if( g[0].i_bits > 16 || g[1].i_bits > 16 )
            *pi_bits = 32;
        else
            *pi_bits = 16;
    }
    else
        return VLC_EGENERIC;

    if( i_index_rate_g1 & 0x08 )
        *pi_rate = 44100 << (i_index_rate_g1 & 0x07);
    else
        *pi_rate = 48000 << (i_index_rate_g1 & 0x07);


    /* Group1 */
    unsigned i_channels1 = 0;
    unsigned i_layout1 = 0;
    for( int i = 0; p_aob_group1[i_assignment][i] != 0; i++ )
    {
        i_channels1++;
        i_layout1 |= p_aob_group1[i_assignment][i];
    }
    /* Group2 */
    unsigned i_channels2 = 0;
    unsigned i_layout2 = 0;
    if( i_index_size_g2 != 0x0f && i_index_rate_g2 != 0x0f )
    {
        for( int i = 0; p_aob_group2[i_assignment][i] != 0; i++ )
        {
            i_channels2++;
            i_layout2 |= p_aob_group2[i_assignment][i];
        }
        assert( (i_layout1 & i_layout2) == 0 );
    }

    /* */
    *pi_channels = i_channels1 + ( g[1].i_bits ? i_channels2 : 0 );
    *pi_layout   = i_layout1   | ( g[1].i_bits ? i_layout2   : 0 );

    /* */
    for( unsigned i = 0; i < 2; i++ )
    {
        const unsigned *p_aob = i == 0 ? p_aob_group1[i_assignment] :
                                         p_aob_group2[i_assignment];
        g[i].i_channels = i == 0 ? i_channels1 :
                                   i_channels2;

        if( !g[i].i_bits )
            continue;
        for( unsigned j = 0; j < g[i].i_channels; j++ )
        {
            g[i].pi_position[j] = 0;
            for( int k = 0; pi_vlc_chan_order_wg4[k] != 0; k++ )
            {
                const unsigned i_channel = pi_vlc_chan_order_wg4[k];
                if( i_channel == p_aob[j] )
                    break;
                if( (*pi_layout) & i_channel )
                    g[i].pi_position[j]++;
            }
        }
    }
    return VLC_SUCCESS;
}

static const uint32_t pi_8channels_in[] =
{ AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT, AOUT_CHAN_CENTER,
  AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT,
  AOUT_CHAN_MIDDLERIGHT, AOUT_CHAN_LFE, 0 };

static const uint32_t pi_7channels_in[] =
{ AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT, AOUT_CHAN_CENTER,
  AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT,
  AOUT_CHAN_MIDDLERIGHT, 0 };

static const uint32_t pi_6channels_in[] =
{ AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT, AOUT_CHAN_CENTER,
  AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT, AOUT_CHAN_LFE, 0 };

static const uint32_t pi_5channels_in[] =
{ AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT, AOUT_CHAN_CENTER,
  AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT, 0 };

static const uint32_t pi_4channels_in[] =
{ AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT,
  AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT, 0 };

static const uint32_t pi_3channels_in[] =
{ AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT,
  AOUT_CHAN_CENTER, 0 };


static int BdHeader( decoder_sys_t *p_sys,
                     unsigned *pi_rate,
                     unsigned *pi_channels,
                     unsigned *pi_channels_padding,
                     unsigned *pi_original_channels,
                     unsigned *pi_bits,
                     const uint8_t *p_header )
{
    const uint32_t h = GetDWBE( p_header );
    const uint32_t *pi_channels_in = NULL;
    switch( ( h & 0xf000) >> 12 )
    {
    case 1:
        *pi_channels = 1;
        *pi_original_channels = AOUT_CHAN_CENTER;
        break;
    case 3:
        *pi_channels = 2;
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
        break;
    case 4:
        *pi_channels = 3;
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER;
        pi_channels_in = pi_3channels_in;
        break;
    case 5:
        *pi_channels = 3;
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARCENTER;
        break;
    case 6:
        *pi_channels = 4;
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER |
                                AOUT_CHAN_REARCENTER;
        break;
    case 7:
        *pi_channels = 4;
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
        pi_channels_in = pi_4channels_in;
        break;
    case 8:
        *pi_channels = 5;
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER |
                                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
        pi_channels_in = pi_5channels_in;
        break;
    case 9:
        *pi_channels = 6;
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER |
                                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT |
                                AOUT_CHAN_LFE;
        pi_channels_in = pi_6channels_in;
        break;
    case 10:
        *pi_channels = 7;
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER |
                                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT |
                                AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT;
        pi_channels_in = pi_7channels_in;
        break;
    case 11:
        *pi_channels = 8;
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER |
                                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT |
                                AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT |
                                AOUT_CHAN_LFE;
        pi_channels_in = pi_8channels_in;
        break;

    default:
        return -1;
    }
    *pi_channels_padding = *pi_channels & 1;

    switch( (h >> 6) & 0x03 )
    {
    case 1:
        *pi_bits = 16;
        break;
    case 2: /* 20 bits but samples are stored on 24 bits */
    case 3: /* 24 bits */
        *pi_bits = 24;
        break;
    default:
        return -1;
    }
    switch( (h >> 8) & 0x0f )
    {
    case 1:
        *pi_rate = 48000;
        break;
    case 4:
        *pi_rate = 96000;
        break;
    case 5:
        *pi_rate = 192000;
        break;
    default:
        return -1;
    }

    if( pi_channels_in )
    {
        p_sys->i_chans_to_reorder =
            aout_CheckChannelReorder( pi_channels_in, NULL,
                                      *pi_original_channels,
                                      p_sys->pi_chan_table );
    }

    return 0;
}

static int WidiHeader( unsigned *pi_rate,
                       unsigned *pi_channels, unsigned *pi_original_channels,
                       unsigned *pi_bits,
                       const uint8_t *p_header )
{
    if ( p_header[0] != 0xa0 || p_header[1] != 0x06 )
        return -1;

    switch( ( p_header[3] & 0x38 ) >> 3 )
    {
    case 0x01: //0b001
        *pi_rate = 44100;
        break;
    case 0x02: //0b010
        *pi_rate = 48000;
        break;
    default:
        return -1;
    }

    if( p_header[3] >> 6 != 0 )
        return -1;
    else
        *pi_bits = 16;

    *pi_channels = (p_header[3] & 0x7) + 1;

    *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;

    return 0;
}

static void VobExtract( block_t *p_aout_buffer, block_t *p_block,
                        unsigned i_bits )
{
    /* 20/24 bits LPCM use special packing */
    if( i_bits == 24 )
    {
        uint32_t *p_out = (uint32_t *)p_aout_buffer->p_buffer;

        while( p_block->i_buffer / 12 )
        {
            /* Sample 1 */
            *(p_out++) = (p_block->p_buffer[ 0] << 24)
                       | (p_block->p_buffer[ 1] << 16)
                       | (p_block->p_buffer[ 8] <<  8);
            /* Sample 2 */
            *(p_out++) = (p_block->p_buffer[ 2] << 24)
                       | (p_block->p_buffer[ 3] << 16)
                       | (p_block->p_buffer[ 9] <<  8);
            /* Sample 3 */
            *(p_out++) = (p_block->p_buffer[ 4] << 24)
                       | (p_block->p_buffer[ 5] << 16)
                       | (p_block->p_buffer[10] <<  8);
            /* Sample 4 */
            *(p_out++) = (p_block->p_buffer[ 6] << 24)
                       | (p_block->p_buffer[ 7] << 16)
                       | (p_block->p_buffer[11] <<  8);

            p_block->i_buffer -= 12;
            p_block->p_buffer += 12;
        }
    }
    else if( i_bits == 20 )
    {
        uint32_t *p_out = (uint32_t *)p_aout_buffer->p_buffer;

        while( p_block->i_buffer / 10 )
        {
            /* Sample 1 */
            *(p_out++) = ( p_block->p_buffer[0]         << 24)
                       | ( p_block->p_buffer[1]         << 16)
                       | ((p_block->p_buffer[8] & 0xF0) <<  8);
            /* Sample 2 */
            *(p_out++) = ( p_block->p_buffer[2]         << 24)
                       | ( p_block->p_buffer[3]         << 16)
                       | ((p_block->p_buffer[8] & 0x0F) << 12);
            /* Sample 3 */
            *(p_out++) = ( p_block->p_buffer[4]         << 24)
                       | ( p_block->p_buffer[5]         << 16)
                       | ((p_block->p_buffer[9] & 0xF0) <<  8);
            /* Sample 4 */
            *(p_out++) = ( p_block->p_buffer[6]         << 24)
                       | ( p_block->p_buffer[7]         << 16)
                       | ((p_block->p_buffer[9] & 0x0F) << 12);

            p_block->i_buffer -= 10;
            p_block->p_buffer += 10;
        }
    }
    else
    {
        assert( i_bits == 16 );
#ifdef WORDS_BIGENDIAN
        memcpy( p_aout_buffer->p_buffer, p_block->p_buffer, p_block->i_buffer );
#else
        swab( p_block->p_buffer, p_aout_buffer->p_buffer, p_block->i_buffer );
#endif
    }
}

static void AobExtract( block_t *p_aout_buffer,
                        block_t *p_block, unsigned i_aoutbits, aob_group_t p_group[2] )
{
    uint8_t *p_out = p_aout_buffer->p_buffer;
    const unsigned i_total_channels = p_group[0].i_channels +
                                      ( p_group[1].i_bits ? p_group[1].i_channels : 0 );

    while( p_block->i_buffer > 0 )
    {
        unsigned int i_aout_written = 0;

        for( int i = 0; i < 2; i++ )
        {
            const aob_group_t *g = &p_group[1-i];
            const unsigned int i_group_size = 2 * g->i_channels * g->i_bits / 8;

            if( p_block->i_buffer < i_group_size )
            {
                p_block->i_buffer = 0;
                break;
            }

            if( !g->i_bits )
                continue;

            for( unsigned n = 0; n < 2; n++ )
            {
                for( unsigned j = 0; j < g->i_channels; j++ )
                {
                    const int i_src = n * g->i_channels + j;
                    const int i_dst = n * i_total_channels + g->pi_position[j];
                    uint32_t *p_out32 = (uint32_t *) &p_out[4*i_dst];

                    if( g->i_bits == 24 )
                    {
                        assert( i_aoutbits == 32 );
                        *p_out32 = (p_block->p_buffer[2*i_src+0] << 24)
                                 | (p_block->p_buffer[2*i_src+1] << 16)
                                 | (p_block->p_buffer[4*g->i_channels+i_src] <<  8);
#ifdef WORDS_BIGENDIAN
                        *p_out32 = vlc_bswap32(*p_out32);
#endif
                        i_aout_written += 4;
                    }
                    else if( g->i_bits == 20 )
                    {
                        assert( i_aoutbits == 32 );
                        *p_out32 = (p_block->p_buffer[2*i_src+0] << 24)
                                 | (p_block->p_buffer[2*i_src+1] << 16)
                                 | (((p_block->p_buffer[4*g->i_channels+i_src] << ((!n)?0:4) ) & 0xf0) <<  8);
#ifdef WORDS_BIGENDIAN
                        *p_out32 = vlc_bswap32(*p_out32);
#endif
                        i_aout_written += 4;
                    }
                    else
                    {
                        assert( g->i_bits == 16 );
                        assert( i_aoutbits == 16 || i_aoutbits == 32 );
                        if( i_aoutbits == 16 )
                        {
#ifdef WORDS_BIGENDIAN
                            memcpy( &p_out[2*i_dst], &p_block->p_buffer[2*i_src], 2 );
#else
                            p_out[2*i_dst+1] = p_block->p_buffer[2*i_src+0];
                            p_out[2*i_dst+0] = p_block->p_buffer[2*i_src+1];
#endif
                            i_aout_written += 2;
                        }
                        else
                        {
                            *p_out32 = (p_block->p_buffer[2*i_src+0] << 24)
                                     | (p_block->p_buffer[2*i_src+1] << 16);
#ifdef WORDS_BIGENDIAN
                            *p_out32 = vlc_bswap32(*p_out32);
#endif
                            i_aout_written += 4;
                        }
                    }
                }
            }

            /* */
            p_block->i_buffer -= i_group_size;
            p_block->p_buffer += i_group_size;
        }
        p_out += i_aout_written;
    }
}
static void BdExtract( block_t *p_aout_buffer, block_t *p_block,
                       unsigned i_frame_length,
                       unsigned i_channels, unsigned i_channels_padding,
                       unsigned i_bits )
{
    if( i_bits != 16 || i_channels_padding > 0 )
    {
        uint8_t *p_src = p_block->p_buffer;
        uint8_t *p_dst = p_aout_buffer->p_buffer;
        int dst_inc = ((i_bits == 16) ? 2 : 4) * i_channels;

        while( i_frame_length > 0 )
        {
#ifdef WORDS_BIGENDIAN
            memcpy( p_dst, p_src, i_channels * i_bits / 8 );
#else
            if (i_bits == 16) {
                swab( p_src, p_dst, (i_channels + i_channels_padding) * i_bits / 8 );
            } else {
                for (unsigned i = 0; i < i_channels; ++i) {
                    p_dst[i * 4] = 0;
                    p_dst[1 + (i * 4)] = p_src[2 + (i * 3)];
                    p_dst[2 + (i * 4)] = p_src[1 + (i * 3)];
                    p_dst[3 + (i * 4)] = p_src[i * 3];
                }
            }
#endif
            p_src += (i_channels + i_channels_padding) * i_bits / 8;
            p_dst += dst_inc;
            i_frame_length--;
        }
    }
    else
    {
#ifdef WORDS_BIGENDIAN
        memcpy( p_aout_buffer->p_buffer, p_block->p_buffer, p_block->i_buffer );
#else
        swab( p_block->p_buffer, p_aout_buffer->p_buffer, p_block->i_buffer );
#endif
    }
}

