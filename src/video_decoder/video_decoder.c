/*****************************************************************************
 * video_decoder.c : video decoder thread
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: video_decoder.c,v 1.54 2001/07/18 14:21:00 massiot Exp $
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

#ifdef HAVE_UNISTD_H
#include <unistd.h>                                              /* getpid() */
#endif

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

#include "vdec_ext-plugins.h"
#include "video_decoder.h"
#include "vpar_pool.h"

/*
 * Local prototypes
 */
static void     RunThread           ( vdec_thread_t *p_vdec );

/*****************************************************************************
 * vdec_CreateThread: create a video decoder thread
 *****************************************************************************
 * This function creates a new video decoder thread, and returns a pointer
 * to its description. On error, it returns NULL.
 *****************************************************************************/
vdec_thread_t * vdec_CreateThread( vdec_pool_t * p_pool )
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

    /*
     * Initialize the parser properties
     */
    p_vdec->p_pool = p_pool;

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
 *****************************************************************************/
void vdec_DestroyThread( vdec_thread_t *p_vdec )
{
    intf_DbgMsg("vdec debug: requesting termination of video decoder thread %p", p_vdec);

    /* Ask thread to kill itself */
    p_vdec->b_die = 1;

    /* Make sure the decoder thread leaves the vpar_GetMacroblock() function */
    vlc_mutex_lock( &p_vdec->p_pool->lock );
    vlc_cond_broadcast( &p_vdec->p_pool->wait_undecoded );
    vlc_mutex_unlock( &p_vdec->p_pool->lock );

    /* Waiting for the decoder thread to exit */
    vlc_thread_join( p_vdec->thread_id );
}

/* following functions are local */

/*****************************************************************************
 * vdec_InitThread: initialize video decoder thread
 *****************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization.
 *****************************************************************************/
void vdec_InitThread( vdec_thread_t *p_vdec )
{
    intf_DbgMsg("vdec debug: initializing video decoder thread %p", p_vdec);

    p_vdec->p_idct_data = NULL;

    p_vdec->p_pool->pf_decode_init( p_vdec );
    p_vdec->p_pool->pf_idct_init( p_vdec );

    /* Mark thread as running and return */
    intf_DbgMsg("vdec debug: InitThread(%p) succeeded", p_vdec);
}

/*****************************************************************************
 * vdec_EndThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization.
 *****************************************************************************/
void vdec_EndThread( vdec_thread_t *p_vdec )
{
    intf_DbgMsg("vdec debug: EndThread(%p)", p_vdec);

    if( p_vdec->p_idct_data != NULL )
    {
        free( p_vdec->p_idct_data );
    }

    free( p_vdec );
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

    vdec_InitThread( p_vdec );

    /*
     * Main loop
     */
    while( !p_vdec->b_die )
    {
        macroblock_t *          p_mb;

        if( (p_mb = vpar_GetMacroblock( p_vdec->p_pool, &p_vdec->b_die )) != NULL )
        {
            p_vdec->p_pool->pf_vdec_decode( p_vdec, p_mb );

            /* Decoding is finished, release the macroblock and free
             * unneeded memory. */
            p_vdec->p_pool->pf_free_mb( p_vdec->p_pool, p_mb );
        }
    }

    /* End of thread */
    vdec_EndThread( p_vdec );
}

