/*****************************************************************************
 * pes.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: pes.c,v 1.1 2002/12/14 21:32:41 fenrir Exp $
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
#elif defined( _MSC_VER ) && defined( _WIN32 ) && !defined( UNDER_CE )
#   include <io.h>
#endif

#include "codecs.h"
#include "pes.h"
#include "bits.h"

int EStoPES( sout_instance_t *p_sout,
             sout_buffer_t **pp_pes,
             sout_buffer_t *p_es,
             int i_stream_id,
             int b_mpeg2 )
{
    sout_buffer_t *p_pes;
    bits_buffer_t bits;
    mtime_t i_pts, i_dts;
    uint8_t *p_data;
    int     i_size;

    i_pts = p_es->i_pts * 9 / 100; // 90000 units clock
    i_dts = p_es->i_dts * 9 / 100; // 90000 units clock

    i_size = p_es->i_size;
    p_data = p_es->p_buffer;

    *pp_pes = p_pes = NULL;

    do
    {
        int     i_copy;

        i_copy = __MIN( i_size, 65500 );

        if( *pp_pes == NULL )
        {
            *pp_pes = p_pes = sout_BufferNew( p_sout, 100 + i_copy);
            p_pes->i_dts = p_es->i_dts;
            p_pes->i_pts = p_es->i_pts;
            p_pes->i_length = p_es->i_length;
        }
        else
        {
            p_pes->p_next = sout_BufferNew( p_sout, 100 + i_copy );
            p_pes = p_pes->p_next;

            p_pes->i_dts    = 0;
            p_pes->i_pts    = 0;
            p_pes->i_length = 0;
        }

        p_pes->i_size = 0;


        bits_initwrite( &bits, 100, p_pes->p_buffer );

        /* first 4 bytes common pes header */
            /* add start code for pes 0x000001 */
        bits_write( &bits, 24, 0x01 );
            /* add stream id */
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
                bits_write( &bits, 16, i_copy );
                bits_align( &bits );
                break;

            default:
                /* arg, a little more difficult */
                if( b_mpeg2 )
                {
                    int     i_pts_dts;

                    if( i_dts > 0 )
                    {
                        bits_write( &bits, 16, i_copy  + 13 );
                        i_pts_dts = 0x03;
                    }
                    else
                    {
                        bits_write( &bits, 16, i_copy  + 8 );
                        i_pts_dts = 0x02;
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
                    if( i_pts_dts & 0x01 )
                    {
                        bits_write( &bits, 8, 0x0a ); // header size -> pts and dts
                    }
                    else
                    {
                        bits_write( &bits, 8, 0x05 ); // header size -> pts
                    }

                    /* write pts */
                    bits_write( &bits, 4, i_pts_dts ); // '0010' or '0011'
                    bits_write( &bits, 3, i_pts >> 30 );
                    bits_write( &bits, 1, 0x01 ); // marker
                    bits_write( &bits, 15, i_pts >> 15 );
                    bits_write( &bits, 1, 0x01 ); // marker
                    bits_write( &bits, 15, i_pts );
                    bits_write( &bits, 1, 0x01 ); // marker

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

                        i_dts = 0; // write dts only once
                    }

                }
                else
                {
                    msg_Warn( p_sout, "es isn't mpeg2 -->corrupted pes" );
                }
                /* now should be stuffing */
                /* and then pes data */

                bits_align( &bits );
                break;
        }

        if( i_copy > 0 )
        {
            memcpy( p_pes->p_buffer + bits.i_data, p_data, i_copy );
        }

        i_size -= i_copy;
        p_data += i_copy;
        p_pes->i_size = bits.i_data + i_copy;


    } while( i_size > 0 );

    sout_BufferDelete( p_sout, p_es );
    return( 0 );
}

