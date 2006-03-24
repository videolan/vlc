/*****************************************************************************
 * plugin.c:
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
 *          Implementation of the winamp plugin MilkDrop
 *          based on projectM http://xmms-projectm.sourceforge.net
 *          and SciVi http://xmms-scivi.sourceforge.net
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

#include "plugin.h"
#include "main.h"
#include "PCM.h"
#include "video_init.h"
#include <GL/glu.h>

#include <vlc/input.h>
#include <vlc/vout.h>
#include "aout_internal.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );

vlc_module_begin();
    set_description( _("GaLaktos visualization plugin") );
    set_capability( "visualization", 0 );
    set_callbacks( Open, Close );
    add_shortcut( "galaktos" );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct aout_filter_sys_t
{
    galaktos_thread_t *p_thread;

} aout_filter_sys_t;

static void DoWork   ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                       aout_buffer_t * );

static void Thread   ( vlc_object_t * );

static char *TitleGet( vlc_object_t * );


extern GLuint RenderTargetTextureID;

/*****************************************************************************
 * Open: open a scope effect plugin
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    aout_filter_t     *p_filter = (aout_filter_t *)p_this;
    aout_filter_sys_t *p_sys;
    galaktos_thread_t *p_thread;

    if ( p_filter->input.i_format != VLC_FOURCC('f','l','3','2' )
         || p_filter->output.i_format != VLC_FOURCC('f','l','3','2') )
    {
        msg_Warn( p_filter, "bad input or output format" );
        return VLC_EGENERIC;
    }
    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
    {
        msg_Warn( p_filter, "input and output formats are not similar" );
        return VLC_EGENERIC;
    }

    p_filter->pf_do_work = DoWork;
    p_filter->b_in_place = 1;

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( aout_filter_sys_t ) );

    /* Create galaktos thread */
    p_sys->p_thread = p_thread =
        vlc_object_create( p_filter, sizeof( galaktos_thread_t ) );
    vlc_object_attach( p_thread, p_this );

