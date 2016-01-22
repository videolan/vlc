/*****************************************************************************
 * scte18.h : SCTE-18 EAS decoder
 *****************************************************************************
 * Copyright (C) 2016 - VideoLAN Authors
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
#ifndef VLC_SCTE18_H
#define VLC_SCTE18_H

#define SCTE18_SI_BASE_PID 0x1FFB
#define SCTE18_TABLE_ID    0xD8
#define SCTE18_DESCRIPTION N_("Emergency Alert Messaging for Cable")

enum
{
    EAS_PRIORITY_TEST = 0,
    EAS_PRIORITY_LOW = 3,
    EAS_PRIORITY_MEDIUM = 7,
    EAS_PRIORITY_HIGH = 11,
    EAS_PRIORITY_MAX = 15
};

/* Get priority without decoding
 */
static inline int scte18_get_EAS_priority( const uint8_t *p_buffer, size_t i_buffer )
{
    if( i_buffer < 17 || p_buffer[0] )
        return -1;

    size_t i_offset = 6;
    size_t i_len = p_buffer[i_offset]; /* EAS code Len */
    i_offset += i_len + 1; /* EAS code Text */
    if( i_offset >= i_buffer )
        return -1;

    i_len = p_buffer[i_offset]; /* NOA Len */
    i_offset += i_len + 1; /* NOA Text */

    i_offset += 1 + 4 + 2 + 1;

    if( i_offset >= i_buffer )
        return -1;

    return (p_buffer[i_offset] & 0x0f);
}

#endif
