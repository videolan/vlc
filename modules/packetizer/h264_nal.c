/*****************************************************************************
 * Copyright © 2010-2014 VideoLAN
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
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

#include "h264_nal.h"

#include <limits.h>

/*
 * For avcC specification, see ISO/IEC 14496-15,
 * For Annex B specification, see ISO/IEC 14496-10
 */

static const uint8_t annexb_startcode[] = { 0x00, 0x00, 0x01 };

int convert_sps_pps( decoder_t *p_dec, const uint8_t *p_buf,
                     uint32_t i_buf_size, uint8_t *p_out_buf,
                     uint32_t i_out_buf_size, uint32_t *p_sps_pps_size,
                     uint32_t *p_nal_length_size)
{
    int i_profile;
    uint32_t i_data_size = i_buf_size, i_nal_size, i_sps_pps_size = 0;
    unsigned int i_loop_end;

    /* */
    if( i_data_size < 7 )
    {
        msg_Err( p_dec, "Input Metadata too small" );
        return VLC_ENOMEM;
    }

    /* Read infos in first 6 bytes */
    i_profile = (p_buf[1] << 16) | (p_buf[2] << 8) | p_buf[3];
    if (p_nal_length_size)
        *p_nal_length_size  = (p_buf[4] & 0x03) + 1;
    p_buf       += 5;
    i_data_size -= 5;

    for ( unsigned int j = 0; j < 2; j++ )
    {
        /* First time is SPS, Second is PPS */
        if( i_data_size < 1 )
        {
            msg_Err( p_dec, "PPS too small after processing SPS/PPS %u",
                    i_data_size );
            return VLC_ENOMEM;
        }
        i_loop_end = p_buf[0] & (j == 0 ? 0x1f : 0xff);
        p_buf++; i_data_size--;

        for ( unsigned int i = 0; i < i_loop_end; i++)
        {
            if( i_data_size < 2 )
            {
                msg_Err( p_dec, "SPS is too small %u", i_data_size );
                return VLC_ENOMEM;
            }

            i_nal_size = (p_buf[0] << 8) | p_buf[1];
            p_buf += 2;
            i_data_size -= 2;

            if( i_data_size < i_nal_size )
            {
                msg_Err( p_dec, "SPS size does not match NAL specified size %u",
                        i_data_size );
                return VLC_ENOMEM;
            }
            if( i_sps_pps_size + 4 + i_nal_size > i_out_buf_size )
            {
                msg_Err( p_dec, "Output SPS/PPS buffer too small" );
                return VLC_ENOMEM;
            }

            p_out_buf[i_sps_pps_size++] = 0;
            p_out_buf[i_sps_pps_size++] = 0;
            p_out_buf[i_sps_pps_size++] = 0;
            p_out_buf[i_sps_pps_size++] = 1;

            memcpy( p_out_buf + i_sps_pps_size, p_buf, i_nal_size );
            i_sps_pps_size += i_nal_size;

            p_buf += i_nal_size;
            i_data_size -= i_nal_size;
        }
    }

    *p_sps_pps_size = i_sps_pps_size;

    return VLC_SUCCESS;
}

void convert_h264_to_annexb( uint8_t *p_buf, uint32_t i_len,
                             size_t i_nal_length_size )
{
    uint32_t nal_len = 0, nal_pos = 0;

    if( i_nal_length_size != 4 )
        return;

    /* This only works for a NAL length size of 4 */
    /* TODO: realloc/memmove if i_nal_length_size is 2 or 1 */
    while( i_len > 0 )
    {
        if( nal_pos < i_nal_length_size ) {
            unsigned int i;
            for( i = 0; nal_pos < i_nal_length_size && i < i_len; i++, nal_pos++ ) {
                nal_len = (nal_len << 8) | p_buf[i];
                p_buf[i] = 0;
            }
            if( nal_pos < i_nal_length_size )
                return;
            p_buf[i - 1] = 1;
            p_buf += i;
            i_len -= i;
        }
        if( nal_len > INT_MAX )
            return;
        if( nal_len > i_len )
        {
            nal_len -= i_len;
            return;
        }
        else
        {
            p_buf += nal_len;
            i_len -= nal_len;
            nal_len = 0;
            nal_pos = 0;
        }
    }
}

