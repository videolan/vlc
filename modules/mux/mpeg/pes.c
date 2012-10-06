/*****************************************************************************
 * pes.c: PES packetizer used by the MPEG multiplexers
 *****************************************************************************
 * Copyright (C) 2001, 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_sout.h>
#include <vlc_block.h>
#include <assert.h>

#include "pes.h"
#include "bits.h"

/** PESHeader, write a pes header
 * \param i_es_size length of payload data. (Must be < PES_PAYLOAD_SIZE_MAX
 *                  unless the conditions for unbounded PES packets are met)
 * \param i_stream_id stream id as follows:
 *                     - 0x00   - 0xff   : normal stream_id as per Table 2-18
 *                     - 0xfd00 - 0xfd7f : stream_id_extension = low 7 bits
 *                                         (stream_id = PES_EXTENDED_STREAM_ID)
 *                     - 0xbd00 - 0xbdff : private_id = low 8 bits
 *                                         (stream_id = PES_PRIVATE_STREAM)
 * \param i_header_size length of padding data to insert into PES packet
 *                      header in bytes.
 */
static inline int PESHeader( uint8_t *p_hdr, mtime_t i_pts, mtime_t i_dts,
                             int i_es_size, es_format_t *p_fmt,
                             int i_stream_id, bool b_mpeg2,
                             bool b_data_alignment, int i_header_size )
{
    bits_buffer_t bits;
    int     i_extra = 0;
    int i_private_id = -1;
    int i_stream_id_extension = 0;

    /* HACK for private stream 1 in ps */
    if( ( i_stream_id >> 8 ) == PES_PRIVATE_STREAM_1 )
    {
        i_private_id = i_stream_id & 0xff;
        i_stream_id = PES_PRIVATE_STREAM_1;
        /* For PES_PRIVATE_STREAM_1 there is an extra header after the
           pes header */
        /* i_private_id != -1 because TS use 0xbd without private_id */
        i_extra = 1;
        if( ( i_private_id & 0xf0 ) == 0x80 )
            i_extra += 3;
    }
    else if( ( i_stream_id >> 8 ) == PES_EXTENDED_STREAM_ID )
    {
        /* Enable support for extended_stream_id as defined in
         * ISO/IEC 13818-1:2000/Amd.2:2003 */
        /* NB, i_extended_stream_id is limited to 7 bits */
        i_stream_id_extension = i_stream_id & 0x7f;
        i_stream_id = PES_EXTENDED_STREAM_ID;
    }

    bits_initwrite( &bits, 50, p_hdr );

    /* add start code */
    bits_write( &bits, 24, 0x01 );
    bits_write( &bits, 8, i_stream_id );
    switch( i_stream_id )
    {
        case PES_PROGRAM_STREAM_MAP:
        case PES_PADDING:
        case PES_PRIVATE_STREAM_2:
        case PES_ECM:
        case PES_EMM:
        case PES_PROGRAM_STREAM_DIRECTORY:
        case PES_DSMCC_STREAM:
        case PES_ITU_T_H222_1_TYPE_E_STREAM:
            /* add pes data size  */
            bits_write( &bits, 16, i_es_size );
            bits_align( &bits );
            return( bits.i_data );

        default:
            /* arg, a little more difficult */
            if( b_mpeg2 )
            {
                int i_pts_dts;
                bool b_pes_extension_flag = false;

                if( i_pts > 0 && i_dts > 0 &&
                    ( i_pts != i_dts || ( p_fmt->i_cat == VIDEO_ES &&
                      p_fmt->i_codec != VLC_CODEC_MPGV ) ) )
                {
                    i_pts_dts = 0x03;
                    if ( !i_header_size ) i_header_size = 0xa;
                }
                else if( i_pts > 0 )
                {
                    i_pts_dts = 0x02;
                    if ( !i_header_size ) i_header_size = 0x5;
                }
                else
                {
                    i_pts_dts = 0x00;
                    if ( !i_header_size ) i_header_size = 0x0;
                }

                if( i_stream_id == PES_EXTENDED_STREAM_ID )
                {
                    b_pes_extension_flag = true;
                    i_header_size += 1 + 1;
                }

                if( b_pes_extension_flag )
                {
                    i_header_size += 1;
                }

                /* Unbounded streams are only allowed in TS (not PS) and only
                 * for some ES, eg. MPEG* Video ES or Dirac ES. */
                if( i_es_size > PES_PAYLOAD_SIZE_MAX )
                    bits_write( &bits, 16, 0 ); // size unbounded
                else
                    bits_write( &bits, 16, i_es_size + i_extra + 3
                                 + i_header_size ); // size
                bits_write( &bits, 2, 0x02 ); // mpeg2 id
                bits_write( &bits, 2, 0x00 ); // pes scrambling control
                bits_write( &bits, 1, 0x00 ); // pes priority
                bits_write( &bits, 1, b_data_alignment ); // data alignement indicator
                bits_write( &bits, 1, 0x00 ); // copyright
                bits_write( &bits, 1, 0x00 ); // original or copy

                bits_write( &bits, 2, i_pts_dts ); // pts_dts flags
                bits_write( &bits, 1, 0x00 ); // escr flags
                bits_write( &bits, 1, 0x00 ); // es rate flag
                bits_write( &bits, 1, 0x00 ); // dsm trick mode flag
                bits_write( &bits, 1, 0x00 ); // additional copy info flag
                bits_write( &bits, 1, 0x00 ); // pes crc flag
                bits_write( &bits, 1, b_pes_extension_flag );
                bits_write( &bits, 8, i_header_size );

                /* write pts */
                if( i_pts_dts & 0x02 )
                {
                    bits_write( &bits, 4, i_pts_dts ); // '0010' or '0011'
                    bits_write( &bits, 3, i_pts >> 30 );
                    bits_write( &bits, 1, 0x01 ); // marker
                    bits_write( &bits, 15, i_pts >> 15 );
                    bits_write( &bits, 1, 0x01 ); // marker
                    bits_write( &bits, 15, i_pts );
                    bits_write( &bits, 1, 0x01 ); // marker
                    i_header_size -= 0x5;
                }
                /* write i_dts */
                if( i_pts_dts & 0x01 )
                {
                    bits_write( &bits, 4, 0x01 ); // '0001'
                    bits_write( &bits, 3, i_dts >> 30 );
                    bits_write( &bits, 1, 0x01 ); // marker
                    bits_write( &bits, 15, i_dts >> 15 );
                    bits_write( &bits, 1, 0x01 ); // marker
                    bits_write( &bits, 15, i_dts );
                    bits_write( &bits, 1, 0x01 ); // marker
                    i_header_size -= 0x5;
                }
                if( b_pes_extension_flag )
                {
                    bits_write( &bits, 1, 0x00 ); // PES_private_data_flag
                    bits_write( &bits, 1, 0x00 ); // pack_header_field_flag
                    bits_write( &bits, 1, 0x00 ); // program_packet_sequence_counter_flag
                    bits_write( &bits, 1, 0x00 ); // P-STD_buffer_flag
                    bits_write( &bits, 3, 0x07 ); // reserved
                    bits_write( &bits, 1, 0x01 ); // PES_extension_flag_2
                    /* skipping unsupported parts: */
                    /*   PES_private_data */
                    /*   pack_header */
                    /*   program_packet_sequence_counter */
                    /*   P-STD_buffer_flag */
                    if( i_stream_id == PES_EXTENDED_STREAM_ID )
                    {
                        /* PES_extension_2 */
                        bits_write( &bits, 1, 0x01 ); // marker
                        bits_write( &bits, 7, 0x01 ); // PES_extension_field_length
                        bits_write( &bits, 1, 0x01 ); // stream_id_extension_flag
                        bits_write( &bits, 7, i_stream_id_extension );
                        i_header_size -= 0x2;
                    }
                    i_header_size -= 0x1;
                }
                while ( i_header_size )
                {
                    bits_write( &bits, 8, 0xff );
                    i_header_size--;
                }
            }
            else /* MPEG1 */
            {
                int i_pts_dts;

                if( i_pts > 0 && i_dts > 0 &&
                    ( i_pts != i_dts || p_fmt->i_cat == VIDEO_ES ) )
                {
                    bits_write( &bits, 16, i_es_size + i_extra + 10 /* + stuffing */ );
                    i_pts_dts = 0x03;
                }
                else if( i_pts > 0 )
                {
                    bits_write( &bits, 16, i_es_size + i_extra + 5 /* + stuffing */ );
                    i_pts_dts = 0x02;
                }
                else
                {
                    bits_write( &bits, 16, i_es_size + i_extra + 1 /* + stuffing */);
                    i_pts_dts = 0x00;
                }

                /* FIXME: Now should be stuffing */

                /* No STD_buffer_scale and STD_buffer_size */

                /* write pts */
                if( i_pts_dts & 0x02 )
                {
                    bits_write( &bits, 4, i_pts_dts ); // '0010' or '0011'
                    bits_write( &bits, 3, i_pts >> 30 );
                    bits_write( &bits, 1, 0x01 ); // marker
                    bits_write( &bits, 15, i_pts >> 15 );
                    bits_write( &bits, 1, 0x01 ); // marker
                    bits_write( &bits, 15, i_pts );
                    bits_write( &bits, 1, 0x01 ); // marker
                }
                /* write i_dts */
                if( i_pts_dts & 0x01 )
                {
                    bits_write( &bits, 4, 0x01 ); // '0001'
                    bits_write( &bits, 3, i_dts >> 30 );
                    bits_write( &bits, 1, 0x01 ); // marker
                    bits_write( &bits, 15, i_dts >> 15 );
                    bits_write( &bits, 1, 0x01 ); // marker
                    bits_write( &bits, 15, i_dts );
                    bits_write( &bits, 1, 0x01 ); // marker
                }
                if( !i_pts_dts )
                {
                    bits_write( &bits, 8, 0x0F );
                }

            }

            /* now should be stuffing */
            /* and then pes data */

            bits_align( &bits );
            if( i_stream_id == PES_PRIVATE_STREAM_1 && i_private_id != -1 )
            {
                bits_write( &bits, 8, i_private_id );
                if( ( i_private_id&0xf0 ) == 0x80 )
                {
                    bits_write( &bits, 24, 0 ); // ac3
                }
            }
            bits_align( &bits );
            return( bits.i_data );
    }
}

