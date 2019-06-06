/*****************************************************************************
 * mpegaudio.c: parse MPEG audio sync info and packetize the stream
 *****************************************************************************
 * Copyright (C) 2001-2016 VLC authors and VideoLAN
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

#include "packetizer_helper.h"

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
typedef struct
{
    /*
     * Input properties
     */
    int        i_state;

    block_bytestream_t bytestream;

    /*
     * Common properties
     */
    date_t          end_date;

    vlc_tick_t i_pts;

    int i_frame_size, i_free_frame_size;
    unsigned int i_channels_conf, i_chan_mode, i_channels;
    unsigned int i_rate, i_max_frame_size, i_frame_length;
    unsigned int i_layer, i_bit_rate;

    bool   b_discontinuity;
} decoder_sys_t;

#define MAD_BUFFER_GUARD 8
#define MPGA_HEADER_SIZE 4

/****************************************************************************
 * Local prototypes
 ****************************************************************************/

static int  Open( vlc_object_t * );
static void Close(  vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )
    set_description( N_("MPEG audio layer I/II/III packetizer") )
    set_capability( "packetizer", 10 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Flush:
 *****************************************************************************/
static void Flush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    date_Set( &p_sys->end_date, VLC_TICK_INVALID );
    p_sys->i_state = STATE_NOSYNC;
    block_BytestreamEmpty( &p_sys->bytestream );
    p_sys->b_discontinuity = true;
}

/*****************************************************************************
 * GetOutBuffer:
 *****************************************************************************/
static uint8_t *GetOutBuffer( decoder_t *p_dec, block_t **pp_out_buffer )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_dec->fmt_out.audio.i_rate != p_sys->i_rate ||
        date_Get( &p_sys->end_date ) == VLC_TICK_INVALID )
    {
        msg_Dbg( p_dec, "MPGA channels:%d samplerate:%d bitrate:%d",
                  p_sys->i_channels, p_sys->i_rate, p_sys->i_bit_rate );

        if( p_sys->end_date.i_divider_num == 0 )
            date_Init( &p_sys->end_date, p_sys->i_rate, 1 );
        else
            date_Change( &p_sys->end_date, p_sys->i_rate, 1 );
        date_Set( &p_sys->end_date, p_sys->i_pts );
    }

    p_dec->fmt_out.i_profile        = p_sys->i_layer;
    p_dec->fmt_out.audio.i_rate     = p_sys->i_rate;
    p_dec->fmt_out.audio.i_channels = p_sys->i_channels;
    p_dec->fmt_out.audio.i_frame_length = p_sys->i_frame_length;
    p_dec->fmt_out.audio.i_bytes_per_frame = p_sys->i_max_frame_size;

    p_dec->fmt_out.audio.i_physical_channels = p_sys->i_channels_conf;
    p_dec->fmt_out.audio.i_chan_mode = p_sys->i_chan_mode;

    p_dec->fmt_out.i_bitrate = p_sys->i_bit_rate * 1000;

    block_t *p_block = block_Alloc( p_sys->i_frame_size );
    if( p_block == NULL )
        return NULL;

    p_block->i_pts = p_block->i_dts = date_Get( &p_sys->end_date );
    p_block->i_length =
        date_Increment( &p_sys->end_date, p_sys->i_frame_length ) - p_block->i_pts;

    *pp_out_buffer = p_block;
    return p_block->p_buffer;
}

/*****************************************************************************
 * SyncInfo: parse MPEG audio sync info
 *****************************************************************************/
