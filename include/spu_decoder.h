/*****************************************************************************
 * spu_decoder.h : sub picture unit decoder thread interface
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
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
 * spudec_thread_t : sub picture unit decoder thread descriptor
 *****************************************************************************/
typedef struct spudec_thread_s
{
    /*
     * Thread properties and locks
     */
    boolean_t           b_die;                                 /* `die' flag */
    boolean_t           b_run;                                 /* `run' flag */
    boolean_t           b_active;                           /* `active' flag */
    boolean_t           b_error;                             /* `error' flag */
    vlc_thread_t        thread_id;                /* id for thread functions */

    /*
     * Input properties
     */
    decoder_fifo_t      fifo;                  /* stores the PES stream data */
    /* The bit stream structure handles the PES stream at the bit level */
    bit_stream_t        bit_stream;

    /*
     * Decoder properties
     */
    unsigned int        total_bits_read;
    /* ... */
    vout_thread_t *     p_vout;          /* needed to create the spu objects */

} spudec_thread_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
spudec_thread_t *       spudec_CreateThread( input_thread_t * p_input );
void                    spudec_DestroyThread( spudec_thread_t * p_spudec );