/*
    var_Create( p_thread, "galaktos-width", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    var_Get( p_thread, "galaktos-width", &width );
    var_Create( p_thread, "galaktos-height", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    var_Get( p_thread, "galaktos-height", &height );
*/
    p_thread->i_cur_sample = 0;
    bzero( p_thread->p_data, 2*2*512 );

    p_thread->i_width = 600;
    p_thread->i_height = 600;
    p_thread->b_fullscreen = 0;
    galaktos_init( p_thread );

    p_thread->i_channels = aout_FormatNbChannels( &p_filter->input );

    p_thread->psz_title = TitleGet( VLC_OBJECT( p_filter ) );

    if( vlc_thread_create( p_thread, "galaktos update thread", Thread,
                           VLC_THREAD_PRIORITY_LOW, VLC_FALSE ) )
    {
        msg_Err( p_filter, "cannot lauch galaktos thread" );
        if( p_thread->psz_title ) free( p_thread->psz_title );
        vlc_object_detach( p_thread );
        vlc_object_destroy( p_thread );
        free( p_sys );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * float to s16 conversion
 *****************************************************************************/
static inline int16_t FloatToInt16( float f )
{
    if( f >= 1.0 )
        return 32767;
    else if( f < -1.0 )
        return -32768;
    else
        return (int16_t)( f * 32768.0 );
}

/*****************************************************************************
 * DoWork: process samples buffer
 *****************************************************************************
 * This function queues the audio buffer to be processed by the galaktos thread
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    int i_samples;
    int i_channels;
    float *p_float;
    galaktos_thread_t *p_thread = p_filter->p_sys->p_thread;

    p_float = (float *)p_in_buf->p_buffer;
    i_channels = p_thread->i_channels;

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes;

    for( i_samples = p_in_buf->i_nb_samples; i_samples > 0; i_samples-- )
    {
        int i_cur_sample = p_thread->i_cur_sample;

        p_thread->p_data[0][i_cur_sample] = FloatToInt16( p_float[0] );
        if( i_channels > 1 )
        {
            p_thread->p_data[1][i_cur_sample] = FloatToInt16( p_float[1] );
        }
        p_float += i_channels;

        if( ++(p_thread->i_cur_sample) == 512 )
        {
            addPCM( p_thread->p_data );
            p_thread->i_cur_sample = 0;
        }
    }
}

/*****************************************************************************
 * Thread:
 *****************************************************************************/
static void Thread( vlc_object_t *p_this )
{
    galaktos_thread_t *p_thread = (galaktos_thread_t*)p_this;

    int count=0;
    double realfps=0,fpsstart=0;
    int timed=0;
    int timestart=0;
    int mspf=0;

    /* Get on OpenGL provider */
    p_thread->p_opengl =
        (vout_thread_t *)vlc_object_create( p_this, VLC_OBJECT_OPENGL );
    if( p_thread->p_opengl == NULL )
    {
        msg_Err( p_thread, "out of memory" );
        return;
    }
    vlc_object_attach( p_thread->p_opengl, p_this );

    p_thread->p_opengl->i_window_width = p_thread->i_width;
    p_thread->p_opengl->i_window_height = p_thread->i_height;
    p_thread->p_opengl->render.i_width = p_thread->i_width;
    p_thread->p_opengl->render.i_height = p_thread->i_width;
    p_thread->p_opengl->render.i_aspect = VOUT_ASPECT_FACTOR;
    p_thread->p_opengl->b_scale = VLC_TRUE;

    p_thread->p_module =
        module_Need( p_thread->p_opengl, "opengl provider", NULL, 0 );
    if( p_thread->p_module == NULL )
    {
        msg_Err( p_thread, "unable to initialize OpenGL" );
        vlc_object_detach( p_thread->p_opengl );
        vlc_object_destroy( p_thread->p_opengl );
        return;
    }

    p_thread->p_opengl->pf_init( p_thread->p_opengl );

    setup_opengl( p_thread->i_width, p_thread->i_height );
    CreateRenderTarget(512, &RenderTargetTextureID, NULL);

    timestart=mdate()/1000;

    while( !p_thread->b_die )
    {
        mspf = 1000 / 60;
        if( galaktos_update( p_thread ) == 1 )
        {
            p_thread->b_die = 1;
        }
        if( p_thread->psz_title )
        {
            free( p_thread->psz_title );
            p_thread->psz_title = NULL;
        }

        if (++count%100==0)
        {
            realfps=100/((mdate()/1000-fpsstart)/1000);
 //           printf("%f\n",realfps);
            fpsstart=mdate()/1000;
        }
        //framerate limiter
        timed=mspf-(mdate()/1000-timestart);
      //   printf("%d,%d\n",time,mspf);
        if (timed>0) msleep(1000*timed);
    //     printf("Limiter %d\n",(mdate()/1000-timestart));
        timestart=mdate()/1000;
    }

    /* Free the openGL provider */
    module_Unneed( p_thread->p_opengl, p_thread->p_module );
    vlc_object_detach( p_thread->p_opengl );
    vlc_object_destroy( p_thread->p_opengl );
}

/*****************************************************************************
 * Close: close the plugin
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    aout_filter_t     *p_filter = (aout_filter_t *)p_this;
    aout_filter_sys_t *p_sys = p_filter->p_sys;

    /* Stop galaktos Thread */
    p_sys->p_thread->b_die = VLC_TRUE;

    galaktos_done( p_sys->p_thread );

    vlc_thread_join( p_sys->p_thread );

    /* Free data */
    vlc_object_detach( p_sys->p_thread );
    vlc_object_destroy( p_sys->p_thread );

    free( p_sys );
}

static char *TitleGet( vlc_object_t *p_this )
{
    char *psz_title = NULL;
    input_thread_t *p_input =
        vlc_object_find( p_this, VLC_OBJECT_INPUT, FIND_ANYWHERE );

    if( p_input )
    {
        char *psz = strrchr( p_input->input.p_item->psz_uri, '/' );

        if( psz )
        {
            psz++;
        }
        else
        {
            psz = p_input->input.p_item->psz_uri;
        }
        if( psz && *psz )
        {
            psz_title = strdup( psz );
        }
        vlc_object_release( p_input );
    }

    return psz_title;
}
