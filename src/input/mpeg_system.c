/*****************************************************************************
 * mpeg_system.c: TS, PS and PES management
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: mpeg_system.c,v 1.35 2001/02/12 07:52:40 sam Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Michel Lespinasse <walken@via.ecp.fr>
 *          Benoît Steiner <benny@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
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
#include "defs.h"

#include <stdlib.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "intf_msg.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"

#include "input.h"
#include "mpeg_system.h"

#include "main.h"                           /* AC3/MPEG channel, SPU channel */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/


/*
 * PES Packet management
 */

/*****************************************************************************
 * MoveChunk
 *****************************************************************************
 * Small utility function used to parse discontinuous headers safely. Copies
 * i_buf_len bytes of data to a buffer and returns the size copied.
 * This is a variation on the theme of input_ext-dec.h:GetChunk().
 *****************************************************************************/
static __inline__ size_t MoveChunk( byte_t * p_dest,
                                    data_packet_t ** pp_data_src,
                                    byte_t ** pp_src,
                                    size_t i_buf_len )
{
    ptrdiff_t           i_available;

    if( (i_available = (*pp_data_src)->p_payload_end - *pp_src)
            >= i_buf_len )
    {
        if( p_dest != NULL )
            memcpy( p_dest, *pp_src, i_buf_len );
        *pp_src += i_buf_len;
        return( i_buf_len );
    }
    else
    {
        size_t          i_init_len = i_buf_len;

        do
        {
            if( p_dest != NULL )
                memcpy( p_dest, *pp_src, i_available );
            *pp_data_src = (*pp_data_src)->p_next;
            i_buf_len -= i_available;
            p_dest += i_available;
            if( *pp_data_src == NULL )
            {
                *pp_src = NULL;
                return( i_init_len - i_buf_len );
            }
            *pp_src = (*pp_data_src)->p_payload_start;
        }
        while( (i_available = (*pp_data_src)->p_payload_end - *pp_src)
                <= i_buf_len );

        if( i_buf_len )
        {
            if( p_dest != NULL )
                memcpy( p_dest, *pp_src, i_buf_len );
            *pp_src += i_buf_len;
        }
        return( i_init_len );
    }
}

/*****************************************************************************
 * input_ParsePES
 *****************************************************************************
 * Parse a finished PES packet and analyze its header.
 *****************************************************************************/
