/*****************************************************************************
 * dvdread.h : DvdRead input module for vlc - shared types and declarations
 *****************************************************************************
 * Copyright (C) 2001-2025 VLC authors and VideoLAN
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Saifelden Mohamed Ismail <saifeldenmi@gmail.com>
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

#ifndef VLC_ACCESS_DVDREAD_H
#define VLC_ACCESS_DVDREAD_H

#include <vlc_common.h>
#include <vlc_es.h>
#include <vlc_tick.h>

#include "../demux/mpeg/pes.h"
#include "../demux/mpeg/ps.h"

#include <sys/types.h>
#include <unistd.h>

#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
#include <dvdread/nav_read.h>
#include <dvdread/nav_print.h>

#include <assert.h>
#include <stdckdint.h>

/*****************************************************************************
 * DVDRead Version Compatibility
 *****************************************************************************/
#if DVDREAD_VERSION > DVDREAD_VERSION_CODE(7, 0, 1)
#define DVDREAD_HAS_DVDVIDEORECORDING 1
#endif

#if DVDREAD_VERSION >= DVDREAD_VERSION_CODE(7, 0, 0)
#define DVDREAD_HAS_DVDAUDIO 1
#endif

typedef enum
{
    DVD_V = 0,
    DVD_A = 1,
    DVD_VR = 2,
} dvd_type_t;

typedef struct input_title_t input_title_t;

typedef struct demux_sys_t demux_sys_t;

typedef struct
{
    /* format-specific hooks used by shared Control()/Demux() paths */
    int        (*set_area)( demux_t *, int, int, int );
    int        (*seek)( demux_t *, uint32_t );
    void       (*find_cell)( demux_t * );
    vlc_tick_t (*title_length)( const demux_sys_t * );
    void       (*demux_titles)( demux_t *, int * );
    bool       (*timeline_base)( const demux_sys_t *, vlc_tick_t * );
    uint32_t   (*time_to_seek_offset)( const demux_sys_t *, vlc_tick_t, uint32_t );
    int        (*ps_source)( void );
    void       (*adjust_anchor)( demux_sys_t *, block_t * );
    bool       requires_vts;
} dvdread_ops_t;

struct demux_sys_t
{
    /* DVDRead state */
    dvd_reader_t *p_dvdread;
    dvd_file_t   *p_title;

    ifo_handle_t *p_vmg_file;
    ifo_handle_t *p_vts_file;

    unsigned updates;
    int i_title;
    int cur_title;
    int i_chapter, i_chapters;
    int cur_chapter;
    int i_angle, i_angles;

    dvd_type_t type;
    union
    {
        /* Video tables */
        struct
        {
            tt_srpt_t    *p_tt_srpt;
            pgc_t        *p_cur_pgc;
        };

#ifdef DVDREAD_HAS_DVDAUDIO
        /* Audio tables */
        struct
        {
            atsi_title_record_t *p_title_table;
        };
#endif

#ifdef DVDREAD_HAS_DVDVIDEORECORDING
        /* VideoRecording tables */
        struct
        {
          /* address map of recordings */
          pgc_gi_t    *pgc_gi;
          /* titles, labels for recordings */
          ud_pgcit_t  *ud_pgcit;
          /* keep track of vobu index */
          /* pointer to current program */
          pgi_t *p_cur_pgi;
        };
#endif

    };

    dsi_t        dsi_pack;
    int          i_ttn;

    int i_pack_len;
    int i_cur_block;
    int i_next_vobu;

    int i_mux_rate;

    /* Current title start/end blocks */
    int i_title_start_block;
    int i_title_end_block;
    uint32_t i_title_blocks;
    int i_title_offset;

    int i_title_start_cell;
    int i_title_end_cell;
    int i_cur_cell;
    int i_next_cell;

    /* ps/dvd anchor pair for rebasing ps scr/pts onto the disc timeline
     * dvd is accumulated cell playback time from title start
     * ps is the matching scr from the first ps pack of that cell
     * both are VLC_TICK_INVALID until the first pack of a cell is seen */
    struct
    {
        vlc_tick_t dvd;
        vlc_tick_t ps;
    } cell_ts;

    /* Track */
    ps_track_t    tk[PS_TK_COUNT];

    int           i_titles;
    input_title_t **titles;

    /* Video */
    int i_sar_num;
    int i_sar_den;

    /* SPU */
    uint32_t clut[VIDEO_PALETTE_CLUT_COUNT];
    const dvdread_ops_t *ops;
};

static inline void DvdReadResetCellTs( demux_sys_t *p_sys )
{
    p_sys->cell_ts.dvd = VLC_TICK_INVALID;
    p_sys->cell_ts.ps = VLC_TICK_INVALID;
}

int OpenCommonDvdread( vlc_object_t *, dvd_type_t, const dvdread_ops_t * );
int OpenVideoRecording( vlc_object_t * );
int OpenAudio( vlc_object_t * );

#ifdef DVDREAD_HAS_DVDVIDEORECORDING
uint32_t   DvdVRGetProgramSectorSpan( const demux_sys_t *, const vobu_map_t * );
#endif

#endif /* VLC_ACCESS_DVDREAD_H */