static block_t *h264_increase_startcode_size( block_t *p_block,
                                              size_t i_start_ofs )
{
    block_t *p_new;
    uint32_t i_buf = p_block->i_buffer - i_start_ofs;
    uint8_t *p_buf = p_block->p_buffer;
    uint8_t *p_new_buf;
    size_t i_ofs = i_start_ofs;
    size_t i_grow = 0;
    size_t i_new_ofs;

    /* Search all startcode of size 3 */
    while( i_buf > 0 )
    {
        if( i_buf > 3 && memcmp( &p_buf[i_ofs], annexb_startcode, 3 ) == 0 )
        {
            if( i_ofs == 0 || p_buf[i_ofs - 1] != 0 )
                i_grow++;
            i_buf -= 3;
            i_ofs += 3;
        }
        else
        {
            i_buf--;
            i_ofs++;
        }
   }

    if( i_grow == 0 )
        return p_block;

    /* Alloc a bigger buffer */
    p_new = block_Alloc( p_block->i_buffer + i_grow );
    if( !p_new )
        return NULL;
    i_buf = p_block->i_buffer - i_start_ofs;
    p_new_buf = p_new->p_buffer;
    i_new_ofs = i_ofs = i_start_ofs;

    /* Copy the beginning of the buffer (same data) */
    if( i_start_ofs )
        memcpy( p_new_buf, p_buf, i_start_ofs );

    /* Copy the rest of the buffer and append a 0 before each 000001 */
    while( i_buf > 0 )
    {
        if( i_buf > 3 && memcmp( &p_buf[i_ofs], annexb_startcode, 3 ) == 0 )
        {
            if( i_ofs == 0 || p_buf[i_ofs - 1] != 0 )
                p_new_buf[i_new_ofs++] = 0;
            for( int i = 0; i < 3; ++i )
                p_new_buf[i_new_ofs++] = p_buf[i_ofs++];
            i_buf -= 3;
        } else
        {
            p_new_buf[i_new_ofs++] = p_buf[i_ofs++];
            i_buf--;
        }
   }

    block_Release( p_block );
    return p_new;
}

static int h264_replace_startcode( uint8_t *p_buf,
                                   size_t i_nal_length_size,
                                   size_t i_startcode_ofs,
                                   size_t i_nal_size )
{
    if( i_nal_size < (unsigned) 1 << ( 8 * i_nal_length_size) )
    {
        /* NAL is too big to fit in i_nal_length_size */
        return -1;
    }

    p_buf[i_startcode_ofs++] = i_nal_size >> (--i_nal_length_size * 8);
    if( !i_nal_length_size )
        return 0;
    p_buf[i_startcode_ofs++] = i_nal_size >> (--i_nal_length_size * 8);
    if( !i_nal_length_size )
        return 0;
    p_buf[i_startcode_ofs++] = i_nal_size >> (--i_nal_length_size * 8);
    p_buf[i_startcode_ofs] = i_nal_size;
    return 0;
}

