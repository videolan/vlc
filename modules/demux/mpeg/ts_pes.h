/*****************************************************************************
 * ts_pes.h: Transport Stream input module for VLC.
 *****************************************************************************
 * Copyright (C) 2004-2019 VLC authors and VideoLAN
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
#ifndef VLC_TS_PES_H
#define VLC_TS_PES_H

#define BLOCK_FLAG_SOURCE_RANDOM_ACCESS (1 << BLOCK_FLAG_PRIVATE_SHIFT)

typedef struct
{
    vlc_object_t *p_obj;
    void *priv;
    void(*pf_parse)(vlc_object_t *, void *, block_t *);
} ts_pes_parse_callback;

bool ts_pes_Drain( ts_pes_parse_callback *cb, ts_stream_t *p_pes );

bool ts_pes_Gather( ts_pes_parse_callback *cb,
                    ts_stream_t *p_pes, block_t *p_pkt,
                    bool b_unit_start, bool b_valid_scrambling );


#endif
