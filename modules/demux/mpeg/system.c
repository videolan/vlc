/*****************************************************************************
 * system.c: helper module for TS, PS and PES management
 *****************************************************************************
 * Copyright (C) 1998-2002 VideoLAN
 * $Id: system.c,v 1.2 2002/08/30 22:22:24 massiot Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Michel Lespinasse <walken@via.ecp.fr>
 *          Benoît Steiner <benny@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
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
#include <stdlib.h>
#include <string.h>                                    /* memcpy(), memset() */
#include <sys/types.h>                                              /* off_t */

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "system.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Activate ( vlc_object_t * );

static ssize_t           ReadPS  ( input_thread_t *, data_packet_t ** );
static es_descriptor_t * ParsePS ( input_thread_t *, data_packet_t * );
static void              DemuxPS ( input_thread_t *, data_packet_t * );

static ssize_t           ReadTS  ( input_thread_t *, data_packet_t ** );
static void              DemuxTS ( input_thread_t *, data_packet_t *,
                                   psi_callback_t );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("generic ISO 13818-1 MPEG demultiplexing") );
    set_capability( "mpeg-system", 100 );
    set_callbacks( Activate, NULL );
vlc_module_end();

/*****************************************************************************
 * Activate: initializes helper functions
 *****************************************************************************/
static int Activate ( vlc_object_t *p_this )
{
    static mpeg_demux_t mpeg_demux =
                    { NULL, ReadPS, ParsePS, DemuxPS, ReadTS, DemuxTS };

    memcpy( p_this->p_private, &mpeg_demux, sizeof( mpeg_demux ) );

    return VLC_SUCCESS;
}

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
static inline size_t MoveChunk( byte_t * p_dest, data_packet_t ** pp_data_src,
                                byte_t ** pp_src, size_t i_buf_len )
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
 * ParsePES
 *****************************************************************************
 * Parse a finished PES packet and analyze its header.
 *****************************************************************************/
