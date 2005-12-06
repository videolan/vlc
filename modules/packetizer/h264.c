/*****************************************************************************
 * h264.c: h264/avc video packetizer
 *****************************************************************************
 * Copyright (C) 2001, 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include <vlc/sout.h>

#include "vlc_block_helper.h"
#include "vlc_bits.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_PACKETIZER );
    set_description( _("H264 video packetizer") );
    set_capability( "packetizer", 50 );
    set_callbacks( Open, Close );
vlc_module_end();


/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static block_t *Packetize( decoder_t *, block_t ** );
static block_t *PacketizeAVC1( decoder_t *, block_t ** );

struct decoder_sys_t
{
    block_bytestream_t bytestream;

    int     i_state;
    int     i_offset;
    uint8_t startcode[4];

    vlc_bool_t b_slice;
    block_t    *p_frame;

    vlc_bool_t   b_sps;
    vlc_bool_t   b_pps;

    /* avcC data */
    int i_avcC_length_size;
    block_t *p_sps;
    block_t *p_pps;

    /* Useful values of the Sequence Parameter Set */
    int i_log2_max_frame_num;
    int b_frame_mbs_only;

    /* Useful values of the Slice Header */
    int i_nal_type;
    int i_nal_ref_idc;
    int i_idr_pic_id;
    int i_frame_num;
};

enum
{
    STATE_NOSYNC,
    STATE_NEXT_SYNC,
};

enum nal_unit_type_e
{
    NAL_UNKNOWN = 0,
    NAL_SLICE   = 1,
    NAL_SLICE_DPA   = 2,
    NAL_SLICE_DPB   = 3,
    NAL_SLICE_DPC   = 4,
    NAL_SLICE_IDR   = 5,    /* ref_idc != 0 */
    NAL_SEI         = 6,    /* ref_idc == 0 */
    NAL_SPS         = 7,
    NAL_PPS         = 8,
    NAL_AU_DELIMITER= 9
    /* ref_idc == 0 for 6,9,10,11,12 */
};

enum nal_priority_e
{
    NAL_PRIORITY_DISPOSABLE = 0,
    NAL_PRIORITY_LOW        = 1,
    NAL_PRIORITY_HIGH       = 2,
    NAL_PRIORITY_HIGHEST    = 3,
};

static block_t *ParseNALBlock( decoder_t *, block_t * );

static block_t *nal_get_annexeb( decoder_t *, uint8_t *p, int );

