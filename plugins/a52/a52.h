/*****************************************************************************
 * a52.h: ATSC A/52 aka AC-3 decoder plugin for vlc.
 *   This plugin makes use of liba52 to decode A/52 audio
 *   (http://liba52.sf.net/).
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: a52.h,v 1.3 2002/05/20 15:03:33 gbazin Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
 * a52_adec_thread_t : a52 decoder thread descriptor
 *****************************************************************************/
typedef struct a52_adec_thread_s
{
    /*
     * liba52 properties
     */
    a52_state_t *p_a52_state;
    int         frame_size;
    int         flags;
    int         sample_rate;
    int         bit_rate;
    boolean_t   b_dynrng;

    /* The bit stream structure handles the PES stream at the bit level */
    bit_stream_t        bit_stream;

    /*
     * Input properties
     */
    decoder_fifo_t     *p_fifo;                /* stores the PES stream data */
    data_packet_t      *p_data;
    decoder_config_t   *p_config;

    /*
     * Output properties
     */
    aout_fifo_t        *p_aout_fifo; /* stores the decompressed audio frames */
    int                i_channels;

    /* temporary buffer to store the raw frame to be decoded */
    u8 p_frame_buffer[3840];

} a52_adec_thread_t;
