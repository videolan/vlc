/*****************************************************************************
 * mpeg_system.c: TS, PS and PES management
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 *
 * Authors: 
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
#include <netinet/in.h>

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

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/


/*
 * PES Packet management
 */

/*****************************************************************************
 * input_DecodePES
 *****************************************************************************
 * Put a PES in the decoder's fifo.
 *****************************************************************************/
void input_DecodePES( input_thread_t * p_input, es_descriptor_t * p_es )
{
#define p_pes (p_es->p_pes)

    /* FIXME: since we don't check the type of the stream anymore, we don't
     * do the following : p_data->p_payload_start++; for DVD_SPU_ES, and
     * DVD SPU support is BROKEN ! */

    if( p_es->p_decoder_fifo != NULL )
    {
        vlc_mutex_lock( &p_es->p_decoder_fifo->data_lock );

#if 0
        if( p_input->stream.b_pace_control )
        {
            /* FIXME : normally we shouldn't need this... */
            while( DECODER_FIFO_ISFULL( *p_es->p_decoder_fifo ) )
            {
                vlc_mutex_unlock( &p_es->p_decoder_fifo->data_lock );
                msleep( 20000 );
                vlc_mutex_lock( &p_es->p_decoder_fifo->data_lock );
            }
        }
#endif

        if( !DECODER_FIFO_ISFULL( *p_es->p_decoder_fifo ) )
        {
            //intf_DbgMsg("Putting %p into fifo %p/%d\n",
            //            p_pes, p_fifo, p_fifo->i_end);
            p_es->p_decoder_fifo->buffer[p_es->p_decoder_fifo->i_end] = p_pes;
            DECODER_FIFO_INCEND( *p_es->p_decoder_fifo );

            /* Warn the decoder that it's got work to do. */
            vlc_cond_signal( &p_es->p_decoder_fifo->data_wait );
        }
        else
        {
            /* The FIFO is full !!! This should not happen. */
            p_input->p_plugin->pf_delete_pes( p_input->p_method_data, p_pes );
            intf_ErrMsg( "PES trashed - fifo full ! (%d, %d)",
                       p_es->i_id, p_es->i_type);
        }
        vlc_mutex_unlock( &p_es->p_decoder_fifo->data_lock );
    }
    else
    {
        intf_ErrMsg("No fifo to receive PES %p (who wrote this damn code ?)",
                    p_pes);
        p_input->p_plugin->pf_delete_pes( p_input->p_method_data, p_pes );
    }
    p_pes = NULL;

#undef p_pes
}

/*****************************************************************************
 * input_ParsePES
 *****************************************************************************
 * Parse a finished PES packet and analyze its header.
 *****************************************************************************/