#define PES_HEADER_SIZE     7
static void ParsePES( input_thread_t * p_input, es_descriptor_t * p_es )
{
    data_packet_t * p_data;
    byte_t *        p_byte;
    byte_t          p_header[PES_HEADER_SIZE];
    int             i_done;

#define p_pes (p_es->p_pes)

    /* Parse the header. The header has a variable length, but in order
     * to improve the algorithm, we will read the 14 bytes we may be
     * interested in */
    p_data = p_pes->p_first;
    p_byte = p_data->p_payload_start;
    i_done = 0;

    if( MoveChunk( p_header, &p_data, &p_byte, PES_HEADER_SIZE )
            != PES_HEADER_SIZE )
    {
        msg_Warn( p_input, "PES packet too short to have a header" );
        input_DeletePES( p_input->p_method_data, p_pes );
        p_pes = NULL;
        return;
    }

    /* Get the PES size if defined */
    p_es->i_pes_real_size = U16_AT(p_header + 4);
    if( p_es->i_pes_real_size )
    {
        p_es->i_pes_real_size += 6;
    }

    /* First read the 6 header bytes common to all PES packets:
     * use them to test the PES validity */
    if( (p_header[0] || p_header[1] || (p_header[2] != 1)) )
    {
        /* packet_start_code_prefix != 0x000001 */
        msg_Err( p_input, "data loss, PES packet does not start with 000001" );
        input_DeletePES( p_input->p_method_data, p_pes );
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
            msg_Warn( p_input, "packet corrupted, PES sizes do not match" );
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
                vlc_bool_t  b_has_pts, b_has_dts;
                byte_t      p_full_header[12];

                p_pes->b_data_alignment = p_header[6] & 0x04;

                i_max_len = MoveChunk( p_full_header, &p_data, &p_byte, 12 );
                if( i_max_len < 2 )
                {
                    msg_Warn( p_input, 
                              "PES packet too short to have a MPEG-2 header" );
                    input_DeletePES( p_input->p_method_data,
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
                        msg_Warn( p_input,
                             "PES packet too short to have a MPEG-2 header" );
                        input_DeletePES( p_input->p_method_data,
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
                            msg_Warn( p_input,
                              "PES packet too short to have a MPEG-2 header" );
                            input_DeletePES( p_input->p_method_data,
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
                vlc_bool_t      b_has_pts, b_has_dts;

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
                        msg_Warn( p_input,
                            "PES packet too short to have a MPEG-1 header" );
                        input_DeletePES( p_input->p_method_data, p_pes );
                        p_pes = NULL;
                        return;
                    }
                }
                if( i_pes_header_size == 23 )
                {
                    msg_Err( p_input, "too much MPEG-1 stuffing" );
                    input_DeletePES( p_input->p_method_data, p_pes );
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
                        msg_Warn( p_input,
                            "PES packet too short to have a MPEG-1 header" );
                        input_DeletePES( p_input->p_method_data, p_pes );
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
                        msg_Warn( p_input,
                            "PES packet too short to have a MPEG-1 header" );
                        input_DeletePES( p_input->p_method_data, p_pes );
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
                            msg_Warn( p_input,
                              "PES packet too short to have a MPEG-1 header" );
                            input_DeletePES( p_input->p_method_data, p_pes );
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

        /* Welcome to the kludge area ! --Meuuh */
        if( p_es->i_fourcc == VLC_FOURCC('a','5','2','b') )
        {
            /* With A/52 audio, we need to skip the first 4 bytes */
            i_pes_header_size += 4;
        }
        else if( p_es->i_fourcc == VLC_FOURCC('l','p','c','b')
                  || p_es->i_fourcc == VLC_FOURCC('s','p','u','b')
                  || p_es->i_fourcc == VLC_FOURCC('d','t','s','b')
                  || p_es->i_fourcc == VLC_FOURCC('s','d','d','b') )
        {
            /* With others, we need to skip the first byte */
            i_pes_header_size += 1;
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
                msg_Err( p_input, "PES header bigger than payload" );
                input_DeletePES( p_input->p_method_data, p_pes );
                p_pes = NULL;
                return;
            }
            i_payload_size = p_data->p_payload_end
                                 - p_data->p_payload_start;
        }
        /* This last packet is partly header, partly payload. */
        if( i_payload_size < i_pes_header_size )
        {
            msg_Err( p_input, "PES header bigger than payload" );
            input_DeletePES( p_input->p_method_data, p_pes );
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
            msg_Err( p_input, "no fifo to receive PES %p "
                              "(who wrote this damn code ?)", p_pes );
            input_DeletePES( p_input->p_method_data, p_pes );
        }
        p_pes = NULL;
    }
#undef p_pes

}

/*****************************************************************************
 * GatherPES:
 *****************************************************************************
 * Gather a PES packet.
 *****************************************************************************/
static void GatherPES( input_thread_t * p_input, data_packet_t * p_data,
                       es_descriptor_t * p_es,
                       vlc_bool_t b_unit_start, vlc_bool_t b_packet_lost )
{
#define p_pes (p_es->p_pes)

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
        ParsePES( p_input, p_es );
    }

    if( !b_unit_start && p_pes == NULL )
    {
        /* Random access... */
        input_DeletePacket( p_input->p_method_data, p_data );
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
            if( (p_pes = input_NewPES( p_input->p_method_data ) ) == NULL )
            {
                msg_Err( p_input, "out of memory" );
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
                p_es->i_pes_real_size = ((u16)p_data->p_payload_start[4] << 8)
                                         + p_data->p_payload_start[5] + 6;
            }
            else
            { 
                p_es->i_pes_real_size = 0;
            } 
        }
        else
        {
            /* Update the relations between the data packets */
            p_pes->p_last->p_next = p_data;
        }

        p_pes->p_last = p_data;
        p_pes->i_nb_data++;

        /* Size of the payload carried in the data packet */
        p_pes->i_pes_size += (p_data->p_payload_end
                                 - p_data->p_payload_start);
    
        /* We can check if the packet is finished */
        if( p_pes->i_pes_size == p_es->i_pes_real_size )
        {
            /* The packet is finished, parse it */
            ParsePES( p_input, p_es );
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

    i_id = p_data->p_demux_start[3];                            /* stream_id */
    if( i_id == 0xBD )
    {
        /* FIXME : this is not valid if the header is split in multiple
         * packets */
        /* stream_private_id */
        i_id |= p_data->p_demux_start[ 9 + p_data->p_demux_start[8] ] << 8;
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

    if( p_data->p_demux_start + 10 > p_data->p_payload_end )
    {
        msg_Err( p_input, "PSM too short, packet corrupt" );
        return;
    }

    if( p_demux->b_has_PSM
        && p_demux->i_PSM_version == (p_data->p_demux_start[6] & 0x1F) )
    {
        /* Already got that one. */
        return;
    }

    p_demux->b_has_PSM = 1;
    p_demux->i_PSM_version = p_data->p_demux_start[6] & 0x1F;

    /* Go to elementary_stream_map_length, jumping over
     * program_stream_info. */
    p_byte = p_data->p_demux_start + 10
              + U16_AT(&p_data->p_demux_start[8]);
    if( p_byte > p_data->p_payload_end )
    {
        msg_Err( p_input, "PSM too short, packet corrupt" );
        return;
    }
    /* This is the full size of the elementary_stream_map.
     * 2 == elementary_stream_map_length
     * Please note that CRC_32 is not included in the length. */
    p_end = p_byte + 2 + U16_AT(p_byte);
    p_byte += 2;
    if( p_end > p_data->p_payload_end )
    {
        msg_Err( p_input, "PSM too short, packet corrupt" );
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
                /* FIXME: do something below */
#if 0
                if( p_es->i_type != p_byte[0] )
                {
                    input_DelES( p_input, p_es );
                    p_es = NULL;
                }
                else
#endif
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
            switch( p_byte[0] )
            {
            case MPEG1_VIDEO_ES:
            case MPEG2_VIDEO_ES:
                p_es->i_fourcc = VLC_FOURCC('m','p','g','v');
                p_es->i_cat = VIDEO_ES;
                break;
            case DVD_SPU_ES:
                p_es->i_fourcc = VLC_FOURCC('s','p','u','b');
                p_es->i_cat = SPU_ES;
                break;
            case MPEG1_AUDIO_ES:
            case MPEG2_AUDIO_ES:
                p_es->i_fourcc = VLC_FOURCC('m','p','g','a');
                p_es->i_cat = AUDIO_ES;
                break;
            case A52_AUDIO_ES:
                p_es->i_fourcc = VLC_FOURCC('a','5','2','b');
                p_es->i_cat = AUDIO_ES;
                break;
            case LPCM_AUDIO_ES:
                p_es->i_fourcc = VLC_FOURCC('l','p','c','b');
                p_es->i_cat = AUDIO_ES;
                break;
            default:
                p_es->i_fourcc = 0;
                break;
            }

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
    while( i_new_es_number < p_input->stream.pp_programs[0]->i_es_number )
    {
        /* We remove pp_es[i_new_es_member] and not pp_es[i] because the
         * list will be emptied starting from the end */
        input_DelES( p_input,
                     p_input->stream.pp_programs[0]->pp_es[i_new_es_number] );
    }

    msg_Dbg( p_input, "the stream map after the PSM is now:" );
    input_DumpStream( p_input );

    vlc_mutex_unlock( &p_input->stream.stream_lock );
}

/*****************************************************************************
 * ReadPS: store a PS packet into a data_buffer_t
 *****************************************************************************/
#define PEEK( SIZE )                                                        \
    i_error = input_Peek( p_input, &p_peek, SIZE );                         \
    if( i_error == -1 )                                                     \
    {                                                                       \
        return( -1 );                                                       \
    }                                                                       \
    else if( i_error < SIZE )                                               \
    {                                                                       \
        /* EOF */                                                           \
        return( 0 );                                                        \
    }

static ssize_t ReadPS( input_thread_t * p_input, data_packet_t ** pp_data )
{
    byte_t *            p_peek;
    size_t              i_packet_size;
    ssize_t             i_error, i_read;

    /* Read what we believe to be a packet header. */
    PEEK( 4 );

    if( p_peek[0] || p_peek[1] || p_peek[2] != 1 || p_peek[3] < 0xB9 )
    {
        if( p_peek[0] || p_peek[1] || p_peek[2] )
        {
            /* It is common for MPEG-1 streams to pad with zeros
             * (although it is forbidden by the recommendation), so
             * don't bother everybody in this case. */
            msg_Warn( p_input, "garbage (0x%.2x%.2x%.2x%.2x)",
                      p_peek[0], p_peek[1], p_peek[2], p_peek[3] );
        }

        /* This is not the startcode of a packet. Read the stream
         * until we find one. */
        while( p_peek[0] || p_peek[1] || p_peek[2] != 1 || p_peek[3] < 0xB9 )
        {
            p_input->p_current_data++;
            PEEK( 4 );
            if( p_input->b_die ) return( -1 );
        }
        /* Packet found. */
    }

    /* 0x1B9 == SYSTEM_END_CODE, it is only 4 bytes long. */
    if( p_peek[3] != 0xB9 )
    {
        /* The packet is at least 6 bytes long. */
        PEEK( 6 );

        if( p_peek[3] != 0xBA )
        {
            /* That's the case for all packets, except pack header. */
            i_packet_size = (p_peek[4] << 8) | p_peek[5];
        }
        else
        {
            /* Pack header. */
            if( (p_peek[4] & 0xC0) == 0x40 )
            {
                /* MPEG-2 */
                i_packet_size = 8;
            }
            else if( (p_peek[4] & 0xF0) == 0x20 )
            {
                /* MPEG-1 */
                i_packet_size = 6;
            }
            else
            {
                msg_Err( p_input, "unable to determine stream type" );
                return( -1 );
            }
        }
    }
    else
    {
        /* System End Code */
        i_packet_size = -2;
    }

    /* Fetch a packet of the appropriate size. */
    i_read = input_SplitBuffer( p_input, pp_data, i_packet_size + 6 );
    if( i_read <= 0 )
    {
        return( i_read );
    }

    /* In MPEG-2 pack headers we still have to read stuffing bytes. */
    if( ((*pp_data)->p_demux_start[3] == 0xBA) && (i_packet_size == 8) )
    {
        size_t i_stuffing = ((*pp_data)->p_demux_start[13] & 0x7);
        /* Force refill of the input buffer - though we don't care
         * about p_peek. Please note that this is unoptimized. */
        PEEK( i_stuffing );
        p_input->p_current_data += i_stuffing;
    }

    return( 1 );
}

#undef PEEK

/*****************************************************************************
 * ParsePS: read the PS header
 *****************************************************************************/
static es_descriptor_t * ParsePS( input_thread_t * p_input,
                                  data_packet_t * p_data )
{
    u32                 i_code;
    es_descriptor_t *   p_es = NULL;

    i_code = p_data->p_demux_start[3];

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
                    p_es->i_stream_id = p_data->p_demux_start[3];

                    /* Set stream type and auto-spawn. */
                    if( (i_id & 0xF0) == 0xE0 )
                    {
                        /* MPEG video */
                        p_es->i_fourcc = VLC_FOURCC('m','p','g','v');
                        p_es->i_cat = VIDEO_ES;
#ifdef AUTO_SPAWN
                        if( !p_input->stream.b_seekable )
                            input_SelectES( p_input, p_es );
#endif
                    }
                    else if( (i_id & 0xE0) == 0xC0 )
                    {
                        /* MPEG audio */
                        p_es->i_fourcc = VLC_FOURCC('m','p','g','a');
                        p_es->i_cat = AUDIO_ES;
#ifdef AUTO_SPAWN
                        if( !p_input->stream.b_seekable )
                        if( config_GetInt( p_input, "audio-channel" )
                                == (p_es->i_id & 0x1F) ||
                            ( config_GetInt( p_input, "audio-channel" ) < 0
                              && !(p_es->i_id & 0x1F) ) )
                        switch( config_GetInt( p_input, "audio-type" ) )
                        {
                        case -1:
                        case REQUESTED_MPEG:
                            input_SelectES( p_input, p_es );
                        }
#endif
                    }
                    else if( (i_id & 0xF0FF) == 0x80BD )
                    {
                        /* A52 audio (0x80->0x8F) */
                        p_es->i_fourcc = VLC_FOURCC('a','5','2','b');
                        p_es->i_cat = AUDIO_ES;
#ifdef AUTO_SPAWN
                        if( !p_input->stream.b_seekable )
                        if( config_GetInt( p_input, "audio-channel" )
                                == ((p_es->i_id & 0xF00) >> 8) ||
                            ( config_GetInt( p_input, "audio-channel" ) < 0
                              && !((p_es->i_id & 0xF00) >> 8)) )
                        switch( config_GetInt( p_input, "audio-type" ) )
                        {
                        case -1:
                        case REQUESTED_A52:
                            input_SelectES( p_input, p_es );
                        }
#endif
                    }
                    else if( (i_id & 0xE0FF) == 0x20BD )
                    {
                        /* Subtitles video (0x20->0x3F) */
                        p_es->i_fourcc = VLC_FOURCC('s','p','u','b');
                        p_es->i_cat = SPU_ES;
#ifdef AUTO_SPAWN
                        if( config_GetInt( p_input, "spu-channel" )
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
                        p_es->i_fourcc = VLC_FOURCC('l','p','c','b');
                        p_es->i_cat = AUDIO_ES;
                    }
                    else
                    {
                        p_es->i_fourcc = 0;
                    }
                }

                /* Tell the interface the stream has changed */
                p_input->stream.b_changed = 1;
            }
        } /* stream.b_is_ok */
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    } /* i_code > 0xBC */

    return( p_es );
}

/*****************************************************************************
 * DemuxPS: first step of demultiplexing: the PS header
 *****************************************************************************/
static void DemuxPS( input_thread_t * p_input, data_packet_t * p_data )
{
    u32                 i_code;
    vlc_bool_t          b_trash = 0;
    es_descriptor_t *   p_es = NULL;

    i_code = ((u32)p_data->p_demux_start[0] << 24)
                | ((u32)p_data->p_demux_start[1] << 16)
                | ((u32)p_data->p_demux_start[2] << 8)
                | p_data->p_demux_start[3];
    if( i_code <= 0x1BC )
    {
        switch( i_code )
        {
        case 0x1BA: /* PACK_START_CODE */
            {
                /* Read the SCR. */
                mtime_t         scr_time;
                u32             i_mux_rate;

                if( (p_data->p_demux_start[4] & 0xC0) == 0x40 )
                {
                    /* MPEG-2 */
                    byte_t      p_header[14];
                    byte_t *    p_byte;
                    p_byte = p_data->p_demux_start;

                    if( MoveChunk( p_header, &p_data, &p_byte, 14 ) != 14 )
                    {
                        msg_Warn( p_input,
                                  "packet too short to have a header" );
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
                    /* FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
                     * This is the biggest kludge ever !
                     * I don't know what's wrong with mux_rate calculation
                     * but this heuristic works well : */
                    i_mux_rate <<= 1;
                    i_mux_rate /= 3;
                }
                else
                {
                    /* MPEG-1 SCR is like PTS. */
                    byte_t      p_header[12];
                    byte_t *    p_byte;
                    p_byte = p_data->p_demux_start;

                    if( MoveChunk( p_header, &p_data, &p_byte, 12 ) != 12 )
                    {
                        msg_Warn( p_input,
                                  "packet too short to have a header" );
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
                input_ClockManageRef( p_input,
                                      p_input->stream.p_selected_program,
                                      scr_time );

                if( i_mux_rate != p_input->stream.i_mux_rate
                     && p_input->stream.i_mux_rate )
                {
                    msg_Warn( p_input,
                              "mux_rate changed, expect cosmetic errors" );
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
            msg_Warn( p_input, "unwanted packet received "
                               "with startcode 0x%.8x", i_code );
        }
    }
    else
    {
        p_es = ParsePS( p_input, p_data );

        vlc_mutex_lock( &p_input->stream.control.control_lock );
        if( p_es != NULL && p_es->p_decoder_fifo != NULL
             && (p_es->i_cat != AUDIO_ES || !p_input->stream.control.b_mute) )
        {
            vlc_mutex_unlock( &p_input->stream.control.control_lock );
            p_es->c_packets++;
            GatherPES( p_input, p_data, p_es, 1, 0 );
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
        input_DeletePacket( p_input->p_method_data, p_data );
        p_input->stream.c_packets_trashed++;
    }
}

 
/*
 * TS Demultiplexing
 */

/*****************************************************************************
 * ReadTS: store a TS packet into a data_buffer_t
 *****************************************************************************/
#define PEEK( SIZE )                                                        \
    i_error = input_Peek( p_input, &p_peek, SIZE );                         \
    if( i_error == -1 )                                                     \
    {                                                                       \
        return( -1 );                                                       \
    }                                                                       \
    else if( i_error < SIZE )                                               \
    {                                                                       \
        /* EOF */                                                           \
        return( 0 );                                                        \
    }

static ssize_t ReadTS( input_thread_t * p_input, data_packet_t ** pp_data )
{
    byte_t *            p_peek;
    ssize_t             i_error, i_read;

    PEEK( 1 );

    if( *p_peek != TS_SYNC_CODE )
    {
        msg_Warn( p_input, "garbage at input (%x)", *p_peek );

        if( p_input->i_mtu )
        {
            while( *p_peek != TS_SYNC_CODE )
            {
                /* Try to resync on next packet. */
                PEEK( TS_PACKET_SIZE );
                p_input->p_current_data += TS_PACKET_SIZE;
                PEEK( 1 );
            }
        }
        else
        {
            /* Move forward until we find 0x47 (and hope it's the good
             * one... FIXME) */
            while( *p_peek != TS_SYNC_CODE )
            {
                p_input->p_current_data++;
                PEEK( 1 );
            }
        }
    }

    i_read = input_SplitBuffer( p_input, pp_data, TS_PACKET_SIZE );
    if( i_read <= 0 )
    {
        return( i_read );
    }

    return( 1 );
}

/*****************************************************************************
 * DemuxTS: first step of demultiplexing: the TS header
 *****************************************************************************/
static void DemuxTS( input_thread_t * p_input, data_packet_t * p_data,
                     psi_callback_t pf_psi_callback )
{
    u16                 i_pid;
    int                 i_dummy;
    vlc_bool_t          b_adaptation;         /* Adaptation field is present */
    vlc_bool_t          b_payload;                 /* Packet carries payload */
    vlc_bool_t          b_unit_start;  /* A PSI or a PES start in the packet */
    vlc_bool_t          b_trash = 0;             /* Is the packet unuseful ? */
    vlc_bool_t          b_lost = 0;             /* Was there a packet loss ? */
    vlc_bool_t          b_psi = 0;                        /* Is this a PSI ? */
    vlc_bool_t          b_pcr = 0;                   /* Does it have a PCR ? */
    es_descriptor_t *   p_es = NULL;
    es_ts_data_t *      p_es_demux = NULL;
    pgrm_ts_data_t *    p_pgrm_demux = NULL;

#define p (p_data->p_demux_start)
    /* Extract flags values from TS common header. */
    i_pid = ((p[1] & 0x1F) << 8) | p[2];
    b_unit_start = (p[1] & 0x40);
    b_adaptation = (p[3] & 0x20);
    b_payload = (p[3] & 0x10);

    /* Find out the elementary stream. */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    
    for( i_dummy = 0; i_dummy < p_input->stream.i_pgrm_number; i_dummy ++ )
    {
        if( (( pgrm_ts_data_t * ) p_input->stream.pp_programs[i_dummy]->
                    p_demux_data)->i_pcr_pid == i_pid )
        {
            b_pcr = 1;
            break;
        }
    }
            
    p_es= input_FindES( p_input, i_pid );
    
    if( (p_es != NULL) && (p_es->p_demux_data != NULL) )
    {
        p_es_demux = (es_ts_data_t *)p_es->p_demux_data;
        
        if( p_es_demux->b_psi )
        {
            b_psi = 1;
        }
        else
        {
            p_pgrm_demux = (pgrm_ts_data_t *)p_es->p_pgrm->p_demux_data; 
        }
    }

    vlc_mutex_lock( &p_input->stream.control.control_lock );
    if( ( p_es == NULL ) || (p_es->i_cat == AUDIO_ES
                              && p_input->stream.control.b_mute) )
    {
        /* Not selected. Just read the adaptation field for a PCR. */
        b_trash = 1;
    }
    else if( p_es->p_decoder_fifo == NULL && !b_psi )
    {
        b_trash = 1; 
    }

    vlc_mutex_unlock( &p_input->stream.control.control_lock );
    vlc_mutex_unlock( &p_input->stream.stream_lock );


    /* Don't change the order of the tests : if b_psi then p_pgrm_demux 
     * may still be null. Who said it was ugly ?
     * I have written worse. --Meuuh */
    if( ( p_es  ) && 
        ((p_es->p_decoder_fifo != NULL) || b_psi || b_pcr ) )
    {
        p_es->c_packets++;

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
                    msg_Warn( p_input, "invalid TS adaptation field (%p)",
                              p_data );
                    p_data->b_discard_payload = 1;
                    p_es->c_invalid_packets++;
                }
    
                /* Now we are sure that the byte containing flags is present:
                 * read it */
                else
                {
                    /* discontinuity_indicator */
                    if( p[5] & 0x80 )
                    {
                        msg_Warn( p_input,
                            "discontinuity_indicator encountered by TS demux "
                            "(position read: %d, saved: %d)",
                            p[5] & 0x80, p_es_demux->i_continuity_counter );
    
                        /* If the PID carries the PCR, there will be a system
                         * time-based discontinuity. We let the PCR decoder
                         * handle that. */
                        p_es->p_pgrm->i_synchro_state = SYNCHRO_REINIT;
    
                        /* There also may be a continuity_counter
                         * discontinuity: resynchronize our counter with
                         * the one of the stream. */
                        p_es_demux->i_continuity_counter = (p[3] & 0x0f) - 1;
                    }
    
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
                b_trash = 1;
            }
            else if( i_dummy <= 0 )
            {
                /* Duplicate packet: mark it as being to be trashed. */
                msg_Warn( p_input,
                          "duplicate packet received by TS demux" );
                b_trash = 1;
            }
            else if( p_es_demux->i_continuity_counter == 0xFF )
            {
                /* This means that the packet is the first one we receive for
                 * this ES since the continuity counter ranges between 0 and
                 * 0x0F excepts when it has been initialized by the input:
                 * init the counter to the correct value. */
                msg_Warn( p_input, "first packet for PID %d received "
                                   "by TS demux", p_es->i_id );
                p_es_demux->i_continuity_counter = (p[3] & 0x0f);
            }
            else
            {
                /* This can indicate that we missed a packet or that the
                 * continuity_counter wrapped and we received a dup packet:
                 * as we don't know, do as if we missed a packet to be sure
                 * to recover from this situation */
                msg_Warn( p_input,
                          "packet lost by TS demux: current %d, packet %d",
                          p_es_demux->i_continuity_counter & 0x0f,
                          p[3] & 0x0f );
                b_lost = 1;
                p_es_demux->i_continuity_counter = p[3] & 0x0f;
            } /* not continuous */
        } /* continuity */
    } /* if selected or PCR */
    
    /* Handle PCR */
    if( b_pcr && b_adaptation && (p[5] & 0x10) && p[4]>=7 )
    {
        /* Read the PCR. */
        mtime_t     pcr_time;
        pcr_time = ( (mtime_t)p[6] << 25 ) |
                   ( (mtime_t)p[7] << 17 ) |
                   ( (mtime_t)p[8] << 9 ) |
                   ( (mtime_t)p[9] << 1 ) |
                   ( (mtime_t)p[10] >> 7 );
        /* Call the pace control. */
        for( i_dummy = 0; i_dummy < p_input->stream.i_pgrm_number; 
                                i_dummy ++ )
        {
            if( ( ( pgrm_ts_data_t * ) p_input->stream.pp_programs[i_dummy]->
                        p_demux_data )->i_pcr_pid == i_pid )
            {
                input_ClockManageRef( p_input,
                    p_input->stream.pp_programs[i_dummy], pcr_time );
            }
        }

    }
    
    /* Trash the packet if it has no payload or if it isn't selected */
    if( b_trash )
    {
        input_DeletePacket( p_input->p_method_data, p_data );
        p_input->stream.c_packets_trashed++;
    }
    else
    {
        if( b_psi )
        {
            /* The payload contains PSI tables */
            (* pf_psi_callback) ( p_input, p_data, p_es, b_unit_start );
        }
        else
        {
            /* The payload carries a PES stream */
            GatherPES( p_input, p_data, p_es, b_unit_start, b_lost ); 
        }

    }

#undef p

}

