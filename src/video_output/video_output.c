/*****************************************************************************
 * video_output.c : video output thread
 *
 * This module describes the programming interface for video output threads.
 * It includes functions allowing to open a new thread, send pictures to a
 * thread, and destroy a previously oppened video output thread.
 *****************************************************************************
 * Copyright (C) 2000-2004 the VideoLAN team
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                                /* free() */

#include <vlc/vlc.h>

#ifdef HAVE_SYS_TIMES_H
#   include <sys/times.h>
#endif

#include "vlc_video.h"
#include "video_output.h"
#include "vlc_spu.h"
#include <vlc/input.h>                 /* for input_thread_t and i_pts_delay */
#include "vlc_playlist.h"

#if defined( SYS_DARWIN )
#include "darwin_specific.h"
#endif

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
static void     InitWindowSize    ( vout_thread_t *, unsigned *, unsigned * );

/* Object variables callbacks */
static int DeinterlaceCallback( vlc_object_t *, char const *,
                                vlc_value_t, vlc_value_t, void * );
static int FilterCallback( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );

/* From vout_intf.c */
int vout_Snapshot( vout_thread_t *, picture_t * );

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
        /* Reattach video output to input before bailing out */
        if( p_vout )
        {
            vlc_object_t *p_playlist;

            p_playlist = vlc_object_find( p_this, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );

            if( p_playlist )
            {
                spu_Attach( p_vout->p_spu, p_this, VLC_FALSE );
                vlc_object_detach( p_vout );
                vlc_object_attach( p_vout, p_playlist );

                vlc_object_release( p_playlist );
            }
            else
            {
                msg_Dbg( p_this, "cannot find playlist, destroying vout" );
                vlc_object_detach( p_vout );
                vout_Destroy( p_vout );
            }
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
            playlist_t *p_playlist;

            p_playlist = vlc_object_find( p_this,
                                          VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
            if( p_playlist )
            {
                vlc_mutex_lock( &p_playlist->gc_lock );
                p_vout = vlc_object_find( p_playlist,
                                          VLC_OBJECT_VOUT, FIND_CHILD );
                /* only first children of p_input for unused vout */
                if( p_vout && p_vout->p_parent != (vlc_object_t *)p_playlist )
                {
                    vlc_object_release( p_vout );
                    p_vout = NULL;
                }
                vlc_mutex_unlock( &p_playlist->gc_lock );
                vlc_object_release( p_playlist );
            }
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
            ( p_vout->fmt_render.i_chroma != p_fmt->i_chroma ) ||
            ( p_vout->fmt_render.i_aspect != p_fmt->i_aspect
                    && !p_vout->b_override_aspect ) ||
            p_vout->b_filter_change )
        {
            /* We are not interested in this format, close this vout */
            vlc_object_detach( p_vout );
            vlc_object_release( p_vout );
            vout_Destroy( p_vout );
            p_vout = NULL;
        }
        else
        {
            /* This video output is cool! Hijack it. */
            vlc_object_detach( p_vout );
            spu_Attach( p_vout->p_spu, p_this, VLC_TRUE );
            vlc_object_attach( p_vout, p_this );
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
    char           * psz_plugin;
    vlc_value_t      val, val2, text;

    unsigned int i_width = p_fmt->i_width;
    unsigned int i_height = p_fmt->i_height;
    vlc_fourcc_t i_chroma = p_fmt->i_chroma;
    unsigned int i_aspect = p_fmt->i_aspect;

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

    /* Initialize locks */
    vlc_mutex_init( p_vout, &p_vout->picture_lock );
    vlc_mutex_init( p_vout, &p_vout->change_lock );

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

    p_vout->b_override_aspect = VLC_FALSE;

    /* If the parent is not a VOUT object, that means we are at the start of
     * the video output pipe */
    if( p_parent->i_object_type != VLC_OBJECT_VOUT )
    {
        int i_screen_aspect_x = 4 , i_screen_aspect_y = 3;
        var_Get( p_vout, "aspect-ratio", &val );
        var_Get( p_vout, "screen-aspect-ratio", &val2 );

        if( val2.psz_string )
        {
            char *psz_parser = strchr( val2.psz_string, ':' );
            if( psz_parser )
            {
                *psz_parser++ = '\0';
                i_screen_aspect_x = atoi( val2.psz_string );
                i_screen_aspect_y = atoi( psz_parser );
            } else {
                AspectRatio( VOUT_ASPECT_FACTOR * atof( val2.psz_string ),
                                 &i_screen_aspect_x, &i_screen_aspect_y );
            }

            free( val2.psz_string );
        }

        /* Check whether the user tried to override aspect ratio */
        if( val.psz_string )
        {
            unsigned int i_new_aspect = i_aspect;
            char *psz_parser = strchr( val.psz_string, ':' );
                int i_aspect_x, i_aspect_y;
                AspectRatio( i_aspect, &i_aspect_x, &i_aspect_y );

            if( psz_parser )
            {
                *psz_parser++ = '\0';
                i_new_aspect = atoi( val.psz_string )
                               * VOUT_ASPECT_FACTOR / atoi( psz_parser );
            }
            else
            {
                if( atof( val.psz_string ) != 0 )
                {
                i_new_aspect = VOUT_ASPECT_FACTOR * atof( val.psz_string );
                }
            }
            i_new_aspect = (int)((float)i_new_aspect
                * (float)i_screen_aspect_y*4.0/((float)i_screen_aspect_x*3.0));

            free( val.psz_string );

            if( i_new_aspect && i_new_aspect != i_aspect )
            {
                int i_aspect_x, i_aspect_y;

                AspectRatio( i_new_aspect, &i_aspect_x, &i_aspect_y );

                msg_Dbg( p_vout, "overriding source aspect ratio to %i:%i",
                         i_aspect_x, i_aspect_y );

                p_vout->render.i_aspect = i_new_aspect;

                p_vout->b_override_aspect = VLC_TRUE;
            }
        }

        /* Look for the default filter configuration */
        var_Create( p_vout, "vout-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        var_Get( p_vout, "vout-filter", &val );
        p_vout->psz_filter_chain = val.psz_string;
    }
    else
    {
        /* continue the parent's filter chain */
        char *psz_end;

        psz_end = strchr( ((vout_thread_t *)p_parent)->psz_filter_chain, ':' );
        if( psz_end && *(psz_end+1) )
            p_vout->psz_filter_chain = strdup( psz_end+1 );
        else p_vout->psz_filter_chain = NULL;
    }

    /* Choose the video output module */
    if( !p_vout->psz_filter_chain || !*p_vout->psz_filter_chain )
    {
        var_Create( p_vout, "vout", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        var_Get( p_vout, "vout", &val );
        psz_plugin = val.psz_string;
    }
    else
    {
        /* the filter chain is a string list of filters separated by double
         * colons */
        char *psz_end;

        psz_end = strchr( p_vout->psz_filter_chain, ':' );
        if( psz_end )
            psz_plugin = strndup( p_vout->psz_filter_chain,
                                  psz_end - p_vout->psz_filter_chain );
        else psz_plugin = strdup( p_vout->psz_filter_chain );
    }

    /* Initialize the dimensions of the video window */
    InitWindowSize( p_vout, &p_vout->i_window_width,
                    &p_vout->i_window_height );

    /* Create the vout thread */
    p_vout->p_module = module_Need( p_vout,
        ( p_vout->psz_filter_chain && *p_vout->psz_filter_chain ) ?
        "video filter" : "video output", psz_plugin, 0 );

    if( psz_plugin ) free( psz_plugin );
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
    val.psz_string = ""; text.psz_string = _("Disable");
    var_Change( p_vout, "deinterlace", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "discard"; text.psz_string = _("Discard");
    var_Change( p_vout, "deinterlace", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "blend"; text.psz_string = _("Blend");
    var_Change( p_vout, "deinterlace", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "mean"; text.psz_string = _("Mean");
    var_Change( p_vout, "deinterlace", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "bob"; text.psz_string = _("Bob");
    var_Change( p_vout, "deinterlace", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "linear"; text.psz_string = _("Linear");
    var_Change( p_vout, "deinterlace", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "x"; text.psz_string = "X";
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
        p_vout->b_die = VLC_TRUE;

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
    vlc_object_t *p_playlist;

    /* Request thread destruction */
    p_vout->b_die = VLC_TRUE;
    vlc_thread_join( p_vout );

    var_Destroy( p_vout, "intf-change" );

    p_playlist = vlc_object_find( p_vout, VLC_OBJECT_PLAYLIST,
                                  FIND_ANYWHERE );

    if( p_vout->psz_filter_chain ) free( p_vout->psz_filter_chain );

    /* Free structure */
    vlc_object_destroy( p_vout );

    /* If it was the last vout, tell the interface to show up */
    if( p_playlist != NULL )
    {
        vout_thread_t *p_another_vout = vlc_object_find( p_playlist,
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
        vlc_object_release( p_playlist );
    }
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

    msg_Dbg( p_vout, "picture out %ix%i, chroma %4.4s, ar %i:%i, sar %i:%i",
             p_vout->output.i_width, p_vout->output.i_height,
             (char*)&p_vout->output.i_chroma,
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

    /*
     * Initialize thread
     */
    p_vout->b_error = InitThread( p_vout );

    /* signal the creation of the vout */
    vlc_thread_ready( p_vout );

    if( p_vout->b_error )
    {
        /* Destroy thread structures allocated by Create and InitThread */
        DestroyThread( p_vout );
        return;
    }

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
                display_date < current_date + p_vout->render_time )
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
            p_subpic = spu_SortSubpictures( p_vout->p_spu, display_date );
        }

        /*
         * Perform rendering
         */
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
    i_low =  i_mask & (- (int32_t)i_mask);          /* lower bit of the mask */
    i_high = i_mask + i_low;                       /* higher bit of the mask */

    /* Transform bits into an index */
    i_low =  BinaryLog (i_low);
    i_high = BinaryLog (i_high);

    /* Update pointers and return */
    *pi_left =   i_low;
    *pi_right = (8 - i_high + i_low);
}

/*****************************************************************************
 * InitWindowSize: find the initial dimensions the video window should have.
 *****************************************************************************
 * This function will check the "width", "height" and "zoom" config options and
 * will calculate the size that the video window should have.
 *****************************************************************************/
static void InitWindowSize( vout_thread_t *p_vout, unsigned *pi_width,
                            unsigned *pi_height )
{
    vlc_value_t val;
    int i_width, i_height;
    uint64_t ll_zoom;

#define FP_FACTOR 1000                             /* our fixed point factor */

    var_Get( p_vout, "align", &val );
    p_vout->i_alignment = val.i_int;

    var_Get( p_vout, "width", &val );
    i_width = val.i_int;
    var_Get( p_vout, "height", &val );
    i_height = val.i_int;
    var_Get( p_vout, "zoom", &val );
    ll_zoom = (uint64_t)( FP_FACTOR * val.f_float );

    if( i_width > 0 && i_height > 0)
    {
        *pi_width = (int)( i_width * ll_zoom / FP_FACTOR );
        *pi_height = (int)( i_height * ll_zoom / FP_FACTOR );
        return;
    }
    else if( i_width > 0 )
    {
        *pi_width = (int)( i_width * ll_zoom / FP_FACTOR );
        *pi_height = (int)( i_width * ll_zoom * VOUT_ASPECT_FACTOR /
                            p_vout->render.i_aspect / FP_FACTOR );
        return;
    }
    else if( i_height > 0 )
    {
        *pi_height = (int)( i_height * ll_zoom / FP_FACTOR );
        *pi_width = (int)( i_height * ll_zoom * p_vout->render.i_aspect /
                           VOUT_ASPECT_FACTOR / FP_FACTOR );
        return;
    }

    if( p_vout->render.i_height * p_vout->render.i_aspect
        >= p_vout->render.i_width * VOUT_ASPECT_FACTOR )
    {
        *pi_width = (int)( p_vout->render.i_height * ll_zoom
          * p_vout->render.i_aspect / VOUT_ASPECT_FACTOR / FP_FACTOR );
        *pi_height = (int)( p_vout->render.i_height * ll_zoom / FP_FACTOR );
    }
    else
    {
        *pi_width = (int)( p_vout->render.i_width * ll_zoom / FP_FACTOR );
        *pi_height = (int)( p_vout->render.i_width * ll_zoom
          * VOUT_ASPECT_FACTOR / p_vout->render.i_aspect / FP_FACTOR );
    }

#undef FP_FACTOR
}

/*****************************************************************************
 * vout_VarCallback: generic callback for intf variables
 *****************************************************************************/
int vout_VarCallback( vlc_object_t * p_this, const char * psz_variable,
                      vlc_value_t old_value, vlc_value_t new_value,
                      void * unused )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;
    vlc_value_t val;
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

#ifdef WIN32
    CloseHandle( p_this->thread_id );
#endif

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