/*****************************************************************************
 * Open: probe the packetizer and return score
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC( 'h', '2', '6', '4') &&
        p_dec->fmt_in.i_codec != VLC_FOURCC( 'H', '2', '6', '4') &&
        p_dec->fmt_in.i_codec != VLC_FOURCC( 'V', 'S', 'S', 'H') &&
        p_dec->fmt_in.i_codec != VLC_FOURCC( 'v', 's', 's', 'h') &&
        ( p_dec->fmt_in.i_codec != VLC_FOURCC( 'a', 'v', 'c', '1') ||
          p_dec->fmt_in.i_extra < 7 ) )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = malloc( sizeof(decoder_sys_t) ) ) == NULL )
    {
        msg_Err( p_dec, "out of memory" );
        return VLC_EGENERIC;
    }
    p_sys->i_state = STATE_NOSYNC;
    p_sys->i_offset = 0;
    p_sys->startcode[0] = 0;
    p_sys->startcode[1] = 0;
    p_sys->startcode[2] = 0;
    p_sys->startcode[3] = 1;
    p_sys->bytestream = block_BytestreamInit( p_dec );
    p_sys->b_slice = VLC_FALSE;
    p_sys->p_frame = NULL;
    p_sys->b_sps   = VLC_FALSE;
    p_sys->b_pps   = VLC_FALSE;
    p_sys->p_sps   = 0;
    p_sys->p_pps   = 0;

    p_sys->i_nal_type = -1;
    p_sys->i_nal_ref_idc = -1;
    p_sys->i_idr_pic_id = -1;
    p_sys->i_frame_num = -1;

    /* Setup properties */
    es_format_Copy( &p_dec->fmt_out, &p_dec->fmt_in );
    p_dec->fmt_out.i_codec = VLC_FOURCC( 'h', '2', '6', '4' );
    /* FIXME: FFMPEG isn't happy at all if you leave this */
    if( p_dec->fmt_out.i_extra ) free( p_dec->fmt_out.p_extra );
    p_dec->fmt_out.i_extra = 0; p_dec->fmt_out.p_extra = 0;

    if( p_dec->fmt_in.i_codec == VLC_FOURCC( 'a', 'v', 'c', '1' ) )
    {
        uint8_t *p = &((uint8_t*)p_dec->fmt_in.p_extra)[4];
        int i_sps, i_pps;
        int i;

        /* Parse avcC */
        p_sys->i_avcC_length_size = 1 + ((*p++)&0x03);

        /* Read SPS */
        i_sps = (*p++)&0x1f;

        for( i = 0; i < i_sps; i++ )
        {
            int i_length = GetWBE( p );
            block_t *p_sps = nal_get_annexeb( p_dec, p + 2, i_length );

            p_sys->p_sps = block_Duplicate( p_sps );
            p_sps->i_pts = p_sps->i_dts = mdate();
            ParseNALBlock( p_dec, p_sps );
            p += 2 + i_length;
        }
        /* Read PPS */
        i_pps = *p++;
        for( i = 0; i < i_pps; i++ )
        {
            int i_length = GetWBE( p );
            block_t *p_pps = nal_get_annexeb( p_dec, p + 2, i_length );

            p_sys->p_pps = block_Duplicate( p_pps );
            p_pps->i_pts = p_pps->i_dts = mdate();
            ParseNALBlock( p_dec, p_pps );
            p += 2 + i_length;
        }
        msg_Dbg( p_dec, "avcC length size=%d sps=%d pps=%d",
                 p_sys->i_avcC_length_size, i_sps, i_pps );

        /* Set callback */
        p_dec->pf_packetize = PacketizeAVC1;
    }
    else
    {
        /* Set callback */
        p_dec->pf_packetize = Packetize;

        /* */
        if( p_dec->fmt_in.i_extra > 0 )
        {
            block_t *p_init = block_New( p_dec, p_dec->fmt_in.i_extra );
            block_t *p_pic;

            memcpy( p_init->p_buffer, p_dec->fmt_in.p_extra,
                    p_dec->fmt_in.i_extra );

            while( ( p_pic = Packetize( p_dec, &p_init ) ) )
            {
                /* Should not occur because we should only receive SPS/PPS */
                block_Release( p_pic );
            }
        }
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: clean up the packetizer
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_frame ) block_ChainRelease( p_sys->p_frame );
    if( p_sys->p_sps ) block_Release( p_sys->p_sps );
    if( p_sys->p_pps ) block_Release( p_sys->p_pps );
    block_BytestreamRelease( &p_sys->bytestream );
    free( p_sys );
}

/****************************************************************************
 * Packetize: the whole thing
 ****************************************************************************/
static block_t *Packetize( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t       *p_pic;

    if( !pp_block || !*pp_block ) return NULL;

    block_BytestreamPush( &p_sys->bytestream, *pp_block );

    for( ;; )
    {
        switch( p_sys->i_state )
        {
            case STATE_NOSYNC:
                if( block_FindStartcodeFromOffset( &p_sys->bytestream,
                      &p_sys->i_offset, p_sys->startcode+1, 3 ) == VLC_SUCCESS)
                {
                    p_sys->i_state = STATE_NEXT_SYNC;
                }

                if( p_sys->i_offset )
                {
                    block_SkipBytes( &p_sys->bytestream, p_sys->i_offset );
                    p_sys->i_offset = 0;
                    block_BytestreamFlush( &p_sys->bytestream );
                }

                if( p_sys->i_state != STATE_NEXT_SYNC )
                {
                    /* Need more data */
                    return NULL;
                }

                p_sys->i_offset = 1; /* To find next startcode */

            case STATE_NEXT_SYNC:
                /* Find the next startcode */
                if( block_FindStartcodeFromOffset( &p_sys->bytestream,
                      &p_sys->i_offset, p_sys->startcode+1, 3 ) != VLC_SUCCESS)
                {
                    /* Need more data */
                    return NULL;
                }

                /* Get the new fragment and set the pts/dts */
                p_pic = block_New( p_dec, p_sys->i_offset );
                p_pic->i_pts = p_sys->bytestream.p_block->i_pts;
                p_pic->i_dts = p_sys->bytestream.p_block->i_dts;

                block_GetBytes( &p_sys->bytestream, p_pic->p_buffer,
                                p_pic->i_buffer );

                if( !p_pic->p_buffer[p_pic->i_buffer-1] ) p_pic->i_buffer--;
                p_sys->i_offset = 0;

                /* Parse the NAL */
                if( !( p_pic = ParseNALBlock( p_dec, p_pic ) ) )
                {
                    p_sys->i_state = STATE_NOSYNC;
                    break;
                }
#if 0
                msg_Dbg( p_dec, "pts="I64Fd" dts="I64Fd,
                         p_pic->i_pts, p_pic->i_dts );
#endif

                /* So p_block doesn't get re-added several times */
                *pp_block = block_BytestreamPop( &p_sys->bytestream );

                p_sys->i_state = STATE_NOSYNC;

                return p_pic;
        }
    }
}

