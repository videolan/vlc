/*****************************************************************************
 * tsutil.c
 *****************************************************************************
 * Copyright (C) 2001-2005, 2015 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_block.h>

#include "tsutil.h"

void PEStoTS( void *p_opaque, PEStoTSCallback pf_callback, block_t *p_pes,
              uint16_t i_pid, bool *pb_discontinuity, uint8_t *pi_continuity_counter )
{
    /* get PES total size */
    uint8_t *p_data = p_pes->p_buffer;
    int      i_size = p_pes->i_buffer;

    bool    b_new_pes = true;

    for (;;)
    {
        /* write header
         * 8b   0x47    sync byte
         * 1b           transport_error_indicator
         * 1b           payload_unit_start
         * 1b           transport_priority
         * 13b          pid
         * 2b           transport_scrambling_control
         * 2b           if adaptation_field 0x03 else 0x01
         * 4b           continuity_counter
         */

        int i_copy = __MIN( i_size, 184 );
        bool b_adaptation_field = i_size < 184;
        block_t *p_ts = block_Alloc( 188 );

        p_ts->p_buffer[0] = 0x47;
        p_ts->p_buffer[1] = ( b_new_pes ? 0x40 : 0x00 )|
                            ( ( i_pid >> 8 )&0x1f );
        p_ts->p_buffer[2] = i_pid & 0xff;
        p_ts->p_buffer[3] = ( b_adaptation_field ? 0x30 : 0x10 )|
                            *pi_continuity_counter;

        b_new_pes = false;
        *pi_continuity_counter = (*pi_continuity_counter+1)%16;

        if( b_adaptation_field )
        {
            int i_stuffing = 184 - i_copy;

            p_ts->p_buffer[4] = i_stuffing - 1;
            if( i_stuffing > 1 )
            {
                p_ts->p_buffer[5] = 0x00;
                if( *pb_discontinuity )
                {
                    p_ts->p_buffer[5] |= 0x80;
                    *pb_discontinuity = false;
                }
                for (int i = 6; i < 6 + i_stuffing - 2; i++ )
                {
                    p_ts->p_buffer[i] = 0xff;
                }
            }
        }
        /* copy payload */
        memcpy( &p_ts->p_buffer[188 - i_copy], p_data, i_copy );
        p_data += i_copy;
        i_size -= i_copy;

        pf_callback( p_opaque, p_ts );

        if( i_size <= 0 )
        {
            block_t *p_next = p_pes->p_next;

            p_pes->p_next = NULL;
            block_Release( p_pes );
            if( p_next == NULL )
                return;

            b_new_pes = true;
            p_pes = p_next;
            i_size = p_pes->i_buffer;
            p_data = p_pes->p_buffer;
        }
    }
}
