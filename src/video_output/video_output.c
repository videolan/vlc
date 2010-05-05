/*****************************************************************************
 * video_output.c : video output thread
 *
 * This module describes the programming interface for video output threads.
 * It includes functions allowing to open a new thread, send pictures to a
 * thread, and destroy a previously oppened video output thread.
 *****************************************************************************
 * Copyright (C) 2000-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <stdlib.h>                                                /* free() */
#include <string.h>
#include <assert.h>

#include <vlc_vout.h>

#include <vlc_filter.h>
#include <vlc_osd.h>

#include <libvlc.h>
#include <vlc_input.h>
#include "vout_internal.h"
#include "interlacing.h"
#include "postprocessing.h"
#include "display.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void     *Thread(void *);
static void     vout_Destructor(vlc_object_t *);

/* Object variables callbacks */
static int FilterCallback( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int VideoFilter2Callback( vlc_object_t *, char const *,
                                 vlc_value_t, vlc_value_t, void * );

/* */
static void PrintVideoFormat(vout_thread_t *, const char *, const video_format_t *);

/* Maximum delay between 2 displayed pictures.
 * XXX it is needed for now but should be removed in the long term.
 */
#define VOUT_REDISPLAY_DELAY (INT64_C(80000))

/**
 * Late pictures having a delay higher than this value are thrashed.
 */
#define VOUT_DISPLAY_LATE_THRESHOLD (INT64_C(20000))

/* Better be in advance when awakening than late... */
#define VOUT_MWAIT_TOLERANCE (INT64_C(1000))

/*****************************************************************************
 * Video Filter2 functions
 *****************************************************************************/
static picture_t *video_new_buffer_filter( filter_t *p_filter )
{
    vout_thread_t *p_vout = (vout_thread_t*)p_filter->p_owner;
    return picture_pool_Get(p_vout->p->private_pool);
}

static void video_del_buffer_filter( filter_t *p_filter, picture_t *p_pic )
{
    VLC_UNUSED(p_filter);
    picture_Release(p_pic);
}

static int video_filter_buffer_allocation_init( filter_t *p_filter, void *p_data )
{
    p_filter->pf_video_buffer_new = video_new_buffer_filter;
    p_filter->pf_video_buffer_del = video_del_buffer_filter;
    p_filter->p_owner = p_data; /* p_vout */
    return VLC_SUCCESS;
}

#undef vout_Request
/*****************************************************************************
 * vout_Request: find a video output thread, create one, or destroy one.
 *****************************************************************************
 * This function looks for a video output thread matching the current
 * properties. If not found, it spawns a new one.
 *****************************************************************************/
vout_thread_t *vout_Request( vlc_object_t *p_this, vout_thread_t *p_vout,
                             video_format_t *p_fmt )
{
    if( !p_fmt )
    {
        /* Video output is no longer used.
         * TODO: support for reusing video outputs with proper _thread-safe_
         * reference handling. */
        if( p_vout )
            vout_CloseAndRelease( p_vout );
        return NULL;
    }

    /* If a video output was provided, lock it, otherwise look for one. */
    if( p_vout )
    {
        vlc_object_hold( p_vout );
    }

    /* TODO: find a suitable unused video output */

    /* If we now have a video output, check it has the right properties */
    if( p_vout )
    {
        vlc_mutex_lock( &p_vout->p->change_lock );

        /* We don't directly check for the "vout-filter" variable for obvious
         * performance reasons. */
        if( p_vout->p->b_filter_change )
        {
            char *psz_filter_chain = var_GetString( p_vout, "vout-filter" );

            if( psz_filter_chain && !*psz_filter_chain )
            {
                free( psz_filter_chain );
                psz_filter_chain = NULL;
            }
            if( p_vout->p->psz_filter_chain && !*p_vout->p->psz_filter_chain )
            {
                free( p_vout->p->psz_filter_chain );
                p_vout->p->psz_filter_chain = NULL;
            }

            if( !psz_filter_chain && !p_vout->p->psz_filter_chain )
            {
                p_vout->p->b_filter_change = false;
            }

            free( psz_filter_chain );
        }

#warning "FIXME: Check RGB masks in vout_Request"
        /* FIXME: check RGB masks */
        if( p_vout->p->fmt_render.i_chroma != vlc_fourcc_GetCodec( VIDEO_ES, p_fmt->i_chroma ) ||
            p_vout->p->fmt_render.i_width != p_fmt->i_width ||
            p_vout->p->fmt_render.i_height != p_fmt->i_height ||
            p_vout->p->b_filter_change )
        {
            vlc_mutex_unlock( &p_vout->p->change_lock );

            /* We are not interested in this format, close this vout */
            vout_CloseAndRelease( p_vout );
            vlc_object_release( p_vout );
            p_vout = NULL;
        }
        else
        {
            /* This video output is cool! Hijack it. */
            /* Correct aspect ratio on change
             * FIXME factorize this code with other aspect ration related code */
            unsigned int i_sar_num;
            unsigned int i_sar_den;
            vlc_ureduce( &i_sar_num, &i_sar_den,
                         p_fmt->i_sar_num, p_fmt->i_sar_den, 50000 );
#if 0
            /* What's that, it does not seems to be used correcly everywhere */
            if( p_vout->i_par_num > 0 && p_vout->i_par_den > 0 )
            {
                i_sar_num *= p_vout->i_par_den;
                i_sar_den *= p_vout->i_par_num;
            }
#endif

            if( i_sar_num > 0 && i_sar_den > 0 &&
                ( i_sar_num != p_vout->p->fmt_render.i_sar_num ||
                  i_sar_den != p_vout->p->fmt_render.i_sar_den ) )
            {
                p_vout->p->fmt_in.i_sar_num = i_sar_num;
                p_vout->p->fmt_in.i_sar_den = i_sar_den;

                p_vout->p->fmt_render.i_sar_num = i_sar_num;
                p_vout->p->fmt_render.i_sar_den = i_sar_den;
                p_vout->p->i_changes |= VOUT_ASPECT_CHANGE;
            }
            vlc_mutex_unlock( &p_vout->p->change_lock );

            vlc_object_release( p_vout );
        }

        if( p_vout )
        {
            msg_Dbg( p_this, "reusing provided vout" );

            spu_Attach( p_vout->p->p_spu, VLC_OBJECT(p_vout), false );
            vlc_object_detach( p_vout );

            vlc_object_attach( p_vout, p_this );
            spu_Attach( p_vout->p->p_spu, VLC_OBJECT(p_vout), true );
        }
    }

    if( !p_vout )
    {
        msg_Dbg( p_this, "no usable vout present, spawning one" );

        p_vout = vout_Create( p_this, p_fmt );
    }

    return p_vout;
}

/*****************************************************************************
 * vout_Create: creates a new video output thread
 *****************************************************************************
 * This function creates a new video output thread, and returns a pointer
 * to its description. On error, it returns NULL.
 *****************************************************************************/
vout_thread_t * (vout_Create)( vlc_object_t *p_parent, video_format_t *p_fmt )
{
    vout_thread_t  *p_vout;                            /* thread descriptor */
    vlc_value_t     text;


    config_chain_t *p_cfg;
    char *psz_parser;
    char *psz_name;

    if( p_fmt->i_width <= 0 || p_fmt->i_height <= 0 )
        return NULL;
    const vlc_fourcc_t i_chroma = vlc_fourcc_GetCodec( VIDEO_ES, p_fmt->i_chroma );

    vlc_ureduce( &p_fmt->i_sar_num, &p_fmt->i_sar_den,
                 p_fmt->i_sar_num, p_fmt->i_sar_den, 50000 );
    if( p_fmt->i_sar_num <= 0 || p_fmt->i_sar_den <= 0 )
        return NULL;

    /* Allocate descriptor */
    static const char typename[] = "video output";
    p_vout = vlc_custom_create( p_parent, sizeof( *p_vout ), VLC_OBJECT_VOUT,
                                typename );
    if( p_vout == NULL )
        return NULL;

    /* */
    p_vout->p = calloc( 1, sizeof(*p_vout->p) );
    if( !p_vout->p )
    {
        vlc_object_release( p_vout );
        return NULL;
    }

    /* */
    p_vout->p->fmt_render        = *p_fmt;   /* FIXME palette */
    p_vout->p->fmt_in            = *p_fmt;   /* FIXME palette */

    p_vout->p->fmt_render.i_chroma = 
    p_vout->p->fmt_in.i_chroma     = i_chroma;
    video_format_FixRgb( &p_vout->p->fmt_render );
    video_format_FixRgb( &p_vout->p->fmt_in );

    /* Initialize misc stuff */
    vout_control_Init( &p_vout->p->control );
    p_vout->p->i_changes    = 0;
    vout_chrono_Init( &p_vout->p->render, 5, 10000 ); /* Arbitrary initial time */
    vout_statistic_Init( &p_vout->p->statistic );
    p_vout->p->b_filter_change = 0;
    p_vout->p->i_par_num =
    p_vout->p->i_par_den = 1;
    p_vout->p->displayed.date = VLC_TS_INVALID;
    p_vout->p->displayed.decoded = NULL;
    p_vout->p->displayed.timestamp = VLC_TS_INVALID;
    p_vout->p->displayed.qtype = QTYPE_NONE;
    p_vout->p->displayed.is_interlaced = false;

    p_vout->p->step.last         = VLC_TS_INVALID;
    p_vout->p->step.timestamp    = VLC_TS_INVALID;

    p_vout->p->pause.is_on  = false;
    p_vout->p->pause.date   = VLC_TS_INVALID;

    p_vout->p->decoder_fifo = picture_fifo_New();
    p_vout->p->decoder_pool = NULL;

    vlc_mouse_Init( &p_vout->p->mouse );

    vout_snapshot_Init( &p_vout->p->snapshot );

    p_vout->p->p_vf2_chain = filter_chain_New( p_vout, "video filter2",
        false, video_filter_buffer_allocation_init, NULL, p_vout );

    /* Initialize locks */
    vlc_mutex_init( &p_vout->p->picture_lock );
    vlc_mutex_init( &p_vout->p->change_lock );
    vlc_mutex_init( &p_vout->p->vfilter_lock );

    /* Mouse coordinates */
    var_Create( p_vout, "mouse-button-down", VLC_VAR_INTEGER );
    var_Create( p_vout, "mouse-moved", VLC_VAR_COORDS );
    var_Create( p_vout, "mouse-clicked", VLC_VAR_COORDS );
    /* Mouse object (area of interest in a video filter) */
    var_Create( p_vout, "mouse-object", VLC_VAR_BOOL );

    /* Attach the new object now so we can use var inheritance below */
    vlc_object_attach( p_vout, p_parent );

    /* Initialize subpicture unit */
    p_vout->p->p_spu = spu_Create( p_vout );

    /* */
    spu_Init( p_vout->p->p_spu );

    spu_Attach( p_vout->p->p_spu, VLC_OBJECT(p_vout), true );

    p_vout->p->is_late_dropped = var_InheritBool( p_vout, "drop-late-frames" );

    /* Take care of some "interface/control" related initialisations */
    vout_IntfInit( p_vout );

    /* Look for the default filter configuration */
    p_vout->p->psz_filter_chain =
        var_CreateGetStringCommand( p_vout, "vout-filter" );

    /* Apply video filter2 objects on the first vout */
    var_Create( p_vout, "video-filter",
                VLC_VAR_STRING | VLC_VAR_DOINHERIT | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_vout, "video-filter", VideoFilter2Callback, NULL );
    var_TriggerCallback( p_vout, "video-filter" );

    /* Choose the video output module */
    if( !p_vout->p->psz_filter_chain || !*p_vout->p->psz_filter_chain )
    {
        psz_parser = NULL;
    }
    else
    {
        psz_parser = strdup( p_vout->p->psz_filter_chain );
        p_vout->p->title.show = false;
    }

    /* Create the vout thread */
    char *psz_tmp = config_ChainCreate( &psz_name, &p_cfg, psz_parser );
    free( psz_parser );
    free( psz_tmp );
    p_vout->p->p_cfg = p_cfg;

    /* Create a few object variables for interface interaction */
    var_Create( p_vout, "vout-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    text.psz_string = _("Filters");
    var_Change( p_vout, "vout-filter", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_vout, "vout-filter", FilterCallback, NULL );

    /* */
    vout_InitInterlacingSupport( p_vout, p_vout->p->displayed.is_interlaced );

    if( p_vout->p->psz_filter_chain && *p_vout->p->psz_filter_chain )
    {
        char *psz_tmp;
        if( asprintf( &psz_tmp, "%s,none", psz_name ) < 0 )
            psz_tmp = strdup( "" );
        free( psz_name );
        psz_name = psz_tmp;
    }
    p_vout->p->psz_module_name = psz_name;

    /* */
    vlc_object_set_destructor( p_vout, vout_Destructor );

    /* */
    vlc_cond_init( &p_vout->p->change_wait );
    if( vlc_clone( &p_vout->p->thread, Thread, p_vout,
                   VLC_THREAD_PRIORITY_OUTPUT ) )
    {
        spu_Attach( p_vout->p->p_spu, VLC_OBJECT(p_vout), false );
        spu_Destroy( p_vout->p->p_spu );
        p_vout->p->p_spu = NULL;
        vlc_object_release( p_vout );
        return NULL;
    }

    vlc_mutex_lock( &p_vout->p->change_lock );
    while( !p_vout->p->b_ready )
    {   /* We are (ab)using the same condition in opposite directions for
         * b_ready and b_done. This works because of the strict ordering. */
        assert( !p_vout->p->b_done );
        vlc_cond_wait( &p_vout->p->change_wait, &p_vout->p->change_lock );
    }
    vlc_mutex_unlock( &p_vout->p->change_lock );

    if( p_vout->p->b_error )
    {
        msg_Err( p_vout, "video output creation failed" );
        vout_CloseAndRelease( p_vout );
        return NULL;
    }

    return p_vout;
}

/*****************************************************************************
 * vout_Close: Close a vout created by vout_Create.
 *****************************************************************************
 * You HAVE to call it on vout created by vout_Create before vlc_object_release.
 * You should NEVER call it on vout not obtained through vout_Create
 * (like with vout_Request or vlc_object_find.)
 * You can use vout_CloseAndRelease() as a convenience method.
 *****************************************************************************/
void vout_Close( vout_thread_t *p_vout )
{
    assert( p_vout );

    vlc_mutex_lock( &p_vout->p->change_lock );
    p_vout->p->b_done = true;
    vlc_cond_signal( &p_vout->p->change_wait );
    vlc_mutex_unlock( &p_vout->p->change_lock );

    vout_snapshot_End( &p_vout->p->snapshot );

    vlc_join( p_vout->p->thread, NULL );
}

/* */
static void vout_Destructor( vlc_object_t * p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    /* Make sure the vout was stopped first */
    //assert( !p_vout->p_module );

    free( p_vout->p->psz_module_name );

    /* */
    if( p_vout->p->p_spu )
        spu_Destroy( p_vout->p->p_spu );

    vout_chrono_Clean( &p_vout->p->render );

    if( p_vout->p->decoder_fifo )
        picture_fifo_Delete( p_vout->p->decoder_fifo );
    assert( !p_vout->p->decoder_pool );

    /* Destroy the locks */
    vlc_cond_destroy( &p_vout->p->change_wait );
    vlc_mutex_destroy( &p_vout->p->picture_lock );
    vlc_mutex_destroy( &p_vout->p->change_lock );
    vlc_mutex_destroy( &p_vout->p->vfilter_lock );
    vout_control_Clean( &p_vout->p->control );

    /* */
    vout_statistic_Clean( &p_vout->p->statistic );

    /* */
    vout_snapshot_Clean( &p_vout->p->snapshot );

    /* */
    free( p_vout->p->psz_filter_chain );

    config_ChainDestroy( p_vout->p->p_cfg );

    free( p_vout->p );

}

/* */
void vout_ChangePause(vout_thread_t *vout, bool is_paused, mtime_t date)
{
    vout_control_cmd_t cmd;
    vout_control_cmd_Init(&cmd, VOUT_CONTROL_PAUSE);
    cmd.u.pause.is_on = is_paused;
    cmd.u.pause.date  = date;
    vout_control_Push(&vout->p->control, &cmd);

    vout_control_WaitEmpty(&vout->p->control);
}

void vout_GetResetStatistic( vout_thread_t *p_vout, int *pi_displayed, int *pi_lost )
{
    vout_statistic_GetReset( &p_vout->p->statistic,
                             pi_displayed, pi_lost );
}

void vout_Flush(vout_thread_t *vout, mtime_t date)
{
    vout_control_PushTime(&vout->p->control, VOUT_CONTROL_FLUSH, date);
    vout_control_WaitEmpty(&vout->p->control);
}

void vout_Reset(vout_thread_t *vout)
{
    vout_control_PushVoid(&vout->p->control, VOUT_CONTROL_RESET);
    vout_control_WaitEmpty(&vout->p->control);
}

bool vout_IsEmpty(vout_thread_t *vout)
{
    vlc_mutex_lock(&vout->p->picture_lock);

    picture_t *picture = picture_fifo_Peek(vout->p->decoder_fifo);
    if (picture)
        picture_Release(picture);

    vlc_mutex_unlock(&vout->p->picture_lock);

    return !picture;
}

void vout_FixLeaks( vout_thread_t *vout )
{
    vlc_mutex_lock(&vout->p->picture_lock);

    picture_t *picture = picture_fifo_Peek(vout->p->decoder_fifo);
    if (!picture) {
        picture = picture_pool_Get(vout->p->decoder_pool);
    }

    if (picture) {
        picture_Release(picture);
        /* Not all pictures has been displayed yet or some are
         * free */
        vlc_mutex_unlock(&vout->p->picture_lock);
        return;
    }

    /* There is no reason that no pictures are available, force one
     * from the pool, becarefull with it though */
    msg_Err(vout, "pictures leaked, trying to workaround");

    /* */
    picture_pool_NonEmpty(vout->p->decoder_pool, false);

    vlc_mutex_unlock(&vout->p->picture_lock);
}
void vout_NextPicture(vout_thread_t *vout, mtime_t *duration)
{
    vout_control_cmd_t cmd;
    vout_control_cmd_Init(&cmd, VOUT_CONTROL_STEP);
    cmd.u.time_ptr = duration;

    vout_control_Push(&vout->p->control, &cmd);
    vout_control_WaitEmpty(&vout->p->control);
}

void vout_DisplayTitle(vout_thread_t *vout, const char *title)
{
    assert(title);
    vout_control_PushString(&vout->p->control, VOUT_CONTROL_OSD_TITLE, title);
}

spu_t *vout_GetSpu( vout_thread_t *p_vout )
{
    return p_vout->p->p_spu;
}

/* vout_Control* are usable by anyone at anytime */
void vout_ControlChangeFullscreen(vout_thread_t *vout, bool fullscreen)
{
    vout_control_PushBool(&vout->p->control, VOUT_CONTROL_FULLSCREEN,
                          fullscreen);
}
void vout_ControlChangeOnTop(vout_thread_t *vout, bool is_on_top)
{
    vout_control_PushBool(&vout->p->control, VOUT_CONTROL_ON_TOP,
                          is_on_top);
}
void vout_ControlChangeDisplayFilled(vout_thread_t *vout, bool is_filled)
{
    vout_control_PushBool(&vout->p->control, VOUT_CONTROL_DISPLAY_FILLED,
                          is_filled);
}
void vout_ControlChangeZoom(vout_thread_t *vout, int num, int den)
{
    vout_control_PushPair(&vout->p->control, VOUT_CONTROL_ZOOM,
                          num, den);
}

/*****************************************************************************
 * InitThread: initialize video output thread
 *****************************************************************************
 * This function is called from Thread and performs the second step of the
 * initialization. It returns 0 on success. Note that the thread's flag are not
 * modified inside this function.
 * XXX You have to enter it with change_lock taken.
 *****************************************************************************/
static int ThreadInit(vout_thread_t *vout)
{
    /* Initialize output method, it allocates direct buffers for us */
    if (vout_InitWrapper(vout))
        return VLC_EGENERIC;
    assert(vout->p->decoder_pool);

    vout->p->displayed.decoded = NULL;

    /* print some usefull debug info about different vout formats
     */
    PrintVideoFormat(vout, "pic render", &vout->p->fmt_render);
    PrintVideoFormat(vout, "pic in",     &vout->p->fmt_in);
    PrintVideoFormat(vout, "pic out",    &vout->p->fmt_out);

    assert(vout->p->fmt_out.i_width  == vout->p->fmt_render.i_width &&
           vout->p->fmt_out.i_height == vout->p->fmt_render.i_height &&
           vout->p->fmt_out.i_chroma == vout->p->fmt_render.i_chroma);
    return VLC_SUCCESS;
}

static int ThreadDisplayPicture(vout_thread_t *vout,
                                bool now, mtime_t *deadline)
{
    int displayed_count = 0;
    int lost_count = 0;

    for (;;) {
        const mtime_t date = mdate();
        const bool is_paused = vout->p->pause.is_on;
        bool redisplay = is_paused && !now && vout->p->displayed.decoded;
        bool is_forced;

        /* FIXME/XXX we must redisplay the last decoded picture (because
         * of potential vout updated, or filters update or SPU update)
         * For now a high update period is needed but it coulmd be removed
         * if and only if:
         * - vout module emits events from theselves.
         * - *and* SPU is modified to emit an event or a deadline when needed.
         *
         * So it will be done latter.
         */
        if (!redisplay) {
            picture_t *peek = picture_fifo_Peek(vout->p->decoder_fifo);
            if (peek) {
                is_forced = peek->b_force || is_paused || now;
                *deadline = (is_forced ? date : peek->date) - vout_chrono_GetHigh(&vout->p->render);
                picture_Release(peek);
            } else {
                redisplay = true;
            }
        }
        if (redisplay) {
             /* FIXME a better way for this delay is needed */
            const mtime_t date_update = vout->p->displayed.date + VOUT_REDISPLAY_DELAY;
            if (date_update > date || !vout->p->displayed.decoded) {
                *deadline = vout->p->displayed.decoded ? date_update : VLC_TS_INVALID;
                break;
            }
            /* */
            is_forced = true;
            *deadline = date - vout_chrono_GetHigh(&vout->p->render);
        }
        if (*deadline > VOUT_MWAIT_TOLERANCE)
            *deadline -= VOUT_MWAIT_TOLERANCE;

        /* If we are too early and can wait, do it */
        if (date < *deadline && !now)
            break;

        picture_t *decoded;
        if (redisplay) {
            decoded = vout->p->displayed.decoded;
            vout->p->displayed.decoded = NULL;
        } else {
            decoded = picture_fifo_Pop(vout->p->decoder_fifo);
            assert(decoded);
            if (!is_forced && !vout->p->is_late_dropped) {
                const mtime_t predicted = date + vout_chrono_GetLow(&vout->p->render);
                const mtime_t late = predicted - decoded->date;
                if (late > 0) {
                    msg_Dbg(vout, "picture might be displayed late (missing %d ms)", (int)(late/1000));
                    if (late > VOUT_DISPLAY_LATE_THRESHOLD) {
                        msg_Warn(vout, "rejected picture because of render time");
                        /* TODO */
                        picture_Release(decoded);
                        lost_count++;
                        break;
                    }
                }
            }

            vout->p->displayed.is_interlaced = !decoded->b_progressive;
            vout->p->displayed.qtype         = decoded->i_qtype;
        }
        vout->p->displayed.timestamp = decoded->date;

        /* */
        if (vout->p->displayed.decoded)
            picture_Release(vout->p->displayed.decoded);
        picture_Hold(decoded);
        vout->p->displayed.decoded = decoded;

        /* */
        vout_chrono_Start(&vout->p->render);

        picture_t *filtered = NULL;
        if (decoded) {
            vlc_mutex_lock(&vout->p->vfilter_lock);
            filtered = filter_chain_VideoFilter(vout->p->p_vf2_chain, decoded);
            //assert(filtered == decoded); // TODO implement
            vlc_mutex_unlock(&vout->p->vfilter_lock);
            if (!filtered)
                continue;
        }

        /*
         * Check for subpictures to display
         */
        const bool do_snapshot = vout_snapshot_IsRequested(&vout->p->snapshot);
        mtime_t spu_render_time = is_forced ? mdate() : filtered->date;
        if (vout->p->pause.is_on)
            spu_render_time = vout->p->pause.date;
        else
            spu_render_time = filtered->date > 1 ? filtered->date : mdate();

        subpicture_t *subpic = spu_SortSubpictures(vout->p->p_spu,
                                                   spu_render_time,
                                                   do_snapshot);
        /*
         * Perform rendering
         *
         * We have to:
         * - be sure to end up with a direct buffer.
         * - blend subtitles, and in a fast access buffer
         */
        picture_t *direct = NULL;
        if (filtered &&
            (vout->p->decoder_pool != vout->p->display_pool || subpic)) {
            picture_t *render;
            if (vout->p->is_decoder_pool_slow)
                render = picture_NewFromFormat(&vout->p->fmt_out);
            else if (vout->p->decoder_pool != vout->p->display_pool)
                render = picture_pool_Get(vout->p->display_pool);
            else
                render = picture_pool_Get(vout->p->private_pool);

            if (render) {
                picture_Copy(render, filtered);

                spu_RenderSubpictures(vout->p->p_spu,
                                      render, &vout->p->fmt_out,
                                      subpic, &vout->p->fmt_in, spu_render_time);
            }
            if (vout->p->is_decoder_pool_slow) {
                direct = picture_pool_Get(vout->p->display_pool);
                if (direct)
                    picture_Copy(direct, render);
                picture_Release(render);

            } else {
                direct = render;
            }
            picture_Release(filtered);
            filtered = NULL;
        } else {
            direct = filtered;
        }

        /*
         * Take a snapshot if requested
         */
        if (direct && do_snapshot)
            vout_snapshot_Set(&vout->p->snapshot, &vout->p->fmt_out, direct);

        /* Render the direct buffer returned by vout_RenderPicture */
        if (direct) {
            vout_RenderWrapper(vout, direct);

            vout_chrono_Stop(&vout->p->render);
#if 0
            {
            static int i = 0;
            if (((i++)%10) == 0)
                msg_Info(vout, "render: avg %d ms var %d ms",
                         (int)(vout->p->render.avg/1000), (int)(vout->p->render.var/1000));
            }
#endif
        }

        /* Wait the real date (for rendering jitter) */
        if (!is_forced)
            mwait(decoded->date);

        /* Display the direct buffer returned by vout_RenderPicture */
        vout->p->displayed.date = mdate();
        if (direct)
            vout_DisplayWrapper(vout, direct);

        displayed_count++;
        break;
    }

    vout_statistic_Update(&vout->p->statistic, displayed_count, lost_count);
    if (displayed_count <= 0)
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static int ThreadManage(vout_thread_t *vout,
                        mtime_t *deadline,
                        vout_interlacing_support_t *interlacing,
                        vout_postprocessing_support_t *postprocessing)
{
    vlc_mutex_lock(&vout->p->picture_lock);

    *deadline = VLC_TS_INVALID;
    ThreadDisplayPicture(vout, false, deadline);

    const int  picture_qtype      = vout->p->displayed.qtype;
    const bool picture_interlaced = vout->p->displayed.is_interlaced;

    vlc_mutex_unlock(&vout->p->picture_lock);

    /* Post processing */
    vout_SetPostProcessingState(vout, postprocessing, picture_qtype);

    /* Deinterlacing */
    vout_SetInterlacingState(vout, interlacing, picture_interlaced);

    if (vout_ManageWrapper(vout)) {
        /* A fatal error occurred, and the thread must terminate
         * immediately, without displaying anything - setting b_error to 1
         * causes the immediate end of the main while() loop. */
        // FIXME pf_end
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static void ThreadDisplayOsdTitle(vout_thread_t *vout, const char *string)
{
    if( !var_InheritBool(vout, "osd"))
        return;
    if (!vout->p->title.show)
        return;

    vlc_assert_locked(&vout->p->change_lock);

    if (vout->p->title.timeout > 0)
        vout_ShowTextRelative(vout, SPU_DEFAULT_CHANNEL,
                              string, NULL,
                              vout->p->title.position,
                              30 + vout->p->fmt_in.i_width
                                 - vout->p->fmt_in.i_visible_width
                                 - vout->p->fmt_in.i_x_offset,
                              20 + vout->p->fmt_in.i_y_offset,
                              INT64_C(1000) * vout->p->title.timeout);
}

static void ThreadChangeFilters(vout_thread_t *vout, const char *filters)
{
    es_format_t fmt;
    es_format_Init(&fmt, VIDEO_ES, vout->p->fmt_render.i_chroma);
    fmt.video = vout->p->fmt_render;

    vlc_mutex_lock(&vout->p->vfilter_lock);

    filter_chain_Reset(vout->p->p_vf2_chain, &fmt, &fmt);
    if (filter_chain_AppendFromString(vout->p->p_vf2_chain,
                                      filters) < 0)
        msg_Err(vout, "Video filter chain creation failed");

    vlc_mutex_unlock(&vout->p->vfilter_lock);
}

static void ThreadChangePause(vout_thread_t *vout, bool is_paused, mtime_t date)
{
    vlc_assert_locked(&vout->p->change_lock);
    assert(!vout->p->pause.is_on || !is_paused);

    if (vout->p->pause.is_on) {
        const mtime_t duration = date - vout->p->pause.date;

        if (vout->p->step.timestamp > VLC_TS_INVALID)
            vout->p->step.timestamp += duration;
        if (vout->p->step.last > VLC_TS_INVALID)
            vout->p->step.last += duration;
        picture_fifo_OffsetDate(vout->p->decoder_fifo, duration);
        if (vout->p->displayed.decoded)
            vout->p->displayed.decoded->date += duration;

        spu_OffsetSubtitleDate(vout->p->p_spu, duration);
    } else {
        vout->p->step.timestamp = VLC_TS_INVALID;
        vout->p->step.last      = VLC_TS_INVALID;
    }
    vout->p->pause.is_on = is_paused;
    vout->p->pause.date  = date;
}

static void ThreadFlush(vout_thread_t *vout, bool below, mtime_t date)
{
    vout->p->step.timestamp = VLC_TS_INVALID;
    vout->p->step.last      = VLC_TS_INVALID;

    picture_t *last = vout->p->displayed.decoded;
    if (last) {
        if (( below && last->date <= date) ||
            (!below && last->date >= date)) {
            picture_Release(last);

            vout->p->displayed.decoded   = NULL;
            vout->p->displayed.date      = VLC_TS_INVALID;
            vout->p->displayed.timestamp = VLC_TS_INVALID;
        }
    }
    picture_fifo_Flush(vout->p->decoder_fifo, date, below);
}

static void ThreadReset(vout_thread_t *vout)
{
    ThreadFlush(vout, true, INT64_MAX);
    if (vout->p->decoder_pool)
        picture_pool_NonEmpty(vout->p->decoder_pool, true);
    vout->p->pause.is_on = false;
    vout->p->pause.date  = mdate();
}

static void ThreadStep(vout_thread_t *vout, mtime_t *duration)
{
    *duration = 0;

    if (vout->p->step.last <= VLC_TS_INVALID)
        vout->p->step.last = vout->p->displayed.timestamp;

    mtime_t dummy;
    if (ThreadDisplayPicture(vout, true, &dummy))
        return;

    vout->p->step.timestamp = vout->p->displayed.timestamp;

    if (vout->p->step.last > VLC_TS_INVALID &&
        vout->p->step.timestamp > vout->p->step.last) {
        *duration = vout->p->step.timestamp - vout->p->step.last;
        vout->p->step.last = vout->p->step.timestamp;
        /* TODO advance subpicture by the duration ... */
    }
}

static void ThreadChangeFullscreen(vout_thread_t *vout, bool fullscreen)
{
    /* FIXME not sure setting "fullscreen" is good ... */
    var_SetBool(vout, "fullscreen", fullscreen);
    vout_SetDisplayFullscreen(vout->p->display.vd, fullscreen);
}

static void ThreadChangeOnTop(vout_thread_t *vout, bool is_on_top)
{
    vout_SetWindowState(vout->p->display.vd,
                        is_on_top ? VOUT_WINDOW_STATE_ABOVE :
                                    VOUT_WINDOW_STATE_NORMAL);
}

static void ThreadChangeDisplayFilled(vout_thread_t *vout, bool is_filled)
{
    vout_SetDisplayFilled(vout->p->display.vd, is_filled);
}

static void ThreadChangeZoom(vout_thread_t *vout, int num, int den)
{
    if (num * 10 < den) {
        num = den;
        den *= 10;
    } else if (num > den * 10) {
        num = den * 10;
    }

    vout_SetDisplayZoom(vout->p->display.vd, num, den);
}

static void ThreadClean(vout_thread_t *vout)
{
    /* Destroy translation tables */
    if (!vout->p->b_error) {
        ThreadFlush(vout, true, INT64_MAX);
        vout_EndWrapper(vout);
    }
}

/*****************************************************************************
 * Thread: video output thread
 *****************************************************************************
 * Video output thread. This function does only returns when the thread is
 * terminated. It handles the pictures arriving in the video heap and the
 * display device events.
 *****************************************************************************/
static void *Thread(void *object)
{
    vout_thread_t *vout = object;
    bool          has_wrapper;

    /*
     * Initialize thread
     */
    has_wrapper = !vout_OpenWrapper(vout, vout->p->psz_module_name);

    vlc_mutex_lock(&vout->p->change_lock);

    if (has_wrapper)
        vout->p->b_error = ThreadInit(vout);
    else
        vout->p->b_error = true;

    /* signal the creation of the vout */
    vout->p->b_ready = true;
    vlc_cond_signal(&vout->p->change_wait);

    if (vout->p->b_error)
        goto exit_thread;

    /* */
    vout_interlacing_support_t interlacing = {
        .is_interlaced = false,
        .date = mdate(),
    };
    vout_postprocessing_support_t postprocessing = {
        .qtype = QTYPE_NONE,
    };

    /*
     * Main loop - it is not executed if an error occurred during
     * initialization
     */
    mtime_t deadline = VLC_TS_INVALID;
    while (!vout->p->b_done && !vout->p->b_error) {
        vout_control_cmd_t cmd;

        vlc_mutex_unlock(&vout->p->change_lock);
        /* FIXME remove thoses ugly timeouts
         */
        while (!vout_control_Pop(&vout->p->control, &cmd, deadline, 100000)) {
            /* TODO remove the lock when possible (ie when
             * vout->p->fmt_* are not protected by it anymore) */
            vlc_mutex_lock(&vout->p->change_lock);
            switch(cmd.type) {
            case VOUT_CONTROL_OSD_TITLE:
                ThreadDisplayOsdTitle(vout, cmd.u.string);
                break;
            case VOUT_CONTROL_CHANGE_FILTERS:
                ThreadChangeFilters(vout, cmd.u.string);
                break;
            case VOUT_CONTROL_PAUSE:
                ThreadChangePause(vout, cmd.u.pause.is_on, cmd.u.pause.date);
                break;
            case VOUT_CONTROL_FLUSH:
                ThreadFlush(vout, false, cmd.u.time);
                break;
            case VOUT_CONTROL_RESET:
                ThreadReset(vout);
                break;
            case VOUT_CONTROL_STEP:
                ThreadStep(vout, cmd.u.time_ptr);
                break;
            case VOUT_CONTROL_FULLSCREEN:
                ThreadChangeFullscreen(vout, cmd.u.boolean);
                break;
            case VOUT_CONTROL_ON_TOP:
                ThreadChangeOnTop(vout, cmd.u.boolean);
                break;
            case VOUT_CONTROL_DISPLAY_FILLED:
                ThreadChangeDisplayFilled(vout, cmd.u.boolean);
                break;
            case VOUT_CONTROL_ZOOM:
                ThreadChangeZoom(vout, cmd.u.pair.a, cmd.u.pair.b);
                break;
            default:
                break;
            }
            vlc_mutex_unlock(&vout->p->change_lock);
            vout_control_cmd_Clean(&cmd);
        }
        vlc_mutex_lock(&vout->p->change_lock);

        /* */
        if (ThreadManage(vout, &deadline,
                         &interlacing, &postprocessing)) {
            vout->p->b_error = true;
            break;
        }
    }

    /*
     * Error loop - wait until the thread destruction is requested
     *
     * XXX I wonder if we should periodically clean the decoder_fifo
     * or have a way to prevent it filling up.
     */
    while (vout->p->b_error && !vout->p->b_done)
        vlc_cond_wait(&vout->p->change_wait, &vout->p->change_lock);

    /* Clean thread */
    ThreadClean(vout);

exit_thread:
    /* Detach subpicture unit from both input and vout */
    spu_Attach(vout->p->p_spu, VLC_OBJECT(vout), false);
    vlc_object_detach(vout->p->p_spu);

    vlc_mutex_unlock(&vout->p->change_lock);

    if (has_wrapper)
        vout_CloseWrapper(vout);
    vout_control_Dead(&vout->p->control);

    /* Destroy the video filters2 */
    filter_chain_Delete(vout->p->p_vf2_chain);

    return NULL;
}

/*****************************************************************************
 * object variables callbacks: a bunch of object variables are used by the
 * interfaces to interact with the vout.
 *****************************************************************************/
static int FilterCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    input_thread_t *p_input;
    (void)psz_cmd; (void)oldval; (void)p_data;

    p_input = (input_thread_t *)vlc_object_find( p_this, VLC_OBJECT_INPUT,
                                                 FIND_PARENT );
    if (!p_input)
    {
        msg_Err( p_vout, "Input not found" );
        return VLC_EGENERIC;
    }

    /* Modify input as well because the vout might have to be restarted */
    var_Create( p_input, "vout-filter", VLC_VAR_STRING );
    var_SetString( p_input, "vout-filter", newval.psz_string );

    /* Now restart current video stream */
    input_Control( p_input, INPUT_RESTART_ES, -VIDEO_ES );
    vlc_object_release( p_input );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Video Filter2 stuff
 *****************************************************************************/
static int VideoFilter2Callback(vlc_object_t *object, char const *cmd,
                                vlc_value_t oldval, vlc_value_t newval,
                                void *data)
{
    vout_thread_t *vout = (vout_thread_t *)object;
    (void)cmd; (void)oldval; (void)data;

    vout_control_PushString(&vout->p->control, VOUT_CONTROL_CHANGE_FILTERS,
                            newval.psz_string);
    return VLC_SUCCESS;
}

/* */
static void PrintVideoFormat(vout_thread_t *vout,
                             const char *description,
                             const video_format_t *fmt)
{
    msg_Dbg(vout, "%s sz %ix%i, of (%i,%i), vsz %ix%i, 4cc %4.4s, sar %i:%i, msk r0x%x g0x%x b0x%x",
            description,
            fmt->i_width, fmt->i_height, fmt->i_x_offset, fmt->i_y_offset,
            fmt->i_visible_width, fmt->i_visible_height,
            (char*)&fmt->i_chroma,
            fmt->i_sar_num, fmt->i_sar_den,
            fmt->i_rmask, fmt->i_gmask, fmt->i_bmask);
}

