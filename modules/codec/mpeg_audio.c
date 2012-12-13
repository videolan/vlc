/*****************************************************************************
 * mpeg_audio.c: parse MPEG audio sync info and packetize the stream
 *****************************************************************************
 * Copyright (C) 2001-2003 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <vlc_modules.h>
#include <assert.h>

#include <vlc_block_helper.h>

#include "../packetizer/packetizer_helper.h"

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /* Module mode */
    bool b_packetizer;

    /*
     * Input properties
     */
    int        i_state;

    block_bytestream_t bytestream;

    /*
     * Common properties
     */
    date_t          end_date;
    unsigned int    i_current_layer;

    mtime_t i_pts;

    int i_frame_size, i_free_frame_size;
    unsigned int i_channels_conf, i_channels;
    unsigned int i_rate, i_max_frame_size, i_frame_length;
    unsigned int i_layer, i_bit_rate;

    bool   b_discontinuity;
};

/* This isn't the place to put mad-specific stuff. However, it makes the
 * mad plug-in's life much easier if we put 8 extra bytes at the end of the
 * buffer, because that way it doesn't have to copy the block_t to a bigger
 * buffer. This has no implication on other plug-ins, and we only lose 8 bytes
 * per frame. --Meuuh */
#define MAD_BUFFER_GUARD 8
#define MPGA_HEADER_SIZE 4

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static int  OpenPacketizer( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );
static block_t *DecodeBlock  ( decoder_t *, block_t ** );

static uint8_t *GetOutBuffer ( decoder_t *, block_t ** );
static block_t *GetAoutBuffer( decoder_t * );
static block_t *GetSoutBuffer( decoder_t * );

static int SyncInfo( uint32_t i_header, unsigned int * pi_channels,
                     unsigned int * pi_channels_conf,
                     unsigned int * pi_sample_rate, unsigned int * pi_bit_rate,
                     unsigned int * pi_frame_length,
                     unsigned int * pi_max_frame_size,
                     unsigned int * pi_layer );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("MPEG audio layer I/II/III decoder") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )
    set_capability( "decoder", 100 )
    set_callbacks( OpenDecoder, CloseDecoder )

    add_submodule ()
    set_description( N_("MPEG audio layer I/II/III packetizer") )
    set_capability( "packetizer", 10 )
    set_callbacks( OpenPacketizer, CloseDecoder )
vlc_module_end ()

/*****************************************************************************
 * Open: probe the decoder and return score
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_MPGA )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;

    /* Misc init */
    p_sys->b_packetizer = false;
    p_sys->i_state = STATE_NOSYNC;
    date_Set( &p_sys->end_date, 0 );
    block_BytestreamInit( &p_sys->bytestream );
    p_sys->i_pts = VLC_TS_INVALID;
    p_sys->b_discontinuity = false;

    /* Set output properties */
    p_dec->fmt_out.i_cat = AUDIO_ES;
    p_dec->fmt_out.i_codec = VLC_CODEC_MPGA;
    p_dec->fmt_out.audio.i_rate = 0; /* So end_date gets initialized */

    /* Set callback */
    p_dec->pf_decode_audio = DecodeBlock;
    p_dec->pf_packetize    = DecodeBlock;

    /* Start with the minimum size for a free bitrate frame */
    p_sys->i_free_frame_size = MPGA_HEADER_SIZE;

    return VLC_SUCCESS;
}

static int OpenDecoder( vlc_object_t *p_this )
{
    /* HACK: Don't use this codec if we don't have an mpga audio filter */
    if( !module_exists( "mpgatofixed32" ) )
        return VLC_EGENERIC;

    return Open( p_this );
}

