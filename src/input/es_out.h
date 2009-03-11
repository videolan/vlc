/*****************************************************************************
 * es_out.h: Input es_out functions
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 * Copyright (C) 2008 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef _INPUT_ES_OUT_H
#define _INPUT_ES_OUT_H 1

#include <vlc_common.h>

enum es_out_query_private_e
{

    /* activate application of mode */
    ES_OUT_SET_ACTIVE = ES_OUT_PRIVATE_START,       /* arg1= bool                     */

    /* set/get mode */
    ES_OUT_SET_MODE,                                /* arg1= int                            */

    /* Get date to wait before demuxing more data */
    ES_OUT_GET_WAKE_UP,                             /* arg1=mtime_t*            res=cannot fail */

    /* Wrapper for some ES command to work with id */
    ES_OUT_SET_ES_BY_ID,
    ES_OUT_RESTART_ES_BY_ID,
    ES_OUT_SET_ES_DEFAULT_BY_ID,

    /* Get buffering state */
    ES_OUT_GET_BUFFERING,                           /* arg1=bool*               res=cannot fail */

    /* Set delay for a ES category */
    ES_OUT_SET_DELAY,                               /* arg1=es_category_e,      res=can fail */

    /* Set record state */
    ES_OUT_SET_RECORD_STATE,                        /* arg1=bool                res=can fail */

    /* Set pause state */
    ES_OUT_SET_PAUSE_STATE,                         /* arg1=bool b_source_paused, bool b_paused arg2=mtime_t   res=can fail */

    /* Set rate */
    ES_OUT_SET_RATE,                                /* arg1=int i_source_rate arg2=int i_rate                  res=can fail */

    /* Set a new time */
    ES_OUT_SET_TIME,                                /* arg1=mtime_t             res=can fail */

    /* Set next frame */
    ES_OUT_SET_FRAME_NEXT,                          /*                          res=can fail */

    /* Set position/time/length */
    ES_OUT_SET_TIMES,                               /* arg1=double f_position arg2=mtime_t i_time arg3=mtime_t i_length res=cannot fail */

    /* Set jitter */
    ES_OUT_SET_JITTER,                              /* arg1=mtime_t i_pts_delay arg2=int i_cr_average res=cannot fail */
};

static inline mtime_t es_out_GetWakeup( es_out_t *p_out )
{
    mtime_t i_wu;
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
static inline int es_out_SetDelay( es_out_t *p_out, int i_cat, mtime_t i_delay )
{
    return es_out_Control( p_out, ES_OUT_SET_DELAY, i_cat, i_delay );
}
static inline int es_out_SetRecordState( es_out_t *p_out, bool b_record )
{
    return es_out_Control( p_out, ES_OUT_SET_RECORD_STATE, b_record );
}
static inline int es_out_SetPauseState( es_out_t *p_out, bool b_source_paused, bool b_paused, mtime_t i_date )
{
    return es_out_Control( p_out, ES_OUT_SET_PAUSE_STATE, b_source_paused, b_paused, i_date );
}
static inline int es_out_SetRate( es_out_t *p_out, int i_source_rate, int i_rate )
{
    return es_out_Control( p_out, ES_OUT_SET_RATE, i_source_rate, i_rate );
}
static inline int es_out_SetTime( es_out_t *p_out, mtime_t i_date )
{
    return es_out_Control( p_out, ES_OUT_SET_TIME, i_date );
}
static inline int es_out_SetFrameNext( es_out_t *p_out )
{
    return es_out_Control( p_out, ES_OUT_SET_FRAME_NEXT );
}
static inline void es_out_SetTimes( es_out_t *p_out, double f_position, mtime_t i_time, mtime_t i_length )
{
    int i_ret = es_out_Control( p_out, ES_OUT_SET_TIMES, f_position, i_time, i_length );
    assert( !i_ret );
}
static inline void es_out_SetJitter( es_out_t *p_out, mtime_t i_pts_delay, int i_cr_average )
{
    int i_ret = es_out_Control( p_out, ES_OUT_SET_JITTER, i_pts_delay, i_cr_average );
    assert( !i_ret );
}

es_out_t  *input_EsOutNew( input_thread_t *, int i_rate );

#endif
