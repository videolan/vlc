/*****************************************************************************
 * clpi.h: BluRay Disc CLPI
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

typedef struct
{
    int     i_pcr_pid;
    int64_t i_packet;   /* TS Packet number */
    int64_t i_start;    /* Presentation start time in 45kHz */
    int64_t i_end;      /* Presentation end time in 45kHz */
} bd_clpi_stc_t;
void bd_clpi_stc_Parse( bd_clpi_stc_t *p_stc, bs_t *s );

typedef struct
{
    int i_pid;          /* PID of the associated stream */
    int i_type;         /* Stream type of the associated stream */
} bd_clpi_stream_t;
void bd_clpi_stream_Parse( bd_clpi_stream_t *p_stream, bs_t *s );

typedef struct
{
    bool    b_angle_point;  /* EP angle point change */
    int64_t i_packet;       /* TS packet number */
    int64_t i_pts;          /* PTS of the associated stream (90kHz, 33bits) */
} bd_clpi_ep_t;

typedef struct
{
    int i_pid;          /* PID of the associated stream */
    int i_type;         /* Stream type of the associated stream */

    int          i_ep;
    bd_clpi_ep_t *p_ep;
} bd_clpi_ep_map_t;
void bd_clpi_ep_map_Clean( bd_clpi_ep_map_t *p_ep_map );
int bd_clpi_ep_map_Parse( bd_clpi_ep_map_t *p_ep_map,
                          bs_t *s, const int i_ep_map_start );

typedef struct
{
    int              i_id;

    int              i_stc;
    bd_clpi_stc_t    *p_stc;
    
    int              i_pmt_pid;
    int              i_stream;
    bd_clpi_stream_t *p_stream;

    int              i_ep_map;
    bd_clpi_ep_map_t *p_ep_map;
} bd_clpi_t;
void bd_clpi_Clean( bd_clpi_t *p_clpi );
int bd_clpi_Parse( bd_clpi_t *p_clpi, bs_t *s, int i_id );