#define PES_HEADER_SIZE     14
void input_ParsePES( input_thread_t * p_input, es_descriptor_t * p_es )
{
    data_packet_t * p_header_data;
    byte_t          p_header[PES_HEADER_SIZE];
    int             i_done, i_todo;

#define p_pes (p_es->p_pes)

    //intf_DbgMsg("End of PES packet %p\n", p_pes);

    /* Parse the header. The header has a variable length, but in order
     * to improve the algorithm, we will read the 14 bytes we may be
     * interested in */
    p_header_data = p_pes->p_first;
    i_done = 0;

    for( ; ; )
    {
        i_todo = p_header_data->p_payload_end
                     - p_header_data->p_payload_start;
        if( i_todo > PES_HEADER_SIZE - i_done )
            i_todo = PES_HEADER_SIZE - i_done;

        memcpy( p_header + i_done, p_header_data->p_payload_start,
                i_todo );
        i_done += i_todo;

        if( i_done < PES_HEADER_SIZE && p_header_data->p_next != NULL )
        {
            p_header_data = p_header_data->p_next;
        }
        else
        {
            break;
        }
    }

    if( i_done != PES_HEADER_SIZE )
    {
        intf_WarnMsg( 3, "PES packet too short to have a header" );
        p_input->p_plugin->pf_delete_pes( p_input->p_method_data, p_pes );
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
        p_input->p_plugin->pf_delete_pes( p_input->p_method_data, p_pes );
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
            p_pes->b_messed_up = 1;
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
                p_pes->b_data_alignment = p_header[6] & 0x04;
                p_pes->b_has_pts = p_header[7] & 0x80;
                i_pes_header_size = p_header[8] + 9;

                /* Now parse the optional header extensions (in the limit of
                 * the 14 bytes). */
                if( p_pes->b_has_pts )
                {
                    p_pes->i_pts =
                      ( ((mtime_t)(p_header[9] & 0x0E) << 29) |
                        (((mtime_t)U16_AT(p_header + 10) << 14) - (1 << 14)) |
                        ((mtime_t)U16_AT(p_header + 12) >> 1) ) * 300;
                    p_pes->i_pts /= 27;
                }
            }
            else
            {
                /* Probably MPEG-1 */
                byte_t *        p_byte;
                data_packet_t * p_data;

                i_pes_header_size = 6;
                p_data = p_pes->p_first;
                p_byte = p_data->p_buffer + 6;
                while( *p_byte == 0xFF && i_pes_header_size < 22 )
                {
                    i_pes_header_size++;
                    p_byte++;
                    if( p_byte >= p_data->p_payload_end )
                    {
                        p_data = p_data->p_next;
                        if( p_data == NULL )
                        {
                            intf_ErrMsg( "MPEG-1 packet too short for header" );
                            p_input->p_plugin->pf_delete_pes( p_input->p_method_data, p_pes );
                            p_pes = NULL;
                            return;
                        }
                        p_byte = p_data->p_payload_start;
                    }
                }
                if( i_pes_header_size == 22 )
                {
                    intf_ErrMsg( "Too much MPEG-1 stuffing" );
                    p_input->p_plugin->pf_delete_pes( p_input->p_method_data, p_pes );
                    p_pes = NULL;
                    return;
                }

                if( (*p_byte & 0xC0) == 0x40 )
                {
                    /* Don't ask why... --Meuuh */
                    p_byte += 2;
                    i_pes_header_size += 2;
                    if( p_byte >= p_data->p_payload_end )
                    {
                        int i_plus = p_byte - p_data->p_payload_end;
                        p_data = p_data->p_next;
                        if( p_data == NULL )
                        {
                            intf_ErrMsg( "MPEG-1 packet too short for header" );
                            p_input->p_plugin->pf_delete_pes( p_input->p_method_data, p_pes );
                            p_pes = NULL;
                            return;
                        }
                        p_byte = p_data->p_payload_start + i_plus;
                    }
                }

                i_pes_header_size++;
                p_pes->b_has_pts = *p_byte & 0x20;

                if( *p_byte & 0x10 )
                {
                    /* DTS */
                    i_pes_header_size += 5;
                }
                if( *p_byte & 0x20 )
                {
                    /* PTS */
                    byte_t      p_pts[5];
                    int         i;

                    i_pes_header_size += 4;
                    p_pts[0] = *p_byte;
                    for( i = 1; i < 5; i++ )
                    {
                        p_byte++;
                        if( p_byte >= p_data->p_payload_end )
                        {
                            p_data = p_data->p_next;
                            if( p_data == NULL )
                            {
                                intf_ErrMsg( "MPEG-1 packet too short for header" );
                                p_input->p_plugin->pf_delete_pes( p_input->p_method_data, p_pes );
                                p_pes = NULL;
                                return;
                            }
                            p_byte = p_data->p_payload_start;
                        }

                        p_pts[i] = *p_byte;
                    }
                    p_pes->i_pts =
                      ( ((mtime_t)(p_pts[0] & 0x0E) << 29) |
                        (((mtime_t)U16_AT(p_pts + 1) << 14) - (1 << 14)) |
                        ((mtime_t)U16_AT(p_pts + 3) >> 1) ) * 300;
                    p_pes->i_pts /= 27;
                }
            }

            /* PTS management */
            if( p_pes->b_has_pts )
            {
                switch( p_es->p_pgrm->i_synchro_state )
                {
                case SYNCHRO_NOT_STARTED:
                    p_pes->b_has_pts = 0;
                    break;

                case SYNCHRO_START:
                    p_pes->i_pts += p_es->p_pgrm->delta_cr;
                    p_es->p_pgrm->delta_absolute = mdate()
                                     - p_pes->i_pts + DEFAULT_PTS_DELAY;
                    p_pes->i_pts += p_es->p_pgrm->delta_absolute;
                    p_es->p_pgrm->i_synchro_state = SYNCHRO_OK;
                    break;

                case SYNCHRO_REINIT: /* We skip a PES | Why ?? --Meuuh */
                    p_pes->b_has_pts = 0;
                    p_es->p_pgrm->i_synchro_state = SYNCHRO_START;
                    break;

                case SYNCHRO_OK:
                    p_pes->i_pts += p_es->p_pgrm->delta_cr
                                         + p_es->p_pgrm->delta_absolute;
                    break;
                }
            }
            break;
        }

        /* Now we've parsed the header, we just have to indicate in some
         * specific data packets where the PES payload begins (renumber
         * p_payload_start), so that the decoders can find the beginning
         * of their data right out of the box. */
        p_header_data = p_pes->p_first;
        i_payload_size = p_header_data->p_payload_end
                                 - p_header_data->p_payload_start;
        while( i_pes_header_size > i_payload_size )
        {
            /* These packets are entirely filled by the PES header. */
            i_pes_header_size -= i_payload_size;
            p_header_data->p_payload_start = p_header_data->p_payload_end;
            /* Go to the next data packet. */
            if( (p_header_data = p_header_data->p_next) == NULL )
            {
                intf_ErrMsg( "PES header bigger than payload" );
                p_input->p_plugin->pf_delete_pes( p_input->p_method_data,
                                                  p_pes );
                p_pes = NULL;
                return;
            }
            i_payload_size = p_header_data->p_payload_end
                                 - p_header_data->p_payload_start;
        }
        /* This last packet is partly header, partly payload. */
        if( i_payload_size < i_pes_header_size )
        {
            intf_ErrMsg( "PES header bigger than payload" );
            p_input->p_plugin->pf_delete_pes( p_input->p_method_data, p_pes );
            p_pes = NULL;
            return;
        }
        p_header_data->p_payload_start += i_pes_header_size;

        /* Now we can eventually put the PES packet in the decoder's
         * PES fifo */
        input_DecodePES( p_input, p_es );
    }
