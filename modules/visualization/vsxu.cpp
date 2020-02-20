/*****************************************************************************
 * vsxu.cpp: visualization module wrapper for Vovoid VSXu
 *****************************************************************************
 * Copyright © 2009-2012 the VideoLAN team, Vovoid Media Technologies
 *
 * Authors: Rémi Duraffort <ivoire@videolan.org>
 *          Laurent Aimar
 *          Jonatan "jaw" Wallmander
 *
 * Used the projectM implementation as reference for this file.
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_vout_window.h>
#include <vlc_opengl.h>
#include <vlc_filter.h>

// vsxu manager include
#include <vsx_manager.h>
#include <logo_intro.h>

// class to handle cyclic buffer
#include "cyclic_buffer.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );

#define WIDTH_TEXT N_("Video width")
#define WIDTH_LONGTEXT N_("The width of the video window, in pixels.")

#define HEIGHT_TEXT N_("Video height")
#define HEIGHT_LONGTEXT N_("The height of the video window, in pixels.")

vlc_module_begin ()
    set_shortname( N_("vsxu"))
    set_description( N_("vsxu") )
    set_capability( "visualization", 0 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_VISUAL )
    add_integer( "vsxu-width", 1280, WIDTH_TEXT, WIDTH_LONGTEXT,
                 false )
    add_integer( "vsxu-height", 800, HEIGHT_TEXT, HEIGHT_LONGTEXT,
                 false )
    add_shortcut( "vsxu" )
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
namespace {

struct filter_sys_t
{
    vlc_thread_t thread;
    vlc_gl_t *gl;

    vlc_mutex_t lock;

    // mutex around the cyclic block
    vlc_mutex_t cyclic_block_mutex;

    // cyclic buffer to cache sound frames in
    cyclic_block_queue* vsxu_cyclic_buffer;

    int i_channels;

    bool b_quit;
};

} // namespace

static block_t *DoWork( filter_t *, block_t * );
static void *Thread( void * );

/**
 * Open the module
 * @param p_this: the filter object
 * @return VLC_SUCCESS or vlc error codes
 */
static int Open( vlc_object_t * p_this )
{
    filter_t     *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    p_sys = p_filter->p_sys = (filter_sys_t*)malloc( sizeof( *p_sys ) );
    if( unlikely( !p_sys ) )
    {
        return VLC_ENOMEM;
    }

    /* Create the object for the thread */
    p_sys->b_quit        = false;
    p_sys->i_channels    = aout_FormatNbChannels( &p_filter->fmt_in.audio );
    vlc_mutex_init( &p_sys->lock );
    vlc_mutex_init( &p_sys->cyclic_block_mutex );
    p_sys->vsxu_cyclic_buffer = new cyclic_block_queue();

    /* Create the openGL provider */
    vout_window_cfg_t cfg;

    memset( &cfg, 0, sizeof(cfg) );
    cfg.width = var_InheritInteger( p_filter, "vsxu-width" );
    cfg.height = var_InheritInteger( p_filter, "vsxu-height" );

    p_sys->gl = vlc_gl_surface_Create( VLC_OBJECT(p_filter), &cfg, NULL );
    if( p_sys->gl == NULL )
        goto error;

    /* Create the thread */
    if( vlc_clone( &p_sys->thread, Thread, p_filter,
                   VLC_THREAD_PRIORITY_LOW ) )
    {
        vlc_gl_surface_Destroy( p_sys->gl );
        goto error;
    }

    p_filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
    p_filter->fmt_out.audio = p_filter->fmt_in.audio;
    p_filter->pf_audio_filter = DoWork;

    return VLC_SUCCESS;

error:
    free( p_sys );
    return VLC_EGENERIC;
}

/**
 * Close the module
 * @param p_this: the filter object
 */
static void Close( vlc_object_t *p_this )
{
    filter_t  *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    vlc_mutex_lock( &p_sys->lock );
    p_sys->b_quit = true;
    vlc_mutex_unlock( &p_sys->lock );

    vlc_join( p_sys->thread, NULL );

    /* Free the ressources */
    vlc_gl_surface_Destroy( p_sys->gl );
    delete p_sys->vsxu_cyclic_buffer;
    free( p_sys );
}

/**
 * Do the actual work with the new sample
 * @param p_filter: filter object
 * @param p_in_buf: input buffer
 */
