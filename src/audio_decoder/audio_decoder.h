/*****************************************************************************
 * audio_decoder.h : audio decoder thread interface
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: audio_decoder.h,v 1.6 2001/03/21 13:42:34 sam Exp $
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
     * Thread properties
     */
    vlc_thread_t        thread_id;                /* id for thread functions */

    /*
     * Input properties
     */
    decoder_fifo_t *    p_fifo;                /* stores the PES stream data */
    /* The bit stream structure handles the PES stream at the bit level */
    bit_stream_t        bit_stream;
    int                 i_read_bits;
    adec_config_t *     p_config;

    /*
     * Decoder properties
     */
    u32                 header;
    int                 frame_size;
    adec_bank_t         bank_0;
    adec_bank_t         bank_1;

    /*
     * Output properties
     */
    struct aout_fifo_s *    p_aout_fifo;   /* stores the decompressed frames */
    struct aout_thread_s *  p_aout;       /* needed to create the audio fifo */

} adec_thread_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
vlc_thread_t adec_CreateThread   ( adec_config_t * p_config );
int          adec_Init           ( adec_thread_t * p_adec );
int          adec_SyncFrame      ( adec_thread_t * p_adec,
                                   adec_sync_info_t * p_sync_info );
int          adec_DecodeFrame    ( adec_thread_t * p_adec, s16 * buffer );

