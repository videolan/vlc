/*****************************************************************************
 * video_decoder.c : video decoder thread
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: video_decoder.c,v 1.50 2001/05/30 17:03:12 sam Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Gaël Hendryckx <jimmy@via.ecp.fr>
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
#include "defs.h"

#include <unistd.h>                                              /* getpid() */

#include <stdlib.h>                                                /* free() */
#include <string.h>                                    /* memcpy(), memset() */
#include <errno.h>                                                  /* errno */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "intf_msg.h"

#include "stream_control.h"
#include "input_ext-dec.h"

#include "video.h"
#include "video_output.h"

#include "vdec_motion.h"
#include "video_decoder.h"

#include "vpar_blocks.h"
#include "vpar_headers.h"
#include "vpar_synchro.h"
#include "video_parser.h"
#include "video_fifo.h"

/*
 * Local prototypes
 */
#ifdef VDEC_SMP
static int      vdec_InitThread     ( vdec_thread_t *p_vdec );
#endif
static void     RunThread           ( vdec_thread_t *p_vdec );
static void     ErrorThread         ( vdec_thread_t *p_vdec );
static void     EndThread           ( vdec_thread_t *p_vdec );

/*****************************************************************************
 * vdec_CreateThread: create a video decoder thread
 *****************************************************************************
 * This function creates a new video decoder thread, and returns a pointer
 * to its description. On error, it returns NULL.
 * Following configuration properties are used:
 * XXX??
 *****************************************************************************/
vdec_thread_t * vdec_CreateThread( vpar_thread_t *p_vpar /*, int *pi_status */ )
{
    vdec_thread_t *     p_vdec;

    intf_DbgMsg("vdec debug: creating video decoder thread");

    /* Allocate the memory needed to store the thread's structure */
    if ( (p_vdec = (vdec_thread_t *)malloc( sizeof(vdec_thread_t) )) == NULL )
    {
        intf_ErrMsg("vdec error: not enough memory for vdec_CreateThread() to create the new thread");
        return( NULL );
    }

    /*
     * Initialize the thread properties
     */
    p_vdec->b_die = 0;
    p_vdec->b_error = 0;

    /*
     * Initialize the parser properties
     */
    p_vdec->p_vpar = p_vpar;

    /* Spawn the video decoder thread */
    if ( vlc_thread_create(&p_vdec->thread_id, "video decoder",
         (vlc_thread_func_t)RunThread, (void *)p_vdec) )
    {
        intf_ErrMsg("vdec error: can't spawn video decoder thread");
        free( p_vdec );
        return( NULL );
    }

    intf_DbgMsg("vdec debug: video decoder thread (%p) created", p_vdec);
    return( p_vdec );
}

/*****************************************************************************
 * vdec_DestroyThread: destroy a video decoder thread
 *****************************************************************************
 * Destroy and terminate thread. This function will return 0 if the thread could
 * be destroyed, and non 0 else. The last case probably means that the thread
 * was still active, and another try may succeed.
 *****************************************************************************/
void vdec_DestroyThread( vdec_thread_t *p_vdec /*, int *pi_status */ )
{
    intf_DbgMsg("vdec debug: requesting termination of video decoder thread %p", p_vdec);

    /* Ask thread to kill itself */
    p_vdec->b_die = 1;

#ifdef VDEC_SMP
    /* Make sure the decoder thread leaves the vpar_GetMacroblock() function */
    vlc_mutex_lock( &(p_vdec->p_vpar->vfifo.lock) );
    vlc_cond_signal( &(p_vdec->p_vpar->vfifo.wait) );
    vlc_mutex_unlock( &(p_vdec->p_vpar->vfifo.lock) );
#endif

    /* Waiting for the decoder thread to exit */
    /* Remove this as soon as the "status" flag is implemented */
    vlc_thread_join( p_vdec->thread_id );
}

/* following functions are local */

/*****************************************************************************
 * vdec_InitThread: initialize video decoder thread
 *****************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization. It returns 0 on success. Note that the thread's flag are not
 * modified inside this function.
 *****************************************************************************/
#ifdef VDEC_SMP
static int vdec_InitThread( vdec_thread_t *p_vdec )
#else
int vdec_InitThread( vdec_thread_t *p_vdec )
#endif
{
    intf_DbgMsg("vdec debug: initializing video decoder thread %p", p_vdec);

    p_vdec->pf_decode_init  = p_vdec->p_vpar->pf_decode_init;
    p_vdec->pf_decode_mb_c  = p_vdec->p_vpar->pf_decode_mb_c;
    p_vdec->pf_decode_mb_bw = p_vdec->p_vpar->pf_decode_mb_bw;

    p_vdec->pf_decode_init( p_vdec );

#ifdef VDEC_SMP
    /* Re-nice ourself */
    if( nice(VDEC_NICE) == -1 )
    {
        intf_WarnMsg( 2, "vdec warning : couldn't nice() (%s)",
                      strerror(errno) );
    }
#endif

    /* Mark thread as running and return */
    intf_DbgMsg("vdec debug: InitThread(%p) succeeded", p_vdec);
    return( 0 );
}

/*****************************************************************************
 * ErrorThread: RunThread() error loop
 *****************************************************************************
 * This function is called when an error occured during thread main's loop. The
 * thread can still receive feed, but must be ready to terminate as soon as
 * possible.
 *****************************************************************************/
static void ErrorThread( vdec_thread_t *p_vdec )
{
    macroblock_t *       p_mb;

    /* Wait until a `die' order */
    while( !p_vdec->b_die )
    {
        p_mb = vpar_GetMacroblock( &p_vdec->p_vpar->vfifo );
        vpar_DestroyMacroblock( &p_vdec->p_vpar->vfifo, p_mb );
    }
}

/*****************************************************************************
 * EndThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization.
 *****************************************************************************/
static void EndThread( vdec_thread_t *p_vdec )
{
    intf_DbgMsg("vdec debug: EndThread(%p)", p_vdec);
}

/*****************************************************************************
 * RunThread: video decoder thread
 *****************************************************************************
 * Video decoder thread. This function does only return when the thread is
 * terminated.
 *****************************************************************************/
static void RunThread( vdec_thread_t *p_vdec )
{
    intf_DbgMsg("vdec debug: running video decoder thread (%p) (pid == %i)",
                p_vdec, getpid());

    /*
     * Initialize thread and free configuration
     */
    p_vdec->b_error = vdec_InitThread( p_vdec );
    if( p_vdec->b_error )
    {
        return;
    }
    p_vdec->b_run = 1;

    /*
     * Main loop - it is not executed if an error occured during
     * initialization
     */
    while( (!p_vdec->b_die) && (!p_vdec->b_error) )
    {
        macroblock_t *          p_mb;

        if( (p_mb = vpar_GetMacroblock( &p_vdec->p_vpar->vfifo )) != NULL )
        {
            p_vdec->pf_decode_mb_c( p_vdec, p_mb );
        }
    }

    /*
     * Error loop
     */
    if( p_vdec->b_error )
    {
        ErrorThread( p_vdec );
    }

    /* End of thread */
    EndThread( p_vdec );
    p_vdec->b_run = 0;
}