static int OpenPacketizer( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    int i_ret = Open( p_this );

    if( i_ret == VLC_SUCCESS ) p_dec->p_sys->b_packetizer = true;

    return i_ret;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function is called just after the thread is launched.
 ****************************************************************************/
static block_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t p_header[MAD_BUFFER_GUARD];
    uint32_t i_header;
    uint8_t *p_buf;
    block_t *p_out_buffer;

    if( !pp_block || !*pp_block ) return NULL;

    if( (*pp_block)->i_flags&(BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
        if( (*pp_block)->i_flags&BLOCK_FLAG_CORRUPTED )
        {
            p_sys->i_state = STATE_NOSYNC;
            block_BytestreamEmpty( &p_sys->bytestream );
        }
        date_Set( &p_sys->end_date, 0 );
        block_Release( *pp_block );
        p_sys->b_discontinuity = true;
        return NULL;
    }

    if( !date_Get( &p_sys->end_date ) && (*pp_block)->i_pts <= VLC_TS_INVALID )
    {
        /* We've just started the stream, wait for the first PTS. */
        msg_Dbg( p_dec, "waiting for PTS" );
        block_Release( *pp_block );
        return NULL;
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
                /* Look for sync word - should be 0xffe */
                if( p_header[0] == 0xff && (p_header[1] & 0xe0) == 0xe0 )
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
            if( p_sys->i_pts > VLC_TS_INVALID &&
                p_sys->i_pts != date_Get( &p_sys->end_date ) )
            {
                date_Set( &p_sys->end_date, p_sys->i_pts );
            }
            p_sys->i_state = STATE_HEADER;

        case STATE_HEADER:
            /* Get MPGA frame header (MPGA_HEADER_SIZE bytes) */
            if( block_PeekBytes( &p_sys->bytestream, p_header,
                                 MPGA_HEADER_SIZE ) != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }

            /* Build frame header */
            i_header = (p_header[0]<<24)|(p_header[1]<<16)|(p_header[2]<<8)
                       |p_header[3];

            /* Check if frame is valid and get frame info */
            p_sys->i_frame_size = SyncInfo( i_header,
                                            &p_sys->i_channels,
                                            &p_sys->i_channels_conf,
                                            &p_sys->i_rate,
                                            &p_sys->i_bit_rate,
                                            &p_sys->i_frame_length,
                                            &p_sys->i_max_frame_size,
                                            &p_sys->i_layer );

            p_dec->fmt_in.i_profile = p_sys->i_layer;

            if( p_sys->i_frame_size == -1 )
            {
                msg_Dbg( p_dec, "emulated startcode" );
                block_SkipByte( &p_sys->bytestream );
                p_sys->i_state = STATE_NOSYNC;
                p_sys->b_discontinuity = true;
                break;
            }

            if( p_sys->i_bit_rate == 0 )
            {
                /* Free bitrate, but 99% emulated startcode :( */
                if( p_dec->p_sys->i_free_frame_size == MPGA_HEADER_SIZE )
                {
                    msg_Dbg( p_dec, "free bitrate mode");
                }
                /* The -1 below is to account for the frame padding */
                p_sys->i_frame_size = p_sys->i_free_frame_size - 1;
            }

            p_sys->i_state = STATE_NEXT_SYNC;

        case STATE_NEXT_SYNC:
            /* TODO: If p_block == NULL, flush the buffer without checking the
             * next sync word */

            /* Check if next expected frame contains the sync word */
            if( block_PeekOffsetBytes( &p_sys->bytestream,
                                       p_sys->i_frame_size, p_header,
                                       MAD_BUFFER_GUARD ) != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }

            if( p_header[0] == 0xff && (p_header[1] & 0xe0) == 0xe0 )
            {
                /* Startcode is fine, let's try the header as an extra check */
                int i_next_frame_size;
                unsigned int i_next_channels, i_next_channels_conf;
                unsigned int i_next_rate, i_next_bit_rate;
                unsigned int i_next_frame_length, i_next_max_frame_size;
                unsigned int i_next_layer;

                /* Build frame header */
                i_header = (p_header[0]<<24)|(p_header[1]<<16)|(p_header[2]<<8)
                           |p_header[3];

                i_next_frame_size = SyncInfo( i_header,
                                              &i_next_channels,
                                              &i_next_channels_conf,
                                              &i_next_rate,
                                              &i_next_bit_rate,
                                              &i_next_frame_length,
                                              &i_next_max_frame_size,
                                              &i_next_layer );

                /* Free bitrate only */
                if( p_sys->i_bit_rate == 0 && i_next_frame_size == -1 )
                {
                    if( (unsigned int)p_sys->i_frame_size >
                        p_sys->i_max_frame_size )
                    {
                        msg_Dbg( p_dec, "frame too big %d > %d "
                                 "(emulated startcode ?)", p_sys->i_frame_size,
                                 p_sys->i_max_frame_size );
                        block_SkipByte( &p_sys->bytestream );
                        p_sys->i_state = STATE_NOSYNC;
                        p_sys->i_free_frame_size = MPGA_HEADER_SIZE;
                        break;
                    }

                    p_sys->i_frame_size++;
                    break;
                }

                if( i_next_frame_size == -1 )
                {
                    msg_Dbg( p_dec, "emulated startcode on next frame" );
                    block_SkipByte( &p_sys->bytestream );
                    p_sys->i_state = STATE_NOSYNC;
                    p_sys->b_discontinuity = true;
                    break;
                }

                /* Check info is in sync with previous one */
                if( i_next_channels_conf != p_sys->i_channels_conf ||
                    i_next_rate != p_sys->i_rate ||
                    i_next_layer != p_sys->i_layer ||
                    i_next_frame_length != p_sys->i_frame_length )
                {
                    /* Free bitrate only */
                    if( p_sys->i_bit_rate == 0 )
                    {
                        p_sys->i_frame_size++;
                        break;
                    }

                    msg_Dbg( p_dec, "parameters changed unexpectedly "
                             "(emulated startcode ?)" );
                    block_SkipByte( &p_sys->bytestream );
                    p_sys->i_state = STATE_NOSYNC;
                    break;
                }

                /* Free bitrate only */
                if( p_sys->i_bit_rate == 0 )
                {
                    if( i_next_bit_rate != 0 )
                    {
                        p_sys->i_frame_size++;
                        break;
                    }
                }

            }
            else
            {
                /* Free bitrate only */
                if( p_sys->i_bit_rate == 0 )
                {
                    if( (unsigned int)p_sys->i_frame_size >
                        p_sys->i_max_frame_size )
                    {
                        msg_Dbg( p_dec, "frame too big %d > %d "
                                 "(emulated startcode ?)", p_sys->i_frame_size,
                                 p_sys->i_max_frame_size );
                        block_SkipByte( &p_sys->bytestream );
                        p_sys->i_state = STATE_NOSYNC;
                        p_sys->i_free_frame_size = MPGA_HEADER_SIZE;
                        break;
                    }

                    p_sys->i_frame_size++;
                    break;
                }

                msg_Dbg( p_dec, "emulated startcode "
                         "(no startcode on following frame)" );
                p_sys->i_state = STATE_NOSYNC;
                block_SkipByte( &p_sys->bytestream );
                break;
            }

            p_sys->i_state = STATE_SEND_DATA;
            break;

        case STATE_GET_DATA:
            /* Make sure we have enough data.
             * (Not useful if we went through NEXT_SYNC) */
            if( block_WaitBytes( &p_sys->bytestream,
                                 p_sys->i_frame_size ) != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }
            p_sys->i_state = STATE_SEND_DATA;

        case STATE_SEND_DATA:
            if( !(p_buf = GetOutBuffer( p_dec, &p_out_buffer )) )
            {
                //p_dec->b_error = true;
                return NULL;
            }

            /* Free bitrate only */
            if( p_sys->i_bit_rate == 0 )
            {
                p_sys->i_free_frame_size = p_sys->i_frame_size;
            }

            /* Copy the whole frame into the buffer. When we reach this point
             * we already know we have enough data available. */
            block_GetBytes( &p_sys->bytestream,
                            p_buf, __MIN( (unsigned)p_sys->i_frame_size, p_out_buffer->i_buffer ) );

            /* Get beginning of next frame for libmad */
            if( !p_sys->b_packetizer )
            {
                assert( p_out_buffer->i_buffer >= (unsigned)p_sys->i_frame_size + MAD_BUFFER_GUARD );
                memcpy( p_buf + p_sys->i_frame_size,
                        p_header, MAD_BUFFER_GUARD );
            }

            p_sys->i_state = STATE_NOSYNC;

            /* Make sure we don't reuse the same pts twice */
            if( p_sys->i_pts == p_sys->bytestream.p_block->i_pts )
                p_sys->i_pts = p_sys->bytestream.p_block->i_pts = VLC_TS_INVALID;

            /* So p_block doesn't get re-added several times */
            *pp_block = block_BytestreamPop( &p_sys->bytestream );

            return p_out_buffer;
        }
    }

    return NULL;
}

