/*****************************************************************************
 * pes.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: pes.c,v 1.7 2003/06/10 22:42:59 gbazin Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
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

#define PES_PAYLOAD_SIZE_MAX 65500

static inline int PESHeader( uint8_t *p_hdr, mtime_t i_pts, mtime_t i_dts,
                             int i_es_size, int i_stream_id, int i_private_id )
{
    bits_buffer_t bits;

    bits_initwrite( &bits, 30, p_hdr );

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
//            if( b_mpeg2 ) // FIXME unsupported
            {
                int     i_pts_dts;
                int     i_extra = 0;

                /* For PES_PRIVATE_STREAM_1 there is an extra header after the
                   pes header */
                if( i_stream_id == PES_PRIVATE_STREAM_1 )
                {
                    i_extra = 1;
                    if( ( i_private_id&0xf0 ) == 0x80 )
                    {
                        i_extra += 3;
                    }
                }

                if( i_pts >= 0 && i_dts >= 0 )
                {
                    bits_write( &bits, 16, i_es_size + i_extra+ 13 );
                    i_pts_dts = 0x03;
                }
                else if( i_pts >= 0 )
                {
                    bits_write( &bits, 16, i_es_size  + i_extra + 8 );
                    i_pts_dts = 0x02;
                }
                else
                {
                    bits_write( &bits, 16, i_es_size  + i_extra + 3 );
                    i_pts_dts = 0x00;
                }

                bits_write( &bits, 2, 0x02 ); // mpeg2 id
                bits_write( &bits, 2, 0x00 ); // pes scrambling control
                bits_write( &bits, 1, 0x00 ); // pes priority
                bits_write( &bits, 1, 0x00 ); // data alignement indicator
                bits_write( &bits, 1, 0x00 ); // copyright
                bits_write( &bits, 1, 0x00 ); // original or copy

                bits_write( &bits, 2, i_pts_dts ); // pts_dts flags
                bits_write( &bits, 1, 0x00 ); // escr flags
                bits_write( &bits, 1, 0x00 ); // es rate flag
                bits_write( &bits, 1, 0x00 ); // dsm trick mode flag
                bits_write( &bits, 1, 0x00 ); // additional copy info flag
                bits_write( &bits, 1, 0x00 ); // pes crc flag
                bits_write( &bits, 1, 0x00 ); // pes extention flags
                if( i_pts_dts == 0x03 )
                {
                    bits_write( &bits, 8, 0x0a ); // header size -> pts and dts
                }
                else if( i_pts_dts == 0x02 )
                {
                    bits_write( &bits, 8, 0x05 ); // header size -> pts
                }
                else
                {
                    bits_write( &bits, 8, 0x00 ); // header size -> 0
                }

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

int E_( EStoPES )( sout_instance_t *p_sout,
                   sout_buffer_t **pp_pes,
                   sout_buffer_t *p_es,
                   int i_stream_id,
                   int b_mpeg2 )
{
    sout_buffer_t *p_es_sav, *p_pes;
    mtime_t i_pts, i_dts;

    uint8_t *p_data;
    int     i_size;

    int     i_private_id = -1;

    uint8_t header[30];     // PES header + extra < 30 (more like 17)
    int     i_pes_payload;
    int     i_pes_header;

    /* HACK for private stream 1 in ps */
    if( ( i_stream_id >> 8 ) == PES_PRIVATE_STREAM_1 )
    {
        i_private_id = i_stream_id & 0xff;
        i_stream_id  = PES_PRIVATE_STREAM_1;
    }

    i_pts = p_es->i_pts < 0 ? -1 : p_es->i_pts * 9 / 100; // 90000 units clock
    i_dts = p_es->i_dts < 0 ? -1 : p_es->i_dts * 9 / 100; // 90000 units clock

    i_size = p_es->i_size;
    p_data = p_es->p_buffer;

    *pp_pes = p_pes = NULL;
    p_es_sav = p_es;

    do
    {
        i_pes_payload = __MIN( i_size, PES_PAYLOAD_SIZE_MAX );
        i_pes_header  = PESHeader( header, i_pts, i_dts, i_pes_payload,
                                   i_stream_id, i_private_id );
        i_dts = -1; // only first PES has a dts/pts
        i_pts = -1;

        if( p_es  )
        {
            if( sout_BufferReallocFromPreHeader( p_sout, p_es, i_pes_header ) )
            {
                msg_Err( p_sout,
                         "cannot realloc preheader (should never happen)" );
                return( -1 );
            }
            /* reuse p_es for first frame */
            *pp_pes = p_pes = p_es;
            /* don't touch i_dts, i_pts, i_length as are already set :) */
            p_es = NULL;
        }
        else
        {
            p_pes->p_next = sout_BufferNew( p_sout,
                                            i_pes_header + i_pes_payload );
            p_pes = p_pes->p_next;

            p_pes->i_dts    = 0;
            p_pes->i_pts    = 0;
            p_pes->i_length = 0;
            if( i_pes_payload > 0 )
            {
                p_sout->p_vlc->pf_memcpy( p_pes->p_buffer + i_pes_header,
                                          p_data, i_pes_payload );
            }
        }

        /* copy header */
        memcpy( p_pes->p_buffer, header, i_pes_header );

        i_size -= i_pes_payload;
        p_data += i_pes_payload;
        p_pes->i_size =  i_pes_header + i_pes_payload;

    } while( i_size > 0 );

    /* sav some space */
    if( p_es_sav->i_size + 10*1024 < p_es_sav->i_buffer_size )
    {
        sout_BufferRealloc( p_sout, p_es_sav, p_es_sav->i_size );
    }

    return( 0 );
}
