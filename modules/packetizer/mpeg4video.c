/*****************************************************************************
 * mpeg4video.c: mpeg 4 video packetizer
 *****************************************************************************
 * Copyright (C) 2001-2006 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
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
#include <vlc_sout.h>
#include <vlc_codec.h>
#include <vlc_block.h>

#include <vlc_bits.h>
#include <vlc_block_helper.h>
#include "packetizer_helper.h"
#include "startcode_helper.h"
#include "iso_color_tables.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_PACKETIZER )
    set_description( N_("MPEG4 video packetizer") )
    set_capability( "packetizer", 50 )
    set_callbacks( Open, Close )
vlc_module_end ()

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
typedef struct
{
    /*
     * Input properties
     */
    packetizer_t packetizer;

    /*
     * Common properties
     */
    vlc_tick_t i_interpolated_pts;
    vlc_tick_t i_interpolated_dts;
    vlc_tick_t i_last_ref_pts;
    int64_t i_last_time_ref;
    int64_t i_time_ref;
    int64_t i_last_time;
    int64_t i_last_timeincr;

    unsigned int i_flags;

    int         i_fps_num;
    int         i_fps_den;

    bool  b_frame;

    /* Current frame being built */
    block_t    *p_frame;
    block_t    **pp_last;
} decoder_sys_t;

static block_t *Packetize( decoder_t *, block_t ** );
static void PacketizeFlush( decoder_t * );

static void PacketizeReset( void *p_private, bool b_broken );
static block_t *PacketizeParse( void *p_private, bool *pb_ts_used, block_t * );
static int PacketizeValidate( void *p_private, block_t * );

static block_t *ParseMPEGBlock( decoder_t *, block_t * );
static int ParseVOL( decoder_t *, es_format_t *, uint8_t *, int );
static int ParseVO( decoder_t *, block_t * );
static int ParseVOP( decoder_t *, block_t * );
static int vlc_log2( unsigned int );

#define VIDEO_OBJECT_MASK                       0x01f
#define VIDEO_OBJECT_LAYER_MASK                 0x00f

#define VIDEO_OBJECT_START_CODE                 0x100
#define VIDEO_OBJECT_LAYER_START_CODE           0x120
#define RESERVED_START_CODE                     0x130
#define VISUAL_OBJECT_SEQUENCE_START_CODE       0x1b0
#define VISUAL_OBJECT_SEQUENCE_END_CODE         0x1b1
#define USER_DATA_START_CODE                    0x1b2
#define GROUP_OF_VOP_START_CODE                 0x1b3
#define VIDEO_SESSION_ERROR_CODE                0x1b4
#define VISUAL_OBJECT_START_CODE                0x1b5
#define VOP_START_CODE                          0x1b6
#define FACE_OBJECT_START_CODE                  0x1ba
#define FACE_OBJECT_PLANE_START_CODE            0x1bb
#define MESH_OBJECT_START_CODE                  0x1bc
#define MESH_OBJECT_PLANE_START_CODE            0x1bd
#define STILL_TEXTURE_OBJECT_START_CODE         0x1be
#define TEXTURE_SPATIAL_LAYER_START_CODE        0x1bf
#define TEXTURE_SNR_LAYER_START_CODE            0x1c0

static const uint8_t p_mp4v_startcode[3] = { 0x00, 0x00, 0x01 };