/*****************************************************************************
 * GetOutBuffer:
 *****************************************************************************/
static uint8_t *GetOutBuffer( decoder_t *p_dec, block_t **pp_out_buffer )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t *p_buf;

    if( p_dec->fmt_out.audio.i_rate != p_sys->i_rate )
    {
        msg_Dbg( p_dec, "MPGA channels:%d samplerate:%d bitrate:%d",
                  p_sys->i_channels, p_sys->i_rate, p_sys->i_bit_rate );

        date_Init( &p_sys->end_date, p_sys->i_rate, 1 );
        date_Set( &p_sys->end_date, p_sys->i_pts );
    }

    p_dec->fmt_out.audio.i_rate     = p_sys->i_rate;
    p_dec->fmt_out.audio.i_channels = p_sys->i_channels;
    p_dec->fmt_out.audio.i_frame_length = p_sys->i_frame_length;
    p_dec->fmt_out.audio.i_bytes_per_frame =
        p_sys->i_max_frame_size + MAD_BUFFER_GUARD;

    p_dec->fmt_out.audio.i_original_channels = p_sys->i_channels_conf;
    p_dec->fmt_out.audio.i_physical_channels =
        p_sys->i_channels_conf & AOUT_CHAN_PHYSMASK;

    p_dec->fmt_out.i_bitrate = p_sys->i_bit_rate * 1000;

    if( p_sys->b_packetizer )
    {
        block_t *p_sout_buffer = GetSoutBuffer( p_dec );
        p_buf = p_sout_buffer ? p_sout_buffer->p_buffer : NULL;
        *pp_out_buffer = p_sout_buffer;
    }
    else
    {
        block_t *p_aout_buffer = GetAoutBuffer( p_dec );
        p_buf = p_aout_buffer ? p_aout_buffer->p_buffer : NULL;
        *pp_out_buffer = p_aout_buffer;
    }

    return p_buf;
}

