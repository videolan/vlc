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

#if defined( __APPLE__ )
/* Include darwin_specific.h here if needed */
#endif

/** FIXME This is quite ugly but needed while we don't have counters
 * helpers */
#include "input/input_internal.h"

#include "modules/modules.h"
#include <assert.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int      InitThread        ( vout_thread_t * );
static void*    RunThread         ( vlc_object_t *  );
static void     ErrorThread       ( vout_thread_t * );
static void     CleanThread       ( vout_thread_t * );
static void     EndThread         ( vout_thread_t * );

static void     AspectRatio       ( int, int *, int * );

static void VideoFormatImportRgb( video_format_t *, const picture_heap_t * );
static void PictureHeapFixRgb( picture_heap_t * );

static void     vout_Destructor   ( vlc_object_t * p_this );

/* Object variables callbacks */
static int DeinterlaceCallback( vlc_object_t *, char const *,
                                vlc_value_t, vlc_value_t, void * );
static int FilterCallback( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int VideoFilter2Callback( vlc_object_t *, char const *,
                                 vlc_value_t, vlc_value_t, void * );

/* From vout_intf.c */
int vout_Snapshot( vout_thread_t *, picture_t * );

/* Display media title in OSD */
static void DisplayTitleOnOSD( vout_thread_t *p_vout );

/*****************************************************************************
 * Video Filter2 functions
 *****************************************************************************/
static picture_t *video_new_buffer_filter( filter_t *p_filter )
{
    picture_t *p_picture;
    vout_thread_t *p_vout = (vout_thread_t*)p_filter->p_owner;

    p_picture = vout_CreatePicture( p_vout, 0, 0, 0 );

    return p_picture;
}

static void video_del_buffer_filter( filter_t *p_filter, picture_t *p_pic )
{
    vout_DestroyPicture( (vout_thread_t*)p_filter->p_owner, p_pic );
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
        vlc_object_yield( p_vout );
    }

    /* TODO: find a suitable unused video output */

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
                p_vout->b_filter_change = false;
            }

            free( psz_filter_chain );
        }

        if( ( p_vout->fmt_render.i_width != p_fmt->i_width ) ||
            ( p_vout->fmt_render.i_height != p_fmt->i_height ) ||
            ( p_vout->fmt_render.i_aspect != p_fmt->i_aspect ) ||
            p_vout->b_filter_change )
        {
            /* We are not interested in this format, close this vout */
            vout_CloseAndRelease( p_vout );
            vlc_object_release( p_vout );
            p_vout = NULL;
        }
        else
        {
            /* This video output is cool! Hijack it. */
            spu_Attach( p_vout->p_spu, p_this, true );
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

    if( i_width <= 0 || i_height <= 0 || i_aspect <= 0 )
        return NULL;

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
    p_vout->pf_control = NULL;
    p_vout->p_window = NULL;
    p_vout->i_par_num = p_vout->i_par_den = 1;

    /* Initialize locks */
    vlc_mutex_init( &p_vout->picture_lock );
    vlc_mutex_init( &p_vout->change_lock );
    vlc_mutex_init( &p_vout->vfilter_lock );

    /* Mouse coordinates */
    var_Create( p_vout, "mouse-x", VLC_VAR_INTEGER );
    var_Create( p_vout, "mouse-y", VLC_VAR_INTEGER );
    var_Create( p_vout, "mouse-button-down", VLC_VAR_INTEGER );
    var_Create( p_vout, "mouse-moved", VLC_VAR_BOOL );
    var_Create( p_vout, "mouse-clicked", VLC_VAR_INTEGER );

    /* Initialize subpicture unit */
    p_vout->p_spu = spu_Create( p_vout );
    spu_Attach( p_vout->p_spu, p_parent, true );

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
        p_vout->psz_filter_chain =
            var_CreateGetStringCommand( p_vout, "vout-filter" );

        /* Apply video filter2 objects on the first vout */
        p_vout->psz_vf2 =
            var_CreateGetStringCommand( p_vout, "video-filter" );
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
        var_Create( p_vout, "video-filter",
                    VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
        p_vout->psz_vf2 = var_GetString( p_vout, "video-filter" );
    }

    var_AddCallback( p_vout, "video-filter", VideoFilter2Callback, NULL );
    p_vout->p_vf2_chain = filter_chain_New( p_vout, "video filter2",
        false, video_filter_buffer_allocation_init, NULL, p_vout );

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
    char* psz_tmp = config_ChainCreate( &psz_name, &p_cfg, psz_parser );
    free( psz_parser );
    free( psz_tmp );
    p_vout->p_cfg = p_cfg;
    p_vout->p_module = module_Need( p_vout,
        ( p_vout->psz_filter_chain && *p_vout->psz_filter_chain ) ?
        "video filter" : "video output", psz_name, p_vout->psz_filter_chain && *p_vout->psz_filter_chain );
    free( psz_name );

    if( p_vout->p_module == NULL )
    {
        msg_Err( p_vout, "no suitable vout module" );
        // FIXME it's ugly but that's exactly the function that need to be called.
        EndThread( p_vout );
        vlc_object_detach( p_vout );
        vlc_object_release( p_vout );
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
        free( val.psz_string );
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
                           VLC_THREAD_PRIORITY_OUTPUT, true ) )
    {
        module_Unneed( p_vout, p_vout->p_module );
        vlc_object_release( p_vout );
        return NULL;
    }

    vlc_object_set_destructor( p_vout, vout_Destructor );

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
 * You should NEVER call it on vout not obtained though vout_Create
 * (like with vout_Request or vlc_object_find.)
 * You can use vout_CloseAndRelease() as a convenient method.
 *****************************************************************************/