block_t *convert_annexb_to_h264( block_t *p_block, size_t i_nal_length_size )
{
    size_t i_startcode_ofs = 0;
    size_t i_startcode_size = 0;
    uint32_t i_buf = p_block->i_buffer;
    uint8_t *p_buf = p_block->p_buffer;
    size_t i_ofs = 0;

    /* The length of the NAL size is encoded using 1, 2 or 4 bytes */
    if( i_nal_length_size != 1 && i_nal_length_size != 2
     && i_nal_length_size != 4 )
        goto error;

    /* Replace the Annex B start code with the size of the NAL. */
    while( i_buf > 0 )
    {
        if( i_buf > 3 && memcmp( &p_buf[i_ofs], annexb_startcode, 3 ) == 0 )
        {
            if( i_startcode_size )
            {
                size_t i_nal_size = i_ofs - i_startcode_ofs - i_startcode_size;

                if( i_ofs > 0 && p_buf[i_ofs - 1] == 0 )
                    i_nal_size--;
                if( h264_replace_startcode( p_buf, i_nal_length_size,
                                            i_startcode_ofs,
                                            i_nal_size ) )
                    goto error;
            }
            if( i_ofs > 0 && p_buf[i_ofs - 1] == 0 )
            {
                /* startcode of size 3 */
                i_startcode_ofs = i_ofs - 1;
                i_startcode_size = 4;
            }
            else
            {
                i_startcode_ofs = i_ofs;
                i_startcode_size = 3;
            }

            if( i_startcode_size < i_nal_length_size )
            {
                /* i_nal_length_size can't fit in i_startcode_size. Therefore,
                 * reallocate a buffer in order to increase all startcode that
                 * are smaller than i_nal_length_size. This is not efficient but
                 * it's a corner case that won't happen often */
                p_block = h264_increase_startcode_size( p_block, i_startcode_ofs );
                if( !p_block )
                    return NULL;

                p_buf = p_block->p_buffer;
                i_startcode_size++;
            }
            i_buf -= 3;
            i_ofs += 3;
        }
        else
        {
            i_buf--;
            i_ofs++;
        }
    }

    if( i_startcode_size
     && h264_replace_startcode( p_buf, i_nal_length_size, i_startcode_ofs,
                                i_ofs - i_startcode_ofs - i_startcode_size) )
        return NULL;
    else
        return p_block;
error:
    block_Release( p_block );
    return NULL;
}

int h264_get_spspps( uint8_t *p_buf, size_t i_buf,
                     uint8_t **pp_sps, size_t *p_sps_size,
                     uint8_t **pp_pps, size_t *p_pps_size )
{
    uint8_t *p_sps = NULL, *p_pps = NULL;
    size_t i_sps_size = 0, i_pps_size = 0;
    int i_nal_type = NAL_UNKNOWN;
    bool b_first_nal = true;
    bool b_has_zero_byte = false;

    while( i_buf > 0 )
    {
        unsigned int i_move = 1;

        /* cf B.1.1: a NAL unit starts and ends with 0x000001 or 0x00000001 */
        if( i_buf > 3 && !memcmp( p_buf, annexb_startcode, 3 ) )
        {
            if( i_nal_type != NAL_UNKNOWN )
            {
                /* update SPS/PPS size */
                if( i_nal_type == NAL_SPS )
                    i_sps_size = p_buf - p_sps - (b_has_zero_byte ? 1 : 0);
                if( i_nal_type == NAL_PPS )
                    i_pps_size = p_buf - p_pps - (b_has_zero_byte ? 1 : 0);

                if( i_sps_size && i_pps_size )
                    break;
            }

            if (i_buf < 4)
                return -1;
            i_nal_type = p_buf[3] & 0x1F;

            /* The start prefix is always 0x00000001 (annexb_startcode + a
             * leading zero byte) for SPS, PPS or the first NAL */
            if( !b_has_zero_byte && ( b_first_nal || i_nal_type == NAL_SPS
             || i_nal_type == NAL_PPS ) )
                return -1;
            b_first_nal = false;

            /* Pointer to the beginning of the SPS/PPS starting with the
             * leading zero byte */
            if( i_nal_type == NAL_SPS && !p_sps )
                p_sps = p_buf - 1;
            if( i_nal_type == NAL_PPS && !p_pps )
                p_pps = p_buf - 1;

            /* cf. 7.4.1.2.3 */
            if( i_nal_type > 18 || ( i_nal_type >= 10 && i_nal_type <= 12 ) )
                return -1;

            /* SPS/PPS are before the slices */
            if ( i_nal_type >= NAL_SLICE && i_nal_type <= NAL_SLICE_IDR )
                break;
            i_move = 4;
        }
        else if( b_first_nal && p_buf[0] != 0 )
        {
            /* leading_zero_8bits only before the first NAL */
            return -1;
        }
        b_has_zero_byte = *p_buf == 0;
        i_buf -= i_move;
        p_buf += i_move;
    }

    if( i_buf == 0 )
    {
        /* update SPS/PPS size if we reach the end of the bytestream */
        if( !i_sps_size && i_nal_type == NAL_SPS )
            i_sps_size = p_buf - p_sps;
        if( !i_pps_size && i_nal_type == NAL_PPS )
            i_pps_size = p_buf - p_pps;
    }
    if( ( !p_sps || !i_sps_size ) && ( !p_pps || !i_pps_size ) )
        return -1;
    *pp_sps = p_sps;
    *p_sps_size = i_sps_size;
    *pp_pps = p_pps;
    *p_pps_size = i_pps_size;

    return 0;
}

