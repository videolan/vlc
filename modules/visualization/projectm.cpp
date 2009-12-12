/*****************************************************************************
 * projectm: visualization module based on libprojectM
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 * $Id$
 *
 * Authors: Rémi Duraffort <ivoire@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_vout.h>
#include <vlc_filter.h>

#include <libprojectM/projectM.hpp>


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );

#define CONFIG_TEXT N_("projectM configuration file")
#define CONFIG_LONGTEXT N_("File that will be used to configure the projectM " \
                           "module.")

#define WIDTH_TEXT N_("Video width")
#define WIDTH_LONGTEXT N_("The width of the video window, in pixels.")

#define HEIGHT_TEXT N_("Video height")
#define HEIGHT_LONGTEXT N_("The height of the video window, in pixels.")

vlc_module_begin ()
    set_shortname( N_("projectM"))
    set_description( N_("libprojectM effect") )
    set_capability( "visualization2", 0 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_VISUAL )
    add_file( "projectm-config", "/usr/share/projectM/config.inp", NULL,
                CONFIG_TEXT, CONFIG_LONGTEXT, true )
    add_integer( "projectm-width", 800, NULL, WIDTH_TEXT, WIDTH_LONGTEXT,
                 false )
    add_integer( "projectm-height", 640, NULL, HEIGHT_TEXT, HEIGHT_LONGTEXT,
                 false )
    add_shortcut( "projectm" )
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct filter_sys_t
{
    /* */
    vlc_thread_t thread;
    vlc_sem_t    ready;
    bool         b_error;

    /* video output module and opengl provider */
    vout_thread_t *p_opengl;
    module_t      *p_module;

    /* libprojectM objects */
    projectM      *p_projectm;
    char          *psz_config;

    /* Window size */
    int i_width;
    int i_height;

    /* audio info */
    int i_channels;

    vlc_mutex_t lock;
    float *p_buffer;
    int   i_buffer_size;
    int   i_nb_samples;
};


static block_t *DoWork( filter_t *, block_t * );
static void *Thread( void * );


/**
 * Init the openGL context
 * p_thread: projectm thread object
 * @return VLC_SUCCESS or vlc error codes
 */
static int initOpenGL( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    p_sys->p_opengl =
        (vout_thread_t *)vlc_object_create( p_filter, sizeof(vout_thread_t) );
    if( !p_sys->p_opengl )
        return VLC_ENOMEM;

    vlc_object_attach( p_sys->p_opengl, p_filter );

    /* Initialize the opengl object */
    video_format_Setup( &p_sys->p_opengl->fmt_in, VLC_CODEC_RGB32,
                        p_sys->i_width, p_sys->i_height, 1 );
    p_sys->p_opengl->i_window_width = p_sys->i_width;
    p_sys->p_opengl->i_window_height = p_sys->i_height;
    p_sys->p_opengl->render.i_width = p_sys->i_width;
    p_sys->p_opengl->render.i_height = p_sys->i_height;
    p_sys->p_opengl->render.i_aspect = VOUT_ASPECT_FACTOR;
    p_sys->p_opengl->b_fullscreen = false;
    p_sys->p_opengl->b_autoscale = true;
    p_sys->p_opengl->i_alignment = 0;
    p_sys->p_opengl->fmt_in.i_sar_num = 1;
    p_sys->p_opengl->fmt_in.i_sar_den = 1;
    p_sys->p_opengl->fmt_render = p_sys->p_opengl->fmt_in;

    /* Ask for the opengl provider */
    p_sys->p_module = module_need( p_sys->p_opengl, "opengl provider",
                                   NULL, false );
    if( !p_sys->p_module )
    {
        msg_Err( p_filter, "unable to initialize OpenGL" );
        vlc_object_detach( p_sys->p_opengl );
        vlc_object_release( p_sys->p_opengl );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}


/**
 * Open the module
 * @param p_this: the filter object
 * @return VLC_SUCCESS or vlc error codes
 */
static int Open( vlc_object_t * p_this )
{
    filter_t     *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    /* Test the audio format */
    if( p_filter->fmt_in.audio.i_format != VLC_CODEC_FL32 ||
        p_filter->fmt_out.audio.i_format != VLC_CODEC_FL32 )
    {
        msg_Warn( p_filter, "bad input or output format" );
        return VLC_EGENERIC;
    }
    if( !AOUT_FMTS_SIMILAR( &p_filter->fmt_in.audio, &p_filter->fmt_out.audio ) )
    {
        msg_Warn( p_filter, "input and outut are not similar" );
        return VLC_EGENERIC;
    }

    p_filter->pf_audio_filter = DoWork;

    p_sys = p_filter->p_sys = (filter_sys_t*)malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    /* Create the object for the thread */
    vlc_sem_init( &p_sys->ready, 0 );
    p_sys->b_error  = false;
    p_sys->i_width  = var_CreateGetInteger( p_filter, "projectm-width" );
    p_sys->i_height = var_CreateGetInteger( p_filter, "projectm-height" );
    p_sys->i_channels = aout_FormatNbChannels( &p_filter->fmt_in.audio );
    p_sys->psz_config = var_CreateGetString( p_filter, "projectm-config" );
    vlc_mutex_init( &p_sys->lock );
    p_sys->p_buffer = NULL;
    p_sys->i_buffer_size = 0;
    p_sys->i_nb_samples = 0;

    /* Create the thread */
    if( vlc_clone( &p_sys->thread, Thread, p_filter, VLC_THREAD_PRIORITY_LOW ) )
        goto error;

    vlc_sem_wait( &p_sys->ready );
    if( p_sys->b_error )
    {
        vlc_join( p_sys->thread, NULL );
        goto error;
    }

    return VLC_SUCCESS;

error:
    vlc_sem_destroy( &p_sys->ready );
    free (p_sys );
    return VLC_EGENERIC;
}


/**
 * Close the module
 * @param p_this: the filter object
 */
static void Close( vlc_object_t *p_this )
{
    filter_t     *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    /* Stop the thread */
    vlc_cancel( p_sys->thread );
    vlc_join( p_sys->thread, NULL );

    /* Free the ressources */
    vlc_sem_destroy( &p_sys->ready );
    vlc_mutex_destroy( &p_sys->lock );
    free( p_sys->p_buffer );
    free( p_sys->psz_config );
    free( p_sys );
}


/**
 * Do the actual work with the new sample
 * @param p_aout: audio output object
 * @param p_filter: filter object
 * @param p_in_buf: input buffer
 * @param p_out_buf: output buffer
 */
static block_t *DoWork( filter_t *p_filter, block_t *p_in_buf )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    vlc_mutex_lock( &p_sys->lock );
    if( p_sys->i_buffer_size > 0 )
    {
        p_sys->p_buffer[0] = 0;
        p_sys->i_nb_samples = __MIN( p_sys->i_buffer_size,
                                     p_in_buf->i_nb_samples );
        for( int i = 0; i < p_sys->i_nb_samples; i++ )
            p_sys->p_buffer[i] = p_in_buf->p_buffer[i];
    }
    vlc_mutex_unlock( &p_sys->lock );

    return p_in_buf;
}

