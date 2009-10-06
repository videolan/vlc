/*****************************************************************************
 * vout_internal.h : Internal vout definitions
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * Copyright (C) 2008 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar < fenrir _AT_ videolan _DOT_ org >
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/


#if defined(__PLUGIN__) || defined(__BUILTIN__) || !defined(__LIBVLC__)
# error This header file can only be included from LibVLC.
#endif

#ifndef _VOUT_INTERNAL_H
#define _VOUT_INTERNAL_H 1

#include "vout_control.h"
#include "snapshot.h"
#include "statistic.h"

/* Number of pictures required to computes the FPS rate */
#define VOUT_FPS_SAMPLES                20

/* */
struct vout_thread_sys_t
{
    /* module */
    const char *psz_module_type;
    char       *psz_module_name;

    /* Thread & synchronization */
    vlc_thread_t    thread;
    vlc_cond_t      change_wait;
    bool            b_ready;
    bool            b_done;

    /* */
    bool            b_picture_displayed;
    bool            b_picture_empty;
    mtime_t         i_picture_displayed_date;
    picture_t       *p_picture_displayed;
    int             i_picture_qtype;
    bool            b_picture_interlaced;
    vlc_cond_t      picture_wait;

    /* */
    vlc_mutex_t     vfilter_lock;         /**< video filter2 change lock */

    /* */
    uint32_t        render_time;           /**< last picture render time */
    unsigned int    i_par_num;           /**< monitor pixel aspect-ratio */
    unsigned int    i_par_den;           /**< monitor pixel aspect-ratio */

    /* */
    bool            b_direct;            /**< rendered are like direct ? */
    filter_t        *p_chroma;

    /**
     * These numbers are not supposed to be accurate, but are a
     * good indication of the thread status */
    count_t         c_fps_samples;                         /**< picture counts */
    mtime_t         p_fps_sample[VOUT_FPS_SAMPLES];     /**< FPS samples dates */

    /* Statistics */
    vout_statistic_t statistic;

    /* Pause */
    bool            b_paused;
    mtime_t         i_pause_date;

    /* Filter chain */
    bool           b_first_vout;  /* True if it is the first vout of the filter chain */
    char           *psz_filter_chain;
    bool            b_filter_change;

    /* Video filter2 chain */
    filter_chain_t *p_vf2_chain;
    char           *psz_vf2;

    /* Snapshot interface */
    vout_snapshot_t snapshot;

    /* Show media title on videoutput */
    bool            b_title_show;
    mtime_t         i_title_timeout;
    int             i_title_position;

    char            *psz_title;

    /* */
    vlc_mouse_t     mouse;
};

/* DO NOT use vout_RenderPicture unless you are in src/video_ouput */
picture_t *vout_RenderPicture( vout_thread_t *, picture_t *,
                               subpicture_t *,
                               mtime_t render_date );

/* DO NOT use vout_UsePictureLocked unless you are in src/video_ouput
 *
 * This function supposes that you call it with picture_lock taken.
 */
void vout_UsePictureLocked( vout_thread_t *p_vout, picture_t *p_pic  );

#endif

