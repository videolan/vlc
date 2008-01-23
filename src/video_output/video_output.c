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

#include <vlc/vlc.h>

#include <stdlib.h>                                                /* free() */
#include <string.h>


#ifdef HAVE_SYS_TIMES_H
#   include <sys/times.h>
#endif

#include <vlc_vout.h>
#include <vlc_playlist.h>

#include <vlc_filter.h>
#include <vlc_osd.h>

#if defined( __APPLE__ )
/* Include darwin_specific.h here if needed */
#endif

/** FIXME This is quite ugly but needed while we don't have counters
 * helpers */
#include "input/input_internal.h"

#include "modules/modules.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int      InitThread        ( vout_thread_t * );
static void     RunThread         ( vout_thread_t * );
static void     ErrorThread       ( vout_thread_t * );
static void     EndThread         ( vout_thread_t * );
static void     DestroyThread     ( vout_thread_t * );

static void     AspectRatio       ( int, int *, int * );
static int      BinaryLog         ( uint32_t );
static void     MaskToShift       ( int *, int *, uint32_t );

/* Object variables callbacks */
static int DeinterlaceCallback( vlc_object_t *, char const *,
                                vlc_value_t, vlc_value_t, void * );
static int FilterCallback( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int VideoFilter2Callback( vlc_object_t *, char const *,
                                 vlc_value_t, vlc_value_t, void * );

/* From vout_intf.c */
int vout_Snapshot( vout_thread_t *, picture_t * );

/* Video filter2 parsing */
static int ParseVideoFilter2Chain( vout_thread_t *, char * );
static void RemoveVideoFilters2( vout_thread_t *p_vout );

/* Display media title in OSD */
static void DisplayTitleOnOSD( vout_thread_t *p_vout );

/*****************************************************************************
 * Video Filter2 functions
 *****************************************************************************/
struct filter_owner_sys_t
{
    vout_thread_t *p_vout;
};

static picture_t *video_new_buffer_filter( filter_t *p_filter )
{
    picture_t *p_picture;
    vout_thread_t *p_vout = p_filter->p_owner->p_vout;

    p_picture = vout_CreatePicture( p_vout, 0, 0, 0 );

    return p_picture;
}

static void video_del_buffer_filter( filter_t *p_filter, picture_t *p_pic )
{
    vout_DestroyPicture( p_filter->p_owner->p_vout, p_pic );
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
        /* Reattach video output to playlist before bailing out */
        if( p_vout )
        {
            playlist_t  *p_playlist = pl_Yield( p_this );
            spu_Attach( p_vout->p_spu, p_this, VLC_FALSE );
            vlc_object_detach( p_vout );
            vlc_object_attach( p_vout, p_playlist );
            pl_Release( p_this );
        }
        return NULL;
    }

    /* If a video output was provided, lock it, otherwise look for one. */
    if( p_vout )
    {
        vlc_object_yield( p_vout );
    }
    else
    {
        p_vout = vlc_object_find( p_this, VLC_OBJECT_VOUT, FIND_CHILD );

        if( !p_vout )
        {
            playlist_t *p_playlist = pl_Yield( p_this );
            vlc_mutex_lock( &p_playlist->gc_lock );
            p_vout = vlc_object_find( p_playlist,
                                      VLC_OBJECT_VOUT, FIND_CHILD );
            /* only first children of p_input for unused vout */
            if( p_vout && p_vout->p_parent != (vlc_object_t *)p_playlist )
            {
                vlc_object_release( p_vout );
                p_vout = NULL;
            }
            if( p_vout )
                vlc_object_detach( p_vout );    /* Remove it from the GC */
            vlc_mutex_unlock( &p_playlist->gc_lock );
            pl_Release( p_this );
        }
    }

    /* If we now have a video output, check it has the right properties */
    if( p_vout )
    {
        char *psz_filter_chain;
        vlc_value_t val;

        /* We don't directly check for the "vout-filter" variable for obvious
         * performance reasons. */
        if( p_vout->b_filter_change )
        {
            var_Get( p_vout, "vout-filter", &val );
            psz_filter_chain = val.psz_string;

            if( psz_filter_chain && !*psz_filter_chain )
            {
                free( psz_filter_chain );
                psz_filter_chain = NULL;
            }
            if( p_vout->psz_filter_chain && !*p_vout->psz_filter_chain )
            {
                free( p_vout->psz_filter_chain );
                p_vout->psz_filter_chain = NULL;
            }

            if( !psz_filter_chain && !p_vout->psz_filter_chain )
            {
                p_vout->b_filter_change = VLC_FALSE;
            }

            if( psz_filter_chain ) free( psz_filter_chain );
        }

        if( ( p_vout->fmt_render.i_width != p_fmt->i_width ) ||
            ( p_vout->fmt_render.i_height != p_fmt->i_height ) ||
            ( p_vout->fmt_render.i_aspect != p_fmt->i_aspect ) ||
            p_vout->b_filter_change )
        {
            /* We are not interested in this format, close this vout */
            vlc_object_release( p_vout );
            vout_Destroy( p_vout );
            p_vout = NULL;
        }
        else
        {
            /* This video output is cool! Hijack it. */
            spu_Attach( p_vout->p_spu, p_this, VLC_TRUE );
            vlc_object_attach( p_vout, p_this );
            if( p_vout->b_title_show )
                DisplayTitleOnOSD( p_vout );
            vlc_object_release( p_vout );
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
    input_thread_t * p_input_thread;
    int              i_index;                               /* loop variable */
    vlc_value_t      val, text;

    unsigned int i_width = p_fmt->i_width;
    unsigned int i_height = p_fmt->i_height;
    vlc_fourcc_t i_chroma = p_fmt->i_chroma;
    unsigned int i_aspect = p_fmt->i_aspect;

    config_chain_t *p_cfg;
    char *psz_parser;
    char *psz_name;

    /* Allocate descriptor */
    p_vout = vlc_object_create( p_parent, VLC_OBJECT_VOUT );
    if( p_vout == NULL )
    {
        msg_Err( p_parent, "out of memory" );
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

    vlc_ureduce( &p_fmt->i_sar_num, &p_fmt->i_sar_den,
                 p_fmt->i_sar_num, p_fmt->i_sar_den, 50000 );
    p_vout->fmt_render        = *p_fmt;   /* FIXME palette */
    p_vout->fmt_in            = *p_fmt;   /* FIXME palette */

    p_vout->render.i_width    = i_width;
    p_vout->render.i_height   = i_height;
    p_vout->render.i_chroma   = i_chroma;
    p_vout->render.i_aspect   = i_aspect;

    p_vout->render.i_rmask    = 0;
    p_vout->render.i_gmask    = 0;
    p_vout->render.i_bmask    = 0;

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
    p_vout->f_gamma      = 0;
    p_vout->b_grayscale  = 0;
    p_vout->b_info       = 0;
    p_vout->b_interface  = 0;
    p_vout->b_scale      = 1;
    p_vout->b_fullscreen = 0;
    p_vout->i_alignment  = 0;
    p_vout->render_time  = 10;
    p_vout->c_fps_samples = 0;
    p_vout->b_filter_change = 0;
    p_vout->pf_control = 0;
    p_vout->p_parent_intf = 0;
    p_vout->i_par_num = p_vout->i_par_den = 1;

    /* Initialize locks */
    vlc_mutex_init( p_vout, &p_vout->picture_lock );
    vlc_mutex_init( p_vout, &p_vout->change_lock );
    vlc_mutex_init( p_vout, &p_vout->vfilter_lock );

    /* Mouse coordinates */
    var_Create( p_vout, "mouse-x", VLC_VAR_INTEGER );
    var_Create( p_vout, "mouse-y", VLC_VAR_INTEGER );
    var_Create( p_vout, "mouse-button-down", VLC_VAR_INTEGER );
    var_Create( p_vout, "mouse-moved", VLC_VAR_BOOL );
    var_Create( p_vout, "mouse-clicked", VLC_VAR_INTEGER );

    /* Initialize subpicture unit */
    p_vout->p_spu = spu_Create( p_vout );
    spu_Attach( p_vout->p_spu, p_parent, VLC_TRUE );

    /* Attach the new object now so we can use var inheritance below */
    vlc_object_attach( p_vout, p_parent );

    spu_Init( p_vout->p_spu );

    /* Take care of some "interface/control" related initialisations */
    vout_IntfInit( p_vout );

    /* If the parent is not a VOUT object, that means we are at the start of
     * the video output pipe */
    if( p_parent->i_object_type != VLC_OBJECT_VOUT )
    {
        /* Look for the default filter configuration */
        var_Create( p_vout, "vout-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        var_Get( p_vout, "vout-filter", &val );
        p_vout->psz_filter_chain = val.psz_string;

        /* Apply video filter2 objects on the first vout */
        var_Create( p_vout, "video-filter",
                    VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        var_Get( p_vout, "video-filter", &val );
        ParseVideoFilter2Chain( p_vout, val.psz_string );
        free( val.psz_string );
    }
    else
    {
        /* continue the parent's filter chain */
        char *psz_tmp;

        /* Ugly hack to jump to our configuration chain */
        p_vout->psz_filter_chain
            = ((vout_thread_t *)p_parent)->psz_filter_chain;
        p_vout->psz_filter_chain
            = config_ChainCreate( &psz_tmp, &p_cfg, p_vout->psz_filter_chain );
        config_ChainDestroy( p_cfg );
        free( psz_tmp );

        /* Create a video filter2 var ... but don't inherit values */
        var_Create( p_vout, "video-filter", VLC_VAR_STRING );
        ParseVideoFilter2Chain( p_vout, NULL );
    }

    var_AddCallback( p_vout, "video-filter", VideoFilter2Callback, NULL );
    p_vout->b_vfilter_change = VLC_TRUE;
    p_vout->i_vfilters = 0;

    /* Choose the video output module */
    if( !p_vout->psz_filter_chain || !*p_vout->psz_filter_chain )
    {
        var_Create( p_vout, "vout", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        var_Get( p_vout, "vout", &val );
        psz_parser = val.psz_string;
    }
    else
    {
        psz_parser = strdup( p_vout->psz_filter_chain );
    }

    /* Create the vout thread */
    config_ChainCreate( &psz_name, &p_cfg, psz_parser );
    free( psz_parser );
    p_vout->p_cfg = p_cfg;
    p_vout->p_module = module_Need( p_vout,
        ( p_vout->psz_filter_chain && *p_vout->psz_filter_chain ) ?
        "video filter" : "video output", psz_name, p_vout->psz_filter_chain && *p_vout->psz_filter_chain );
    free( psz_name );

    if( p_vout->p_module == NULL )
    {
        msg_Err( p_vout, "no suitable vout module" );
        vlc_object_detach( p_vout );
        vlc_object_destroy( p_vout );
        return NULL;
    }

    /* Create a few object variables for interface interaction */
    var_Create( p_vout, "deinterlace", VLC_VAR_STRING | VLC_VAR_HASCHOICE );
    text.psz_string = _("Deinterlace");
    var_Change( p_vout, "deinterlace", VLC_VAR_SETTEXT, &text, NULL );
    val.psz_string = (char *)""; text.psz_string = _("Disable");
    var_Change( p_vout, "deinterlace", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = (char *)"discard"; text.psz_string = _("Discard");
    var_Change( p_vout, "deinterlace", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = (char *)"blend"; text.psz_string = _("Blend");
    var_Change( p_vout, "deinterlace", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = (char *)"mean"; text.psz_string = _("Mean");
    var_Change( p_vout, "deinterlace", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = (char *)"bob"; text.psz_string = _("Bob");
    var_Change( p_vout, "deinterlace", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = (char *)"linear"; text.psz_string = _("Linear");
    var_Change( p_vout, "deinterlace", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = (char *)"x"; text.psz_string = (char *)"X";
    var_Change( p_vout, "deinterlace", VLC_VAR_ADDCHOICE, &val, &text );

    if( var_Get( p_vout, "deinterlace-mode", &val ) == VLC_SUCCESS )
    {
        var_Set( p_vout, "deinterlace", val );
        if( val.psz_string ) free( val.psz_string );
    }
    var_AddCallback( p_vout, "deinterlace", DeinterlaceCallback, NULL );

    var_Create( p_vout, "vout-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    text.psz_string = _("Filters");
    var_Change( p_vout, "vout-filter", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_vout, "vout-filter", FilterCallback, NULL );

    /* Calculate delay created by internal caching */
    p_input_thread = (input_thread_t *)vlc_object_find( p_vout,
                                           VLC_OBJECT_INPUT, FIND_ANYWHERE );
    if( p_input_thread )
    {
        p_vout->i_pts_delay = p_input_thread->i_pts_delay;
        vlc_object_release( p_input_thread );
    }
    else
    {
        p_vout->i_pts_delay = DEFAULT_PTS_DELAY;
    }

    if( vlc_thread_create( p_vout, "video output", RunThread,
                           VLC_THREAD_PRIORITY_OUTPUT, VLC_TRUE ) )
    {
        msg_Err( p_vout, "out of memory" );
        module_Unneed( p_vout, p_vout->p_module );
        vlc_object_detach( p_vout );
        vlc_object_destroy( p_vout );
        return NULL;
    }

    if( p_vout->b_error )
    {
        msg_Err( p_vout, "video output creation failed" );

        /* Make sure the thread is destroyed */
        vlc_object_kill( p_vout );
        vlc_thread_join( p_vout );

        vlc_object_detach( p_vout );
        vlc_object_destroy( p_vout );
        return NULL;
    }

    return p_vout;
}

/*****************************************************************************
 * vout_Destroy: destroys a previously created video output
 *****************************************************************************
 * Destroy a terminated thread.
 * The function will request a destruction of the specified thread. If pi_error
 * is NULL, it will return once the thread is destroyed. Else, it will be
 * update using one of the THREAD_* constants.
 *****************************************************************************/
void vout_Destroy( vout_thread_t *p_vout )
{
    vout_thread_t *p_another_vout;
    playlist_t *p_playlist = pl_Yield( p_vout );

    /* Request thread destruction */
    vlc_object_kill( p_vout );
    vlc_thread_join( p_vout );

    var_Destroy( p_vout, "intf-change" );

    if( p_vout->psz_filter_chain ) free( p_vout->psz_filter_chain );

    config_ChainDestroy( p_vout->p_cfg );

    /* Free structure */
    vlc_object_destroy( p_vout );
#ifndef __APPLE__
    /* This is a dirty hack for mostly Linux, where there is no way to get the GUI
       back if you closed it while playing video. This is solved in Mac OS X,
       where we have this novelty called menubar, that will always allow you access
       to the applications main functionality. They should try that on linux sometime */
    p_another_vout = vlc_object_find( p_playlist,
                                      VLC_OBJECT_VOUT, FIND_ANYWHERE );
    if( p_another_vout == NULL )
    {
        vlc_value_t val;
        val.b_bool = VLC_TRUE;
        var_Set( p_playlist, "intf-show", val );
    }
    else
    {
        vlc_object_release( p_another_vout );
    }
#endif
    vlc_object_release( p_playlist );
}

/*****************************************************************************
 * InitThread: initialize video output thread
 *****************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization. It returns 0 on success. Note that the thread's flag are not
 * modified inside this function.
 *****************************************************************************/
static int InitThread( vout_thread_t *p_vout )
{
    int i, i_aspect_x, i_aspect_y;

    vlc_mutex_lock( &p_vout->change_lock );

#ifdef STATS
    p_vout->c_loops = 0;
#endif

    /* Initialize output method, it allocates direct buffers for us */
    if( p_vout->pf_init( p_vout ) )
    {
        vlc_mutex_unlock( &p_vout->change_lock );
        return VLC_EGENERIC;
    }

    if( !I_OUTPUTPICTURES )
    {
        msg_Err( p_vout, "plugin was unable to allocate at least "
                         "one direct buffer" );
        p_vout->pf_end( p_vout );
        vlc_mutex_unlock( &p_vout->change_lock );
        return VLC_EGENERIC;
    }

    if( I_OUTPUTPICTURES > VOUT_MAX_PICTURES )
    {
        msg_Err( p_vout, "plugin allocated too many direct buffers, "
                         "our internal buffers must have overflown." );
        p_vout->pf_end( p_vout );
        vlc_mutex_unlock( &p_vout->change_lock );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_vout, "got %i direct buffer(s)", I_OUTPUTPICTURES );

    AspectRatio( p_vout->fmt_render.i_aspect, &i_aspect_x, &i_aspect_y );

    msg_Dbg( p_vout, "picture in %ix%i (%i,%i,%ix%i), "
             "chroma %4.4s, ar %i:%i, sar %i:%i",
             p_vout->fmt_render.i_width, p_vout->fmt_render.i_height,
             p_vout->fmt_render.i_x_offset, p_vout->fmt_render.i_y_offset,
             p_vout->fmt_render.i_visible_width,
             p_vout->fmt_render.i_visible_height,
             (char*)&p_vout->fmt_render.i_chroma,
             i_aspect_x, i_aspect_y,
             p_vout->fmt_render.i_sar_num, p_vout->fmt_render.i_sar_den );

    AspectRatio( p_vout->fmt_in.i_aspect, &i_aspect_x, &i_aspect_y );

    msg_Dbg( p_vout, "picture user %ix%i (%i,%i,%ix%i), "
             "chroma %4.4s, ar %i:%i, sar %i:%i",
             p_vout->fmt_in.i_width, p_vout->fmt_in.i_height,
             p_vout->fmt_in.i_x_offset, p_vout->fmt_in.i_y_offset,
             p_vout->fmt_in.i_visible_width,
             p_vout->fmt_in.i_visible_height,
             (char*)&p_vout->fmt_in.i_chroma,
             i_aspect_x, i_aspect_y,
             p_vout->fmt_in.i_sar_num, p_vout->fmt_in.i_sar_den );

    if( !p_vout->fmt_out.i_width || !p_vout->fmt_out.i_height )
    {
        p_vout->fmt_out.i_width = p_vout->fmt_out.i_visible_width =
            p_vout->output.i_width;
        p_vout->fmt_out.i_height = p_vout->fmt_out.i_visible_height =
            p_vout->output.i_height;
        p_vout->fmt_out.i_x_offset =  p_vout->fmt_out.i_y_offset = 0;

        p_vout->fmt_out.i_aspect = p_vout->output.i_aspect;
        p_vout->fmt_out.i_chroma = p_vout->output.i_chroma;
    }
    if( !p_vout->fmt_out.i_sar_num || !p_vout->fmt_out.i_sar_num )
    {
        p_vout->fmt_out.i_sar_num = p_vout->fmt_out.i_aspect *
            p_vout->fmt_out.i_height;
        p_vout->fmt_out.i_sar_den = VOUT_ASPECT_FACTOR *
            p_vout->fmt_out.i_width;
    }

    vlc_ureduce( &p_vout->fmt_out.i_sar_num, &p_vout->fmt_out.i_sar_den,
                 p_vout->fmt_out.i_sar_num, p_vout->fmt_out.i_sar_den, 0 );

    AspectRatio( p_vout->fmt_out.i_aspect, &i_aspect_x, &i_aspect_y );

    msg_Dbg( p_vout, "picture out %ix%i (%i,%i,%ix%i), "
             "chroma %4.4s, ar %i:%i, sar %i:%i",
             p_vout->fmt_out.i_width, p_vout->fmt_out.i_height,
             p_vout->fmt_out.i_x_offset, p_vout->fmt_out.i_y_offset,
             p_vout->fmt_out.i_visible_width,
             p_vout->fmt_out.i_visible_height,
             (char*)&p_vout->fmt_out.i_chroma,
             i_aspect_x, i_aspect_y,
             p_vout->fmt_out.i_sar_num, p_vout->fmt_out.i_sar_den );

    /* Calculate shifts from system-updated masks */
    MaskToShift( &p_vout->output.i_lrshift, &p_vout->output.i_rrshift,
                 p_vout->output.i_rmask );
    MaskToShift( &p_vout->output.i_lgshift, &p_vout->output.i_rgshift,
                 p_vout->output.i_gmask );
    MaskToShift( &p_vout->output.i_lbshift, &p_vout->output.i_rbshift,
                 p_vout->output.i_bmask );

    /* Check whether we managed to create direct buffers similar to
     * the render buffers, ie same size and chroma */
    if( ( p_vout->output.i_width == p_vout->render.i_width )
     && ( p_vout->output.i_height == p_vout->render.i_height )
     && ( vout_ChromaCmp( p_vout->output.i_chroma, p_vout->render.i_chroma ) ) )
    {
        /* Cool ! We have direct buffers, we can ask the decoder to
         * directly decode into them ! Map the first render buffers to
         * the first direct buffers, but keep the first direct buffer
         * for memcpy operations */
        p_vout->b_direct = 1;

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
        p_vout->b_direct = 0;

        /* Choose the best module */
        p_vout->chroma.p_module = module_Need( p_vout, "chroma", NULL, 0 );

        if( p_vout->chroma.p_module == NULL )
        {
            msg_Err( p_vout, "no chroma module for %4.4s to %4.4s",
                     (char*)&p_vout->render.i_chroma,
                     (char*)&p_vout->output.i_chroma );
            p_vout->pf_end( p_vout );
            vlc_mutex_unlock( &p_vout->change_lock );
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

    /* Link pictures back to their heap */
    for( i = 0 ; i < I_RENDERPICTURES ; i++ )
    {
        PP_RENDERPICTURE[ i ]->p_heap = &p_vout->render;
    }

    for( i = 0 ; i < I_OUTPUTPICTURES ; i++ )
    {
        PP_OUTPUTPICTURE[ i ]->p_heap = &p_vout->output;
    }

/* XXX XXX mark thread ready */
    return VLC_SUCCESS;
}

/*****************************************************************************
 * RunThread: video output thread
 *****************************************************************************
 * Video output thread. This function does only returns when the thread is
 * terminated. It handles the pictures arriving in the video heap and the
 * display device events.
 *****************************************************************************/
static void RunThread( vout_thread_t *p_vout)
{
    int             i_index;                                /* index in heap */
    int             i_idle_loops = 0;  /* loops without displaying a picture */
    mtime_t         current_date;                            /* current date */
    mtime_t         display_date;                            /* display date */

    picture_t *     p_picture;                            /* picture pointer */
    picture_t *     p_last_picture = NULL;                   /* last picture */
    picture_t *     p_directbuffer;              /* direct buffer to display */

    subpicture_t *  p_subpic = NULL;                   /* subpicture pointer */

    input_thread_t *p_input = NULL ;           /* Parent input, if it exists */

    vlc_value_t     val;
    vlc_bool_t      b_drop_late;

    int             i_displayed = 0, i_lost = 0, i_loops = 0;

    /*
     * Initialize thread
     */
    p_vout->b_error = InitThread( p_vout );

    var_Create( p_vout, "drop-late-frames", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Get( p_vout, "drop-late-frames", &val );
    b_drop_late = val.b_bool;

    /* signal the creation of the vout */
    vlc_thread_ready( p_vout );

    if( p_vout->b_error )
    {
        /* Destroy thread structures allocated by Create and InitThread */
        DestroyThread( p_vout );
        return;
    }

    if( p_vout->b_title_show )
        DisplayTitleOnOSD( p_vout );

    /*
     * Main loop - it is not executed if an error occurred during
     * initialization
     */
    while( (!p_vout->b_die) && (!p_vout->b_error) )
    {
        /* Initialize loop variables */
        p_picture = NULL;
        display_date = 0;
        current_date = mdate();

        if( p_input && p_input->b_die )
        {
            vlc_object_release( p_input );
            p_input = NULL;
        }

        i_loops++;
        if( i_loops % 20 == 0 )
        {
            if( !p_input )
            {
                p_input = vlc_object_find( p_vout, VLC_OBJECT_INPUT,
                                           FIND_PARENT );
            }
            if( p_input )
            {
                vlc_mutex_lock( &p_input->p->counters.counters_lock );
                stats_UpdateInteger( p_vout, p_input->p->counters.p_lost_pictures,
                                     i_lost , NULL);
                stats_UpdateInteger( p_vout,
                                     p_input->p->counters.p_displayed_pictures,
                                     i_displayed , NULL);
                i_displayed = i_lost = 0;
                vlc_mutex_unlock( &p_input->p->counters.counters_lock );
            }
        }
#if 0
        p_vout->c_loops++;
        if( !(p_vout->c_loops % VOUT_STATS_NB_LOOPS) )
        {
            msg_Dbg( p_vout, "picture heap: %d/%d",
                     I_RENDERPICTURES, p_vout->i_heap_size );
        }
#endif

        /*
         * Find the picture to display (the one with the earliest date).
         * This operation does not need lock, since only READY_PICTUREs
         * are handled. */
        for( i_index = 0; i_index < I_RENDERPICTURES; i_index++ )
        {
            if( (PP_RENDERPICTURE[i_index]->i_status == READY_PICTURE)
                && ( (p_picture == NULL) ||
                     (PP_RENDERPICTURE[i_index]->date < display_date) ) )
            {
                p_picture = PP_RENDERPICTURE[i_index];
                display_date = p_picture->date;
            }
        }

        if( p_picture )
        {
            /* If we met the last picture, parse again to see whether there is
             * a more appropriate one. */
            if( p_picture == p_last_picture )
            {
                for( i_index = 0; i_index < I_RENDERPICTURES; i_index++ )
                {
                    if( (PP_RENDERPICTURE[i_index]->i_status == READY_PICTURE)
                        && (PP_RENDERPICTURE[i_index] != p_last_picture)
                        && ((p_picture == p_last_picture) ||
                            (PP_RENDERPICTURE[i_index]->date < display_date)) )
                    {
                        p_picture = PP_RENDERPICTURE[i_index];
                        display_date = p_picture->date;
                    }
                }
            }

            /* If we found better than the last picture, destroy it */
            if( p_last_picture && p_picture != p_last_picture )
            {
                vlc_mutex_lock( &p_vout->picture_lock );
                if( p_last_picture->i_refcount )
                {
                    p_last_picture->i_status = DISPLAYED_PICTURE;
                }
                else
                {
                    p_last_picture->i_status = DESTROYED_PICTURE;
                    p_vout->i_heap_size--;
                }
                vlc_mutex_unlock( &p_vout->picture_lock );
                p_last_picture = NULL;
            }

            /* Compute FPS rate */
            p_vout->p_fps_sample[ p_vout->c_fps_samples++ % VOUT_FPS_SAMPLES ]
                = display_date;

            if( !p_picture->b_force &&
                p_picture != p_last_picture &&
                display_date < current_date + p_vout->render_time &&
                b_drop_late )
            {
                /* Picture is late: it will be destroyed and the thread
                 * will directly choose the next picture */
                vlc_mutex_lock( &p_vout->picture_lock );
                if( p_picture->i_refcount )
                {
                    /* Pretend we displayed the picture, but don't destroy
                     * it since the decoder might still need it. */
                    p_picture->i_status = DISPLAYED_PICTURE;
                }
                else
                {
                    /* Destroy the picture without displaying it */
                    p_picture->i_status = DESTROYED_PICTURE;
                    p_vout->i_heap_size--;
                }
                msg_Warn( p_vout, "late picture skipped ("I64Fd")",
                                  current_date - display_date );
                i_lost++;
                vlc_mutex_unlock( &p_vout->picture_lock );

                continue;
            }

            if( display_date >
                current_date + p_vout->i_pts_delay + VOUT_BOGUS_DELAY )
            {
                /* Picture is waaay too early: it will be destroyed */
                vlc_mutex_lock( &p_vout->picture_lock );
                if( p_picture->i_refcount )
                {
                    /* Pretend we displayed the picture, but don't destroy
                     * it since the decoder might still need it. */
                    p_picture->i_status = DISPLAYED_PICTURE;
                }
                else
                {
                    /* Destroy the picture without displaying it */
                    p_picture->i_status = DESTROYED_PICTURE;
                    p_vout->i_heap_size--;
                }
                i_lost++;
                msg_Warn( p_vout, "vout warning: early picture skipped "
                          "("I64Fd")", display_date - current_date
                          - p_vout->i_pts_delay );
                vlc_mutex_unlock( &p_vout->picture_lock );

                continue;
            }

            if( display_date > current_date + VOUT_DISPLAY_DELAY )
            {
                /* A picture is ready to be rendered, but its rendering date
                 * is far from the current one so the thread will perform an
                 * empty loop as if no picture were found. The picture state
                 * is unchanged */
                p_picture    = NULL;
                display_date = 0;
            }
            else if( p_picture == p_last_picture )
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
                    display_date = current_date + p_vout->render_time;
                }
            }
        }

        if( p_picture == NULL )
        {
            i_idle_loops++;
        }

        /* Video Filter2 stuff */
        if( p_vout->b_vfilter_change == VLC_TRUE )
        {
            int i;
            vlc_mutex_lock( &p_vout->vfilter_lock );
            RemoveVideoFilters2( p_vout );
            for( i = 0; i < p_vout->i_vfilters_cfg; i++ )
            {
                filter_t *p_vfilter =
                    p_vout->pp_vfilters[p_vout->i_vfilters] =
                        vlc_object_create( p_vout, VLC_OBJECT_FILTER );

                vlc_object_attach( p_vfilter, p_vout );

                p_vfilter->pf_vout_buffer_new = video_new_buffer_filter;
                p_vfilter->pf_vout_buffer_del = video_del_buffer_filter;

                if( !p_vout->i_vfilters )
                {
                    p_vfilter->fmt_in.video = p_vout->fmt_render;
                }
                else
                {
                    p_vfilter->fmt_in.video = (p_vfilter-1)->fmt_out.video;
                }
                /* TODO: one day filters in the middle of the chain might
                 * have a different fmt_out.video than fmt_render ... */
                p_vfilter->fmt_out.video = p_vout->fmt_render;

                p_vfilter->p_cfg = p_vout->p_vfilters_cfg[i];
                p_vfilter->p_module = module_Need( p_vfilter, "video filter2",
                                                   p_vout->psz_vfilters[i],
                                                   VLC_TRUE );

                if( p_vfilter->p_module )
                {
                    p_vfilter->p_owner =
                        malloc( sizeof( filter_owner_sys_t ) );
                    p_vfilter->p_owner->p_vout = p_vout;
                    p_vout->i_vfilters++;
                    msg_Dbg( p_vout, "video filter found (%s)",
                             p_vout->psz_vfilters[i] );
                }
                else
                {
                    msg_Err( p_vout, "no video filter found (%s)",
                             p_vout->psz_vfilters[i] );
                    vlc_object_detach( p_vfilter );
                    vlc_object_destroy( p_vfilter );
                }
            }
            p_vout->b_vfilter_change = VLC_FALSE;
            vlc_mutex_unlock( &p_vout->vfilter_lock );
        }

        if( p_picture )
        {
            int i;
            for( i = 0; i < p_vout->i_vfilters; i++ )
            {
                picture_t *p_old = p_picture;
                p_picture  = p_vout->pp_vfilters[i]->pf_video_filter(
                                 p_vout->pp_vfilters[i], p_picture );
                if( !p_picture )
                {
                    break;
                }
                /* FIXME: this is kind of wrong
                 * if you have 2 or more vfilters and the 2nd breaks,
                 * on the next loop the 1st one will be applied again */

                /* if p_old and p_picture are the same (ie the filter
                 * worked on the old picture), then following code is
                 * still alright since i_status gets changed back to
                 * the right value */
                if( p_old->i_refcount )
                {
                    p_old->i_status = DISPLAYED_PICTURE;
                }
                else
                {
                    p_old->i_status = DESTROYED_PICTURE;
                }
                p_picture->i_status = READY_PICTURE;
            }
        }

        if( p_picture && p_vout->b_snapshot )
        {
            p_vout->b_snapshot = VLC_FALSE;
            vout_Snapshot( p_vout, p_picture );
        }

        /*
         * Check for subpictures to display
         */
        if( display_date > 0 )
        {
            if( !p_input )
            {
                p_input = vlc_object_find( p_vout, VLC_OBJECT_INPUT,
                                           FIND_PARENT );
            }
            p_subpic = spu_SortSubpictures( p_vout->p_spu, display_date,
            p_input ? var_GetBool( p_input, "state" ) == PAUSE_S : VLC_FALSE );
        }

        /*
         * Perform rendering
         */
        i_displayed++;
        p_directbuffer = vout_RenderPicture( p_vout, p_picture, p_subpic );

        /*
         * Call the plugin-specific rendering method if there is one
         */
        if( p_picture != NULL && p_directbuffer != NULL && p_vout->pf_render )
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
            if( current_render_time < p_vout->render_time +
                VOUT_DISPLAY_DELAY )
            {
                /* Store render time using a sliding mean weighting to
                 * current value in a 3 to 1 ratio*/
                p_vout->render_time *= 3;
                p_vout->render_time += current_render_time;
                p_vout->render_time >>= 2;
            }
        }

        /* Give back change lock */
        vlc_mutex_unlock( &p_vout->change_lock );

        /* Sleep a while or until a given date */
        if( display_date != 0 )
        {
            /* If there are filters in the chain, better give them the picture
             * in advance */
            if( !p_vout->psz_filter_chain || !*p_vout->psz_filter_chain )
            {
                mwait( display_date - VOUT_MWAIT_TOLERANCE );
            }
        }
        else
        {
            msleep( VOUT_IDLE_SLEEP );
        }

        /* On awakening, take back lock and send immediately picture
         * to display. */
        vlc_mutex_lock( &p_vout->change_lock );

        /*
         * Display the previously rendered picture
         */
        if( p_picture != NULL && p_directbuffer != NULL )
        {
            /* Display the direct buffer returned by vout_RenderPicture */
            if( p_vout->pf_display )
            {
                p_vout->pf_display( p_vout, p_directbuffer );
            }

            /* Tell the vout this was the last picture and that it does not
             * need to be forced anymore. */
            p_last_picture = p_picture;
            p_last_picture->b_force = 0;
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
            p_vout->b_error = 1;
        }

        if( p_vout->i_changes & VOUT_SIZE_CHANGE )
        {
            /* this must only happen when the vout plugin is incapable of
             * rescaling the picture itself. In this case we need to destroy
             * the current picture buffers and recreate new ones with the right
             * dimensions */
            int i;

            p_vout->i_changes &= ~VOUT_SIZE_CHANGE;

            p_vout->pf_end( p_vout );
            for( i = 0; i < I_OUTPUTPICTURES; i++ )
                 p_vout->p_picture[ i ].i_status = FREE_PICTURE;

            I_OUTPUTPICTURES = 0;
            if( p_vout->pf_init( p_vout ) )
            {
                msg_Err( p_vout, "cannot resize display" );
                /* FIXME: pf_end will be called again in EndThread() */
                p_vout->b_error = 1;
            }

            /* Need to reinitialise the chroma plugin */
            if( p_vout->chroma.p_module )
            {
                if( p_vout->chroma.p_module->pf_deactivate )
                    p_vout->chroma.p_module->pf_deactivate( VLC_OBJECT(p_vout) );
                p_vout->chroma.p_module->pf_activate( VLC_OBJECT(p_vout) );
            }
        }

        if( p_vout->i_changes & VOUT_PICTURE_BUFFERS_CHANGE )
        {
            /* This happens when the picture buffers need to be recreated.
             * This is useful on multimonitor displays for instance.
             *
             * Warning: This only works when the vout creates only 1 picture
             * buffer!! */
            p_vout->i_changes &= ~VOUT_PICTURE_BUFFERS_CHANGE;

            if( !p_vout->b_direct )
            {
                module_Unneed( p_vout, p_vout->chroma.p_module );
            }

            vlc_mutex_lock( &p_vout->picture_lock );

            p_vout->pf_end( p_vout );

            I_OUTPUTPICTURES = I_RENDERPICTURES = 0;

            p_vout->b_error = InitThread( p_vout );

            vlc_mutex_unlock( &p_vout->picture_lock );
        }
    }

    if( p_input )
    {
        vlc_object_release( p_input );
    }

    /*
     * Error loop - wait until the thread destruction is requested
     */
    if( p_vout->b_error )
    {
        ErrorThread( p_vout );
    }

    /* End of thread */
    EndThread( p_vout );

    /* Destroy thread structures allocated by CreateThread */
    DestroyThread( p_vout );
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
    /* Wait until a `die' order */
    while( !p_vout->b_die )
    {
        /* Sleep a while */
        msleep( VOUT_IDLE_SLEEP );
    }
}

/*****************************************************************************
 * EndThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization. It frees all resources allocated by InitThread.
 *****************************************************************************/
static void EndThread( vout_thread_t *p_vout )
{
    int     i_index;                                        /* index in heap */

#ifdef STATS
    {
        struct tms cpu_usage;
        times( &cpu_usage );

        msg_Dbg( p_vout, "cpu usage (user: %d, system: %d)",
                 cpu_usage.tms_utime, cpu_usage.tms_stime );
    }
#endif

    if( !p_vout->b_direct )
    {
        module_Unneed( p_vout, p_vout->chroma.p_module );
    }

    /* Destroy all remaining pictures */
    for( i_index = 0; i_index < 2 * VOUT_MAX_PICTURES + 1; i_index++ )
    {
        if ( p_vout->p_picture[i_index].i_type == MEMORY_PICTURE )
        {
            free( p_vout->p_picture[i_index].p_data_orig );
        }
    }

    /* Destroy subpicture unit */
    spu_Attach( p_vout->p_spu, VLC_OBJECT(p_vout), VLC_FALSE );
    spu_Destroy( p_vout->p_spu );

    /* Destroy the video filters2 */
    RemoveVideoFilters2( p_vout );

    /* Destroy translation tables */
    p_vout->pf_end( p_vout );

    /* Release the change lock */
    vlc_mutex_unlock( &p_vout->change_lock );
}

/*****************************************************************************
 * DestroyThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends. It frees all ressources
 * allocated by CreateThread. Status is available at this stage.
 *****************************************************************************/
static void DestroyThread( vout_thread_t *p_vout )
{
    /* Destroy the locks */
    vlc_mutex_destroy( &p_vout->picture_lock );
    vlc_mutex_destroy( &p_vout->change_lock );
    vlc_mutex_destroy( &p_vout->vfilter_lock );

    /* Release the module */
    if( p_vout && p_vout->p_module )
    {
        module_Unneed( p_vout, p_vout->p_module );
    }
}

/* following functions are local */

static int ReduceHeight( int i_ratio )
{
    int i_dummy = VOUT_ASPECT_FACTOR;
    int i_pgcd  = 1;

    if( !i_ratio )
    {
        return i_pgcd;
    }

    /* VOUT_ASPECT_FACTOR is (2^7 * 3^3 * 5^3), we just check for 2, 3 and 5 */
    while( !(i_ratio & 1) && !(i_dummy & 1) )
    {
        i_ratio >>= 1;
        i_dummy >>= 1;
        i_pgcd  <<= 1;
    }

    while( !(i_ratio % 3) && !(i_dummy % 3) )
    {
        i_ratio /= 3;
        i_dummy /= 3;
        i_pgcd  *= 3;
    }

    while( !(i_ratio % 5) && !(i_dummy % 5) )
    {
        i_ratio /= 5;
        i_dummy /= 5;
        i_pgcd  *= 5;
    }

    return i_pgcd;
}

static void AspectRatio( int i_aspect, int *i_aspect_x, int *i_aspect_y )
{
    unsigned int i_pgcd = ReduceHeight( i_aspect );
    *i_aspect_x = i_aspect / i_pgcd;
    *i_aspect_y = VOUT_ASPECT_FACTOR / i_pgcd;
}

/*****************************************************************************
 * BinaryLog: computes the base 2 log of a binary value
 *****************************************************************************
 * This functions is used by MaskToShift, to get a bit index from a binary
 * value.
 *****************************************************************************/
static int BinaryLog( uint32_t i )
{
    int i_log = 0;

    if( i == 0 ) return -31337;

    if( i & 0xffff0000 ) i_log += 16;
    if( i & 0xff00ff00 ) i_log += 8;
    if( i & 0xf0f0f0f0 ) i_log += 4;
    if( i & 0xcccccccc ) i_log += 2;
    if( i & 0xaaaaaaaa ) i_log += 1;

    return i_log;
}

/*****************************************************************************
 * MaskToShift: transform a color mask into right and left shifts
 *****************************************************************************
 * This function is used for obtaining color shifts from masks.
 *****************************************************************************/
static void MaskToShift( int *pi_left, int *pi_right, uint32_t i_mask )
{
    uint32_t i_low, i_high;            /* lower hand higher bits of the mask */

    if( !i_mask )
    {
        *pi_left = *pi_right = 0;
        return;
    }

    /* Get bits */
    i_low = i_high = i_mask;

    i_low &= - (int32_t)i_low;          /* lower bit of the mask */
    i_high += i_low;                    /* higher bit of the mask */

    /* Transform bits into an index. Also deal with i_high overflow, which
     * is faster than changing the BinaryLog code to handle 64 bit integers. */
    i_low =  BinaryLog (i_low);
    i_high = i_high ? BinaryLog (i_high) : 32;

    /* Update pointers and return */
    *pi_left =   i_low;
    *pi_right = (8 - i_high + i_low);
}

/*****************************************************************************
 * vout_VarCallback: generic callback for intf variables
 *****************************************************************************/
int vout_VarCallback( vlc_object_t * p_this, const char * psz_variable,
                      vlc_value_t oldval, vlc_value_t newval,
                      void *p_data )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;
    vlc_value_t val;
    (void)psz_variable; (void)newval; (void)oldval; (void)p_data;
    val.b_bool = VLC_TRUE;
    var_Set( p_vout, "intf-change", val );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Helper thread for object variables callbacks.
 * Only used to avoid deadlocks when using the video embedded mode.
 *****************************************************************************/
typedef struct suxor_thread_t
{
    VLC_COMMON_MEMBERS
    input_thread_t *p_input;

} suxor_thread_t;

static void SuxorRestartVideoES( suxor_thread_t *p_this )
{
    vlc_value_t val;

    vlc_thread_ready( p_this );

    /* Now restart current video stream */
    var_Get( p_this->p_input, "video-es", &val );
    if( val.i_int >= 0 )
    {
        vlc_value_t val_es;
        val_es.i_int = -VIDEO_ES;
        var_Set( p_this->p_input, "video-es", val_es );
        var_Set( p_this->p_input, "video-es", val );
    }

    vlc_object_release( p_this->p_input );

    vlc_object_destroy( p_this );
}

/*****************************************************************************
 * object variables callbacks: a bunch of object variables are used by the
 * interfaces to interact with the vout.
 *****************************************************************************/
static int DeinterlaceCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    input_thread_t *p_input;
    vlc_value_t val;

    char *psz_mode = newval.psz_string;
    char *psz_filter, *psz_deinterlace = NULL;
    (void)psz_cmd; (void)oldval; (void)p_data;

    var_Get( p_vout, "vout-filter", &val );
    psz_filter = val.psz_string;
    if( psz_filter ) psz_deinterlace = strstr( psz_filter, "deinterlace" );

    if( !psz_mode || !*psz_mode )
    {
        if( psz_deinterlace )
        {
            char *psz_src = psz_deinterlace + sizeof("deinterlace") - 1;
            if( psz_src[0] == ':' ) psz_src++;
            memmove( psz_deinterlace, psz_src, strlen(psz_src) + 1 );
        }
    }
    else if( !psz_deinterlace )
    {
        psz_filter = realloc( psz_filter, strlen( psz_filter ) +
                              sizeof(":deinterlace") );
        if( psz_filter && *psz_filter ) strcat( psz_filter, ":" );
        strcat( psz_filter, "deinterlace" );
    }

    p_input = (input_thread_t *)vlc_object_find( p_this, VLC_OBJECT_INPUT,
                                                 FIND_PARENT );
    if( !p_input ) return VLC_EGENERIC;

    if( psz_mode && *psz_mode )
    {
        /* Modify input as well because the vout might have to be restarted */
        val.psz_string = psz_mode;
        var_Create( p_input, "deinterlace-mode", VLC_VAR_STRING );
        var_Set( p_input, "deinterlace-mode", val );
    }
    vlc_object_release( p_input );

    val.b_bool = VLC_TRUE;
    var_Set( p_vout, "intf-change", val );

    val.psz_string = psz_filter;
    var_Set( p_vout, "vout-filter", val );
    if( psz_filter ) free( psz_filter );

    return VLC_SUCCESS;
}

static int FilterCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    input_thread_t *p_input;
    vlc_value_t val;
    (void)psz_cmd; (void)oldval; (void)p_data;

    p_input = (input_thread_t *)vlc_object_find( p_this, VLC_OBJECT_INPUT,
                                                 FIND_PARENT );
    if (!p_input)
    {
        msg_Err( p_vout, "Input not found" );
        return( VLC_EGENERIC );
    }

    val.b_bool = VLC_TRUE;
    var_Set( p_vout, "intf-change", val );

    /* Modify input as well because the vout might have to be restarted */
    val.psz_string = newval.psz_string;
    var_Create( p_input, "vout-filter", VLC_VAR_STRING );

    var_Set( p_input, "vout-filter", val );

    /* Now restart current video stream */
    var_Get( p_input, "video-es", &val );
    if( val.i_int >= 0 )
    {
        suxor_thread_t *p_suxor =
            vlc_object_create( p_vout, sizeof(suxor_thread_t) );
        p_suxor->p_input = p_input;
        p_vout->b_filter_change = VLC_TRUE;
        vlc_object_yield( p_input );
        vlc_thread_create( p_suxor, "suxor", SuxorRestartVideoES,
                           VLC_THREAD_PRIORITY_LOW, VLC_FALSE );
    }

    vlc_object_release( p_input );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Video Filter2 stuff
 *****************************************************************************/
static int ParseVideoFilter2Chain( vout_thread_t *p_vout, char *psz_vfilters )
{
    int i;
    for( i = 0; i < p_vout->i_vfilters_cfg; i++ )
    {
        struct config_chain_t *p_cfg =
            p_vout->p_vfilters_cfg[p_vout->i_vfilters_cfg];
        config_ChainDestroy( p_cfg );
        if( p_vout->psz_vfilters[p_vout->i_vfilters_cfg] )
        {
            free( p_vout->psz_vfilters[p_vout->i_vfilters_cfg] );
            p_vout->psz_vfilters[p_vout->i_vfilters_cfg] = NULL;
        }
    }
    p_vout->i_vfilters_cfg = 0;
    if( psz_vfilters && *psz_vfilters )
    {
        char *psz_parser = psz_vfilters;

        while( psz_parser && *psz_parser )
        {
            psz_parser = config_ChainCreate(
                            &p_vout->psz_vfilters[p_vout->i_vfilters_cfg],
                            &p_vout->p_vfilters_cfg[p_vout->i_vfilters_cfg],
                            psz_parser );
            msg_Dbg( p_vout, "adding vfilter: %s",
                     p_vout->psz_vfilters[p_vout->i_vfilters_cfg] );
            p_vout->i_vfilters_cfg++;
            if( psz_parser && *psz_parser )
            {
                if( p_vout->i_vfilters_cfg == MAX_VFILTERS )
                {
                    msg_Warn( p_vout,
                  "maximum number of video filters reached. \"%s\" discarded",
                              psz_parser );
                    break;
                }
            }
        }
    }
    return VLC_SUCCESS;
}

static int VideoFilter2Callback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    (void)psz_cmd; (void)oldval; (void)p_data;

    vlc_mutex_lock( &p_vout->vfilter_lock );
    ParseVideoFilter2Chain( p_vout, newval.psz_string );
    p_vout->b_vfilter_change = VLC_TRUE;
    vlc_mutex_unlock( &p_vout->vfilter_lock );

    return VLC_SUCCESS;
}

static void RemoveVideoFilters2( vout_thread_t *p_vout )
{
    int i;
    for( i = 0; i < p_vout->i_vfilters; i++ )
    {
        vlc_object_detach( p_vout->pp_vfilters[i] );
        if( p_vout->pp_vfilters[i]->p_module )
        {
            module_Unneed( p_vout->pp_vfilters[i],
                           p_vout->pp_vfilters[i]->p_module );
        }

        free( p_vout->pp_vfilters[i]->p_owner );
        vlc_object_destroy( p_vout->pp_vfilters[i] );
    }
    p_vout->i_vfilters = 0;
}

static void DisplayTitleOnOSD( vout_thread_t *p_vout )
{
    input_thread_t *p_input;
    mtime_t i_now, i_stop;

    p_input = (input_thread_t *)vlc_object_find( p_vout,
              VLC_OBJECT_INPUT, FIND_ANYWHERE );
    if( p_input )
    {
        i_now = mdate();
        i_stop = i_now + (mtime_t)(p_vout->i_title_timeout * 1000);
        char *psz_nowplaying =
            input_item_GetNowPlaying( input_GetItem( p_input ) );
        char *psz_artist = input_item_GetArtist( input_GetItem( p_input ) );
        char *psz_name = input_item_GetTitle( input_GetItem( p_input ) );
        if( EMPTY_STR( psz_name ) )
        {
            free( psz_name );
            psz_name = input_item_GetName( input_GetItem( p_input ) );
        }
        if( !EMPTY_STR( psz_nowplaying ) )
        {
            vout_ShowTextAbsolute( p_vout, DEFAULT_CHAN,
                                   psz_nowplaying, NULL,
                                   p_vout->i_title_position,
                                   30 + p_vout->fmt_in.i_width
                                      - p_vout->fmt_in.i_visible_width
                                      - p_vout->fmt_in.i_x_offset,
                                   20 + p_vout->fmt_in.i_y_offset,
                                   i_now, i_stop );
        }
        else if( !EMPTY_STR( psz_artist ) )
        {
            char *psz_string = NULL;
            if( asprintf( &psz_string, "%s - %s", psz_name, psz_artist ) != -1 )
            {
                vout_ShowTextAbsolute( p_vout, DEFAULT_CHAN,
                                       psz_string, NULL,
                                       p_vout->i_title_position,
                                       30 + p_vout->fmt_in.i_width
                                          - p_vout->fmt_in.i_visible_width
                                          - p_vout->fmt_in.i_x_offset,
                                       20 + p_vout->fmt_in.i_y_offset,
                                       i_now, i_stop );
                free( psz_string );
            }
        }
        else
        {
            vout_ShowTextAbsolute( p_vout, DEFAULT_CHAN,
                                   psz_name, NULL,
                                   p_vout->i_title_position,
                                   30 + p_vout->fmt_in.i_width
                                      - p_vout->fmt_in.i_visible_width
                                      - p_vout->fmt_in.i_x_offset,
                                   20 + p_vout->fmt_in.i_y_offset,
                                   i_now, i_stop );
        }
        vlc_object_release( p_input );
        free( psz_artist );
        free( psz_name );
        free( psz_nowplaying );
    }
}

