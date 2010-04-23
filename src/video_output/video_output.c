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
#include "vout_pictures.h"
#include "vout_internal.h"

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
static void PostProcessEnable( vout_thread_t * );
static void PostProcessDisable( vout_thread_t * );
static void PostProcessSetFilterQuality( vout_thread_t *p_vout );
static int  PostProcessCallback( vlc_object_t *, char const *,
                                 vlc_value_t, vlc_value_t, void * );
/* */
static void DeinterlaceEnable( vout_thread_t * );
static void DeinterlaceNeeded( vout_thread_t *, bool );

/* Display media title in OSD */
static void DisplayTitleOnOSD( vout_thread_t *p_vout );

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
        if( p_vout->fmt_render.i_chroma != vlc_fourcc_GetCodec( VIDEO_ES, p_fmt->i_chroma ) ||
            p_vout->fmt_render.i_width != p_fmt->i_width ||
            p_vout->fmt_render.i_height != p_fmt->i_height ||
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
                ( i_sar_num != p_vout->fmt_render.i_sar_num ||
                  i_sar_den != p_vout->fmt_render.i_sar_den ) )
            {
                p_vout->fmt_in.i_sar_num = i_sar_num;
                p_vout->fmt_in.i_sar_den = i_sar_den;

                p_vout->fmt_render.i_sar_num = i_sar_num;
                p_vout->fmt_render.i_sar_den = i_sar_den;
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
    p_vout->fmt_render        = *p_fmt;   /* FIXME palette */
    p_vout->fmt_in            = *p_fmt;   /* FIXME palette */

    p_vout->fmt_render.i_chroma = 
    p_vout->fmt_in.i_chroma     = i_chroma;
    video_format_FixRgb( &p_vout->fmt_render );
    video_format_FixRgb( &p_vout->fmt_in );

    /* Initialize misc stuff */
    p_vout->p->i_changes    = 0;
    p_vout->p->b_fullscreen = 0;
    vout_chrono_Init( &p_vout->p->render, 5, 10000 ); /* Arbitrary initial time */
    vout_statistic_Init( &p_vout->p->statistic );
    p_vout->p->b_filter_change = 0;
    p_vout->p->i_par_num =
    p_vout->p->i_par_den = 1;
    p_vout->p->is_late_dropped = var_InheritBool( p_vout, "drop-late-frames" );
    p_vout->p->b_picture_empty = false;
    p_vout->p->displayed.date = VLC_TS_INVALID;
    p_vout->p->displayed.decoded = NULL;
    p_vout->p->displayed.timestamp = VLC_TS_INVALID;
    p_vout->p->displayed.qtype = QTYPE_NONE;
    p_vout->p->displayed.is_interlaced = false;

    p_vout->p->step.is_requested = false;
    p_vout->p->step.last         = VLC_TS_INVALID;
    p_vout->p->step.timestamp    = VLC_TS_INVALID;

    p_vout->p->pause.is_on  = false;
    p_vout->p->pause.date   = VLC_TS_INVALID;

    p_vout->p->decoder_fifo = picture_fifo_New();
    p_vout->p->decoder_pool = NULL;

    vlc_mouse_Init( &p_vout->p->mouse );

    vout_snapshot_Init( &p_vout->p->snapshot );

    /* Initialize locks */
    vlc_mutex_init( &p_vout->p->picture_lock );
    vlc_cond_init( &p_vout->p->picture_wait );
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

    /* Take care of some "interface/control" related initialisations */
    vout_IntfInit( p_vout );

    /* If the parent is not a VOUT object, that means we are at the start of
     * the video output pipe */
    if( vlc_internals( p_parent )->i_object_type != VLC_OBJECT_VOUT )
    {
        /* Look for the default filter configuration */
        p_vout->p->psz_filter_chain =
            var_CreateGetStringCommand( p_vout, "vout-filter" );

        /* Apply video filter2 objects on the first vout */
        p_vout->p->psz_vf2 =
            var_CreateGetStringCommand( p_vout, "video-filter" );

        p_vout->p->b_first_vout = true;
    }
    else
    {
        /* continue the parent's filter chain */
        char *psz_tmp;

        /* Ugly hack to jump to our configuration chain */
        p_vout->p->psz_filter_chain
            = ((vout_thread_t *)p_parent)->p->psz_filter_chain;
        p_vout->p->psz_filter_chain
            = config_ChainCreate( &psz_tmp, &p_cfg, p_vout->p->psz_filter_chain );
        config_ChainDestroy( p_cfg );
        free( psz_tmp );

        /* Create a video filter2 var ... but don't inherit values */
        var_Create( p_vout, "video-filter",
                    VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
        p_vout->p->psz_vf2 = var_GetString( p_vout, "video-filter" );

        /* */
        p_vout->p->b_first_vout = false;
    }

    var_AddCallback( p_vout, "video-filter", VideoFilter2Callback, NULL );
    p_vout->p->p_vf2_chain = filter_chain_New( p_vout, "video filter2",
        false, video_filter_buffer_allocation_init, NULL, p_vout );

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
    DeinterlaceEnable( p_vout );

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
    vlc_cond_destroy( &p_vout->p->picture_wait );
    vlc_mutex_destroy( &p_vout->p->picture_lock );
    vlc_mutex_destroy( &p_vout->p->change_lock );
    vlc_mutex_destroy( &p_vout->p->vfilter_lock );

    /* */
    vout_statistic_Clean( &p_vout->p->statistic );

    /* */
    vout_snapshot_Clean( &p_vout->p->snapshot );

    /* */
    free( p_vout->p->psz_filter_chain );
    free( p_vout->p->title.value );

    config_ChainDestroy( p_vout->p->p_cfg );

    free( p_vout->p );

}

/* */
void vout_ChangePause( vout_thread_t *p_vout, bool b_paused, mtime_t i_date )
{
    vlc_mutex_lock( &p_vout->p->change_lock );

    assert( !p_vout->p->pause.is_on || !b_paused );

    vlc_mutex_lock( &p_vout->p->picture_lock );

    if( p_vout->p->pause.is_on )
    {
        const mtime_t i_duration = i_date - p_vout->p->pause.date;

        if (p_vout->p->step.timestamp > VLC_TS_INVALID)
            p_vout->p->step.timestamp += i_duration;
        if (!b_paused)
            p_vout->p->step.last = p_vout->p->step.timestamp;
        picture_fifo_OffsetDate( p_vout->p->decoder_fifo, i_duration );
        if (p_vout->p->displayed.decoded)
            p_vout->p->displayed.decoded->date += i_duration;

        vlc_cond_signal( &p_vout->p->picture_wait );
        vlc_mutex_unlock( &p_vout->p->picture_lock );

        spu_OffsetSubtitleDate( p_vout->p->p_spu, i_duration );
    }
    else
    {
        if (b_paused)
            p_vout->p->step.last = p_vout->p->step.timestamp;
        vlc_mutex_unlock( &p_vout->p->picture_lock );
    }
    p_vout->p->pause.is_on = b_paused;
    p_vout->p->pause.date  = i_date;

    vlc_mutex_unlock( &p_vout->p->change_lock );
}

void vout_GetResetStatistic( vout_thread_t *p_vout, int *pi_displayed, int *pi_lost )
{
    vout_statistic_GetReset( &p_vout->p->statistic,
                             pi_displayed, pi_lost );
}

static void Flush(vout_thread_t *vout, mtime_t date, bool reset, bool below)
{
    vlc_assert_locked(&vout->p->picture_lock);
    vout->p->step.timestamp = VLC_TS_INVALID;
    vout->p->step.last      = VLC_TS_INVALID;

    picture_t *last = vout->p->displayed.decoded;
    if (last) {
        if (reset) {
            picture_Release(last);
            vout->p->displayed.decoded = NULL;
        } else if (( below  && last->date <= date) ||
                   (!below && last->date >= date)) {
            vout->p->step.is_requested = true;
        }
    }
    picture_fifo_Flush( vout->p->decoder_fifo, date, below );
}

void vout_Flush(vout_thread_t *vout, mtime_t date)
{
    vlc_mutex_lock(&vout->p->picture_lock);

    Flush(vout, date, false, false);

    vlc_cond_signal(&vout->p->picture_wait);
    vlc_mutex_unlock(&vout->p->picture_lock);
}

void vout_Reset(vout_thread_t *vout)
{
    vlc_mutex_lock(&vout->p->picture_lock);

    Flush(vout, INT64_MAX, true, true);
    if (vout->p->decoder_pool)
        picture_pool_NonEmpty(vout->p->decoder_pool, true);
    vout->p->pause.is_on = false;
    vout->p->pause.date  = mdate();

    vlc_cond_signal( &vout->p->picture_wait );
    vlc_mutex_unlock(&vout->p->picture_lock);
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

    vlc_cond_signal(&vout->p->picture_wait);
    vlc_mutex_unlock(&vout->p->picture_lock);
}
void vout_NextPicture(vout_thread_t *vout, mtime_t *duration)
{
    vlc_mutex_lock(&vout->p->picture_lock);

    vout->p->b_picture_empty = false;
    vout->p->step.is_requested = true;

    /* FIXME I highly doubt that it can works with only one cond_t FIXME */
    vlc_cond_signal(&vout->p->picture_wait);

    while (vout->p->step.is_requested && !vout->p->b_picture_empty)
        vlc_cond_wait(&vout->p->picture_wait, &vout->p->picture_lock);

    if (vout->p->step.last > VLC_TS_INVALID &&
        vout->p->step.timestamp > vout->p->step.last) {
        *duration = vout->p->step.timestamp - vout->p->step.last;
        vout->p->step.last = vout->p->step.timestamp;
    } else {
        *duration = 0;
    }

    /* TODO advance subpicture by the duration ... */
    vlc_mutex_unlock(&vout->p->picture_lock);
}

void vout_DisplayTitle( vout_thread_t *p_vout, const char *psz_title )
{
    assert( psz_title );

    if( !var_InheritBool( p_vout, "osd" ) )
        return;

    vlc_mutex_lock( &p_vout->p->change_lock );
    free( p_vout->p->title.value );
    p_vout->p->title.value = strdup( psz_title );
    vlc_mutex_unlock( &p_vout->p->change_lock );
}

spu_t *vout_GetSpu( vout_thread_t *p_vout )
{
    return p_vout->p->p_spu;
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
    PrintVideoFormat(vout, "pic render", &vout->fmt_render);
    PrintVideoFormat(vout, "pic in",     &vout->fmt_in);
    PrintVideoFormat(vout, "pic out",    &vout->fmt_out);

    assert(vout->fmt_out.i_width  == vout->fmt_render.i_width &&
           vout->fmt_out.i_height == vout->fmt_render.i_height &&
           vout->fmt_out.i_chroma == vout->fmt_render.i_chroma);
    return VLC_SUCCESS;
}

/*****************************************************************************
 * CleanThread: clean up after InitThread
 *****************************************************************************
 * This function is called after a sucessful
 * initialization. It frees all resources allocated by InitThread.
 * XXX You have to enter it with change_lock taken.
 *****************************************************************************/
static void ThreadClean(vout_thread_t *vout)
{
    /* Destroy translation tables */
    if (!vout->p->b_error) {
        picture_fifo_Flush(vout->p->decoder_fifo, INT64_MAX, false);
        vout_EndWrapper(vout);
    }
}

static int ThreadDisplayPicture(vout_thread_t *vout,
                                bool now, mtime_t *deadline)
{
    int displayed_count = 0;
    int lost_count = 0;

    for (;;) {
        const mtime_t date = mdate();
        const bool is_paused = vout->p->pause.is_on;
        bool redisplay = is_paused && !now;
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
                render = picture_NewFromFormat(&vout->fmt_out);
            else if (vout->p->decoder_pool != vout->p->display_pool)
                render = picture_pool_Get(vout->p->display_pool);
            else
                render = picture_pool_Get(vout->p->private_pool);

            if (render) {
                picture_Copy(render, filtered);

                spu_RenderSubpictures(vout->p->p_spu,
                                      render, &vout->fmt_out,
                                      subpic, &vout->fmt_in, spu_render_time);
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
            vout_snapshot_Set(&vout->p->snapshot, &vout->fmt_out, direct);

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
    bool    last_picture_interlaced      = false;
    int     last_picture_qtype           = QTYPE_NONE;
    mtime_t last_picture_interlaced_date = mdate();

    /*
     * Main loop - it is not executed if an error occurred during
     * initialization
     */
    while (!vout->p->b_done && !vout->p->b_error) {
        /* */
        if(vout->p->title.show && vout->p->title.value)
            DisplayTitleOnOSD(vout);

        vlc_mutex_lock(&vout->p->picture_lock);

        mtime_t deadline = VLC_TS_INVALID;
        bool has_displayed = !ThreadDisplayPicture(vout, vout->p->step.is_requested, &deadline);

        if (has_displayed) {
            vout->p->step.timestamp = vout->p->displayed.timestamp;
            if (vout->p->step.last <= VLC_TS_INVALID)
                vout->p->step.last = vout->p->step.timestamp;
        }
        if (vout->p->step.is_requested) {
            if (!has_displayed && !vout->p->b_picture_empty) {
                picture_t *peek = picture_fifo_Peek(vout->p->decoder_fifo);
                if (peek)
                    picture_Release(peek);
                if (!peek) {
                    vout->p->b_picture_empty = true;
                    vlc_cond_signal(&vout->p->picture_wait);
                }
            }
            if (has_displayed) {
                vout->p->step.is_requested = false;
                vlc_cond_signal(&vout->p->picture_wait);
            }
        }

        const int  picture_qtype      = vout->p->displayed.qtype;
        const bool picture_interlaced = vout->p->displayed.is_interlaced;

        vlc_mutex_unlock(&vout->p->picture_lock);

        if (vout_ManageWrapper(vout)) {
            /* A fatal error occurred, and the thread must terminate
             * immediately, without displaying anything - setting b_error to 1
             * causes the immediate end of the main while() loop. */
            // FIXME pf_end
            vout->p->b_error = true;
            break;
        }

        /* Post processing */
        const int postproc_change = (picture_qtype != QTYPE_NONE) - (last_picture_qtype != QTYPE_NONE);
        if (postproc_change == 1)
            PostProcessEnable(vout);
        else if (postproc_change == -1)
            PostProcessDisable(vout);
        if (postproc_change)
            last_picture_qtype = picture_qtype;

        /* Deinterlacing
         * Wait 30s before quiting interlacing mode */
        const int interlacing_change = (!!picture_interlaced) - (!!last_picture_interlaced);
        if ((interlacing_change == 1) ||
            (interlacing_change == -1 && last_picture_interlaced_date + 30000000 < mdate())) {
            DeinterlaceNeeded(vout, picture_interlaced);
            last_picture_interlaced = picture_interlaced;
        }
        if (picture_interlaced)
            last_picture_interlaced_date = mdate();

        /* Check for "video filter2" changes */
        vlc_mutex_lock(&vout->p->vfilter_lock);
        if (vout->p->psz_vf2) {
            es_format_t fmt;

            es_format_Init(&fmt, VIDEO_ES, vout->fmt_render.i_chroma);
            fmt.video = vout->fmt_render;
            filter_chain_Reset(vout->p->p_vf2_chain, &fmt, &fmt);

            if (filter_chain_AppendFromString(vout->p->p_vf2_chain,
                                              vout->p->psz_vf2) < 0)
                msg_Err(vout, "Video filter chain creation failed");

            free(vout->p->psz_vf2);
            vout->p->psz_vf2 = NULL;

            if (last_picture_qtype != QTYPE_NONE)
                PostProcessSetFilterQuality(vout);
        }
        vlc_mutex_unlock(&vout->p->vfilter_lock);

        vlc_mutex_unlock(&vout->p->change_lock);

        if (deadline > VLC_TS_INVALID) {
            vlc_mutex_lock(&vout->p->picture_lock);
            vlc_cond_timedwait(&vout->p->picture_wait, &vout->p->picture_lock, deadline);
            vlc_mutex_unlock(&vout->p->picture_lock);
        }

        vlc_mutex_lock(&vout->p->change_lock);
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

    /* Destroy the video filters2 */
    filter_chain_Delete(vout->p->p_vf2_chain);

    vlc_mutex_unlock(&vout->p->change_lock);

    if (has_wrapper)
        vout_CloseWrapper(vout);

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

    var_SetBool( p_vout, "intf-change", true );

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
static int VideoFilter2Callback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    (void)psz_cmd; (void)oldval; (void)p_data;

    vlc_mutex_lock( &p_vout->p->vfilter_lock );
    p_vout->p->psz_vf2 = strdup( newval.psz_string );
    vlc_mutex_unlock( &p_vout->p->vfilter_lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Post-processing
 *****************************************************************************/
static bool PostProcessIsPresent( const char *psz_filter )
{
    const char  *psz_pp = "postproc";
    const size_t i_pp = strlen(psz_pp);
    return psz_filter &&
           !strncmp( psz_filter, psz_pp, strlen(psz_pp) ) &&
           ( psz_filter[i_pp] == '\0' || psz_filter[i_pp] == ':' );
}

static int PostProcessCallback( vlc_object_t *p_this, char const *psz_cmd,
                                vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    static const char *psz_pp = "postproc";

    char *psz_vf2 = var_GetString( p_vout, "video-filter" );

    if( newval.i_int <= 0 )
    {
        if( PostProcessIsPresent( psz_vf2 ) )
        {
            strcpy( psz_vf2, &psz_vf2[strlen(psz_pp)] );
            if( *psz_vf2 == ':' )
                strcpy( psz_vf2, &psz_vf2[1] );
        }
    }
    else
    {
        if( !PostProcessIsPresent( psz_vf2 ) )
        {
            if( psz_vf2 )
            {
                char *psz_tmp = psz_vf2;
                if( asprintf( &psz_vf2, "%s:%s", psz_pp, psz_tmp ) < 0 )
                    psz_vf2 = psz_tmp;
                else
                    free( psz_tmp );
            }
            else
            {
                psz_vf2 = strdup( psz_pp );
            }
        }
    }
    if( psz_vf2 )
    {
        var_SetString( p_vout, "video-filter", psz_vf2 );
        free( psz_vf2 );
    }

    return VLC_SUCCESS;
}
static void PostProcessEnable( vout_thread_t *p_vout )
{
    vlc_value_t text;
    msg_Dbg( p_vout, "Post-processing available" );
    var_Create( p_vout, "postprocess", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Post processing");
    var_Change( p_vout, "postprocess", VLC_VAR_SETTEXT, &text, NULL );

    for( int i = 0; i <= 6; i++ )
    {
        vlc_value_t val;
        vlc_value_t text;
        char psz_text[1+1];

        val.i_int = i;
        snprintf( psz_text, sizeof(psz_text), "%d", i );
        if( i == 0 )
            text.psz_string = _("Disable");
        else
            text.psz_string = psz_text;
        var_Change( p_vout, "postprocess", VLC_VAR_ADDCHOICE, &val, &text );
    }
    var_AddCallback( p_vout, "postprocess", PostProcessCallback, NULL );

    /* */
    char *psz_filter = var_GetNonEmptyString( p_vout, "video-filter" );
    int i_postproc_q = 0;
    if( PostProcessIsPresent( psz_filter ) )
        i_postproc_q = var_CreateGetInteger( p_vout, "postproc-q" );

    var_SetInteger( p_vout, "postprocess", i_postproc_q );

    free( psz_filter );
}
static void PostProcessDisable( vout_thread_t *p_vout )
{
    msg_Dbg( p_vout, "Post-processing no more available" );
    var_Destroy( p_vout, "postprocess" );
}
static void PostProcessSetFilterQuality( vout_thread_t *p_vout )
{
    vlc_object_t *p_pp = vlc_object_find_name( p_vout, "postproc", FIND_CHILD );
    if( !p_pp )
        return;

    var_SetInteger( p_pp, "postproc-q", var_GetInteger( p_vout, "postprocess" ) );
    vlc_object_release( p_pp );
}


static void DisplayTitleOnOSD( vout_thread_t *p_vout )
{
    const mtime_t i_start = mdate();
    const mtime_t i_stop = i_start + INT64_C(1000) * p_vout->p->title.timeout;

    if( i_stop <= i_start )
        return;

    vlc_assert_locked( &p_vout->p->change_lock );

    vout_ShowTextAbsolute( p_vout, DEFAULT_CHAN,
                           p_vout->p->title.value, NULL,
                           p_vout->p->title.position,
                           30 + p_vout->fmt_in.i_width
                              - p_vout->fmt_in.i_visible_width
                              - p_vout->fmt_in.i_x_offset,
                           20 + p_vout->fmt_in.i_y_offset,
                           i_start, i_stop );

    free( p_vout->p->title.value );

    p_vout->p->title.value = NULL;
}

/*****************************************************************************
 * Deinterlacing
 *****************************************************************************/
typedef struct
{
    const char *psz_mode;
    bool       b_vout_filter;
} deinterlace_mode_t;

/* XXX
 * You can use the non vout filter if and only if the video properties stay the
 * same (width/height/chroma/fps), at least for now.
 */
static const deinterlace_mode_t p_deinterlace_mode[] = {
    { "",        false },
    //{ "discard", true },
    { "blend",   false },
    //{ "mean",    true  },
    //{ "bob",     true },
    //{ "linear",  true },
    { "x",       false },
    //{ "yadif",   true },
    //{ "yadif2x", true },
    { NULL,      false }
};
static const deinterlace_mode_t *DeinterlaceGetMode( const char *psz_mode )
{
    for( const deinterlace_mode_t *p_mode = &p_deinterlace_mode[0]; p_mode->psz_mode; p_mode++ )
    {
        if( !strcmp( p_mode->psz_mode, psz_mode ) )
            return p_mode;
    }
    return NULL;
}

static char *FilterFind( char *psz_filter_base, const char *psz_module )
{
    const size_t i_module = strlen( psz_module );
    const char *psz_filter = psz_filter_base;

    if( !psz_filter || i_module <= 0 )
        return NULL;

    for( ;; )
    {
        char *psz_find = strstr( psz_filter, psz_module );
        if( !psz_find )
            return NULL;
        if( psz_find[i_module] == '\0' || psz_find[i_module] == ':' )
            return psz_find;
        psz_filter = &psz_find[i_module];
    }
}

static bool DeinterlaceIsPresent( vout_thread_t *p_vout, bool b_vout_filter )
{
    char *psz_filter = var_GetNonEmptyString( p_vout, b_vout_filter ? "vout-filter" : "video-filter" );

    bool b_found = FilterFind( psz_filter, "deinterlace" ) != NULL;

    free( psz_filter );

    return b_found;
}

static void DeinterlaceRemove( vout_thread_t *p_vout, bool b_vout_filter )
{
    const char *psz_variable = b_vout_filter ? "vout-filter" : "video-filter";
    char *psz_filter = var_GetNonEmptyString( p_vout, psz_variable );

    char *psz = FilterFind( psz_filter, "deinterlace" );
    if( !psz )
    {
        free( psz_filter );
        return;
    }

    /* */
    strcpy( &psz[0], &psz[strlen("deinterlace")] );
    if( *psz == ':' )
        strcpy( &psz[0], &psz[1] );

    var_SetString( p_vout, psz_variable, psz_filter );
    free( psz_filter );
}
static void DeinterlaceAdd( vout_thread_t *p_vout, bool b_vout_filter )
{
    const char *psz_variable = b_vout_filter ? "vout-filter" : "video-filter";

    char *psz_filter = var_GetNonEmptyString( p_vout, psz_variable );

    if( FilterFind( psz_filter, "deinterlace" ) )
    {
        free( psz_filter );
        return;
    }

    /* */
    if( psz_filter )
    {
        char *psz_tmp = psz_filter;
        if( asprintf( &psz_filter, "%s:%s", psz_tmp, "deinterlace" ) < 0 )
            psz_filter = psz_tmp;
        else
            free( psz_tmp );
    }
    else
    {
        psz_filter = strdup( "deinterlace" );
    }

    if( psz_filter )
    {
        var_SetString( p_vout, psz_variable, psz_filter );
        free( psz_filter );
    }
}

static void DeinterlaceSave( vout_thread_t *p_vout, int i_deinterlace, const char *psz_mode, bool is_needed )
{
    /* We have to set input variable to ensure restart support
     * XXX it is only needed because of vout-filter but must be done
     * for non video filter anyway */
    vlc_object_t *p_input = vlc_object_find( p_vout, VLC_OBJECT_INPUT, FIND_PARENT );
    if( !p_input )
        return;

    /* Another hack for "vout filter" mode */
    if( i_deinterlace < 0 )
        i_deinterlace = is_needed ? -2 : -3;

    var_Create( p_input, "deinterlace", VLC_VAR_INTEGER );
    var_SetInteger( p_input, "deinterlace", i_deinterlace );

    static const char * const ppsz_variable[] = {
        "deinterlace-mode",
        "filter-deinterlace-mode",
        "sout-deinterlace-mode",
        NULL
    };
    for( int i = 0; ppsz_variable[i]; i++ )
    {
        var_Create( p_input, ppsz_variable[i], VLC_VAR_STRING );
        var_SetString( p_input, ppsz_variable[i], psz_mode );
    }

    vlc_object_release( p_input );
}
static int DeinterlaceCallback( vlc_object_t *p_this, char const *psz_cmd,
                                vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(newval); VLC_UNUSED(p_data);
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    /* */
    const int  i_deinterlace = var_GetInteger( p_this, "deinterlace" );
    char       *psz_mode     = var_GetString( p_this, "deinterlace-mode" );
    const bool is_needed     = var_GetBool( p_this, "deinterlace-needed" );
    if( !psz_mode )
        return VLC_EGENERIC;

    DeinterlaceSave( p_vout, i_deinterlace, psz_mode, is_needed );

    /* */
    bool b_vout_filter = false;
    const deinterlace_mode_t *p_mode = DeinterlaceGetMode( psz_mode );
    if( p_mode )
        b_vout_filter = p_mode->b_vout_filter;

    /* */
    char *psz_old;
    if( b_vout_filter )
    {
        psz_old = var_CreateGetString( p_vout, "filter-deinterlace-mode" );
    }
    else
    {
        psz_old = var_CreateGetString( p_vout, "sout-deinterlace-mode" );
        var_SetString( p_vout, "sout-deinterlace-mode", psz_mode );
    }

    msg_Dbg( p_vout, "deinterlace %d, mode %s, is_needed %d", i_deinterlace, psz_mode, is_needed );
    if( i_deinterlace == 0 || ( i_deinterlace == -1 && !is_needed ) )
    {
        DeinterlaceRemove( p_vout, false );
        DeinterlaceRemove( p_vout, true );
    }
    else
    {
        if( !DeinterlaceIsPresent( p_vout, b_vout_filter ) )
        {
            DeinterlaceRemove( p_vout, !b_vout_filter );
            DeinterlaceAdd( p_vout, b_vout_filter );
        }
        else
        {
            /* The deinterlace filter was already inserted but we have changed the mode */
            DeinterlaceRemove( p_vout, !b_vout_filter );
            if( psz_old && strcmp( psz_old, psz_mode ) )
                var_TriggerCallback( p_vout, b_vout_filter ? "vout-filter" : "video-filter" );
        }
    }

    /* */
    free( psz_old );
    free( psz_mode );
    return VLC_SUCCESS;
}

static void DeinterlaceEnable( vout_thread_t *p_vout )
{
    vlc_value_t val, text;

    if( !p_vout->p->b_first_vout )
        return;

    msg_Dbg( p_vout, "Deinterlacing available" );

    /* Create the configuration variables */
    /* */
    var_Create( p_vout, "deinterlace", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT | VLC_VAR_HASCHOICE );
    int i_deinterlace = var_GetInteger( p_vout, "deinterlace" );

    text.psz_string = _("Deinterlace");
    var_Change( p_vout, "deinterlace", VLC_VAR_SETTEXT, &text, NULL );

    const module_config_t *p_optd = config_FindConfig( VLC_OBJECT(p_vout), "deinterlace" );
    var_Change( p_vout, "deinterlace", VLC_VAR_CLEARCHOICES, NULL, NULL );
    for( int i = 0; p_optd && i < p_optd->i_list; i++ )
    {
        val.i_int  = p_optd->pi_list[i];
        text.psz_string = (char*)vlc_gettext(p_optd->ppsz_list_text[i]);
        var_Change( p_vout, "deinterlace", VLC_VAR_ADDCHOICE, &val, &text );
    }
    var_AddCallback( p_vout, "deinterlace", DeinterlaceCallback, NULL );
    /* */
    var_Create( p_vout, "deinterlace-mode", VLC_VAR_STRING | VLC_VAR_DOINHERIT | VLC_VAR_HASCHOICE );
    char *psz_deinterlace = var_GetNonEmptyString( p_vout, "deinterlace-mode" );

    text.psz_string = _("Deinterlace mode");
    var_Change( p_vout, "deinterlace-mode", VLC_VAR_SETTEXT, &text, NULL );

    const module_config_t *p_optm = config_FindConfig( VLC_OBJECT(p_vout), "deinterlace-mode" );
    var_Change( p_vout, "deinterlace-mode", VLC_VAR_CLEARCHOICES, NULL, NULL );
    for( int i = 0; p_optm && i < p_optm->i_list; i++ )
    {
        if( !DeinterlaceGetMode( p_optm->ppsz_list[i] ) )
            continue;

        val.psz_string  = p_optm->ppsz_list[i];
        text.psz_string = (char*)vlc_gettext(p_optm->ppsz_list_text[i]);
        var_Change( p_vout, "deinterlace-mode", VLC_VAR_ADDCHOICE, &val, &text );
    }
    var_AddCallback( p_vout, "deinterlace-mode", DeinterlaceCallback, NULL );
    /* */
    var_Create( p_vout, "deinterlace-needed", VLC_VAR_BOOL );
    var_AddCallback( p_vout, "deinterlace-needed", DeinterlaceCallback, NULL );

    /* Override the initial value from filters if present */
    char *psz_filter_mode = NULL;
    if( DeinterlaceIsPresent( p_vout, true ) )
        psz_filter_mode = var_CreateGetNonEmptyString( p_vout, "filter-deinterlace-mode" );
    else if( DeinterlaceIsPresent( p_vout, false ) )
        psz_filter_mode = var_CreateGetNonEmptyString( p_vout, "sout-deinterlace-mode" );
    if( psz_filter_mode )
    {
        free( psz_deinterlace );
        if( i_deinterlace >= -1 )
            i_deinterlace = 1;
        psz_deinterlace = psz_filter_mode;
    }

    /* */
    if( i_deinterlace == -2 )
        p_vout->p->displayed.is_interlaced = true;
    else if( i_deinterlace == -3 )
        p_vout->p->displayed.is_interlaced = false;
    if( i_deinterlace < 0 )
        i_deinterlace = -1;

    /* */
    val.psz_string = psz_deinterlace ? psz_deinterlace : p_optm->orig.psz;
    var_Change( p_vout, "deinterlace-mode", VLC_VAR_SETVALUE, &val, NULL );
    val.b_bool = p_vout->p->displayed.is_interlaced;
    var_Change( p_vout, "deinterlace-needed", VLC_VAR_SETVALUE, &val, NULL );

    var_SetInteger( p_vout, "deinterlace", i_deinterlace );
    free( psz_deinterlace );
}

static void DeinterlaceNeeded( vout_thread_t *p_vout, bool is_interlaced )
{
    msg_Dbg( p_vout, "Detected %s video",
             is_interlaced ? "interlaced" : "progressive" );
    var_SetBool( p_vout, "deinterlace-needed", is_interlaced );
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

