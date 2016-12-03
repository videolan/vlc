/*****************************************************************************
 * libmp4mux.h: mp4/mov muxer
 *****************************************************************************
 * Copyright (C) 2001, 2002, 2003, 2006, 20115 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin at videolan dot org>
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
#include <vlc_common.h>
#include <vlc_es.h>
#include <vlc_boxes.h>

typedef struct
{
    uint64_t i_pos;
    int      i_size;

    mtime_t  i_pts_dts;
    mtime_t  i_length;
    unsigned int i_flags;
} mp4mux_entry_t;

typedef struct
{
    uint64_t i_duration;
    mtime_t i_start_time;
    mtime_t i_start_offset;
} mp4mux_edit_t;

typedef struct
{
    unsigned i_track_id;
    es_format_t   fmt;

    /* index */
    unsigned int i_entry_count;
    unsigned int i_entry_max;
    mp4mux_entry_t *entry;

    /* XXX: needed for other codecs too, see lavf */
    block_t      *a52_frame;

    /* stats */
    int64_t      i_read_duration;
    uint32_t     i_timescale;
    mtime_t      i_firstdts; /* the really first packet */
    bool         b_hasbframes;

    /* temp stuff */
    /* for later stco fix-up (fast start files) */
    uint64_t     i_stco_pos;

    /* frags */
    uint32_t     i_trex_default_length;
    uint32_t     i_trex_default_size;

    /* edit list */
    unsigned int i_edits_count;
    mp4mux_edit_t *p_edits;

} mp4mux_trackinfo_t;

bool mp4mux_trackinfo_Init( mp4mux_trackinfo_t *, unsigned, uint32_t );
void mp4mux_trackinfo_Clear( mp4mux_trackinfo_t * );

bo_t *box_new     (const char *fcc);
bo_t *box_full_new(const char *fcc, uint8_t v, uint32_t f);
void  box_fix     (bo_t *box, uint32_t);
void  box_gather  (bo_t *box, bo_t *box2);

bool mp4mux_CanMux(vlc_object_t *, const es_format_t *);
bo_t *mp4mux_GetFtyp(vlc_fourcc_t, uint32_t, vlc_fourcc_t[], size_t i_fourcc);
bo_t *mp4mux_GetMoovBox(vlc_object_t *, mp4mux_trackinfo_t **pp_tracks, unsigned int i_tracks,
                        int64_t i_movie_duration,
                        bool b_fragmented, bool b_mov, bool b_64ext, bool b_stco64);