static block_t *DoWork( filter_t *p_filter, block_t *p_in_buf )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    vlc_mutex_lock( &p_sys->lock );
    vlc_mutex_lock( &p_sys->cyclic_block_mutex );

    unsigned i_nb_samples = __MIN( 1024,
                                 p_in_buf->i_nb_samples );

    const float *p_src = (float*)p_in_buf->p_buffer;
    // iterate block holder
    size_t i_bh_data_iter = 0;

    // calc 512-byte-aligned sample count to grab, we don't need more
    unsigned i_num_samples = i_nb_samples - i_nb_samples % 512;

    // muls are cheaper than divs
    float f_onedivchannels = 1.0f / (float)p_sys->i_channels;

    block_holder* p_block_holder = p_sys->vsxu_cyclic_buffer->get_insertion_object();
    p_block_holder->pts = p_in_buf->i_pts;
    for( unsigned i = 0; i < i_num_samples; i++ )
    {
        float f_v = 0;
        for( int j = 0; j < p_sys->i_channels; j++ )
        {
            f_v += p_src[p_sys->i_channels * i + j];
        }

        // insert into our little cyclic buffer
        p_block_holder->data[i_bh_data_iter] = f_v * f_onedivchannels;
        i_bh_data_iter++;
        if (i_bh_data_iter == 512 && i < i_num_samples-256)
        {
            p_block_holder = p_sys->vsxu_cyclic_buffer->get_insertion_object();
            p_block_holder->pts = p_in_buf->i_pts + 11609;

            i_bh_data_iter = 0;
        }
    }

    vlc_mutex_unlock( &p_sys->cyclic_block_mutex );
    vlc_mutex_unlock( &p_sys->lock );
    return p_in_buf;
}

/**
 * VSXu update thread which do the rendering
 * @param p_this: the p_thread object
 */
static void *Thread( void *p_data )
{
    filter_t  *p_filter = (filter_t*)p_data;
    filter_sys_t *p_sys = p_filter->p_sys;
    vlc_gl_t *gl = p_sys->gl;

    // our abstract manager holder
    vsx_manager_abs* manager = 0;

    // temp audio buffer for sending to vsxu through manager
    float f_sample_buf[512];

    // vsxu logo intro
    vsx_logo_intro* intro = 0;

    bool first = true;
    bool run = true;

    // tell main thread we are ready
    if( vlc_gl_MakeCurrent( gl ) != VLC_SUCCESS )
    {
        msg_Err( p_filter, "Can't attach gl context" );
        return NULL;
    }

    while ( run )
    {
        /* Manage the events */
        unsigned width, height;

        if( vlc_gl_surface_CheckSize( p_sys->gl, &width, &height ) )
        {
            /* ??? */
        }

        // look for control commands from outside the thread
        vlc_mutex_lock( &p_sys->lock );
            if( p_sys->b_quit )
            {
                run = false;
            }
        vlc_mutex_unlock( &p_sys->lock );

        if (first)
        {
            // only run this once
            first = false;

            // create a new manager
            manager = manager_factory();

            // init manager with the shared path and sound input type.
            manager->init( 0, "media_player" );

            // only show logo once
            // keep track of iterations
            static int i_iterations = 0;
            if ( i_iterations++ < 1 )
            {
                intro = new vsx_logo_intro();
                intro->set_destroy_textures( false );
            }
        }

        // lock cyclic buffer mutex and copy floats
        vlc_mutex_lock( &p_sys->cyclic_block_mutex );
            block_holder* bh = p_sys->vsxu_cyclic_buffer->consume();
            memcpy( &f_sample_buf[0], (void*)(&bh->data[0]), sizeof(float) * 512 );
        vlc_mutex_unlock( &p_sys->cyclic_block_mutex );

        // send sound pointer to vsxu
        manager->set_sound_wave( &f_sample_buf[0] );

        // render vsxu engine
        if (manager) manager->render();

        // render intro
        if (intro) intro->draw();

        // swap buffers etc.
        vlc_gl_Swap( gl );
    }

    // stop vsxu nicely (unloads textures and frees memory)
    if (manager) manager->stop();

    // call manager factory to destruct our manager object
    if (manager) manager_destroy( manager );

    // delete the intro (if ever allocated)
    if (intro) delete intro;

    vlc_gl_ReleaseCurrent( gl );

    // clean up the cyclic buffer
    vlc_mutex_lock( &p_sys->cyclic_block_mutex );
        p_sys->vsxu_cyclic_buffer->reset();
    vlc_mutex_unlock( &p_sys->cyclic_block_mutex );

    // die
    return NULL;
}