#define PES_HEADER_SIZE     7
void input_ParsePES( input_thread_t * p_input, es_descriptor_t * p_es )
{
    data_packet_t * p_data;
    byte_t *        p_byte;
    byte_t          p_header[PES_HEADER_SIZE];
    int             i_done;

#define p_pes (p_es->p_pes)

    //intf_DbgMsg("End of PES packet %p", p_pes);

    /* Parse the header. The header has a variable length, but in order
     * to improve the algorithm, we will read the 14 bytes we may be
     * interested in */
    p_data = p_pes->p_first;
    p_byte = p_data->p_payload_start;
    i_done = 0;

    if( MoveChunk( p_header, &p_data, &p_byte, PES_HEADER_SIZE )
            != PES_HEADER_SIZE )
    {
        intf_WarnMsg( 3, "PES packet too short to have a header" );
        p_input->pf_delete_pes( p_input->p_method_data, p_pes );
        p_pes = NULL;
        return;
    }

    /* Get the PES size if defined */
    p_es->i_pes_real_size = U16_AT(p_header + 4) + 6;

    /* First read the 6 header bytes common to all PES packets:
     * use them to test the PES validity */
    if( (p_header[0] || p_header[1] || (p_header[2] != 1)) )
    {
        /* packet_start_code_prefix != 0x000001 */
        intf_ErrMsg( "PES packet doesn't start with 0x000001 : data loss" );
        p_input->pf_delete_pes( p_input->p_method_data, p_pes );
        p_pes = NULL;
    }
    else
    {
        int i_pes_header_size, i_payload_size;

        if ( p_es->i_pes_real_size &&
             (p_es->i_pes_real_size != p_pes->i_pes_size) )
        {
            /* PES_packet_length is set and != total received payload */
            /* Warn the decoder that the data may be corrupt. */
            intf_WarnMsg( 3, "PES sizes do not match : packet corrupted" );
        }

        switch( p_es->i_stream_id )
        {
        case 0xBC:  /* Program stream map */
        case 0xBE:  /* Padding */
        case 0xBF:  /* Private stream 2 */
        case 0xB0:  /* ECM */
        case 0xB1:  /* EMM */
        case 0xFF:  /* Program stream directory */
        case 0xF2:  /* DSMCC stream */
        case 0xF8:  /* ITU-T H.222.1 type E stream */
            /* The payload begins immediately after the 6 bytes header, so
             * we have finished with the parsing */
            i_pes_header_size = 6;
            break;

        default:
            if( (p_header[6] & 0xC0) == 0x80 )
            {
                /* MPEG-2 : the PES header contains at least 3 more bytes. */
                size_t      i_max_len;
                boolean_t   b_has_pts, b_has_dts;
                byte_t      p_full_header[12];

                p_pes->b_data_alignment = p_header[6] & 0x04;

                i_max_len = MoveChunk( p_full_header, &p_data, &p_byte, 12 );
                if( i_max_len < 2 )
                {
                    intf_WarnMsg( 3,
                            "PES packet too short to have a MPEG-2 header" );
                    p_input->pf_delete_pes( p_input->p_method_data,
                                            p_pes );
                    p_pes = NULL;
                    return;
                }

                b_has_pts = p_full_header[0] & 0x80;
                b_has_dts = p_full_header[0] & 0x40;
                i_pes_header_size = p_full_header[1] + 9;

                /* Now parse the optional header extensions */
                if( b_has_pts )
                {
                    if( i_max_len < 7 )
                    {
                        intf_WarnMsg( 3,
                            "PES packet too short to have a MPEG-2 header" );
                        p_input->pf_delete_pes( p_input->p_method_data,
                                                p_pes );
                        p_pes = NULL;
                        return;
                    }
                    p_pes->i_pts = input_ClockGetTS( p_input, p_es->p_pgrm,
                    ( ((mtime_t)(p_full_header[2] & 0x0E) << 29) |
                      (((mtime_t)U16_AT(p_full_header + 3) << 14) - (1 << 14)) |
                      ((mtime_t)U16_AT(p_full_header + 5) >> 1) ) );

                    if( b_has_dts )
                    {
                        if( i_max_len < 12 )
                        {
                            intf_WarnMsg( 3,
                              "PES packet too short to have a MPEG-2 header" );
                            p_input->pf_delete_pes( p_input->p_method_data,
                                                    p_pes );
                            p_pes = NULL;
                            return;
                        }
                        p_pes->i_dts = input_ClockGetTS( p_input, p_es->p_pgrm,
                        ( ((mtime_t)(p_full_header[7] & 0x0E) << 29) |
                          (((mtime_t)U16_AT(p_full_header + 8) << 14)
                                - (1 << 14)) |
                          ((mtime_t)U16_AT(p_full_header + 10) >> 1) ) );
                    }
                }
            }
            else
            {
                /* Probably MPEG-1 */
                boolean_t       b_has_pts, b_has_dts;

                i_pes_header_size = 6;
                p_data = p_pes->p_first;
                p_byte = p_data->p_payload_start;
                /* Cannot fail because the previous one succeeded. */
                MoveChunk( NULL, &p_data, &p_byte, 6 );

                while( *p_byte == 0xFF && i_pes_header_size < 22 )
                {
                    i_pes_header_size++;
                    if( MoveChunk( NULL, &p_data, &p_byte, 1 ) != 1 )
                    {
                        intf_WarnMsg( 3,
                            "PES packet too short to have a MPEG-1 header" );
                        p_input->pf_delete_pes( p_input->p_method_data, p_pes );
                        p_pes = NULL;
                        return;
                    }
                }
                if( i_pes_header_size == 22 )
                {
                    intf_ErrMsg( "Too much MPEG-1 stuffing" );
                    p_input->pf_delete_pes( p_input->p_method_data, p_pes );
                    p_pes = NULL;
                    return;
                }

                if( (*p_byte & 0xC0) == 0x40 )
                {
                    /* Don't ask why... --Meuuh */
                    /* Erm... why ? --Sam */
                    /* Well... According to the recommendation, it is for
                     * STD_buffer_scale and STD_buffer_size. --Meuuh */
                    i_pes_header_size += 2;
                    if( MoveChunk( NULL, &p_data, &p_byte, 2 ) != 2 )
                    {
                        intf_WarnMsg( 3,
                            "PES packet too short to have a MPEG-1 header" );
                        p_input->pf_delete_pes( p_input->p_method_data, p_pes );
                        p_pes = NULL;
                        return;
                    }
                }

                i_pes_header_size++;

                b_has_pts = *p_byte & 0x20;
                b_has_dts = *p_byte & 0x10;

                if( b_has_pts )
                {
                    byte_t      p_ts[5];

                    i_pes_header_size += 4;
                    if( MoveChunk( p_ts, &p_data, &p_byte, 5 ) != 5 )
                    {
                        intf_WarnMsg( 3,
                            "PES packet too short to have a MPEG-1 header" );
                        p_input->pf_delete_pes( p_input->p_method_data, p_pes );
                        p_pes = NULL;
                        return;
                    }

                    p_pes->i_pts = input_ClockGetTS( p_input, p_es->p_pgrm,
                      ( ((mtime_t)(p_ts[0] & 0x0E) << 29) |
                        (((mtime_t)U16_AT(p_ts + 1) << 14) - (1 << 14)) |
                        ((mtime_t)U16_AT(p_ts + 3) >> 1) ) );

                    if( b_has_dts )
                    {
                        i_pes_header_size += 5;
                        if( MoveChunk( p_ts, &p_data, &p_byte, 5 ) != 5 )
                        {
                            intf_WarnMsg( 3,
                              "PES packet too short to have a MPEG-1 header" );
                            p_input->pf_delete_pes( p_input->p_method_data,
                                                    p_pes );
                            p_pes = NULL;
                            return;
                        }

                        p_pes->i_dts = input_ClockGetTS( p_input,
                                                         p_es->p_pgrm,
                            ( ((mtime_t)(p_ts[0] & 0x0E) << 29) |
                              (((mtime_t)U16_AT(p_ts + 1) << 14) - (1 << 14)) |
                              ((mtime_t)U16_AT(p_ts + 3) >> 1) ) );
                    }
                }
            }

            break;
        }

        if( p_es->i_stream_id == 0xbd )
        {
            /* With private stream 1, the first byte of the payload
             * is a stream_private_id, so skip it. */
            i_pes_header_size++;
        }

        /* Now we've parsed the header, we just have to indicate in some
         * specific data packets where the PES payload begins (renumber
         * p_payload_start), so that the decoders can find the beginning
         * of their data right out of the box. */
        p_data = p_pes->p_first;
        i_payload_size = p_data->p_payload_end
                                 - p_data->p_payload_start;
        while( i_pes_header_size > i_payload_size )
        {
            /* These packets are entirely filled by the PES header. */
            i_pes_header_size -= i_payload_size;
            p_data->p_payload_start = p_data->p_payload_end;
            /* Go to the next data packet. */
            if( (p_data = p_data->p_next) == NULL )
            {
                intf_ErrMsg( "PES header bigger than payload" );
                p_input->pf_delete_pes( p_input->p_method_data, p_pes );
                p_pes = NULL;
                return;
            }
            i_payload_size = p_data->p_payload_end
                                 - p_data->p_payload_start;
        }
        /* This last packet is partly header, partly payload. */
        if( i_payload_size < i_pes_header_size )
        {
            intf_ErrMsg( "PES header bigger than payload" );
            p_input->pf_delete_pes( p_input->p_method_data, p_pes );
            p_pes = NULL;
            return;
        }
        p_data->p_payload_start += i_pes_header_size;

        /* Now we can eventually put the PES packet in the decoder's
         * PES fifo */
        if( p_es->p_decoder_fifo != NULL )
        {
            input_DecodePES( p_es->p_decoder_fifo, p_pes );
        }
        else
        {
            intf_ErrMsg("No fifo to receive PES %p (who wrote this damn code ?)",
                        p_pes);
            p_input->pf_delete_pes( p_input->p_method_data, p_pes );
        }
        p_pes = NULL;
    }
#undef p_pes

}

