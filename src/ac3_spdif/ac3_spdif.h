/*****************************************************************************
 * ac3_spdif.h: header for ac3 pass-through
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: ac3_spdif.h,v 1.5 2001/09/30 20:25:13 bozo Exp $
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
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
 ****************************************************************************/

typedef struct ac3_info_s
{
    int i_bit_rate;
    int i_frame_size;
    int i_sample_rate;
    int i_bs_mod;
} ac3_info_t;

/*****************************************************************************
 * ac3_spdif_thread_t : ac3 pass-through thread descriptor
 *****************************************************************************/
typedef struct ac3_spdif_thread_s
{
    /*
     * Thread properties
     */
    vlc_thread_t        thread_id;                /* id for thread functions */

    /*
     * Input properties
     */
    decoder_fifo_t *    p_fifo;                /* stores the PES stream data */
    adec_config_t *     p_config;

    /* The bit stream structure handles the PES stream at the bit level */
    bit_stream_t        bit_stream;
    int                 i_available;

    /*
     * Decoder properties
     */
    ac3_info_t          ac3_info;
    u8 *                p_ac3;
    u8 *                p_iec;

    /* current pes date */
    mtime_t             i_pts;
    mtime_t             i_real_pts;

    /*
     * Output properties
     */
    aout_fifo_t *       p_aout_fifo; /* stores the decompressed audio frames */

} ac3_spdif_thread_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
vlc_thread_t    spdif_CreateThread( adec_config_t * p_config );

