/*****************************************************************************
 * vc1.c
 *****************************************************************************
 * Copyright (C) 2001, 2002, 2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc_block.h>

#include <vlc_bits.h>
#include <vlc_block_helper.h>
#include "packetizer_helper.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_PACKETIZER )
    set_description( N_("VC-1 packetizer") )
    set_capability( "packetizer", 50 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct decoder_sys_t
{
    /*
     * Input properties
     */
    packetizer_t packetizer;

    /* Current sequence header */
    bool b_sequence_header;
    struct
    {
        block_t *p_sh;
        bool b_advanced_profile;
        bool b_interlaced;
        bool b_frame_interpolation;
        bool b_range_reduction;
        bool b_has_bframe;
    } sh;
    bool b_entry_point;
    struct
    {
        block_t *p_ep;
    } ep;

    /* */
    bool  b_frame;

    /* Current frame being built */
    mtime_t    i_frame_dts;
    mtime_t    i_frame_pts;
    block_t    *p_frame;
    block_t    **pp_last;


    mtime_t i_interpolated_dts;
    bool    b_check_startcode;
};

typedef enum
{
    IDU_TYPE_SEQUENCE_HEADER = 0x0f,
    IDU_TYPE_ENTRY_POINT = 0x0e,
    IDU_TYPE_FRAME = 0x0D,
    IDU_TYPE_FIELD = 0x0C,
    IDU_TYPE_SLICE = 0x0B,
    IDU_TYPE_END_OF_SEQUENCE = 0x0A,

    IDU_TYPE_SEQUENCE_LEVEL_USER_DATA = 0x1F,
    IDU_TYPE_ENTRY_POINT_USER_DATA = 0x1E,
    IDU_TYPE_FRAME_USER_DATA = 0x1D,
    IDU_TYPE_FIELD_USER_DATA = 0x1C,
    IDU_TYPE_SLICE_USER_DATA = 0x1B,
} idu_type_t;

static block_t *Packetize( decoder_t *p_dec, block_t **pp_block );

static void PacketizeReset( void *p_private, bool b_broken );
static block_t *PacketizeParse( void *p_private, bool *pb_ts_used, block_t * );
static int PacketizeValidate( void *p_private, block_t * );

static block_t *ParseIDU( decoder_t *p_dec, bool *pb_used_ts, block_t *p_frag );

static const uint8_t p_vc1_startcode[3] = { 0x00, 0x00, 0x01 };
/*****************************************************************************
 * Open: probe the packetizer and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to choose.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec !=  VLC_CODEC_VC1 )
        return VLC_EGENERIC;

    p_dec->pf_packetize = Packetize;

    /* Create the output format */
    es_format_Copy( &p_dec->fmt_out, &p_dec->fmt_in );
    p_dec->p_sys = p_sys = malloc( sizeof( decoder_sys_t ) );
    if( unlikely( !p_sys ) )
        return VLC_ENOMEM;

    packetizer_Init( &p_sys->packetizer,
                     p_vc1_startcode, sizeof(p_vc1_startcode),
                     NULL, 0, 4,
                     PacketizeReset, PacketizeParse, PacketizeValidate, p_dec );

    p_sys->b_sequence_header = false;
    p_sys->sh.p_sh = NULL;
    p_sys->b_entry_point = false;
    p_sys->ep.p_ep = NULL;

    p_sys->i_frame_dts = VLC_TS_INVALID;
    p_sys->i_frame_pts = VLC_TS_INVALID;

    p_sys->b_frame = false;
    p_sys->p_frame = NULL;
    p_sys->pp_last = &p_sys->p_frame;

    p_sys->i_interpolated_dts = VLC_TS_INVALID;
    p_sys->b_check_startcode = p_dec->fmt_in.b_packetized;

    if( p_dec->fmt_out.i_extra > 0 )
    {
        uint8_t *p_extra = p_dec->fmt_out.p_extra;

        /* With (some) ASF the first byte has to be stripped */
        if( p_extra[0] != 0x00 )
        {
            memcpy( &p_extra[0], &p_extra[1], p_dec->fmt_out.i_extra - 1 );
            p_dec->fmt_out.i_extra--;
        }

        /* */
        if( p_dec->fmt_out.i_extra > 0 )
            packetizer_Header( &p_sys->packetizer,
                               p_dec->fmt_out.p_extra, p_dec->fmt_out.i_extra );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    packetizer_Clean( &p_sys->packetizer );
    if( p_sys->p_frame )
        block_Release( p_sys->p_frame );
    free( p_sys );
}