/****************************************************************************
 * PacketizeAVC1: the whole thing
 ****************************************************************************/
static block_t *PacketizeAVC1( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t       *p_block;
    block_t       *p_ret = NULL;
    uint8_t       *p;

    if( !pp_block || !*pp_block ) return NULL;

    p_block = *pp_block;
    *pp_block = NULL;

#if 0
    if( //(p_block->i_flags & BLOCK_FLAG_TYPE_I) &&
        p_sys->p_sps && p_sys->p_pps )
    {
        block_t *p_pic;
        block_t *p_sps = block_Duplicate( p_sys->p_sps );
        block_t *p_pps = block_Duplicate( p_sys->p_pps );
        p_sps->i_dts = p_pps->i_dts = p_block->i_dts;
        p_sps->i_pts = p_pps->i_pts = p_block->i_pts;
        p_pic = ParseNALBlock( p_dec, p_sps );
        if( p_pic ) block_ChainAppend( &p_ret, p_pic );
        p_pic = ParseNALBlock( p_dec, p_pps );
        if( p_pic ) block_ChainAppend( &p_ret, p_pic );
    }
#endif

    for( p = p_block->p_buffer; p < &p_block->p_buffer[p_block->i_buffer]; )
    {
        block_t *p_pic;
        int i_size = 0;
        int i;

        for( i = 0; i < p_sys->i_avcC_length_size; i++ )
        {
            i_size = (i_size << 8) | (*p++);
        }

        if( i_size > 0 )
        {
            block_t *p_part = nal_get_annexeb( p_dec, p, i_size );

            p_part->i_dts = p_block->i_dts;
            p_part->i_pts = p_block->i_pts;

            /* Parse the NAL */
            if( ( p_pic = ParseNALBlock( p_dec, p_part ) ) )
            {
                block_ChainAppend( &p_ret, p_pic );
            }
        }
        p += i_size;
    }
    block_Release( p_block );

    return p_ret;
}

static block_t *nal_get_annexeb( decoder_t *p_dec, uint8_t *p, int i_size )
{
    block_t *p_nal;

    p_nal = block_New( p_dec, 3 + i_size );

    /* Add start code */
    p_nal->p_buffer[0] = 0x00;
    p_nal->p_buffer[1] = 0x00;
    p_nal->p_buffer[2] = 0x01;

    /* Copy nalu */
    memcpy( &p_nal->p_buffer[3], p, i_size );

    return p_nal;
}

static void nal_get_decoded( uint8_t **pp_ret, int *pi_ret,
                             uint8_t *src, int i_src )
{
    uint8_t *end = &src[i_src];
    uint8_t *dst = malloc( i_src );

    *pp_ret = dst;

    while( src < end )
    {
        if( src < end - 3 && src[0] == 0x00 && src[1] == 0x00 &&
            src[2] == 0x03 )
        {
            *dst++ = 0x00;
            *dst++ = 0x00;

            src += 3;
            continue;
        }
        *dst++ = *src++;
    }

    *pi_ret = dst - *pp_ret;
}

static inline int bs_read_ue( bs_t *s )
{
    int i = 0;

    while( bs_read1( s ) == 0 && s->p < s->p_end && i < 32 )
    {
        i++;
    }
    return( ( 1 << i) - 1 + bs_read( s, i ) );
}

static inline int bs_read_se( bs_t *s )
{
    int val = bs_read_ue( s );

    return val&0x01 ? (val+1)/2 : -(val/2);
}


static block_t *ParseNALBlock( decoder_t *p_dec, block_t *p_frag )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_pic = NULL;

    const int i_nal_ref_idc = (p_frag->p_buffer[3] >> 5)&0x03;
    const int i_nal_type = p_frag->p_buffer[3]&0x1f;