/** EStoPES, encapsulate an elementary stream block into PES packet(s)
 * each with a maximal payload size of @i_max_pes_size@.
 *
 * In some circumstances, unbounded PES packets are allowed:
 *  - Transport streams only (NOT programme streams)
 *  - Only some types of elementary streams (eg MPEG2 video)
 * It is the responsibility of the caller to enforce these constraints.
 *
 * EStoPES will only produce an unbounded PES packet if:
 *  - ES is VIDEO_ES
 *  - i_max_pes_size > PES_PAYLOAD_SIZE_MAX
 *  - length of p_es > PES_PAYLOAD_SIZE_MAX
 * If the last condition is not met, a single PES packet is produced
 * which is not unbounded in length.
 *
 * \param i_stream_id stream id as follows:
 *                     - 0x00   - 0xff   : normal stream_id as per Table 2-18
 *                     - 0xfd00 - 0xfd7f : stream_id_extension = low 7 bits
 *                                         (stream_id = PES_EXTENDED_STREAM_ID)
 *                     - 0xbd00 - 0xbdff : private_id = low 8 bits
 *                                         (stream_id = PES_PRIVATE_STREAM)
 * \param i_header_size length of padding data to insert into PES packet
 *                      header in bytes.
 * \param i_max_pes_size maximum length of each pes packet payload.
 *                       if zero, uses default maximum.
 *                       To allow unbounded PES packets in transport stream
 *                       VIDEO_ES, set to INT_MAX.
 */
