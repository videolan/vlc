
/*****************************************************************************
 * subsdec.h : sub picture unit decoder thread interface
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: subsdec.h,v 1.1 2003/07/22 20:49:10 hartman Exp $
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

#if defined(HAVE_ICONV)
#include <iconv.h>
#endif

typedef struct subsdec_thread_t subsdec_thread_t;


/*****************************************************************************
 * subsdec_thread_t : sub picture unit decoder thread descriptor
 *****************************************************************************/
struct subsdec_thread_t
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

    /*
     * Output properties
     */
    vout_thread_t *     p_vout;          /* needed to create the spu objects */

    /*
     * Private properties
     */
#if defined(HAVE_ICONV)
    iconv_t             iconv_handle;     /* handle to iconv instance */
#endif
};


/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int  E_(SyncPacket)           ( subsdec_thread_t * );
void E_(ParsePacket)          ( subsdec_thread_t * );

void E_(ParseText)            ( subsdec_thread_t * );

