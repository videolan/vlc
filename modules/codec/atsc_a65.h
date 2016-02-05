/*****************************************************************************
 * atsc_a65.h : ATSC A65 decoding helpers
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
#ifndef VLC_ATSC_A65_H
#define VLC_ATSC_A65_H

#define GPS_UTC_EPOCH_OFFSET 315964800

typedef struct atsc_a65_handle_t atsc_a65_handle_t;

atsc_a65_handle_t *atsc_a65_handle_New( const char *psz_lang );
void atsc_a65_handle_Release( atsc_a65_handle_t * );

char * atsc_a65_Decode_multiple_string( atsc_a65_handle_t *, const uint8_t *, size_t );
char * atsc_a65_Decode_simple_UTF16_string( atsc_a65_handle_t *, const uint8_t *, size_t );

static inline time_t atsc_a65_GPSTimeToEpoch( time_t i_seconds, time_t i_gpstoepoch_leaptime_offset )
{
    return i_seconds + GPS_UTC_EPOCH_OFFSET - i_gpstoepoch_leaptime_offset;
}

#endif