static int SyncInfo( uint32_t i_header, unsigned int * pi_channels,
                     unsigned int * pi_channels_conf,
                     unsigned int * pi_chan_mode,
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
    *pi_chan_mode = 0;

    if( *pi_layer != 4 &&
        i_bitrate_index < 0x0f &&
        i_samplerate_index != 0x03 &&
        i_emphasis != 0x02 )
    {
        switch ( i_mode )
        {
        case 2: /* dual-mono */
            *pi_chan_mode = AOUT_CHANMODE_DUALMONO;
            /* fall through */
        case 0: /* stereo */
        case 1: /* joint stereo */
            *pi_channels = 2;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
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

    block_t *p_block = pp_block ? *pp_block : NULL;

    if (p_block)
    {
        if( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
        {
            /* First always drain complete blocks before discontinuity */
            block_t *p_drain = DecodeBlock( p_dec, NULL );
            if( p_drain )
                return p_drain;

            Flush( p_dec );

            if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
            {
                block_Release( p_block );
                return NULL;
            }
        }

        if( p_block->i_pts == VLC_TICK_INVALID &&
            date_Get( &p_sys->end_date ) == VLC_TICK_INVALID )
        {
            /* We've just started the stream, wait for the first PTS. */
            msg_Dbg( p_dec, "waiting for PTS" );
            block_Release( p_block );
            return NULL;
        }

        block_BytestreamPush( &p_sys->bytestream, p_block );
    }

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
            /* fallthrough */

        case STATE_SYNC:
            /* New frame, set the Presentation Time Stamp */
            p_sys->i_pts = p_sys->bytestream.p_block->i_pts;
            if( p_sys->i_pts != VLC_TICK_INVALID &&
                p_sys->i_pts != date_Get( &p_sys->end_date ) )
            {
                if( p_dec->fmt_in.i_original_fourcc == VLC_FOURCC( 'D','V','R',' ') )
                {
                    if( date_Get( &p_sys->end_date ) == VLC_TICK_INVALID )
                        date_Set( &p_sys->end_date, p_sys->i_pts );
                }
                else if ( p_sys->i_pts != date_Get( &p_sys->end_date ) )
                {
                    date_Set( &p_sys->end_date, p_sys->i_pts );
                }
            }
            p_sys->i_state = STATE_HEADER;
            /* fallthrough */

        case STATE_HEADER:
            /* Get MPGA frame header (MPGA_HEADER_SIZE bytes) */
            if( block_PeekBytes( &p_sys->bytestream, p_header,
                                 MPGA_HEADER_SIZE ) != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }

            /* Build frame header */
            i_header = GetDWBE(p_header);

            /* Check if frame is valid and get frame info */
            p_sys->i_frame_size = SyncInfo( i_header,
                                            &p_sys->i_channels,
                                            &p_sys->i_channels_conf,
                                            &p_sys->i_chan_mode,
                                            &p_sys->i_rate,
                                            &p_sys->i_bit_rate,
                                            &p_sys->i_frame_length,
                                            &p_sys->i_max_frame_size,
                                            &p_sys->i_layer );

            if( p_sys->i_frame_size == -1 )
            {
                msg_Dbg( p_dec, "emulated startcode" );
                block_SkipByte( &p_sys->bytestream );
                p_sys->i_state = STATE_NOSYNC;
                break;
            }

            if( p_sys->i_bit_rate == 0 )
            {
                /* Free bitrate, but 99% emulated startcode :( */
                if( p_sys->i_free_frame_size == MPGA_HEADER_SIZE )
                {
                    msg_Dbg( p_dec, "free bitrate mode");
                }
                /* The -1 below is to account for the frame padding */
                p_sys->i_frame_size = p_sys->i_free_frame_size - 1;
            }

            p_sys->i_state = STATE_NEXT_SYNC;
            /* fallthrough */

        case STATE_NEXT_SYNC:
            /* Check if next expected frame contains the sync word */
            if( block_PeekOffsetBytes( &p_sys->bytestream,
                                       p_sys->i_frame_size, p_header,
                                       MAD_BUFFER_GUARD ) != VLC_SUCCESS )
            {
                if( p_block == NULL ) /* drain */
                {
                    p_sys->i_state = STATE_SEND_DATA;
                    break;
                }
                /* Need more data */
                return NULL;
            }

            if( p_header[0] == 0xff && (p_header[1] & 0xe0) == 0xe0 )
            {
                /* Startcode is fine, let's try the header as an extra check */
                int i_next_frame_size;
                unsigned int i_next_channels, i_next_stereo_mode, i_next_channels_conf;
                unsigned int i_next_rate, i_next_bit_rate;
                unsigned int i_next_frame_length, i_next_max_frame_size;
                unsigned int i_next_layer;

                /* Build frame header */
                i_header = GetDWBE(p_header);

                i_next_frame_size = SyncInfo( i_header,
                                              &i_next_channels,
                                              &i_next_channels_conf,
                                              &i_next_stereo_mode,
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
                    break;
                }

                /* Check info is in sync with previous one */
                if( i_next_channels_conf != p_sys->i_channels_conf ||
                    i_next_stereo_mode != p_sys->i_chan_mode ||
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

            p_sys->i_state = STATE_GET_DATA;
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
            /* fallthrough */

        case STATE_SEND_DATA:
            if( !(p_buf = GetOutBuffer( p_dec, &p_out_buffer )) )
            {
                return NULL;
            }

            /* Free bitrate only */
            if( p_sys->i_bit_rate == 0 )
            {
                p_sys->i_free_frame_size = p_sys->i_frame_size;
            }

            /* Copy the whole frame into the buffer. */
            if (block_GetBytes( &p_sys->bytestream,
                            p_buf, __MIN( (unsigned)p_sys->i_frame_size, p_out_buffer->i_buffer ) )) {
                block_Release(p_out_buffer);
                return NULL;
            }

            p_sys->i_state = STATE_NOSYNC;

            /* Make sure we don't reuse the same pts twice */
            if( p_sys->i_pts == p_sys->bytestream.p_block->i_pts )
                p_sys->i_pts = p_sys->bytestream.p_block->i_pts = VLC_TICK_INVALID;

            if( p_sys->b_discontinuity )
            {
                p_out_buffer->i_flags |= BLOCK_FLAG_DISCONTINUITY;
                p_sys->b_discontinuity = false;
            }

            /* So p_block doesn't get re-added several times */
            p_block = block_BytestreamPop( &p_sys->bytestream );
            if (pp_block)
                *pp_block = p_block;
            else if (p_block)
                block_Release(p_block);

            return p_out_buffer;
        }
    }

    return NULL;
}

/*****************************************************************************
 * Close: clean up the decoder
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_BytestreamRelease( &p_sys->bytestream );

    free( p_sys );
}

/*****************************************************************************
 * Open: probe the decoder and return score
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if(( p_dec->fmt_in.i_codec != VLC_CODEC_MPGA ) &&
       ( p_dec->fmt_in.i_codec != VLC_CODEC_MP3 ) )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;

    /* Misc init */
    p_sys->i_state = STATE_NOSYNC;
    date_Init( &p_sys->end_date, 1, 1 );
    block_BytestreamInit( &p_sys->bytestream );
    p_sys->i_pts = VLC_TICK_INVALID;
    p_sys->b_discontinuity = false;
    p_sys->i_frame_size = 0;

    p_sys->i_channels_conf = p_sys->i_chan_mode = p_sys->i_channels =
    p_sys->i_rate = p_sys->i_max_frame_size = p_sys->i_frame_length =
    p_sys->i_layer = p_sys->i_bit_rate = 0;

    /* Set output properties */
    p_dec->fmt_out.i_codec = VLC_CODEC_MPGA;
    p_dec->fmt_out.audio.i_rate = 0; /* So end_date gets initialized */

    /* Set callback */
    p_dec->pf_packetize    = DecodeBlock;
    p_dec->pf_flush        = Flush;
    p_dec->pf_get_cc       = NULL;

    /* Start with the minimum size for a free bitrate frame */
    p_sys->i_free_frame_size = MPGA_HEADER_SIZE;

    return VLC_SUCCESS;
}
