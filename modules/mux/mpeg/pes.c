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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include "codecs.h"
#include "pes.h"
#include "bits.h"

static inline int PESHeader( uint8_t *p_hdr, mtime_t i_pts, mtime_t i_dts,
                             int i_es_size, es_format_t *p_fmt,
                             int i_stream_id, int i_private_id,
                             vlc_bool_t b_mpeg2, vlc_bool_t b_data_alignment,
                             int i_header_size )
{
    bits_buffer_t bits;
    int     i_extra = 0;

    /* For PES_PRIVATE_STREAM_1 there is an extra header after the
       pes header */
    /* i_private_id != -1 because TS use 0xbd without private_id */
    if( i_stream_id == PES_PRIVATE_STREAM_1 && i_private_id != -1 )
    {
        i_extra = 1;
        if( ( i_private_id & 0xf0 ) == 0x80 )
        {
            i_extra += 3;
        }
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

                if( i_pts > 0 && i_dts > 0 &&
                    ( i_pts != i_dts || ( p_fmt->i_cat == VIDEO_ES &&
                      p_fmt->i_codec != VLC_FOURCC('m','p','g','v') ) ) )
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
                bits_write( &bits, 1, 0x00 ); // pes extention flags
                bits_write( &bits, 8, i_header_size ); // header size -> pts and dts

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

int E_( EStoPES )( sout_instance_t *p_sout, block_t **pp_pes, block_t *p_es,
                   es_format_t *p_fmt, int i_stream_id,
                   int b_mpeg2, int b_data_alignment, int i_header_size,
                   int i_max_pes_size )
{
    block_t *p_pes;
    mtime_t i_pts, i_dts, i_length;

    uint8_t *p_data;
    int     i_size;

    int     i_private_id = -1;

    uint8_t header[50];     // PES header + extra < 50 (more like 17)
    int     i_pes_payload;
    int     i_pes_header;

    int     i_pes_count = 1;

    /* HACK for private stream 1 in ps */
    if( ( i_stream_id >> 8 ) == PES_PRIVATE_STREAM_1 )
    {
        i_private_id = i_stream_id & 0xff;
        i_stream_id  = PES_PRIVATE_STREAM_1;
    }

    if( p_fmt->i_codec == VLC_FOURCC( 'm', 'p','4', 'v' ) &&
        p_es->i_flags & BLOCK_FLAG_TYPE_I )
    {
        /* For MPEG4 video, add VOL before I-frames */
        p_es = block_Realloc( p_es, p_fmt->i_extra, p_es->i_buffer );

        memcpy( p_es->p_buffer, p_fmt->p_extra, p_fmt->i_extra );
    }

    i_pts = p_es->i_pts <= 0 ? 0 : p_es->i_pts * 9 / 100; // 90000 units clock
    i_dts = p_es->i_dts <= 0 ? 0 : p_es->i_dts * 9 / 100; // 90000 units clock

    i_size = p_es->i_buffer;
    p_data = p_es->p_buffer;

    *pp_pes = p_pes = NULL;

#ifdef DEBUG
    memset( header, 0, 50 );
#endif

    do
    {
        i_pes_payload = __MIN( i_size, (i_max_pes_size ?
                               i_max_pes_size : PES_PAYLOAD_SIZE_MAX) );
        i_pes_header  = PESHeader( header, i_pts, i_dts, i_pes_payload,
                                   p_fmt, i_stream_id, i_private_id, b_mpeg2,
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
            p_pes->p_next = block_New( p_sout, i_pes_header + i_pes_payload );
            p_pes = p_pes->p_next;

            p_pes->i_dts    = 0;
            p_pes->i_pts    = 0;
            p_pes->i_length = 0;
            if( i_pes_payload > 0 )
            {
                p_sout->p_vlc->pf_memcpy( p_pes->p_buffer + i_pes_header,
                                          p_data, i_pes_payload );
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
