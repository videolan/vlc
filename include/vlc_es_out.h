/*****************************************************************************
 * vlc_es_out.h
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef _VLC_ES_OUT_H
#define _VLC_ES_OUT_H 1

/**
 * \defgroup es out Es Out
 * @{
 */

enum es_out_mode_e
{
    ES_OUT_MODE_NONE,   /* don't select anything */
    ES_OUT_MODE_ALL,    /* eg for stream output */
    ES_OUT_MODE_AUTO,   /* best audio/video or for input follow audio-track, sub-track */
    ES_OUT_MODE_PARTIAL /* select programs given after --programs */
};

enum es_out_query_e
{
    /* activate apply of mode */
    ES_OUT_SET_ACTIVE,  /* arg1= vlc_bool_t                     */
    /* see if mode is currently aplied or not */
    ES_OUT_GET_ACTIVE,  /* arg1= vlc_bool_t*                    */

    /* set/get mode */
    ES_OUT_SET_MODE,    /* arg1= int                            */
    ES_OUT_GET_MODE,    /* arg2= int*                           */

    /* set es selected for the es category(audio/video/spu) */
    ES_OUT_SET_ES,      /* arg1= es_out_id_t*                   */

    /* force selection/unselection of the ES (bypass current mode)*/
    ES_OUT_SET_ES_STATE,/* arg1= es_out_id_t* arg2=vlc_bool_t   */
    ES_OUT_GET_ES_STATE,/* arg1= es_out_id_t* arg2=vlc_bool_t*  */

    /* */
    ES_OUT_SET_GROUP,   /* arg1= int                            */
    ES_OUT_GET_GROUP,   /* arg1= int*                           */

    /* PCR handling, dts/pts will be automatically computed using thoses PCR
     * XXX: SET_PCR(_GROUP) is in charge of the pace control. They will wait to slow
     * down the demuxer to read at the right speed.
     * XXX: if you want PREROLL just call RESET_PCR and ES_OUT_SET_NEXT_DISPLAY_TIME and send
     * data to the decoder *without* calling SET_PCR until preroll is finished.
     */
    ES_OUT_SET_PCR,             /* arg1=int64_t i_pcr(microsecond!) (using default group 0)*/
    ES_OUT_SET_GROUP_PCR,       /* arg1= int i_group, arg2=int64_t i_pcr(microsecond!)*/
    ES_OUT_RESET_PCR,           /* no arg */

    /* Timestamp handling, convert an input timestamp to a global clock one.
     * (shouldn't have to be used by input plugins directly) */
    ES_OUT_GET_TS,             /* arg1=int64_t i_ts(microsecond!) (using default group 0), arg2=int64_t* converted i_ts */

    /* Try not to use this one as it is a bit hacky */
    ES_OUT_SET_FMT,     /* arg1= es_out_id_t* arg2=es_format_t* */

    /* Allow preroll of data (data with dts/pts < i_pts for one ES will be decoded but not displayed */
    ES_OUT_SET_NEXT_DISPLAY_TIME,   /* arg1=es_out_id_t* arg2=int64_t i_pts(microsecond) */
    /* Set meta data for group (dynamic) */
    ES_OUT_SET_GROUP_META,  /* arg1=int i_group arg2=vlc_meta_t */
};

struct es_out_t
{
    es_out_id_t *(*pf_add)    ( es_out_t *, es_format_t * );
    int          (*pf_send)   ( es_out_t *, es_out_id_t *, block_t * );
    void         (*pf_del)    ( es_out_t *, es_out_id_t * );
    int          (*pf_control)( es_out_t *, int i_query, va_list );

    es_out_sys_t    *p_sys;
};

static inline es_out_id_t * es_out_Add( es_out_t *out, es_format_t *fmt )
{
    return out->pf_add( out, fmt );
}
static inline void es_out_Del( es_out_t *out, es_out_id_t *id )
{
    out->pf_del( out, id );
}
static inline int es_out_Send( es_out_t *out, es_out_id_t *id,
                               block_t *p_block )
{
    return out->pf_send( out, id, p_block );
}

static inline int es_out_vaControl( es_out_t *out, int i_query, va_list args )
{
    return out->pf_control( out, i_query, args );
}
static inline int es_out_Control( es_out_t *out, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = es_out_vaControl( out, i_query, args );
    va_end( args );
    return i_result;
}

/**
 * @}
 */

#endif
