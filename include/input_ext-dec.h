/*****************************************************************************
 * input_ext-dec.h: structures exported to the VideoLAN decoders
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: input_ext-dec.h,v 1.83 2003/11/24 00:39:00 fenrir Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Michel Kaempf <maxx@via.ecp.fr>
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

#ifndef _VLC_INPUT_EXT_DEC_H
#define _VLC_INPUT_EXT_DEC_H 1

/* Structures exported to the decoders */

/*****************************************************************************
 * data_packet_t
 *****************************************************************************
 * Describe a data packet.
 *****************************************************************************/
struct data_packet_t
{
    /* Used to chain the packets that carry data for a same PES or PSI */
    data_packet_t *  p_next;

    /* start of the PS or TS packet */
    byte_t *         p_demux_start;
    /* start of the PES payload in this packet */
    byte_t *         p_payload_start;
    byte_t *         p_payload_end; /* guess ? :-) */
    /* is the packet messed up ? */
    vlc_bool_t       b_discard_payload;

    /* pointer to the real data */
    data_buffer_t *  p_buffer;
};

/*****************************************************************************
 * pes_packet_t
 *****************************************************************************
 * Describes an PES packet, with its properties, and pointers to the TS packets
 * containing it.
 *****************************************************************************/
struct pes_packet_t
{
    /* Chained list to the next PES packet (depending on the context) */
    pes_packet_t *  p_next;

    /* PES properties */
    vlc_bool_t      b_data_alignment;          /* used to find the beginning of
                                                * a video or audio unit */
    vlc_bool_t      b_discontinuity;          /* This packet doesn't follow the
                                               * previous one */

    mtime_t         i_pts;            /* PTS for this packet (zero if unset) */
    mtime_t         i_dts;            /* DTS for this packet (zero if unset) */
    int             i_rate;   /* current reading pace (see stream_control.h) */

    unsigned int    i_pes_size;            /* size of the current PES packet */

    /* Chained list to packets */
    data_packet_t * p_first;              /* The first packet contained by this
                                           * PES (used by decoders). */
    data_packet_t * p_last;            /* The last packet contained by this
                                        * PES (used by the buffer allocator) */
    unsigned int    i_nb_data; /* Number of data packets in the chained list */
};

/*****************************************************************************
 * Prototypes from input_ext-dec.c
 *****************************************************************************/
VLC_EXPORT( void, input_DeletePES,         ( input_buffers_t *, pes_packet_t * ) );

#endif /* "input_ext-dec.h" */