/*****************************************************************************
 * GetAoutBuffer:
 *****************************************************************************/
static block_t *GetAoutBuffer( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_buf;

    p_buf = decoder_NewAudioBuffer( p_dec, p_sys->i_frame_length );
    if( p_buf == NULL ) return NULL;

    p_buf->i_pts = date_Get( &p_sys->end_date );
    p_buf->i_length = date_Increment( &p_sys->end_date, p_sys->i_frame_length )
                      - p_buf->i_pts;
    if( p_sys->b_discontinuity )
        p_buf->i_flags |= BLOCK_FLAG_DISCONTINUITY;
    p_sys->b_discontinuity = false;

    /* Hack for libmad filter */
    p_buf = block_Realloc( p_buf, 0, p_sys->i_frame_size + MAD_BUFFER_GUARD );

    return p_buf;
}

/*****************************************************************************
 * GetSoutBuffer:
 *****************************************************************************/
static block_t *GetSoutBuffer( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;

    p_block = block_Alloc( p_sys->i_frame_size );
    if( p_block == NULL ) return NULL;

    p_block->i_pts = p_block->i_dts = date_Get( &p_sys->end_date );

    p_block->i_length =
        date_Increment( &p_sys->end_date, p_sys->i_frame_length ) - p_block->i_pts;

    return p_block;
}

/*****************************************************************************
 * CloseDecoder: clean up the decoder
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_BytestreamRelease( &p_sys->bytestream );

    free( p_sys );
}

/*****************************************************************************
 * SyncInfo: parse MPEG audio sync info
 *****************************************************************************/