int h264_parse_sps( const uint8_t *p_sps_buf, int i_sps_size,
                    struct nal_sps *p_sps )
{
    uint8_t *pb_dec = NULL;
    int     i_dec = 0;
    bs_t s;
    int i_tmp;

    if (i_sps_size < 5 || (p_sps_buf[4] & 0x1f) != NAL_SPS)
        return -1;

    memset( p_sps, 0, sizeof(struct nal_sps) );
    CreateRbspFromNAL( &pb_dec, &i_dec, &p_sps_buf[5],
                      i_sps_size - 5 );

    bs_init( &s, pb_dec, i_dec );
    int i_profile_idc = bs_read( &s, 8 );
    p_sps->i_profile = i_profile_idc;
    p_sps->i_profile_compatibility = bs_read( &s, 8 );
    p_sps->i_level = bs_read( &s, 8 );
    /* sps id */
    p_sps->i_id = bs_read_ue( &s );
    if( p_sps->i_id >= SPS_MAX || p_sps->i_id < 0 )
    {
        free( pb_dec );
        return -1;
    }

    if( i_profile_idc == PROFILE_H264_HIGH || i_profile_idc == PROFILE_H264_HIGH_10 ||
        i_profile_idc == PROFILE_H264_HIGH_422 || i_profile_idc == PROFILE_H264_HIGH_444_PREDICTIVE ||
        i_profile_idc ==  PROFILE_H264_CAVLC_INTRA || i_profile_idc ==  PROFILE_H264_SVC_BASELINE ||
        i_profile_idc ==  PROFILE_H264_SVC_HIGH )
    {
        /* chroma_format_idc */
        const int i_chroma_format_idc = bs_read_ue( &s );
        if( i_chroma_format_idc == 3 )
            bs_skip( &s, 1 ); /* separate_colour_plane_flag */
        /* bit_depth_luma_minus8 */
        bs_read_ue( &s );
        /* bit_depth_chroma_minus8 */
        bs_read_ue( &s );
        /* qpprime_y_zero_transform_bypass_flag */
        bs_skip( &s, 1 );
        /* seq_scaling_matrix_present_flag */
        i_tmp = bs_read( &s, 1 );
        if( i_tmp )
        {
            for( int i = 0; i < ((3 != i_chroma_format_idc) ? 8 : 12); i++ )
            {
                /* seq_scaling_list_present_flag[i] */
                i_tmp = bs_read( &s, 1 );
                if( !i_tmp )
                    continue;
                const int i_size_of_scaling_list = (i < 6 ) ? 16 : 64;
                /* scaling_list (...) */
                int i_lastscale = 8;
                int i_nextscale = 8;
                for( int j = 0; j < i_size_of_scaling_list; j++ )
                {
                    if( i_nextscale != 0 )
                    {
                        /* delta_scale */
                        i_tmp = bs_read_se( &s );
                        i_nextscale = ( i_lastscale + i_tmp + 256 ) % 256;
                        /* useDefaultScalingMatrixFlag = ... */
                    }
                    /* scalinglist[j] */
                    i_lastscale = ( i_nextscale == 0 ) ? i_lastscale : i_nextscale;
                }
            }
        }
    }