#undef p_pes
}

/*****************************************************************************
 * input_GatherPES:
 *****************************************************************************
 * Gather a PES packet.
 *****************************************************************************/
void input_GatherPES( input_thread_t * p_input, data_packet_t *p_data,
                      es_descriptor_t * p_es,
                      boolean_t b_unit_start, boolean_t b_packet_lost )
{
#define p_pes (p_es->p_pes)

    //intf_DbgMsg("PES-demultiplexing %p (%p)\n", p_ts_packet, p_pes);

    /* If we lost data, insert an NULL data packet (philosophy : 0 is quite
     * often an escape sequence in decoders, so that should make them wait
     * for the next start code). */
    if( b_packet_lost && p_pes != NULL )
    {
        data_packet_t *             p_pad_data;
        if( (p_pad_data = p_input->p_plugin->pf_new_packet( p_input,
                                            PADDING_PACKET_SIZE )) == NULL )
        {
            intf_ErrMsg("Out of memory\n");
            p_input->b_error = 1;
            return;
        }
        memset( p_data->p_buffer, 0, PADDING_PACKET_SIZE );
        p_pad_data->b_discard_payload = 1;
        p_pes->b_messed_up = 1;
        input_GatherPES( p_input, p_pad_data, p_es, 0, 0 );
    }

    if( b_unit_start && p_pes != NULL )
    {
        /* If the TS packet contains the begining of a new PES packet, and
         * if we were reassembling a PES packet, then the PES should be
         * complete now, so parse its header and give it to the decoders. */
        input_ParsePES( p_input, p_es );
    }

    if( !b_unit_start && p_pes == NULL )
    {
        /* Random access... */
        p_input->p_plugin->pf_delete_packet( p_input->p_method_data, p_data );
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
            if( (p_pes = (pes_packet_t *)malloc( sizeof(pes_packet_t) )) == NULL )
            {
                intf_ErrMsg("Out of memory");
                p_input->b_error = 1;
                return;
            }
            //intf_DbgMsg("New PES packet %p (first data: %p)\n", p_pes, p_data);

            /* Init the PES fields so that the first data packet could be
             * correctly added to the PES packet (see below). */
            p_pes->p_first = p_data;
            p_pes->b_messed_up = p_pes->b_discontinuity = 0;
            p_pes->i_pes_size = 0;

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

        p_data->p_next = NULL;
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
 * Pace control
 */

/*
 *   DISCUSSION : SYNCHRONIZATION METHOD
 *
 *   In some cases we can impose the pace of reading (when reading from a
 *   file or a pipe), and for the synchronization we simply sleep() until
 *   it is time to deliver the packet to the decoders. When reading from
 *   the network, we must be read at the same pace as the server writes,
 *   otherwise the kernel's buffer will trash packets. The risk is now to
 *   overflow the input buffers in case the server goes too fast, that is
 *   why we do these calculations :
 *
 *   We compute an average for the pcr because we want to eliminate the
 *   network jitter and keep the low frequency variations. The average is
 *   in fact a low pass filter and the jitter is a high frequency signal
 *   that is why it is eliminated by the filter/average.
 *
 *   The low frequency variations enable us to synchronize the client clock
 *   with the server clock because they represent the time variation between
 *   the 2 clocks. Those variations (ie the filtered pcr) are used to compute
 *   the presentation dates for the audio and video frames. With those dates
 *   we can decode (or trash) the MPEG2 stream at "exactly" the same rate
 *   as it is sent by the server and so we keep the synchronization between
 *   the server and the client.
 *
 *   It is a very important matter if you want to avoid underflow or overflow
 *   in all the FIFOs, but it may be not enough.
 */

/*****************************************************************************
 * Constants
 *****************************************************************************/

/* Maximum number of samples used to compute the dynamic average value,
 * it is also the maximum of c_average_count in pgrm_ts_data_t.
 * We use the following formula :
 * new_average = (old_average * c_average + new_sample_value) / (c_average +1) */
#define CR_MAX_AVERAGE_COUNTER 40

/* Maximum gap allowed between two CRs. */
#define CR_MAX_GAP 1000000

/*****************************************************************************
 * CRReInit : Reinitialize the clock reference
 *****************************************************************************/
static void CRReInit( pgrm_descriptor_t * p_pgrm )
{
    p_pgrm->delta_cr        = 0;
    p_pgrm->last_cr         = 0;
    p_pgrm->c_average_count = 0;
}

/* FIXME: find a better name */
/*****************************************************************************
 * CRDecode : Decode a clock reference
 *****************************************************************************/
static void CRDecode( input_thread_t * p_input, es_descriptor_t * p_es,
                      mtime_t cr_time )
{
    pgrm_descriptor_t *     p_pgrm;
    if( p_es != NULL )
    {
        p_pgrm = p_es->p_pgrm;
    }
    else
    {
        p_pgrm = p_input->stream.pp_programs[0];
    }

    if( p_input->stream.b_pace_control )
    {
        /* Wait a while before delivering the packets to the decoder. */
        mwait( cr_time + p_pgrm->delta_absolute );
    }
    else
    {
        mtime_t                 sys_time, delta_cr;

        sys_time = mdate();
        delta_cr = sys_time - cr_time;

        if( (p_es != NULL && p_es->b_discontinuity) ||
            ( p_pgrm->last_cr != 0 &&
                  (    (p_pgrm->last_cr - cr_time) > CR_MAX_GAP
                    || (p_pgrm->last_cr - cr_time) < - CR_MAX_GAP ) ) )
        {
            intf_WarnMsg( 3, "CR re-initialiazed" );
            CRReInit( p_pgrm );
            p_pgrm->i_synchro_state = SYNCHRO_REINIT;
            if( p_es != NULL )
            {
                p_es->b_discontinuity = 0;
            }
        }
        p_pgrm->last_cr = cr_time;

        if( p_pgrm->c_average_count == CR_MAX_AVERAGE_COUNTER )
        {
            p_pgrm->delta_cr = ( delta_cr + (p_pgrm->delta_cr
                                              * (CR_MAX_AVERAGE_COUNTER - 1)) )
                                 / CR_MAX_AVERAGE_COUNTER;
        }
        else
        {
            p_pgrm->delta_cr = ( delta_cr + (p_pgrm->delta_cr
                                              * p_pgrm->c_average_count) )
                                 / ( p_pgrm->c_average_count + 1 );
            p_pgrm->c_average_count++;
        }

        if( p_pgrm->i_synchro_state == SYNCHRO_NOT_STARTED )
        {
            p_pgrm->i_synchro_state = SYNCHRO_START;
        }
    }
}


/*
 * PS Demultiplexing
 */

/*****************************************************************************
 * DecodePSM: Decode the Program Stream Map information
 *****************************************************************************/
static void DecodePSM( input_thread_t * p_input, data_packet_t * p_data )
{
    stream_ps_data_t *  p_demux =
                 (stream_ps_data_t *)p_input->stream.p_demux_data;

    if( !p_demux->b_is_PSM_complete )
    {
        byte_t *    p_byte;
        byte_t *    p_end;
        int         i_es = 0;

        intf_DbgMsg( "Building PSM" );
        if( p_data->p_payload_start + 10 > p_data->p_payload_end )
        {
            intf_ErrMsg( "PSM too short : packet corrupt" );
            return;
        }
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
         * 4 == CRC_32 */
        p_end = p_byte + 2 + U16_AT(p_byte) - 4;
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
            p_input->p_es[i_es].i_id
                = p_input->p_es[i_es].i_stream_id
                = p_byte[1];
            p_input->p_es[i_es].i_type = p_byte[0];
            p_input->p_es[i_es].p_pgrm = p_input->stream.pp_programs[0];
            p_input->p_es[i_es].b_discontinuity = 0;
            p_input->p_es[i_es].p_pes = NULL;
            p_byte += 4 + U16_AT(&p_byte[2]);

#ifdef AUTO_SPAWN
            switch( p_input->p_es[i_es].i_type )
            {
                case MPEG1_AUDIO_ES:
                case MPEG2_AUDIO_ES:
                    /* Spawn audio thread. */
                    intf_DbgMsg( "Starting an MPEG-audio decoder" );
                    break;

                case MPEG1_VIDEO_ES:
                case MPEG2_VIDEO_ES:
                    /* Spawn video thread. */
                    intf_DbgMsg( "Starting an MPEG-video decoder" );
                    break;
            }
#endif

            i_es++;
        }

        vlc_mutex_unlock( &p_input->stream.stream_lock );
        p_demux->i_PSM_version = p_data->p_buffer[6] & 0x1F;
        p_demux->b_is_PSM_complete = 1;
    }
    else if( p_demux->i_PSM_version != (p_data->p_buffer[6] & 0x1F) )
    {
        /* FIXME */
        intf_ErrMsg( "PSM changed, this is not supported yet !" );
        p_demux->i_PSM_version = p_data->p_buffer[6] & 0x1F;
    }
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
    if( i_code >= 0x1B9 && i_code <= 0x1BC )
    {
        switch( i_code )
        {
        case 0x1BA: /* PACK_START_CODE */
            if( p_input->stream.pp_programs[0]->i_synchro_state == SYNCHRO_OK )
            {
                /* Convert the SCR in microseconds. */
                mtime_t         scr_time;

                if( (p_data->p_buffer[4] & 0xC0) == 0x40 )
                {
                    /* MPEG-2 */
                    scr_time =
                      (( ((mtime_t)(p_data->p_buffer[4] & 0x38) << 27) |
                         ((mtime_t)(p_data->p_buffer[4] & 0x3) << 26) |
                         ((mtime_t)(p_data->p_buffer[5]) << 20) |
                         ((mtime_t)(p_data->p_buffer[6] & 0xF8) << 12) |
                         ((mtime_t)(p_data->p_buffer[6] & 0x3) << 13) |
                         ((mtime_t)(p_data->p_buffer[7]) << 5) |
                         ((mtime_t)(p_data->p_buffer[8] & 0xF8) >> 3)
                      ) * 300) / 27;
                }
                else
                {
                    /* MPEG-1 SCR is like PTS */
                    scr_time =
                      (( ((mtime_t)(p_data->p_buffer[4] & 0x0E) << 29) |
                         (((mtime_t)U16_AT(p_data->p_buffer + 5) << 14)
                           - (1 << 14)) |
                         ((mtime_t)U16_AT(p_data->p_buffer + 7) >> 1)
                      ) * 300) / 27;
                }
                /* Call the pace control. */
                CRDecode( p_input, NULL, scr_time );
            }
            b_trash = 1;
            break;

        case 0x1BB: /* SYSTEM_START_CODE */
            b_trash = 1;                              /* Nothing interesting */
            break;

        case 0x1BC: /* PROGRAM_STREAM_MAP_CODE */
            intf_ErrMsg("meuuuuh\n");
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
        u16                 i_id;
        int                 i_dummy;

        /* This is a PES packet. Find out if we want it or not. */
        i_id = p_data->p_buffer[3];                     /* ID of the stream. */

        vlc_mutex_lock( &p_input->stream.stream_lock );
        for( i_dummy = 0; i_dummy < INPUT_MAX_ES; i_dummy++ )
        {
            if( p_input->p_es[i_dummy].i_id == i_id )
            {
                p_es = &p_input->p_es[i_dummy];
                break;
            }
        }
        vlc_mutex_unlock( &p_input->stream.stream_lock );

        if( p_es == NULL )
        {
#if 1
            /* FIXME ! */
            if( (i_id & 0xC0L) == 0xC0L )
            {
                /* MPEG video and audio */
                for( i_dummy = 0; i_dummy < INPUT_MAX_ES; i_dummy++ )
                {
                    if( p_input->p_es[i_dummy].i_id == EMPTY_ID )
                    {
                        p_es = &p_input->p_es[i_dummy];
                        break;
                    }
                }

                if( p_es != NULL && (i_id & 0xF0L) == 0xE0L )
                {
                    /* MPEG video */
                    vdec_config_t * p_config;
                    p_es->i_id = p_es->i_stream_id = i_id;
                    p_es->i_type = MPEG2_VIDEO_ES;
                    p_es->p_pgrm = p_input->stream.pp_programs[0];
                    p_es->b_discontinuity = 0;
                    p_es->p_pes = NULL;

#ifdef AUTO_SPAWN
                    p_config = (vdec_config_t *)malloc( sizeof(vdec_config_t) );
                    p_config->p_vout = p_input->p_default_vout;
                    /* FIXME ! */
                    p_config->decoder_config.i_stream_id = i_id;
                    p_config->decoder_config.i_type = MPEG2_VIDEO_ES;
                    p_config->decoder_config.p_stream_ctrl =
                        &p_input->stream.control;
                    p_config->decoder_config.p_decoder_fifo =
                        (decoder_fifo_t *)malloc( sizeof(decoder_fifo_t) );
                    vlc_mutex_init(&p_config->decoder_config.p_decoder_fifo->data_lock);
                    vlc_cond_init(&p_config->decoder_config.p_decoder_fifo->data_wait);
                    p_config->decoder_config.p_decoder_fifo->i_start =
                        p_config->decoder_config.p_decoder_fifo->i_end = 0;
                    p_config->decoder_config.p_decoder_fifo->b_die = 0;
                    p_config->decoder_config.p_decoder_fifo->p_packets_mgt =
                        p_input->p_method_data;
                    p_config->decoder_config.p_decoder_fifo->pf_delete_pes =
                        p_input->p_plugin->pf_delete_pes;
                    p_es->p_decoder_fifo = p_config->decoder_config.p_decoder_fifo;
                    p_config->decoder_config.pf_init_bit_stream =
                        InitBitstream;
                    for( i_dummy = 0; i_dummy < INPUT_MAX_SELECTED_ES; i_dummy++ )
                    {
                        if( p_input->pp_selected_es[i_dummy] == NULL )
                        {
                            p_input->pp_selected_es[i_dummy] = p_es;
                            break;
                        }
                    }

                    p_es->thread_id = vpar_CreateThread( p_config );
#endif
                }
                else if( p_es != NULL && (i_id & 0xE0) == 0xC0 )
                {
                    /* MPEG audio */
                    adec_config_t * p_config;
                    p_es->i_id = p_es->i_stream_id = i_id;
                    p_es->i_type = MPEG2_AUDIO_ES;
                    p_es->p_pgrm = p_input->stream.pp_programs[0];
                    p_es->b_discontinuity = 0;
                    p_es->p_pes = NULL;

#ifdef AUTO_SPAWN
                    p_config = (adec_config_t *)malloc( sizeof(adec_config_t) );
                    p_config->p_aout = p_input->p_default_aout;
                    /* FIXME ! */
                    p_config->decoder_config.i_stream_id = i_id;
                    p_config->decoder_config.i_type = MPEG2_AUDIO_ES;
                    p_config->decoder_config.p_stream_ctrl =
                        &p_input->stream.control;
                    p_config->decoder_config.p_decoder_fifo =
                        (decoder_fifo_t *)malloc( sizeof(decoder_fifo_t) );
                    vlc_mutex_init(&p_config->decoder_config.p_decoder_fifo->data_lock);
                    vlc_cond_init(&p_config->decoder_config.p_decoder_fifo->data_wait);
                    p_config->decoder_config.p_decoder_fifo->i_start =
                        p_config->decoder_config.p_decoder_fifo->i_end = 0;
                    p_config->decoder_config.p_decoder_fifo->b_die = 0;
                    p_config->decoder_config.p_decoder_fifo->p_packets_mgt =
                        p_input->p_method_data;
                    p_config->decoder_config.p_decoder_fifo->pf_delete_pes =
                        p_input->p_plugin->pf_delete_pes;
                    p_es->p_decoder_fifo = p_config->decoder_config.p_decoder_fifo;
                    p_config->decoder_config.pf_init_bit_stream =
                        InitBitstream;
                    for( i_dummy = 0; i_dummy < INPUT_MAX_SELECTED_ES; i_dummy++ )
                    {
                        if( p_input->pp_selected_es[i_dummy] == NULL )
                        {
                            p_input->pp_selected_es[i_dummy] = p_es;
                            break;
                        }
                    }

                    p_es->thread_id = adec_CreateThread( p_config );
#endif
                }
                else
                {
                    b_trash = 1;
                }
            }
            else
                b_trash = 1;
#else
            b_trash = 1;
#endif
        }

        if( p_es != NULL )
        {
#ifdef STATS
            p_es->c_packets++;
#endif
            input_GatherPES( p_input, p_data, p_es, 1, 0 );
        }
    }

    /* Trash the packet if it has no payload or if it isn't selected */
    if( b_trash )
    {
        p_input->p_plugin->pf_delete_packet( p_input, p_data );
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

    //intf_DbgMsg("input debug: TS-demultiplexing packet %p, pid %d\n",
    //            p_ts_packet, U16_AT(&p[1]) & 0x1fff);

    /* Extract flags values from TS common header. */
    i_pid = U16_AT(&p[1]) & 0x1fff;
    b_unit_start = (p[1] & 0x40);
    b_adaptation = (p[3] & 0x20);
    b_payload = (p[3] & 0x10);

    /* Find out the elementary stream. */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    for( i_dummy = 0; i_dummy < INPUT_MAX_ES; i_dummy++ )
    {
        if( p_input->p_es[i_dummy].i_id != EMPTY_ID )
        {
            if( p_input->p_es[i_dummy].i_id == i_pid )
            {
                p_es = &p_input->p_es[i_dummy];
                p_es_demux = (es_ts_data_t *)p_es->p_demux_data;
                p_pgrm_demux = (pgrm_ts_data_t *)p_es->p_pgrm->p_demux_data;
                break;
            }
        }
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );

#ifdef STATS
    p_es->c_packets++;
#endif

    if( p_es->p_decoder_fifo == NULL )
    {
        /* Not selected. Just read the adaptation field for a PCR. */
        b_trash = 1;
    }

    if( (p_es->p_decoder_fifo != NULL) || (p_pgrm_demux->i_pcr_pid == i_pid) )
    {
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
                            "discontinuity_indicator"                       \
                            " encountered by TS demux (position read: %d,"  \
                            " saved: %d)",
                            p[5] & 0x80, p_es_demux->i_continuity_counter );
    
                        /* If the PID carries the PCR, there will be a system
                         * time-based discontinuity. We let the PCR decoder
                         * handle that. */
                        p_es->b_discontinuity = 1;
    
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
                            /* Convert the PCR in microseconds.
                             * WARNING: do not remove the casts in the
                             * following calculation ! */
                            mtime_t     pcr_time;
                            pcr_time =
                                    ( (( (mtime_t)U32_AT((u32*)&p[6]) << 1 )
                                      | ( p[10] >> 7 )) * 300 ) / 27;
                            /* Call the pace control. */
                            CRDecode( p_input, p_es, pcr_time );
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
                /* This is a packet without payload, this is allowed by the draft.
                 * As there is nothing interesting in this packet (except PCR that
                 * have already been handled), we can trash the packet. */
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
                /* This means that the packet is the first one we receive for this
                 * ES since the continuity counter ranges between 0 and 0x0F
                 * excepts when it has been initialized by the input: Init the
                 * counter to the correct value. */
                intf_DbgMsg( "First packet for PID %d received by TS demux",
                             p_es->i_id );
                p_es_demux->i_continuity_counter = (p[3] & 0x0f);
            }
            else
            {
                /* This can indicate that we missed a packet or that the
                 * continuity_counter wrapped and we received a dup packet: as we
                 * don't know, do as if we missed a packet to be sure to recover
                 * from this situation */
                intf_WarnMsg( 2,
                           "Packet lost by TS demux: current %d, packet %d\n",
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
        p_input->p_plugin->pf_delete_packet( p_input, p_data );
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
