/*****************************************************************************
 * spu_decoder.h : sub picture unit decoder thread interface
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
    vlc_thread_t        thread_id;                /* id for thread functions */

    /*
     * Input properties
     */
    decoder_fifo_t *    p_fifo;                /* stores the PES stream data */
    /* The bit stream structure handles the PES stream at the bit level */
    bit_stream_t        bit_stream;
    vdec_config_t *     p_config;

    /*
     * Output properties
     */
    vout_thread_t *     p_vout;          /* needed to create the spu objects */

    /*
     * Private properties
     */
    int                 i_spu_size;            /* size of current SPU packet */
    int                 i_rle_size;                  /* size of the RLE part */
    subpicture_t *      p_spu;

} spudec_thread_t;

/*****************************************************************************
 * SPU commands
 *****************************************************************************/
#define SPU_CMD_FORCE_DISPLAY       0x00
#define SPU_CMD_START_DISPLAY       0x01
#define SPU_CMD_STOP_DISPLAY        0x02
#define SPU_CMD_SET_PALETTE         0x03
#define SPU_CMD_SET_ALPHACHANNEL    0x04
#define SPU_CMD_SET_COORDINATES     0x05
#define SPU_CMD_SET_OFFSETS         0x06
#define SPU_CMD_END                 0xff

/*****************************************************************************
 * GetNibble: read a nibble from a source packet.
 *****************************************************************************/
static __inline__ u8 GetNibble( u8 *p_source, int *pi_index )
{
    if( *pi_index & 0x1 )
    {
        return( p_source[(*pi_index)++ >> 1] & 0xf );
    }
    else
    {
        return( p_source[(*pi_index)++ >> 1] >> 4 );
    }
}

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
vlc_thread_t       spudec_CreateThread( vdec_config_t * p_config );

