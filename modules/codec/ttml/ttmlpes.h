/*****************************************************************************
 * ttmlpes.c : TTML subtitles in EN 303 560 payload
 *****************************************************************************
 * Copyright (C) 2018 VideoLabs, VLC authors and VideoLAN
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

#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_stream.h>

enum
{
    TTML_UNCOMPRESSED_DOCUMENT = 0x01,
    TTML_GZIP_COMPRESSED_DOCUMENT = 0x02,
};

struct ttml_in_pes_ctx
{
    vlc_tick_t i_offset; /* relative segment offset to apply */
    vlc_tick_t i_prev_block_time; /* because blocks are duplicated */
    vlc_tick_t i_prev_segment_start_time; /* because content can overlap */
};

static void ttml_in_pes_Init(struct ttml_in_pes_ctx *p)
{
    p->i_offset = 0;
    p->i_prev_block_time = -1;
    p->i_prev_segment_start_time = -1;
}

static inline bool TTML_in_PES(decoder_t *p_dec)
{
    return p_dec->fmt_in.i_codec == VLC_CODEC_TTML_TS;
}

static block_t * DecompressTTML( decoder_t *p_dec, const uint8_t *p_data, size_t i_data )
{
    block_t *p_decomp = NULL;
    block_t **pp_append = &p_decomp;

    stream_t *s = vlc_stream_MemoryNew( p_dec, (uint8_t *) p_data, i_data, true );
    if( !s )
        return NULL;
    stream_t *p_inflate = vlc_stream_FilterNew( s, "inflate" );
    if( p_inflate )
    {
        for( ;; )
        {
            *pp_append = vlc_stream_Block( p_inflate, 64 * 1024 );
            if( *pp_append == NULL ||
                (*pp_append)->i_buffer < 64*1024 )
                break;
            pp_append = &((*pp_append)->p_next);
        }
        s = p_inflate;
    }
    vlc_stream_Delete( s );
    return p_decomp ? block_ChainGather( p_decomp ) : NULL;
}

static int ParsePESEncap( decoder_t *p_dec,
                          struct ttml_in_pes_ctx *p_ctx,
                          int(*pf_decode)(decoder_t *, block_t *),
                          block_t *p_block )
{
    if( p_block == NULL ) /* No Drain */
        return VLCDEC_SUCCESS;

    if( p_block->i_buffer < 7 )
    {
        block_Release( p_block );
        return VLC_EGENERIC;
    }

    if( p_block->i_dts == p_ctx->i_prev_block_time )
    {
        block_Release( p_block );
        return VLC_SUCCESS;
    }

    int64_t i_segment_base = GetDWBE(p_block->p_buffer);
    i_segment_base = (i_segment_base << 16) | GetWBE(&p_block->p_buffer[4]);
    i_segment_base *= 100;
    uint8_t i_num_segments = p_block->p_buffer[6];
    size_t i_data = p_block->i_buffer - 7;
    const uint8_t *p_data = &p_block->p_buffer[7];
    p_ctx->i_offset = (p_block->i_dts - VLC_TICK_0) - i_segment_base;
    for( uint8_t i=0; i<i_num_segments; i++ )
    {
        if( i_data < 3 )
            break;
        uint8_t i_type = p_data[0];
        uint16_t i_size = GetWBE(&p_data[1]);
        if( i_size > i_data - 3 )
            break;
        block_t *p_segment = NULL;
        if( i_type == TTML_UNCOMPRESSED_DOCUMENT )
        {
            p_segment = block_Alloc( i_size );
            if( p_segment )
                memcpy( p_segment->p_buffer, &p_data[3], i_size );
        }
        else if( i_type == TTML_GZIP_COMPRESSED_DOCUMENT )
        {
            p_segment = DecompressTTML( p_dec, &p_data[3], i_size );
        }

        if( p_segment )
        {
            block_CopyProperties( p_segment, p_block );
            pf_decode( p_dec, p_segment );
        }

        p_data += 3 + i_size;
        i_data -= 3 + i_size;
    }

    p_ctx->i_prev_block_time = p_block->i_dts;
    block_Release( p_block );
    return VLC_SUCCESS;
}
