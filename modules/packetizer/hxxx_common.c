/*****************************************************************************
 * hxxx_common.c: AVC/HEVC packetizers shared code
 *****************************************************************************
 * Copyright (C) 2001-2015 VLC authors and VideoLAN
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
#include "hxxx_common.h"

#include <vlc_block.h>
#include <vlc_codec.h>

block_t *CreateAnnexbNAL( const uint8_t *p, size_t i_size )
{
    block_t *p_nal;

    p_nal = block_Alloc( 4 + i_size );
    if( !p_nal ) return NULL;

    /* Add start code */
    p_nal->p_buffer[0] = 0x00;
    p_nal->p_buffer[1] = 0x00;
    p_nal->p_buffer[2] = 0x00;
    p_nal->p_buffer[3] = 0x01;

    /* Copy nalu */
    memcpy( &p_nal->p_buffer[4], p, i_size );

    return p_nal;
}

/****************************************************************************
 * PacketizeXXC1: Takes VCL blocks of data and creates annexe B type NAL stream
 * Will always use 4 byte 0 0 0 1 startcodes
 * Will prepend a SPS and PPS before each keyframe
 ****************************************************************************/
block_t *PacketizeXXC1( decoder_t *p_dec, uint8_t i_nal_length_size,
                        block_t **pp_block, pf_annexb_nal_packetizer pf_nal_parser )
{
    block_t       *p_block;
    block_t       *p_ret = NULL;
    uint8_t       *p;

    if( !pp_block || !*pp_block )
        return NULL;
    if( (*pp_block)->i_flags&(BLOCK_FLAG_CORRUPTED) )
    {
        block_Release( *pp_block );
        return NULL;
    }

    p_block = *pp_block;
    *pp_block = NULL;

    for( p = p_block->p_buffer; p < &p_block->p_buffer[p_block->i_buffer]; )
    {
        block_t *p_pic;
        bool b_dummy;
        int i_size = 0;
        int i;

        for( i = 0; i < i_nal_length_size; i++ )
        {
            i_size = (i_size << 8) | (*p++);
        }

        if( i_size <= 0 ||
            i_size > ( p_block->p_buffer + p_block->i_buffer - p ) )
        {
            msg_Err( p_dec, "Broken frame : size %d is too big", i_size );
            break;
        }

        block_t *p_part = CreateAnnexbNAL( p, i_size );
        if( !p_part )
            break;

        p_part->i_dts = p_block->i_dts;
        p_part->i_pts = p_block->i_pts;

        /* Parse the NAL */
        if( ( p_pic = pf_nal_parser( p_dec, &b_dummy, p_part ) ) )
        {
            block_ChainAppend( &p_ret, p_pic );
        }
        p += i_size;
    }
    block_Release( p_block );

    return p_ret;
}