    /* Skip i_log2_max_frame_num */
    p_sps->i_log2_max_frame_num = bs_read_ue( &s );
    if( p_sps->i_log2_max_frame_num > 12)
        p_sps->i_log2_max_frame_num = 12;
    /* Read poc_type */
    p_sps->i_pic_order_cnt_type = bs_read_ue( &s );
    if( p_sps->i_pic_order_cnt_type == 0 )
    {
        /* skip i_log2_max_poc_lsb */
        p_sps->i_log2_max_pic_order_cnt_lsb = bs_read_ue( &s );
        if( p_sps->i_log2_max_pic_order_cnt_lsb > 12 )
            p_sps->i_log2_max_pic_order_cnt_lsb = 12;
    }
    else if( p_sps->i_pic_order_cnt_type == 1 )
    {
        int i_cycle;
        /* skip b_delta_pic_order_always_zero */
        p_sps->i_delta_pic_order_always_zero_flag = bs_read( &s, 1 );
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
            i_cycle--;
        }
    }
    /* i_num_ref_frames */
    bs_read_ue( &s );
    /* b_gaps_in_frame_num_value_allowed */
    bs_skip( &s, 1 );

    /* Read size */
    p_sps->i_width  = 16 * ( bs_read_ue( &s ) + 1 );
    p_sps->i_height = 16 * ( bs_read_ue( &s ) + 1 );

    /* b_frame_mbs_only */
    p_sps->b_frame_mbs_only = bs_read( &s, 1 );
    p_sps->i_height *=  ( 2 - p_sps->b_frame_mbs_only );
    if( p_sps->b_frame_mbs_only == 0 )
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
        p_sps->vui.b_valid = true;
        /* read the aspect ratio part if any */
        i_tmp = bs_read( &s, 1 );
        if( i_tmp )
        {
            static const struct { int w, h; } sar[17] =
            {
                { 0,   0 }, { 1,   1 }, { 12, 11 }, { 10, 11 },
                { 16, 11 }, { 40, 33 }, { 24, 11 }, { 20, 11 },
                { 32, 11 }, { 80, 33 }, { 18, 11 }, { 15, 11 },
                { 64, 33 }, { 160,99 }, {  4,  3 }, {  3,  2 },
                {  2,  1 },
            };
            int i_sar = bs_read( &s, 8 );
            int w, h;

            if( i_sar < 17 )
            {
                w = sar[i_sar].w;
                h = sar[i_sar].h;
            }
            else if( i_sar == 255 )
            {
                w = bs_read( &s, 16 );
                h = bs_read( &s, 16 );
            }
            else
            {
                w = 0;
                h = 0;
            }

            if( w != 0 && h != 0 )
            {
                p_sps->vui.i_sar_num = w;
                p_sps->vui.i_sar_den = h;
            }
            else
            {
                p_sps->vui.i_sar_num = 1;
                p_sps->vui.i_sar_den = 1;
            }
        }

        /* overscan */
        i_tmp = bs_read( &s, 1 );
        if ( i_tmp )
            bs_read( &s, 1 );

        /* video signal type */
        i_tmp = bs_read( &s, 1 );
        if( i_tmp )
        {
            bs_read( &s, 4 );
            /* colour desc */
            bs_read( &s, 1 );
            if ( i_tmp )
                bs_read( &s, 24 );
        }

        /* chroma loc info */
        i_tmp = bs_read( &s, 1 );
        if( i_tmp )
        {
            bs_read_ue( &s );
            bs_read_ue( &s );
        }

        /* timing info */
        p_sps->vui.b_timing_info_present_flag = bs_read( &s, 1 );
        if( p_sps->vui.b_timing_info_present_flag )
        {
            p_sps->vui.i_num_units_in_tick = bs_read( &s, 32 );
            p_sps->vui.i_time_scale = bs_read( &s, 32 );
            p_sps->vui.b_fixed_frame_rate = bs_read( &s, 1 );
        }

        /* Nal hrd & VC1 hrd parameters */
        p_sps->vui.b_cpb_dpb_delays_present_flag = false;
        for ( int i=0; i<2; i++ )
        {
            i_tmp = bs_read( &s, 1 );
            if( i_tmp )
            {
                p_sps->vui.b_cpb_dpb_delays_present_flag = true;
                uint32_t count = bs_read_ue( &s ) + 1;
                bs_read( &s, 4 );
                bs_read( &s, 4 );
                for( uint32_t i=0; i<count; i++ )
                {
                    bs_read_ue( &s );
                    bs_read_ue( &s );
                    bs_read( &s, 1 );
                }
                bs_read( &s, 5 );
                p_sps->vui.i_cpb_removal_delay_length_minus1 = bs_read( &s, 5 );
                p_sps->vui.i_dpb_output_delay_length_minus1 = bs_read( &s, 5 );
                bs_read( &s, 5 );
            }
        }

        if( p_sps->vui.b_cpb_dpb_delays_present_flag )
            bs_read( &s, 1 );

        /* pic struct info */
        p_sps->vui.b_pic_struct_present_flag = bs_read( &s, 1 );

        /* + unparsed remains */
    }

    free( pb_dec );

    return 0;
}