#define OUTPUT \
    do {                                                \
        p_pic = block_ChainGather( p_sys->p_frame );    \
        p_pic->i_length = 0;    /* FIXME */             \
                                                        \
        p_sys->p_frame = NULL;                          \
        p_sys->b_slice = VLC_FALSE;                     \
    } while(0)


    if( p_sys->b_slice && !p_sys->b_sps )
    {
        block_ChainRelease( p_sys->p_frame );
        msg_Warn( p_dec, "waiting for SPS" );

        /* Reset context */
        p_sys->p_frame = NULL;
        p_sys->b_slice = VLC_FALSE;
    }

    if( !p_sys->b_sps &&
        i_nal_type >= NAL_SLICE && i_nal_type <= NAL_SLICE_IDR )
    {
        p_sys->b_slice = VLC_TRUE;
        /* Fragment will be discarded later on */
    }
    else if( i_nal_type >= NAL_SLICE && i_nal_type <= NAL_SLICE_IDR )
    {
        uint8_t *dec;
        int i_dec, i_first_mb, i_slice_type, i_frame_num, i_pic_flags = 0;
        vlc_bool_t b_pic = VLC_FALSE;
        bs_t s;

        /* do not convert the whole frame */
        nal_get_decoded( &dec, &i_dec, &p_frag->p_buffer[4],
                         __MIN( p_frag->i_buffer - 4, 60 ) );
        bs_init( &s, dec, i_dec );

        /* first_mb_in_slice */
        i_first_mb = bs_read_ue( &s );

        /* slice_type */
        switch( (i_slice_type = bs_read_ue( &s )) )
        {
            case 0: case 5:
                i_pic_flags = BLOCK_FLAG_TYPE_P;
                break;
            case 1: case 6:
                i_pic_flags = BLOCK_FLAG_TYPE_B;
                break;
            case 2: case 7:
                i_pic_flags = BLOCK_FLAG_TYPE_I;
                break;
            case 3: case 8: /* SP */
                i_pic_flags = BLOCK_FLAG_TYPE_P;
                break;
            case 4: case 9:
                i_pic_flags = BLOCK_FLAG_TYPE_I;
                break;
        }

        /* pic_parameter_set_id */
        bs_read_ue( &s );
        /* frame_num */
        i_frame_num = bs_read( &s, p_sys->i_log2_max_frame_num + 4 );

        /* Detection of the first VCL NAL unit of a primary coded picture
         * (cf. 7.4.1.2.4) */
        if( i_frame_num != p_sys->i_frame_num ||
            ( (i_nal_ref_idc != p_sys->i_nal_ref_idc) &&
              (!i_nal_ref_idc || !p_sys->i_nal_ref_idc) ) )
        {
            b_pic = VLC_TRUE;
        }
        p_sys->i_frame_num = i_frame_num;
        p_sys->i_nal_ref_idc = i_nal_ref_idc;

        if( !p_sys->b_frame_mbs_only )
        {
            /* field_pic_flag */
            if( bs_read( &s, 1 ) )
            {
                /* bottom_field_flag */
                bs_read( &s, 1 );
            }
        }

        if( i_nal_type == NAL_SLICE_IDR )
        {
            /* id_pic_id */
            int i_idr_pic_id = bs_read_ue( &s );
            if( p_sys->i_nal_type != i_nal_type ) b_pic = VLC_TRUE;
            if( p_sys->i_idr_pic_id != i_idr_pic_id ) b_pic = VLC_TRUE;
            p_sys->i_idr_pic_id = i_idr_pic_id;
        }
        p_sys->i_nal_type = i_nal_type;

        if( b_pic && p_sys->b_slice ) OUTPUT;

        p_sys->b_slice = VLC_TRUE;

        free( dec );
    }
    else if( i_nal_type == NAL_SPS )
    {
        uint8_t *dec;
        int     i_dec;
        bs_t s;
        int i_tmp;

        if( !p_sys->b_sps ) msg_Dbg( p_dec, "found NAL_SPS" );

        p_sys->b_sps = VLC_TRUE;

        nal_get_decoded( &dec, &i_dec, &p_frag->p_buffer[4],
                         p_frag->i_buffer - 4 );

        bs_init( &s, dec, i_dec );
        /* Skip profile(8), constraint_set012, reserver(5), level(8) */
        bs_skip( &s, 8 + 1+1+1 + 5 + 8 );
        /* sps id */
        bs_read_ue( &s );
        /* Skip i_log2_max_frame_num */
        p_sys->i_log2_max_frame_num = bs_read_ue( &s );
        /* Read poc_type */
        i_tmp = bs_read_ue( &s );
        if( i_tmp == 0 )
        {
            /* skip i_log2_max_poc_lsb */
            bs_read_ue( &s );
        }
        else if( i_tmp == 1 )
        {
            int i_cycle;
            /* skip b_delta_pic_order_always_zero */
            bs_skip( &s, 1 );
            /* skip i_offset_for_non_ref_pic */
            bs_read_se( &s );
            /* skip i_offset_for_top_to_bottom_field */
            bs_read_se( &s );
            /* read i_num_ref_frames_in_poc_cycle */
            i_cycle = bs_read_ue( &s );
            if( i_cycle > 256 ) i_cycle = 256;
            while( i_cycle > 0 )
            {
                /* skip i_offset_for_ref_frame */
                bs_read_se(&s );
            }
        }
        /* i_num_ref_frames */
        bs_read_ue( &s );
        /* b_gaps_in_frame_num_value_allowed */
        bs_skip( &s, 1 );

        /* Read size */
        p_dec->fmt_out.video.i_width  = 16 * ( bs_read_ue( &s ) + 1 );
        p_dec->fmt_out.video.i_height = 16 * ( bs_read_ue( &s ) + 1 );

        /* b_frame_mbs_only */
        p_sys->b_frame_mbs_only = bs_read( &s, 1 );
        if( p_sys->b_frame_mbs_only == 0 )
        {
            bs_skip( &s, 1 );
        }
        /* b_direct8x8_inference */
        bs_skip( &s, 1 );

        /* crop */
        i_tmp = bs_read( &s, 1 );
        if( i_tmp )
        {
            /* left */
            bs_read_ue( &s );
            /* right */
            bs_read_ue( &s );
            /* top */
            bs_read_ue( &s );
            /* bottom */
            bs_read_ue( &s );
        }

        /* vui */
        i_tmp = bs_read( &s, 1 );
        if( i_tmp )
        {
            /* read the aspect ratio part if any FIXME check it */
            i_tmp = bs_read( &s, 1 );
            if( i_tmp )
            {
                static const struct { int w, h; } sar[14] =
                {
                    { 0,   0 }, { 1,   1 }, { 12, 11 }, { 10, 11 },
                    { 16, 11 }, { 40, 33 }, { 24, 11 }, { 20, 11 },
                    { 32, 11 }, { 80, 33 }, { 18, 11 }, { 15, 11 },
                    { 64, 33 }, { 160,99 },
                };
                int i_sar = bs_read( &s, 8 );
                int w, h;

                if( i_sar < 14 )
                {
                    w = sar[i_sar].w;
                    h = sar[i_sar].h;
                }
                else
                {
                    w = bs_read( &s, 16 );
                    h = bs_read( &s, 16 );
                }
                if( h != 0 )
                    p_dec->fmt_out.video.i_aspect = VOUT_ASPECT_FACTOR * w /
                        h * p_dec->fmt_out.video.i_width /
                        p_dec->fmt_out.video.i_height;
                else
                    p_dec->fmt_out.video.i_aspect = VOUT_ASPECT_FACTOR;
            }
        }

        free( dec );


        if( p_sys->b_slice ) OUTPUT;
    }
    else if( i_nal_type == NAL_PPS )
    {
        bs_t s;
        bs_init( &s, &p_frag->p_buffer[4], p_frag->i_buffer - 4 );

        if( !p_sys->b_pps ) msg_Dbg( p_dec, "found NAL_PPS" );
        p_sys->b_pps = VLC_TRUE;

        /* TODO */

        if( p_sys->b_slice ) OUTPUT;
    }
    else if( i_nal_type == NAL_AU_DELIMITER ||
             i_nal_type == NAL_SEI ||
             ( i_nal_type >= 13 && i_nal_type <= 18 ) )
    {
        if( p_sys->b_slice ) OUTPUT;
    }

#undef OUTPUT

    /* Append the block */
    block_ChainAppend( &p_sys->p_frame, p_frag );

    return p_pic;
}
