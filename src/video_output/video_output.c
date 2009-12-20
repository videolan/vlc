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


#ifdef HAVE_SYS_TIMES_H
#   include <sys/times.h>
#endif

#include <vlc_vout.h>

#include <vlc_filter.h>
#include <vlc_osd.h>
#include <assert.h>

#if defined( __APPLE__ )
/* Include darwin_specific.h here if needed */
#endif

/** FIXME This is quite ugly but needed while we don't have counters
 * helpers */
//#include "input/input_internal.h"

#include <libvlc.h>
#include <vlc_input.h>
#include "vout_pictures.h"
#include "vout_internal.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int      InitThread        ( vout_thread_t * );
static void*    RunThread         ( void *  );
static void     ErrorThread       ( vout_thread_t * );
static void     CleanThread       ( vout_thread_t * );
static void     EndThread         ( vout_thread_t * );

static void VideoFormatImportRgb( video_format_t *, const picture_heap_t * );
static void PictureHeapFixRgb( picture_heap_t * );

static void     vout_Destructor   ( vlc_object_t * p_this );

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

/* From vout_intf.c */
int vout_Snapshot( vout_thread_t *, picture_t * );

/* Display media title in OSD */
static void DisplayTitleOnOSD( vout_thread_t *p_vout );

/* Time during which the thread will sleep if it has nothing to
 * display (in micro-seconds) */
#define VOUT_IDLE_SLEEP                 ((int)(0.020*CLOCK_FREQ))

/* Maximum lap of time allowed between the beginning of rendering and
 * display. If, compared to the current date, the next image is too
 * late, the thread will perform an idle loop. This time should be
 * at least VOUT_IDLE_SLEEP plus the time required to render a few
 * images, to avoid trashing of decoded images */
#define VOUT_DISPLAY_DELAY              ((int)(0.200*CLOCK_FREQ))

/* Better be in advance when awakening than late... */
#define VOUT_MWAIT_TOLERANCE            ((mtime_t)(0.020*CLOCK_FREQ))

/* Minimum number of direct pictures the video output will accept without
 * creating additional pictures in system memory */
#ifdef OPTIMIZE_MEMORY
#   define VOUT_MIN_DIRECT_PICTURES        (VOUT_MAX_PICTURES/2)
#else
#   define VOUT_MIN_DIRECT_PICTURES        (3*VOUT_MAX_PICTURES/4)
#endif

/*****************************************************************************
 * Video Filter2 functions
 *****************************************************************************/
static picture_t *video_new_buffer_filter( filter_t *p_filter )
{
    vout_thread_t *p_vout = (vout_thread_t*)p_filter->p_owner;
    picture_t *p_picture = vout_CreatePicture( p_vout, 0, 0, 0 );

    p_picture->i_status = READY_PICTURE;

    return p_picture;
}

static void video_del_buffer_filter( filter_t *p_filter, picture_t *p_pic )
{
    vout_thread_t *p_vout = (vout_thread_t*)p_filter->p_owner;

    vlc_mutex_lock( &p_vout->picture_lock );
    vout_UsePictureLocked( p_vout, p_pic );
    vlc_mutex_unlock( &p_vout->picture_lock );
}

static int video_filter_buffer_allocation_init( filter_t *p_filter, void *p_data )
{
    p_filter->pf_vout_buffer_new = video_new_buffer_filter;
    p_filter->pf_vout_buffer_del = video_del_buffer_filter;
    p_filter->p_owner = p_data; /* p_vout */
    return VLC_SUCCESS;
}

/*****************************************************************************
 * vout_Request: find a video output thread, create one, or destroy one.
 *****************************************************************************
 * This function looks for a video output thread matching the current
 * properties. If not found, it spawns a new one.
 *****************************************************************************/
