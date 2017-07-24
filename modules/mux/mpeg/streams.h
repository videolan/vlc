/*****************************************************************************
 * streams.h
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
#ifndef VLC_MPEG_STREAMS_H_
#define VLC_MPEG_STREAMS_H_

typedef struct
{
    uint16_t        i_pid;

    uint8_t         i_stream_type;
    uint8_t         i_continuity_counter;
    bool            b_discontinuity;

} tsmux_stream_t;

typedef struct
{
    int             i_stream_id; /* keep as int for drac */

    /* Specific to mpeg4 in mpeg2ts */
    int             i_es_id;

    /* language is iso639-2T */
    size_t          i_langs;
    uint8_t         *lang;
} pesmux_stream_t;

#endif
