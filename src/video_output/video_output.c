/*****************************************************************************
 * video_output.c : video output thread
 * This module describes the programming interface for video output threads.
 * It includes functions allowing to open a new thread, send pictures to a
 * thread, and destroy a previously oppened video output thread.
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: video_output.c,v 1.158 2002/02/15 13:32:54 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
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
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include <videolan/vlc.h>

#ifdef HAVE_SYS_TIMES_H
#   include <sys/times.h>
#endif

#include "video.h"
#include "video_output.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int      InitThread        ( vout_thread_t *p_vout );
static void     RunThread         ( vout_thread_t *p_vout );
static void     ErrorThread       ( vout_thread_t *p_vout );
static void     EndThread         ( vout_thread_t *p_vout );
static void     DestroyThread     ( vout_thread_t *p_vout, int i_status );
static int      ReduceHeight      ( int );

/*****************************************************************************
 * vout_InitBank: initialize the video output bank.
 *****************************************************************************/
void vout_InitBank ( void )
{
    p_vout_bank->i_count = 0;

    vlc_mutex_init( &p_vout_bank->lock );
}

/*****************************************************************************
 * vout_EndBank: empty the video output bank.
 *****************************************************************************
 * This function ends all unused video outputs and empties the bank in
 * case of success.
 *****************************************************************************/
void vout_EndBank ( void )
{
    /* Ask all remaining video outputs to die */
    while( p_vout_bank->i_count )
    {
        vout_DestroyThread(
                p_vout_bank->pp_vout[ --p_vout_bank->i_count ], NULL );
    }

    vlc_mutex_destroy( &p_vout_bank->lock );
}

/*****************************************************************************
 * vout_CreateThread: creates a new video output thread
 *****************************************************************************
 * This function creates a new video output thread, and returns a pointer
 * to its description. On error, it returns NULL.
 * If pi_status is NULL, then the function will block until the thread is ready.
 * If not, it will be updated using one of the THREAD_* constants.
 *****************************************************************************/