/**
 * Clean up function when Thread() is cancelled.
 */
static void ThreadCleanup( void *p_data )
{
    filter_t     *p_filter = (filter_t*)p_data;
    filter_sys_t *p_sys = p_filter->p_sys;

    /* Cleanup */
    delete p_sys->p_projectm;

    /* Free the openGL provider */
    module_unneed( p_sys->p_opengl, p_sys->p_module );
    vlc_object_detach( p_sys->p_opengl );
    vlc_object_release( p_sys->p_opengl );
}

/**
 * ProjectM update thread which do the rendering
 * @param p_this: the p_thread object
 */
static void *Thread( void *p_data )
{
    filter_t     *p_filter = (filter_t*)p_data;
    filter_sys_t *p_sys = p_filter->p_sys;
    int cancel = vlc_savecancel();

    /* Create the openGL provider */
    if( initOpenGL( p_filter ) )
    {
        p_sys->b_error = true;
        vlc_sem_post( &p_sys->ready );
        return NULL;
    }
    vlc_cleanup_push( ThreadCleanup, p_filter );

    /* Initialize the opengl provider for this thread */
    p_sys->p_opengl->pf_init( p_sys->p_opengl );

    /* Create the projectM object */
    p_sys->p_projectm = new projectM( p_sys->psz_config );
    p_sys->i_buffer_size = p_sys->p_projectm->pcm()->maxsamples;
    p_sys->p_buffer = (float*)calloc( p_sys->i_buffer_size,
                                      sizeof( float ) );

    vlc_sem_post( &p_sys->ready );

    /* TODO: Give to projectm the name of the input
    p_sys->p_projectm->projectM_setTitle( "" ); */

    /* Reset the dislay to get the right size */
    p_sys->p_projectm->projectM_resetGL( p_sys->i_width,
                                         p_sys->i_height );

    for( ;; )
    {
        /* Manage the events */
        p_sys->p_opengl->pf_manage( p_sys->p_opengl );
        /* Render the image and swap the buffers */
        vlc_mutex_lock( &p_sys->lock );
        if( p_sys->i_nb_samples > 0 )
            p_sys->p_projectm->pcm()->addPCMfloat( p_sys->p_buffer,
                                                   p_sys->i_nb_samples );

        p_sys->p_projectm->renderFrame();
        p_sys->p_opengl->pf_swap( p_sys->p_opengl );
        vlc_mutex_unlock( &p_sys->lock );

        /* TODO: use a fps limiter */
        vlc_restorecancel( cancel );
        msleep( 10000 );
        cancel = vlc_savecancel();
    }
    vlc_cleanup_pop();
}