/*****************************************************************************
 * Open: probe the packetizer and return score
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_MP4V )
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = malloc( sizeof(decoder_sys_t) ) ) == NULL )
        return VLC_ENOMEM;
    memset( p_sys, 0, sizeof(decoder_sys_t) );

    /* Misc init */
    packetizer_Init( &p_sys->packetizer,
                     p_mp4v_startcode, sizeof(p_mp4v_startcode), startcode_FindAnnexB,
                     NULL, 0, 4,
                     PacketizeReset, PacketizeParse, PacketizeValidate, NULL,
                     p_dec );

    p_sys->p_frame = NULL;
    p_sys->pp_last = &p_sys->p_frame;

    /* Setup properties */
    es_format_Copy( &p_dec->fmt_out, &p_dec->fmt_in );
    p_dec->fmt_out.i_codec = VLC_CODEC_MP4V;

    if( p_dec->fmt_out.i_extra )
    {
        /* We have a vol */
        msg_Dbg( p_dec, "opening with vol size: %d", p_dec->fmt_out.i_extra );
        ParseVOL( p_dec, &p_dec->fmt_out,
                  p_dec->fmt_out.p_extra, p_dec->fmt_out.i_extra );
    }

    /* Set callback */
    p_dec->pf_packetize = Packetize;
    p_dec->pf_flush = PacketizeFlush;
    p_dec->pf_get_cc = NULL;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: clean up the packetizer
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    packetizer_Clean( &p_sys->packetizer );
    if( p_sys->p_frame )
        block_ChainRelease( p_sys->p_frame );
    free( p_sys );
}

/****************************************************************************
 * Packetize: the whole thing
 ****************************************************************************/
static block_t *Packetize( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    return packetizer_Packetize( &p_sys->packetizer, pp_block );
}

static void PacketizeFlush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    packetizer_Flush( &p_sys->packetizer );
}

/*****************************************************************************
 * Helpers:
 *****************************************************************************/
static void PacketizeReset( void *p_private, bool b_flush )
{
    decoder_t *p_dec = p_private;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( b_flush )
    {
        if( p_sys->p_frame )
            block_ChainRelease( p_sys->p_frame );
        p_sys->p_frame = NULL;
        p_sys->pp_last = &p_sys->p_frame;
    }

    p_sys->i_interpolated_pts =
    p_sys->i_interpolated_dts =
    p_sys->i_last_ref_pts = VLC_TICK_INVALID;

    p_sys->i_last_time_ref =
    p_sys->i_time_ref =
    p_sys->i_last_time =
    p_sys->i_last_timeincr = 0;
}

static block_t *PacketizeParse( void *p_private, bool *pb_ts_used, block_t *p_block )
{
    decoder_t *p_dec = p_private;
    const vlc_tick_t i_dts = p_block->i_dts;
    const vlc_tick_t i_pts = p_block->i_pts;

    block_t *p_au = ParseMPEGBlock( p_dec, p_block );

    *pb_ts_used = p_au &&  p_au->i_dts == i_dts && p_au->i_pts == i_pts;

    return p_au;
}