/*****************************************************************************
 * input_GatherPES:
 *****************************************************************************
 * Gather a PES packet.
 *****************************************************************************/
void input_GatherPES( input_thread_t * p_input, data_packet_t * p_data,
                      es_descriptor_t * p_es,
                      boolean_t b_unit_start, boolean_t b_packet_lost )
{
#define p_pes (p_es->p_pes)

    //intf_DbgMsg("PES-demultiplexing %p (%p)", p_ts_packet, p_pes);

    /* If we lost data, insert a NULL data packet (philosophy : 0 is quite
     * often an escape sequence in decoders, so that should make them wait
     * for the next start code). */
    if( b_packet_lost )
    {
        input_NullPacket( p_input, p_es );
    }

    if( b_unit_start && p_pes != NULL )
    {
        /* If the data packet contains the begining of a new PES packet, and
         * if we were reassembling a PES packet, then the PES should be
         * complete now, so parse its header and give it to the decoders. */
        input_ParsePES( p_input, p_es );
    }

    if( !b_unit_start && p_pes == NULL )
    {
        /* Random access... */
        p_input->pf_delete_packet( p_input->p_method_data, p_data );
    }
    else
    {
        if( b_unit_start )
        {
            /* If we are at the beginning of a new PES packet, we must fetch
             * a new PES buffer to begin with the reassembly of this PES
             * packet. This is also here that we can synchronize with the
             * stream if we lost packets or if the decoder has just
             * started. */
            if( (p_pes = p_input->pf_new_pes( p_input->p_method_data ) ) == NULL )
            {
                intf_ErrMsg("Out of memory");
                p_input->b_error = 1;
                return;
            }
            p_pes->i_rate = p_input->stream.control.i_rate;
            p_pes->p_first = p_data;

            /* If the PES header fits in the first data packet, we can
             * already set p_gather->i_pes_real_size. */
            if( p_data->p_payload_end - p_data->p_payload_start
                    >= PES_HEADER_SIZE )
            {
                p_es->i_pes_real_size =
                                U16_AT(p_data->p_payload_start + 4) + 6;
            }
            else
            {
                p_es->i_pes_real_size = 0;
            }
        }
        else
        {
            /* Update the relations between the data packets */
            p_es->p_last->p_next = p_data;
        }

        p_es->p_last = p_data;

        /* Size of the payload carried in the data packet */
        p_pes->i_pes_size += (p_data->p_payload_end
                                 - p_data->p_payload_start);
    
        /* We can check if the packet is finished */
        if( p_pes->i_pes_size == p_es->i_pes_real_size )
        {
            /* The packet is finished, parse it */
            input_ParsePES( p_input, p_es );
        }
    }
#undef p_pes
}


