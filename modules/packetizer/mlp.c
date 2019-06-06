/*****************************************************************************
 * mlp.c: packetize MLP/TrueHD audio
 *****************************************************************************
 * Copyright (C) 2008 Laurent Aimar
 *
 * Authors: Laurent Aimar < fenrir _AT videolan _DOT_ org >
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
#include <vlc_bits.h>
#include <assert.h>

#include "packetizer_helper.h"
#include "a52.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_PACKETIZER )
    set_description( N_("MLP/TrueHD parser") )
    set_capability( "packetizer", 50 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 *
 *****************************************************************************/
typedef struct
{
    int i_type;
    unsigned i_rate;
    unsigned i_channels;
    int i_channels_conf;
    unsigned i_samples;

    bool b_vbr;
    unsigned  i_bitrate;

    unsigned  i_substreams;

} mlp_header_t;

typedef struct
{
    /*
     * Input properties
     */
    int i_state;

    block_bytestream_t bytestream;

    /*
     * Common properties
     */
    date_t  end_date;
    bool    b_discontinuity;

    vlc_tick_t i_pts;
    int i_frame_size;

    bool         b_mlp;
    mlp_header_t mlp;
} decoder_sys_t;

#define MLP_MAX_SUBSTREAMS (16)
#define MLP_HEADER_SYNC (28)
#define MLP_HEADER_SIZE (4 + MLP_HEADER_SYNC + 4 * MLP_MAX_SUBSTREAMS)

static const uint8_t pu_start_code[3] = { 0xf8, 0x72, 0x6f };

/**
 * It parse MLP sync info.
 *
 * TODO handle CRC (at offset 26)
 */
static int TrueHdChannels( int i_map )
{
    static const uint8_t pu_thd[13] =
    {
         2, 1, 1, 2, 2, 2, 2, 1, 1, 2, 2, 1, 1
    };
    int i_count = 0;

    for( int i = 0; i < 13; i++ )
    {
        if( i_map & (1<<i) )
            i_count += pu_thd[i];
    }
    return i_count;
}

static int MlpParse( mlp_header_t *p_mlp, const uint8_t p_hdr[MLP_HEADER_SYNC] )
{
    bs_t s;

    assert( !memcmp( p_hdr, pu_start_code, 3 ) );

    /* TODO Checksum ? */

    /* */
    bs_init( &s, &p_hdr[3], MLP_HEADER_SYNC - 3 );

    /* Stream type */
    p_mlp->i_type = bs_read( &s, 8 );
    int i_rate_idx1;

    if( p_mlp->i_type == 0xbb )        /* MLP */
    {
        static const unsigned pu_channels[32] = {
            1, 2, 3, 4, 3, 4, 5, 3, 4, 5, 4, 5, 6, 4, 5, 4,
            5, 6, 5, 5, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        };

        bs_skip( &s, 4 + 4 );

        i_rate_idx1 = bs_read( &s, 4 );

        // Just skip the 4 following, since we don't use it
        // const int i_rate_idx2 = bs_read( &s, 4 );
        bs_skip( &s, 4 );

        bs_skip( &s, 11 );

        const int i_channel_idx = bs_read( &s, 5 );
        p_mlp->i_channels = pu_channels[i_channel_idx];
    }
    else if( p_mlp->i_type == 0xba )   /* True HD */
    {
        i_rate_idx1 = bs_read( &s, 4 );

        bs_skip( &s, 8 );

        const int i_channel1 = bs_read( &s, 5 );

        bs_skip( &s, 2 );

        const int i_channel2 = bs_read( &s, 13 );
        if( i_channel2 )
            p_mlp->i_channels = TrueHdChannels( i_channel2 );
        else
            p_mlp->i_channels = TrueHdChannels( i_channel1 );
    }
    else
    {
        return VLC_EGENERIC;
    }

    if( i_rate_idx1 == 0x0f )
        p_mlp->i_rate = 0;
    else
        p_mlp->i_rate = ( ( i_rate_idx1 & 0x8 ) ? 44100 : 48000 ) << (i_rate_idx1 & 0x7);
    p_mlp->i_channels_conf = 0; /* TODO ? */

    p_mlp->i_samples = 40 << ( i_rate_idx1 & 0x07 );

    bs_skip( &s, 48 );

    p_mlp->b_vbr = bs_read( &s, 1 );
    p_mlp->i_bitrate = ( bs_read( &s, 15 ) * p_mlp->i_rate + 8) / 16;

    p_mlp->i_substreams = bs_read( &s, 4 );
    bs_skip( &s, 4 + 11 * 8 );

    //fprintf( stderr, "i_samples = %d channels:%d rate:%d bitsrate=%d substreams=%d\n",
    //        p_mlp->i_samples, p_mlp->i_channels, p_mlp->i_rate, p_mlp->i_bitrate, p_mlp->i_substreams );
    return VLC_SUCCESS;
}

