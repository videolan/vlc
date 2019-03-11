/*****************************************************************************
 * dts.c: parse DTS audio sync info and packetize the stream
 *****************************************************************************
 * Copyright (C) 2001-2016 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Thomas Guillem <thomas@gllm.fr>
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
#include <vlc_block_helper.h>
#include <vlc_modules.h>

#include "dts_header.h"

#include "packetizer_helper.h"

static int  Open( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_PACKETIZER )
    set_description( N_("DTS audio packetizer") )
    set_capability( "packetizer", 10 )
    set_callbacks( Open, Close )
vlc_module_end ()

typedef struct
{
    /*
     * Input properties
     */
    int i_state;

    block_bytestream_t bytestream;
    size_t i_next_offset;

    /*
     * Common properties
     */
    date_t  end_date;
    bool    b_date_set;

    vlc_tick_t i_pts;
    bool    b_discontinuity;

    vlc_dts_header_t first, second;
    size_t  i_input_size;
} decoder_sys_t;

enum
{
    STATE_SYNC_SUBSTREAM_EXTENSIONS = STATE_CUSTOM_FIRST,
    STATE_NEXT_SYNC_SUBSTREAM_EXTENSIONS,
};

static void PacketizeFlush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    p_sys->b_discontinuity = true;
    date_Set( &p_sys->end_date, VLC_TICK_INVALID );
    p_sys->i_state = STATE_NOSYNC;
    block_BytestreamEmpty( &p_sys->bytestream );
}

static block_t *GetOutBuffer( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( !p_sys->b_date_set
     || p_dec->fmt_out.audio.i_rate != p_sys->first.i_rate )
    {
        msg_Dbg( p_dec, "DTS samplerate:%d bitrate:%d",
                 p_sys->first.i_rate, p_sys->first.i_bitrate );

        date_Init( &p_sys->end_date, p_sys->first.i_rate, 1 );
        date_Set( &p_sys->end_date, p_sys->i_pts );
        p_sys->b_date_set = true;
    }

    p_dec->fmt_out.audio.i_rate     = p_sys->first.i_rate;
    if( p_dec->fmt_out.audio.i_bytes_per_frame < p_sys->first.i_frame_size )
        p_dec->fmt_out.audio.i_bytes_per_frame = p_sys->first.i_frame_size;
    p_dec->fmt_out.audio.i_frame_length = p_sys->first.i_frame_length;

    p_dec->fmt_out.audio.i_chan_mode = p_sys->first.i_chan_mode;
    p_dec->fmt_out.audio.i_physical_channels = p_sys->first.i_physical_channels;
    p_dec->fmt_out.audio.i_channels =
        vlc_popcount( p_dec->fmt_out.audio.i_physical_channels );

    p_dec->fmt_out.i_bitrate = p_sys->first.i_bitrate;

    block_t *p_block = block_Alloc( p_sys->i_input_size );
    if( p_block == NULL )
        return NULL;

    p_block->i_nb_samples = p_sys->first.i_frame_length;
    p_block->i_pts = p_block->i_dts = date_Get( &p_sys->end_date );
    p_block->i_length =
        date_Increment( &p_sys->end_date, p_block->i_nb_samples ) - p_block->i_pts;
    return p_block;
}