/*
 * PS Demultiplexing
 */

/*****************************************************************************
 * GetID: Get the ID of a stream
 *****************************************************************************/
static u16 GetID( data_packet_t * p_data )
{
    u16         i_id;

    i_id = p_data->p_buffer[3];                                 /* stream_id */
    if( i_id == 0xBD )
    {
        /* stream_private_id */
        i_id |= p_data->p_buffer[ 9 + p_data->p_buffer[8] ] << 8;
    }
    return( i_id );
}

/*****************************************************************************
 * DecodePSM: Decode the Program Stream Map information
 *****************************************************************************/
static void DecodePSM( input_thread_t * p_input, data_packet_t * p_data )
{
    stream_ps_data_t *  p_demux =
                 (stream_ps_data_t *)p_input->stream.p_demux_data;
    byte_t *            p_byte;
    byte_t *            p_end;
    int                 i;
    int                 i_new_es_number = 0;

    intf_Msg("input info: Your stream contains Program Stream Map information");
    intf_Msg("input info: Please send a mail to <massiot@via.ecp.fr>");

    if( p_data->p_payload_start + 10 > p_data->p_payload_end )
    {
        intf_ErrMsg( "PSM too short : packet corrupt" );
        return;
    }

    if( p_demux->b_has_PSM
        && p_demux->i_PSM_version == (p_data->p_buffer[6] & 0x1F) )
    {
        /* Already got that one. */
        return;
    }

    intf_DbgMsg( "Building PSM" );
    p_demux->b_has_PSM = 1;
    p_demux->i_PSM_version = p_data->p_buffer[6] & 0x1F;

    /* Go to elementary_stream_map_length, jumping over
     * program_stream_info. */
    p_byte = p_data->p_payload_start + 10
              + U16_AT(&p_data->p_payload_start[8]);
    if( p_byte > p_data->p_payload_end )
    {
        intf_ErrMsg( "PSM too short : packet corrupt" );
        return;
    }
    /* This is the full size of the elementary_stream_map.
     * 2 == elementary_stream_map_length
     * Please note that CRC_32 is not included in the length. */
    p_end = p_byte + 2 + U16_AT(p_byte);
    p_byte += 2;
    if( p_end > p_data->p_payload_end )
    {
        intf_ErrMsg( "PSM too short : packet corrupt" );
        return;
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );

    /* 4 == minimum useful size of a section */
    while( p_byte + 4 <= p_end )
    {
        es_descriptor_t *   p_es = NULL;
        u8                  i_stream_id = p_byte[1];
        /* FIXME: there will be a problem with private streams... (same
         * stream_id) */

        /* Look for the ES in the ES table */
        for( i = i_new_es_number;
             i < p_input->stream.pp_programs[0]->i_es_number;
             i++ )
        {
            if( p_input->stream.pp_programs[0]->pp_es[i]->i_stream_id
                    == i_stream_id )
            {
                p_es = p_input->stream.pp_programs[0]->pp_es[i];
                if( p_es->i_type != p_byte[0] )
                {
                    input_DelES( p_input, p_es );
                    p_es = NULL;
                }
                else
                {
                    /* Move the ES to the beginning. */
                    p_input->stream.pp_programs[0]->pp_es[i]
                        = p_input->stream.pp_programs[0]->pp_es[ i_new_es_number ];
                    p_input->stream.pp_programs[0]->pp_es[ i_new_es_number ]
                        = p_es;
                    i_new_es_number++;
                }
                break;
            }
        }

        /* The goal is to have all the ES we have just read in the
         * beginning of the pp_es table, and all the others at the end,
         * so that we can close them more easily at the end. */
        if( p_es == NULL )
        {
            p_es = input_AddES( p_input, p_input->stream.pp_programs[0],
                                i_stream_id, 0 );
            p_es->i_type = p_byte[0];
            p_es->b_audio = ( p_es->i_type == MPEG1_AUDIO_ES
                              || p_es->i_type == MPEG2_AUDIO_ES
                              || p_es->i_type == AC3_AUDIO_ES
                              || p_es->i_type == LPCM_AUDIO_ES
                            );

            /* input_AddES has inserted the new element at the end. */
            p_input->stream.pp_programs[0]->pp_es[
                p_input->stream.pp_programs[0]->i_es_number ]
                = p_input->stream.pp_programs[0]->pp_es[ i_new_es_number ];
            p_input->stream.pp_programs[0]->pp_es[ i_new_es_number ] = p_es;
            i_new_es_number++;
        }
        p_byte += 4 + U16_AT(&p_byte[2]);
    }

    /* Un-select the streams that are no longer parts of the program. */
    for( i = i_new_es_number;
         i < p_input->stream.pp_programs[0]->i_es_number;
         i++ )
    {
        /* We remove pp_es[i_new_es_member] and not pp_es[i] because the
         * list will be emptied starting from the end */
        input_DelES( p_input,
                     p_input->stream.pp_programs[0]->pp_es[i_new_es_number] );
    }

#ifdef STATS
    intf_Msg( "input info: The stream map after the PSM is now :" );
    input_DumpStream( p_input );
#endif

    vlc_mutex_unlock( &p_input->stream.stream_lock );
}

