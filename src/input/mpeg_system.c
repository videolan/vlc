/*****************************************************************************
 * mpeg_system.c: TS, PS and PES management
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: mpeg_system.c,v 1.47 2001/04/06 09:15:47 sam Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Michel Lespinasse <walken@via.ecp.fr>
 *          Benoît Steiner <benny@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
 *          Henri Fallon <henri@via.ecp.fr>
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
#include <string.h>                                    /* memcpy(), memset() */

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

static void input_DecodePAT( input_thread_t *, es_descriptor_t *);
static void input_DecodePMT( input_thread_t *, es_descriptor_t *);

/*
 * PES Packet management
 */

/*****************************************************************************
 * MoveChunk
 *****************************************************************************
 * Small utility function used to parse discontinuous headers safely. Copies
 * i_buf_len bytes of data to a buffer and returns the size copied.
 * It also solves some alignment problems on non-IA-32, non-PPC processors.
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
                      ((mtime_t)(p_full_header[3]) << 22) |
                      ((mtime_t)(p_full_header[4] & 0xFE) << 14) |
                      ((mtime_t)p_full_header[5] << 7) |
                      ((mtime_t)p_full_header[6] >> 1) ) );

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

                while( *p_byte == 0xFF && i_pes_header_size < 23 )
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
                if( i_pes_header_size == 23 )
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
                         (((mtime_t)U32_AT(p_ts) & 0xFFFE00) << 6) |
                         ((mtime_t)p_ts[3] << 7) |
                         ((mtime_t)p_ts[4] >> 1) ) );

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
                              (((mtime_t)U32_AT(p_ts) & 0xFFFE00) << 6) |
                              ((mtime_t)p_ts[3] << 7) |
                              ((mtime_t)p_ts[4] >> 1) ) );
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

    i_id = p_data->p_payload_start[3];                                 /* stream_id */
    if( i_id == 0xBD )
    {
        /* FIXME : this is not valid if the header is split in multiple
         * packets */
        /* stream_private_id */
        i_id |= p_data->p_payload_start[ 9 + p_data->p_payload_start[8] ] << 8;
    }
    return( i_id );
}

/*****************************************************************************
 * DecodePSM: Decode the Program Stream Map information
 *****************************************************************************
 * FIXME : loads are not aligned in this function
 *****************************************************************************/