static int PacketizeValidate( void *p_private, block_t *p_au )
{
    decoder_t *p_dec = p_private;
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* We've just started the stream, wait for the first PTS.
     * We discard here so we can still get the sequence header. */
    if( p_sys->i_interpolated_pts == VLC_TICK_INVALID &&
        p_sys->i_interpolated_dts == VLC_TICK_INVALID )
    {
        msg_Dbg( p_dec, "need a starting pts/dts" );
        return VLC_EGENERIC;
    }

    /* When starting the stream we can have the first frame with
     * a null DTS (i_interpolated_pts is initialized to 0) */
    if( p_au->i_dts == VLC_TICK_INVALID )
        p_au->i_dts = p_au->i_pts;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * ParseMPEGBlock: Re-assemble fragments into a block containing a picture
 *****************************************************************************/
static block_t *ParseMPEGBlock( decoder_t *p_dec, block_t *p_frag )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_pic = NULL;

    if( p_frag->i_buffer < 4 )
        return p_frag;

    const uint32_t i_startcode = GetDWBE( p_frag->p_buffer );
    if( i_startcode == VISUAL_OBJECT_SEQUENCE_START_CODE ||
        i_startcode == VISUAL_OBJECT_SEQUENCE_END_CODE ||
        i_startcode == USER_DATA_START_CODE )
    {   /* VOS and USERDATA */
#if 0
        /* Remove VOS start/end code from the original stream */
        block_Release( p_frag );
#else
        /* Append the block for now since ts/ps muxers rely on VOL
         * being present in the stream */
        block_ChainLastAppend( &p_sys->pp_last, p_frag );
#endif
        return NULL;
    }
    else if( i_startcode >= VIDEO_OBJECT_LAYER_START_CODE &&
             i_startcode < RESERVED_START_CODE )
    {
        /* Copy the complete VOL */
        if( (size_t)p_dec->fmt_out.i_extra != p_frag->i_buffer )
        {
            p_dec->fmt_out.p_extra =
                xrealloc( p_dec->fmt_out.p_extra, p_frag->i_buffer );
            p_dec->fmt_out.i_extra = p_frag->i_buffer;
        }
        memcpy( p_dec->fmt_out.p_extra, p_frag->p_buffer, p_frag->i_buffer );
        ParseVOL( p_dec, &p_dec->fmt_out,
                  p_dec->fmt_out.p_extra, p_dec->fmt_out.i_extra );

#if 0
        /* Remove from the original stream */
        block_Release( p_frag );
#else
        /* Append the block for now since ts/ps muxers rely on VOL
         * being present in the stream */
        block_ChainLastAppend( &p_sys->pp_last, p_frag );
#endif
        return NULL;
    }
    else
    {
        if( !p_dec->fmt_out.i_extra )
        {
            msg_Warn( p_dec, "waiting for VOL" );
            block_Release( p_frag );
            return NULL;
        }

        /* Append the block */
        block_ChainLastAppend( &p_sys->pp_last, p_frag );
    }

    if( i_startcode == VISUAL_OBJECT_START_CODE )
    {
        ParseVO( p_dec, p_frag );
    }
    else
    if( i_startcode == VOP_START_CODE &&
        ParseVOP( p_dec, p_frag ) == VLC_SUCCESS )
    {
        /* We are dealing with a VOP */
        p_pic = block_ChainGather( p_sys->p_frame );
        p_pic->i_flags = p_sys->i_flags;
        p_pic->i_pts = p_sys->i_interpolated_pts;
        p_pic->i_dts = p_sys->i_interpolated_dts;

#if 0
    msg_Err( p_dec, "output dts/pts (%"PRId64",%"PRId64")", p_pic->i_dts, p_pic->i_pts );
#endif
        /* Reset context */
        p_sys->p_frame = NULL;
        p_sys->pp_last = &p_sys->p_frame;
    }

    return p_pic;
}

/* ParseVOL:
 *  TODO:
 *      - support aspect ratio
 */
static int ParseVOL( decoder_t *p_dec, es_format_t *fmt,
                     uint8_t *p_vol, int i_vol )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_vo_ver_id, i_ar, i_shape;
    bs_t s;

    for( ;; )
    {
        if( i_vol <= 5 )
            return VLC_EGENERIC;

        if( p_vol[0] == 0x00 && p_vol[1] == 0x00 && p_vol[2] == 0x01 &&
            p_vol[3] >= 0x20 && p_vol[3] <= 0x2f ) break;

        p_vol++; i_vol--;
    }

    bs_init( &s, &p_vol[4], i_vol - 4 );

    bs_skip( &s, 1 );   /* random access */
    bs_skip( &s, 8 );   /* vo_type */
    if( bs_read1( &s ) )
    {
        i_vo_ver_id = bs_read( &s, 4 );
        bs_skip( &s, 3 );
    }
    else
    {
        i_vo_ver_id = 1;
    }
    i_ar = bs_read( &s, 4 );
    if( i_ar == 0xf )
    {
        bs_skip( &s, 8 );  /* ar_width */
        bs_skip( &s, 8 );  /* ar_height */
    }
    if( bs_read1( &s ) )
    {
        /* vol control parameter */
        bs_skip( &s, 2 ); /* chroma_format */
        bs_read1( &s ); /* low_delay */

        if( bs_read1( &s ) )
        {
            bs_skip( &s, 16 );
            bs_skip( &s, 16 );
            bs_skip( &s, 16 );
            bs_skip( &s, 3 );
            bs_skip( &s, 11 );
            bs_skip( &s, 1 );
            bs_skip( &s, 16 );
        }
    }
    /* shape 0->RECT, 1->BIN, 2->BIN_ONLY, 3->GRAY */
    i_shape = bs_read( &s, 2 );
    if( i_shape == 3 && i_vo_ver_id != 1 )
    {
        bs_skip( &s, 4 );
    }

    if( !bs_read1( &s ) ) return VLC_EGENERIC; /* Marker */

    p_sys->i_fps_num = bs_read( &s, 16 ); /* Time increment resolution*/
    if( !p_sys->i_fps_num ) p_sys->i_fps_num = 1;

    if( !bs_read1( &s ) ) return VLC_EGENERIC; /* Marker */

    if( bs_read1( &s ) )
    {
        int i_time_increment_bits = vlc_log2( p_sys->i_fps_num - 1 ) + 1;

        if( i_time_increment_bits < 1 ) i_time_increment_bits = 1;

        p_sys->i_fps_den = bs_read( &s, i_time_increment_bits );
    }
    if( i_shape == 0 )
    {
        bs_skip( &s, 1 );
        fmt->video.i_width = bs_read( &s, 13 );
        bs_skip( &s, 1 );
        fmt->video.i_height= bs_read( &s, 13 );
        bs_skip( &s, 1 );
    }

    return VLC_SUCCESS;
}