/*****************************************************************************
 * input_ParsePS: read the PS header
 *****************************************************************************/
es_descriptor_t * input_ParsePS( input_thread_t * p_input,
                                 data_packet_t * p_data )
{
    u32                 i_code;
    es_descriptor_t *   p_es = NULL;

    i_code = U32_AT( p_data->p_buffer );
    if( i_code > 0x1BC ) /* ES start code */
    {
        u16                 i_id;
        int                 i_dummy;

        /* This is a PES packet. Find out if we want it or not. */
        i_id = GetID( p_data );

        vlc_mutex_lock( &p_input->stream.stream_lock );
        if( p_input->stream.pp_programs[0]->b_is_ok )
        {
            /* Look only at the selected ES. */
            for( i_dummy = 0; i_dummy < p_input->stream.i_selected_es_number;
                 i_dummy++ )
            {
                if( p_input->stream.pp_selected_es[i_dummy] != NULL
                    && p_input->stream.pp_selected_es[i_dummy]->i_id == i_id )
                {
                    p_es = p_input->stream.pp_selected_es[i_dummy];
                    break;
                }
            }
        }
        else
        {
            stream_ps_data_t * p_demux =
              (stream_ps_data_t *)p_input->stream.pp_programs[0]->p_demux_data;

            /* Search all ES ; if not found -> AddES */
            p_es = input_FindES( p_input, i_id );

            if( p_es == NULL && !p_demux->b_has_PSM )
            {
                p_es = input_AddES( p_input, p_input->stream.pp_programs[0],
                                    i_id, 0 );
                if( p_es != NULL )
                {
                    p_es->i_stream_id = p_data->p_buffer[3];

                    /* Set stream type and auto-spawn. */
                    if( (i_id & 0xF0) == 0xE0 )
                    {
                        /* MPEG video */
                        p_es->i_type = MPEG2_VIDEO_ES;
#ifdef AUTO_SPAWN
                        if( !p_input->stream.b_seekable )
                            input_SelectES( p_input, p_es );
#endif
                    }
                    else if( (i_id & 0xE0) == 0xC0 )
                    {
                        /* MPEG audio */
                        p_es->i_type = MPEG2_AUDIO_ES;
                        p_es->b_audio = 1;
#ifdef AUTO_SPAWN
                        if( !p_input->stream.b_seekable )
                        if( main_GetIntVariable( INPUT_CHANNEL_VAR, 0 )
                                == (p_es->i_id & 0x1F) )
                        switch( main_GetIntVariable( INPUT_AUDIO_VAR, 0 ) )
                        {
                        case 0:
                            main_PutIntVariable( INPUT_CHANNEL_VAR,
                                                 REQUESTED_MPEG );
                        case REQUESTED_MPEG:
                            input_SelectES( p_input, p_es );
                        }
#endif
                    }
                    else if( (i_id & 0xF0FF) == 0x80BD )
                    {
                        /* AC3 audio (0x80->0x8F) */
                        p_es->i_type = AC3_AUDIO_ES;
                        p_es->b_audio = 1;
#ifdef AUTO_SPAWN
                        if( !p_input->stream.b_seekable )
                        if( main_GetIntVariable( INPUT_CHANNEL_VAR, 0 )
                                == ((p_es->i_id & 0xF00) >> 8) )
                        switch( main_GetIntVariable( INPUT_AUDIO_VAR, 0 ) )
                        {
                        case 0:
                            main_PutIntVariable( INPUT_CHANNEL_VAR,
                                                 REQUESTED_AC3 );
                        case REQUESTED_AC3:
                            input_SelectES( p_input, p_es );
                        }
#endif
                    }
                    else if( (i_id & 0xE0FF) == 0x20BD )
                    {
                        /* Subtitles video (0x20->0x3F) */
                        p_es->i_type = DVD_SPU_ES;
#ifdef AUTO_SPAWN
                        if( main_GetIntVariable( INPUT_SUBTITLE_VAR, -1 )
                                == ((p_es->i_id & 0x1F00) >> 8) )
                        {
                            if( !p_input->stream.b_seekable )
                                input_SelectES( p_input, p_es );
                        }
#endif
                    }
                    else if( (i_id & 0xF0FF) == 0xA0BD )
                    {
                        /* LPCM audio (0xA0->0xAF) */
                        p_es->i_type = LPCM_AUDIO_ES;
                        p_es->b_audio = 1;
                        /* FIXME : write the decoder */
                    }
                    else
                    {
                        p_es->i_type = UNKNOWN_ES;
                    }
                }
            }
        } /* stream.b_is_ok */
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    } /* i_code > 0xBC */

    return( p_es );
}

