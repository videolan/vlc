/*****************************************************************************
 * mpeg_adec.h : audio decoder thread interface
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: decoder.h,v 1.4 2002/12/06 16:34:05 sam Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
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
 * adec_thread_t : audio decoder thread descriptor
 *****************************************************************************/
typedef struct adec_thread_s
{
    /*
     * Sync Information
     */
    int                 i_sync;
    /*
     * Input properties
     */
    decoder_fifo_t *    p_fifo;                /* stores the PES stream data */
    /* The bit stream structure handles the PES stream at the bit level */
    bit_stream_t        bit_stream;
    int                 i_read_bits;

    /*
     * Decoder properties
     */
    uint32_t            header;
    unsigned int        frame_size;
    adec_bank_t         bank_0;
    adec_bank_t         bank_1;

    /*
     * Output properties
     */
    aout_instance_t     *p_aout;       /* opaque */
    aout_input_t        *p_aout_input; /* opaque */
    audio_sample_format_t output_format;
    audio_date_t        end_date;
} adec_thread_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/*
 * From adec_generic.c
 */
int adec_SyncFrame( adec_thread_t *, adec_sync_info_t * );
int adec_DecodeFrame( adec_thread_t * , float * );

