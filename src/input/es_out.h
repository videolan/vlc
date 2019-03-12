/*****************************************************************************
 * es_out.h: Input es_out functions
 *****************************************************************************
 * Copyright (C) 1998-2008 VLC authors and VideoLAN
 * Copyright (C) 2008 Laurent Aimar
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef LIBVLC_INPUT_ES_OUT_H
#define LIBVLC_INPUT_ES_OUT_H 1

#include <vlc_common.h>

enum es_out_mode_e
{
    ES_OUT_MODE_NONE,   /* don't select anything */
    ES_OUT_MODE_ALL,    /* eg for stream output */
    ES_OUT_MODE_AUTO,   /* best audio/video or for input follow audio-track, sub-track */
    ES_OUT_MODE_PARTIAL,/* select programs given after --programs */
    ES_OUT_MODE_END     /* mark the es_out as dead */
};

enum es_out_query_private_e
{
    /* set/get mode */
    ES_OUT_SET_MODE = ES_OUT_PRIVATE_START,         /* arg1= int                            */

    /* Get date to wait before demuxing more data */
    ES_OUT_GET_WAKE_UP,                             /* arg1=vlc_tick_t*            res=cannot fail */

    /* Wrapper for some ES command to work with id */
    ES_OUT_SET_ES_BY_ID,                            /* arg1= int, arg2= bool (forced) */
    ES_OUT_RESTART_ES_BY_ID,
    ES_OUT_SET_ES_DEFAULT_BY_ID,
    ES_OUT_GET_ES_OBJECTS_BY_ID,                    /* arg1=int id, vlc_object_t **dec, vout_thread_t **, audio_output_t ** res=can fail*/

    /* Stop all selected ES and save the stopped state in a context. free the
     * context or call ES_OUT_STOP_ALL_ES */
    ES_OUT_STOP_ALL_ES,                             /* arg1=void ** */
    /* Start all ES from the context returned by ES_OUT_STOP_ALL_ES */
    ES_OUT_START_ALL_ES,                            /* arg1=void * */

    /* Get buffering state */
    ES_OUT_GET_BUFFERING,                           /* arg1=bool*               res=cannot fail */

    /* Set delay for a ES category */
    ES_OUT_SET_DELAY,                               /* arg1=es_category_e,      res=cannot fail */

    /* Set record state */
    ES_OUT_SET_RECORD_STATE,                        /* arg1=bool                res=can fail */

    /* Set pause state */
    ES_OUT_SET_PAUSE_STATE,                         /* arg1=bool b_source_paused, bool b_paused arg2=vlc_tick_t res=can fail */

    /* Set rate */
    ES_OUT_SET_RATE,                                /* arg1=double source_rate arg2=double rate res=can fail */

    /* Set next frame */
    ES_OUT_SET_FRAME_NEXT,                          /*                          res=can fail */

    /* Set position/time/length */
    ES_OUT_SET_TIMES,                               /* arg1=double f_position arg2=vlc_tick_t i_time arg3=vlc_tick_t i_length res=cannot fail */

    /* Set jitter */
    ES_OUT_SET_JITTER,                              /* arg1=vlc_tick_t i_pts_delay arg2= vlc_tick_t i_pts_jitter, arg2=int i_cr_average res=cannot fail */

    /* Get forced group */
    ES_OUT_GET_GROUP_FORCED,                        /* arg1=int * res=cannot fail */

    /* Set End Of Stream */
    ES_OUT_SET_EOS,                                 /* res=cannot fail */

    /* Set a VBI/Teletext page */
    ES_OUT_SET_VBI_PAGE,                            /* arg1=unsigned res=can fail */

    /* Set VBI/Teletext menu transparent */
    ES_OUT_SET_VBI_TRANSPARENCY                     /* arg1=bool res=can fail */
};

static inline void es_out_SetMode( es_out_t *p_out, int i_mode )
{
    int i_ret = es_out_Control( p_out, ES_OUT_SET_MODE, i_mode );
    assert( !i_ret );
}
static inline vlc_tick_t es_out_GetWakeup( es_out_t *p_out )
{
    vlc_tick_t i_wu;
    int i_ret = es_out_Control( p_out, ES_OUT_GET_WAKE_UP, &i_wu );

    assert( !i_ret );
    return i_wu;
}
static inline bool es_out_GetBuffering( es_out_t *p_out )
{
    bool b;
    int i_ret = es_out_Control( p_out, ES_OUT_GET_BUFFERING, &b );

    assert( !i_ret );
    return b;
}
static inline bool es_out_GetEmpty( es_out_t *p_out )
{
    bool b;
    int i_ret = es_out_Control( p_out, ES_OUT_GET_EMPTY, &b );

    assert( !i_ret );
    return b;
}
static inline void es_out_SetDelay( es_out_t *p_out, int i_cat, vlc_tick_t i_delay )
{
    int i_ret = es_out_Control( p_out, ES_OUT_SET_DELAY, i_cat, i_delay );
    assert( !i_ret );
}
static inline int es_out_SetRecordState( es_out_t *p_out, bool b_record )
{
    return es_out_Control( p_out, ES_OUT_SET_RECORD_STATE, b_record );
}
static inline int es_out_SetPauseState( es_out_t *p_out, bool b_source_paused, bool b_paused, vlc_tick_t i_date )
{
    return es_out_Control( p_out, ES_OUT_SET_PAUSE_STATE, b_source_paused, b_paused, i_date );
}
static inline int es_out_SetRate( es_out_t *p_out, float source_rate, float rate )
{
    return es_out_Control( p_out, ES_OUT_SET_RATE, source_rate, rate );
}
static inline int es_out_SetFrameNext( es_out_t *p_out )
{
    return es_out_Control( p_out, ES_OUT_SET_FRAME_NEXT );
}
static inline void es_out_SetTimes( es_out_t *p_out, double f_position, vlc_tick_t i_time, vlc_tick_t i_length )
{
    int i_ret = es_out_Control( p_out, ES_OUT_SET_TIMES, f_position, i_time, i_length );
    assert( !i_ret );
}
static inline void es_out_SetJitter( es_out_t *p_out,
                                     vlc_tick_t i_pts_delay, vlc_tick_t i_pts_jitter, int i_cr_average )
{
    int i_ret = es_out_Control( p_out, ES_OUT_SET_JITTER,
                                i_pts_delay, i_pts_jitter, i_cr_average );
    assert( !i_ret );
}
static inline int es_out_GetGroupForced( es_out_t *p_out )
{
    int i_group;
    int i_ret = es_out_Control( p_out, ES_OUT_GET_GROUP_FORCED, &i_group );
    assert( !i_ret );
    return i_group;
}
static inline void es_out_Eos( es_out_t *p_out )
{
    int i_ret = es_out_Control( p_out, ES_OUT_SET_EOS );
    assert( !i_ret );
}

es_out_t  *input_EsOutNew( input_thread_t *, float rate );
es_out_t  *input_EsOutTimeshiftNew( input_thread_t *, es_out_t *, float i_rate );

#endif