static int SyncInfo( uint32_t i_header, unsigned int * pi_channels,
                     unsigned int * pi_channels_conf,
                     unsigned int * pi_sample_rate, unsigned int * pi_bit_rate,
                     unsigned int * pi_frame_length,
                     unsigned int * pi_max_frame_size, unsigned int * pi_layer)
{
    static const int ppi_bitrate[2][3][16] =
    {
        {
            /* v1 l1 */
            { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384,
              416, 448, 0},
            /* v1 l2 */
            { 0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256,
              320, 384, 0},
            /* v1 l3 */
            { 0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224,
              256, 320, 0}
        },

        {
            /* v2 l1 */
            { 0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192,
              224, 256, 0},
            /* v2 l2 */
            { 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128,
              144, 160, 0},
            /* v2 l3 */
            { 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128,
              144, 160, 0}
        }
    };

    static const int ppi_samplerate[2][4] = /* version 1 then 2 */
    {
        { 44100, 48000, 32000, 0 },
        { 22050, 24000, 16000, 0 }
    };

    int i_version, i_mode, i_emphasis;
    bool b_padding, b_mpeg_2_5;
    int i_frame_size = 0;
    int i_bitrate_index, i_samplerate_index;
    int i_max_bit_rate;

    b_mpeg_2_5  = 1 - ((i_header & 0x100000) >> 20);
    i_version   = 1 - ((i_header & 0x80000) >> 19);
    *pi_layer   = 4 - ((i_header & 0x60000) >> 17);
    //bool b_crc = !((i_header >> 16) & 0x01);
    i_bitrate_index = (i_header & 0xf000) >> 12;
    i_samplerate_index = (i_header & 0xc00) >> 10;
    b_padding   = (i_header & 0x200) >> 9;
    /* Extension */
    i_mode      = (i_header & 0xc0) >> 6;
    /* Modeext, copyright & original */
    i_emphasis  = i_header & 0x3;

    if( *pi_layer != 4 &&
        i_bitrate_index < 0x0f &&
        i_samplerate_index != 0x03 &&
        i_emphasis != 0x02 )
    {
        switch ( i_mode )
        {
        case 0: /* stereo */
        case 1: /* joint stereo */
            *pi_channels = 2;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
            break;
        case 2: /* dual-mono */
            *pi_channels = 2;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                                | AOUT_CHAN_DUALMONO;
            break;
        case 3: /* mono */
            *pi_channels = 1;
            *pi_channels_conf = AOUT_CHAN_CENTER;
            break;
        }
        *pi_bit_rate = ppi_bitrate[i_version][*pi_layer-1][i_bitrate_index];
        i_max_bit_rate = ppi_bitrate[i_version][*pi_layer-1][14];
        *pi_sample_rate = ppi_samplerate[i_version][i_samplerate_index];

        if ( b_mpeg_2_5 )
        {
            *pi_sample_rate >>= 1;
        }

        switch( *pi_layer )
        {
        case 1:
            i_frame_size = ( 12000 * *pi_bit_rate / *pi_sample_rate +
                           b_padding ) * 4;
            *pi_max_frame_size = ( 12000 * i_max_bit_rate /
                                 *pi_sample_rate + 1 ) * 4;
            *pi_frame_length = 384;
            break;

        case 2:
            i_frame_size = 144000 * *pi_bit_rate / *pi_sample_rate + b_padding;
            *pi_max_frame_size = 144000 * i_max_bit_rate / *pi_sample_rate + 1;
            *pi_frame_length = 1152;
            break;

        case 3:
            i_frame_size = ( i_version ? 72000 : 144000 ) *
                           *pi_bit_rate / *pi_sample_rate + b_padding;
            *pi_max_frame_size = ( i_version ? 72000 : 144000 ) *
                                 i_max_bit_rate / *pi_sample_rate + 1;
            *pi_frame_length = i_version ? 576 : 1152;
            break;

        default:
            break;
        }

        /* Free bitrate mode can support higher bitrates */
        if( !*pi_bit_rate ) *pi_max_frame_size *= 2;
    }
    else
    {
        return -1;
    }

    return i_frame_size;
}