vout_thread_t * vout_CreateThread   ( int *pi_status,
                                      int i_width, int i_height,
                                      u32 i_chroma, int i_aspect )
{
    vout_thread_t * p_vout;                             /* thread descriptor */
    int             i_status;                               /* thread status */
    int             i_index;                                /* loop variable */
    char          * psz_plugin;

    /* Allocate descriptor */
    p_vout = (vout_thread_t *) malloc( sizeof(vout_thread_t) );
    if( p_vout == NULL )
    {
        intf_ErrMsg( "vout error: vout thread creation returned %s",
                     strerror(ENOMEM) );
        return( NULL );
    }

    /* Choose the best module */
    psz_plugin = main_GetPszVariable( VOUT_FILTER_VAR, "" );
    if( psz_plugin[0] == '\0' )
    {
        psz_plugin = main_GetPszVariable( VOUT_METHOD_VAR, "" );
    }

    /* Initialize thread properties - thread id and locks will be initialized
     * later */
    p_vout->b_die               = 0;
    p_vout->b_error             = 0;
    p_vout->b_active            = 0;
    p_vout->pi_status           = (pi_status != NULL) ? pi_status : &i_status;
    *p_vout->pi_status          = THREAD_CREATE;

    /* Initialize pictures and subpictures - translation tables and functions
     * will be initialized later in InitThread */
    for( i_index = 0; i_index < VOUT_MAX_PICTURES; i_index++)
    {
        p_vout->p_picture[i_index].i_status = FREE_PICTURE;
        p_vout->p_picture[i_index].i_type   = EMPTY_PICTURE;
    }

    for( i_index = 0; i_index < VOUT_MAX_SUBPICTURES; i_index++)
    {
        p_vout->p_subpicture[i_index].i_status = FREE_SUBPICTURE;
        p_vout->p_subpicture[i_index].i_type   = EMPTY_SUBPICTURE;
    }

    /* Initialize the rendering heap */
    p_vout->i_heap_size = 0;

    I_RENDERPICTURES = 0;
    p_vout->render.i_width    = i_width;
    p_vout->render.i_height   = i_height;
    p_vout->render.i_chroma   = i_chroma;
    p_vout->render.i_aspect   = i_aspect;

    /* Initialize misc stuff */
    p_vout->i_changes    = 0;
    p_vout->f_gamma      = 0;
    p_vout->b_grayscale  = 0;
    p_vout->b_info       = 0;
    p_vout->b_interface  = 0;
    p_vout->b_scale      = 1;
    p_vout->b_fullscreen = 0;
    p_vout->render_time  = 10;

    /* user requested fullscreen? */
    if( main_GetIntVariable( VOUT_FULLSCREEN_VAR, VOUT_FULLSCREEN_DEFAULT ) )
        p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;

    p_vout->p_module
        = module_Need( MODULE_CAPABILITY_VOUT, psz_plugin, (void *)p_vout );

    if( p_vout->p_module == NULL )
    {
        intf_ErrMsg( "vout error: no suitable vout module" );
        free( p_vout );
        return( NULL );
    }

#define f p_vout->p_module->p_functions->vout.functions.vout
    p_vout->pf_create     = f.pf_create;
    p_vout->pf_init       = f.pf_init;
    p_vout->pf_end        = f.pf_end;
    p_vout->pf_destroy    = f.pf_destroy;
    p_vout->pf_manage     = f.pf_manage;
    p_vout->pf_render     = f.pf_render;
    p_vout->pf_display    = f.pf_display;
#undef f

    /* Create thread and set locks */
    vlc_mutex_init( &p_vout->picture_lock );
    vlc_mutex_init( &p_vout->subpicture_lock );
    vlc_mutex_init( &p_vout->change_lock );

    if( vlc_thread_create( &p_vout->thread_id, "video output",
                           (void *) RunThread, (void *) p_vout) )
    {
        intf_ErrMsg("vout error: %s", strerror(ENOMEM));
        p_vout->pf_destroy( p_vout );
        module_Unneed( p_vout->p_module );
        free( p_vout );
        return( NULL );
    }

    /* If status is NULL, wait until the thread is created */
    if( pi_status == NULL )
    {
        do
        {
            msleep( THREAD_SLEEP );
        }while( (i_status != THREAD_READY) && (i_status != THREAD_ERROR)
                && (i_status != THREAD_FATAL) );
        if( i_status != THREAD_READY )
        {
            return( NULL );
        }
    }

    return( p_vout );
}

/*****************************************************************************
 * vout_DestroyThread: destroys a previously created thread
 *****************************************************************************
 * Destroy a terminated thread.
 * The function will request a destruction of the specified thread. If pi_error
 * is NULL, it will return once the thread is destroyed. Else, it will be
 * update using one of the THREAD_* constants.
 *****************************************************************************/