void vout_Close( vout_thread_t *p_vout )
{
    assert( p_vout );

    vlc_object_kill( p_vout );
    vlc_thread_join( p_vout );
    module_Unneed( p_vout, p_vout->p_module );
    p_vout->p_module = NULL;
}

/* */
static void vout_Destructor( vlc_object_t * p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    /* Make sure the vout was stopped first */
    assert( !p_vout->p_module );

    /* Destroy the locks */
    vlc_mutex_destroy( &p_vout->picture_lock );
    vlc_mutex_destroy( &p_vout->change_lock );
    vlc_mutex_destroy( &p_vout->vfilter_lock );

    free( p_vout->psz_filter_chain );

    config_ChainDestroy( p_vout->p_cfg );

#ifndef __APPLE__
    vout_thread_t *p_another_vout;

    /* This is a dirty hack mostly for Linux, where there is no way to get the
     * GUI back if you closed it while playing video. This is solved in
     * Mac OS X, where we have this novelty called menubar, that will always
     * allow you access to the applications main functionality. They should try
     * that on linux sometime. */
    p_another_vout = vlc_object_find( p_this->p_libvlc,
                                      VLC_OBJECT_VOUT, FIND_ANYWHERE );
    if( p_another_vout == NULL )
        var_SetBool( p_this->p_libvlc, "intf-show", true );
    else
        vlc_object_release( p_another_vout );
#endif
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
static void DropPicture( vout_thread_t *p_vout, picture_t *p_picture );

static int InitThread( vout_thread_t *p_vout )
{
    int i, i_aspect_x, i_aspect_y;

#ifdef STATS
    p_vout->c_loops = 0;
#endif

    /* Initialize output method, it allocates direct buffers for us */
    if( p_vout->pf_init( p_vout ) )
        return VLC_EGENERIC;

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

    /* FIXME removed the need of both fmt_* and heap infos */
    /* Calculate shifts from system-updated masks */
    PictureHeapFixRgb( &p_vout->render );
    VideoFormatImportRgb( &p_vout->fmt_render, &p_vout->render );

    PictureHeapFixRgb( &p_vout->output );
    VideoFormatImportRgb( &p_vout->fmt_out, &p_vout->output );

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

    /* Link pictures back to their heap */
    for( i = 0 ; i < I_RENDERPICTURES ; i++ )
    {
        PP_RENDERPICTURE[ i ]->p_heap = &p_vout->render;
    }

    for( i = 0 ; i < I_OUTPUTPICTURES ; i++ )
    {
        PP_OUTPUTPICTURE[ i ]->p_heap = &p_vout->output;
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
static void* RunThread( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
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
    bool      b_drop_late;

    int             i_displayed = 0, i_lost = 0, i_loops = 0;

    /*
     * Initialize thread
     */
    vlc_mutex_lock( &p_vout->change_lock );
    p_vout->b_error = InitThread( p_vout );

    var_Create( p_vout, "drop-late-frames", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Get( p_vout, "drop-late-frames", &val );
    b_drop_late = val.b_bool;

    /* signal the creation of the vout */
    vlc_thread_ready( p_vout );

    if( p_vout->b_error )
    {
        EndThread( p_vout );
        vlc_mutex_unlock( &p_vout->change_lock );
        return NULL;
    }

    vlc_object_lock( p_vout );

    if( p_vout->b_title_show )
        DisplayTitleOnOSD( p_vout );

    /*
     * Main loop - it is not executed if an error occurred during
     * initialization
     */
    while( vlc_object_alive( p_vout ) && !p_vout->b_error )
    {
        /* Initialize loop variables */
        p_picture = NULL;
        display_date = 0;
        current_date = mdate();

        i_loops++;
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
            vlc_object_release( p_input );
            p_input = NULL;
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
                DropPicture( p_vout, p_last_picture );
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
                DropPicture( p_vout, p_picture );
                i_lost++;
                msg_Warn( p_vout, "late picture skipped (%"PRId64")",
                                  current_date - display_date );
                continue;
            }

            if( display_date >
                current_date + p_vout->i_pts_delay + VOUT_BOGUS_DELAY )
            {
                /* Picture is waaay too early: it will be destroyed */
                DropPicture( p_vout, p_picture );
                i_lost++;
                msg_Warn( p_vout, "vout warning: early picture skipped "
                          "(%"PRId64")", display_date - current_date
                          - p_vout->i_pts_delay );
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

        if( p_picture )
        {
            p_picture = filter_chain_VideoFilter( p_vout->p_vf2_chain,
                                                  p_picture );
        }

        if( p_picture && p_vout->b_snapshot )
        {
            p_vout->b_snapshot = false;
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
            p_input ? var_GetBool( p_input, "state" ) == PAUSE_S : false );
            if( p_input )
                vlc_object_release( p_input );
            p_input = NULL;
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

        vlc_object_unlock( p_vout );

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
        vlc_object_lock( p_vout );
        /* Note: vlc_object_alive() could be false here, and we
         * could be dead */
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
            // FIXME pf_end
            p_vout->b_error = 1;
            break;
        }

        if( p_vout->i_changes & VOUT_SIZE_CHANGE )
        {
            /* this must only happen when the vout plugin is incapable of
             * rescaling the picture itself. In this case we need to destroy
             * the current picture buffers and recreate new ones with the right
             * dimensions */
            int i;

            p_vout->i_changes &= ~VOUT_SIZE_CHANGE;

            assert( !p_vout->b_direct );

            ChromaDestroy( p_vout );

            vlc_mutex_lock( &p_vout->picture_lock );

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

            if( !p_vout->b_direct )
                ChromaDestroy( p_vout );

            vlc_mutex_lock( &p_vout->picture_lock );

            p_vout->pf_end( p_vout );

            I_OUTPUTPICTURES = I_RENDERPICTURES = 0;

            p_vout->b_error = InitThread( p_vout );
            if( p_vout->b_error )
                msg_Err( p_vout, "InitThread after VOUT_PICTURE_BUFFERS_CHANGE failed\n" );

            vlc_mutex_unlock( &p_vout->picture_lock );

            if( p_vout->b_error )
                break;
        }

        /* Check for "video filter2" changes */
        vlc_mutex_lock( &p_vout->vfilter_lock );
        if( p_vout->psz_vf2 )
        {
            es_format_t fmt;

            es_format_Init( &fmt, VIDEO_ES, p_vout->fmt_render.i_chroma );
            fmt.video = p_vout->fmt_render;
            filter_chain_Reset( p_vout->p_vf2_chain, &fmt, &fmt );

            if( filter_chain_AppendFromString( p_vout->p_vf2_chain,
                                               p_vout->psz_vf2 ) < 0 )
                msg_Err( p_vout, "Video filter chain creation failed" );

            free( p_vout->psz_vf2 );
            p_vout->psz_vf2 = NULL;
        }
        vlc_mutex_unlock( &p_vout->vfilter_lock );
    }


    if( p_input )
    {
        vlc_object_release( p_input );
    }

    /*
     * Error loop - wait until the thread destruction is requested
     */
    if( p_vout->b_error )
        ErrorThread( p_vout );

    /* End of thread */
    CleanThread( p_vout );
    EndThread( p_vout );
    vlc_mutex_unlock( &p_vout->change_lock );

    vlc_object_unlock( p_vout );
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
    /* Wait until a `die' order */
    while( vlc_object_alive( p_vout ) )
        vlc_object_wait( p_vout );
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

    if( !p_vout->b_direct )
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

    /* Destroy subpicture unit */
    spu_Attach( p_vout->p_spu, VLC_OBJECT(p_vout), false );
    spu_Destroy( p_vout->p_spu );

    /* Destroy the video filters2 */
    filter_chain_Delete( p_vout->p_vf2_chain );
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
    p_chroma = p_vout->p_chroma =
        vlc_custom_create( p_vout, sizeof(filter_t), VLC_OBJECT_GENERIC,
                           typename );

    vlc_object_attach( p_chroma, p_vout );

    /* TODO: Set the fmt_in and fmt_out stuff here */
    p_chroma->fmt_in.video = p_vout->fmt_render;
    p_chroma->fmt_out.video = p_vout->fmt_out;
    VideoFormatImportRgb( &p_chroma->fmt_in.video, &p_vout->render );
    VideoFormatImportRgb( &p_chroma->fmt_out.video, &p_vout->output );

    p_chroma->p_module = module_Need( p_chroma, "video filter2", NULL, 0 );

    if( p_chroma->p_module == NULL )
    {
        msg_Err( p_vout, "no chroma module for %4.4s to %4.4s i=%dx%d o=%dx%d",
                 (char*)&p_vout->render.i_chroma,
                 (char*)&p_vout->output.i_chroma,
                 p_chroma->fmt_in.video.i_width, p_chroma->fmt_in.video.i_height,
                 p_chroma->fmt_out.video.i_width, p_chroma->fmt_out.video.i_height
                 );

        vlc_object_release( p_vout->p_chroma );
        p_vout->p_chroma = NULL;
        return VLC_EGENERIC;
    }
    p_chroma->pf_vout_buffer_new = ChromaGetPicture;
    return VLC_SUCCESS;
}

static void ChromaDestroy( vout_thread_t *p_vout )
{
    assert( !p_vout->b_direct );

    if( !p_vout->p_chroma )
        return;

    module_Unneed( p_vout->p_chroma, p_vout->p_chroma->p_module );
    vlc_object_release( p_vout->p_chroma );
    p_vout->p_chroma = NULL;
}

static void DropPicture( vout_thread_t *p_vout, picture_t *p_picture )
{
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
    vlc_mutex_unlock( &p_vout->picture_lock );
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
 * Helper thread for object variables callbacks.
 * Only used to avoid deadlocks when using the video embedded mode.
 *****************************************************************************/
typedef struct suxor_thread_t
{
    VLC_COMMON_MEMBERS
    input_thread_t *p_input;

} suxor_thread_t;

static void* SuxorRestartVideoES( vlc_object_t * p_vlc_t )
{
    suxor_thread_t *p_this = (suxor_thread_t *) p_vlc_t;
    /* Now restart current video stream */
    int val = var_GetInteger( p_this->p_input, "video-es" );
    if( val >= 0 )
    {
        var_SetInteger( p_this->p_input, "video-es", -VIDEO_ES );
        var_SetInteger( p_this->p_input, "video-es", val );
    }

    vlc_object_release( p_this->p_input );

    vlc_object_release( p_this );
    return NULL;
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

    val.b_bool = true;
    var_Set( p_vout, "intf-change", val );

    val.psz_string = psz_filter;
    var_Set( p_vout, "vout-filter", val );
    free( psz_filter );

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

    val.b_bool = true;
    var_Set( p_vout, "intf-change", val );

    /* Modify input as well because the vout might have to be restarted */
    val.psz_string = newval.psz_string;
    var_Create( p_input, "vout-filter", VLC_VAR_STRING );

    var_Set( p_input, "vout-filter", val );

    /* Now restart current video stream */
    var_Get( p_input, "video-es", &val );
    if( val.i_int >= 0 )
    {
        static const char typename[] = "kludge";
        suxor_thread_t *p_suxor =
            vlc_custom_create( p_vout, sizeof(suxor_thread_t),
                               VLC_OBJECT_GENERIC, typename );
        p_suxor->p_input = p_input;
        p_vout->b_filter_change = true;
        vlc_object_yield( p_input );
        vlc_thread_create( p_suxor, "suxor", SuxorRestartVideoES,
                           VLC_THREAD_PRIORITY_LOW, false );
    }

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

    vlc_mutex_lock( &p_vout->vfilter_lock );
    p_vout->psz_vf2 = strdup( newval.psz_string );
    vlc_mutex_unlock( &p_vout->vfilter_lock );

    return VLC_SUCCESS;
}

static void DisplayTitleOnOSD( vout_thread_t *p_vout )
{
    input_thread_t *p_input;
    mtime_t i_now, i_stop;

    if( !config_GetInt( p_vout, "osd" ) ) return;

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