/*****************************************************************************
 * Packetize: packetize an access unit
 *****************************************************************************/
static block_t *Packetize( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->b_check_startcode && pp_block && *pp_block )
    {
        /* Fix syntax for (some?) VC1 from asf */
        const unsigned i_startcode = sizeof(p_vc1_startcode);

        block_t *p_block = *pp_block;
        if( p_block->i_buffer > 0 &&
            ( p_block->i_buffer < i_startcode ||
              memcmp( p_block->p_buffer, p_vc1_startcode, i_startcode ) ) )
        {
            *pp_block = p_block = block_Realloc( p_block, i_startcode+1, p_block->i_buffer );
            if( p_block )
            {
                memcpy( p_block->p_buffer, p_vc1_startcode, i_startcode );

                if( p_sys->b_sequence_header && p_sys->sh.b_interlaced &&
                    p_block->i_buffer > i_startcode+1 &&
                    (p_block->p_buffer[i_startcode+1] & 0xc0) == 0xc0 )
                    p_block->p_buffer[i_startcode] = IDU_TYPE_FIELD;
                else
                    p_block->p_buffer[i_startcode] = IDU_TYPE_FRAME;
            }
        }
        p_sys->b_check_startcode = false;
    }

    block_t *p_au = packetizer_Packetize( &p_sys->packetizer, pp_block );
    if( !p_au )
        p_sys->b_check_startcode = p_dec->fmt_in.b_packetized;

    return p_au;
}

static void PacketizeReset( void *p_private, bool b_broken )
{
    decoder_t *p_dec = p_private;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( b_broken )
    {
        if( p_sys->p_frame )
            block_ChainRelease( p_sys->p_frame );
        p_sys->p_frame = NULL;
        p_sys->pp_last = &p_sys->p_frame;
        p_sys->b_frame = false;
    }

    p_sys->i_frame_dts = VLC_TS_INVALID;
    p_sys->i_frame_pts = VLC_TS_INVALID;
    p_sys->i_interpolated_dts = VLC_TS_INVALID;
}
static block_t *PacketizeParse( void *p_private, bool *pb_ts_used, block_t *p_block )
{
    decoder_t *p_dec = p_private;

    return ParseIDU( p_dec, pb_ts_used, p_block );
}

static int PacketizeValidate( void *p_private, block_t *p_au )
{
    decoder_t *p_dec = p_private;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->i_interpolated_dts <= VLC_TS_INVALID )
    {
        msg_Dbg( p_dec, "need a starting pts/dts" );
        return VLC_EGENERIC;
    }
    VLC_UNUSED(p_au);
    return VLC_SUCCESS;
}