static int ParseVO( decoder_t *p_dec, block_t *p_vo )
{
    bs_t s;
    bs_init( &s, &p_vo->p_buffer[4], p_vo->i_buffer - 4 );
    if( bs_read1( &s ) )
        bs_skip( &s, 7 );

    const uint8_t visual_object_type = bs_read( &s, 4 );
    if( visual_object_type == 1 /* video ID */ ||
        visual_object_type == 2 /* still texture ID */ )
    {
        uint8_t colour_primaries = 1;
        uint8_t colour_xfer = 1;
        uint8_t colour_matrix_coeff = 1;
        uint8_t full_range = 0;
        if( bs_read1( &s ) ) /* video_signal_type */
        {
            bs_read( &s, 3 );
            full_range = bs_read( &s, 1 );
            if( bs_read( &s, 1 ) ) /* colour description */
            {
                colour_primaries = bs_read( &s, 8 );
                colour_xfer = bs_read( &s, 8 );
                colour_matrix_coeff = bs_read( &s, 8 );
            }
        }

        if( p_dec->fmt_in.video.primaries == COLOR_PRIMARIES_UNDEF )
        {
            p_dec->fmt_out.video.primaries = iso_23001_8_cp_to_vlc_primaries( colour_primaries );
            p_dec->fmt_out.video.transfer = iso_23001_8_tc_to_vlc_xfer( colour_xfer );
            p_dec->fmt_out.video.space = iso_23001_8_mc_to_vlc_coeffs( colour_matrix_coeff );
            p_dec->fmt_out.video.color_range = full_range ? COLOR_RANGE_FULL : COLOR_RANGE_LIMITED;
        }
    }

    return VLC_SUCCESS;
}

