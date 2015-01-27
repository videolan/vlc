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
#ifndef _STREAMS_H
#define _STREAMS_H 1

typedef struct
{
    int             i_pid;

    int             i_continuity_counter;
    bool            b_discontinuity;

} ts_stream_t;

typedef struct
{
    vlc_fourcc_t    i_codec;

    int             i_stream_type;
    int             i_stream_id;

    /* to be used for carriege of DIV3 */
    vlc_fourcc_t    i_bih_codec;
    int             i_bih_width, i_bih_height;

    /* Specific to mpeg4 in mpeg2ts */
    int             i_es_id;

    int             i_extra;
    uint8_t         *p_extra;

    /* language is iso639-2T */
    int             i_langs;
    uint8_t         *lang;
} pes_stream_t;

#endif