/*****************************************************************************
 * input_DemuxPS: first step of demultiplexing: the PS header
 *****************************************************************************/
void input_DemuxPS( input_thread_t * p_input, data_packet_t * p_data )
{
    u32                 i_code;
    boolean_t           b_trash = 0;
    es_descriptor_t *   p_es = NULL;

    i_code = U32_AT( p_data->p_buffer );
    if( i_code <= 0x1BC )
    {
        switch( i_code )
        {
        case 0x1BA: /* PACK_START_CODE */
            {
                /* Read the SCR. */
                mtime_t         scr_time;

                if( (p_data->p_buffer[4] & 0xC0) == 0x40 )
                {
                    /* MPEG-2 */
                    scr_time =
                         ((mtime_t)(p_data->p_buffer[4] & 0x38) << 27) |
                         ((mtime_t)(U32_AT(p_data->p_buffer + 4) & 0x03FFF800)
                                        << 4) |
                         ((mtime_t)(U32_AT(p_data->p_buffer + 6) & 0x03FFF800)
                                        >> 11);
                }
                else
                {
                    /* MPEG-1 SCR is like PTS. */
                    scr_time =
                         ((mtime_t)(p_data->p_buffer[4] & 0x0E) << 29) |
                         (((mtime_t)U16_AT(p_data->p_buffer + 5) << 14)
                           - (1 << 14)) |
                         ((mtime_t)U16_AT(p_data->p_buffer + 7) >> 1);
                }
                /* Call the pace control. */
                //intf_Msg("+%lld", scr_time);
                input_ClockManageRef( p_input, p_input->stream.pp_programs[0],
                                      scr_time );
                b_trash = 1;
            }
            break;

        case 0x1BB: /* SYSTEM_START_CODE */
            b_trash = 1;                              /* Nothing interesting */
            break;

        case 0x1BC: /* PROGRAM_STREAM_MAP_CODE */
            DecodePSM( p_input, p_data );
            b_trash = 1;
            break;
    
        case 0x1B9: /* PROGRAM_END_CODE */
            b_trash = 1;
            break;
   
        default:
            /* This should not happen */
            b_trash = 1;
            intf_WarnMsg( 1, "Unwanted packet received with start code %x",
                          i_code );
        }
    }
    else
    {
        p_es = input_ParsePS( p_input, p_data );

        vlc_mutex_lock( &p_input->stream.control.control_lock );
        if( p_es != NULL && p_es->p_decoder_fifo != NULL
             && (!p_es->b_audio || !p_input->stream.control.b_mute) )
        {
            vlc_mutex_unlock( &p_input->stream.control.control_lock );
#ifdef STATS
            p_es->c_packets++;
#endif
            input_GatherPES( p_input, p_data, p_es, 1, 0 );
        }
        else
        {
            vlc_mutex_unlock( &p_input->stream.control.control_lock );
            b_trash = 1;
        }
    }

    /* Trash the packet if it has no payload or if it isn't selected */
    if( b_trash )
    {
        p_input->pf_delete_packet( p_input->p_method_data, p_data );
#ifdef STATS
        p_input->c_packets_trashed++;
#endif
    }
}