static int ParseVOP( decoder_t *p_dec, block_t *p_vop )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int64_t i_time_increment, i_time_ref;
    int i_modulo_time_base = 0, i_time_increment_bits;
    bs_t s;

    bs_init( &s, &p_vop->p_buffer[4], p_vop->i_buffer - 4 );

    switch( bs_read( &s, 2 ) )
    {
    case 0:
        p_sys->i_flags = BLOCK_FLAG_TYPE_I;
        break;
    case 1:
        p_sys->i_flags = BLOCK_FLAG_TYPE_P;
        break;
    case 2:
        p_sys->i_flags = BLOCK_FLAG_TYPE_B;
        p_sys->b_frame = true;
        break;
    case 3: /* gni ? */
        p_sys->i_flags = BLOCK_FLAG_TYPE_PB;
        break;
    }

    while( bs_read( &s, 1 ) ) i_modulo_time_base++;
    if( !bs_read1( &s ) ) return VLC_EGENERIC; /* Marker */

    /* VOP time increment */
    i_time_increment_bits = vlc_log2(p_sys->i_fps_num - 1) + 1;
    if( i_time_increment_bits < 1 ) i_time_increment_bits = 1;
    i_time_increment = bs_read( &s, i_time_increment_bits );

    /* Interpolate PTS/DTS */
    if( !(p_sys->i_flags & BLOCK_FLAG_TYPE_B) )
    {
        p_sys->i_last_time_ref = p_sys->i_time_ref;
        p_sys->i_time_ref +=
            (i_modulo_time_base * p_sys->i_fps_num);
        i_time_ref = p_sys->i_time_ref;
    }
    else
    {
        i_time_ref = p_sys->i_last_time_ref +
            (i_modulo_time_base * p_sys->i_fps_num);
    }

    int64_t i_time_diff = (i_time_ref + i_time_increment) - (p_sys->i_last_time + p_sys->i_last_timeincr);
    if( p_sys->i_fps_num && i_modulo_time_base == 0 && i_time_diff < 0 && -i_time_diff > p_sys->i_fps_num )
    {
        msg_Warn(p_dec, "missing modulo_time_base update");
        i_modulo_time_base += -i_time_diff / p_sys->i_fps_num;
        p_sys->i_time_ref += (i_modulo_time_base * p_sys->i_fps_num);
        p_sys->i_time_ref += p_sys->i_last_timeincr % p_sys->i_fps_num;
        i_time_ref = p_sys->i_time_ref;
    }

    if( p_sys->i_fps_num < 5 && /* Work-around buggy streams */
        p_dec->fmt_in.video.i_frame_rate > 0 &&
        p_dec->fmt_in.video.i_frame_rate_base > 0 )
    {
        p_sys->i_interpolated_pts += vlc_tick_from_samples(
        p_dec->fmt_in.video.i_frame_rate_base,
        p_dec->fmt_in.video.i_frame_rate);
    }
    else if( p_sys->i_fps_num )
    {
        i_time_diff = (i_time_ref + i_time_increment) - (p_sys->i_last_time + p_sys->i_last_timeincr);
        p_sys->i_interpolated_pts += vlc_tick_from_samples( i_time_diff, p_sys->i_fps_num );
    }

#if 0
    msg_Err( p_dec, "interp dts/pts (%"PRId64",%"PRId64"), dts/pts (%"PRId64",%"PRId64") %"PRId64" mod %d inc %"PRId64,
             p_sys->i_interpolated_dts, p_sys->i_interpolated_pts,
             p_vop->i_dts, p_vop->i_pts, p_sys->i_time_ref, i_modulo_time_base, i_time_increment );
#endif

    p_sys->i_last_time = i_time_ref;
    p_sys->i_last_timeincr = i_time_increment;

    /* Correct interpolated dts when we receive a new pts/dts */
    if( p_vop->i_pts != VLC_TICK_INVALID )
        p_sys->i_interpolated_pts = p_vop->i_pts;
    if( p_vop->i_dts != VLC_TICK_INVALID )
        p_sys->i_interpolated_dts = p_vop->i_dts;

    if( (p_sys->i_flags & BLOCK_FLAG_TYPE_B) || !p_sys->b_frame )
    {
        /* Trivial case (DTS == PTS) */

        p_sys->i_interpolated_dts = p_sys->i_interpolated_pts;

        if( p_vop->i_pts != VLC_TICK_INVALID )
            p_sys->i_interpolated_dts = p_vop->i_pts;
        if( p_vop->i_dts != VLC_TICK_INVALID )
            p_sys->i_interpolated_dts = p_vop->i_dts;

        p_sys->i_interpolated_pts = p_sys->i_interpolated_dts;
    }
    else
    {
        if( p_sys->i_last_ref_pts != VLC_TICK_INVALID )
            p_sys->i_interpolated_dts = p_sys->i_last_ref_pts;

        p_sys->i_last_ref_pts = p_sys->i_interpolated_pts;
    }

    return VLC_SUCCESS;
}

/* look at libavutil av_log2 ;) */
static int vlc_log2( unsigned int v )
{
    int n = 0;
    static const int vlc_log2_table[16] =
    {
        0,0,1,1,2,2,2,2, 3,3,3,3,3,3,3,3
    };

    if( v&0xffff0000 )
    {
        v >>= 16;
        n += 16;
    }
    if( v&0xff00 )
    {
        v >>= 8;
        n += 8;
    }
    if( v&0xf0 )
    {
        v >>= 4;
        n += 4;
    }
    n += vlc_log2_table[v];

    return n;
}