vout_thread_t *__vout_Request( vlc_object_t *p_this, vout_thread_t *p_vout,
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
        vlc_mutex_lock( &p_vout->change_lock );

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

        if( p_vout->fmt_render.i_chroma != vlc_fourcc_GetCodec( VIDEO_ES, p_fmt->i_chroma ) ||
            p_vout->fmt_render.i_width != p_fmt->i_width ||
            p_vout->fmt_render.i_height != p_fmt->i_height ||
            p_vout->p->b_filter_change )
        {
            vlc_mutex_unlock( &p_vout->change_lock );

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

                p_vout->render.i_aspect = (int64_t)i_sar_num *
                                                   p_vout->fmt_render.i_width *
                                                   VOUT_ASPECT_FACTOR /
                                                   i_sar_den /
                                                   p_vout->fmt_render.i_height;
                p_vout->i_changes |= VOUT_ASPECT_CHANGE;
            }
            vlc_mutex_unlock( &p_vout->change_lock );

            vlc_object_release( p_vout );
        }

        if( p_vout )
        {
            msg_Dbg( p_this, "reusing provided vout" );

            spu_Attach( p_vout->p_spu, VLC_OBJECT(p_vout), false );
            vlc_object_detach( p_vout );

            vlc_object_attach( p_vout, p_this );
            spu_Attach( p_vout->p_spu, VLC_OBJECT(p_vout), true );
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
vout_thread_t * __vout_Create( vlc_object_t *p_parent, video_format_t *p_fmt )
{
    vout_thread_t  * p_vout;                            /* thread descriptor */
    int              i_index;                               /* loop variable */
    vlc_value_t      text;

    unsigned int i_width = p_fmt->i_width;
    unsigned int i_height = p_fmt->i_height;
    vlc_fourcc_t i_chroma = vlc_fourcc_GetCodec( VIDEO_ES, p_fmt->i_chroma );

    config_chain_t *p_cfg;
    char *psz_parser;
    char *psz_name;

    if( i_width <= 0 || i_height <= 0 )
        return NULL;

    vlc_ureduce( &p_fmt->i_sar_num, &p_fmt->i_sar_den,
                 p_fmt->i_sar_num, p_fmt->i_sar_den, 50000 );
    if( p_fmt->i_sar_num <= 0 || p_fmt->i_sar_den <= 0 )
        return NULL;
    unsigned int i_aspect = (int64_t)p_fmt->i_sar_num *
                                     i_width *
                                     VOUT_ASPECT_FACTOR /
                                     p_fmt->i_sar_den /
                                     i_height;

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

    /* Initialize pictures - translation tables and functions
     * will be initialized later in InitThread */
    for( i_index = 0; i_index < 2 * VOUT_MAX_PICTURES + 1; i_index++)
    {
        p_vout->p_picture[i_index].pf_lock = NULL;
        p_vout->p_picture[i_index].pf_unlock = NULL;
        p_vout->p_picture[i_index].i_status = FREE_PICTURE;
        p_vout->p_picture[i_index].i_type   = EMPTY_PICTURE;
        p_vout->p_picture[i_index].b_slow   = 0;
    }

    /* No images in the heap */
    p_vout->i_heap_size = 0;

    /* Initialize the rendering heap */
    I_RENDERPICTURES = 0;

    p_vout->fmt_render        = *p_fmt;   /* FIXME palette */
    p_vout->fmt_in            = *p_fmt;   /* FIXME palette */

    p_vout->render.i_width    = i_width;
    p_vout->render.i_height   = i_height;
    p_vout->render.i_chroma   = i_chroma;
    p_vout->render.i_aspect   = i_aspect;

    p_vout->render.i_rmask    = p_fmt->i_rmask;
    p_vout->render.i_gmask    = p_fmt->i_gmask;
    p_vout->render.i_bmask    = p_fmt->i_bmask;

    p_vout->render.i_last_used_pic = -1;
    p_vout->render.b_allow_modify_pics = 1;

    /* Zero the output heap */
    I_OUTPUTPICTURES = 0;
    p_vout->output.i_width    = 0;
    p_vout->output.i_height   = 0;
    p_vout->output.i_chroma   = 0;
    p_vout->output.i_aspect   = 0;

    p_vout->output.i_rmask    = 0;
    p_vout->output.i_gmask    = 0;
    p_vout->output.i_bmask    = 0;

    /* Initialize misc stuff */
    p_vout->i_changes    = 0;
    p_vout->b_autoscale  = 1;
    p_vout->i_zoom      = ZOOM_FP_FACTOR;
    p_vout->b_fullscreen = 0;
    p_vout->i_alignment  = 0;
    p_vout->p->render_time  = 10;
    p_vout->p->c_fps_samples = 0;
    vout_statistic_Init( &p_vout->p->statistic );
    p_vout->p->b_filter_change = 0;
    p_vout->p->b_paused = false;
    p_vout->p->i_pause_date = 0;
    p_vout->pf_control = NULL;
    p_vout->p->i_par_num =
    p_vout->p->i_par_den = 1;
    p_vout->p->p_picture_displayed = NULL;
    p_vout->p->i_picture_displayed_date = 0;
    p_vout->p->b_picture_displayed = false;
    p_vout->p->b_picture_empty = false;
    p_vout->p->i_picture_qtype = QTYPE_NONE;
    p_vout->p->b_picture_interlaced = false;

    vlc_mouse_Init( &p_vout->p->mouse );

    vout_snapshot_Init( &p_vout->p->snapshot );

    /* Initialize locks */
    vlc_mutex_init( &p_vout->picture_lock );
    vlc_cond_init( &p_vout->p->picture_wait );
    vlc_mutex_init( &p_vout->change_lock );
    vlc_mutex_init( &p_vout->p->vfilter_lock );

    /* Mouse coordinates */
    var_Create( p_vout, "mouse-x", VLC_VAR_INTEGER );
    var_Create( p_vout, "mouse-y", VLC_VAR_INTEGER );
    var_Create( p_vout, "mouse-button-down", VLC_VAR_INTEGER );
    var_Create( p_vout, "mouse-moved", VLC_VAR_BOOL );
    var_Create( p_vout, "mouse-clicked", VLC_VAR_BOOL );

    /* Initialize subpicture unit */
    p_vout->p_spu = spu_Create( p_vout );

    /* Attach the new object now so we can use var inheritance below */
    vlc_object_attach( p_vout, p_parent );

    /* */
    spu_Init( p_vout->p_spu );

    spu_Attach( p_vout->p_spu, VLC_OBJECT(p_vout), true );

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
        psz_parser = var_CreateGetString( p_vout, "vout" );
    }
    else
    {
        psz_parser = strdup( p_vout->p->psz_filter_chain );
        p_vout->p->b_title_show = false;
    }

    /* Create the vout thread */
    char* psz_tmp = config_ChainCreate( &psz_name, &p_cfg, psz_parser );
    free( psz_parser );
    free( psz_tmp );
    p_vout->p_cfg = p_cfg;

    /* Create a few object variables for interface interaction */
    var_Create( p_vout, "vout-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    text.psz_string = _("Filters");
    var_Change( p_vout, "vout-filter", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_vout, "vout-filter", FilterCallback, NULL );

    /* */
    DeinterlaceEnable( p_vout );

    if( p_vout->p->psz_filter_chain && *p_vout->p->psz_filter_chain )
        p_vout->p->psz_module_type = "video filter";
    else
        p_vout->p->psz_module_type = "video output";
    p_vout->p->psz_module_name = psz_name;
    p_vout->p_module = NULL;

    /* */
    vlc_object_set_destructor( p_vout, vout_Destructor );

    /* */
    vlc_cond_init( &p_vout->p->change_wait );
    if( vlc_clone( &p_vout->p->thread, RunThread, p_vout,
                   VLC_THREAD_PRIORITY_OUTPUT ) )
    {
        spu_Attach( p_vout->p_spu, VLC_OBJECT(p_vout), false );
        spu_Destroy( p_vout->p_spu );
        p_vout->p_spu = NULL;
        vlc_object_release( p_vout );
        return NULL;
    }

    vlc_mutex_lock( &p_vout->change_lock );
    while( !p_vout->p->b_ready )
    {   /* We are (ab)using the same condition in opposite directions for
         * b_ready and b_done. This works because of the strict ordering. */
        assert( !p_vout->p->b_done );
        vlc_cond_wait( &p_vout->p->change_wait, &p_vout->change_lock );
    }
    vlc_mutex_unlock( &p_vout->change_lock );

    if( p_vout->b_error )
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

    vlc_mutex_lock( &p_vout->change_lock );
    p_vout->p->b_done = true;
    vlc_cond_signal( &p_vout->p->change_wait );
    vlc_mutex_unlock( &p_vout->change_lock );

    vout_snapshot_End( &p_vout->p->snapshot );

    vlc_join( p_vout->p->thread, NULL );
}

/* */
static void vout_Destructor( vlc_object_t * p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    /* Make sure the vout was stopped first */
    assert( !p_vout->p_module );

    free( p_vout->p->psz_module_name );

    /* */
    if( p_vout->p_spu )
        spu_Destroy( p_vout->p_spu );

    /* Destroy the locks */
    vlc_cond_destroy( &p_vout->p->change_wait );
    vlc_cond_destroy( &p_vout->p->picture_wait );
    vlc_mutex_destroy( &p_vout->picture_lock );
    vlc_mutex_destroy( &p_vout->change_lock );
    vlc_mutex_destroy( &p_vout->p->vfilter_lock );

    /* */
    vout_statistic_Clean( &p_vout->p->statistic );

    /* */
    vout_snapshot_Clean( &p_vout->p->snapshot );

    /* */
    free( p_vout->p->psz_filter_chain );
    free( p_vout->p->psz_title );

    config_ChainDestroy( p_vout->p_cfg );

    free( p_vout->p );

}

/* */
void vout_ChangePause( vout_thread_t *p_vout, bool b_paused, mtime_t i_date )
{
    vlc_mutex_lock( &p_vout->change_lock );

    assert( !p_vout->p->b_paused || !b_paused );

    vlc_mutex_lock( &p_vout->picture_lock );

    p_vout->p->i_picture_displayed_date = 0;

    if( p_vout->p->b_paused )
    {
        const mtime_t i_duration = i_date - p_vout->p->i_pause_date;

        for( int i_index = 0; i_index < I_RENDERPICTURES; i_index++ )
        {
            picture_t *p_pic = PP_RENDERPICTURE[i_index];

            if( p_pic->i_status == READY_PICTURE )
                p_pic->date += i_duration;
        }
        vlc_cond_signal( &p_vout->p->picture_wait );
        vlc_mutex_unlock( &p_vout->picture_lock );

        spu_OffsetSubtitleDate( p_vout->p_spu, i_duration );
    }
    else
    {
        vlc_mutex_unlock( &p_vout->picture_lock );
    }
    p_vout->p->b_paused = b_paused;
    p_vout->p->i_pause_date = i_date;

    vlc_mutex_unlock( &p_vout->change_lock );
}

void vout_GetResetStatistic( vout_thread_t *p_vout, int *pi_displayed, int *pi_lost )
{
    vout_statistic_GetReset( &p_vout->p->statistic,
                             pi_displayed, pi_lost );
}

void vout_Flush( vout_thread_t *p_vout, mtime_t i_date )
{
    vlc_mutex_lock( &p_vout->picture_lock );
    p_vout->p->i_picture_displayed_date = 0;
    for( int i = 0; i < p_vout->render.i_pictures; i++ )
    {
        picture_t *p_pic = p_vout->render.pp_picture[i];

        if( p_pic->i_status == READY_PICTURE ||
            p_pic->i_status == DISPLAYED_PICTURE )
        {
            /* We cannot change picture status if it is in READY_PICTURE state,
             * Just make sure they won't be displayed */
            if( p_pic->date > i_date )
                p_pic->date = i_date;
        }
    }
    vlc_cond_signal( &p_vout->p->picture_wait );
    vlc_mutex_unlock( &p_vout->picture_lock );
}

void vout_FixLeaks( vout_thread_t *p_vout, bool b_forced )
{
    int i_pic, i_ready_pic;

    vlc_mutex_lock( &p_vout->picture_lock );

    for( i_pic = 0, i_ready_pic = 0; i_pic < p_vout->render.i_pictures && !b_forced; i_pic++ )
    {
        const picture_t *p_pic = p_vout->render.pp_picture[i_pic];

        if( p_pic->i_status == READY_PICTURE )
        {
            i_ready_pic++;
            /* If we have at least 2 ready pictures, wait for the vout thread to
             * process one */
            if( i_ready_pic >= 2 )
                break;

            continue;
        }

        if( p_pic->i_status == DISPLAYED_PICTURE )
        {
            /* If at least one displayed picture is not referenced
             * let vout free it */
            if( p_pic->i_refcount == 0 )
                break;
        }
    }
    if( i_pic < p_vout->render.i_pictures && !b_forced )
    {
        vlc_mutex_unlock( &p_vout->picture_lock );
        return;
    }

    /* Too many pictures are still referenced, there is probably a bug
     * with the decoder */
    if( !b_forced )
        msg_Err( p_vout, "pictures leaked, resetting the heap" );

    /* Just free all the pictures */
    for( i_pic = 0; i_pic < p_vout->render.i_pictures; i_pic++ )
    {
        picture_t *p_pic = p_vout->render.pp_picture[i_pic];

        msg_Dbg( p_vout, "[%d] %d %d", i_pic, p_pic->i_status, p_pic->i_refcount );
        p_pic->i_refcount = 0;

        switch( p_pic->i_status )
        {
        case READY_PICTURE:
        case DISPLAYED_PICTURE:
        case RESERVED_PICTURE:
            if( p_pic != p_vout->p->p_picture_displayed )
                vout_UsePictureLocked( p_vout, p_pic );
            break;
        }
    }
    vlc_cond_signal( &p_vout->p->picture_wait );
    vlc_mutex_unlock( &p_vout->picture_lock );
}
void vout_NextPicture( vout_thread_t *p_vout, mtime_t *pi_duration )
{
    vlc_mutex_lock( &p_vout->picture_lock );

    const mtime_t i_displayed_date = p_vout->p->i_picture_displayed_date;

    p_vout->p->b_picture_displayed = false;
    p_vout->p->b_picture_empty = false;
    if( p_vout->p->p_picture_displayed )
    {
        p_vout->p->p_picture_displayed->date = 1;
        vlc_cond_signal( &p_vout->p->picture_wait );
    }

    while( !p_vout->p->b_picture_displayed && !p_vout->p->b_picture_empty )
        vlc_cond_wait( &p_vout->p->picture_wait, &p_vout->picture_lock );

    *pi_duration = __MAX( p_vout->p->i_picture_displayed_date - i_displayed_date, 0 );

    /* TODO advance subpicture by the duration ... */

    vlc_mutex_unlock( &p_vout->picture_lock );
}

void vout_DisplayTitle( vout_thread_t *p_vout, const char *psz_title )
{
    assert( psz_title );

    if( !config_GetInt( p_vout, "osd" ) )
        return;

    vlc_mutex_lock( &p_vout->change_lock );
    free( p_vout->p->psz_title );
    p_vout->p->psz_title = strdup( psz_title );
    vlc_mutex_unlock( &p_vout->change_lock );
}

spu_t *vout_GetSpu( vout_thread_t *p_vout )
{
    return p_vout->p_spu;
}

/*****************************************************************************
 * InitThread: initialize video output thread
 *****************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization. It returns 0 on success. Note that the thread's flag are not
 * modified inside this function.
 * XXX You have to enter it with change_lock taken.
 *****************************************************************************/
static int ChromaCreate( vout_thread_t *p_vout );
static void ChromaDestroy( vout_thread_t *p_vout );

static bool ChromaIsEqual( const picture_heap_t *p_output, const picture_heap_t *p_render )
{
     if( !vout_ChromaCmp( p_output->i_chroma, p_render->i_chroma ) )
         return false;

     if( p_output->i_chroma != VLC_CODEC_RGB15 &&
         p_output->i_chroma != VLC_CODEC_RGB16 &&
         p_output->i_chroma != VLC_CODEC_RGB24 &&
         p_output->i_chroma != VLC_CODEC_RGB32 )
         return true;

     return p_output->i_rmask == p_render->i_rmask &&
            p_output->i_gmask == p_render->i_gmask &&
            p_output->i_bmask == p_render->i_bmask;
}

static int InitThread( vout_thread_t *p_vout )
{
    int i;

    /* Initialize output method, it allocates direct buffers for us */
    if( p_vout->pf_init( p_vout ) )
        return VLC_EGENERIC;

    p_vout->p->p_picture_displayed = NULL;

    if( !I_OUTPUTPICTURES )
    {
        msg_Err( p_vout, "plugin was unable to allocate at least "
                         "one direct buffer" );
        p_vout->pf_end( p_vout );
        return VLC_EGENERIC;
    }

    if( I_OUTPUTPICTURES > VOUT_MAX_PICTURES )
    {
        msg_Err( p_vout, "plugin allocated too many direct buffers, "
                         "our internal buffers must have overflown." );
        p_vout->pf_end( p_vout );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_vout, "got %i direct buffer(s)", I_OUTPUTPICTURES );

    if( !p_vout->fmt_out.i_width || !p_vout->fmt_out.i_height )
    {
        p_vout->fmt_out.i_width = p_vout->fmt_out.i_visible_width =
            p_vout->output.i_width;
        p_vout->fmt_out.i_height = p_vout->fmt_out.i_visible_height =
            p_vout->output.i_height;
        p_vout->fmt_out.i_x_offset =  p_vout->fmt_out.i_y_offset = 0;

        p_vout->fmt_out.i_chroma = p_vout->output.i_chroma;
    }
    if( !p_vout->fmt_out.i_sar_num || !p_vout->fmt_out.i_sar_num )
    {
        p_vout->fmt_out.i_sar_num = p_vout->output.i_aspect *
            p_vout->fmt_out.i_height;
        p_vout->fmt_out.i_sar_den = VOUT_ASPECT_FACTOR *
            p_vout->fmt_out.i_width;
    }

    vlc_ureduce( &p_vout->fmt_out.i_sar_num, &p_vout->fmt_out.i_sar_den,
                 p_vout->fmt_out.i_sar_num, p_vout->fmt_out.i_sar_den, 0 );

    /* FIXME removed the need of both fmt_* and heap infos */
    /* Calculate shifts from system-updated masks */
    PictureHeapFixRgb( &p_vout->render );
    VideoFormatImportRgb( &p_vout->fmt_render, &p_vout->render );

    PictureHeapFixRgb( &p_vout->output );
    VideoFormatImportRgb( &p_vout->fmt_out, &p_vout->output );

    /* print some usefull debug info about different vout formats
     */
    msg_Dbg( p_vout, "pic render sz %ix%i, of (%i,%i), vsz %ix%i, 4cc %4.4s, sar %i:%i, msk r0x%x g0x%x b0x%x",
             p_vout->fmt_render.i_width, p_vout->fmt_render.i_height,
             p_vout->fmt_render.i_x_offset, p_vout->fmt_render.i_y_offset,
             p_vout->fmt_render.i_visible_width,
             p_vout->fmt_render.i_visible_height,
             (char*)&p_vout->fmt_render.i_chroma,
             p_vout->fmt_render.i_sar_num, p_vout->fmt_render.i_sar_den,
             p_vout->fmt_render.i_rmask, p_vout->fmt_render.i_gmask, p_vout->fmt_render.i_bmask );

    msg_Dbg( p_vout, "pic in sz %ix%i, of (%i,%i), vsz %ix%i, 4cc %4.4s, sar %i:%i, msk r0x%x g0x%x b0x%x",
             p_vout->fmt_in.i_width, p_vout->fmt_in.i_height,
             p_vout->fmt_in.i_x_offset, p_vout->fmt_in.i_y_offset,
             p_vout->fmt_in.i_visible_width,
             p_vout->fmt_in.i_visible_height,
             (char*)&p_vout->fmt_in.i_chroma,
             p_vout->fmt_in.i_sar_num, p_vout->fmt_in.i_sar_den,
             p_vout->fmt_in.i_rmask, p_vout->fmt_in.i_gmask, p_vout->fmt_in.i_bmask );

    msg_Dbg( p_vout, "pic out sz %ix%i, of (%i,%i), vsz %ix%i, 4cc %4.4s, sar %i:%i, msk r0x%x g0x%x b0x%x",
             p_vout->fmt_out.i_width, p_vout->fmt_out.i_height,
             p_vout->fmt_out.i_x_offset, p_vout->fmt_out.i_y_offset,
             p_vout->fmt_out.i_visible_width,
             p_vout->fmt_out.i_visible_height,
             (char*)&p_vout->fmt_out.i_chroma,
             p_vout->fmt_out.i_sar_num, p_vout->fmt_out.i_sar_den,
             p_vout->fmt_out.i_rmask, p_vout->fmt_out.i_gmask, p_vout->fmt_out.i_bmask );

    /* Check whether we managed to create direct buffers similar to
     * the render buffers, ie same size and chroma */
    if( ( p_vout->output.i_width == p_vout->render.i_width )
     && ( p_vout->output.i_height == p_vout->render.i_height )
     && ( ChromaIsEqual( &p_vout->output, &p_vout->render ) ) )
    {
        /* Cool ! We have direct buffers, we can ask the decoder to
         * directly decode into them ! Map the first render buffers to
         * the first direct buffers, but keep the first direct buffer
         * for memcpy operations */
        p_vout->p->b_direct = true;

        for( i = 1; i < VOUT_MAX_PICTURES; i++ )
        {
            if( p_vout->p_picture[ i ].i_type != DIRECT_PICTURE &&
                I_RENDERPICTURES >= VOUT_MIN_DIRECT_PICTURES - 1 &&
                p_vout->p_picture[ i - 1 ].i_type == DIRECT_PICTURE )
            {
                /* We have enough direct buffers so there's no need to
                 * try to use system memory buffers. */
                break;
            }
            PP_RENDERPICTURE[ I_RENDERPICTURES ] = &p_vout->p_picture[ i ];
            I_RENDERPICTURES++;
        }

        msg_Dbg( p_vout, "direct render, mapping "
                 "render pictures 0-%i to system pictures 1-%i",
                 VOUT_MAX_PICTURES - 2, VOUT_MAX_PICTURES - 1 );
    }
    else
    {
        /* Rats... Something is wrong here, we could not find an output
         * plugin able to directly render what we decode. See if we can
         * find a chroma plugin to do the conversion */
        p_vout->p->b_direct = false;

        if( ChromaCreate( p_vout ) )
        {
            p_vout->pf_end( p_vout );
            return VLC_EGENERIC;
        }

        msg_Dbg( p_vout, "indirect render, mapping "
                 "render pictures 0-%i to system pictures %i-%i",
                 VOUT_MAX_PICTURES - 1, I_OUTPUTPICTURES,
                 I_OUTPUTPICTURES + VOUT_MAX_PICTURES - 1 );

        /* Append render buffers after the direct buffers */
        for( i = I_OUTPUTPICTURES; i < 2 * VOUT_MAX_PICTURES; i++ )
        {
            PP_RENDERPICTURE[ I_RENDERPICTURES ] = &p_vout->p_picture[ i ];
            I_RENDERPICTURES++;

            /* Check if we have enough render pictures */
            if( I_RENDERPICTURES == VOUT_MAX_PICTURES )
                break;
        }
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * RunThread: video output thread
 *****************************************************************************
 * Video output thread. This function does only returns when the thread is
 * terminated. It handles the pictures arriving in the video heap and the
 * display device events.
 *****************************************************************************/
static void* RunThread( void *p_this )
{
    vout_thread_t *p_vout = p_this;
    int             i_idle_loops = 0;  /* loops without displaying a picture */
    int             i_picture_qtype_last = QTYPE_NONE;
    bool            b_picture_interlaced_last = false;
    mtime_t         i_picture_interlaced_last_date;

    /*
     * Initialize thread
     */
    p_vout->p_module = module_need( p_vout,
                                    p_vout->p->psz_module_type,
                                    p_vout->p->psz_module_name,
                                    !strcmp(p_vout->p->psz_module_type, "video filter") );

    vlc_mutex_lock( &p_vout->change_lock );

    if( p_vout->p_module )
        p_vout->b_error = InitThread( p_vout );
    else
        p_vout->b_error = true;

    /* signal the creation of the vout */
    p_vout->p->b_ready = true;
    vlc_cond_signal( &p_vout->p->change_wait );

    if( p_vout->b_error )
        goto exit_thread;

    /* */
    const bool b_drop_late = var_CreateGetBool( p_vout, "drop-late-frames" );
    i_picture_interlaced_last_date = mdate();

    /*
     * Main loop - it is not executed if an error occurred during
     * initialization
     */
    while( !p_vout->p->b_done && !p_vout->b_error )
    {
        /* Initialize loop variables */
        const mtime_t current_date = mdate();
        picture_t *p_picture;
        picture_t *p_filtered_picture;
        mtime_t display_date;
        picture_t *p_directbuffer;
        int i_index;

        if( p_vout->p->b_title_show && p_vout->p->psz_title )
            DisplayTitleOnOSD( p_vout );

        vlc_mutex_lock( &p_vout->picture_lock );

        /* Look for the earliest picture but after the last displayed one */
        picture_t *p_last = p_vout->p->p_picture_displayed;;

        p_picture = NULL;
        for( i_index = 0; i_index < I_RENDERPICTURES; i_index++ )
        {
            picture_t *p_pic = PP_RENDERPICTURE[i_index];

            if( p_pic->i_status != READY_PICTURE )
                continue;

            if( p_vout->p->b_paused && p_last && p_last->date > 1 )
                continue;

            if( p_last && p_pic != p_last && p_pic->date <= p_last->date )
            {
                /* Drop old picture */
                vout_UsePictureLocked( p_vout, p_pic );
            }
            else if( !p_vout->p->b_paused && !p_pic->b_force && p_pic != p_last &&
                     p_pic->date < current_date + p_vout->p->render_time &&
                     b_drop_late )
            {
                /* Picture is late: it will be destroyed and the thread
                 * will directly choose the next picture */
                vout_UsePictureLocked( p_vout, p_pic );
                vout_statistic_Update( &p_vout->p->statistic, 0, 1 );

                msg_Warn( p_vout, "late picture skipped (%"PRId64" > %d)",
                                  current_date - p_pic->date, - p_vout->p->render_time );
            }
            else if( ( !p_last || p_last->date < p_pic->date ) &&
                     ( p_picture == NULL || p_pic->date < p_picture->date ) )
            {
                p_picture = p_pic;
            }
        }
        if( !p_picture )
        {
            p_picture = p_last;

            if( !p_vout->p->b_picture_empty )
            {
                p_vout->p->b_picture_empty = true;
                vlc_cond_signal( &p_vout->p->picture_wait );
            }
        }

        display_date = 0;
        if( p_picture )
        {
            display_date = p_picture->date;

            /* If we found better than the last picture, destroy it */
            if( p_last && p_picture != p_last )
            {
                vout_UsePictureLocked( p_vout, p_last );
                p_vout->p->p_picture_displayed = p_last = NULL;
            }

            /* Compute FPS rate */
            p_vout->p->p_fps_sample[ p_vout->p->c_fps_samples++ % VOUT_FPS_SAMPLES ] = display_date;

            if( !p_vout->p->b_paused && display_date > current_date + VOUT_DISPLAY_DELAY )
            {
                /* A picture is ready to be rendered, but its rendering date
                 * is far from the current one so the thread will perform an
                 * empty loop as if no picture were found. The picture state
                 * is unchanged */
                p_picture    = NULL;
                display_date = 0;
            }
            else if( p_picture == p_last )
            {
                /* We are asked to repeat the previous picture, but we first
                 * wait for a couple of idle loops */
                if( i_idle_loops < 4 )
                {
                    p_picture    = NULL;
                    display_date = 0;
                }
                else
                {
                    /* We set the display date to something high, otherwise
                     * we'll have lots of problems with late pictures */
                    display_date = current_date + p_vout->p->render_time;
                }
            }
            else if( p_vout->p->b_paused && display_date > current_date + VOUT_DISPLAY_DELAY )
            {
                display_date = current_date + VOUT_DISPLAY_DELAY;
            }

            if( p_picture )
            {
                if( p_picture->date > 1 )
                {
                    p_vout->p->i_picture_displayed_date = p_picture->date;
                    if( p_picture != p_last && !p_vout->p->b_picture_displayed )
                    {
                        p_vout->p->b_picture_displayed = true;
                        vlc_cond_signal( &p_vout->p->picture_wait );
                    }
                }
                p_vout->p->p_picture_displayed = p_picture;
            }
        }

        /* */
        const int i_postproc_type = p_vout->p->i_picture_qtype;
        const int i_postproc_state = (p_vout->p->i_picture_qtype != QTYPE_NONE) - (i_picture_qtype_last != QTYPE_NONE);

        const bool b_picture_interlaced = p_vout->p->b_picture_interlaced;
        const int  i_picture_interlaced_state = (!!p_vout->p->b_picture_interlaced) - (!!b_picture_interlaced_last);

        vlc_mutex_unlock( &p_vout->picture_lock );

        if( p_picture == NULL )
            i_idle_loops++;

        p_filtered_picture = NULL;
        if( p_picture )
            p_filtered_picture = filter_chain_VideoFilter( p_vout->p->p_vf2_chain,
                                                           p_picture );

        const bool b_snapshot = vout_snapshot_IsRequested( &p_vout->p->snapshot );

        /*
         * Check for subpictures to display
         */
        mtime_t spu_render_time;
        if( p_vout->p->b_paused )
            spu_render_time = p_vout->p->i_pause_date;
        else if( p_picture )
            spu_render_time = p_picture->date > 1 ? p_picture->date : mdate();
        else
            spu_render_time = 0;

        subpicture_t *p_subpic = spu_SortSubpictures( p_vout->p_spu,
                                                      spu_render_time,
                                                      b_snapshot );
        /*
         * Perform rendering
         */
        vout_statistic_Update( &p_vout->p->statistic, 1, 0 );
        p_directbuffer = vout_RenderPicture( p_vout,
                                             p_filtered_picture, p_subpic,
                                             spu_render_time );

        /*
         * Take a snapshot if requested
         */
        if( p_directbuffer && b_snapshot )
            vout_snapshot_Set( &p_vout->p->snapshot,
                               &p_vout->fmt_out, p_directbuffer );

        /*
         * Call the plugin-specific rendering method if there is one
         */
        if( p_filtered_picture != NULL && p_directbuffer != NULL && p_vout->pf_render )
        {
            /* Render the direct buffer returned by vout_RenderPicture */
            p_vout->pf_render( p_vout, p_directbuffer );
        }

        /*
         * Sleep, wake up
         */
        if( display_date != 0 && p_directbuffer != NULL )
        {
            mtime_t current_render_time = mdate() - current_date;
            /* if render time is very large we don't include it in the mean */
            if( current_render_time < p_vout->p->render_time +
                VOUT_DISPLAY_DELAY )
            {
                /* Store render time using a sliding mean weighting to
                 * current value in a 3 to 1 ratio*/
                p_vout->p->render_time *= 3;
                p_vout->p->render_time += current_render_time;
                p_vout->p->render_time >>= 2;
            }
            else
                msg_Dbg( p_vout, "skipped big render time %d > %d", (int) current_render_time,
                 (int) (p_vout->p->render_time +VOUT_DISPLAY_DELAY ) ) ;
        }

        /* Give back change lock */
        vlc_mutex_unlock( &p_vout->change_lock );

        /* Sleep a while or until a given date */
        if( display_date != 0 )
        {
            /* If there are *vout* filters in the chain, better give them the picture
             * in advance */
            if( !p_vout->p->psz_filter_chain || !*p_vout->p->psz_filter_chain )
            {
                mwait( display_date - VOUT_MWAIT_TOLERANCE );
            }
        }
        else
        {
            /* Wait until a frame is being sent or a spurious wakeup (not a problem here) */
            vlc_mutex_lock( &p_vout->picture_lock );
            vlc_cond_timedwait( &p_vout->p->picture_wait, &p_vout->picture_lock, current_date + VOUT_IDLE_SLEEP );
            vlc_mutex_unlock( &p_vout->picture_lock );
        }

        /* On awakening, take back lock and send immediately picture
         * to display. */
        /* Note: p_vout->p->b_done could be true here and now */
        vlc_mutex_lock( &p_vout->change_lock );

        /*
         * Display the previously rendered picture
         */
        if( p_filtered_picture != NULL && p_directbuffer != NULL )
        {
            /* Display the direct buffer returned by vout_RenderPicture */
            if( p_vout->pf_display )
                p_vout->pf_display( p_vout, p_directbuffer );

            /* Tell the vout this was the last picture and that it does not
             * need to be forced anymore. */
            p_picture->b_force = false;
        }

        /* Drop the filtered picture if created by video filters */
        if( p_filtered_picture != NULL && p_filtered_picture != p_picture )
        {
            vlc_mutex_lock( &p_vout->picture_lock );
            vout_UsePictureLocked( p_vout, p_filtered_picture );
            vlc_mutex_unlock( &p_vout->picture_lock );
        }

        if( p_picture != NULL )
        {
            /* Reinitialize idle loop count */
            i_idle_loops = 0;
        }

        /*
         * Check events and manage thread
         */
        if( p_vout->pf_manage && p_vout->pf_manage( p_vout ) )
        {
            /* A fatal error occurred, and the thread must terminate
             * immediately, without displaying anything - setting b_error to 1
             * causes the immediate end of the main while() loop. */
            // FIXME pf_end
            p_vout->b_error = 1;
            break;
        }

        while( p_vout->i_changes & VOUT_ON_TOP_CHANGE )
        {
            p_vout->i_changes &= ~VOUT_ON_TOP_CHANGE;
            vlc_mutex_unlock( &p_vout->change_lock );
            vout_Control( p_vout, VOUT_SET_STAY_ON_TOP, p_vout->b_on_top );
            vlc_mutex_lock( &p_vout->change_lock );
        }

        if( p_vout->i_changes & VOUT_SIZE_CHANGE )
        {
            /* this must only happen when the vout plugin is incapable of
             * rescaling the picture itself. In this case we need to destroy
             * the current picture buffers and recreate new ones with the right
             * dimensions */
            int i;

            p_vout->i_changes &= ~VOUT_SIZE_CHANGE;

            assert( !p_vout->p->b_direct );

            ChromaDestroy( p_vout );

            vlc_mutex_lock( &p_vout->picture_lock );

            p_vout->pf_end( p_vout );

            p_vout->p->p_picture_displayed = NULL;
            for( i = 0; i < I_OUTPUTPICTURES; i++ )
                 p_vout->p_picture[ i ].i_status = FREE_PICTURE;
            vlc_cond_signal( &p_vout->p->picture_wait );

            I_OUTPUTPICTURES = 0;

            if( p_vout->pf_init( p_vout ) )
            {
                msg_Err( p_vout, "cannot resize display" );
                /* FIXME: pf_end will be called again in CleanThread()? */
                p_vout->b_error = 1;
            }

            vlc_mutex_unlock( &p_vout->picture_lock );

            /* Need to reinitialise the chroma plugin. Since we might need
             * resizing too and it's not sure that we already had it,
             * recreate the chroma plugin chain from scratch. */
            /* dionoea */
            if( ChromaCreate( p_vout ) )
            {
                msg_Err( p_vout, "WOW THIS SUCKS BIG TIME!!!!!" );
                p_vout->b_error = 1;
            }
            if( p_vout->b_error )
                break;
        }

        if( p_vout->i_changes & VOUT_PICTURE_BUFFERS_CHANGE )
        {
            /* This happens when the picture buffers need to be recreated.
             * This is useful on multimonitor displays for instance.
             *
             * Warning: This only works when the vout creates only 1 picture
             * buffer!! */
            p_vout->i_changes &= ~VOUT_PICTURE_BUFFERS_CHANGE;

            if( !p_vout->p->b_direct )
                ChromaDestroy( p_vout );

            vlc_mutex_lock( &p_vout->picture_lock );

            p_vout->pf_end( p_vout );

            I_OUTPUTPICTURES = I_RENDERPICTURES = 0;

            p_vout->b_error = InitThread( p_vout );
            if( p_vout->b_error )
                msg_Err( p_vout, "InitThread after VOUT_PICTURE_BUFFERS_CHANGE failed" );

            vlc_cond_signal( &p_vout->p->picture_wait );
            vlc_mutex_unlock( &p_vout->picture_lock );

            if( p_vout->b_error )
                break;
        }

        /* Post processing */
        if( i_postproc_state == 1 )
            PostProcessEnable( p_vout );
        else if( i_postproc_state == -1 )
            PostProcessDisable( p_vout );
        if( i_postproc_state != 0 )
            i_picture_qtype_last = i_postproc_type;

        /* Deinterlacing
         * Wait 30s before quiting interlacing mode */
        if( ( i_picture_interlaced_state == 1 ) ||
            ( i_picture_interlaced_state == -1 && i_picture_interlaced_last_date + 30000000 < current_date ) )
        {
            DeinterlaceNeeded( p_vout, b_picture_interlaced );
            b_picture_interlaced_last = b_picture_interlaced;
        }
        if( b_picture_interlaced )
            i_picture_interlaced_last_date = current_date;


        /* Check for "video filter2" changes */
        vlc_mutex_lock( &p_vout->p->vfilter_lock );
        if( p_vout->p->psz_vf2 )
        {
            es_format_t fmt;

            es_format_Init( &fmt, VIDEO_ES, p_vout->fmt_render.i_chroma );
            fmt.video = p_vout->fmt_render;
            filter_chain_Reset( p_vout->p->p_vf2_chain, &fmt, &fmt );

            if( filter_chain_AppendFromString( p_vout->p->p_vf2_chain,
                                               p_vout->p->psz_vf2 ) < 0 )
                msg_Err( p_vout, "Video filter chain creation failed" );

            free( p_vout->p->psz_vf2 );
            p_vout->p->psz_vf2 = NULL;

            if( i_picture_qtype_last != QTYPE_NONE )
                PostProcessSetFilterQuality( p_vout );
        }
        vlc_mutex_unlock( &p_vout->p->vfilter_lock );
    }

    /*
     * Error loop - wait until the thread destruction is requested
     */
    if( p_vout->b_error )
        ErrorThread( p_vout );

    /* Clean thread */
    CleanThread( p_vout );

exit_thread:
    /* End of thread */
    EndThread( p_vout );
    vlc_mutex_unlock( &p_vout->change_lock );

    if( p_vout->p_module )
        module_unneed( p_vout, p_vout->p_module );
    p_vout->p_module = NULL;

    return NULL;
}

/*****************************************************************************
 * ErrorThread: RunThread() error loop
 *****************************************************************************
 * This function is called when an error occurred during thread main's loop.
 * The thread can still receive feed, but must be ready to terminate as soon
 * as possible.
 *****************************************************************************/
static void ErrorThread( vout_thread_t *p_vout )
{
    /* Wait until a `close' order */
    while( !p_vout->p->b_done )
        vlc_cond_wait( &p_vout->p->change_wait, &p_vout->change_lock );
}

/*****************************************************************************
 * CleanThread: clean up after InitThread
 *****************************************************************************
 * This function is called after a sucessful
 * initialization. It frees all resources allocated by InitThread.
 * XXX You have to enter it with change_lock taken.
 *****************************************************************************/
static void CleanThread( vout_thread_t *p_vout )
{
    int     i_index;                                        /* index in heap */

    if( !p_vout->p->b_direct )
        ChromaDestroy( p_vout );

    /* Destroy all remaining pictures */
    for( i_index = 0; i_index < 2 * VOUT_MAX_PICTURES + 1; i_index++ )
    {
        if ( p_vout->p_picture[i_index].i_type == MEMORY_PICTURE )
        {
            free( p_vout->p_picture[i_index].p_data_orig );
        }
    }

    /* Destroy translation tables */
    if( !p_vout->b_error )
        p_vout->pf_end( p_vout );
}

/*****************************************************************************
 * EndThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends.
 * It frees all resources not allocated by InitThread.
 * XXX You have to enter it with change_lock taken.
 *****************************************************************************/
static void EndThread( vout_thread_t *p_vout )
{
#ifdef STATS
    {
        struct tms cpu_usage;
        times( &cpu_usage );

        msg_Dbg( p_vout, "cpu usage (user: %d, system: %d)",
                 cpu_usage.tms_utime, cpu_usage.tms_stime );
    }
#endif

    /* FIXME does that function *really* need to be called inside the thread ? */

    /* Detach subpicture unit from both input and vout */
    spu_Attach( p_vout->p_spu, VLC_OBJECT(p_vout), false );
    vlc_object_detach( p_vout->p_spu );

    /* Destroy the video filters2 */
    filter_chain_Delete( p_vout->p->p_vf2_chain );
}

/* Thread helpers */
static picture_t *ChromaGetPicture( filter_t *p_filter )
{
    picture_t *p_pic = (picture_t *)p_filter->p_owner;
    p_filter->p_owner = NULL;
    return p_pic;
}

static int ChromaCreate( vout_thread_t *p_vout )
{
    static const char typename[] = "chroma";
    filter_t *p_chroma;

    /* Choose the best module */
    p_chroma = p_vout->p->p_chroma =
        vlc_custom_create( p_vout, sizeof(filter_t), VLC_OBJECT_GENERIC,
                           typename );

    vlc_object_attach( p_chroma, p_vout );

    /* TODO: Set the fmt_in and fmt_out stuff here */
    p_chroma->fmt_in.video = p_vout->fmt_render;
    p_chroma->fmt_out.video = p_vout->fmt_out;
    VideoFormatImportRgb( &p_chroma->fmt_in.video, &p_vout->render );
    VideoFormatImportRgb( &p_chroma->fmt_out.video, &p_vout->output );

    p_chroma->p_module = module_need( p_chroma, "video filter2", NULL, false );

    if( p_chroma->p_module == NULL )
    {
        msg_Err( p_vout, "no chroma module for %4.4s to %4.4s i=%dx%d o=%dx%d",
                 (char*)&p_vout->render.i_chroma,
                 (char*)&p_vout->output.i_chroma,
                 p_chroma->fmt_in.video.i_width, p_chroma->fmt_in.video.i_height,
                 p_chroma->fmt_out.video.i_width, p_chroma->fmt_out.video.i_height
                 );

        vlc_object_release( p_vout->p->p_chroma );
        p_vout->p->p_chroma = NULL;

        return VLC_EGENERIC;
    }
    p_chroma->pf_vout_buffer_new = ChromaGetPicture;
    return VLC_SUCCESS;
}

static void ChromaDestroy( vout_thread_t *p_vout )
{
    assert( !p_vout->p->b_direct );

    if( !p_vout->p->p_chroma )
        return;

    module_unneed( p_vout->p->p_chroma, p_vout->p->p_chroma->p_module );
    vlc_object_release( p_vout->p->p_chroma );
    p_vout->p->p_chroma = NULL;
}

/* following functions are local */

/**
 * This function copies all RGB informations from a picture_heap_t into
 * a video_format_t
 */
static void VideoFormatImportRgb( video_format_t *p_fmt, const picture_heap_t *p_heap )
{
    p_fmt->i_rmask = p_heap->i_rmask;
    p_fmt->i_gmask = p_heap->i_gmask;
    p_fmt->i_bmask = p_heap->i_bmask;
    p_fmt->i_rrshift = p_heap->i_rrshift;
    p_fmt->i_lrshift = p_heap->i_lrshift;
    p_fmt->i_rgshift = p_heap->i_rgshift;
    p_fmt->i_lgshift = p_heap->i_lgshift;
    p_fmt->i_rbshift = p_heap->i_rbshift;
    p_fmt->i_lbshift = p_heap->i_lbshift;
}

/**
 * This funtion copes all RGB informations from a video_format_t into
 * a picture_heap_t
 */
static void VideoFormatExportRgb( const video_format_t *p_fmt, picture_heap_t *p_heap )
{
    p_heap->i_rmask = p_fmt->i_rmask;
    p_heap->i_gmask = p_fmt->i_gmask;
    p_heap->i_bmask = p_fmt->i_bmask;
    p_heap->i_rrshift = p_fmt->i_rrshift;
    p_heap->i_lrshift = p_fmt->i_lrshift;
    p_heap->i_rgshift = p_fmt->i_rgshift;
    p_heap->i_lgshift = p_fmt->i_lgshift;
    p_heap->i_rbshift = p_fmt->i_rbshift;
    p_heap->i_lbshift = p_fmt->i_lbshift;
}

/**
 * This function computes rgb shifts from masks
 */
static void PictureHeapFixRgb( picture_heap_t *p_heap )
{
    video_format_t fmt;

    /* */
    fmt.i_chroma = p_heap->i_chroma;
    VideoFormatImportRgb( &fmt, p_heap );

    /* */
    video_format_FixRgb( &fmt );

    VideoFormatExportRgb( &fmt, p_heap );
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
    const mtime_t i_stop = i_start + INT64_C(1000) * p_vout->p->i_title_timeout;

    if( i_stop <= i_start )
        return;

    vlc_assert_locked( &p_vout->change_lock );

    vout_ShowTextAbsolute( p_vout, DEFAULT_CHAN,
                           p_vout->p->psz_title, NULL,
                           p_vout->p->i_title_position,
                           30 + p_vout->fmt_in.i_width
                              - p_vout->fmt_in.i_visible_width
                              - p_vout->fmt_in.i_x_offset,
                           20 + p_vout->fmt_in.i_y_offset,
                           i_start, i_stop );

    free( p_vout->p->psz_title );

    p_vout->p->psz_title = NULL;
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
    { "discard", true },
    { "blend",   false },
    { "mean",    true  },
    { "bob",     true },
    { "linear",  true },
    { "x",       false },
    { "yadif",   true },
    { "yadif2x", true },
    { NULL,      true }
};

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
    bool b_vout_filter = true;
    for( const deinterlace_mode_t *p_mode = &p_deinterlace_mode[0]; p_mode->psz_mode; p_mode++ )
    {
        if( !strcmp( p_mode->psz_mode, psz_mode ) )
        {
            b_vout_filter = p_mode->b_vout_filter;
            break;
        }
    }

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
        p_vout->p->b_picture_interlaced = true;
    else if( i_deinterlace == -3 )
        p_vout->p->b_picture_interlaced = false;
    if( i_deinterlace < 0 )
        i_deinterlace = -1;

    /* */
    val.psz_string = psz_deinterlace ? psz_deinterlace : p_optm->orig.psz;
    var_Change( p_vout, "deinterlace-mode", VLC_VAR_SETVALUE, &val, NULL );
    val.b_bool = p_vout->p->b_picture_interlaced;
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