/*
 * TS Demultiplexing
 */

/*****************************************************************************
 * input_DemuxTS: first step of demultiplexing: the TS header
 *****************************************************************************/
void input_DemuxTS( input_thread_t * p_input, data_packet_t * p_data )
{
    int                 i_pid, i_dummy;
    boolean_t           b_adaptation;         /* Adaptation field is present */
    boolean_t           b_payload;                 /* Packet carries payload */
    boolean_t           b_unit_start;  /* A PSI or a PES start in the packet */
    boolean_t           b_trash = 0;             /* Is the packet unuseful ? */
    boolean_t           b_lost = 0;             /* Was there a packet loss ? */
    es_descriptor_t *   p_es = NULL;
    es_ts_data_t *      p_es_demux = NULL;
    pgrm_ts_data_t *    p_pgrm_demux = NULL;

#define p (p_data->p_buffer)

    //intf_DbgMsg("input debug: TS-demultiplexing packet %p, pid %d",
    //            p_ts_packet, U16_AT(&p[1]) & 0x1fff);

    /* Extract flags values from TS common header. */
    i_pid = U16_AT(&p[1]) & 0x1fff;
    b_unit_start = (p[1] & 0x40);
    b_adaptation = (p[3] & 0x20);
    b_payload = (p[3] & 0x10);

    /* Find out the elementary stream. */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_es = input_FindES( p_input, i_pid );

    vlc_mutex_lock( &p_input->stream.control.control_lock );
    if( p_es == NULL || p_es->p_decoder_fifo == NULL
         || (p_es->b_audio && p_input->stream.control.b_mute) )
    {
        /* Not selected. Just read the adaptation field for a PCR. */
        b_trash = 1;
    }
    vlc_mutex_unlock( &p_input->stream.control.control_lock );
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    if( (p_es->p_decoder_fifo != NULL) || (p_pgrm_demux->i_pcr_pid == i_pid) )
    {
#ifdef STATS
        p_es->c_packets++;
#endif

        /* Extract adaptation field information if any */
        if( !b_adaptation )
        {
            /* We don't have any adaptation_field, so payload starts
             * immediately after the 4 byte TS header */
            p_data->p_payload_start += 4;
        }
        else
        {
            /* p[4] is adaptation_field_length minus one */
            p_data->p_payload_start += 5 + p[4];
    
            /* The adaptation field can be limited to the
             * adaptation_field_length byte, so that there is nothing to do:
             * skip this possibility */
            if( p[4] )
            {
                /* If the packet has both adaptation_field and payload,
                 * adaptation_field cannot be more than 182 bytes long; if
                 * there is only an adaptation_field, it must fill the next
                 * 183 bytes. */
                if( b_payload ? (p[4] > 182) : (p[4] != 183) )
                {
                    intf_WarnMsg( 2,
                        "invalid TS adaptation field (%p)",
                        p_data );
                    p_data->b_discard_payload = 1;
#ifdef STATS
                    p_es->c_invalid_packets++;
#endif
                }
    
                /* Now we are sure that the byte containing flags is present:
                 * read it */
                else
                {
                    /* discontinuity_indicator */
                    if( p[5] & 0x80 )
                    {
                        intf_WarnMsg( 2,
                            "discontinuity_indicator"
                            " encountered by TS demux (position read: %d,"
                            " saved: %d)",
                            p[5] & 0x80, p_es_demux->i_continuity_counter );
    
                        /* If the PID carries the PCR, there will be a system
                         * time-based discontinuity. We let the PCR decoder
                         * handle that. */
                        p_es->p_pgrm->i_synchro_state = SYNCHRO_REINIT;
    
                        /* There also may be a continuity_counter
                         * discontinuity: resynchronise our counter with
                         * the one of the stream. */
                        p_es_demux->i_continuity_counter = (p[3] & 0x0f) - 1;
                    }
    
                    /* If this is a PCR_PID, and this TS packet contains a
                     * PCR, we pass it along to the PCR decoder. */
                    if( (p_pgrm_demux->i_pcr_pid == i_pid) && (p[5] & 0x10) )
                    {
                        /* There should be a PCR field in the packet, check
                         * if the adaptation field is long enough to carry
                         * it. */
                        if( p[4] >= 7 )
                        {
                            /* Read the PCR. */
                            mtime_t     pcr_time;
                            pcr_time =
                                    ( (mtime_t)U32_AT((u32*)&p[6]) << 1 )
                                      | ( p[10] >> 7 );
                            /* Call the pace control. */
                            input_ClockManageRef( p_input, p_es->p_pgrm,
                                                  pcr_time );
                        }
                    } /* PCR ? */
                } /* valid TS adaptation field ? */
            } /* length > 0 */
        } /* has adaptation field */
    
        /* Check the continuity of the stream. */
        i_dummy = ((p[3] & 0x0f) - p_es_demux->i_continuity_counter) & 0x0f;
        if( i_dummy == 1 )
        {
            /* Everything is ok, just increase our counter */
            p_es_demux->i_continuity_counter++;
        }
        else
        {
            if( !b_payload && i_dummy == 0 )
            {
                /* This is a packet without payload, this is allowed by the
                 * draft. As there is nothing interesting in this packet
                 * (except PCR that have already been handled), we can trash
                 * the packet. */
                intf_WarnMsg( 1,
                              "Packet without payload received by TS demux" );
                b_trash = 1;
            }
            else if( i_dummy <= 0 )
            {
                /* FIXME: this can never happen, can it ? --Meuuh */
                /* Duplicate packet: mark it as being to be trashed. */
                intf_WarnMsg( 1, "Duplicate packet received by TS demux" );
                b_trash = 1;
            }
            else if( p_es_demux->i_continuity_counter == 0xFF )
            {
                /* This means that the packet is the first one we receive for
                 * this ES since the continuity counter ranges between 0 and
                 * 0x0F excepts when it has been initialized by the input:
                 * init the counter to the correct value. */
                intf_DbgMsg( "First packet for PID %d received by TS demux",
                             p_es->i_id );
                p_es_demux->i_continuity_counter = (p[3] & 0x0f);
            }
            else
            {
                /* This can indicate that we missed a packet or that the
                 * continuity_counter wrapped and we received a dup packet:
                 * as we don't know, do as if we missed a packet to be sure
                 * to recover from this situation */
                intf_WarnMsg( 2,
                           "Packet lost by TS demux: current %d, packet %d",
                           p_es_demux->i_continuity_counter & 0x0f,
                           p[3] & 0x0f );
                b_lost = 1;
                p_es_demux->i_continuity_counter = p[3] & 0x0f;
            } /* not continuous */
        } /* continuity */
    } /* if selected or PCR */

    /* Trash the packet if it has no payload or if it isn't selected */
    if( b_trash )
    {
        p_input->pf_delete_packet( p_input, p_data );
#ifdef STATS
        p_input->c_packets_trashed++;
#endif
    }
    else
    {
        if( p_es_demux->b_psi )
        {
            /* The payload contains PSI tables */
#if 0
            /* FIXME ! write the PSI decoder :p */
            input_DemuxPSI( p_input, p_data, p_es,
                            b_unit_start, b_lost );
#endif
        }
        else
        {
            /* The payload carries a PES stream */
            if( b_unit_start )
            input_GatherPES( p_input, p_data, p_es, b_unit_start, b_lost );
        }
    }

#undef p
}
