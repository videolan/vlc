/*****************************************************************************
 * ac3_adec.h : ac3 decoder thread interface
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: ac3_adec.h,v 1.2 2002/01/22 23:14:26 massiot Exp $
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
 * ac3dec_thread_t : ac3 decoder thread descriptor
 *****************************************************************************/

typedef struct ac3dec_thread_s
{
    /*
     * Decoder properties
     */
    ac3dec_t *          ac3_decoder;
    
    /*
     * Thread properties
     */
    vlc_thread_t        thread_id;                /* id for thread functions */

    /*
     * Input properties
     */
    decoder_fifo_t *    p_fifo;                /* stores the PES stream data */
    decoder_config_t *  p_config;

    /*
     * Output properties
     */
    aout_fifo_t *       p_aout_fifo; /* stores the decompressed audio frames */

} ac3dec_thread_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
vlc_thread_t            ac3dec_CreateThread( decoder_config_t * p_config );