void vout_DestroyThread( vout_thread_t *p_vout, int *pi_status )
{
    int     i_status;                                       /* thread status */

    /* Set status */
    p_vout->pi_status = (pi_status != NULL) ? pi_status : &i_status;
    *p_vout->pi_status = THREAD_DESTROY;

    /* Request thread destruction */
    p_vout->b_die = 1;

    /* If status is NULL, wait until thread has been destroyed */
    if( pi_status == NULL )
    {
        do
        {
            msleep( THREAD_SLEEP );
        } while( (i_status != THREAD_OVER) && (i_status != THREAD_ERROR)
                 && (i_status != THREAD_FATAL) );
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
    int i, i_pgcd;

    /* Update status */
    *p_vout->pi_status = THREAD_START;

    vlc_mutex_lock( &p_vout->change_lock );

#ifdef STATS
    p_vout->c_loops = 0;
#endif

    /* Initialize output method, it issues its own error messages */
    if( p_vout->pf_init( p_vout ) )
    {
        vlc_mutex_unlock( &p_vout->change_lock );
        return( 1 );
    }

    if( !I_OUTPUTPICTURES )
    {
        intf_ErrMsg( "vout error: plugin was unable to allocate at least "
                     "one direct buffer" );
        p_vout->pf_end( p_vout );
        vlc_mutex_unlock( &p_vout->change_lock );
        return( 1 );
    }

    intf_WarnMsg( 1, "vout info: got %i direct buffer(s)", I_OUTPUTPICTURES );

    i_pgcd = ReduceHeight( p_vout->render.i_aspect );
    intf_WarnMsg( 1, "vout info: picture in %ix%i, chroma 0x%.8x (%4.4s), "
                     "aspect ratio %i:%i",
                  p_vout->render.i_width, p_vout->render.i_height,
                  p_vout->render.i_chroma, (char*)&p_vout->render.i_chroma,
                  p_vout->render.i_aspect / i_pgcd,
                  VOUT_ASPECT_FACTOR / i_pgcd );

    i_pgcd = ReduceHeight( p_vout->output.i_aspect );
    intf_WarnMsg( 1, "vout info: picture out %ix%i, chroma 0x%.8x (%4.4s), "
                     "aspect ratio %i:%i",
                  p_vout->output.i_width, p_vout->output.i_height,
                  p_vout->output.i_chroma, (char*)&p_vout->output.i_chroma,
                  p_vout->output.i_aspect / i_pgcd,
                  VOUT_ASPECT_FACTOR / i_pgcd );

    /* Check whether we managed to create direct buffers similar to
     * the render buffers, ie same size, chroma and aspect ratio */
    if( ( p_vout->output.i_width == p_vout->render.i_width )
     && ( p_vout->output.i_height == p_vout->render.i_height )
     && ( vout_ChromaCmp( p_vout->output.i_chroma, p_vout->render.i_chroma ) )
     && ( p_vout->output.i_aspect == p_vout->render.i_aspect ) )
    {
        /* Cool ! We have direct buffers, we can ask the decoder to
         * directly decode into them ! Map the first render buffers to
         * the first direct buffers, but keep the first direct buffer
         * for memcpy operations */
        p_vout->b_direct = 1;

        intf_WarnMsg( 2, "vout info: direct render, mapping "
                         "render pictures 0-%i to system pictures 1-%i",
                         VOUT_MAX_PICTURES - 2, VOUT_MAX_PICTURES - 1 );

        for( i = 1; i < VOUT_MAX_PICTURES; i++ )
        {
            PP_RENDERPICTURE[ I_RENDERPICTURES ] = &p_vout->p_picture[ i ];
            I_RENDERPICTURES++;
        }

    }
    else
    {
        /* Rats... Something is wrong here, we could not find an output
         * plugin able to directly render what we decode. See if we can
         * find a chroma plugin to do the conversion */
        p_vout->b_direct = 0;

        /* Choose the best module */
        p_vout->chroma.p_module
            = module_Need( MODULE_CAPABILITY_CHROMA, NULL, (void *)p_vout );

        if( p_vout->chroma.p_module == NULL )
        {
            intf_ErrMsg( "vout error: no chroma module for %4.4s to %4.4s",
                         &p_vout->render.i_chroma, &p_vout->output.i_chroma );
            p_vout->pf_end( p_vout );
            vlc_mutex_unlock( &p_vout->change_lock );
            return( 1 );
        }

#define f p_vout->chroma.p_module->p_functions->chroma.functions.chroma
        p_vout->chroma.pf_init       = f.pf_init;
        p_vout->chroma.pf_end        = f.pf_end;
#undef f

        if( p_vout->chroma.pf_init( p_vout ) )
        {
            intf_ErrMsg( "vout error: could not initialize chroma module" );
            module_Unneed( p_vout->chroma.p_module );
            p_vout->pf_end( p_vout );
            vlc_mutex_unlock( &p_vout->change_lock );
            return( 1 );
        }

        if( I_OUTPUTPICTURES < VOUT_MAX_PICTURES )
        {
            intf_WarnMsg( 2, "vout info: indirect render, mapping "
                             "render pictures %i-%i to system pictures %i-%i",
                             I_OUTPUTPICTURES - 1, VOUT_MAX_PICTURES - 2,
                             I_OUTPUTPICTURES, VOUT_MAX_PICTURES - 1 );
        }
        else
        {
            intf_WarnMsg( 2, "vout info: indirect render, no system "
                             "pictures needed, we have %i directbuffers",
                             I_OUTPUTPICTURES );
        }

        /* Append render buffers after the direct buffers */
        for( i = I_OUTPUTPICTURES; i < VOUT_MAX_PICTURES; i++ )
        {
            PP_RENDERPICTURE[ I_RENDERPICTURES ] = &p_vout->p_picture[ i ];
            I_RENDERPICTURES++;
        }
    }

    /* Mark thread as running and return */
    p_vout->b_active = 1;
    *p_vout->pi_status = THREAD_READY;

    intf_DbgMsg("thread ready");
    return( 0 );
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
    mtime_t         current_date;                            /* current date */
    mtime_t         display_date;                            /* display date */

    picture_t *     p_picture;                            /* picture pointer */
    picture_t *     p_directbuffer;              /* direct buffer to display */

    subpicture_t *  p_subpic;                          /* subpicture pointer */

    /*
     * Initialize thread
     */
    p_vout->b_error = InitThread( p_vout );
    if( p_vout->b_error )
    {
        /* Destroy thread structures allocated by Create and InitThread */
        p_vout->pf_destroy( p_vout );

        DestroyThread( p_vout, THREAD_ERROR );
        return;
    }

    /*
     * Main loop - it is not executed if an error occured during
     * initialization
     */
    while( (!p_vout->b_die) && (!p_vout->b_error) )
    {
        /* Initialize loop variables */
        display_date = 0;
        current_date = mdate();

#ifdef STATS
        p_vout->c_loops++;
        if( !(p_vout->c_loops % VOUT_STATS_NB_LOOPS) )
        {
            intf_Msg( "vout stats: picture heap: %d/%d",
                      I_RENDERPICTURES, p_vout->i_heap_size );
        }
#endif

        /*
         * Find the picture to display - this operation does not need lock,
         * since only READY_PICTUREs are handled
         */
        p_picture = NULL;

        for( i_index = 0;
             i_index < I_RENDERPICTURES;
             i_index++ )
        {
            if( (PP_RENDERPICTURE[i_index]->i_status == READY_PICTURE)
                && ( (p_picture == NULL) ||
                     (PP_RENDERPICTURE[i_index]->date < display_date) ) )
            {
                p_picture = PP_RENDERPICTURE[i_index];
                display_date = p_picture->date;
            }
        }

        if( p_picture != NULL )
        {
            /* Compute FPS rate */
            p_vout->p_fps_sample[ p_vout->c_fps_samples++ % VOUT_FPS_SAMPLES ]
                = display_date;

            if( display_date < current_date + p_vout->render_time )
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
                intf_WarnMsg( 1, "vout warning: late picture skipped (%p)",
                              p_picture );
                vlc_mutex_unlock( &p_vout->picture_lock );

                continue;
            }
            else if( display_date > current_date + VOUT_DISPLAY_DELAY )
            {
                /* A picture is ready to be rendered, but its rendering date
                 * is far from the current one so the thread will perform an
                 * empty loop as if no picture were found. The picture state
                 * is unchanged */
                p_picture    = NULL;
                display_date = 0;
            }
        }

        /*
         * Check for subpictures to display
         */
        p_subpic = vout_SortSubPictures( p_vout, display_date );

        /*
         * Perform rendering
         */
        p_directbuffer = vout_RenderPicture( p_vout, p_picture, p_subpic );

        /*
         * Call the plugin-specific rendering method
         */
        if( p_picture != NULL )
        {
            /* Render the direct buffer returned by vout_RenderPicture */
            p_vout->pf_render( p_vout, p_directbuffer );
        }

        /*
         * Sleep, wake up
         */
        if( display_date != 0 )
        {
            /* Store render time using Bresenham algorithm */
            p_vout->render_time += mdate() - current_date;
            p_vout->render_time >>= 1;
        }

        /* Give back change lock */
        vlc_mutex_unlock( &p_vout->change_lock );

        /* Sleep a while or until a given date */
        if( display_date != 0 )
        {
            mwait( display_date - VOUT_MWAIT_TOLERANCE );
        }
        else
        {
            msleep( VOUT_IDLE_SLEEP );
        }

        /* On awakening, take back lock and send immediately picture
         * to display. */
        vlc_mutex_lock( &p_vout->change_lock );

#ifdef TRACE_VOUT
        intf_DbgMsg( "picture %p, subpicture %p", p_picture, p_subpic );
#endif

        /*
         * Display the previously rendered picture
         */
        if( p_picture != NULL )
        {
            /* Display the direct buffer returned by vout_RenderPicture */
            p_vout->pf_display( p_vout, p_directbuffer );

            /* Remove picture from heap */
            vlc_mutex_lock( &p_vout->picture_lock );
            if( p_picture->i_refcount )
            {
                p_picture->i_status = DISPLAYED_PICTURE;
            }
            else
            {
                p_picture->i_status = DESTROYED_PICTURE;
                p_vout->i_heap_size--;
            }
            vlc_mutex_unlock( &p_vout->picture_lock );
        }

        /*
         * Check events and manage thread
         */
        if( p_vout->pf_manage( p_vout ) )
        {
            /* A fatal error occured, and the thread must terminate immediately,
             * without displaying anything - setting b_error to 1 causes the
             * immediate end of the main while() loop. */
            p_vout->b_error = 1;
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

    /* Destroy method-dependant resources */
    p_vout->pf_destroy( p_vout );

    /* Destroy thread structures allocated by CreateThread */
    DestroyThread( p_vout, THREAD_OVER );
    intf_DbgMsg( "thread end" );
}

/*****************************************************************************
 * ErrorThread: RunThread() error loop
 *****************************************************************************
 * This function is called when an error occured during thread main's loop. The
 * thread can still receive feed, but must be ready to terminate as soon as
 * possible.
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
 * initialization. It frees all ressources allocated by InitThread.
 *****************************************************************************/
static void EndThread( vout_thread_t *p_vout )
{
    int     i_index;                                        /* index in heap */

    /* Store status */
    *p_vout->pi_status = THREAD_END;

#ifdef STATS
    {
        struct tms cpu_usage;
        times( &cpu_usage );

        intf_Msg( "vout stats: cpu usage (user: %d, system: %d)",
                  cpu_usage.tms_utime, cpu_usage.tms_stime );
    }
#endif

    if( !p_vout->b_direct )
    {
        p_vout->chroma.pf_end( p_vout );
        module_Unneed( p_vout->chroma.p_module );
    }

    /* Destroy all remaining pictures */
    for( i_index = 0; i_index < VOUT_MAX_PICTURES; i_index++ )
    {
        if ( p_vout->p_picture[i_index].i_type == MEMORY_PICTURE )
        {
            free( p_vout->p_picture[i_index].p_data );
        }
    }

    /* Destroy all remaining subpictures */
    for( i_index = 0; i_index < VOUT_MAX_SUBPICTURES; i_index++ )
    {
        if( p_vout->p_subpicture[i_index].i_status != FREE_SUBPICTURE )
        {
            free( p_vout->p_subpicture[i_index].p_data );
        }
    }

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
static void DestroyThread( vout_thread_t *p_vout, int i_status )
{
    int *pi_status;                                         /* status adress */

    /* Store status adress */
    pi_status = p_vout->pi_status;

    /* Destroy the locks */
    vlc_mutex_destroy( &p_vout->picture_lock );
    vlc_mutex_destroy( &p_vout->subpicture_lock );
    vlc_mutex_destroy( &p_vout->change_lock );

    /* Release the module */
    module_Unneed( p_vout->p_module );

    /* Free structure */
    free( p_vout );
    *pi_status = i_status;
}

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

    while( !(i_ratio % 3) && !(i_ratio % 3) )
    {
        i_ratio /= 3;
        i_dummy /= 3;
        i_pgcd  *= 3;
    }

    while( !(i_ratio % 5) && !(i_ratio % 5) )
    {
        i_ratio /= 5;
        i_dummy /= 5;
        i_pgcd  *= 5;
    }

    return i_pgcd;
}

