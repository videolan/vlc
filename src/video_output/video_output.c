/*****************************************************************************
 * video_output.c : video output thread
 * This module describes the programming interface for video output threads.
 * It includes functions allowing to open a new thread, send pictures to a
 * thread, and destroy a previously oppened video output thread.
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: video_output.c,v 1.177 2002/05/19 23:51:37 massiot Exp $
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
static int      BinaryLog         ( u32 );
static void     MaskToShift       ( int *, int *, u32 );
static void     InitWindowSize    ( vout_thread_t *, int *, int * );

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
    if( !(psz_plugin = config_GetPszVariable( "filter" )) )
    {
        psz_plugin = config_GetPszVariable( "vout" );
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
    for( i_index = 0; i_index < 2 * VOUT_MAX_PICTURES; i_index++)
    {
        p_vout->p_picture[i_index].i_status = FREE_PICTURE;
        p_vout->p_picture[i_index].i_type   = EMPTY_PICTURE;
    }

    for( i_index = 0; i_index < VOUT_MAX_SUBPICTURES; i_index++)
    {
        p_vout->p_subpicture[i_index].i_status = FREE_SUBPICTURE;
        p_vout->p_subpicture[i_index].i_type   = EMPTY_SUBPICTURE;
    }

    /* No images in the heap */
    p_vout->i_heap_size = 0;

    /* Initialize the rendering heap */
    I_RENDERPICTURES = 0;
    p_vout->render.i_width    = i_width;
    p_vout->render.i_height   = i_height;
    p_vout->render.i_chroma   = i_chroma;
    p_vout->render.i_aspect   = i_aspect;

    p_vout->render.i_rmask    = 0;
    p_vout->render.i_gmask    = 0;
    p_vout->render.i_bmask    = 0;

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
    p_vout->render_time  = 10;
    p_vout->c_fps_samples= 0;

    /* user requested fullscreen? */
    if( config_GetIntVariable( "fullscreen" ) )
        p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;

    /* Initialize the dimensions of the video window */
    InitWindowSize( p_vout, &p_vout->i_window_width,
                    &p_vout->i_window_height );


    p_vout->p_module
        = module_Need( MODULE_CAPABILITY_VOUT, psz_plugin, (void *)p_vout );

    if( psz_plugin ) free( psz_plugin );
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
 * vout_ChromaCmp: compare two chroma values
 *****************************************************************************
 * This function returns 1 if the two fourcc values given as argument are
 * the same format (eg. UYVY / UYNV) or almost the same format (eg. I420/YV12)
 *****************************************************************************/