int  EStoPES ( block_t **pp_pes, block_t *p_es,
                   es_format_t *p_fmt, int i_stream_id,
                   int b_mpeg2, int b_data_alignment, int i_header_size,
                   int i_max_pes_size )
{
    block_t *p_pes;
    mtime_t i_pts, i_dts, i_length;

    uint8_t *p_data;
    int     i_size;

    uint8_t header[50];     // PES header + extra < 50 (more like 17)
    int     i_pes_payload;
    int     i_pes_header;

    int     i_pes_count = 1;

    assert( i_max_pes_size >= 0 );
    assert( i_header_size >= 0 );

    /* NB, Only video ES may have unbounded length */
    if( !i_max_pes_size ||
        ( p_fmt->i_cat != VIDEO_ES && i_max_pes_size > PES_PAYLOAD_SIZE_MAX ) )
    {
        i_max_pes_size = PES_PAYLOAD_SIZE_MAX;
    }

    if( ( p_fmt->i_codec == VLC_CODEC_MP4V ||
          p_fmt->i_codec == VLC_CODEC_H264 ) &&
        p_es->i_flags & BLOCK_FLAG_TYPE_I )
    {
        /* For MPEG4 video, add VOL before I-frames,
           for H264 add SPS/PPS before keyframes*/
        p_es = block_Realloc( p_es, p_fmt->i_extra, p_es->i_buffer );

        memcpy( p_es->p_buffer, p_fmt->p_extra, p_fmt->i_extra );
    }

    if( p_fmt->i_codec == VLC_CODEC_H264 )
    {
        unsigned offset=2;
        while(offset < p_es->i_buffer )
        {
            if( p_es->p_buffer[offset-2] == 0 &&
                p_es->p_buffer[offset-1] == 0 &&
                p_es->p_buffer[offset] == 1 )
                break;
            offset++;
        }
        offset++;
        if( offset <= p_es->i_buffer-4 &&
            ((p_es->p_buffer[offset] & 0x1f) != 9) ) /* Not AUD */
        {
            /* Make similar AUD as libavformat does */
            p_es = block_Realloc( p_es, 6, p_es->i_buffer );
            p_es->p_buffer[0] = 0x00;
            p_es->p_buffer[1] = 0x00;
            p_es->p_buffer[2] = 0x00;
            p_es->p_buffer[3] = 0x01;
            p_es->p_buffer[4] = 0x09;
            p_es->p_buffer[5] = 0xe0;
        }

    }

    i_pts = p_es->i_pts <= 0 ? 0 : p_es->i_pts * 9 / 100; // 90000 units clock
    i_dts = p_es->i_dts <= 0 ? 0 : p_es->i_dts * 9 / 100; // 90000 units clock

    i_size = p_es->i_buffer;
    p_data = p_es->p_buffer;

    *pp_pes = p_pes = NULL;

    do
    {
        i_pes_payload = __MIN( i_size, i_max_pes_size );
        i_pes_header  = PESHeader( header, i_pts, i_dts, i_pes_payload,
                                   p_fmt, i_stream_id, b_mpeg2,
                                   b_data_alignment, i_header_size );
        i_dts = 0; // only first PES has a dts/pts
        i_pts = 0;

        if( p_es )
        {
            p_es = block_Realloc( p_es, i_pes_header, p_es->i_buffer );
            p_data = p_es->p_buffer+i_pes_header;
            /* reuse p_es for first frame */
            *pp_pes = p_pes = p_es;
            /* don't touch i_dts, i_pts, i_length as are already set :) */
            p_es = NULL;
        }
        else
        {
            p_pes->p_next = block_Alloc( i_pes_header + i_pes_payload );
            p_pes = p_pes->p_next;

            p_pes->i_dts    = 0;
            p_pes->i_pts    = 0;
            p_pes->i_length = 0;
            if( i_pes_payload > 0 )
            {
                memcpy( p_pes->p_buffer + i_pes_header, p_data,
                            i_pes_payload );
            }
            i_pes_count++;
        }

        /* copy header */
        memcpy( p_pes->p_buffer, header, i_pes_header );

        i_size -= i_pes_payload;
        p_data += i_pes_payload;
        p_pes->i_buffer =  i_pes_header + i_pes_payload;

    } while( i_size > 0 );

    /* Now redate all pes */
    i_dts    = (*pp_pes)->i_dts;
    i_length = (*pp_pes)->i_length / i_pes_count;
    for( p_pes = *pp_pes; p_pes != NULL; p_pes = p_pes->p_next )
    {
        p_pes->i_dts = i_dts;
        p_pes->i_length = i_length;

        i_dts += i_length;
    }

    return 0;
}