/* DecodeRIDU: decode the startcode emulation prevention (same than h264) */
static void DecodeRIDU( uint8_t *p_ret, int *pi_ret, uint8_t *src, int i_src )
{
    uint8_t *end = &src[i_src];
    uint8_t *dst_end = &p_ret[*pi_ret];
    uint8_t *dst = p_ret;

    while( src < end && dst < dst_end )
    {
        if( src < end - 3 && src[0] == 0x00 && src[1] == 0x00 &&
            src[2] == 0x03 && dst < dst_end - 1 )
        {
            *dst++ = 0x00;
            *dst++ = 0x00;

            src += 3;
            continue;
        }
        *dst++ = *src++;
    }

    *pi_ret = dst - p_ret;
}
/* BuildExtraData: gather sequence header and entry point */
static void BuildExtraData( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    es_format_t *p_es = &p_dec->fmt_out;
    int i_extra;
    if( !p_sys->b_sequence_header || !p_sys->b_entry_point )
        return;

    i_extra = p_sys->sh.p_sh->i_buffer + p_sys->ep.p_ep->i_buffer;
    if( p_es->i_extra != i_extra )
    {
        p_es->i_extra = i_extra;
        p_es->p_extra = xrealloc( p_es->p_extra, p_es->i_extra );
    }
    memcpy( p_es->p_extra,
            p_sys->sh.p_sh->p_buffer, p_sys->sh.p_sh->i_buffer );
    memcpy( (uint8_t*)p_es->p_extra + p_sys->sh.p_sh->i_buffer,
            p_sys->ep.p_ep->p_buffer, p_sys->ep.p_ep->i_buffer );
}
/* ParseIDU: parse an Independent Decoding Unit */
static block_t *ParseIDU( decoder_t *p_dec, bool *pb_used_ts, block_t *p_frag )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_pic;
    const idu_type_t idu = p_frag->p_buffer[3];

    *pb_used_ts = false;
    if( !p_sys->b_sequence_header && idu != IDU_TYPE_SEQUENCE_HEADER )
    {
        msg_Warn( p_dec, "waiting for sequence header" );
        block_Release( p_frag );
        return NULL;
    }
    if( p_sys->b_sequence_header && !p_sys->b_entry_point && idu != IDU_TYPE_ENTRY_POINT )
    {
        msg_Warn( p_dec, "waiting for entry point" );
        block_Release( p_frag );
        return NULL;
    }
    /* TODO we do not gather ENTRY_POINT and SEQUENCE_DATA user data
     * But It should not be a problem for decoder */

    /* Do we have completed a frame */
    p_pic = NULL;
    if( p_sys->b_frame &&
        idu != IDU_TYPE_FRAME_USER_DATA &&
        idu != IDU_TYPE_FIELD && idu != IDU_TYPE_FIELD_USER_DATA &&
        idu != IDU_TYPE_SLICE && idu != IDU_TYPE_SLICE_USER_DATA &&
        idu != IDU_TYPE_END_OF_SEQUENCE )
    {
        /* Prepend SH and EP on I */
        if( p_sys->p_frame->i_flags & BLOCK_FLAG_TYPE_I )
        {
            block_t *p_list = block_Duplicate( p_sys->sh.p_sh );
            block_ChainAppend( &p_list, block_Duplicate( p_sys->ep.p_ep ) );
            block_ChainAppend( &p_list, p_sys->p_frame );

            p_list->i_flags = p_sys->p_frame->i_flags;

            p_sys->p_frame = p_list;
        }

        /* */
        p_pic = block_ChainGather( p_sys->p_frame );
        p_pic->i_dts = p_sys->i_frame_dts;
        p_pic->i_pts = p_sys->i_frame_pts;

        /* */
        if( p_pic->i_dts > VLC_TS_INVALID )
            p_sys->i_interpolated_dts = p_pic->i_dts;

        /* We can interpolate dts/pts only if we have a frame rate */
        if( p_dec->fmt_out.video.i_frame_rate != 0 && p_dec->fmt_out.video.i_frame_rate_base != 0 )
        {
            if( p_sys->i_interpolated_dts > VLC_TS_INVALID )
                p_sys->i_interpolated_dts += INT64_C(1000000) *
                                             p_dec->fmt_out.video.i_frame_rate_base /
                                             p_dec->fmt_out.video.i_frame_rate;

            //msg_Dbg( p_dec, "-------------- XXX0 dts=%"PRId64" pts=%"PRId64" interpolated=%"PRId64,
            //         p_pic->i_dts, p_pic->i_pts, p_sys->i_interpolated_dts );
            if( p_pic->i_dts <= VLC_TS_INVALID )
                p_pic->i_dts = p_sys->i_interpolated_dts;

            if( p_pic->i_pts <= VLC_TS_INVALID )
            {
                if( !p_sys->sh.b_has_bframe || (p_pic->i_flags & BLOCK_FLAG_TYPE_B ) )
                    p_pic->i_pts = p_pic->i_dts;
                /* TODO compute pts for other case */
            }
        }

        //msg_Dbg( p_dec, "-------------- dts=%"PRId64" pts=%"PRId64, p_pic->i_dts, p_pic->i_pts );

        /* Reset context */
        p_sys->b_frame = false;
        p_sys->i_frame_dts = VLC_TS_INVALID;
        p_sys->i_frame_pts = VLC_TS_INVALID;
        p_sys->p_frame = NULL;
        p_sys->pp_last = &p_sys->p_frame;
    }

    /*  */
    if( p_sys->i_frame_dts <= VLC_TS_INVALID && p_sys->i_frame_pts <= VLC_TS_INVALID )
    {
        p_sys->i_frame_dts = p_frag->i_dts;
        p_sys->i_frame_pts = p_frag->i_pts;
        *pb_used_ts = true;
    }

    /* We will add back SH and EP on I frames */
    block_t *p_release = NULL;
    if( idu != IDU_TYPE_SEQUENCE_HEADER && idu != IDU_TYPE_ENTRY_POINT )
        block_ChainLastAppend( &p_sys->pp_last, p_frag );
    else
        p_release = p_frag;

    /* Parse IDU */
    if( idu == IDU_TYPE_SEQUENCE_HEADER )
    {
        es_format_t *p_es = &p_dec->fmt_out;
        bs_t s;
        int i_profile;
        uint8_t ridu[32];
        int     i_ridu = sizeof(ridu);

        /* */
        if( p_sys->sh.p_sh )
            block_Release( p_sys->sh.p_sh );
        p_sys->sh.p_sh = block_Duplicate( p_frag );

        /* Extract the raw IDU */
        DecodeRIDU( ridu, &i_ridu, &p_frag->p_buffer[4], p_frag->i_buffer - 4 );

        /* Auto detect VC-1_SPMP_PESpacket_PayloadFormatHeader (SMPTE RP 227) for simple/main profile
         * TODO find a test case and valid it */
        if( i_ridu > 4 && (ridu[0]&0x80) == 0 ) /* for advanced profile, the first bit is 1 */
        {
            video_format_t *p_v = &p_dec->fmt_in.video;
            const size_t i_potential_width  = GetWBE( &ridu[0] );
            const size_t i_potential_height = GetWBE( &ridu[2] );

            if( i_potential_width >= 2  && i_potential_width <= 8192 &&
                i_potential_height >= 2 && i_potential_height <= 8192 )
            {
                if( ( p_v->i_width <= 0 && p_v->i_height <= 0  ) ||
                    ( p_v->i_width  == i_potential_width &&  p_v->i_height == i_potential_height ) )
                {
                    static const uint8_t startcode[4] = { 0x00, 0x00, 0x01, IDU_TYPE_SEQUENCE_HEADER };
                    p_es->video.i_width  = i_potential_width;
                    p_es->video.i_height = i_potential_height;

                    /* Remove it */
                    p_frag->p_buffer += 4;
                    p_frag->i_buffer -= 4;
                    memcpy( p_frag->p_buffer, startcode, sizeof(startcode) );
                }
            }
        }

        /* Parse it */
        bs_init( &s, ridu, i_ridu );
        i_profile = bs_read( &s, 2 );
        if( i_profile == 3 )
        {
            const int i_level = bs_read( &s, 3 );

            /* Advanced profile */
            p_sys->sh.b_advanced_profile = true;
            p_sys->sh.b_range_reduction = false;
            p_sys->sh.b_has_bframe = true;

            bs_skip( &s, 2+3+5+1 ); // chroma format + frame rate Q + bit rate Q + postprocflag

            p_es->video.i_width  = 2*bs_read( &s, 12 )+2;
            p_es->video.i_height = 2*bs_read( &s, 12 )+2;

            if( !p_sys->b_sequence_header )
                msg_Dbg( p_dec, "found sequence header for advanced profile level L%d resolution %dx%d",
                         i_level, p_es->video.i_width, p_es->video.i_height);

            bs_skip( &s, 1 );// pulldown
            p_sys->sh.b_interlaced = bs_read( &s, 1 );
            bs_skip( &s, 1 );// frame counter
            p_sys->sh.b_frame_interpolation = bs_read( &s, 1 );
            bs_skip( &s, 1 );       // Reserved
            bs_skip( &s, 1 );       // Psf

            if( bs_read( &s, 1 ) )  /* Display extension */
            {
                const int i_display_width  = bs_read( &s, 14 )+1;
                const int i_display_height = bs_read( &s, 14 )+1;

                p_es->video.i_sar_num = i_display_width  * p_es->video.i_height;
                p_es->video.i_sar_den = i_display_height * p_es->video.i_width;

                if( !p_sys->b_sequence_header )
                    msg_Dbg( p_dec, "display size %dx%d", i_display_width, i_display_height );

                if( bs_read( &s, 1 ) )  /* Pixel aspect ratio (PAR/SAR) */
                {
                    static const int p_ar[16][2] = {
                        { 0, 0}, { 1, 1}, {12,11}, {10,11}, {16,11}, {40,33},
                        {24,11}, {20,11}, {32,11}, {80,33}, {18,11}, {15,11},
                        {64,33}, {160,99},{ 0, 0}, { 0, 0}
                    };
                    int i_ar = bs_read( &s, 4 );
                    unsigned i_ar_w, i_ar_h;

                    if( i_ar == 15 )
                    {
                        i_ar_w = bs_read( &s, 8 );
                        i_ar_h = bs_read( &s, 8 );
                    }
                    else
                    {
                        i_ar_w = p_ar[i_ar][0];
                        i_ar_h = p_ar[i_ar][1];
                    }
                    vlc_ureduce( &i_ar_w, &i_ar_h, i_ar_w, i_ar_h, 0 );
                    if( !p_sys->b_sequence_header )
                        msg_Dbg( p_dec, "aspect ratio %d:%d", i_ar_w, i_ar_h );
                }
            }
            if( bs_read( &s, 1 ) )  /* Frame rate */
            {
                int i_fps_num = 0;
                int i_fps_den = 0;
                if( bs_read( &s, 1 ) )
                {
                    i_fps_num = bs_read( &s, 16 )+1;
                    i_fps_den = 32;
                }
                else
                {
                    const int i_nr = bs_read( &s, 8 );
                    const int i_dn = bs_read( &s, 4 );

                    switch( i_nr )
                    {
                    case 1: i_fps_num = 24000; break;
                    case 2: i_fps_num = 25000; break;
                    case 3: i_fps_num = 30000; break;
                    case 4: i_fps_num = 50000; break;
                    case 5: i_fps_num = 60000; break;
                    case 6: i_fps_num = 48000; break;
                    case 7: i_fps_num = 72000; break;
                    }
                    switch( i_dn )
                    {
                    case 1: i_fps_den = 1000; break;
                    case 2: i_fps_den = 1001; break;
                    }
                }
                if( i_fps_num != 0 && i_fps_den != 0 )
                    vlc_ureduce( &p_es->video.i_frame_rate, &p_es->video.i_frame_rate_base, i_fps_num, i_fps_den, 0 );

                if( !p_sys->b_sequence_header )
                    msg_Dbg( p_dec, "frame rate %d/%d", p_es->video.i_frame_rate, p_es->video.i_frame_rate_base );
            }
        }
        else
        {
            /* Simple and main profile */
            p_sys->sh.b_advanced_profile = false;
            p_sys->sh.b_interlaced = false;

            if( !p_sys->b_sequence_header )
                msg_Dbg( p_dec, "found sequence header for %s profile", i_profile == 0 ? "simple" : "main" );

            bs_skip( &s, 2+3+5+1+1+     // reserved + frame rate Q + bit rate Q + loop filter + reserved
                         1+1+1+1+2+     // multiresolution + reserved + fast uv mc + extended mv + dquant
                         1+1+1+1 );     // variable size transform + reserved + overlap + sync marker
            p_sys->sh.b_range_reduction = bs_read( &s, 1 );
            if( bs_read( &s, 3 ) > 0 )
                p_sys->sh.b_has_bframe = true;
            else
                p_sys->sh.b_has_bframe = false;
            bs_skip( &s, 2 );           // quantizer

            p_sys->sh.b_frame_interpolation = bs_read( &s, 1 );
        }
        p_sys->b_sequence_header = true;
        BuildExtraData( p_dec );
    }
    else if( idu == IDU_TYPE_ENTRY_POINT )
    {
        if( p_sys->ep.p_ep )
            block_Release( p_sys->ep.p_ep );
        p_sys->ep.p_ep = block_Duplicate( p_frag );

        if( !p_sys->b_entry_point )
            msg_Dbg( p_dec, "found entry point" );

        p_sys->b_entry_point = true;
        BuildExtraData( p_dec );
    }
    else if( idu == IDU_TYPE_FRAME )
    {
        bs_t s;
        uint8_t ridu[8];
        int     i_ridu = sizeof(ridu);

        /* Extract the raw IDU */
        DecodeRIDU( ridu, &i_ridu, &p_frag->p_buffer[4], p_frag->i_buffer - 4 );

        /* Parse it + interpolate pts/dts if possible */
        bs_init( &s, ridu, i_ridu );

        if( p_sys->sh.b_advanced_profile )
        {
            int i_fcm = 0;

            if( p_sys->sh.b_interlaced )
            {
                if( bs_read( &s, 1 ) )
                {
                    if( bs_read( &s, 1 ) )
                        i_fcm = 1;  /* interlaced field */
                    else
                        i_fcm = 2;  /* interlaced frame */
                }
            }

            if( i_fcm == 1 ) /*interlaced field */
            {
                /* XXX for mixed I/P we should check reference usage before marking them I (too much work) */
                switch( bs_read( &s, 3 ) )
                {
                case 0: /* II */
                case 1: /* IP */
                case 2: /* PI */
                    p_sys->p_frame->i_flags |= BLOCK_FLAG_TYPE_I;
                    p_sys->p_frame->i_flags |= BLOCK_FLAG_TYPE_I;
                    p_sys->p_frame->i_flags |= BLOCK_FLAG_TYPE_I;
                    break;
                case 3: /* PP */
                    p_sys->p_frame->i_flags |= BLOCK_FLAG_TYPE_P;
                    break;
                case 4: /* BB */
                case 5: /* BBi */
                case 6: /* BiB */
                case 7: /* BiBi */
                    p_sys->p_frame->i_flags |= BLOCK_FLAG_TYPE_B;
                    break;
                }
            }
            else
            {
                if( !bs_read( &s, 1 ) )
                    p_sys->p_frame->i_flags |= BLOCK_FLAG_TYPE_P;
                else if( !bs_read( &s, 1 ) )
                    p_sys->p_frame->i_flags |= BLOCK_FLAG_TYPE_B;
                else if( !bs_read( &s, 1 ) )
                    p_sys->p_frame->i_flags |= BLOCK_FLAG_TYPE_I;
                else if( !bs_read( &s, 1 ) )
                    p_sys->p_frame->i_flags |= BLOCK_FLAG_TYPE_B;   /* Bi */
                else
                    p_sys->p_frame->i_flags |= BLOCK_FLAG_TYPE_P;   /* P Skip */
            }
        }
        else
        {
            if( p_sys->sh.b_frame_interpolation )
                bs_skip( &s, 1 );   // interpolate
            bs_skip( &s, 2 );       // frame count
            if( p_sys->sh.b_range_reduction )
                bs_skip( &s, 1 );   // range reduction

            if( bs_read( &s, 1 ) )
                p_sys->p_frame->i_flags |= BLOCK_FLAG_TYPE_P;
            else if( !p_sys->sh.b_has_bframe || bs_read( &s, 1 ) )
                p_sys->p_frame->i_flags |= BLOCK_FLAG_TYPE_I;
            else
                p_sys->p_frame->i_flags |= BLOCK_FLAG_TYPE_B;
        }
        p_sys->b_frame = true;
    }

    if( p_release )
        block_Release( p_release );
    return p_pic;
}

