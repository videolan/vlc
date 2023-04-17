/*****************************************************************************
 * vlc_es_out.h: es_out (demuxer output) descriptor, queries and methods
 *****************************************************************************
 * Copyright (C) 1999-2004 VLC authors and VideoLAN
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

#ifndef VLC_ES_OUT_H
#define VLC_ES_OUT_H 1

#include <assert.h>

#include <vlc_es.h>

/**
 * \defgroup es_out ES output
 * \ingroup input
 * Elementary streams output
 * @{
 * \file
 * Elementary streams output interface
 */

enum es_out_query_e
{
    /* set or change the selected ES in its category (audio/video/spu) */
    ES_OUT_SET_ES,      /* arg1= es_out_id_t*                   */
    ES_OUT_UNSET_ES,    /* arg1= es_out_id_t* res=can fail      */
    ES_OUT_RESTART_ES,  /* arg1= es_out_id_t*                   */

    /* set 'default' tag on ES (copied across from container) */
    ES_OUT_SET_ES_DEFAULT, /* arg1= es_out_id_t*                */

    /* force (un)selection of the ES (bypass current mode)
       XXX: this will not change the state of any other ES ! (see SET_ES) */
    ES_OUT_SET_ES_STATE,/* arg1= es_out_id_t* arg2=bool   */
    ES_OUT_GET_ES_STATE,/* arg1= es_out_id_t* arg2=bool*  */

    /* sets es selection policy when in auto mode */
    ES_OUT_SET_ES_CAT_POLICY, /* arg1=es_format_category_e arg2=es_out_policy_e */

    /* */
    ES_OUT_SET_GROUP,   /* arg1= int                            */

    /* PCR handling, DTS/PTS will be automatically computed using those PCR
     * XXX: SET_PCR(_GROUP) are in charge of the pace control. They will wait
     * to slow down the demuxer so that it reads at the right speed.
     * XXX: if you want PREROLL just call ES_OUT_SET_NEXT_DISPLAY_TIME and send
     * as you would normally do.
     */
    ES_OUT_SET_PCR,             /* arg1=vlc_tick_t i_pcr(microsecond!) (using default group 0)*/
    ES_OUT_SET_GROUP_PCR,       /* arg1= int i_group, arg2=vlc_tick_t i_pcr(microsecond!)*/
    ES_OUT_RESET_PCR,           /* no arg */

    /* This will update the fmt, drain and restart the decoder (if any).
     * The new fmt must have the same i_cat and i_id. */
    ES_OUT_SET_ES_FMT,         /* arg1= es_out_id_t* arg2=es_format_t* res=can fail */

    /* Allow preroll of data (data with dts/pts < i_pts for all ES will be decoded but not displayed */
    ES_OUT_SET_NEXT_DISPLAY_TIME,       /* arg1=int64_t i_pts(microsecond) */
    /* Set meta data for group (dynamic) (The vlc_meta_t is not modified nor released) */
    ES_OUT_SET_GROUP_META,  /* arg1=int i_group arg2=const vlc_meta_t */
    /* Set epg for group (dynamic) (The vlc_epg_t is not modified nor released) */
    ES_OUT_SET_GROUP_EPG,       /* arg1=int i_group arg2=const vlc_epg_t * */
    ES_OUT_SET_GROUP_EPG_EVENT, /* arg1=int i_group arg2=const vlc_epg_event_t * */
    ES_OUT_SET_EPG_TIME,        /* arg1=int int64_t */

    /* */
    ES_OUT_DEL_GROUP,       /* arg1=int i_group */

    /* Set scrambled state for one es */
    ES_OUT_SET_ES_SCRAMBLED_STATE,  /* arg1=int i_group arg2=es_out_id_t* */

    /* Stop any buffering being done, and ask if es_out has no more data to
     * play.
     * It will not block and so MUST be used carrefully. The only good reason
     * is for interactive playback (like for DVD menu).
     * XXX You SHALL call ES_OUT_RESET_PCR before any other es_out_Control/Send calls. */
    ES_OUT_GET_EMPTY,       /* arg1=bool*   res=cannot fail */

    /* Set global meta data (The vlc_meta_t is not modified nor released) */
    ES_OUT_SET_META, /* arg1=const vlc_meta_t * */

    /* PCR system clock manipulation for external clock synchronization */
    ES_OUT_GET_PCR_SYSTEM, /* arg1=vlc_tick_t *, arg2=vlc_tick_t * res=can fail */
    ES_OUT_MODIFY_PCR_SYSTEM, /* arg1=int is_absolute, arg2=vlc_tick_t, res=can fail */

    ES_OUT_POST_SUBNODE, /* arg1=input_item_node_t *, res=can fail */

    ES_OUT_VOUT_SET_MOUSE_EVENT, /* arg1= es_out_id_t* (video es),
                                    arg2=vlc_mouse_event, arg3=void *(user_data),
                                    res=can fail */

    ES_OUT_VOUT_ADD_OVERLAY, /* arg1= es_out_id_t* (video es),
                              * arg2= subpicture_t *,
                              * arg3= size_t * (channel id), res= can fail */
    ES_OUT_VOUT_DEL_OVERLAY, /* arg1= es_out_id_t* (video es),
                              * arg2= size_t (channel id), res= can fail */

    ES_OUT_SPU_SET_HIGHLIGHT, /* arg1= es_out_id_t* (spu es),
                                 arg2= const vlc_spu_highlight_t *, res=can fail  */

    /* First value usable for private control */
    ES_OUT_PRIVATE_START = 0x10000,
};

