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

typedef struct mp4mux_handle_t mp4mux_handle_t;

typedef struct
{
    uint64_t i_pos;
    int      i_size;

    vlc_tick_t  i_pts_dts;
    vlc_tick_t  i_length;
    unsigned int i_flags;
} mp4mux_sample_t;

typedef struct
{
    vlc_tick_t i_duration;
    vlc_tick_t i_start_time;
    vlc_tick_t i_start_offset;
} mp4mux_edit_t;

typedef struct
{
    unsigned i_track_id;
    es_format_t   fmt;

    /* index */
    unsigned int i_samples_count;
    unsigned int i_samples_max;
    mp4mux_sample_t *samples;

    /* XXX: needed for other codecs too, see lavf */
    block_t      *a52_frame;

    /* stats */
    vlc_tick_t   i_read_duration;
    uint32_t     i_timescale;
    vlc_tick_t   i_firstdts; /* the really first packet */
    bool         b_hasbframes;

    /* temp stuff */
    /* for later stco fix-up (fast start files) */
    uint64_t     i_stco_pos;

    /* frags */
    vlc_tick_t   i_trex_default_length;
    uint32_t     i_trex_default_size;

    /* edit list */
    unsigned int i_edits_count;
    mp4mux_edit_t *p_edits;

} mp4mux_trackinfo_t;

enum mp4mux_options
{
    FRAGMENTED          = 1 << 0,
    QUICKTIME           = 1 << 1,
    USE64BITEXT         = 1 << 2,
};

mp4mux_handle_t * mp4mux_New(enum mp4mux_options);
void mp4mux_Delete(mp4mux_handle_t *);
void mp4mux_Set64BitExt(mp4mux_handle_t *);
bool mp4mux_Is(mp4mux_handle_t *, enum mp4mux_options);

mp4mux_trackinfo_t * mp4mux_track_Add(mp4mux_handle_t *, unsigned id,
                                      const es_format_t *fmt, uint32_t timescale);
/* ELST */
bool mp4mux_track_AddEdit(mp4mux_trackinfo_t *, const mp4mux_edit_t *);
const mp4mux_edit_t *mp4mux_track_GetLastEdit(const mp4mux_trackinfo_t *);
void mp4mux_track_DebugEdits(vlc_object_t *, const mp4mux_trackinfo_t *);
/* Samples */
bool mp4mux_track_AddSample(mp4mux_trackinfo_t *, const mp4mux_sample_t *);
mp4mux_sample_t *mp4mux_track_GetLastSample(mp4mux_trackinfo_t *);


bo_t *mp4mux_GetMoov(mp4mux_handle_t *, vlc_object_t *, vlc_tick_t i_movie_duration);

/* old */

bo_t *box_new     (const char *fcc);
bo_t *box_full_new(const char *fcc, uint8_t v, uint32_t f);
void  box_fix     (bo_t *box, uint32_t);
void  box_gather  (bo_t *box, bo_t *box2);

bool mp4mux_CanMux(vlc_object_t *, const es_format_t *, vlc_fourcc_t, bool);
bo_t *mp4mux_GetFtyp(vlc_fourcc_t, uint32_t, vlc_fourcc_t[], size_t i_fourcc);

