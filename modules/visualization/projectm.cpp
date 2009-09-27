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
    set_capability( "visualization", 0 )
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
typedef struct
{
    VLC_COMMON_MEMBERS

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
    float *p_buffer;
    int   i_buffer_size;
    int   i_nb_samples;

    vlc_mutex_t lock;
} projectm_thread_t;


struct aout_filter_sys_t
{
    projectm_thread_t *p_thread;
};


static void DoWork( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                    aout_buffer_t * );
static void* Thread( vlc_object_t * );


/**
 * Init the openGL context
 * p_thread: projectm thread object
 * @return VLC_SUCCESS or vlc error codes
 */
static int initOpenGL( projectm_thread_t *p_thread )
{
    p_thread->p_opengl = (vout_thread_t *)vlc_object_create( p_thread,
                                                    sizeof( vout_thread_t ) );
    if( !p_thread->p_opengl )
        return VLC_ENOMEM;

    vlc_object_attach( p_thread->p_opengl, p_thread );

    /* Initialize the opengl object */
    video_format_Setup( &p_thread->p_opengl->fmt_in, VLC_CODEC_RGB32,
                        p_thread->i_width, p_thread->i_height, 1 );
    p_thread->p_opengl->i_window_width = p_thread->i_width;
    p_thread->p_opengl->i_window_height = p_thread->i_height;
    p_thread->p_opengl->render.i_width = p_thread->i_width;
    p_thread->p_opengl->render.i_height = p_thread->i_height;
    p_thread->p_opengl->render.i_aspect = VOUT_ASPECT_FACTOR;
    p_thread->p_opengl->b_fullscreen = false;
    p_thread->p_opengl->b_autoscale = true;
    p_thread->p_opengl->i_alignment = 0;
    p_thread->p_opengl->fmt_in.i_sar_num = 1;
    p_thread->p_opengl->fmt_in.i_sar_den = 1;
    p_thread->p_opengl->fmt_render = p_thread->p_opengl->fmt_in;

    /* Ask for the opengl provider */
    p_thread->p_module = module_need( p_thread->p_opengl, "opengl provider",
                                      NULL, false );
    if( !p_thread->p_module )
    {
        msg_Err( p_thread, "unable to initialize OpenGL" );
        vlc_object_detach( p_thread->p_opengl );
        vlc_object_release( p_thread->p_opengl );
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
    aout_filter_t       *p_filter = (aout_filter_t *)p_this;
    aout_filter_sys_t   *p_sys;
    projectm_thread_t   *p_thread;

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

    p_filter->pf_do_work = DoWork;
    p_filter->b_in_place = true;

    p_sys = p_filter->p_sys = (aout_filter_sys_t*)malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    /* Create the object for the thread */
    p_sys->p_thread = p_thread = (projectm_thread_t *)
                    vlc_object_create( p_filter, sizeof( projectm_thread_t ) );
    vlc_object_attach( p_sys->p_thread, p_filter );
    p_thread->i_width  = var_CreateGetInteger( p_filter, "projectm-width" );
    p_thread->i_height = var_CreateGetInteger( p_filter, "projectm-height" );

    /* Create the openGL provider */
    int i_ret = initOpenGL( p_sys->p_thread );
    if( i_ret != VLC_SUCCESS )
    {
        vlc_object_detach( p_sys->p_thread );
        vlc_object_release( p_sys->p_thread );
        free( p_sys );
        return i_ret;
    }

    p_thread->i_channels = aout_FormatNbChannels( &p_filter->fmt_in.audio );
    p_thread->psz_config = var_CreateGetString( p_filter, "projectm-config" );
    vlc_mutex_init( &p_thread->lock );
    p_thread->p_buffer = NULL;
    p_thread->i_buffer_size = 0;
    p_thread->i_nb_samples = 0;

    /* Create the thread */
    if( vlc_thread_create( p_thread, "projectm update thread", Thread,
                           VLC_THREAD_PRIORITY_LOW ) )
    {
        msg_Err( p_filter, "cannot launch the projectm thread" );
        vlc_object_detach( p_thread );
        vlc_object_release( p_thread );
        free (p_sys );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}


/**
 * Close the module
 * @param p_this: the filter object
 */
static void Close( vlc_object_t *p_this )
{
    aout_filter_t     *p_filter = (aout_filter_t *)p_this;
    aout_filter_sys_t *p_sys = p_filter->p_sys;
    projectm_thread_t *p_thread = p_sys->p_thread;

    /* Stop the thread */
    vlc_object_kill( p_thread );
    vlc_thread_join( p_thread );

    /* Free the ressources */
    vlc_mutex_destroy( &p_thread->lock );
    free( p_thread->p_buffer );
    free( p_thread->psz_config );

    vlc_object_detach( p_thread );
    vlc_object_release( p_thread );

    free( p_sys );
}


/**
 * Do the actual work with the new sample
 * @param p_aout: audio output object
 * @param p_filter: filter object
 * @param p_in_buf: input buffer
 * @param p_out_buf: output buffer
 */
static void DoWork( aout_instance_t *p_aout, aout_filter_t *p_filter,
                    aout_buffer_t *p_in_buf, aout_buffer_t *p_out_buf )
{
    projectm_thread_t *p_thread = p_filter->p_sys->p_thread;

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_buffer = p_in_buf->i_buffer;

    vlc_mutex_lock( &p_thread->lock );
    if( p_thread->i_buffer_size > 0 )
    {
        p_thread->p_buffer[0] = 0;
        p_thread->i_nb_samples = __MIN( p_thread->i_buffer_size,
                                        p_in_buf->i_nb_samples );
        for( int i = 0; i < p_thread->i_nb_samples; i++ )
            p_thread->p_buffer[i] = p_in_buf->p_buffer[i];
    }

    vlc_mutex_unlock( &p_thread->lock );

    return;
}


/**
 * ProjectM update thread which do the rendering
 * @param p_this: the p_thread object
 */
static void* Thread( vlc_object_t *p_this )
{
    /* we don't want to be interupted in this thread */
    int cancel = vlc_savecancel();
    projectm_thread_t *p_thread = (projectm_thread_t *)p_this;

    /* Initialize the opengl provider for this thread */
    p_thread->p_opengl->pf_init( p_thread->p_opengl );

    /* Create the projectM object */
    p_thread->p_projectm = new projectM( p_thread->psz_config );
    p_thread->i_buffer_size = p_thread->p_projectm->pcm()->maxsamples;
    p_thread->p_buffer = (float*)malloc( p_thread->i_buffer_size *
                                         sizeof( float ) );

    /* TODO: Give to projectm the name of the input
    p_thread->p_projectm->projectM_setTitle( "" ); */

    /* Reset the dislay to get the right size */
    p_thread->p_projectm->projectM_resetGL( p_thread->i_width,
                                            p_thread->i_height );

    while( vlc_object_alive( p_thread ) )
    {
        /* Manage the events */
        p_thread->p_opengl->pf_manage( p_thread->p_opengl );
        /* Render the image and swap the buffers */
        vlc_mutex_lock( &p_thread->lock );
        if( p_thread->i_nb_samples > 0 )
            p_thread->p_projectm->pcm()->addPCMfloat( p_thread->p_buffer,
                                                      p_thread->i_nb_samples );

        p_thread->p_projectm->renderFrame();
        p_thread->p_opengl->pf_swap( p_thread->p_opengl );
        vlc_mutex_unlock( &p_thread->lock );

        /* TODO: use a fps limiter */
        msleep( 10000 );
    }


    /* Cleanup */
    delete p_thread->p_projectm;

    /* Free the openGL provider */
    module_unneed( p_thread->p_opengl, p_thread->p_module );
    vlc_object_detach( p_thread->p_opengl );
    vlc_object_release( p_thread->p_opengl );


    vlc_restorecancel( cancel );
    return NULL;
}