static void DecodePSM( input_thread_t * p_input, data_packet_t * p_data )
{
    stream_ps_data_t *  p_demux =
                 (stream_ps_data_t *)p_input->stream.p_demux_data;
    byte_t *            p_byte;
    byte_t *            p_end;
    int                 i;
    int                 i_new_es_number = 0;

    if( p_data->p_payload_start + 10 > p_data->p_payload_end )
    {
        intf_ErrMsg( "PSM too short : packet corrupt" );
        return;
    }

    if( p_demux->b_has_PSM
        && p_demux->i_PSM_version == (p_data->p_payload_start[6] & 0x1F) )
    {
        /* Already got that one. */
        return;
    }

    intf_DbgMsg( "Building PSM" );
    p_demux->b_has_PSM = 1;
    p_demux->i_PSM_version = p_data->p_payload_start[6] & 0x1F;

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

    i_code = p_data->p_payload_start[3];

    if( i_code > 0xBC ) /* ES start code */
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
                    p_es->i_stream_id = p_data->p_payload_start[3];

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
                        p_es->b_spu = 1;
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

    i_code = U32_AT( p_data->p_payload_start );
    if( i_code <= 0x1BC )
    {
        switch( i_code )
        {
        case 0x1BA: /* PACK_START_CODE */
            {
                /* Read the SCR. */
                mtime_t         scr_time;
                u32             i_mux_rate;

                if( (p_data->p_payload_start[4] & 0xC0) == 0x40 )
                {
                    /* MPEG-2 */
                    byte_t      p_header[14];
                    byte_t *    p_byte;
                    p_byte = p_data->p_payload_start;

                    if( MoveChunk( p_header, &p_data, &p_byte, 14 ) != 14 )
                    {
                        intf_WarnMsg( 3, "Packet too short to have a header" );
                        b_trash = 1;
                        break;
                    }
                    scr_time =
                         ((mtime_t)(p_header[4] & 0x38) << 27) |
                         ((mtime_t)(U32_AT(p_header + 4) & 0x03FFF800)
                                        << 4) |
                         ((( ((mtime_t)U16_AT(p_header + 6) << 16)
                            | (mtime_t)U16_AT(p_header + 8) ) & 0x03FFF800)
                                        >> 11);

                    /* mux_rate */
                    i_mux_rate = ((u32)U16_AT(p_header + 10) << 6)
                                   | (p_header[12] >> 2);
                }
                else
                {
                    /* MPEG-1 SCR is like PTS. */
                    byte_t      p_header[12];
                    byte_t *    p_byte;
                    p_byte = p_data->p_payload_start;

                    if( MoveChunk( p_header, &p_data, &p_byte, 12 ) != 12 )
                    {
                        intf_WarnMsg( 3, "Packet too short to have a header" );
                        b_trash = 1;
                        break;
                    }
                    scr_time =
                         ((mtime_t)(p_header[4] & 0x0E) << 29) |
                         (((mtime_t)U32_AT(p_header + 4) & 0xFFFE00) << 6) |
                         ((mtime_t)p_header[7] << 7) |
                         ((mtime_t)p_header[8] >> 1);

                    /* mux_rate */
                    i_mux_rate = (U32_AT(p_header + 8) & 0x7FFFFE) >> 1;
                }
                /* Call the pace control. */
                input_ClockManageRef( p_input, p_input->stream.pp_programs[0],
                                      scr_time );

                if( i_mux_rate != p_input->stream.i_mux_rate
                     && p_input->stream.i_mux_rate )
                {
                    intf_WarnMsg(2,
                                 "Mux_rate changed - expect cosmetic errors");
                }
                p_input->stream.i_mux_rate = i_mux_rate;

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
            intf_WarnMsg( 1, "Unwanted packet received with start code 0x%.8x",
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
    u16                 i_pid;
    int                 i_dummy;
    boolean_t           b_adaptation;         /* Adaptation field is present */
    boolean_t           b_payload;                 /* Packet carries payload */
    boolean_t           b_unit_start;  /* A PSI or a PES start in the packet */
    boolean_t           b_trash = 0;             /* Is the packet unuseful ? */
    boolean_t           b_lost = 0;             /* Was there a packet loss ? */
    boolean_t           b_psi = 0;                        /* Is this a PSI ? */
    es_descriptor_t *   p_es = NULL;
    es_ts_data_t *      p_es_demux = NULL;
    pgrm_ts_data_t *    p_pgrm_demux = NULL;

    #define p (p_data->p_buffer)
    /* Extract flags values from TS common header. */
    i_pid = U16_AT(&p[1]) & 0x1fff;
    b_unit_start = (p[1] & 0x40);
    b_adaptation = (p[3] & 0x20);
    b_payload = (p[3] & 0x10);

    /* Find out the elementary stream. */
    vlc_mutex_lock( &p_input->stream.stream_lock );
        
    p_es= input_FindES( p_input, i_pid );
    
    if( (p_es != NULL) && (p_es->p_demux_data != NULL) )
    {
        p_es_demux = (es_ts_data_t *)p_es->p_demux_data;
        
        if( p_es_demux->b_psi )
            b_psi = 1;
        else
            p_pgrm_demux = (pgrm_ts_data_t *)p_es->p_pgrm->p_demux_data; 
    }

    vlc_mutex_lock( &p_input->stream.control.control_lock );
    if( ( p_es == NULL ) || (p_es->b_audio && p_input->stream.control.b_mute) )
    {
        /* Not selected. Just read the adaptation field for a PCR. */
        b_trash = 1;
    }
    else if( p_es->p_decoder_fifo == NULL  && !b_psi )
      b_trash =1; 

    vlc_mutex_unlock( &p_input->stream.control.control_lock );
    vlc_mutex_unlock( &p_input->stream.stream_lock );


    /* Don't change the order of the tests : if b_psi then p_pgrm_demux 
     * may still be null. Who said it was ugly ? */
    if( ( p_es != NULL ) && 
        ((p_es->p_decoder_fifo != NULL) || b_psi 
                                   || (p_pgrm_demux->i_pcr_pid == i_pid) ) )
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
            (p_es_demux->i_continuity_counter)++;
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
        p_input->pf_delete_packet( p_input->p_method_data, p_data );
#ifdef STATS
        p_input->c_packets_trashed++;
#endif
    }
    else
    {
        if( b_psi )
        {
            /* The payload contains PSI tables */
            input_DemuxPSI( p_input, p_data, p_es,
                            b_unit_start, b_lost );

        }
        else
        {
            /* The payload carries a PES stream */
            input_GatherPES( p_input, p_data, p_es, b_unit_start, b_lost ); 
        }

    }

#undef p

}

/*
 * PSI demultiplexing and decoding
 */

/*****************************************************************************
 * DemuxPSI : makes up complete PSI data
 *****************************************************************************/
void input_DemuxPSI( input_thread_t * p_input, data_packet_t * p_data, 
        es_descriptor_t * p_es, boolean_t b_unit_start, boolean_t b_lost )
{
    es_ts_data_t  * p_demux_data;
    
    p_demux_data = (es_ts_data_t *)p_es->p_demux_data;

#define p_psi (p_demux_data->p_psi_section)
#define p (p_data->p_payload_start)

    if( b_unit_start )
    {
        /* unit_start set to 1 -> presence of a pointer field
         * (see ISO/IEC 13818 (2.4.4.2) which should be set to 0x00 */
        if( (u8)p[0] != 0x00 )
        {
        /*    intf_WarnMsg( 2, */
            intf_ErrMsg( "Non zero pointer field found. Trying to continue" );
            p+=(u8)p[0];
        }
        else
            p++;

        /* This is the begining of a new section */

        if( ((u8)(p[1]) & 0xc0) != 0x80 ) 
        {
            intf_ErrMsg( "Invalid PSI packet" );
            p_psi->b_trash = 1;
        }
        else 
        {
            p_psi->i_section_length = U16_AT(p+1) & 0x0fff;
            p_psi->b_section_complete = 0;
            p_psi->i_read_in_section = 0;
            p_psi->i_section_number = (u8)p[6];

            if( p_psi->b_is_complete || p_psi->i_section_number == 0 )
            {
                /* This is a new PSI packet */
                p_psi->b_is_complete = 0;
                p_psi->b_trash = 0;
                p_psi->i_version_number = ( p[5] >> 1 ) & 0x1f;
                p_psi->i_last_section_number = (u8)p[7];

                /* We'll write at the begining of the buffer */
                p_psi->p_current = p_psi->buffer;
            }
            else
            {
                if( p_psi->b_section_complete )
                {
                    /* New Section of an already started PSI */
                    p_psi->b_section_complete = 0;
                    
                    if( p_psi->i_version_number != (( p[5] >> 1 ) & 0x1f) )
                    {
                        intf_WarnMsg( 2,"PSI version differs inside same PAT" );
                        p_psi->b_trash = 1;
                    }
                    if( p_psi->i_section_number + 1 != (u8)p[6] )
                    {
                        intf_WarnMsg( 2, 
                                "PSI Section discontinuity. Packet lost ?");
                        p_psi->b_trash = 1;
                    }
                    else
                        p_psi->i_section_number++;
                }
                else
                {
                    intf_WarnMsg( 2, "Received unexpected new PSI section" );
                    p_psi->b_trash = 1;
                }
            }
        }
    } /* b_unit_start */
    
    if( !p_psi->b_trash )
    {
        /* read */
        if( (p_data->p_payload_end - p) >=
            ( p_psi->i_section_length - p_psi->i_read_in_section ) )
        {
            /* The end of the section is in this TS packet */
            memcpy( p_psi->p_current, p, 
            (p_psi->i_section_length - p_psi->i_read_in_section) );
    
            p_psi->b_section_complete = 1;
            p_psi->p_current += 
                (p_psi->i_section_length - p_psi->i_read_in_section);
                        
            if( p_psi->i_section_number == p_psi->i_last_section_number )
            {
                /* This was the last section of PSI */
                p_psi->b_is_complete = 1;
            }
        }
        else
        {
            memcpy( p_psi->buffer, p, p_data->p_payload_end - p );
            p_psi->i_read_in_section+= p_data->p_payload_end - p;

            p_psi->p_current += p_data->p_payload_end - p;
        }
    }

    if ( p_psi->b_is_complete )
    {
        switch( p_demux_data->i_psi_type)
        {
            case PSI_IS_PAT:
                input_DecodePAT( p_input, p_es );
                break;
            case PSI_IS_PMT:
                input_DecodePMT( p_input, p_es );
                break;
            default:
                intf_ErrMsg("Received unknown PSI in demuxPSI");
        }
    }
#undef p_psi    
#undef p
    
    return ;
}

/*****************************************************************************
 * DecodePAT : Decodes Programm association table and deal with it
 *****************************************************************************/
static void input_DecodePAT( input_thread_t * p_input, es_descriptor_t * p_es )
{
    
    stream_ts_data_t  * p_stream_data;
    es_ts_data_t      * p_demux_data;

    p_demux_data = (es_ts_data_t *)p_es->p_demux_data;
    p_stream_data = (stream_ts_data_t *)p_input->stream.p_demux_data;
    
#define p_psi (p_demux_data->p_psi_section)

    if( p_stream_data->i_pat_version != p_psi->i_version_number )
    {
        /* PAT has changed. We are going to delete all programms and 
         * create new ones. We chose not to only change what was needed
         * as a PAT change may mean the stream is radically changing and
         * this is a secure method to avoid krashed */
        pgrm_descriptor_t * p_pgrm;
        es_descriptor_t   * p_current_es;
        es_ts_data_t      * p_es_demux;
        pgrm_ts_data_t    * p_pgrm_demux;
        byte_t            * p_current_data;           
        
        int                 i_section_length,i_program_id,i_pmt_pid;
        int                 i_loop, i_current_section;
        
        p_current_data = p_psi->buffer;


        for( i_loop = 0; i_loop < p_input->stream.i_pgrm_number; i_loop++ )
        {
            input_DelProgram( p_input, p_input->stream.pp_programs[i_loop] );
        }
        
        do
        {
            i_section_length = U16_AT(p_current_data+1) & 0x0fff;
            i_current_section = (u8)p_current_data[6];
    
            for( i_loop = 0; i_loop < (i_section_length-9)/4 ; i_loop++ )
            {
                i_program_id = U16_AT(p_current_data + i_loop*4 + 8);
                i_pmt_pid = U16_AT( p_current_data + i_loop*4 + 10) & 0x1fff;
    
                /* If program = 0, we're having info about NIT not PMT */
                if( i_program_id )
                {
                    /* Add this program */
                    p_pgrm = input_AddProgram( p_input, i_program_id, 
                                               sizeof( pgrm_ts_data_t ) );
                   
                    /* whatis the PID of the PMT of this program */
                    p_pgrm_demux = (pgrm_ts_data_t *)p_pgrm->p_demux_data;
                    p_pgrm_demux->i_pmt_version = PMT_UNINITIALIZED;
    
                    /* Add the PMT ES to this program */
                    p_current_es = input_AddES( p_input, p_pgrm,(u16)i_pmt_pid,
                                        sizeof( es_ts_data_t) );
                    p_es_demux = (es_ts_data_t *)p_current_es->p_demux_data;
                    p_es_demux->b_psi = 1;
                    p_es_demux->i_psi_type = PSI_IS_PMT;
                    
                    p_es_demux->p_psi_section = 
                                            malloc( sizeof( psi_section_t ) );
                    p_es_demux->p_psi_section->b_is_complete = 0;
                }
            }
            
            p_current_data+=3+i_section_length;
            
        } while( i_current_section < p_psi->i_last_section_number );
        
        /* Go to the beginning of the next section*/
        p_stream_data->i_pat_version = p_psi->i_version_number;

    }
#undef p_psi    

}

/*****************************************************************************
 * DecodePMT : decode a given Program Stream Map
 * ***************************************************************************
 * When the PMT changes, it may mean a deep change in the stream, and it is
 * careful to deletes the ES and add them again. If the PMT doesn't change,
 * there no need to do anything.
 *****************************************************************************/
static void input_DecodePMT( input_thread_t * p_input, es_descriptor_t * p_es )
{

    pgrm_ts_data_t            * p_pgrm_data;
    es_ts_data_t              * p_demux_data;

    p_demux_data = (es_ts_data_t *)p_es->p_demux_data;
    p_pgrm_data = (pgrm_ts_data_t *)p_es->p_pgrm->p_demux_data;
    
#define p_psi (p_demux_data->p_psi_section)

    if( p_psi->i_version_number != p_pgrm_data->i_pmt_version ) 
    {
        es_descriptor_t   * p_new_es;  
        es_ts_data_t      * p_es_demux;
        byte_t            * p_current_data, * p_current_section;
        int                 i_section_length,i_current_section;
        int                 i_prog_info_length, i_loop;
        int                 i_es_info_length, i_pid, i_stream_type;
        
        p_current_section = p_psi->buffer;
        p_current_data = p_psi->buffer;
        
        p_pgrm_data->i_pcr_pid = U16_AT(p_current_section + 8) & 0x1fff;
        
        /* Lock stream information */
        vlc_mutex_lock( &p_input->stream.stream_lock );

        /* Delete all ES in this program  except the PSI */
        for( i_loop=0; i_loop < p_es->p_pgrm->i_es_number; i_loop++ )
        {
            p_es_demux = (es_ts_data_t *)
                         p_es->p_pgrm->pp_es[i_loop]->p_demux_data;
            if ( ! p_es_demux->b_psi )
            input_DelES( p_input, p_es->p_pgrm->pp_es[i_loop] );
        }

        /* Then add what we received in this PMT */
        do
        {
            
            i_section_length = U16_AT(p_current_data+1) & 0x0fff;
            i_current_section = (u8)p_current_data[6];
            i_prog_info_length = U16_AT(p_current_data+10) & 0x0fff;

            /* For the moment we ignore program descriptors */
            p_current_data += 12+i_prog_info_length;
    
            /* The end of the section, before the CRC is at 
             * p_current_section + i_section_length -1 */
            while( p_current_data < p_current_section + i_section_length -1 )
            {
                i_stream_type = (int)p_current_data[0];
                i_pid = U16_AT( p_current_data + 1 ) & 0x1fff;
                i_es_info_length = U16_AT( p_current_data + 3 ) & 0x0fff;
                
                /* Add this ES to the program */
                p_new_es = input_AddES( p_input, p_es->p_pgrm, 
                                        (u16)i_pid, sizeof( es_ts_data_t ) );
                p_new_es->i_type = i_stream_type;
                
                /* We want to decode */
                input_SelectES( p_input, p_new_es );
                p_current_data += 5 + i_es_info_length;
            }

            /* Go to the beginning of the next section*/
            p_current_data += 3+i_section_length;
           
            p_current_section+=1;
            
        } while( i_current_section < p_psi->i_last_section_number );

        p_pgrm_data->i_pmt_version = p_psi->i_version_number;

    }
    
#undef p_psi

    /*  Remove lock */
    vlc_mutex_unlock( &p_input->stream.stream_lock );
}