static int SyncInfo( const uint8_t *p_hdr, bool *pb_mlp, mlp_header_t *p_mlp )
{
    /* Check major sync presence */
    const bool b_has_sync = !memcmp( &p_hdr[4], pu_start_code, 3 );

    /* Wait for a major sync */
    if( !b_has_sync && !*pb_mlp )
        return 0;

    /* Parse major sync if present */
    if( b_has_sync )
    {
        *pb_mlp = !MlpParse( p_mlp, &p_hdr[4] );

        if( !*pb_mlp )
            return 0;
    }

    if( !b_has_sync )
    {
        int i_tmp = 0 ^ p_hdr[0] ^ p_hdr[1] ^ p_hdr[2] ^ p_hdr[3];
        const uint8_t *p = &p_hdr[4];

        for( unsigned i = 0; i < p_mlp->i_substreams; i++ )
        {
            i_tmp ^= *p++;
            i_tmp ^= *p++;
            if( p[-2] & 0x80 )
            {
                i_tmp ^= *p++;
                i_tmp ^= *p++;
            }
        }
        i_tmp = ( i_tmp >> 4 ) ^ i_tmp;

        if( ( i_tmp & 0x0f ) != 0x0f )
            return 0;
    }

    /* */
    const int i_word = ( ( p_hdr[0] << 8 ) | p_hdr[1] ) & 0xfff;
    return i_word * 2;
}

/**
 * It returns the size of an AC3/EAC3 frame (or 0 if invalid)
 */
static int SyncInfoDolby( const uint8_t *p_buf )
{
    vlc_a52_header_t a52;
    if( vlc_a52_header_Parse( &a52, p_buf, MLP_HEADER_SIZE ) == VLC_SUCCESS )
        return a52.i_size;
    else
        return 0;
}

static void Flush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    p_sys->b_mlp = false;
    p_sys->i_state = STATE_NOSYNC;
    p_sys->b_discontinuity = true;
    block_BytestreamEmpty( &p_sys->bytestream );
    date_Set( &p_sys->end_date, VLC_TICK_INVALID );
}