int vout_ChromaCmp( u32 i_chroma, u32 i_amorhc )
{
    /* If they are the same, they are the same ! */
    if( i_chroma == i_amorhc )
    {
        return 1;
    }

    /* Check for equivalence classes */
    switch( i_chroma )
    {
        case FOURCC_I420:
        case FOURCC_IYUV:
        case FOURCC_YV12:
            switch( i_amorhc )
            {
                case FOURCC_I420:
                case FOURCC_IYUV:
                case FOURCC_YV12:
                    return 1;

                default:
                    return 0;
            }

        case FOURCC_UYVY:
        case FOURCC_UYNV:
        case FOURCC_Y422:
            switch( i_amorhc )
            {
                case FOURCC_UYVY:
                case FOURCC_UYNV:
                case FOURCC_Y422:
                    return 1;

                default:
                    return 0;
            }

        case FOURCC_YUY2:
        case FOURCC_YUNV:
            switch( i_amorhc )
            {
                case FOURCC_YUY2:
                case FOURCC_YUNV:
                    return 1;

                default:
                    return 0;
            }

        default:
            return 0;
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

    /* Initialize output method, it allocates direct buffers for us */
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

    /* Calculate shifts from system-updated masks */
    MaskToShift( &p_vout->output.i_lrshift, &p_vout->output.i_rrshift,
                 p_vout->output.i_rmask );
    MaskToShift( &p_vout->output.i_lgshift, &p_vout->output.i_rgshift,
                 p_vout->output.i_gmask );
    MaskToShift( &p_vout->output.i_lbshift, &p_vout->output.i_rbshift,
                 p_vout->output.i_bmask );

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

        if( I_OUTPUTPICTURES < 2 * VOUT_MAX_PICTURES )
        {
            intf_WarnMsg( 2, "vout info: indirect render, mapping "
                             "render pictures %i-%i to system pictures %i-%i",
                             I_OUTPUTPICTURES - 1, 2 * VOUT_MAX_PICTURES - 2,
                             I_OUTPUTPICTURES, 2 * VOUT_MAX_PICTURES - 1 );
        }
        else
        {
            /* FIXME: if this happens, we don't have any render picture left */
            intf_WarnMsg( 2, "vout info: indirect render, no system "
                             "pictures needed, we have %i directbuffers",
                             I_OUTPUTPICTURES );
            intf_ErrMsg( "vout: this is a bug!\n");
        }

        /* Append render buffers after the direct buffers */
        for( i = I_OUTPUTPICTURES; i < 2 * VOUT_MAX_PICTURES; i++ )
        {
            PP_RENDERPICTURE[ I_RENDERPICTURES ] = &p_vout->p_picture[ i ];
            I_RENDERPICTURES++;
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

    /* Mark thread as running and return */
    p_vout->b_active = 1;
    *p_vout->pi_status = THREAD_READY;

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
                intf_WarnMsg( 1, "vout warning: late picture skipped (%lld)",
                              current_date - display_date );
                vlc_mutex_unlock( &p_vout->picture_lock );

                continue;
            }
#if 0
            /* Removed because it causes problems for some people --Meuuh */
            else if( display_date > current_date + VOUT_BOGUS_DELAY )
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
                intf_WarnMsg( 1, "vout warning: early picture skipped (%lld)",
                              display_date - current_date );
                vlc_mutex_unlock( &p_vout->picture_lock );

                continue;
            }
#endif
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
            /* A fatal error occured, and the thread must terminate
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
                intf_ErrMsg( "vout error: cannot resize display" );
                /* FixMe: p_vout->pf_end will be called again in EndThread() */
                p_vout->b_error = 1;
            }

            /* Need to reinitialise the chroma plugin */
            p_vout->chroma.pf_end( p_vout );
            p_vout->chroma.pf_init( p_vout );
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
    for( i_index = 0; i_index < 2 * VOUT_MAX_PICTURES; i_index++ )
    {
        if ( p_vout->p_picture[i_index].i_type == MEMORY_PICTURE )
        {
            free( p_vout->p_picture[i_index].p_data_orig );
        }
    }

    /* Destroy all remaining subpictures */
    for( i_index = 0; i_index < VOUT_MAX_SUBPICTURES; i_index++ )
    {
        if( p_vout->p_subpicture[i_index].i_status != FREE_SUBPICTURE )
        {
            free( p_vout->p_subpicture[i_index].p_sys );
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

/*****************************************************************************
 * BinaryLog: computes the base 2 log of a binary value
 *****************************************************************************
 * This functions is used by MaskToShift, to get a bit index from a binary
 * value.
 *****************************************************************************/
static int BinaryLog(u32 i)
{
    int i_log = 0;

    if(i & 0xffff0000) i_log += 16;
    if(i & 0xff00ff00) i_log += 8;
    if(i & 0xf0f0f0f0) i_log += 4;
    if(i & 0xcccccccc) i_log += 2;
    if(i & 0xaaaaaaaa) i_log += 1;

    if (i != ((u32)1 << i_log))
    {
        intf_ErrMsg( "vout error: binary log overflow for %i", i );
    }

    return( i_log );
}

/*****************************************************************************
 * MaskToShift: transform a color mask into right and left shifts
 *****************************************************************************
 * This function is used for obtaining color shifts from masks.
 *****************************************************************************/
static void MaskToShift( int *pi_left, int *pi_right, u32 i_mask )
{
    u32 i_low, i_high;                 /* lower hand higher bits of the mask */

    if( !i_mask )
    {
        *pi_left = *pi_right = 0;
        return;
    }

    /* Get bits */
    i_low =  i_mask & (- i_mask);                   /* lower bit of the mask */
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
static void InitWindowSize( vout_thread_t *p_vout, int *pi_width,
                            int *pi_height )
{
    int i_width, i_height;
    double f_zoom;

    i_width = config_GetIntVariable( "width" );
    i_height = config_GetIntVariable( "height" );
    f_zoom = config_GetFloatVariable( "zoom" );

    if( (i_width >= 0) && (i_height >= 0))
    {
        *pi_width = i_width * f_zoom;
        *pi_height = i_height * f_zoom;
        return;
    }
    else if( i_width >= 0 )
    {
        *pi_width = i_width * f_zoom;
        *pi_height = i_width * f_zoom * VOUT_ASPECT_FACTOR /
                        p_vout->render.i_aspect;
        return;
    }
    else if( i_height >= 0 )
    {
        *pi_height = i_height * f_zoom;
        *pi_width = i_height * f_zoom * p_vout->render.i_aspect /
                       VOUT_ASPECT_FACTOR;
        return;
    }

    if( p_vout->render.i_height * p_vout->render.i_aspect
        >= p_vout->render.i_width * VOUT_ASPECT_FACTOR )
    {
        *pi_width = p_vout->render.i_height * f_zoom
          * p_vout->render.i_aspect / VOUT_ASPECT_FACTOR;
        *pi_height = p_vout->render.i_height * f_zoom;
    }
    else
    {
        *pi_width = p_vout->render.i_width * f_zoom;
        *pi_height = p_vout->render.i_width * f_zoom
          * VOUT_ASPECT_FACTOR / p_vout->render.i_aspect;
    }
}