int h264_parse_pps( const uint8_t *p_pps_buf, int i_pps_size,
                    struct nal_pps *p_pps )
{
    bs_t s;

    if (i_pps_size < 5 || (p_pps_buf[4] & 0x1f) != NAL_PPS)
        return -1;

    memset( p_pps, 0, sizeof(struct nal_pps) );
    bs_init( &s, &p_pps_buf[5], i_pps_size - 5 );
    p_pps->i_id = bs_read_ue( &s ); // pps id
    p_pps->i_sps_id = bs_read_ue( &s ); // sps id
    if( p_pps->i_id >= PPS_MAX || p_pps->i_sps_id >= SPS_MAX )
    {
        return -1;
    }
    bs_skip( &s, 1 ); // entropy coding mode flag
    p_pps->i_pic_order_present_flag = bs_read( &s, 1 );
    /* TODO */

    return 0;
}

block_t *h264_create_avcdec_config_record( size_t i_nal_length_size,
                                           struct nal_sps *p_sps,
                                           const uint8_t *p_sps_buf,
                                           size_t i_sps_size,
                                           const uint8_t *p_pps_buf,
                                           size_t i_pps_size )
{
    bo_t bo;

    /* The length of the NAL size is encoded using 1, 2 or 4 bytes */
    if( i_nal_length_size != 1 && i_nal_length_size != 2
     && i_nal_length_size != 4 )
        return NULL;

    /* 6 * int(8), i_sps_size - 4, 1 * int(8), i_pps_size - 4 */
    if( bo_init( &bo, 7 + i_sps_size + i_pps_size - 8 ) != true )
        return NULL;

    bo_add_8( &bo, 1 ); /* configuration version */
    bo_add_8( &bo, p_sps->i_profile );
    bo_add_8( &bo, p_sps->i_profile_compatibility );
    bo_add_8( &bo, p_sps->i_level );
    bo_add_8( &bo, 0xfc | (i_nal_length_size - 1) ); /* 0b11111100 | lengthsize - 1*/

    bo_add_8( &bo, 0xe0 | (i_sps_size > 0 ? 1 : 0) ); /* 0b11100000 | sps_count */

    if( i_sps_size > 4 )
    {
        /* the SPS data we have got includes 4 leading
         * bytes which we need to remove */
        bo_add_16be( &bo, i_sps_size - 4 );
        bo_add_mem( &bo, i_sps_size - 4, p_sps_buf + 4 );
    }

    bo_add_8( &bo, (i_pps_size > 0 ? 1 : 0) ); /* pps_count */
    if( i_pps_size > 4 )
    {
        /* the PPS data we have got includes 4 leading
         * bytes which we need to remove */
        bo_add_16be( &bo, i_pps_size - 4 );
        bo_add_mem( &bo, i_pps_size - 4, p_pps_buf + 4 );
    }
    return bo.b;
}

bool h264_get_profile_level(const es_format_t *p_fmt, size_t *p_profile,
                            size_t *p_level, size_t *p_nal_length_size)
{
    uint8_t *p = (uint8_t*)p_fmt->p_extra;
    if(!p || !p_fmt->p_extra) return false;

    /* Check the profile / level */
    if (p_fmt->i_original_fourcc == VLC_FOURCC('a','v','c','1') && p[0] == 1)
    {
        if (p_fmt->i_extra < 12) return false;
        if (p_nal_length_size) *p_nal_length_size = 1 + (p[4]&0x03);
        if (!(p[5]&0x1f)) return false;
        p += 8;
    }
    else
    {
        if (p_fmt->i_extra < 8) return false;
        if (!p[0] && !p[1] && !p[2] && p[3] == 1) p += 4;
        else if (!p[0] && !p[1] && p[2] == 1) p += 3;
        else return false;
    }

    if ( ((*p++)&0x1f) != 7) return false;

    /* Get profile/level out of first SPS */
    if (p_profile) *p_profile = p[0];
    if (p_level) *p_level = p[2];
    return true;
}