static block_t *Packetize( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t p_header[MLP_HEADER_SIZE];
    block_t *p_out_buffer;

    block_t *p_block = pp_block ? *pp_block : NULL;

    if ( p_block )
    {
        if( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
        {
            /* First always drain complete blocks before discontinuity */
            block_t *p_drain = Packetize( p_dec, NULL );
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

    for( ;; )
    {
        switch( p_sys->i_state )
        {
        case STATE_NOSYNC:
            while( !block_PeekBytes( &p_sys->bytestream, p_header, MLP_HEADER_SIZE ) )
            {
                if( SyncInfo( p_header, &p_sys->b_mlp, &p_sys->mlp ) > 0 )
                {
                    p_sys->i_state = STATE_SYNC;
                    break;
                }
                else if( SyncInfoDolby( p_header ) > 0 )
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
            /* Get a MLP header */
            if( block_PeekBytes( &p_sys->bytestream, p_header, MLP_HEADER_SIZE ) )
            {
                /* Need more data */
                return NULL;
            }

            /* Check if frame is valid and get frame info */
            p_sys->i_frame_size = SyncInfoDolby( p_header );
            if( p_sys->i_frame_size <= 0 )
                p_sys->i_frame_size = SyncInfo( p_header, &p_sys->b_mlp, &p_sys->mlp );
            if( p_sys->i_frame_size <= 0 )
            {
                msg_Dbg( p_dec, "emulated sync word" );
                block_SkipByte( &p_sys->bytestream );
                p_sys->b_mlp = false;
                p_sys->i_state = STATE_NOSYNC;
                break;
            }
            p_sys->i_state = STATE_NEXT_SYNC;
            /* fallthrough */

        case STATE_NEXT_SYNC:
            /* Check if next expected frame contains the sync word */
            if( block_PeekOffsetBytes( &p_sys->bytestream,
                                       p_sys->i_frame_size, p_header, MLP_HEADER_SIZE ) )
            {
                if( p_block == NULL ) /* drain */
                {
                    p_sys->i_state = STATE_GET_DATA;
                    break;
                }
                /* Need more data */
                return NULL;
            }

            bool b_mlp = p_sys->b_mlp;
            mlp_header_t mlp = p_sys->mlp;
            if( SyncInfo( p_header, &b_mlp, &mlp ) <= 0 && SyncInfoDolby( p_header ) <= 0 )
            {
                msg_Dbg( p_dec, "emulated sync word "
                         "(no sync on following frame)" );
                p_sys->b_mlp = false;
                p_sys->i_state = STATE_NOSYNC;
                block_SkipByte( &p_sys->bytestream );
                break;
            }
            p_sys->i_state = STATE_GET_DATA;
            break;

        case STATE_GET_DATA:
            /* Make sure we have enough data. */
            if( block_WaitBytes( &p_sys->bytestream, p_sys->i_frame_size ) )
            {
                /* Need more data */
                return NULL;
            }
            p_sys->i_state = STATE_SEND_DATA;
            /* fallthrough */

        case STATE_SEND_DATA:
            /* When we reach this point we already know we have enough
             * data available. */
            p_out_buffer = block_Alloc( p_sys->i_frame_size );
            if( !p_out_buffer )
                return NULL;

            /* Copy the whole frame into the buffer */
            block_GetBytes( &p_sys->bytestream,
                            p_out_buffer->p_buffer, p_out_buffer->i_buffer );

            /* Just ignore (E)AC3 frames */
            if( SyncInfoDolby( p_out_buffer->p_buffer ) > 0 )
            {
                block_Release( p_out_buffer );
                p_sys->i_state = STATE_NOSYNC;
                break;
            }

            /* Setup output */
            if( p_dec->fmt_out.audio.i_rate != p_sys->mlp.i_rate )
            {
                msg_Info( p_dec, "MLP channels: %d samplerate: %d",
                          p_sys->mlp.i_channels, p_sys->mlp.i_rate );

                if( p_sys->mlp.i_rate > 0 )
                    date_Change( &p_sys->end_date, p_sys->mlp.i_rate, 1 );
            }

            p_dec->fmt_out.audio.i_rate     = p_sys->mlp.i_rate;
            p_dec->fmt_out.audio.i_channels = p_sys->mlp.i_channels;
            p_dec->fmt_out.audio.i_physical_channels = p_sys->mlp.i_channels_conf;
            p_dec->fmt_out.audio.i_bytes_per_frame = p_sys->i_frame_size;
            p_dec->fmt_out.audio.i_frame_length = p_sys->mlp.i_samples;

            p_out_buffer->i_pts = p_out_buffer->i_dts = date_Get( &p_sys->end_date );
            p_out_buffer->i_nb_samples = p_sys->mlp.i_samples;

            p_out_buffer->i_length =
                date_Increment( &p_sys->end_date, p_sys->mlp.i_samples ) - p_out_buffer->i_pts;

            /* Make sure we don't reuse the same pts twice */
            if( p_sys->i_pts == p_sys->bytestream.p_block->i_pts )
                p_sys->i_pts = p_sys->bytestream.p_block->i_pts = VLC_TICK_INVALID;

            if( p_sys->b_discontinuity )
            {
                p_out_buffer->i_flags |= BLOCK_FLAG_DISCONTINUITY;
                p_sys->b_discontinuity = false;
            }

            /* So p_block doesn't get re-added several times */
            if( pp_block )
                *pp_block = block_BytestreamPop( &p_sys->bytestream );

            p_sys->i_state = STATE_NOSYNC;

            return p_out_buffer;
        }
    }

    return NULL;
}

static int Open( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_MLP &&
        p_dec->fmt_in.i_codec != VLC_CODEC_TRUEHD )
        return VLC_EGENERIC;

    /* */
    p_dec->p_sys = p_sys = malloc( sizeof(*p_sys) );
    if( !p_sys )
        return VLC_ENOMEM;

    /* */
    p_sys->i_state = STATE_NOSYNC;
    date_Init( &p_sys->end_date, 1, 1 );

    block_BytestreamInit( &p_sys->bytestream );
    p_sys->b_mlp = false;
    p_sys->b_discontinuity = false;

    /* Set output properties (Passthrough only) */
    p_dec->fmt_out.i_codec = p_dec->fmt_in.i_codec;
    p_dec->fmt_out.audio.i_rate = 0;

    /* Set callback */
    p_dec->pf_packetize = Packetize;
    p_dec->pf_flush     = Flush;
    p_dec->pf_get_cc    = NULL;
    return VLC_SUCCESS;
}

static void Close( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_BytestreamRelease( &p_sys->bytestream );

    free( p_sys );
}