static block_t *PacketizeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t p_header[VLC_DTS_HEADER_SIZE];
    block_t *p_out_buffer;

    block_t *p_block = pp_block ? *pp_block : NULL;

    if( p_block )
    {
        if ( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) ) {
            /* First always drain complete blocks before discontinuity */
            block_t *p_drain = PacketizeBlock( p_dec, NULL );
            if(p_drain)
                return p_drain;

            PacketizeFlush( p_dec );

            if ( p_block->i_flags & BLOCK_FLAG_CORRUPTED ) {
                block_Release( p_block );
                return NULL;
            }
        }

        if ( p_block->i_pts == VLC_TICK_INVALID &&
             date_Get( &p_sys->end_date ) == VLC_TICK_INVALID ) {
            /* We've just started the stream, wait for the first PTS. */
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
            while( block_PeekBytes( &p_sys->bytestream, p_header, 6 )
                   == VLC_SUCCESS )
            {
                if( vlc_dts_header_IsSync( p_header, 6 ) )
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
                date_Set( &p_sys->end_date, p_sys->i_pts );
            }
            p_sys->i_state = STATE_HEADER;
            /* fallthrough */

        case STATE_HEADER:
            /* Get DTS frame header (VLC_DTS_HEADER_SIZE bytes) */
            if( block_PeekBytes( &p_sys->bytestream, p_header,
                                 VLC_DTS_HEADER_SIZE ) != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }

            /* Check if frame is valid and get frame info */
            if( vlc_dts_header_Parse( &p_sys->first, p_header,
                                      VLC_DTS_HEADER_SIZE ) != VLC_SUCCESS )
            {
                msg_Dbg( p_dec, "emulated sync word" );
                block_SkipByte( &p_sys->bytestream );
                p_sys->i_state = STATE_NOSYNC;
                break;
            }

            if( p_sys->first.syncword == DTS_SYNC_SUBSTREAM )
                p_sys->i_state = STATE_SYNC_SUBSTREAM_EXTENSIONS;
            else
                p_sys->i_state = STATE_NEXT_SYNC;
            p_sys->i_input_size = p_sys->i_next_offset = p_sys->first.i_frame_size;
            break;

        case STATE_SYNC_SUBSTREAM_EXTENSIONS:
            /* Peek into the substream extension (sync + header size < frame_size) */
            if( block_PeekOffsetBytes( &p_sys->bytestream,
                                       p_sys->first.i_substream_header_size,
                                       p_header,
                                       VLC_DTS_HEADER_SIZE ) != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }

            vlc_dts_header_t xssheader;
            if( vlc_dts_header_Parse( &xssheader, p_header,
                                      VLC_DTS_HEADER_SIZE ) != VLC_SUCCESS )
            {
                msg_Dbg( p_dec, "emulated substream sync word, can't find extension" );
                block_SkipByte( &p_sys->bytestream );
                p_sys->i_state = STATE_NOSYNC;
                break;
            }

            if( xssheader.syncword == DTS_SYNC_SUBSTREAM_LBR )
            {
                /*
                 * LBR exists as independant SUBSTREAM. It is seen valid
                 * only when SUBSTREAM[LBR]..SUBTREAM.
                 * CORE...SUBSTREAM is regular extension.
                 * SUBSTREAM...CORE is sync issue.
                 */
                p_dec->fmt_out.i_profile = PROFILE_DTS_EXPRESS;
                p_sys->first.i_rate = xssheader.i_rate;
                p_sys->first.i_frame_length = xssheader.i_frame_length;
                p_sys->i_state = STATE_NEXT_SYNC;
                break;
            }

            msg_Warn( p_dec, "substream without the paired core stream, skip it" );
            p_sys->i_state = STATE_NOSYNC;
            p_dec->fmt_out.i_profile = PROFILE_DTS;
            if( block_SkipBytes( &p_sys->bytestream,
                                 p_sys->first.i_frame_size ) != VLC_SUCCESS )
                return NULL;
            break;

        case STATE_NEXT_SYNC:
            /* Check if next expected frame contains the sync word */
            while( p_sys->i_state == STATE_NEXT_SYNC )
            {
                if( block_PeekOffsetBytes( &p_sys->bytestream,
                                           p_sys->i_next_offset, p_header,
                                           VLC_DTS_HEADER_SIZE )
                                           != VLC_SUCCESS )
                {
                    if( p_block == NULL ) /* drain */
                    {
                        p_sys->i_state = STATE_GET_DATA;
                        break;
                    }
                    /* Need more data */
                    return NULL;
                }

                if( p_header[0] == 0 )
                {
                    /* DTS wav files, audio CD's and some mkvs use stuffing */
                    p_sys->i_next_offset++;
                    continue;
                }

                if( !vlc_dts_header_IsSync( p_header, VLC_DTS_HEADER_SIZE ) )
                {
                    /* Even frame size is likely incorrect FSIZE #18166 */
                    if( (p_sys->first.i_frame_size % 2) && p_sys->i_next_offset > 0 &&
                        block_PeekOffsetBytes( &p_sys->bytestream,
                                               p_sys->i_next_offset - 1, p_header,
                                               VLC_DTS_HEADER_SIZE ) == 0 &&
                         vlc_dts_header_IsSync( p_header, VLC_DTS_HEADER_SIZE ) )
                    {
                        p_sys->i_input_size = p_sys->i_next_offset = p_sys->first.i_frame_size - 1;
                        /* reenter */
                        break;
                    }
                    msg_Dbg( p_dec, "emulated sync word "
                             "(no sync on following frame)" );
                    p_sys->i_state = STATE_NOSYNC;
                    block_SkipByte( &p_sys->bytestream );
                    break;
                }

                /* Check if a DTS substream packet is located just after
                 * the core packet */
                if( p_sys->i_next_offset == p_sys->first.i_frame_size &&
                    vlc_dts_header_Parse( &p_sys->second,
                                          p_header, VLC_DTS_HEADER_SIZE ) == VLC_SUCCESS &&
                    p_sys->second.syncword == DTS_SYNC_SUBSTREAM )
                {
                    p_sys->i_state = STATE_NEXT_SYNC_SUBSTREAM_EXTENSIONS;
                }
                else
                {
                    p_dec->fmt_out.i_profile = PROFILE_DTS;
                    p_sys->i_state = STATE_GET_DATA;
                }
            }
            break;

        case STATE_NEXT_SYNC_SUBSTREAM_EXTENSIONS:
            assert(p_sys->second.syncword == DTS_SYNC_SUBSTREAM);
            if( p_sys->first.syncword == DTS_SYNC_SUBSTREAM )
            {
                /* First substream must have been LBR */
                p_dec->fmt_out.i_profile = PROFILE_DTS_EXPRESS;
            }
            else /* Otherwise that's core + extensions, we need to output both */
            {
                p_dec->fmt_out.i_profile = PROFILE_DTS_HD;
                p_sys->i_input_size += p_sys->second.i_frame_size;
            }
            p_sys->i_state = STATE_GET_DATA;
            break;

        case STATE_GET_DATA:
            /* Make sure we have enough data. */
            if( block_WaitBytes( &p_sys->bytestream,
                                 p_sys->i_input_size ) != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }
            p_sys->i_state = STATE_SEND_DATA;
            /* fallthrough */

        case STATE_SEND_DATA:
            if( !(p_out_buffer = GetOutBuffer( p_dec )) )
            {
                return NULL;
            }

            /* Copy the whole frame into the buffer. When we reach this point
             * we already know we have enough data available. */
            block_GetBytes( &p_sys->bytestream, p_out_buffer->p_buffer,
                            p_out_buffer->i_buffer );

            /* Make sure we don't reuse the same pts twice */
            if( p_sys->i_pts == p_sys->bytestream.p_block->i_pts )
                p_sys->i_pts = p_sys->bytestream.p_block->i_pts = VLC_TICK_INVALID;

            if( p_sys->b_discontinuity )
            {
                p_sys->b_discontinuity = false;
                p_out_buffer->i_flags |= BLOCK_FLAG_DISCONTINUITY;
            }

            /* So p_block doesn't get re-added several times */
            if( pp_block )
                *pp_block = block_BytestreamPop( &p_sys->bytestream );

            p_sys->i_state = STATE_NOSYNC;

            return p_out_buffer;
        }
    }
}

static void Close( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_BytestreamRelease( &p_sys->bytestream );

    free( p_sys );
}

static int Open( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_DTS )
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = malloc(sizeof(decoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;

    /* Misc init */
    p_sys->i_state = STATE_NOSYNC;
    date_Set( &p_sys->end_date, VLC_TICK_INVALID );
    p_sys->i_pts = VLC_TICK_INVALID;
    p_sys->b_date_set = false;
    p_sys->b_discontinuity = false;
    memset(&p_sys->first, 0, sizeof(vlc_dts_header_t));
    memset(&p_sys->second, 0, sizeof(vlc_dts_header_t));
    block_BytestreamInit( &p_sys->bytestream );

    /* Set output properties (passthrough only) */
    p_dec->fmt_out.i_codec = p_dec->fmt_in.i_codec;
    p_dec->fmt_out.audio = p_dec->fmt_in.audio;

    /* Set callback */
    p_dec->pf_packetize = PacketizeBlock;
    p_dec->pf_flush     = PacketizeFlush;
    p_dec->pf_get_cc    = NULL;
    return VLC_SUCCESS;
}
