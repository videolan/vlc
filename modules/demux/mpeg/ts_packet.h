/*****************************************************************************
 * ts_packet.h : MPEG-TS packet headers
 *****************************************************************************
 * Copyright (C) 2025 - VideoLabs, VideoLAN and VLC authors
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/
#ifndef VLC_TS_PACKET_H
#define VLC_TS_PACKET_H

#include "timestamps.h"

#define TS_PACKET_SIZE_188 188
#define TS_PACKET_SIZE_192 192
#define TS_PACKET_SIZE_204 204
#define TS_PACKET_SIZE_MAX 204
#define TS_HEADER_SIZE 4

#define AS_BUF(pkt) pkt->p_buffer,pkt->i_buffer

static inline unsigned PKTHeaderAndAFSize( const block_t *p_pkt )
{
    const uint8_t *p = p_pkt->p_buffer;
    unsigned i_size = 4;
    if ( p[3] & 0x20 ) // adaptation field
        i_size += 1 + __MIN(p[4], 182);
    return i_size;
}

static inline int PIDGet( const block_t *p )
{
    return ( (p->p_buffer[1]&0x1f)<<8 )|p->p_buffer[2];
}

static inline ts_90khz_t GetPCR( const block_t *p_pkt )
{
    const uint8_t *p = p_pkt->p_buffer;

    ts_90khz_t i_pcr = TS_90KHZ_INVALID;

    if(unlikely(p_pkt->i_buffer < 12))
        return i_pcr;

    const uint8_t i_adaption = p[3] & 0x30;

    if( ( ( i_adaption == 0x30 && p[4] <= 182 ) ||   /* adaptation 0b11 */
          ( i_adaption == 0x20 && p[4] == 183 ) ) && /* adaptation 0b10 */
        ( p[4] >= 7 )  &&
        ( p[5] & 0x10 ) ) /* PCR carry flag */
    {
        /* PCR is 33 bits */
        i_pcr = ( (ts_90khz_t)p[6] << 25 ) |
                ( (ts_90khz_t)p[7] << 17 ) |
                ( (ts_90khz_t)p[8] << 9 ) |
                ( (ts_90khz_t)p[9] << 1 ) |
                ( (ts_90khz_t)p[10] >> 7 );
    }
    return i_pcr;
}

#endif