enum es_out_policy_e
{
    ES_OUT_ES_POLICY_EXCLUSIVE = 0,/* Enforces single ES selection only */
    ES_OUT_ES_POLICY_SIMULTANEOUS, /* Allows multiple ES per cat */
    /* Exclusive by default, and simultaneous if specifically requested more
     * than one track at once */
    ES_OUT_ES_POLICY_AUTO,
};

struct es_out_callbacks
{
    es_out_id_t *(*add)(es_out_t *, input_source_t *in, const es_format_t *);
    int          (*send)(es_out_t *, es_out_id_t *, block_t *);
    void         (*del)(es_out_t *, es_out_id_t *);
    int          (*control)(es_out_t *, input_source_t *in, int query, va_list);
    void         (*destroy)(es_out_t *);
    /**
     * Private control callback, must be NULL for es_out created from modules.
     */
    int          (*priv_control)(es_out_t *, int query, va_list);
};

struct es_out_t
{
    const struct es_out_callbacks *cbs;
};

VLC_USED
static inline es_out_id_t * es_out_Add( es_out_t *out, const es_format_t *fmt )
{
    return out->cbs->add( out, NULL, fmt );
}

static inline void es_out_Del( es_out_t *out, es_out_id_t *id )
{
    out->cbs->del( out, id );
}

static inline int es_out_Send( es_out_t *out, es_out_id_t *id,
                               block_t *p_block )
{
    return out->cbs->send( out, id, p_block );
}

static inline int es_out_vaControl( es_out_t *out, int i_query, va_list args )
{
    return out->cbs->control( out, NULL, i_query, args );
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

static inline void es_out_Delete( es_out_t *p_out )
{
    p_out->cbs->destroy( p_out );
}

/* PCR handling, DTS/PTS will be automatically computed using those PCR
 * XXX: SET_PCR(_GROUP) are in charge of the pace control. They will wait
 * to slow down the demuxer so that it reads at the right speed.
 * XXX: if you want PREROLL just call ES_OUT_SET_NEXT_DISPLAY_TIME and send
 * as you would normally do.
 */
static inline int es_out_SetPCR( es_out_t *out, vlc_tick_t pcr )
{
    return es_out_Control( out, ES_OUT_SET_PCR, pcr );
}

/* Reset the PCR */
VLC_USED static inline int es_out_ResetPCR( es_out_t *out )
{
    return es_out_Control( out, ES_OUT_RESET_PCR );
}

/* Set or change the selected ES in its category (audio/video/spu) */
VLC_USED static inline int es_out_SetES( es_out_t *out, es_out_id_t *id )
{
    return es_out_Control( out, ES_OUT_SET_ES, id );
}

/* Unset selected ES */
VLC_USED static inline int es_out_UnsetES( es_out_t *out, es_out_id_t *id )
{
    return es_out_Control( out, ES_OUT_UNSET_ES, id );
}

/* Restart the selected ES */
VLC_USED static inline int es_out_RestartES( es_out_t *out, es_out_id_t *id )
{
    return es_out_Control( out, ES_OUT_RESTART_ES, id );
}

/* Set or change the default ES */
VLC_USED static inline int es_out_SetESDefault( es_out_t *out, es_out_id_t *id )
{
    return es_out_Control( out, ES_OUT_SET_ES_DEFAULT, id );
}

/* Force (un)selection of the ES (bypass current mode) */
VLC_USED static inline int es_out_SetESState( es_out_t *out, es_out_id_t *id, bool state )
{
    return es_out_Control( out, ES_OUT_SET_ES_STATE, id, state );
}

/* This will update the fmt, drain and restart the decoder (if any).
 * The new fmt must have the same i_cat and i_id. */
VLC_USED static inline int es_out_SetESFmt( es_out_t *out, es_format_t *fmt )
{
    return es_out_Control( out, ES_OUT_SET_ES_FMT, fmt );
}

/* Sets ES selection policy when in auto mode */
VLC_USED static inline int es_out_SetESCatPolicy( es_out_t *out, enum es_format_category_e cat, enum es_out_policy_e policy )
{
    return es_out_Control( out, ES_OUT_SET_ES_CAT_POLICY, cat, policy );
}

/* Get the selected state of the ES */
VLC_USED static inline bool es_out_GetESState( es_out_t *out, es_out_id_t *id)
{
    bool state = false;
    
    es_out_Control( out, ES_OUT_GET_ES_STATE, id, &state );
    return state;
}

/* Allow preroll of data (data with dts/pts < i_pts for all ES will be decoded but not displayed) */
VLC_USED static inline int es_out_SetNextDisplayTime( es_out_t *out, vlc_tick_t ndt )
{
    return es_out_Control( out, ES_OUT_SET_NEXT_DISPLAY_TIME, ndt );
}

/* Set global meta data (The vlc_meta_t is not modified nor released) */
VLC_USED static inline int es_out_SetMeta( es_out_t *out, const vlc_meta_t *meta )
{
    return es_out_Control( out, ES_OUT_SET_META, meta );
}
#define es_out_ControlSetMeta es_out_SetMeta

/* Get the PCR system clock manipulation for external clock synchronization */
VLC_USED static inline int es_out_GetPcrSystem( es_out_t *out, vlc_tick_t *pi_system, vlc_tick_t *pi_delay )
{
    return es_out_Control( out, ES_OUT_GET_PCR_SYSTEM, pi_system, pi_delay );
}
#define es_out_ControlGetPcrSystem es_out_GetPcrSystem

/* Modify the PCR system clock manipulation for external clock synchronization */
VLC_USED static inline int es_out_ModifyPcrSystem( es_out_t *out, bool b_absolute, vlc_tick_t i_system )
{
    return es_out_Control( out, ES_OUT_MODIFY_PCR_SYSTEM, b_absolute, i_system );
}
#define es_out_ControlModifyPcrSystem es_out_ModifyPcrSystem

/**
 * @}
 */

#endif
